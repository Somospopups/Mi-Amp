#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <atomic>
#include <cmath>
#include <algorithm>
#include "XVBassDSP.h"

namespace miamp
{

struct StudioFxSnapshot
{
    // 01: MI MULTI-COMP
    bool  compOn       = false;
    float compParallel = 85.0f;  // 0..100%
    float compXLowHz   = 180.0f; // 60..450 Hz
    float compXHighHz  = 2400.0f;// 800..6500 Hz
    struct BandCfg
    {
        bool  on       = true;
        bool  solo     = false;
        float threshDb = -18.0f; // -40..0 dB
        float ratio    = 3.0f;   // 1.2..12.0
        float attackMs = 12.0f;  // 0.5..80 ms
        float relMs    = 90.0f;  // 20..500 ms
        float gainDb   = 0.0f;   // -12..+12 dB
    };
    std::array<BandCfg, 3> compBands {{
        { true, false, -18.0f, 4.0f, 12.0f,  90.0f,  1.2f },
        { true, false, -14.0f, 2.5f, 20.0f, 120.0f,  0.5f },
        { true, false, -20.0f, 3.0f,  5.0f,  70.0f,  0.0f }
    }};

    // 02: MI STUDIO EQ
    bool  eqOn       = false;
    bool  eqAnalyzer = true;
    float eqHpfHz    = 80.0f;    // 20..400 Hz
    float eqLpfHz    = 12000.0f; // 2500..20000 Hz
    float eqOutDb    = 0.0f;     // -12..+12 dB
    struct EqBandCfg
    {
        float freqHz = 1000.0f;
        float gainDb = 0.0f;
        float q      = 1.2f;
    };
    std::array<EqBandCfg, 5> eqBands {{
        {   55.0f,  0.0f, 0.9f },
        {  110.0f,  1.0f, 1.1f },
        {  360.0f, -2.0f, 1.6f },
        { 1800.0f,  1.5f, 1.3f },
        { 6200.0f,  0.5f, 0.9f }
    }};

    // 03: MI DIGITAL DELAY
    bool  dlyOn       = false;
    bool  dlyPingPong = true;
    bool  dlySync     = false;
    float dlyTimeMs   = 375.0f; // 20..1500 ms
    int   dlySubdiv   = 2;      // 0:1/4, 1:1/8, 2:1/8D, 3:1/16, 4:1/4T, 5:1/8T
    float dlyFeedback = 34.0f;  // 0..92%
    float dlyLowCutHz = 180.0f; // 20..1000 Hz
    float dlyHiCutHz  = 4800.0f;// 1000..16000 Hz
    float dlyMix      = 24.0f;  // 0..100%

    // 04: MI DIGITAL REVERB
    bool  revOn       = false;
    int   revMode     = 0;      // 0=Plate, 1=Hall, 2=Room, 3=Ambient
    float revPreDlyMs = 24.0f;  // 0..120 ms
    float revDecaySec = 2.1f;   // 0.3..8.0 s
    float revDampHz   = 5200.0f;// 1000..16000 Hz
    float revWidthPct = 100.0f; // 0..100%
    float revMixPct   = 24.0f;  // 0..100%
};

class MiAmpStudioFxDSP
{
public:
    static constexpr int kFftOrder = 10; // 1024-point FFT
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kSpectrumBins = 48;

