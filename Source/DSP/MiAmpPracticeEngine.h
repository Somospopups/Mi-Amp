#pragma once
#include <JuceHeader.h>
#include "../ThirdParty/signalsmith/signalsmith-stretch.h"

#if defined(MIAMP_HAS_DEMUCS) && MIAMP_HAS_DEMUCS
 #include <onnxruntime_c_api.h>
#endif

#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace miamp_practice
{

inline constexpr int kNumStems  = 5; // 0: Vocals, 1: Guitar, 2: Bass, 3: Drums, 4: Other/FX
inline constexpr int kTrackTake = 5; // MI GUITARRA / MI BAJO
inline constexpr int kTrackMet  = 6; // METRONOMO
inline constexpr int kNumTracks = 7;

enum class StemQuality : int
{
    None = 0,
    InstantDsp = 1,
    AiSeparating = 2,
    AiReady = 3
};

enum class PlaylistEndMode : int
{
    Continuous = 0,      // Corrido
    PauseAndChoose = 1   // Pausar y Elegir
};

struct PracticeSongItem
{
    juce::String id;
    juce::String title;
    juce::String audioFilePath;
    bool isFreeTake = false;
    double durationSec = 180.0;
    float bpm = 120.0f;
    StemQuality stemQuality = StemQuality::None;
    juce::String linkedPresetName;
    bool linkedPresetIsBass = false;
    bool autoLoadPreset = true;
};

struct LoadedSongStems
{
    juce::String songId;
    double sampleRate = 44100.0;
    int numSamples = 0;
    bool isFreeTake = false;
    bool isAiSeparated = false;
    std::array<juce::AudioBuffer<float>, kNumStems> stems;
    static constexpr int kPeakBins = 420;
    std::array<std::vector<float>, kNumStems> peaks; // 0..1 normalized envelope per bin
};

inline juce::File getPracticeRootDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PopupsAudio").getChildFile ("MiAmp").getChildFile ("Practice");
    dir.createDirectory();
    return dir;
}

inline juce::File getModelsDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PopupsAudio").getChildFile ("MiAmp").getChildFile ("Models");
    dir.createDirectory();
    return dir;
}

inline juce::File getDemucsModelFile()
{
    return getModelsDir().getChildFile ("htdemucs_6s_fp16weights.onnx");
}

inline bool isDemucsModelReady()
{
    const auto f = getDemucsModelFile();
    return f.existsAsFile() && f.getSize() >= 130000000;
}

inline juce::String demucsModelDownloadUrl()
{
    return "https://huggingface.co/StemSplitio/htdemucs-6s-onnx/resolve/main/htdemucs_6s_fp16weights.onnx";
}

inline juce::File getSongCacheDir (const juce::String& songId)
{
    auto d = getPracticeRootDir().getChildFile ("Cache").getChildFile (songId);
    d.createDirectory();
    return d;
}

inline juce::File getPlaylistXmlFile()
{
    return getPracticeRootDir().getChildFile ("playlist.xml");
}

inline juce::String makeSongId (const juce::File& file)
{
    const auto key = file.getFullPathName() + "|" + juce::String (file.getSize());
    return juce::String::toHexString (key.hashCode64()).paddedLeft ('0', 16);
}

inline const char* stemFileName (int idx) noexcept
{
    static constexpr const char* kNames[kNumStems] = {
        "vocals.wav", "guitar.wav", "bass.wav", "drums.wav", "other.wav"
    };
    return kNames[juce::jlimit (0, kNumStems - 1, idx)];
}

inline void computePeaksForBuffer (const juce::AudioBuffer<float>& buf, int numSamples,
                                   int numBins, std::vector<float>& outPeaks)
{
    outPeaks.assign (static_cast<size_t> (numBins), 0.0f);
    if (numSamples <= 0 || buf.getNumSamples() <= 0 || buf.getNumChannels() <= 0)
        return;

    const int avail = juce::jmin (numSamples, buf.getNumSamples());
    const float* ch0 = buf.getReadPointer (0);
    const float* ch1 = buf.getNumChannels() > 1 ? buf.getReadPointer (1) : ch0;

    for (int b = 0; b < numBins; ++b)
    {
        const int s0 = (b * avail) / numBins;
        const int s1 = juce::jmax (s0 + 1, ((b + 1) * avail) / numBins);
        const int step = juce::jmax (1, (s1 - s0) / 32);
        float pk = 0.0f;
        for (int s = s0; s < s1 && s < avail; s += step)
        {
            const float v = 0.5f * (std::fabs (ch0[s]) + std::fabs (ch1[s]));
            if (v > pk) pk = v;
        }
        outPeaks[static_cast<size_t> (b)] = juce::jlimit (0.0f, 1.0f, pk * 1.15f);
    }
}

// Fast, zero-dependency DSP 5-stem splitter used immediately on song import
// before/during background AI separation.
inline void splitStemsDspInstant (const juce::AudioBuffer<float>& src, double sampleRate,
                                  std::array<juce::AudioBuffer<float>, kNumStems>& outStems)
{
    const int n = src.getNumSamples();
    for (int i = 0; i < kNumStems; ++i)
    {
        outStems[static_cast<size_t> (i)].setSize (2, n, false, true, true);
        outStems[static_cast<size_t> (i)].clear();
    }
    if (n <= 0 || src.getNumChannels() <= 0)
        return;

    const float* inL = src.getReadPointer (0);
    const float* inR = src.getNumChannels() > 1 ? src.getReadPointer (1) : inL;

    float* voxL = outStems[0].getWritePointer (0); float* voxR = outStems[0].getWritePointer (1);
    float* gtrL = outStems[1].getWritePointer (0); float* gtrR = outStems[1].getWritePointer (1);
    float* basL = outStems[2].getWritePointer (0); float* basR = outStems[2].getWritePointer (1);
    float* drmL = outStems[3].getWritePointer (0); float* drmR = outStems[3].getWritePointer (1);
    float* othL = outStems[4].getWritePointer (0); float* othR = outStems[4].getWritePointer (1);

    const float sr = static_cast<float> (juce::jmax (8000.0, sampleRate));
    auto coeff = [sr] (float fc) -> float
    {
        return 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * juce::jmin (fc, sr * 0.45f) / sr);
    };

    const float cSub  = coeff (185.0f);
    const float cMidL = coeff (280.0f);
    const float cMidH = coeff (4200.0f);
    const float cHi   = coeff (5800.0f);
    const float cEnvF = coeff (95.0f);
    const float cEnvS = coeff (12.0f);

    float lpBass1 = 0.0f, lpBass2 = 0.0f;
    float lpMidLowL = 0.0f, lpMidLowR = 0.0f;
    float lpMidHiL  = 0.0f, lpMidHiR  = 0.0f;
    float lpHiCutL  = 0.0f, lpHiCutR  = 0.0f;
    float envFast = 0.0f, envSlow = 0.0f;

    for (int i = 0; i < n; ++i)
    {
        const float l = inL[i];
        const float r = inR[i];
        const float mid  = 0.5f * (l + r);
        const float side = 0.5f * (l - r);

        // 1. Bass (<185 Hz mono 2-pole LPF)
        lpBass1 += cSub * (mid - lpBass1);
        lpBass2 += cSub * (lpBass1 - lpBass2);
        const float bassSig = lpBass2;

        // 2. Drums (transient ratio on kick + snare/hihat)
        lpHiCutL += cHi * (l - lpHiCutL);
        lpHiCutR += cHi * (r - lpHiCutR);
        const float hiL = l - lpHiCutL;
        const float hiR = r - lpHiCutR;

        const float absMid = std::fabs (mid);
        envFast += cEnvF * (absMid - envFast);
        envSlow += cEnvS * (absMid - envSlow);
        const float transRatio = juce::jlimit (0.0f, 1.0f, (envFast - envSlow * 1.08f) / (envSlow + 0.03f));

        const float dL = hiL * (0.65f + 0.35f * transRatio) + (mid - lpBass2) * transRatio * 0.55f + bassSig * transRatio * 0.45f;
        const float dR = hiR * (0.65f + 0.35f * transRatio) + (mid - lpBass2) * transRatio * 0.55f + bassSig * transRatio * 0.45f;

        // 3. Vocals (centered Mid bandpass 280-4200 Hz attenuated by stereo side energy)
        lpMidLowL += cMidL * (l - lpMidLowL);
        lpMidLowR += cMidL * (r - lpMidLowR);
        lpMidHiL  += cMidH * (l - lpMidHiL);
        lpMidHiR  += cMidH * (r - lpMidHiR);
        const float bpL = lpMidHiL - lpMidLowL;
        const float bpR = lpMidHiR - lpMidLowR;
        const float bpMid  = 0.5f * (bpL + bpR);
        const float bpSide = 0.5f * (bpL - bpR);

        const float centerMask = juce::jlimit (0.0f, 1.0f,
            (std::fabs (bpMid) - 0.85f * std::fabs (bpSide)) / (std::fabs (bpMid) + 0.02f));
        const float vSig = bpMid * centerMask * (1.0f - 0.45f * transRatio);

        // 4. Guitar (mid-band stereo + remaining harmonic mid)
        const float gL = (bpL - vSig) * 0.72f + side * 0.35f;
        const float gR = (bpR - vSig) * 0.72f - side * 0.35f;

        // 5. Bass residual without kick transient
        const float bOut = bassSig * (1.0f - 0.35f * transRatio);

        // 6. Other/FX = exact residual so sum reconstructs original audio
        const float oL = l - (vSig + gL + bOut + dL);
        const float oR = r - (vSig + gR + bOut + dR);

        voxL[i] = vSig; voxR[i] = vSig;
        gtrL[i] = gL;   gtrR[i] = gR;
        basL[i] = bOut; basR[i] = bOut;
        drmL[i] = dL;   drmR[i] = dR;
        othL[i] = oL;   othR[i] = oR;
    }
}

class MiAmpPracticeEngine
{
public:
    MiAmpPracticeEngine()
    {
        for (auto& v : trackVolume) v.store (1.0f);
        for (auto& m : trackMute)   m.store (false);
        for (auto& s : trackSolo)   s.store (false);
        trackExportChecked.fill (true);
        trackVolume[kTrackMet].store (0.75f);
        trackExportChecked[kTrackMet] = false; // metronome off by default in export
        loadPlaylistFromDisk();
    }

    ~MiAmpPracticeEngine()
    {
        if (aliveToken != nullptr)
            aliveToken->store (false, std::memory_order_release);
        stopBackgroundWorkers();
    }

