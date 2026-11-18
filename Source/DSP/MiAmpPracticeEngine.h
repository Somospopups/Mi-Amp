#pragma once
#include <JuceHeader.h>
#include "../ThirdParty/signalsmith/signalsmith-stretch.h"

#if defined(MIAMP_HAS_DEMUCS) && MIAMP_HAS_DEMUCS
 #include "model.hpp"
#endif

#include <array>
#include <atomic>
#include <cmath>
#include <memory>
#include <mutex>
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
    return getModelsDir().getChildFile ("ggml-model-htdemucs-6s-f16.bin");
}

inline bool isDemucsModelReady()
{
    const auto f = getDemucsModelFile();
    return f.existsAsFile() && f.getSize() >= 52000000;
}

inline juce::String demucsModelDownloadUrl()
{
    return "https://huggingface.co/datasets/Retrobear/demucs.cpp/resolve/main/ggml-model-htdemucs-6s-f16.bin";
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
        formatManager.registerBasicFormats();
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

        // If a song is already selected but stems were loaded at a different sample rate, reload them
        if (activeSongIndex >= 0 && activeSongIndex < static_cast<int> (playlist.size()))
        {
            const auto* cur = activeStemsPtr.load (std::memory_order_acquire);
            if (cur == nullptr || std::abs (cur->sampleRate - engineSampleRate) > 1.0)
                loadSongAtIndex (activeSongIndex, false);
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

            // Detect duration cheaply via reader
            if (auto r = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (f)))
            {
                if (r->sampleRate > 0.0)
                    item.durationSec = static_cast<double> (r->lengthInSamples) / r->sampleRate;
            }
            const auto cacheDir = getSongCacheDir (item.id);
            bool allCached = true;
            for (int s = 0; s < kNumStems; ++s)
                if (! cacheDir.getChildFile (stemFileName (s)).existsAsFile()) { allCached = false; break; }
            item.stemQuality = allCached ? StemQuality::AiReady : StemQuality::InstantDsp;

            if (firstAdded < 0) firstAdded = static_cast<int> (playlist.size());
            playlist.push_back (item);
        }
        savePlaylistToDisk();
        if (firstAdded >= 0 && (activeSongIndex < 0 || playlist.size() == static_cast<size_t> (files.size())))
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
        loaderThread = std::make_unique<SongLoaderThread> (
            item, sr, formatManager,
            [this, autoPlayAfterLoad] (std::shared_ptr<LoadedSongStems> loaded, bool shouldStartAi)
            {
                if (loaded == nullptr) return;
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
        aiThread = std::make_unique<AiWorkerThread> (
            AiWorkerThread::Task::DownloadModel, PracticeSongItem{}, 0.0, formatManager,
            [this] (float p, const juce::String& msg)
            {
                aiProgress.store (p, std::memory_order_relaxed);
                aiStatusText = msg;
            },
            [this] (bool ok, const juce::String& songId, const juce::String& errMsg)
            {
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

        aiThread = std::make_unique<AiWorkerThread> (
            AiWorkerThread::Task::SeparateSong, item, engineSampleRate, formatManager,
            [this] (float p, const juce::String& msg)
            {
                aiProgress.store (p, std::memory_order_relaxed);
                aiStatusText = msg;
            },
            [this] (bool ok, const juce::String& songId, const juce::String& errMsg)
            {
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
        // Clean up retired stem snapshots safely after a short grace period
        if (retiredStems.size() > 3)
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
        const int64_t loopA = juce::jlimit<int64_t> (0, timelineLen - 1, static_cast<int64_t> (loopStartSec.load (std::memory_order_relaxed) * engineSampleRate));
        const int64_t loopB = juce::jlimit<int64_t> (loopA + 1024, timelineLen, static_cast<int64_t> (loopEndSec.load (std::memory_order_relaxed) * engineSampleRate));

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
    class SongLoaderThread final : public juce::Thread
    {
    public:
        SongLoaderThread (PracticeSongItem songItem, double targetSr, juce::AudioFormatManager& fm,
                          std::function<void(std::shared_ptr<LoadedSongStems>, bool)> onDone)
            : juce::Thread ("MiAmpSongLoader"), item (std::move (songItem)), targetSampleRate (targetSr),
              formats (fm), doneCb (std::move (onDone)) {}

        void run() override
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
                    if (! readAndResampleFile (cacheDir.getChildFile (stemFileName (s)), targetSampleRate, tmp))
                    {
                        allOk = false;
                        break;
                    }
                    maxLen = juce::jmax (maxLen, tmp.getNumSamples());
                    res->stems[static_cast<size_t> (s)] = std::move (tmp);
                }
                if (allOk && maxLen > 0)
                {
                    res->numSamples = maxLen;
                    res->isAiSeparated = true;
                    for (int s = 0; s < kNumStems; ++s)
                        computePeaksForBuffer (res->stems[static_cast<size_t> (s)], maxLen, LoadedSongStems::kPeakBins, res->peaks[static_cast<size_t> (s)]);
                    juce::MessageManager::callAsync ([cb = doneCb, res] { if (cb) cb (res, false); });
                    return;
                }
            }

            if (threadShouldExit()) return;

            juce::AudioBuffer<float> fullAudio;
            if (! readAndResampleFile (juce::File (item.audioFilePath), targetSampleRate, fullAudio) || fullAudio.getNumSamples() <= 0)
                return;

            res->numSamples = fullAudio.getNumSamples();
            res->isAiSeparated = false;
            splitStemsDspInstant (fullAudio, targetSampleRate, res->stems);
            for (int s = 0; s < kNumStems; ++s)
                computePeaksForBuffer (res->stems[static_cast<size_t> (s)], res->numSamples, LoadedSongStems::kPeakBins, res->peaks[static_cast<size_t> (s)]);

            juce::MessageManager::callAsync ([cb = doneCb, res] { if (cb) cb (res, true); });
        }

        bool readAndResampleFile (const juce::File& file, double targetSr, juce::AudioBuffer<float>& outBuf)
        {
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
            if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
                return false;

            const int srcLen = static_cast<int> (juce::jmin<int64_t> (reader->lengthInSamples, static_cast<int64_t> (reader->sampleRate * 900.0)));
            juce::AudioBuffer<float> srcBuf (juce::jmax (1, static_cast<int> (reader->numChannels)), srcLen);
            if (! reader->read (&srcBuf, 0, srcLen, 0, true, true))
                return false;

            // Ensure stereo
            juce::AudioBuffer<float> stereoSrc (2, srcLen);
            stereoSrc.copyFrom (0, 0, srcBuf, 0, 0, srcLen);
            stereoSrc.copyFrom (1, 0, srcBuf, srcBuf.getNumChannels() > 1 ? 1 : 0, 0, srcLen);

            if (std::abs (reader->sampleRate - targetSr) < 1.0)
            {
                outBuf = std::move (stereoSrc);
                return true;
            }

            const double ratio = reader->sampleRate / targetSr;
            const int dstLen = juce::jmax (1, static_cast<int> (std::llround (static_cast<double> (srcLen) / ratio)));
            outBuf.setSize (2, dstLen);
            for (int c = 0; c < 2; ++c)
            {
                juce::LagrangeInterpolator interp;
                interp.reset();
                interp.process (ratio, stereoSrc.getReadPointer (c), outBuf.getWritePointer (c), dstLen);
            }
            return true;
        }

        PracticeSongItem item;
        double targetSampleRate;
        juce::AudioFormatManager& formats;
        std::function<void(std::shared_ptr<LoadedSongStems>, bool)> doneCb;
    };

    class AiWorkerThread final : public juce::Thread
    {
    public:
        enum class Task { DownloadModel, SeparateSong };
        AiWorkerThread (Task t, PracticeSongItem songItem, double sr, juce::AudioFormatManager& fm,
                        std::function<void(float, const juce::String&)> progressCb,
                        std::function<void(bool, const juce::String&, const juce::String&)> doneCb)
            : juce::Thread ("MiAmpDemucsAI"), task (t), item (std::move (songItem)),
              targetSr (sr), formats (fm), onProg (std::move (progressCb)), onDone (std::move (doneCb)) {}

        void run() override
        {
            if (task == Task::DownloadModel)
            {
                runDownload();
                return;
            }
            runSeparation();
        }

    private:
        void runDownload()
        {
            const auto dest = getDemucsModelFile();
            const auto part = dest.getSiblingFile (dest.getFileName() + ".part");
            part.deleteFile();

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

            const int64_t total = stream->getTotalLength() > 0 ? stream->getTotalLength() : 54855129;
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
                postProg (p, "Descargando motor AI: " + juce::String (juce::roundToInt (p * 100.0f)) + "%");
            }
            fos.flush();

            if (threadShouldExit() || readTotal < 52000000)
            {
                part.deleteFile();
                postDone (false, threadShouldExit() ? "Descarga cancelada" : "Descarga incompleta");
                return;
            }
            // Validate GGML magic 'dmc6' (0x646d6336)
            {
                juce::FileInputStream fis (part);
                uint32_t magic = 0;
                if (! fis.openedOk() || fis.read (&magic, 4) < 4 || magic != 0x646d6336u)
                {
                    part.deleteFile();
                    postDone (false, "Archivo de modelo invalido");
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

            postProg (0.04f, "Decodificando audio a 44.1 kHz...");
            std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (juce::File (item.audioFilePath)));
            if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
            {
                postDone (false, "No se pudo leer el archivo de audio");
                return;
            }

            const int srcLen = static_cast<int> (juce::jmin<int64_t> (reader->lengthInSamples, static_cast<int64_t> (reader->sampleRate * 900.0)));
            juce::AudioBuffer<float> srcBuf (juce::jmax (1, static_cast<int> (reader->numChannels)), srcLen);
            if (! reader->read (&srcBuf, 0, srcLen, 0, true, true))
            {
                postDone (false, "Error leyendo muestras de audio");
                return;
            }

            juce::AudioBuffer<float> audio44;
            const double demucsSr = static_cast<double> (demucscpp::SUPPORTED_SAMPLE_RATE);
            if (std::abs (reader->sampleRate - demucsSr) < 1.0)
            {
                audio44.setSize (2, srcLen);
                audio44.copyFrom (0, 0, srcBuf, 0, 0, srcLen);
                audio44.copyFrom (1, 0, srcBuf, srcBuf.getNumChannels() > 1 ? 1 : 0, 0, srcLen);
            }
            else
            {
                const double ratio = reader->sampleRate / demucsSr;
                const int dstLen = juce::jmax (1, static_cast<int> (std::llround (static_cast<double> (srcLen) / ratio)));
                audio44.setSize (2, dstLen);
                for (int c = 0; c < 2; ++c)
                {
                    juce::LagrangeInterpolator interp;
                    interp.reset();
                    interp.process (ratio, srcBuf.getReadPointer (c < srcBuf.getNumChannels() ? c : 0),
                                    audio44.getWritePointer (c), dstLen);
                }
            }
            if (threadShouldExit()) return;

            postProg (0.08f, "Cargando modelo HTDemucs-6s...");
            auto model = std::make_unique<demucscpp::demucs_model>();
            if (! demucscpp::load_demucs_model (modelFile.getFullPathName().toStdString(), model.get()))
            {
                postDone (false, "Error cargando pesos HTDemucs-6s");
                return;
            }
            if (threadShouldExit()) return;

            const int n44 = audio44.getNumSamples();
            Eigen::MatrixXf mix (2, n44);
            const float* pL = audio44.getReadPointer (0);
            const float* pR = audio44.getReadPointer (1);
            for (int i = 0; i < n44; ++i)
            {
                mix (0, i) = pL[i];
                mix (1, i) = pR[i];
            }

            Eigen::Tensor3dXf stemsOut = demucscpp::demucs_inference (
                *model, mix,
                [this] (float prog, const std::string&)
                {
                    const float p = juce::jlimit (0.10f, 0.94f, 0.10f + prog * 0.84f);
                    postProg (p, "Separando 6 pistas AI: " + juce::String (juce::roundToInt (p * 100.0f)) + "%");
                });
            model.reset();
            if (threadShouldExit()) return;

            postProg (0.96f, "Guardando stems locales (.wav)...");
            const auto cacheDir = getSongCacheDir (item.id);
            juce::AudioBuffer<float> stemBuf (2, n44);

            auto writeSource = [&] (int fileStemIdx, int demucsSrcA, int demucsSrcB = -1) -> bool
            {
                float* oL = stemBuf.getWritePointer (0);
                float* oR = stemBuf.getWritePointer (1);
                for (int i = 0; i < n44; ++i)
                {
                    float l = stemsOut (demucsSrcA, 0, i);
                    float r = stemsOut (demucsSrcA, 1, i);
                    if (demucsSrcB >= 0)
                    {
                        l += stemsOut (demucsSrcB, 0, i);
                        r += stemsOut (demucsSrcB, 1, i);
                    }
                    oL[i] = l;
                    oR[i] = r;
                }
                return writeWav24Static (cacheDir.getChildFile (stemFileName (fileStemIdx)), stemBuf, demucsSr);
            };

            // HTDemucs-6s order: 0=drums, 1=bass, 2=other, 3=vocals, 4=guitar, 5=piano
            const bool ok = writeSource (0, 3)      // VOZ
                         && writeSource (1, 4)      // GUITARRA
                         && writeSource (2, 1)      // BAJO
                         && writeSource (3, 0)      // BATERIA
                         && writeSource (4, 2, 5);  // OTROS / FX (other + piano)

            postDone (ok, ok ? juce::String() : "Error guardando cache WAV");
           #else
            juce::ignoreUnused (targetSr, formats);
            postDone (false, "Motor Demucs AI disponible en build CMake");
           #endif
        }

        void postProg (float p, const juce::String& msg)
        {
            juce::MessageManager::callAsync ([cb = onProg, p, msg] { if (cb) cb (p, msg); });
        }
        void postDone (bool ok, const juce::String& err)
        {
            juce::MessageManager::callAsync ([cb = onDone, ok, id = item.id, err] { if (cb) cb (ok, id, err); });
        }

        Task task;
        PracticeSongItem item;
        double targetSr;
        juce::AudioFormatManager& formats;
        std::function<void(float, const juce::String&)> onProg;
        std::function<void(bool, const juce::String&, const juce::String&)> onDone;
    };

    void reloadCachedAiStemsHotSwap (const juce::String& songId)
    {
        if (activeSongIndex < 0 || activeSongIndex >= static_cast<int> (playlist.size())) return;
        const auto item = playlist[static_cast<size_t> (activeSongIndex)];
        if (item.id != songId) return;

        if (loaderThread != nullptr) { loaderThread->signalThreadShouldExit(); loaderThread->stopThread (2000); }
        loaderThread = std::make_unique<SongLoaderThread> (
            item, engineSampleRate, formatManager,
            [this] (std::shared_ptr<LoadedSongStems> loaded, bool)
            {
                if (loaded == nullptr) return;
                setActiveStems (loaded);
                if (onStateChanged) onStateChanged();
            });
        loaderThread->startThread (juce::Thread::Priority::normal);
    }

    void setActiveStems (std::shared_ptr<LoadedSongStems> next)
    {
        if (activeStemsOwner != nullptr)
            retiredStems.push_back (activeStemsOwner);
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

    juce::AudioFormatManager formatManager;
    double engineSampleRate = 44100.0;

    std::vector<PracticeSongItem> playlist;
    int activeSongIndex = -1;
    PlaylistEndMode endMode = PlaylistEndMode::Continuous;

    std::shared_ptr<LoadedSongStems> activeStemsOwner;
    std::vector<std::shared_ptr<LoadedSongStems>> retiredStems;
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

    std::unique_ptr<SongLoaderThread> loaderThread;
    std::unique_ptr<AiWorkerThread>   aiThread;
    std::atomic<bool>  aiBusy { false };
    std::atomic<float> aiProgress { 0.0f };
    juce::String aiStatusText;
};

} // namespace miamp_practice