    void prepare (double sampleRate, int /*samplesPerBlock*/, int /*numChannels*/ = 2)
    {
        sr = std::max (22050.0, sampleRate);

        // Multi-comp reset
        for (int c = 0; c < 2; ++c)
        {
            for (int f = 0; f < 2; ++f)
            {
                lrLow1[c][f].reset();  lrHigh1[c][f].reset();
                lrLow2[c][f].reset();  lrHigh2[c][f].reset();
            }
            eqHpf[c].reset();
            eqLpf[c].reset();
            for (int b = 0; b < 5; ++b)
                eqPeaks[c][b].reset();
            dlyFbLp[c].reset();
            dlyFbHp[c].reset();
        }
        for (int b = 0; b < 3; ++b)
        {
            compEnv[b] = 0.0f;
            grDbAtomics[b].store (0.0f, std::memory_order_relaxed);
        }

        // Delay buffers (up to 2.0 sec)
        const int maxDly = static_cast<int> (sr * 2.0) + 16;
        dlyBuf[0].assign (static_cast<size_t> (maxDly), 0.0f);
        dlyBuf[1].assign (static_cast<size_t> (maxDly), 0.0f);
        dlyWrite = 0;

        // Reverb buffers (Pre-delay + 6 parallel Schroeder/Moorer combs + 3 series allpass per channel)
        const int maxPre = static_cast<int> (sr * 0.14) + 16;
        revPreBuf[0].assign (static_cast<size_t> (maxPre), 0.0f);
        revPreBuf[1].assign (static_cast<size_t> (maxPre), 0.0f);
        revPreWrite = 0;

        static constexpr int kCombBase[6] = { 1116, 1188, 1277, 1356, 1422, 1491 };
        static constexpr int kApBase[3]   = { 556, 441, 341 };
        const double sc = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            const int spread = c * 23;
            for (int k = 0; k < 6; ++k)
            {
                const int len = std::max (64, static_cast<int> (std::round ((kCombBase[k] + spread) * sc)));
                revCombBuf[c][k].assign (static_cast<size_t> (len), 0.0f);
                revCombIdx[c][k] = 0;
                revCombLp[c][k] = 0.0f;
            }
            for (int k = 0; k < 3; ++k)
            {
                const int len = std::max (32, static_cast<int> (std::round ((kApBase[k] + spread) * sc)));
                revApBuf[c][k].assign (static_cast<size_t> (len), 0.0f);
                revApIdx[c][k] = 0;
            }
        }

        // FFT analyzer
        fftFifo.fill (0.0f);
        fftData.fill (0.0f);
        fftFifoIdx = 0;
        for (auto& m : spectrumBins)
            m.store (0.0f, std::memory_order_relaxed);
    }

    void reset() noexcept
    {
        for (int b = 0; b < 3; ++b)
        {
            compEnv[b] = 0.0f;
            grDbAtomics[b].store (0.0f, std::memory_order_relaxed);
        }
        std::fill (dlyBuf[0].begin(), dlyBuf[0].end(), 0.0f);
        std::fill (dlyBuf[1].begin(), dlyBuf[1].end(), 0.0f);
        dlyWrite = 0;
        std::fill (revPreBuf[0].begin(), revPreBuf[0].end(), 0.0f);
        std::fill (revPreBuf[1].begin(), revPreBuf[1].end(), 0.0f);
        revPreWrite = 0;
        for (int c = 0; c < 2; ++c)
        {
            for (int k = 0; k < 6; ++k)
            {
                std::fill (revCombBuf[c][k].begin(), revCombBuf[c][k].end(), 0.0f);
                revCombLp[c][k] = 0.0f;
            }
            for (int k = 0; k < 3; ++k)
                std::fill (revApBuf[c][k].begin(), revApBuf[c][k].end(), 0.0f);
        }
    }

    float getBandGrDb (int band) const noexcept
    {
        if (band < 0 || band >= 3) return 0.0f;
        return grDbAtomics[static_cast<size_t> (band)].load (std::memory_order_relaxed);
    }

    float getSpectrumBin (int bin) const noexcept
    {
        if (bin < 0 || bin >= kSpectrumBins) return 0.0f;
        return spectrumBins[static_cast<size_t> (bin)].load (std::memory_order_relaxed);
    }