    void prepareToPlay (double sampleRate, int maxBlockSize)
    {
        engineSampleRate = juce::jmax (8000.0, sampleRate);
        const int scratchFrames = juce::jmax (8192, maxBlockSize * 4);
        stretchInBuf.setSize  (2, scratchFrames, false, true, true);
        stretchOutBuf.setSize (2, scratchFrames, false, true, true);
        takeInBuf.setSize     (2, scratchFrames, false, true, true);
        takeOutBuf.setSize    (2, scratchFrames, false, true, true);

        backingStretch.presetDefault (2, static_cast<float> (engineSampleRate), false);
        takeStretch.presetDefault    (2, static_cast<float> (engineSampleRate), false);
        lastBackingPitch = 999.0f;
        lastTakePitch    = 999.0f;
        needsStretchReset.store (true, std::memory_order_release);

        // If a song is already selected but stems were loaded at a different sample rate, reload safely on message thread
        if (activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size()))
        {
            const auto* cur = activeStemsPtr.load (std::memory_order_acquire);
            if (cur == nullptr || std::abs (cur->sampleRate - engineSampleRate) > 1.0)
            {
                if (juce::MessageManager::getInstance()->isThisTheMessageThread())
                {
                    loadSongAtIndex (activeSongIndex, false);
                }
                else
                {
                    auto tok = aliveToken;
                    juce::MessageManager::callAsync ([this, tok]
                    {
                        if (tok && tok->load (std::memory_order_acquire)
                            && activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size()))
                            loadSongAtIndex (activeSongIndex, false);
                    });
                }
            }
        }
    }

    void stopBackgroundWorkers()
    {
        if (aiThread != nullptr)
        {
            aiThread->signalThreadShouldExit();
            aiThread->stopThread (5000);
            aiThread.reset();
        }
        if (loaderThread != nullptr)
        {
            loaderThread->signalThreadShouldExit();
            loaderThread->stopThread (3000);
            loaderThread.reset();
        }
    }

    //==========================================================================
    // Playlist Management & Persistence
    //==========================================================================
    const std::vector<PracticeSongItem>& getPlaylist() const noexcept { return playlist; }
    int getActiveSongIndex() const noexcept { return activeSongIndex; }

    PlaylistEndMode getEndMode() const noexcept { return endMode; }
    void setEndMode (PlaylistEndMode m) { endMode = m; savePlaylistToDisk(); }

    void addAudioFilesToPlaylist (const juce::Array<juce::File>& files)
    {
        int firstAdded = -1;
        for (const auto& f : files)
        {
            if (! f.existsAsFile()) continue;
            PracticeSongItem item;
            item.id = makeSongId (f);
            item.title = f.getFileNameWithoutExtension();
            item.audioFilePath = f.getFullPathName();
            item.isFreeTake = false;
            item.bpm = 120.0f;

            const auto cacheDir = getSongCacheDir (item.id);
            bool allCached = true;
            for (int s = 0; s < kNumStems; ++s)
                if (! cacheDir.getChildFile (stemFileName (s)).existsAsFile()) { allCached = false; break; }
            item.stemQuality = allCached ? StemQuality::AiReady : StemQuality::InstantDsp;

            if (firstAdded < 0) firstAdded = static_cast<int> (playlist.size());
            playlist.push_back (item);
        }
        savePlaylistToDisk();
        if (firstAdded >= 0 && (activeSongIndex < 0 || ! isPlaying()))
            loadSongAtIndex (firstAdded, false);
    }

    void addFreeTakeSession (const juce::String& name = {})
    {
        PracticeSongItem item;
        item.id = "freetake_" + juce::String (juce::Time::currentTimeMillis());
        item.title = name.isNotEmpty() ? name : ("Toma Libre #" + juce::String (static_cast<int> (playlist.size()) + 1));
        item.isFreeTake = true;
        item.durationSec = 180.0;
        item.bpm = bpm.load (std::memory_order_relaxed);
        item.stemQuality = StemQuality::None;
        playlist.push_back (item);
        savePlaylistToDisk();
        loadSongAtIndex (static_cast<int> (playlist.size()) - 1, false);
    }

    void removeSongAtIndex (int index)
    {
        if (index < 0 || index >= static_cast<int> (playlist.size())) return;
        const bool wasActive = (index == activeSongIndex);
        playlist.erase (playlist.begin() + index);
        if (playlist.empty())
        {
            stop();
            activeSongIndex = -1;
            setActiveStems (nullptr);
        }
        else if (wasActive)
        {
            stop();
            activeSongIndex = juce::jlimit (0, static_cast<int> (playlist.size()) - 1, index);
            loadSongAtIndex (activeSongIndex, false);
        }
        else if (index < activeSongIndex)
        {
            --activeSongIndex;
        }
        savePlaylistToDisk();
    }

    void moveSongInPlaylist (int index, int delta)
    {
        const int target = index + delta;
        if (index < 0 || target < 0 || index >= static_cast<int> (playlist.size())
            || target >= static_cast<int> (playlist.size()))
            return;
        std::swap (playlist[static_cast<size_t> (index)], playlist[static_cast<size_t> (target)]);
        if (activeSongIndex == index) activeSongIndex = target;
        else if (activeSongIndex == target) activeSongIndex = index;
        savePlaylistToDisk();
    }

    void linkPresetToActiveSong (const juce::String& presetName, bool isBass)
    {
        if (activeSongIndex < 0 || activeSongIndex >= static_cast<int> (playlist.size())) return;
        auto& item = playlist[static_cast<size_t> (activeSongIndex)];
        item.linkedPresetName = presetName;
        item.linkedPresetIsBass = isBass;
        savePlaylistToDisk();
    }

    void clearLinkedPresetOnActiveSong()
    {
        if (activeSongIndex < 0 || activeSongIndex >= static_cast<int> (playlist.size())) return;
        playlist[static_cast<size_t> (activeSongIndex)].linkedPresetName = {};
        savePlaylistToDisk();
    }

    //==========================================================================
    // Song Loading & Stem AI Worker
    //==========================================================================
    void loadSongAtIndex (int index, bool autoPlayAfterLoad)
    {
        if (index < 0 || index >= static_cast<int> (playlist.size()))
            return;

        stop();
        endChoicePending.store (false, std::memory_order_release);
        activeSongIndex = index;
        const auto item = playlist[static_cast<size_t> (index)];

        // Clear previous recorded take when switching songs
        clearUserTake();

        bpm.store (juce::jlimit (40.0f, 240.0f, item.bpm), std::memory_order_relaxed);
        loopEnabled.store (false, std::memory_order_relaxed);
        loopStartSec.store (0.0, std::memory_order_relaxed);
        loopEndSec.store (juce::jmax (4.0, item.durationSec), std::memory_order_relaxed);

        if (item.isFreeTake)
        {
            auto freeStems = std::make_shared<LoadedSongStems>();
            freeStems->songId = item.id;
            freeStems->sampleRate = engineSampleRate;
            freeStems->numSamples = static_cast<int> (item.durationSec * engineSampleRate);
            freeStems->isFreeTake = true;
            setActiveStems (freeStems);
            ensureTakeCapacity (freeStems->numSamples + static_cast<int> (engineSampleRate * 30.0));
            if (autoPlayAfterLoad) play();
            return;
        }

        if (loaderThread != nullptr)
        {
            loaderThread->signalThreadShouldExit();
            loaderThread->stopThread (2000);
        }

        const double sr = engineSampleRate;
        auto tok = aliveToken;
        loaderThread = std::make_unique<SongLoaderThread> (
            item, sr, tok,
            [this, tok, autoPlayAfterLoad] (std::shared_ptr<LoadedSongStems> loaded, bool shouldStartAi)
            {
                if (! tok || ! tok->load (std::memory_order_acquire)) return;
                if (loaded == nullptr)
                {
                    aiStatusText = "No se pudo abrir el archivo de audio (usa MP3, WAV, FLAC, AIFF u OGG)";
                    if (onStateChanged) onStateChanged();
                    return;
                }
                setActiveStems (loaded);
                if (activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size())
                    && playlist[static_cast<size_t> (activeSongIndex)].id == loaded->songId)
                {
                    auto& cur = playlist[static_cast<size_t> (activeSongIndex)];
                    cur.durationSec = static_cast<double> (loaded->numSamples) / juce::jmax (1.0, loaded->sampleRate);
                    cur.stemQuality = loaded->isAiSeparated ? StemQuality::AiReady : StemQuality::InstantDsp;
                    loopEndSec.store (cur.durationSec, std::memory_order_relaxed);
                    ensureTakeCapacity (loaded->numSamples + static_cast<int> (loaded->sampleRate * 15.0));
                    savePlaylistToDisk();

                    if (shouldStartAi && isDemucsModelReady() && ! loaded->isAiSeparated)
                        startAiSeparationForSong (activeSongIndex);
                }
                if (autoPlayAfterLoad)
                    play();
                if (onStateChanged)
                    onStateChanged();
            });
        loaderThread->startThread (juce::Thread::Priority::normal);
    }

    void startModelDownload()
    {
        if (aiBusy.load (std::memory_order_relaxed)) return;
        if (aiThread != nullptr) { aiThread->stopThread (2000); aiThread.reset(); }
        aiBusy.store (true, std::memory_order_release);
        aiProgress.store (0.0f, std::memory_order_relaxed);
        aiStatusText = "Descargando motor AI (htdemucs-6s · ~53 MB)...";
        auto tok = aliveToken;
        aiThread = std::make_unique<AiWorkerThread> (
            AiWorkerThread::Task::DownloadModel, PracticeSongItem{}, 0.0, tok,
            [this, tok] (float p, const juce::String& msg)
            {
                if (! tok || ! tok->load (std::memory_order_acquire)) return;
                aiProgress.store (p, std::memory_order_relaxed);
                aiStatusText = msg;
            },
            [this, tok] (bool ok, const juce::String& songId, const juce::String& errMsg)
            {
                if (! tok || ! tok->load (std::memory_order_acquire)) return;
                juce::ignoreUnused (songId);
                aiBusy.store (false, std::memory_order_release);
                aiProgress.store (ok ? 1.0f : 0.0f, std::memory_order_relaxed);
                aiStatusText = ok ? "Motor AI listo (100% local)" : errMsg;
                if (ok && activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size()))
                {
                    const auto& s = playlist[static_cast<size_t> (activeSongIndex)];
                    if (! s.isFreeTake && s.stemQuality != StemQuality::AiReady)
                        startAiSeparationForSong (activeSongIndex);
                }
                if (onStateChanged) onStateChanged();
            });
        aiThread->startThread (juce::Thread::Priority::low);
    }

    void startAiSeparationForSong (int index)
    {
        if (index < 0 || index >= static_cast<int> (playlist.size())) return;
        if (aiBusy.load (std::memory_order_relaxed)) return;
        const auto item = playlist[static_cast<size_t> (index)];
        if (item.isFreeTake || ! isDemucsModelReady()) return;

        if (aiThread != nullptr) { aiThread->stopThread (3000); aiThread.reset(); }
        playlist[static_cast<size_t> (index)].stemQuality = StemQuality::AiSeparating;
        aiBusy.store (true, std::memory_order_release);
        aiProgress.store (0.0f, std::memory_order_relaxed);
        aiStatusText = "Separando stems con AI local: " + item.title;

        auto tok = aliveToken;
        aiThread = std::make_unique<AiWorkerThread> (
            AiWorkerThread::Task::SeparateSong, item, engineSampleRate, tok,
            [this, tok] (float p, const juce::String& msg)
            {
                if (! tok || ! tok->load (std::memory_order_acquire)) return;
                aiProgress.store (p, std::memory_order_relaxed);
                aiStatusText = msg;
            },
            [this, tok] (bool ok, const juce::String& songId, const juce::String& errMsg)
            {
                if (! tok || ! tok->load (std::memory_order_acquire)) return;
                aiBusy.store (false, std::memory_order_release);
                aiProgress.store (ok ? 1.0f : 0.0f, std::memory_order_relaxed);
                aiStatusText = ok ? "Stems AI listos y guardados en cache" : errMsg;
                for (size_t i = 0; i < playlist.size(); ++i)
                {
                    if (playlist[i].id == songId)
                    {
                        playlist[i].stemQuality = ok ? StemQuality::AiReady : StemQuality::InstantDsp;
                        if (ok && static_cast<int> (i) == activeSongIndex)
                            reloadCachedAiStemsHotSwap (songId);
                        break;
                    }
                }
                savePlaylistToDisk();
                if (onStateChanged) onStateChanged();
            });
        aiThread->startThread (juce::Thread::Priority::low);
    }

    bool isAiBusy() const noexcept { return aiBusy.load (std::memory_order_relaxed); }
    float getAiProgress() const noexcept { return aiProgress.load (std::memory_order_relaxed); }
    juce::String getAiStatusText() const { return aiStatusText; }

    //==========================================================================
    // Transport, Loop A-B, Speed/Pitch, Metronome & Recording Controls
    //==========================================================================
    void play()
    {
        if (activeSongIndex < 0 && ! playlist.empty())
            loadSongAtIndex (0, true);

        endChoicePending.store (false, std::memory_order_release);
        if (preCountEnabled.load (std::memory_order_relaxed) && ! playing.load (std::memory_order_relaxed))
            armPreCount (false);
        else
        {
            needsStretchReset.store (true, std::memory_order_release);
            playing.store (true, std::memory_order_release);
        }
    }

    void pause()
    {
        playing.store (false, std::memory_order_release);
        recording.store (false, std::memory_order_release);
        preCountSamplesRemaining.store (0, std::memory_order_release);
        preCountBeatDisplay.store (0, std::memory_order_relaxed);
    }

    void stop()
    {
        pause();
        seekToSeconds (loopEnabled.load (std::memory_order_relaxed) ? loopStartSec.load (std::memory_order_relaxed) : 0.0);
    }

    void toggleRecord()
    {
        if (recording.load (std::memory_order_relaxed))
        {
            recording.store (false, std::memory_order_release);
            preCountSamplesRemaining.store (0, std::memory_order_release);
            preCountBeatDisplay.store (0, std::memory_order_relaxed);
            recomputeTakePeaks();
            return;
        }

        if (activeSongIndex < 0)
            addFreeTakeSession();

        const auto* st = activeStemsPtr.load (std::memory_order_acquire);
        const int targetCap = (st != nullptr ? st->numSamples : static_cast<int> (engineSampleRate * 180.0))
                            + static_cast<int> (engineSampleRate * 30.0);
        ensureTakeCapacity (targetCap);
        endChoicePending.store (false, std::memory_order_release);

        if (preCountEnabled.load (std::memory_order_relaxed) && ! playing.load (std::memory_order_relaxed))
        {
            armPreCount (true);
        }
        else
        {
            needsStretchReset.store (true, std::memory_order_release);
            recording.store (true, std::memory_order_release);
            playing.store (true, std::memory_order_release);
        }
    }

    void clearUserTake()
    {
        const bool wasRec = recording.load (std::memory_order_relaxed);
        recording.store (false, std::memory_order_release);
        juce::ignoreUnused (wasRec);
        const juce::SpinLock::ScopedLockType lock (takeLock);
        userTakeBuffer.clear();
        recordedLengthSamples.store (0, std::memory_order_relaxed);
        userTakePeaks.assign (LoadedSongStems::kPeakBins, 0.0f);
    }

    void seekToSeconds (double sec)
    {
        const double dur = getDurationSeconds();
        const double clamped = juce::jlimit (0.0, juce::jmax (0.0, dur), sec);
        playheadSample.store (static_cast<int64_t> (std::llround (clamped * engineSampleRate)), std::memory_order_release);
        needsStretchReset.store (true, std::memory_order_release);
    }

    double getPlayheadSeconds() const noexcept
    {
        return static_cast<double> (playheadSample.load (std::memory_order_relaxed)) / juce::jmax (1.0, engineSampleRate);
    }

    double getDurationSeconds() const noexcept
    {
        if (const auto* st = activeStemsPtr.load (std::memory_order_acquire))
            return static_cast<double> (st->numSamples) / juce::jmax (1.0, st->sampleRate);
        if (activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size()))
            return playlist[static_cast<size_t> (activeSongIndex)].durationSec;
        return 180.0;
    }

    bool isPlaying() const noexcept { return playing.load (std::memory_order_relaxed); }
    bool isRecording() const noexcept { return recording.load (std::memory_order_relaxed); }
    bool isInPreCount() const noexcept { return preCountSamplesRemaining.load (std::memory_order_relaxed) > 0; }
    int getPreCountBeat() const noexcept { return preCountBeatDisplay.load (std::memory_order_relaxed); }
    int getCurrentMetronomeBeat() const noexcept { return currentBeatDisplay.load (std::memory_order_relaxed); }

    void setLoopAHere()
    {
        const double t = getPlayheadSeconds();
        loopStartSec.store (t, std::memory_order_relaxed);
        if (loopEndSec.load (std::memory_order_relaxed) <= t + 0.5)
            loopEndSec.store (juce::jmin (getDurationSeconds(), t + 8.0), std::memory_order_relaxed);
    }

    void setLoopBHere()
    {
        const double t = getPlayheadSeconds();
        const double a = loopStartSec.load (std::memory_order_relaxed);
        if (t > a + 0.4)
            loopEndSec.store (t, std::memory_order_relaxed);
    }

    void setLoopEnabled (bool on)
    {
        loopEnabled.store (on, std::memory_order_relaxed);
    }
    bool isLoopEnabled() const noexcept { return loopEnabled.load (std::memory_order_relaxed); }
    double getLoopStartSeconds() const noexcept { return loopStartSec.load (std::memory_order_relaxed); }
    double getLoopEndSeconds() const noexcept { return loopEndSec.load (std::memory_order_relaxed); }

    void setPlaybackSpeed (float spd) { playbackSpeed.store (juce::jlimit (0.50f, 1.25f, spd), std::memory_order_relaxed); }
    float getPlaybackSpeed() const noexcept { return playbackSpeed.load (std::memory_order_relaxed); }

    void setPitchSemitones (int st) { pitchSemitones.store (juce::jlimit (-6, 6, st), std::memory_order_relaxed); }
    int getPitchSemitones() const noexcept { return pitchSemitones.load (std::memory_order_relaxed); }

    void setMetronomeEnabled (bool on) { metronomeEnabled.store (on, std::memory_order_relaxed); }
    bool isMetronomeEnabled() const noexcept { return metronomeEnabled.load (std::memory_order_relaxed); }

    void setPreCountEnabled (bool on) { preCountEnabled.store (on, std::memory_order_relaxed); }
    bool isPreCountEnabled() const noexcept { return preCountEnabled.load (std::memory_order_relaxed); }

    void setBpm (float newBpm)
    {
        const float clamped = juce::jlimit (40.0f, 240.0f, newBpm);
        bpm.store (clamped, std::memory_order_relaxed);
        if (activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size()))
            playlist[static_cast<size_t> (activeSongIndex)].bpm = clamped;
    }
    float getBpm() const noexcept { return bpm.load (std::memory_order_relaxed); }

    void tapTempo()
    {
        const double nowMs = juce::Time::getMillisecondCounterHiRes();
        if (! tapTimesMs.empty() && (nowMs - tapTimesMs.back()) > 2200.0)
            tapTimesMs.clear();
        tapTimesMs.push_back (nowMs);
        if (tapTimesMs.size() > 5)
            tapTimesMs.erase (tapTimesMs.begin());
        if (tapTimesMs.size() >= 2)
        {
            double sum = 0.0;
            for (size_t i = 1; i < tapTimesMs.size(); ++i)
                sum += (tapTimesMs[i] - tapTimesMs[i - 1]);
            const double avgMs = sum / static_cast<double> (tapTimesMs.size() - 1);
            if (avgMs > 200.0)
                setBpm (static_cast<float> (60000.0 / avgMs));
        }
    }

    // Per-track controls (0..4 stems, 5 take, 6 metronome)
    float getTrackVolume (int t) const noexcept { return trackVolume[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))].load (std::memory_order_relaxed); }
    void  setTrackVolume (int t, float v) { trackVolume[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))].store (juce::jlimit (0.0f, 1.5f, v), std::memory_order_relaxed); }
    bool  isTrackMuted (int t) const noexcept { return trackMute[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))].load (std::memory_order_relaxed); }
    void  setTrackMuted (int t, bool m) { trackMute[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))].store (m, std::memory_order_relaxed); }
    bool  isTrackSolo (int t) const noexcept { return trackSolo[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))].load (std::memory_order_relaxed); }
    void  setTrackSolo (int t, bool s) { trackSolo[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))].store (s, std::memory_order_relaxed); }
    bool  isTrackExportChecked (int t) const noexcept { return trackExportChecked[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))]; }
    void  setTrackExportChecked (int t, bool e) { trackExportChecked[static_cast<size_t> (juce::jlimit (0, kNumTracks - 1, t))] = e; }

    const LoadedSongStems* getLoadedStemsSnapshot() const noexcept { return activeStemsPtr.load (std::memory_order_acquire); }
    std::vector<float> getUserTakePeaksSnapshot() const
    {
        const juce::SpinLock::ScopedLockType lock (takeLock);
        return userTakePeaks;
    }
    bool hasRecordedTake() const noexcept { return recordedLengthSamples.load (std::memory_order_relaxed) > 0; }

    // Called from message-thread timer (~30 Hz) to handle end-of-song & live take waveform updates
    bool pollEndOfSongEvent()
    {
        if (! trackEndedFlag.exchange (false, std::memory_order_acq_rel))
            return false;

        if (recording.load (std::memory_order_relaxed))
            recording.store (false, std::memory_order_release);
        recomputeTakePeaks();

        if (endMode == PlaylistEndMode::Continuous)
        {
            const int nextIdx = activeSongIndex + 1;
            if (nextIdx < static_cast<int> (playlist.size()))
            {
                loadSongAtIndex (nextIdx, true);
                if (onSongAutoAdvanced) onSongAutoAdvanced (nextIdx);
            }
            else
            {
                seekToSeconds (0.0);
            }
        }
        else
        {
            endChoicePending.store (true, std::memory_order_release);
        }
        return true;
    }

    bool isEndChoicePending() const noexcept { return endChoicePending.load (std::memory_order_relaxed); }
    void dismissEndChoiceAndRepeat()
    {
        endChoicePending.store (false, std::memory_order_release);
        seekToSeconds (0.0);
        play();
    }
    void dismissEndChoiceAndNext()
    {
        endChoicePending.store (false, std::memory_order_release);
        if (! playlist.empty())
        {
            const int nextIdx = (activeSongIndex + 1) % static_cast<int> (playlist.size());
            loadSongAtIndex (nextIdx, true);
            if (onSongAutoAdvanced) onSongAutoAdvanced (nextIdx);
        }
    }
    void dismissEndChoiceOnly()
    {
        endChoicePending.store (false, std::memory_order_release);
    }

    void updateLiveRecordingPeaksIfNeeded()
    {
        if (recording.load (std::memory_order_relaxed))
            recomputeTakePeaks();
        // Clean up retired stem snapshots safely after >= 2 seconds so audio thread never sees a freed pointer
        const uint32_t nowMs = juce::Time::getMillisecondCounter();
        while (! retiredStems.empty() && (nowMs - retiredStems.front().retiredAtMs) > 2000)
            retiredStems.erase (retiredStems.begin());
    }

    //==========================================================================
    // Real-Time Audio Thread Processing (Called from MiAmpAudioProcessor::processBlock)
    //==========================================================================
    void processBlockPlaybackAndRecord (juce::AudioBuffer<float>& ioAmpBuffer)
    {
        const int numSamples = ioAmpBuffer.getNumSamples();
        if (numSamples <= 0 || ioAmpBuffer.getNumChannels() < 2)
            return;

        if (! playing.load (std::memory_order_acquire))
            return;

        // 1. Pre-count (1-2-3-4) before playback/recording starts
        int64_t preRem = preCountSamplesRemaining.load (std::memory_order_acquire);
        if (preRem > 0)
        {
            const float curBpm = bpm.load (std::memory_order_relaxed);
            const double samplesPerBeat = juce::jmax (1000.0, (60.0 / curBpm) * engineSampleRate);
            const int64_t totalPre = static_cast<int64_t> (std::llround (4.0 * samplesPerBeat));

            float* outL = ioAmpBuffer.getWritePointer (0);
            float* outR = ioAmpBuffer.getWritePointer (1);

            for (int i = 0; i < numSamples; ++i)
            {
                const int64_t elapsed = totalPre - preRem;
                const int beatIdx = static_cast<int> (static_cast<double> (elapsed) / samplesPerBeat);
                const int beatSample = static_cast<int> (elapsed - static_cast<int64_t> (beatIdx * samplesPerBeat));
                if (beatSample == 0)
                {
                    clickPhase = 0.0f;
                    clickEnv = 1.0f;
                    clickFreq = (beatIdx == 0) ? 1320.0f : 880.0f;
                    preCountBeatDisplay.store (juce::jlimit (1, 4, beatIdx + 1), std::memory_order_relaxed);
                }
                const float click = nextMetronomeSample() * 0.55f;
                outL[i] += click;
                outR[i] += click;

                if (--preRem <= 0)
                {
                    preCountSamplesRemaining.store (0, std::memory_order_release);
                    preCountBeatDisplay.store (0, std::memory_order_relaxed);
                    needsStretchReset.store (true, std::memory_order_release);
                    if (pendingRecordAfterPreCount.exchange (false, std::memory_order_acq_rel))
                        recording.store (true, std::memory_order_release);
                    break;
                }
            }
            preCountSamplesRemaining.store (preRem, std::memory_order_release);
            return;
        }

        const LoadedSongStems* stems = activeStemsPtr.load (std::memory_order_acquire);
        const int64_t timelineLen = (stems != nullptr && stems->numSamples > 0)
                                        ? static_cast<int64_t> (stems->numSamples)
                                        : static_cast<int64_t> (engineSampleRate * 180.0);

        int64_t pos = playheadSample.load (std::memory_order_relaxed);
        const float spd = playbackSpeed.load (std::memory_order_relaxed);
        const int pitchSt = pitchSemitones.load (std::memory_order_relaxed);
        const bool loopOn = loopEnabled.load (std::memory_order_relaxed);
        const int64_t safeTimeline = juce::jmax<int64_t> (2048, timelineLen);
        const int64_t loopA = juce::jlimit<int64_t> (0, safeTimeline - 1024, static_cast<int64_t> (loopStartSec.load (std::memory_order_relaxed) * engineSampleRate));
        const int64_t loopB = juce::jlimit<int64_t> (loopA + 1024, safeTimeline, static_cast<int64_t> (loopEndSec.load (std::memory_order_relaxed) * engineSampleRate));

        // 2. Capture live processed MI AMP tone into User Take Track if armed & recording
        const bool isRec = recording.load (std::memory_order_acquire);
        if (isRec)
        {
            const juce::SpinLock::ScopedTryLockType tryLock (takeLock);
            if (tryLock.isLocked() && userTakeBuffer.getNumSamples() > 0)
            {
                const int maxCap = userTakeBuffer.getNumSamples();
                float* tkL = userTakeBuffer.getWritePointer (0);
                float* tkR = userTakeBuffer.getWritePointer (1);
                const float* ampL = ioAmpBuffer.getReadPointer (0);
                const float* ampR = ioAmpBuffer.getReadPointer (1);
                for (int i = 0; i < numSamples; ++i)
                {
                    const int dst = static_cast<int> (pos + static_cast<int64_t> (std::floor (i * spd)));
                    if (dst >= 0 && dst < maxCap)
                    {
                        tkL[dst] = ampL[i];
                        tkR[dst] = ampR[i];
                        if (dst + 1 > recordedLengthSamples.load (std::memory_order_relaxed))
                            recordedLengthSamples.store (dst + 1, std::memory_order_relaxed);
                    }
                }
            }
        }

        // Determine Mute/Solo gains
        bool anySolo = false;
        for (int t = 0; t < kNumTracks; ++t)
            if (trackSolo[static_cast<size_t> (t)].load (std::memory_order_relaxed)) { anySolo = true; break; }

        auto effGain = [&] (int t) -> float
        {
            const size_t idx = static_cast<size_t> (t);
            if (trackMute[idx].load (std::memory_order_relaxed)) return 0.0f;
            if (anySolo && ! trackSolo[idx].load (std::memory_order_relaxed)) return 0.0f;
            return trackVolume[idx].load (std::memory_order_relaxed);
        };

        float stemGains[kNumStems];
        for (int s = 0; s < kNumStems; ++s) stemGains[s] = effGain (s);
        const float takeGain = isRec ? 0.0f : effGain (kTrackTake);
        const float metGain  = metronomeEnabled.load (std::memory_order_relaxed) ? effGain (kTrackMet) : 0.0f;

        if (needsStretchReset.exchange (false, std::memory_order_acq_rel))
        {
            backingStretch.reset();
            takeStretch.reset();
            speedFractAccum = 0.0;
        }

        const bool bypassStretch = (std::abs (spd - 1.0f) < 0.005f && pitchSt == 0);
        float* outL = ioAmpBuffer.getWritePointer (0);
        float* outR = ioAmpBuffer.getWritePointer (1);

        const float curBpm = bpm.load (std::memory_order_relaxed);
        const double samplesPerBeat = juce::jmax (1000.0, (60.0 / curBpm) * engineSampleRate);

        if (bypassStretch)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                if (loopOn && pos >= loopB)
                    pos = loopA;

                if (pos >= timelineLen)
                {
                    if (stems != nullptr && stems->isFreeTake)
                    {
                        pos = 0;
                    }
                    else
                    {
                        playing.store (false, std::memory_order_release);
                        recording.store (false, std::memory_order_release);
                        trackEndedFlag.store (true, std::memory_order_release);
                        break;
                    }
                }

                float mixL = 0.0f, mixR = 0.0f;
                if (stems != nullptr && ! stems->isFreeTake && pos < stems->numSamples)
                {
                    for (int s = 0; s < kNumStems; ++s)
                    {
                        const float g = stemGains[s];
                        if (g <= 0.0001f) continue;
                        mixL += stems->stems[static_cast<size_t> (s)].getSample (0, static_cast<int> (pos)) * g;
                        mixR += stems->stems[static_cast<size_t> (s)].getSample (1, static_cast<int> (pos)) * g;
                    }
                }

                if (takeGain > 0.0001f && pos < recordedLengthSamples.load (std::memory_order_relaxed))
                {
                    const juce::SpinLock::ScopedTryLockType tryLock (takeLock);
                    if (tryLock.isLocked() && pos < userTakeBuffer.getNumSamples())
                    {
                        mixL += userTakeBuffer.getSample (0, static_cast<int> (pos)) * takeGain;
                        mixR += userTakeBuffer.getSample (1, static_cast<int> (pos)) * takeGain;
                    }
                }

                if (metGain > 0.0001f)
                {
                    const double beatPos = std::fmod (static_cast<double> (pos), samplesPerBeat);
                    if (beatPos < 1.0)
                    {
                        const int beatInBar = static_cast<int> (std::floor (static_cast<double> (pos) / samplesPerBeat)) % 4;
                        clickPhase = 0.0f;
                        clickEnv = 1.0f;
                        clickFreq = (beatInBar == 0) ? 1320.0f : 880.0f;
                        currentBeatDisplay.store (beatInBar + 1, std::memory_order_relaxed);
                    }
                    const float c = nextMetronomeSample() * metGain * 0.55f;
                    mixL += c;
                    mixR += c;
                }

                outL[i] += mixL;
                outR[i] += mixR;
                ++pos;
            }
        }
        else
        {
            // Time-Stretch & Pitch-Shift via Signalsmith Stretch
            const double exactIn = static_cast<double> (numSamples) * static_cast<double> (spd) + speedFractAccum;
            const int inFrames = juce::jlimit (1, stretchInBuf.getNumSamples(), static_cast<int> (std::floor (exactIn)));
            speedFractAccum = exactIn - static_cast<double> (inFrames);

            float* inL = stretchInBuf.getWritePointer (0);
            float* inR = stretchInBuf.getWritePointer (1);
            float* tkInL = takeInBuf.getWritePointer (0);
            float* tkInR = takeInBuf.getWritePointer (1);

            bool hitEnd = false;
            bool hasTakeSamples = false;
            const int64_t recLen = recordedLengthSamples.load (std::memory_order_relaxed);

            for (int i = 0; i < inFrames; ++i)
            {
                if (loopOn && pos >= loopB)
                    pos = loopA;

                if (pos >= timelineLen)
                {
                    if (stems != nullptr && stems->isFreeTake)
                        pos = 0;
                    else
                    {
                        hitEnd = true;
                        inL[i] = inR[i] = tkInL[i] = tkInR[i] = 0.0f;
                        continue;
                    }
                }

                float sL = 0.0f, sR = 0.0f;
                if (stems != nullptr && ! stems->isFreeTake && pos < stems->numSamples)
                {
                    for (int s = 0; s < kNumStems; ++s)
                    {
                        const float g = stemGains[s];
                        if (g <= 0.0001f) continue;
                        sL += stems->stems[static_cast<size_t> (s)].getSample (0, static_cast<int> (pos)) * g;
                        sR += stems->stems[static_cast<size_t> (s)].getSample (1, static_cast<int> (pos)) * g;
                    }
                }
                inL[i] = sL;
                inR[i] = sR;

                if (takeGain > 0.0001f && pos < recLen)
                {
                    const juce::SpinLock::ScopedTryLockType tryLock (takeLock);
                    if (tryLock.isLocked() && pos < userTakeBuffer.getNumSamples())
                    {
                        tkInL[i] = userTakeBuffer.getSample (0, static_cast<int> (pos)) * takeGain;
                        tkInR[i] = userTakeBuffer.getSample (1, static_cast<int> (pos)) * takeGain;
                        hasTakeSamples = true;
                    }
                    else
                        tkInL[i] = tkInR[i] = 0.0f;
                }
                else
                    tkInL[i] = tkInR[i] = 0.0f;

                if (metGain > 0.0001f)
                {
                    const double beatPos = std::fmod (static_cast<double> (pos), samplesPerBeat);
                    if (beatPos < 1.5)
                    {
                        const int beatInBar = static_cast<int> (std::floor (static_cast<double> (pos) / samplesPerBeat)) % 4;
                        clickPhase = 0.0f;
                        clickEnv = 1.0f;
                        clickFreq = (beatInBar == 0) ? 1320.0f : 880.0f;
                        currentBeatDisplay.store (beatInBar + 1, std::memory_order_relaxed);
                    }
                }
                ++pos;
            }

            if (std::abs (static_cast<float> (pitchSt) - lastBackingPitch) > 0.01f)
            {
                backingStretch.setTransposeSemitones (static_cast<float> (pitchSt));
                lastBackingPitch = static_cast<float> (pitchSt);
            }
            float* bInPtrs[2]  = { inL, inR };
            float* bOutPtrs[2] = { stretchOutBuf.getWritePointer (0), stretchOutBuf.getWritePointer (1) };
            backingStretch.process (bInPtrs, inFrames, bOutPtrs, numSamples);

            float* tOutL = takeOutBuf.getWritePointer (0);
            float* tOutR = takeOutBuf.getWritePointer (1);
            if (hasTakeSamples)
            {
                if (std::abs (static_cast<float> (pitchSt) - lastTakePitch) > 0.01f)
                {
                    takeStretch.setTransposeSemitones (static_cast<float> (pitchSt));
                    lastTakePitch = static_cast<float> (pitchSt);
                }
                float* tInPtrs[2]  = { tkInL, tkInR };
                float* tOutPtrs[2] = { tOutL, tOutR };
                takeStretch.process (tInPtrs, inFrames, tOutPtrs, numSamples);
            }
            else
            {
                std::fill_n (tOutL, numSamples, 0.0f);
                std::fill_n (tOutR, numSamples, 0.0f);
            }

            for (int i = 0; i < numSamples; ++i)
            {
                float c = (metGain > 0.0001f) ? (nextMetronomeSample() * metGain * 0.55f) : 0.0f;
                outL[i] += bOutPtrs[0][i] + tOutL[i] + c;
                outR[i] += bOutPtrs[1][i] + tOutR[i] + c;
            }

            if (hitEnd)
            {
                playing.store (false, std::memory_order_release);
                recording.store (false, std::memory_order_release);
                trackEndedFlag.store (true, std::memory_order_release);
            }
        }

        playheadSample.store (pos, std::memory_order_release);
    }

    //==========================================================================
    // Selective .WAV Export (Mix, Checked Stems, or User Take Only)
    //==========================================================================
    bool exportMixWav (const juce::File& targetFile)
    {
        const auto* st = activeStemsPtr.load (std::memory_order_acquire);
        const int64_t recLen = recordedLengthSamples.load (std::memory_order_relaxed);
        const int totalIn = (st != nullptr && ! st->isFreeTake) ? st->numSamples : static_cast<int> (recLen);
        if (totalIn <= 0) return false;

        const float spd = playbackSpeed.load (std::memory_order_relaxed);
        const int pitchSt = pitchSemitones.load (std::memory_order_relaxed);
        const bool applyStretch = (std::abs (spd - 1.0f) >= 0.005f || pitchSt != 0);

        juce::AudioBuffer<float> rawMix (2, totalIn);
        rawMix.clear();
        const double sr = (st != nullptr) ? st->sampleRate : engineSampleRate;
        const double samplesPerBeat = juce::jmax (1000.0, (60.0 / bpm.load (std::memory_order_relaxed)) * sr);

        for (int s = 0; s < kNumStems; ++s)
        {
            if (st == nullptr || st->isFreeTake || ! trackExportChecked[static_cast<size_t> (s)]) continue;
            const float g = trackVolume[static_cast<size_t> (s)].load (std::memory_order_relaxed);
            const int n = juce::jmin (totalIn, st->stems[static_cast<size_t> (s)].getNumSamples());
            rawMix.addFrom (0, 0, st->stems[static_cast<size_t> (s)], 0, 0, n, g);
            rawMix.addFrom (1, 0, st->stems[static_cast<size_t> (s)], 1, 0, n, g);
        }

        if (trackExportChecked[kTrackTake] && recLen > 0)
        {
            const juce::SpinLock::ScopedLockType lock (takeLock);
            const float g = trackVolume[kTrackTake].load (std::memory_order_relaxed);
            const int n = juce::jmin (totalIn, static_cast<int> (recLen), userTakeBuffer.getNumSamples());
            rawMix.addFrom (0, 0, userTakeBuffer, 0, 0, n, g);
            rawMix.addFrom (1, 0, userTakeBuffer, 1, 0, n, g);
        }

        if (trackExportChecked[kTrackMet])
        {
            const float g = trackVolume[kTrackMet].load (std::memory_order_relaxed) * 0.55f;
            float ph = 0.0f, env = 0.0f, fr = 880.0f;
            for (int i = 0; i < totalIn; ++i)
            {
                if (std::fmod (static_cast<double> (i), samplesPerBeat) < 1.0)
                {
                    const int b = static_cast<int> (std::floor (static_cast<double> (i) / samplesPerBeat)) % 4;
                    ph = 0.0f; env = 1.0f; fr = (b == 0) ? 1320.0f : 880.0f;
                }
                if (env > 0.0001f)
                {
                    const float c = std::sin (ph) * env * g;
                    rawMix.addSample (0, i, c);
                    rawMix.addSample (1, i, c);
                    ph += juce::MathConstants<float>::twoPi * fr / static_cast<float> (sr);
                    env *= 0.9965f;
                }
            }
        }

        juce::AudioBuffer<float> finalBuf;
        if (applyStretch)
        {
            const int outLen = juce::jmax (1024, static_cast<int> (std::llround (static_cast<double> (totalIn) / static_cast<double> (spd))));
            finalBuf.setSize (2, outLen);
            signalsmith::stretch::SignalsmithStretch<float> offlineStretch;
            offlineStretch.presetDefault (2, static_cast<float> (sr));
            offlineStretch.setTransposeSemitones (static_cast<float> (pitchSt));
            const float* inPtrs[2] = { rawMix.getReadPointer (0), rawMix.getReadPointer (1) };
            float* outPtrs[2]      = { finalBuf.getWritePointer (0), finalBuf.getWritePointer (1) };
            offlineStretch.exact (inPtrs, totalIn, outPtrs, outLen);
        }
        else
        {
            finalBuf = std::move (rawMix);
        }

        normalizeSafeCeiling (finalBuf, 0.94f);
        return writeWav24 (targetFile, finalBuf, sr);
    }

    int exportCheckedStemsToFolder (const juce::File& folder)
    {
        if (! folder.isDirectory()) folder.createDirectory();
        const auto* st = activeStemsPtr.load (std::memory_order_acquire);
        const juce::String base = (activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size()))
                                      ? juce::File::createLegalFileName (playlist[static_cast<size_t> (activeSongIndex)].title)
                                      : juce::String ("MiAmp_Ensayo");
        const double sr = (st != nullptr) ? st->sampleRate : engineSampleRate;
        int written = 0;

        static constexpr const char* kSuffixes[kNumTracks] = {
            "VOZ", "GUITARRA", "BAJO", "BATERIA", "OTROS", "MI_TOMA", "METRONOMO"
        };

        for (int s = 0; s < kNumStems; ++s)
        {
            if (st == nullptr || st->isFreeTake || ! trackExportChecked[static_cast<size_t> (s)]) continue;
            if (writeWav24 (folder.getChildFile (base + "_" + kSuffixes[s] + ".wav"), st->stems[static_cast<size_t> (s)], sr))
                ++written;
        }

        if (trackExportChecked[kTrackTake] && hasRecordedTake())
        {
            if (exportUserTakeWav (folder.getChildFile (base + "_" + kSuffixes[kTrackTake] + ".wav")))
                ++written;
        }
        return written;
    }

    bool exportUserTakeWav (const juce::File& targetFile)
    {
        const int len = static_cast<int> (recordedLengthSamples.load (std::memory_order_relaxed));
        if (len <= 0) return false;
        juce::AudioBuffer<float> copyBuf (2, len);
        {
            const juce::SpinLock::ScopedLockType lock (takeLock);
            const int n = juce::jmin (len, userTakeBuffer.getNumSamples());
            copyBuf.copyFrom (0, 0, userTakeBuffer, 0, 0, n);
            copyBuf.copyFrom (1, 0, userTakeBuffer, 1, 0, n);
        }
        normalizeSafeCeiling (copyBuf, 0.94f);
        return writeWav24 (targetFile, copyBuf, engineSampleRate);
    }

    std::function<void()> onStateChanged;
    std::function<void(int)> onSongAutoAdvanced;