    template <typename AudioBufferType>
    void processStereo (AudioBufferType& buffer, const StudioFxSnapshot& snap, double dawBpm = 120.0) noexcept
    {
        const int numCh = std::min (2, buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        if (numCh <= 0 || numSamples <= 0)
            return;

        float* chL = buffer.getWritePointer (0);
        float* chR = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

        // 01: MI MULTI-COMP
        if (snap.compOn)
            processMultiComp (chL, chR, numSamples, snap);
        else
        {
            for (int b = 0; b < 3; ++b)
                grDbAtomics[b].store (0.0f, std::memory_order_relaxed);
        }

        // 02: MI STUDIO EQ + FFT Analyzer feed
        if (snap.eqOn)
            processStudioEq (chL, chR, numSamples, snap);

        if (snap.eqAnalyzer)
            pushFftSamples (chL, chR, numSamples);

        // 03: MI DIGITAL DELAY
        if (snap.dlyOn)
            processDelay (chL, chR, numSamples, snap, dawBpm);

        // 04: MI DIGITAL REVERB
        if (snap.revOn)
            processReverb (chL, chR, numSamples, snap);
    }

private:
    void processMultiComp (float* chL, float* chR, int numSamples, const StudioFxSnapshot& snap) noexcept
    {
        const float xLo = std::clamp (snap.compXLowHz, 60.0f, 500.0f);
        const float xHi = std::clamp (snap.compXHighHz, xLo + 250.0f, 8000.0f);
        for (int c = 0; c < 2; ++c)
        {
            for (int f = 0; f < 2; ++f)
            {
                lrLow1[c][f].setLowPass   (sr, xLo, 0.7071f);
                lrHigh1[c][f].setHighPass (sr, xLo, 0.7071f);
                lrLow2[c][f].setLowPass   (sr, xHi, 0.7071f);
                lrHigh2[c][f].setHighPass (sr, xHi, 0.7071f);
            }
        }

        const bool anySolo = snap.compBands[0].solo || snap.compBands[1].solo || snap.compBands[2].solo;
        float attCoef[3], relCoef[3], threshLin[3], ratio[3], makeGain[3];
        bool  bandActive[3], bandPass[3];

        for (int b = 0; b < 3; ++b)
        {
            const auto& cfg = snap.compBands[static_cast<size_t> (b)];
            attCoef[b] = std::exp (-1.0f / static_cast<float> (std::max (0.0004, cfg.attackMs * 0.001) * sr));
            relCoef[b] = std::exp (-1.0f / static_cast<float> (std::max (0.015,  cfg.relMs * 0.001) * sr));
            threshLin[b] = std::pow (10.0f, cfg.threshDb / 20.0f);
            ratio[b]     = std::clamp (cfg.ratio, 1.0f, 20.0f);
            makeGain[b]  = std::pow (10.0f, cfg.gainDb / 20.0f);
            bandActive[b] = cfg.on;
            bandPass[b]   = (! anySolo) || cfg.solo;
        }

        const float parMix = std::clamp (snap.compParallel * 0.01f, 0.0f, 1.0f);
        float maxGrDb[3] { 0.0f, 0.0f, 0.0f };

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = chL[i];
            const float inR = (chR != nullptr) ? chR[i] : inL;

            // Linkwitz-Riley 4th-order 3-band split
            const float l1_L = lrLow1[0][1].processSample (lrLow1[0][0].processSample (inL));
            const float mh_L = lrHigh1[0][1].processSample (lrHigh1[0][0].processSample (inL));
            const float m_L  = lrLow2[0][1].processSample (lrLow2[0][0].processSample (mh_L));
            const float h_L  = lrHigh2[0][1].processSample (lrHigh2[0][0].processSample (mh_L));

            const float l1_R = (chR != nullptr) ? lrLow1[1][1].processSample (lrLow1[1][0].processSample (inR)) : l1_L;
            const float mh_R = (chR != nullptr) ? lrHigh1[1][1].processSample (lrHigh1[1][0].processSample (inR)) : mh_L;
            const float m_R  = (chR != nullptr) ? lrLow2[1][1].processSample (lrLow2[1][0].processSample (mh_R)) : m_L;
            const float h_R  = (chR != nullptr) ? lrHigh2[1][1].processSample (lrHigh2[1][0].processSample (mh_R)) : h_L;

            float bL[3] { l1_L, m_L, h_L };
            float bR[3] { l1_R, m_R, h_R };
            float sumL = 0.0f, sumR = 0.0f;

            for (int b = 0; b < 3; ++b)
            {
                if (! bandPass[b]) continue;

                const float peak = std::max (std::fabs (bL[b]), std::fabs (bR[b]));
                compEnv[b] = (peak > compEnv[b])
                    ? (attCoef[b] * compEnv[b] + (1.0f - attCoef[b]) * peak)
                    : (relCoef[b] * compEnv[b] + (1.0f - relCoef[b]) * peak);

                float gr = 1.0f;
                if (bandActive[b] && compEnv[b] > threshLin[b] && threshLin[b] > 1e-6f)
                {
                    const float overDb = 20.0f * std::log10 (compEnv[b] / threshLin[b]);
                    const float redDb  = overDb * (1.0f - 1.0f / ratio[b]);
                    gr = std::pow (10.0f, -redDb / 20.0f);
                    maxGrDb[b] = std::max (maxGrDb[b], redDb);
                }

                const float g = gr * (bandActive[b] ? makeGain[b] : 1.0f);
                sumL += bL[b] * g;
                sumR += bR[b] * g;
            }

            chL[i] = inL * (1.0f - parMix) + sumL * parMix;
            if (chR != nullptr)
                chR[i] = inR * (1.0f - parMix) + sumR * parMix;
        }

        for (int b = 0; b < 3; ++b)
        {
            const float prev = grDbAtomics[b].load (std::memory_order_relaxed);
            grDbAtomics[b].store (prev * 0.72f + maxGrDb[b] * 0.28f, std::memory_order_relaxed);
        }
    }