private:
    static void sanitizeAudioBuffer (juce::AudioBuffer<float>& buf, int numSamples) noexcept
    {
        const int chs = buf.getNumChannels();
        const int n   = juce::jmin (numSamples, buf.getNumSamples());
        for (int c = 0; c < chs; ++c)
        {
            float* p = buf.getWritePointer (c);
            for (int i = 0; i < n; ++i)
                if (! std::isfinite (p[i]))
                    p[i] = 0.0f;
        }
    }

    static bool resampleStereoSafe (const juce::AudioBuffer<float>& srcBuf, double srcSr,
                                    double dstSr, int srcLen, juce::AudioBuffer<float>& outBuf)
    {
        if (srcLen <= 0 || srcBuf.getNumChannels() <= 0 || srcSr <= 0.0 || dstSr <= 0.0)
            return false;

        // Allocate +32 zero-padded tail samples so LagrangeInterpolator 5-tap stencil never reads out of bounds
        juce::AudioBuffer<float> stereoSrc (2, srcLen + 32);
        stereoSrc.clear();
        const int chR = srcBuf.getNumChannels() > 1 ? 1 : 0;
        stereoSrc.copyFrom (0, 0, srcBuf, 0,   0, srcLen);
        stereoSrc.copyFrom (1, 0, srcBuf, chR, 0, srcLen);

        if (std::abs (srcSr - dstSr) < 1.0)
        {
            outBuf.setSize (2, srcLen, false, true, true);
            outBuf.copyFrom (0, 0, stereoSrc, 0, 0, srcLen);
            outBuf.copyFrom (1, 0, stereoSrc, 1, 0, srcLen);
            return true;
        }

        const double ratio = srcSr / dstSr;
        const int dstLen = juce::jmax (1, static_cast<int> (std::floor (static_cast<double> (srcLen) / ratio)));
        outBuf.setSize (2, dstLen, false, true, true);
        for (int c = 0; c < 2; ++c)
        {
            juce::LagrangeInterpolator interp;
            interp.reset();
            interp.process (ratio, stereoSrc.getReadPointer (c), outBuf.getWritePointer (c), dstLen);
        }
        sanitizeAudioBuffer (outBuf, dstLen);
        return true;
    }

    static bool readStereoAudioFileInner (const juce::File* filePtr, double targetSr, juce::Thread* threadPtr,
                                          juce::AudioBuffer<float>* outBufPtr)
    {
        if (filePtr == nullptr || threadPtr == nullptr || outBufPtr == nullptr || ! filePtr->existsAsFile())
            return false;

        const juce::File& file = *filePtr;
        juce::AudioFormatManager localFormats;

        // Prefer pure-C++ decoders first so MP3/WAV/FLAC/AIFF/OGG never touch Windows Wmvcore.dll / COM
        localFormats.registerFormat (new juce::WavAudioFormat(), true);
        localFormats.registerFormat (new juce::AiffAudioFormat(), false);
       #if defined(JUCE_USE_FLAC) && JUCE_USE_FLAC
        localFormats.registerFormat (new juce::FlacAudioFormat(), false);
       #endif
       #if defined(JUCE_USE_OGGVORBIS) && JUCE_USE_OGGVORBIS
        localFormats.registerFormat (new juce::OggVorbisAudioFormat(), false);
       #endif
       #if defined(JUCE_USE_MP3AUDIOFORMAT) && JUCE_USE_MP3AUDIOFORMAT
        localFormats.registerFormat (new juce::MP3AudioFormat(), false);
       #endif

        std::unique_ptr<juce::AudioFormatReader> reader (localFormats.createReaderFor (file));
        if (reader == nullptr)
        {
            // Fallback (e.g. M4A / WMA / CoreAudio) only if pure-C++ formats could not decode
            juce::AudioFormatManager fallbackFormats;
            fallbackFormats.registerBasicFormats();
            reader.reset (fallbackFormats.createReaderFor (file));
        }

        if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0 || reader->numChannels <= 0)
            return false;

        const int srcLen = static_cast<int> (juce::jmin<int64_t> (reader->lengthInSamples, static_cast<int64_t> (reader->sampleRate * 600.0)));
        if (srcLen <= 0)
            return false;

        juce::AudioBuffer<float> srcBuf (2, srcLen);
        srcBuf.clear();

        constexpr int kReadChunk = 65536;
        int pos = 0;
        while (pos < srcLen && ! threadPtr->threadShouldExit())
        {
            const int nThis = juce::jmin (kReadChunk, srcLen - pos);
            if (! reader->read (&srcBuf, pos, nThis, static_cast<int64_t> (pos), true, true))
                break;
            pos += nThis;
        }
        if (threadPtr->threadShouldExit() || pos <= 0)
            return false;

        sanitizeAudioBuffer (srcBuf, pos);
        const bool ok = resampleStereoSafe (srcBuf, reader->sampleRate, targetSr, pos, *outBufPtr);
        srcBuf.setSize (0, 0);
        return ok;
    }

   #if defined(_MSC_VER)
    __declspec(noinline) static bool sehReadStereoAudioFile (const juce::File* filePtr, double targetSr, juce::Thread* threadPtr,
                                                             juce::AudioBuffer<float>* outBufPtr, bool* outSehFault) noexcept
    {
        __try
        {
            return readStereoAudioFileInner (filePtr, targetSr, threadPtr, outBufPtr);
        }
        __except (1)
        {
            if (outSehFault) *outSehFault = true;
            return false;
        }
    }
   #endif

    static bool readStereoAudioFileSafe (const juce::File& file, double targetSr, juce::Thread& thread,
                                         juce::AudioBuffer<float>& outBuf)
    {
       #if defined(_MSC_VER)
        bool sehFault = false;
        const bool ok = sehReadStereoAudioFile (&file, targetSr, &thread, &outBuf, &sehFault);
        return ! sehFault && ok;
       #else
        return readStereoAudioFileInner (&file, targetSr, &thread, &outBuf);
       #endif
    }

   #if defined(MIAMP_HAS_DEMUCS) && MIAMP_HAS_DEMUCS
    static juce::File findOnnxRuntimeLibraryFile()
    {
       #if defined(_WIN32)
        const char* libName = "onnxruntime.dll";
        const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const auto appDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getParentDirectory();
        const std::array<juce::File, 5> candidates = {
            exeDir.getChildFile (libName),
            appDir.getChildFile (libName),
            exeDir.getParentDirectory().getChildFile (libName),
            getModelsDir().getChildFile (libName),
            juce::File ("C:\\Program Files\\Popups Audio\\MI AMP\\onnxruntime.dll")
        };
       #elif defined(__APPLE__)
        const char* libName = "libonnxruntime.dylib";
        const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const auto appDir = juce::File::getSpecialLocation (juce::File::currentApplicationFile);
        const std::array<juce::File, 5> candidates = {
            exeDir.getParentDirectory().getChildFile ("Frameworks").getChildFile (libName),
            appDir.getChildFile ("Contents").getChildFile ("Frameworks").getChildFile (libName),
            exeDir.getChildFile (libName),
            getModelsDir().getChildFile (libName),
            juce::File ("/Applications/MI AMP.app/Contents/Frameworks/libonnxruntime.dylib")
        };
       #else
        const char* libName = "libonnxruntime.so";
        const auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
        const std::array<juce::File, 2> candidates = {
            exeDir.getChildFile (libName),
            getModelsDir().getChildFile (libName)
        };
       #endif
        for (const auto& c : candidates)
            if (c.existsAsFile())
                return c;
        return {};
    }

    static const OrtApi* getOrtApi (juce::String& outErr)
    {
        static std::mutex sOrtMutex;
        static std::unique_ptr<juce::DynamicLibrary> sOrtLib;
        static const OrtApi* sOrtApi = nullptr;

        const std::lock_guard<std::mutex> lock (sOrtMutex);
        if (sOrtApi != nullptr)
            return sOrtApi;

        const auto libFile = findOnnxRuntimeLibraryFile();
        if (! libFile.existsAsFile())
        {
            outErr = "No se encontro onnxruntime junto a MI AMP";
            return nullptr;
        }

        auto lib = std::make_unique<juce::DynamicLibrary>();
        if (! lib->open (libFile.getFullPathName()))
        {
            outErr = "No se pudo cargar " + libFile.getFileName();
            return nullptr;
        }

        using GetApiBaseFn = const OrtApiBase* (ORT_API_CALL*) (void);
        auto fnPtr = lib->getFunction ("OrtGetApiBase");
        if (fnPtr == nullptr)
        {
            outErr = "Simbolo OrtGetApiBase no encontrado en onnxruntime";
            return nullptr;
        }

        auto getBase = reinterpret_cast<GetApiBaseFn> (fnPtr);
        const OrtApiBase* base = getBase != nullptr ? getBase() : nullptr;
        if (base == nullptr || base->GetApi == nullptr)
        {
            outErr = "OrtGetApiBase devolvio nulo";
            return nullptr;
        }

        const OrtApi* api = base->GetApi (ORT_API_VERSION);
        if (api == nullptr)
        {
            static constexpr uint32_t kFallbacks[] = { 23u, 22u, 21u, 20u, 17u, 15u };
            for (uint32_t v : kFallbacks)
            {
                api = base->GetApi (v);
                if (api != nullptr) break;
            }
        }
        if (api == nullptr)
        {
            outErr = "Version de ONNX Runtime incompatible";
            return nullptr;
        }

        sOrtLib = std::move (lib);
        sOrtApi = api;
        return sOrtApi;
    }

    static bool runOnnxSeparationInner (const juce::File* modelFilePtr,
                                        const juce::AudioBuffer<float>* audio44Ptr,
                                        std::array<juce::AudioBuffer<float>, kNumStems>* outStemsPtr,
                                        juce::Thread* threadPtr,
                                        const std::function<void(float, const juce::String&, bool)>* progCbPtr,
                                        juce::String* outErrPtr)
    {
        auto setErr = [&] (const juce::String& m)
        {
            if (outErrPtr) *outErrPtr = m;
            return false;
        };

        juce::String ortErr;
        const OrtApi* ort = getOrtApi (ortErr);
        if (ort == nullptr)
            return setErr (ortErr);

        OrtEnv* env = nullptr;
        OrtSessionOptions* sessOpts = nullptr;
        OrtSession* session = nullptr;
        OrtMemoryInfo* memInfo = nullptr;

        auto cleanup = [&]
        {
            if (memInfo != nullptr)  { ort->ReleaseMemoryInfo (memInfo); memInfo = nullptr; }
            if (session != nullptr)  { ort->ReleaseSession (session); session = nullptr; }
            if (sessOpts != nullptr) { ort->ReleaseSessionOptions (sessOpts); sessOpts = nullptr; }
            if (env != nullptr)      { ort->ReleaseEnv (env); env = nullptr; }
        };

        auto checkStatus = [&] (OrtStatus* st, const char* stage) -> bool
        {
            if (st == nullptr) return true;
            const char* msg = ort->GetErrorMessage (st);
            const juce::String s = juce::String (stage) + ": " + juce::String::fromUTF8 (msg ? msg : "error");
            ort->ReleaseStatus (st);
            setErr (s);
            return false;
        };

        if (! checkStatus (ort->CreateEnv (ORT_LOGGING_LEVEL_WARNING, "MiAmpPractice", &env), "CreateEnv"))
        {
            cleanup(); return false;
        }
        if (! checkStatus (ort->CreateSessionOptions (&sessOpts), "CreateSessionOptions"))
        {
            cleanup(); return false;
        }
        ort->SetSessionGraphOptimizationLevel (sessOpts, ORT_ENABLE_ALL);
        const unsigned int hw = std::thread::hardware_concurrency();
        const int nThreads = juce::jlimit (1, 6, hw > 1 ? static_cast<int> (hw / 2) : 2);
        ort->SetIntraOpNumThreads (sessOpts, nThreads);

        const juce::String modelPathStr = modelFilePtr->getFullPathName();
       #if defined(_WIN32)
        OrtStatus* stSess = ort->CreateSession (env, modelPathStr.toWideCharPointer(), sessOpts, &session);
       #else
        OrtStatus* stSess = ort->CreateSession (env, modelPathStr.toRawUTF8(), sessOpts, &session);
       #endif
        if (! checkStatus (stSess, "CreateSession"))
        {
            cleanup(); return false;
        }

        if (! checkStatus (ort->CreateCpuMemoryInfo (OrtArenaAllocator, OrtMemTypeDefault, &memInfo), "CreateCpuMemoryInfo"))
        {
            cleanup(); return false;
        }

        const int totalSamples = audio44Ptr->getNumSamples();
        const float* inL = audio44Ptr->getReadPointer (0);
        const float* inR = audio44Ptr->getReadPointer (1);

        // HTDemucs-6s ONNX fixed segment spec: (1, 2, 343980) -> stems (1, 6, 2, 343980)
        // Order: 0=drums, 1=bass, 2=other, 3=vocals, 4=guitar, 5=piano
        constexpr int kSegSamples = 343980;
        constexpr int kOverlap    = kSegSamples / 4; // 85995
        constexpr int kStride     = kSegSamples - kOverlap; // 257985

        std::vector<float> window (static_cast<size_t> (kSegSamples), 1.0f);
        for (int i = 0; i < kOverlap; ++i)
        {
            const float fade = static_cast<float> (i) / static_cast<float> (juce::jmax (1, kOverlap - 1));
            window[static_cast<size_t> (i)] = fade;
            window[static_cast<size_t> (kSegSamples - 1 - i)] = fade;
        }

        for (int s = 0; s < kNumStems; ++s)
        {
            (*outStemsPtr)[static_cast<size_t> (s)].setSize (2, totalSamples, false, true, true);
            (*outStemsPtr)[static_cast<size_t> (s)].clear();
        }
        std::vector<float> weightAcc (static_cast<size_t> (totalSamples), 0.0f);
        std::vector<float> mixChunk (static_cast<size_t> (2 * kSegSamples), 0.0f);

        const int nChunks = juce::jmax (1, (totalSamples + kStride - 1) / kStride);
        const int64_t inputShape[3] = { 1, 2, kSegSamples };
        const char* inputNames[1]  = { "mix" };
        const char* outputNames[1] = { "stems" };

        for (int ci = 0; ci < nChunks && ! threadPtr->threadShouldExit(); ++ci)
        {
            const int start = ci * kStride;
            const int end   = juce::jmin (start + kSegSamples, totalSamples);
            const int cLen  = end - start;
            if (cLen <= 0) break;

            std::fill (mixChunk.begin(), mixChunk.end(), 0.0f);
            double energy = 0.0;
            for (int i = 0; i < cLen; ++i)
            {
                const float l = inL[start + i];
                const float r = inR[start + i];
                mixChunk[static_cast<size_t> (i)] = l;
                mixChunk[static_cast<size_t> (kSegSamples + i)] = r;
                energy += static_cast<double> (l) * l + static_cast<double> (r) * r;
            }

            if (energy >= 1e-9)
            {
                OrtValue* inTensor = nullptr;
                OrtValue* outTensor = nullptr;
                if (! checkStatus (ort->CreateTensorWithDataAsOrtValue (
                                       memInfo, mixChunk.data(), mixChunk.size() * sizeof (float),
                                       inputShape, 3, ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT, &inTensor),
                                   "CreateInputTensor"))
                {
                    cleanup(); return false;
                }

                OrtStatus* runSt = ort->Run (session, nullptr, inputNames,
                                             const_cast<const OrtValue* const*> (&inTensor), 1,
                                             outputNames, 1, &outTensor);
                ort->ReleaseValue (inTensor);

                if (! checkStatus (runSt, "Run"))
                {
                    if (outTensor != nullptr) ort->ReleaseValue (outTensor);
                    cleanup(); return false;
                }

                float* outData = nullptr;
                if (! checkStatus (ort->GetTensorMutableData (outTensor, reinterpret_cast<void**> (&outData)), "GetOutputData")
                    || outData == nullptr)
                {
                    ort->ReleaseValue (outTensor);
                    cleanup(); return false;
                }

                auto accumStem = [&] (int dstStem, int ortSrcA, int ortSrcB = -1)
                {
                    float* dL = (*outStemsPtr)[static_cast<size_t> (dstStem)].getWritePointer (0);
                    float* dR = (*outStemsPtr)[static_cast<size_t> (dstStem)].getWritePointer (1);
                    const float* sAL = outData + static_cast<size_t> (ortSrcA * 2 + 0) * kSegSamples;
                    const float* sAR = outData + static_cast<size_t> (ortSrcA * 2 + 1) * kSegSamples;
                    const float* sBL = ortSrcB >= 0 ? (outData + static_cast<size_t> (ortSrcB * 2 + 0) * kSegSamples) : nullptr;
                    const float* sBR = ortSrcB >= 0 ? (outData + static_cast<size_t> (ortSrcB * 2 + 1) * kSegSamples) : nullptr;

                    for (int i = 0; i < cLen; ++i)
                    {
                        float l = sAL[i] + (sBL != nullptr ? sBL[i] : 0.0f);
                        float r = sAR[i] + (sBR != nullptr ? sBR[i] : 0.0f);
                        if (! std::isfinite (l)) l = 0.0f;
                        if (! std::isfinite (r)) r = 0.0f;
                        const float w = window[static_cast<size_t> (i)];
                        dL[start + i] += l * w;
                        dR[start + i] += r * w;
                    }
                };

                // HTDemucs-6s order: 0=drums, 1=bass, 2=other, 3=vocals, 4=guitar, 5=piano
                accumStem (0, 3);     // VOZ
                accumStem (1, 4);     // GUITARRA
                accumStem (2, 1);     // BAJO
                accumStem (3, 0);     // BATERIA
                accumStem (4, 2, 5);  // OTROS / FX (other + piano)

                ort->ReleaseValue (outTensor);
            }

            for (int i = 0; i < cLen; ++i)
                weightAcc[static_cast<size_t> (start + i)] += window[static_cast<size_t> (i)];

            if (progCbPtr && *progCbPtr)
            {
                const float p = 0.12f + (0.82f * static_cast<float> (ci + 1)) / static_cast<float> (nChunks);
                (*progCbPtr) (p, "Separando 6 pistas AI (ONNX): " + juce::String (juce::roundToInt (p * 100.0f)) + "%", false);
            }
        }

        cleanup();
        if (threadPtr->threadShouldExit())
            return setErr ("Separacion cancelada");

        for (int s = 0; s < kNumStems; ++s)
        {
            float* dL = (*outStemsPtr)[static_cast<size_t> (s)].getWritePointer (0);
            float* dR = (*outStemsPtr)[static_cast<size_t> (s)].getWritePointer (1);
            for (int i = 0; i < totalSamples; ++i)
            {
                const float w = juce::jmax (1e-8f, weightAcc[static_cast<size_t> (i)]);
                dL[i] /= w;
                dR[i] /= w;
            }
        }

        return true;
    }

   #if defined(_MSC_VER)
    __declspec(noinline) static bool sehRunOnnxSeparation (const juce::File* modelFilePtr,
                                                           const juce::AudioBuffer<float>* audio44Ptr,
                                                           std::array<juce::AudioBuffer<float>, kNumStems>* outStemsPtr,
                                                           juce::Thread* threadPtr,
                                                           const std::function<void(float, const juce::String&, bool)>* progCbPtr,
                                                           juce::String* outErrPtr,
                                                           bool* outSehFault) noexcept
    {
        __try
        {
            return runOnnxSeparationInner (modelFilePtr, audio44Ptr, outStemsPtr, threadPtr, progCbPtr, outErrPtr);
        }
        __except (1)
        {
            if (outSehFault) *outSehFault = true;
            return false;
        }
    }
   #endif
   #endif

    class SongLoaderThread final : public juce::Thread
    {
    public:
        SongLoaderThread (PracticeSongItem songItem, double targetSr,
                          std::shared_ptr<std::atomic<bool>> alive,
                          std::function<void(std::shared_ptr<LoadedSongStems>, bool)> onDone)
            : juce::Thread ("MiAmpSongLoader", 2 * 1024 * 1024),
              item (std::move (songItem)), targetSampleRate (targetSr),
              aliveFlag (std::move (alive)), doneCb (std::move (onDone)) {}

        void run() override
        {
            try
            {
                const auto cacheDir = getSongCacheDir (item.id);
                bool hasAiCache = true;
                for (int s = 0; s < kNumStems; ++s)
                    if (! cacheDir.getChildFile (stemFileName (s)).existsAsFile()) { hasAiCache = false; break; }

                auto res = std::make_shared<LoadedSongStems>();
                res->songId = item.id;
                res->sampleRate = targetSampleRate;
                res->isFreeTake = false;

                if (hasAiCache)
                {
                    bool allOk = true;
                    int maxLen = 0;
                    for (int s = 0; s < kNumStems && ! threadShouldExit(); ++s)
                    {
                        juce::AudioBuffer<float> tmp;
                        if (! readStereoAudioFileSafe (cacheDir.getChildFile (stemFileName (s)), targetSampleRate, *this, tmp))
                        {
                            allOk = false;
                            break;
                        }
                        maxLen = juce::jmax (maxLen, tmp.getNumSamples());
                        res->stems[static_cast<size_t> (s)] = std::move (tmp);
                    }
                    if (allOk && maxLen > 0 && ! threadShouldExit())
                    {
                        res->numSamples = maxLen;
                        res->isAiSeparated = true;
                        for (int s = 0; s < kNumStems; ++s)
                            computePeaksForBuffer (res->stems[static_cast<size_t> (s)], maxLen, LoadedSongStems::kPeakBins, res->peaks[static_cast<size_t> (s)]);
                        auto tok = aliveFlag;
                        juce::MessageManager::callAsync ([cb = doneCb, tok, res]
                        {
                            if (tok && tok->load (std::memory_order_acquire) && cb)
                                cb (res, false);
                        });
                        return;
                    }
                }

                if (threadShouldExit()) return;

                juce::AudioBuffer<float> fullAudio;
                if (! readStereoAudioFileSafe (juce::File (item.audioFilePath), targetSampleRate, *this, fullAudio) || fullAudio.getNumSamples() <= 0)
                {
                    if (! threadShouldExit())
                    {
                        auto tok = aliveFlag;
                        juce::MessageManager::callAsync ([cb = doneCb, tok]
                        {
                            if (tok && tok->load (std::memory_order_acquire) && cb)
                                cb (nullptr, false);
                        });
                    }
                    return;
                }

                res->numSamples = fullAudio.getNumSamples();
                res->isAiSeparated = false;
                splitStemsDspInstant (fullAudio, targetSampleRate, res->stems);
                fullAudio.setSize (0, 0);
                for (int s = 0; s < kNumStems; ++s)
                    computePeaksForBuffer (res->stems[static_cast<size_t> (s)], res->numSamples, LoadedSongStems::kPeakBins, res->peaks[static_cast<size_t> (s)]);

                auto tok = aliveFlag;
                juce::MessageManager::callAsync ([cb = doneCb, tok, res]
                {
                    if (tok && tok->load (std::memory_order_acquire) && cb)
                        cb (res, true);
                });
            }
            catch (...)
            {
                if (! threadShouldExit())
                {
                    auto tok = aliveFlag;
                    juce::MessageManager::callAsync ([cb = doneCb, tok]
                    {
                        if (tok && tok->load (std::memory_order_acquire) && cb)
                            cb (nullptr, false);
                    });
                }
            }
        }

        PracticeSongItem item;
        double targetSampleRate;
        std::shared_ptr<std::atomic<bool>> aliveFlag;
        std::function<void(std::shared_ptr<LoadedSongStems>, bool)> doneCb;
    };

    class AiWorkerThread final : public juce::Thread
    {
    public:
        enum class Task { DownloadModel, SeparateSong };
        AiWorkerThread (Task t, PracticeSongItem songItem, double sr,
                        std::shared_ptr<std::atomic<bool>> alive,
                        std::function<void(float, const juce::String&)> progressCb,
                        std::function<void(bool, const juce::String&, const juce::String&)> doneCb)
            : juce::Thread ("MiAmpDemucsAI", 8 * 1024 * 1024),
              task (t), item (std::move (songItem)),
              targetSr (sr), aliveFlag (std::move (alive)),
              onProg (std::move (progressCb)), onDone (std::move (doneCb)) {}

        void run() override
        {
            try
            {
                if (task == Task::DownloadModel)
                {
                    runDownload();
                    return;
                }
                runSeparation();
            }
            catch (const std::exception& e)
            {
                postDone (false, juce::String ("Error AI: ") + e.what());
            }
            catch (...)
            {
                postDone (false, "Error inesperado en el motor AI");
            }
        }

    private:
        void runDownload()
        {
            const auto dest = getDemucsModelFile();
            const auto part = dest.getSiblingFile (dest.getFileName() + ".part");
            part.deleteFile();

            // Clean up legacy ggml model if present
            const auto legacyBin = getModelsDir().getChildFile ("ggml-model-htdemucs-6s-f16.bin");
            if (legacyBin.existsAsFile()) legacyBin.deleteFile();

            juce::URL url (demucsModelDownloadUrl());
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (25000)
                            .withNumRedirectsToFollow (6)
                            .withExtraHeaders ("User-Agent: MiAmp-PracticeStudio\r\n");

            std::unique_ptr<juce::InputStream> stream (url.createInputStream (opts));
            if (stream == nullptr)
            {
                postDone (false, "Sin conexion para descargar el motor AI");
                return;
            }

            const int64_t streamLen = stream->getTotalLength();
            const int64_t total = streamLen > 0 ? streamLen : 136000000;
            juce::FileOutputStream fos (part);
            if (! fos.openedOk())
            {
                postDone (false, "No se pudo escribir el archivo del modelo AI");
                return;
            }

            std::vector<uint8_t> buf (65536);
            int64_t readTotal = 0;
            while (! threadShouldExit())
            {
                const int got = stream->read (buf.data(), static_cast<int> (buf.size()));
                if (got <= 0) break;
                fos.write (buf.data(), static_cast<size_t> (got));
                readTotal += got;
                const float p = juce::jlimit (0.0f, 0.99f, static_cast<float> (readTotal) / static_cast<float> (total));
                postProg (p, "Descargando motor AI (ONNX): " + juce::String (juce::roundToInt (p * 100.0f)) + "%");
            }
            fos.flush();

            if (threadShouldExit() || readTotal < 130000000 || (streamLen > 0 && readTotal < streamLen))
            {
                part.deleteFile();
                postDone (false, threadShouldExit() ? "Descarga cancelada" : "Descarga incompleta");
                return;
            }
            // Reject accidental HTML/JSON error responses
            {
                juce::FileInputStream fis (part);
                char head[16] = {};
                if (! fis.openedOk() || fis.read (head, 8) < 8 || head[0] == '<' || head[0] == '{')
                {
                    part.deleteFile();
                    postDone (false, "Archivo de modelo ONNX invalido");
                    return;
                }
            }
            dest.deleteFile();
            if (! part.moveFileTo (dest))
            {
                postDone (false, "No se pudo guardar el modelo AI");
                return;
            }
            postDone (true, {});
        }

        void runSeparation()
        {
           #if defined(MIAMP_HAS_DEMUCS) && MIAMP_HAS_DEMUCS
            const auto modelFile = getDemucsModelFile();
            if (! isDemucsModelReady())
            {
                postDone (false, "Falta descargar el motor AI");
                return;
            }

            postProg (0.04f, "Decodificando audio a 44.1 kHz...", true);
            const juce::File srcFile (item.audioFilePath);
            if (! srcFile.existsAsFile())
            {
                postDone (false, "Archivo de audio no encontrado");
                return;
            }

            juce::AudioBuffer<float> audio44;
            constexpr double demucsSr = 44100.0;
            if (! readStereoAudioFileSafe (srcFile, demucsSr, *this, audio44))
            {
                postDone (false, "No se pudo leer o remuestrear el archivo de audio");
                return;
            }
            if (threadShouldExit()) return;

            const int n44 = audio44.getNumSamples();
            if (n44 < 4410)
            {
                postDone (false, "El audio es demasiado corto para separar pistas");
                return;
            }

            postProg (0.08f, "Cargando modelo HTDemucs-6s (ONNX Runtime)...", true);
            std::array<juce::AudioBuffer<float>, kNumStems> outStems;
            juce::String ortErr;
            std::function<void(float, const juce::String&, bool)> progFn =
                [this] (float p, const juce::String& msg, bool force) { postProg (p, msg, force); };

            bool inferOk = false;
            bool sehFault = false;
           #if defined(_MSC_VER)
            inferOk = sehRunOnnxSeparation (&modelFile, &audio44, &outStems, this, &progFn, &ortErr, &sehFault);
           #else
            inferOk = runOnnxSeparationInner (&modelFile, &audio44, &outStems, this, &progFn, &ortErr);
           #endif

            audio44.setSize (0, 0);
            if (threadShouldExit()) return;

            if (sehFault || ! inferOk)
            {
                postDone (false, sehFault ? "Fallo protegido en ONNX Runtime (usando DSP)"
                                          : (ortErr.isNotEmpty() ? ortErr : juce::String ("Error en separacion AI")));
                return;
            }

            postProg (0.96f, "Guardando stems locales (.wav)...", true);
            const auto cacheDir = getSongCacheDir (item.id);
            bool ok = true;
            for (int s = 0; s < kNumStems; ++s)
                if (! writeWav24Static (cacheDir.getChildFile (stemFileName (s)), outStems[static_cast<size_t> (s)], demucsSr))
                {
                    ok = false;
                    break;
                }

            postDone (ok, ok ? juce::String() : "Error guardando cache WAV");
           #else
            juce::ignoreUnused (targetSr);
            postDone (false, "Motor ONNX AI disponible en build CMake");
           #endif
        }

        void postProg (float p, const juce::String& msg, bool force = false)
        {
            const uint32_t now = juce::Time::getMillisecondCounter();
            if (! force && (now - lastProgPostMs) < 120)
                return;
            lastProgPostMs = now;
            auto tok = aliveFlag;
            juce::MessageManager::callAsync ([cb = onProg, tok, p, msg]
            {
                if (tok && tok->load (std::memory_order_acquire) && cb)
                    cb (p, msg);
            });
        }
        void postDone (bool ok, const juce::String& err)
        {
            auto tok = aliveFlag;
            juce::MessageManager::callAsync ([cb = onDone, tok, ok, id = item.id, err]
            {
                if (tok && tok->load (std::memory_order_acquire) && cb)
                    cb (ok, id, err);
            });
        }

        Task task;
        PracticeSongItem item;
        double targetSr;
        std::shared_ptr<std::atomic<bool>> aliveFlag;
        uint32_t lastProgPostMs = 0;
        std::function<void(float, const juce::String&)> onProg;
        std::function<void(bool, const juce::String&, const juce::String&)> onDone;
    };

    void reloadCachedAiStemsHotSwap (const juce::String& songId)
    {
        if (activeSongIndex < 0 || activeSongIndex >= static_cast<int> (playlist.size())) return;
        const auto item = playlist[static_cast<size_t> (activeSongIndex)];
        if (item.id != songId) return;

        if (loaderThread != nullptr) { loaderThread->signalThreadShouldExit(); loaderThread->stopThread (2000); }
        auto tok = aliveToken;
        loaderThread = std::make_unique<SongLoaderThread> (
            item, engineSampleRate, tok,
            [this, tok] (std::shared_ptr<LoadedSongStems> loaded, bool)
            {
                if (! tok || ! tok->load (std::memory_order_acquire) || loaded == nullptr) return;
                setActiveStems (loaded);
                if (onStateChanged) onStateChanged();
            });
        loaderThread->startThread (juce::Thread::Priority::normal);
    }

    void setActiveStems (std::shared_ptr<LoadedSongStems> next)
    {
        if (activeStemsOwner != nullptr)
            retiredStems.push_back ({ activeStemsOwner, juce::Time::getMillisecondCounter() });
        activeStemsOwner = std::move (next);
        activeStemsPtr.store (activeStemsOwner.get(), std::memory_order_release);
    }

    void ensureTakeCapacity (int minSamples)
    {
        const int target = juce::jmax (minSamples, static_cast<int> (engineSampleRate * 180.0));
        const juce::SpinLock::ScopedLockType lock (takeLock);
        if (userTakeBuffer.getNumSamples() < target)
            userTakeBuffer.setSize (2, target, true, true, true);
        if (userTakePeaks.size() != static_cast<size_t> (LoadedSongStems::kPeakBins))
            userTakePeaks.assign (LoadedSongStems::kPeakBins, 0.0f);
    }

    void recomputeTakePeaks()
    {
        const int recLen = static_cast<int> (recordedLengthSamples.load (std::memory_order_relaxed));
        const juce::SpinLock::ScopedLockType lock (takeLock);
        const int timeline = (activeStemsPtr.load (std::memory_order_acquire) != nullptr)
                                 ? juce::jmax (1, activeStemsPtr.load (std::memory_order_acquire)->numSamples)
                                 : juce::jmax (1, recLen);
        userTakePeaks.assign (LoadedSongStems::kPeakBins, 0.0f);
        if (recLen <= 0 || userTakeBuffer.getNumSamples() <= 0) return;

        const float* ch0 = userTakeBuffer.getReadPointer (0);
        const float* ch1 = userTakeBuffer.getReadPointer (1);
        const int avail = juce::jmin (recLen, userTakeBuffer.getNumSamples());
        for (int b = 0; b < LoadedSongStems::kPeakBins; ++b)
        {
            const int s0 = (b * timeline) / LoadedSongStems::kPeakBins;
            const int s1 = juce::jmax (s0 + 1, ((b + 1) * timeline) / LoadedSongStems::kPeakBins);
            if (s0 >= avail) break;
            const int step = juce::jmax (1, (s1 - s0) / 32);
            float pk = 0.0f;
            for (int s = s0; s < s1 && s < avail; s += step)
                pk = juce::jmax (pk, 0.5f * (std::fabs (ch0[s]) + std::fabs (ch1[s])));
            userTakePeaks[static_cast<size_t> (b)] = juce::jlimit (0.0f, 1.0f, pk * 1.25f);
        }
    }

    void armPreCount (bool startRecordingAfter)
    {
        const float curBpm = bpm.load (std::memory_order_relaxed);
        const double samplesPerBeat = juce::jmax (1000.0, (60.0 / curBpm) * engineSampleRate);
        const int64_t totalPre = static_cast<int64_t> (std::llround (4.0 * samplesPerBeat));
        pendingRecordAfterPreCount.store (startRecordingAfter, std::memory_order_release);
        preCountBeatDisplay.store (1, std::memory_order_relaxed);
        preCountSamplesRemaining.store (totalPre, std::memory_order_release);
        playing.store (true, std::memory_order_release);
    }

    float nextMetronomeSample() noexcept
    {
        if (clickEnv <= 0.0001f) return 0.0f;
        const float s = std::sin (clickPhase) * clickEnv;
        clickPhase += juce::MathConstants<float>::twoPi * clickFreq / static_cast<float> (engineSampleRate);
        if (clickPhase > juce::MathConstants<float>::twoPi)
            clickPhase -= juce::MathConstants<float>::twoPi;
        clickEnv *= 0.9965f;
        return s;
    }

    static void normalizeSafeCeiling (juce::AudioBuffer<float>& buf, float ceiling)
    {
        float peak = 0.0f;
        for (int c = 0; c < buf.getNumChannels(); ++c)
            peak = juce::jmax (peak, buf.getMagnitude (c, 0, buf.getNumSamples()));
        if (peak > ceiling && peak > 1e-6f)
            buf.applyGain (ceiling / peak);
    }

    static bool writeWav24Static (const juce::File& file, const juce::AudioBuffer<float>& buf, double sr)
    {
        file.deleteFile();
        std::unique_ptr<juce::FileOutputStream> fos (file.createOutputStream());
        if (fos == nullptr || ! fos->openedOk()) return false;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            wav.createWriterFor (fos.get(), sr, static_cast<unsigned int> (buf.getNumChannels()), 24, {}, 0));
        if (writer == nullptr) return false;
        fos.release();
        return writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }

    bool writeWav24 (const juce::File& file, const juce::AudioBuffer<float>& buf, double sr) const
    {
        return writeWav24Static (file, buf, sr);
    }

    void savePlaylistToDisk() const
    {
        juce::XmlElement root ("MiAmpPracticePlaylist");
        root.setAttribute ("endMode", static_cast<int> (endMode));
        root.setAttribute ("activeIndex", activeSongIndex);
        for (const auto& s : playlist)
        {
            auto* e = root.createNewChildElement ("Song");
            e->setAttribute ("id", s.id);
            e->setAttribute ("title", s.title);
            e->setAttribute ("path", s.audioFilePath);
            e->setAttribute ("freeTake", s.isFreeTake);
            e->setAttribute ("durationSec", s.durationSec);
            e->setAttribute ("bpm", static_cast<double> (s.bpm));
            e->setAttribute ("linkedPreset", s.linkedPresetName);
            e->setAttribute ("linkedPresetBass", s.linkedPresetIsBass);
            e->setAttribute ("autoLoadPreset", s.autoLoadPreset);
        }
        root.writeTo (getPlaylistXmlFile());
    }

    void loadPlaylistFromDisk()
    {
        const auto file = getPlaylistXmlFile();
        if (! file.existsAsFile()) return;
        if (auto xml = juce::XmlDocument::parse (file))
        {
            endMode = xml->getIntAttribute ("endMode", 0) == 1 ? PlaylistEndMode::PauseAndChoose : PlaylistEndMode::Continuous;
            playlist.clear();
            for (auto* e : xml->getChildIterator())
            {
                if (! e->hasTagName ("Song")) continue;
                PracticeSongItem item;
                item.id = e->getStringAttribute ("id");
                item.title = e->getStringAttribute ("title");
                item.audioFilePath = e->getStringAttribute ("path");
                item.isFreeTake = e->getBoolAttribute ("freeTake", false);
                item.durationSec = e->getDoubleAttribute ("durationSec", 180.0);
                item.bpm = static_cast<float> (e->getDoubleAttribute ("bpm", 120.0));
                item.linkedPresetName = e->getStringAttribute ("linkedPreset");
                item.linkedPresetIsBass = e->getBoolAttribute ("linkedPresetBass", false);
                item.autoLoadPreset = e->getBoolAttribute ("autoLoadPreset", true);
                if (! item.isFreeTake && ! juce::File (item.audioFilePath).existsAsFile())
                    continue;
                const auto cacheDir = getSongCacheDir (item.id);
                bool allCached = ! item.isFreeTake;
                if (allCached)
                    for (int s = 0; s < kNumStems; ++s)
                        if (! cacheDir.getChildFile (stemFileName (s)).existsAsFile()) { allCached = false; break; }
                item.stemQuality = item.isFreeTake ? StemQuality::None : (allCached ? StemQuality::AiReady : StemQuality::InstantDsp);
                playlist.push_back (std::move (item));
            }
            activeSongIndex = xml->getIntAttribute ("activeIndex", playlist.empty() ? -1 : 0);
            if (activeSongIndex >= static_cast<int> (playlist.size()))
                activeSongIndex = playlist.empty() ? -1 : 0;
        }
    }

    struct RetiredStemEntry
    {
        std::shared_ptr<LoadedSongStems> stems;
        uint32_t retiredAtMs = 0;
    };

    double engineSampleRate = 44100.0;

    std::vector<PracticeSongItem> playlist;
    int activeSongIndex = -1;
    PlaylistEndMode endMode = PlaylistEndMode::Continuous;

    std::shared_ptr<LoadedSongStems> activeStemsOwner;
    std::vector<RetiredStemEntry> retiredStems;
    std::atomic<const LoadedSongStems*> activeStemsPtr { nullptr };

    juce::SpinLock takeLock;
    juce::AudioBuffer<float> userTakeBuffer;
    std::atomic<int64_t> recordedLengthSamples { 0 };
    std::vector<float> userTakePeaks;

    std::array<std::atomic<float>, kNumTracks> trackVolume;
    std::array<std::atomic<bool>, kNumTracks>  trackMute;
    std::array<std::atomic<bool>, kNumTracks>  trackSolo;
    std::array<bool, kNumTracks>               trackExportChecked {};

    std::atomic<bool> playing { false };
    std::atomic<bool> recording { false };
    std::atomic<bool> pendingRecordAfterPreCount { false };
    std::atomic<bool> trackEndedFlag { false };
    std::atomic<bool> endChoicePending { false };
    std::atomic<int64_t> playheadSample { 0 };
    std::atomic<int64_t> preCountSamplesRemaining { 0 };
    std::atomic<int> preCountBeatDisplay { 0 };
    std::atomic<int> currentBeatDisplay { 1 };

    std::atomic<bool>   loopEnabled { false };
    std::atomic<double> loopStartSec { 0.0 };
    std::atomic<double> loopEndSec   { 180.0 };
    std::atomic<float>  playbackSpeed { 1.0f };
    std::atomic<int>    pitchSemitones { 0 };
    std::atomic<bool>   metronomeEnabled { true };
    std::atomic<bool>   preCountEnabled { true };
    std::atomic<float>  bpm { 120.0f };
    std::atomic<bool>   needsStretchReset { true };

    signalsmith::stretch::SignalsmithStretch<float> backingStretch, takeStretch;
    juce::AudioBuffer<float> stretchInBuf, stretchOutBuf, takeInBuf, takeOutBuf;
    float lastBackingPitch = 999.0f, lastTakePitch = 999.0f;
    double speedFractAccum = 0.0;
    float clickPhase = 0.0f, clickEnv = 0.0f, clickFreq = 880.0f;

    std::vector<double> tapTimesMs;

    std::shared_ptr<std::atomic<bool>> aliveToken = std::make_shared<std::atomic<bool>> (true);
    std::unique_ptr<SongLoaderThread> loaderThread;
    std::unique_ptr<AiWorkerThread>   aiThread;
    std::atomic<bool>  aiBusy { false };
    std::atomic<float> aiProgress { 0.0f };
    juce::String aiStatusText;
};

} // namespace miamp_practice