    void processStudioEq (float* chL, float* chR, int numSamples, const StudioFxSnapshot& snap) noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            eqHpf[c].setHighPass (sr, std::clamp (snap.eqHpfHz, 20.0f, 500.0f), 0.707f);
            eqLpf[c].setLowPass  (sr, std::clamp (snap.eqLpfHz, 1500.0f, static_cast<float> (sr * 0.46)), 0.707f);
            for (int b = 0; b < 5; ++b)
            {
                const auto& cfg = snap.eqBands[static_cast<size_t> (b)];
                eqPeaks[c][b].setPeaking (sr,
                                          std::clamp (cfg.freqHz, 25.0f, static_cast<float> (sr * 0.45)),
                                          std::clamp (cfg.gainDb, -15.0f, 15.0f),
                                          std::clamp (cfg.q, 0.25f, 10.0f));
            }
        }
        const float outTrim = std::pow (10.0f, std::clamp (snap.eqOutDb, -12.0f, 12.0f) / 20.0f);
        const bool useHpf = snap.eqHpfHz > 22.0f;
        const bool useLpf = snap.eqLpfHz < 19500.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float sL = chL[i];
            if (useHpf) sL = eqHpf[0].processSample (sL);
            if (useLpf) sL = eqLpf[0].processSample (sL);
            for (int b = 0; b < 5; ++b)
                if (std::fabs (snap.eqBands[static_cast<size_t> (b)].gainDb) > 0.05f)
                    sL = eqPeaks[0][b].processSample (sL);
            chL[i] = sL * outTrim;

            if (chR != nullptr)
            {
                float sR = chR[i];
                if (useHpf) sR = eqHpf[1].processSample (sR);
                if (useLpf) sR = eqLpf[1].processSample (sR);
                for (int b = 0; b < 5; ++b)
                    if (std::fabs (snap.eqBands[static_cast<size_t> (b)].gainDb) > 0.05f)
                        sR = eqPeaks[1][b].processSample (sR);
                chR[i] = sR * outTrim;
            }
        }
    }

    void processDelay (float* chL, float* chR, int numSamples, const StudioFxSnapshot& snap, double bpm) noexcept
    {
        float timeMs = std::clamp (snap.dlyTimeMs, 15.0f, 1600.0f);
        if (snap.dlySync)
        {
            const double safeBpm = std::clamp (bpm, 40.0, 240.0);
            const double qMs = 60000.0 / safeBpm;
            static constexpr double mults[6] = { 1.0, 0.5, 0.75, 0.25, 0.666667, 0.333333 };
            timeMs = std::clamp (static_cast<float> (qMs * mults[std::clamp (snap.dlySubdiv, 0, 5)]), 15.0f, 1600.0f);
        }

        const float fb = std::clamp (snap.dlyFeedback * 0.01f, 0.0f, 0.92f);
        const float mix = std::clamp (snap.dlyMix * 0.01f, 0.0f, 1.0f);
        for (int c = 0; c < 2; ++c)
        {
            dlyFbHp[c].setHighPass (sr, std::clamp (snap.dlyLowCutHz, 20.0f, 1200.0f), 0.707f);
            dlyFbLp[c].setLowPass  (sr, std::clamp (snap.dlyHiCutHz, 900.0f, 18000.0f), 0.707f);
        }

        const int bufLen = static_cast<int> (dlyBuf[0].size());
        const float dSampL = std::clamp (static_cast<float> (timeMs * 0.001 * sr), 1.0f, static_cast<float> (bufLen - 4));
        const float dSampR = snap.dlyPingPong ? dSampL : std::clamp (dSampL * 1.014f, 1.0f, static_cast<float> (bufLen - 4));

        auto readInterp = [&] (int ch, float dSamp) -> float
        {
            float r = static_cast<float> (dlyWrite) - dSamp;
            while (r < 0.0f) r += static_cast<float> (bufLen);
            const int i0 = static_cast<int> (r) % bufLen;
            const int i1 = (i0 + 1) % bufLen;
            const float f = r - std::floor (r);
            return dlyBuf[ch][static_cast<size_t> (i0)] * (1.0f - f) + dlyBuf[ch][static_cast<size_t> (i1)] * f;
        };

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = chL[i];
            const float inR = (chR != nullptr) ? chR[i] : inL;
            const float wL = readInterp (0, dSampL);
            const float wR = readInterp (1, dSampR);

            const float fL = dlyFbLp[0].processSample (dlyFbHp[0].processSample (wL));
            const float fR = dlyFbLp[1].processSample (dlyFbHp[1].processSample (wR));

            if (snap.dlyPingPong)
            {
                const float mono = 0.5f * (inL + inR);
                dlyBuf[0][static_cast<size_t> (dlyWrite)] = mono + fR * fb;
                dlyBuf[1][static_cast<size_t> (dlyWrite)] = fL * fb;
            }
            else
            {
                dlyBuf[0][static_cast<size_t> (dlyWrite)] = inL + fL * fb;
                dlyBuf[1][static_cast<size_t> (dlyWrite)] = inR + fR * fb;
            }
            dlyWrite = (dlyWrite + 1) % bufLen;

            chL[i] = inL * (1.0f - mix * 0.5f) + wL * mix;
            if (chR != nullptr)
                chR[i] = inR * (1.0f - mix * 0.5f) + wR * mix;
        }
    }

    void processReverb (float* chL, float* chR, int numSamples, const StudioFxSnapshot& snap) noexcept
    {
        const int preLen = static_cast<int> (revPreBuf[0].size());
        const int preSamps = std::clamp (static_cast<int> (std::round (snap.revPreDlyMs * 0.001 * sr)), 0, preLen - 2);
        const float decay = std::clamp (snap.revDecaySec, 0.25f, 8.5f);
        const float modeScale = (snap.revMode == 1) ? 0.92f : (snap.revMode == 2 ? 0.78f : (snap.revMode == 3 ? 0.96f : 0.87f));
        const float combFb = std::clamp (0.64f + (decay / 8.5f) * 0.30f * modeScale, 0.62f, 0.945f);
        const float dampHz = std::clamp (snap.revDampHz, 800.0f, 16000.0f);
        const float dampCoef = std::clamp (std::exp (-6.283185f * dampHz / static_cast<float> (sr)), 0.05f, 0.88f);
        const float width = std::clamp (snap.revWidthPct * 0.01f, 0.0f, 1.0f);
        const float mix = std::clamp (snap.revMixPct * 0.01f, 0.0f, 1.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            const float inL = chL[i];
            const float inR = (chR != nullptr) ? chR[i] : inL;

            revPreBuf[0][static_cast<size_t> (revPreWrite)] = inL;
            revPreBuf[1][static_cast<size_t> (revPreWrite)] = inR;
            const int rPre = (revPreWrite - preSamps + preLen) % preLen;
            const float pL = revPreBuf[0][static_cast<size_t> (rPre)];
            const float pR = revPreBuf[1][static_cast<size_t> (rPre)];
            revPreWrite = (revPreWrite + 1) % preLen;

            const float feedL = (pL * 0.75f + pR * 0.25f) * 0.28f;
            const float feedR = (pR * 0.75f + pL * 0.25f) * 0.28f;

            float wetL = 0.0f, wetR = 0.0f;
            for (int k = 0; k < 6; ++k)
            {
                const int lenL = static_cast<int> (revCombBuf[0][k].size());
                const float outL = revCombBuf[0][k][static_cast<size_t> (revCombIdx[0][k])];
                revCombLp[0][k] = outL * (1.0f - dampCoef) + revCombLp[0][k] * dampCoef;
                revCombBuf[0][k][static_cast<size_t> (revCombIdx[0][k])] = feedL + revCombLp[0][k] * combFb;
                revCombIdx[0][k] = (revCombIdx[0][k] + 1) % lenL;
                wetL += outL;

                const int lenR = static_cast<int> (revCombBuf[1][k].size());
                const float outR = revCombBuf[1][k][static_cast<size_t> (revCombIdx[1][k])];
                revCombLp[1][k] = outR * (1.0f - dampCoef) + revCombLp[1][k] * dampCoef;
                revCombBuf[1][k][static_cast<size_t> (revCombIdx[1][k])] = feedR + revCombLp[1][k] * combFb;
                revCombIdx[1][k] = (revCombIdx[1][k] + 1) % lenR;
                wetR += outR;
            }

            for (int k = 0; k < 3; ++k)
            {
                const int lenL = static_cast<int> (revApBuf[0][k].size());
                const float bufOutL = revApBuf[0][k][static_cast<size_t> (revApIdx[0][k])];
                const float apInL = wetL + bufOutL * 0.5f;
                revApBuf[0][k][static_cast<size_t> (revApIdx[0][k])] = apInL;
                wetL = bufOutL - wetL * 0.5f;
                revApIdx[0][k] = (revApIdx[0][k] + 1) % lenL;

                const int lenR = static_cast<int> (revApBuf[1][k].size());
                const float bufOutR = revApBuf[1][k][static_cast<size_t> (revApIdx[1][k])];
                const float apInR = wetR + bufOutR * 0.5f;
                revApBuf[1][k][static_cast<size_t> (revApIdx[1][k])] = apInR;
                wetR = bufOutR - wetR * 0.5f;
                revApIdx[1][k] = (revApIdx[1][k] + 1) % lenR;
            }

            // Stereo M/S width control
            const float mid  = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR) * width;
            wetL = (mid + side) * 0.45f;
            wetR = (mid - side) * 0.45f;

            chL[i] = inL * (1.0f - mix * 0.45f) + wetL * mix;
            if (chR != nullptr)
                chR[i] = inR * (1.0f - mix * 0.45f) + wetR * mix;
        }
    }

    void pushFftSamples (const float* chL, const float* chR, int numSamples) noexcept
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = (chR != nullptr) ? 0.5f * (chL[i] + chR[i]) : chL[i];
            fftFifo[static_cast<size_t> (fftFifoIdx++)] = m;
            if (fftFifoIdx >= kFftSize)
            {
                fftFifoIdx = 0;
                fftData.fill (0.0f);
                std::memcpy (fftData.data(), fftFifo.data(), static_cast<size_t> (kFftSize) * sizeof (float));
                hannWindow.multiplyWithWindowingTable (fftData.data(), kFftSize);
                forwardFft.performFrequencyOnlyForwardTransform (fftData.data());

                // Map 512 FFT bins logarithmically (30 Hz..16 kHz) into kSpectrumBins
                for (int b = 0; b < kSpectrumBins; ++b)
                {
                    const float t0 = static_cast<float> (b) / static_cast<float> (kSpectrumBins);
                    const float t1 = static_cast<float> (b + 1) / static_cast<float> (kSpectrumBins);
                    const float f0 = 30.0f * std::pow (16000.0f / 30.0f, t0);
                    const float f1 = 30.0f * std::pow (16000.0f / 30.0f, t1);
                    const int bin0 = std::clamp (static_cast<int> (f0 * kFftSize / sr), 1, kFftSize / 2 - 1);
                    const int bin1 = std::clamp (static_cast<int> (f1 * kFftSize / sr), bin0, kFftSize / 2 - 1);
                    float peak = 0.0f;
                    for (int k = bin0; k <= bin1; ++k)
                        peak = std::max (peak, fftData[static_cast<size_t> (k)]);
                    const float norm = peak / static_cast<float> (kFftSize * 0.25f);
                    const float db = 20.0f * std::log10 (std::max (1e-5f, norm));
                    const float lvl = std::clamp ((db + 66.0f) / 66.0f, 0.0f, 1.0f);
                    const float old = spectrumBins[static_cast<size_t> (b)].load (std::memory_order_relaxed);
                    spectrumBins[static_cast<size_t> (b)].store (old * 0.62f + lvl * 0.38f, std::memory_order_relaxed);
                }
            }
        }
    }

    double sr = 48000.0;

    // Multi-comp state
    xvdsp::Biquad lrLow1[2][2], lrHigh1[2][2], lrLow2[2][2], lrHigh2[2][2];
    float compEnv[3] { 0.0f, 0.0f, 0.0f };
    std::array<std::atomic<float>, 3> grDbAtomics;

    // Studio EQ state
    xvdsp::Biquad eqHpf[2], eqLpf[2];
    xvdsp::Biquad eqPeaks[2][5];

    // Delay state
    std::vector<float> dlyBuf[2];
    int dlyWrite = 0;
    xvdsp::Biquad dlyFbHp[2], dlyFbLp[2];

    // Reverb state
    std::vector<float> revPreBuf[2];
    int revPreWrite = 0;
    std::vector<float> revCombBuf[2][6];
    int revCombIdx[2][6] {};
    float revCombLp[2][6] {};
    std::vector<float> revApBuf[2][3];
    int revApIdx[2][3] {};

    // FFT Analyzer
    juce::dsp::FFT forwardFft { kFftOrder };
    juce::dsp::WindowingFunction<float> hannWindow { kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::array<float, kFftSize> fftFifo {};
    std::array<float, kFftSize * 2> fftData {};
    int fftFifoIdx = 0;
    std::array<std::atomic<float>, kSpectrumBins> spectrumBins {};
};

} // namespace miamp
