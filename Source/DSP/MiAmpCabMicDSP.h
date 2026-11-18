#pragma once
#include <JuceHeader.h>
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include "XVBassDSP.h"

namespace miamp
{

struct CabMicSnapshot
{
    bool  isBass   = false;
    int   cabIndex = 1;     // Guitar: 0..6 built-in, 7=Custom WAV, 8=OFF | Bass: 0..5 built-in, 6=Custom WAV, 7=OFF
    bool  cabBypassed = false;

    // Mic 1: MI-57 (Dynamic)
    bool  mic1On    = true;
    float mic1Level = 78.0f; // 0..100
    float mic1PosX  = 28.0f; // 0=center, 100=edge
    float mic1PosY  = 42.0f; // 0..100 vertical speaker pos
    int   mic1Dist  = 0;     // 0=Close, 1=Mid, 2=Far
    bool  mic1Phase = false;

    // Mic 2: MI-87 (Condenser)
    bool  mic2On    = true;
    float mic2Level = 52.0f; // 0..100
    float mic2PosX  = 58.0f; // 0=center, 100=edge
    float mic2PosY  = 58.0f; // 0..100
    int   mic2Dist  = 1;     // 0=Close, 1=Mid, 2=Far
    bool  mic2Phase = false;

    float micMix    = 50.0f; // 0=100% MI-57, 50=balanced blend, 100=100% MI-87
    float roomAmt   = 18.0f; // 0..100 stereo studio room ambience
};

class MiAmpCabMicDSP
{
public:
    static constexpr int kCustomGtrIdx = 8;
    static constexpr int kOffGtrIdx    = 9;
    static constexpr int kCustomBassIdx = 7;
    static constexpr int kOffBassIdx    = 8;

    void prepare (double sampleRate, int samplesPerBlock, int numChannels = 2)
    {
        sr = std::max (22050.0, sampleRate);
        maxBlock = std::max (64, samplesPerBlock);
        const int ch = std::clamp (numChannels, 1, 2);

        juce::dsp::ProcessSpec spec { sr, static_cast<juce::uint32> (maxBlock), static_cast<juce::uint32> (ch) };
        cabConv.prepare (spec);
        customConvGtr.prepare (spec);
        customConvBass.prepare (spec);

        for (int m = 0; m < 2; ++m)
        {
            micPreFilt1[m].reset();
            micPreFilt2[m].reset();
            micProxFilt[m].reset();
            micConeLpf[m].reset();
            micDelayLine[m].assign (static_cast<size_t> (std::max (256, static_cast<int> (sr * 0.025))), 0.0f);
            micDelayWrite[m] = 0;
        }

        // Stereo early reflection & room lines
        static constexpr float kRoomMsL[4] = { 4.3f, 9.7f, 16.4f, 24.8f };
        static constexpr float kRoomMsR[4] = { 5.8f, 11.9f, 19.1f, 28.3f };
        for (int t = 0; t < 4; ++t)
        {
            roomBufL[t].assign (static_cast<size_t> (std::max (64, static_cast<int> (sr * (kRoomMsL[t] * 0.001) + 16))), 0.0f);
            roomBufR[t].assign (static_cast<size_t> (std::max (64, static_cast<int> (sr * (kRoomMsR[t] * 0.001) + 16))), 0.0f);
            roomIdxL[t] = 0;
            roomIdxR[t] = 0;
        }
        roomDampL.setLowPass (sr, 3600.0f, 0.707f);
        roomDampR.setLowPass (sr, 3400.0f, 0.707f);
        roomHpL.setHighPass  (sr, 110.0f, 0.707f);
        roomHpR.setHighPass  (sr, 110.0f, 0.707f);

        workMono.assign (static_cast<size_t> (maxBlock), 0.0f);
        workStereo.setSize (2, maxBlock);

        lastBuiltInBass = false;
        lastBuiltInIdx = -1;
        loadBuiltInCab (false, 1);
    }

    void reset() noexcept
    {
        cabConv.reset();
        customConvGtr.reset();
        customConvBass.reset();
        for (int m = 0; m < 2; ++m)
        {
            micPreFilt1[m].reset();
            micPreFilt2[m].reset();
            micProxFilt[m].reset();
            micConeLpf[m].reset();
            std::fill (micDelayLine[m].begin(), micDelayLine[m].end(), 0.0f);
            micDelayWrite[m] = 0;
        }
        for (int t = 0; t < 4; ++t)
        {
            std::fill (roomBufL[t].begin(), roomBufL[t].end(), 0.0f);
            std::fill (roomBufR[t].begin(), roomBufR[t].end(), 0.0f);
            roomIdxL[t] = 0;
            roomIdxR[t] = 0;
        }
        roomDampL.reset();
        roomDampR.reset();
        roomHpL.reset();
        roomHpR.reset();
    }

    bool loadCustomIr (bool forBass, const juce::File& file) { return loadCustomIRFile (forBass, file); }

    bool loadCustomIRFile (bool forBass, const juce::File& file)
    {
        if (! file.existsAsFile())
            return false;
        auto& conv = forBass ? customConvBass : customConvGtr;
        conv.loadImpulseResponse (file,
                                  juce::dsp::Convolution::Stereo::no,
                                  juce::dsp::Convolution::Trim::yes,
                                  0);
        if (forBass) hasCustomBassIr = true;
        else         hasCustomGtrIr  = true;
        return true;
    }

    bool hasCustomIR (bool forBass) const noexcept { return forBass ? hasCustomBassIr : hasCustomGtrIr; }

    template <typename AudioBufferType>
    void processStereo (AudioBufferType& buffer, const CabMicSnapshot& snap)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        if (numSamples <= 0 || numCh <= 0)
            return;

        if (static_cast<int> (workMono.size()) < numSamples)
        {
            workMono.resize (static_cast<size_t> (numSamples));
            workStereo.setSize (2, numSamples, false, false, true);
        }

        // Collapse input to mono cab feed (if already stereo, average L+R)
        if (numCh == 1)
        {
            std::memcpy (workMono.data(), buffer.getReadPointer (0), static_cast<size_t> (numSamples) * sizeof (float));
        }
        else
        {
            const float* r0 = buffer.getReadPointer (0);
            const float* r1 = buffer.getReadPointer (1);
            for (int i = 0; i < numSamples; ++i)
                workMono[static_cast<size_t> (i)] = 0.5f * (r0[i] + r1[i]);
        }

        const int offIdx    = snap.isBass ? kOffBassIdx    : kOffGtrIdx;
        const int customIdx = snap.isBass ? kCustomBassIdx : kCustomGtrIdx;
        const bool isOff = snap.cabBypassed || snap.cabIndex == offIdx;
        const bool useCustom = (! isOff) && (snap.cabIndex == customIdx) && hasCustomIR (snap.isBass);

        // Copy mono feed to stereo work buffer for convolution
        workStereo.copyFrom (0, 0, workMono.data(), numSamples);
        if (workStereo.getNumChannels() > 1)
            workStereo.copyFrom (1, 0, workMono.data(), numSamples);

        if (! isOff)
        {
            if (useCustom)
            {
                juce::dsp::AudioBlock<float> blk (workStereo.getArrayOfWritePointers(),
                                                  static_cast<size_t> (std::min (2, numCh)),
                                                  static_cast<size_t> (numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx (blk);
                (snap.isBass ? customConvBass : customConvGtr).process (ctx);
            }
            else
            {
                const int builtIdx = (snap.cabIndex == customIdx) ? (snap.isBass ? 6 : 7) : std::clamp (snap.cabIndex, 0, snap.isBass ? 6 : 7);
                if (lastBuiltInBass != snap.isBass || lastBuiltInIdx != builtIdx)
                    loadBuiltInCab (snap.isBass, builtIdx);

                juce::dsp::AudioBlock<float> blk (workStereo.getArrayOfWritePointers(),
                                                  static_cast<size_t> (std::min (2, numCh)),
                                                  static_cast<size_t> (numSamples));
                juce::dsp::ProcessContextReplacing<float> ctx (blk);
                cabConv.process (ctx);
            }
        }

        // Update dual-microphone filters
        updateMicFilters (0, snap.isBass, snap.mic1PosX, snap.mic1PosY, snap.mic1Dist);
        updateMicFilters (1, snap.isBass, snap.mic2PosX, snap.mic2PosY, snap.mic2Dist);

        // Delay samples for Close / Mid / Far (time-of-arrival + phase realism)
        const float distMs[3] = { 0.0f, 0.42f, 1.35f };
        const int dSamp0 = std::clamp (static_cast<int> (std::round (distMs[std::clamp (snap.mic1Dist, 0, 2)] * 0.001 * sr)), 0, static_cast<int> (micDelayLine[0].size()) - 2);
        const int dSamp1 = std::clamp (static_cast<int> (std::round (distMs[std::clamp (snap.mic2Dist, 0, 2)] * 0.001 * sr)), 0, static_cast<int> (micDelayLine[1].size()) - 2);

        const float mixN = std::clamp (snap.micMix * 0.01f, 0.0f, 1.0f);
        const float bal1 = std::cos (mixN * 1.5707963f);
        const float bal2 = std::sin (mixN * 1.5707963f);

        const float g1 = (snap.mic1On ? (snap.mic1Level * 0.01f) : 0.0f) * bal1 * (snap.mic1Phase ? -1.0f : 1.0f) * 1.38f;
        const float g2 = (snap.mic2On ? (snap.mic2Level * 0.01f) : 0.0f) * bal2 * (snap.mic2Phase ? -1.0f : 1.0f) * 1.38f;
        const bool bothOff = (! snap.mic1On && ! snap.mic2On);

        const float roomSend = isOff ? 0.0f : (std::clamp (snap.roomAmt * 0.01f, 0.0f, 1.0f) * 0.42f);

        const float* cabL = workStereo.getReadPointer (0);
        float* outL = buffer.getWritePointer (0);
        float* outR = (numCh > 1) ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            const float c = cabL[i];
            float micSum = 0.0f;

            if (isOff || bothOff)
            {
                micSum = isOff ? (c * 0.85f) : (c * 0.55f);
            }
            else
            {
                // MI-57 (Dynamic)
                float m1 = micPreFilt1[0].processSample (c);
                m1 = micPreFilt2[0].processSample (m1);
                m1 = micProxFilt[0].processSample (m1);
                m1 = micConeLpf[0].processSample (m1);
                const int len0 = static_cast<int> (micDelayLine[0].size());
                micDelayLine[0][static_cast<size_t> (micDelayWrite[0])] = m1;
                const int rIdx0 = (micDelayWrite[0] - dSamp0 + len0) % len0;
                const float m1d = micDelayLine[0][static_cast<size_t> (rIdx0)];
                micDelayWrite[0] = (micDelayWrite[0] + 1) % len0;

                // MI-87 (Condenser)
                float m2 = micPreFilt1[1].processSample (c);
                m2 = micPreFilt2[1].processSample (m2);
                m2 = micProxFilt[1].processSample (m2);
                m2 = micConeLpf[1].processSample (m2);
                const int len1 = static_cast<int> (micDelayLine[1].size());
                micDelayLine[1][static_cast<size_t> (micDelayWrite[1])] = m2;
                const int rIdx1 = (micDelayWrite[1] - dSamp1 + len1) % len1;
                const float m2d = micDelayLine[1][static_cast<size_t> (rIdx1)];
                micDelayWrite[1] = (micDelayWrite[1] + 1) % len1;

                micSum = m1d * g1 + m2d * g2;
            }

            // Stereo room ambience (natural multi-tap early reflections)
            float rL = 0.0f, rR = 0.0f;
            if (roomSend > 0.001f)
            {
                static constexpr float tapG[4] = { 0.48f, -0.36f, 0.28f, -0.20f };
                const float feed = roomHpL.processSample (micSum);
                for (int t = 0; t < 4; ++t)
                {
                    const float dl = roomBufL[t][static_cast<size_t> (roomIdxL[t])];
                    roomBufL[t][static_cast<size_t> (roomIdxL[t])] = feed + dl * 0.22f;
                    roomIdxL[t] = (roomIdxL[t] + 1) % static_cast<int> (roomBufL[t].size());
                    rL += dl * tapG[t];

                    const float dr = roomBufR[t][static_cast<size_t> (roomIdxR[t])];
                    roomBufR[t][static_cast<size_t> (roomIdxR[t])] = feed + dr * 0.22f;
                    roomIdxR[t] = (roomIdxR[t] + 1) % static_cast<int> (roomBufR[t].size());
                    rR += dr * tapG[t];
                }
                rL = roomDampL.processSample (rL) * roomSend;
                rR = roomDampR.processSample (rR) * roomSend;
            }

            // Subtle distance-dependent air perspective when Mics are in Far mode (natural stereo width)
            const float stereoSpread = (snap.mic1On && snap.mic2On && snap.mic1Dist != snap.mic2Dist) ? 0.05f : 0.0f;
            const float finalL = micSum * (1.0f + stereoSpread) + rL;
            const float finalR = micSum * (1.0f - stereoSpread) + rR;

            outL[i] = finalL;
            if (outR != nullptr)
                outR[i] = finalR;
        }
    }

private:
    void updateMicFilters (int micIdx, bool isBass, float posX, float posY, int dist) noexcept
    {
        const float xN = std::clamp (posX * 0.01f, 0.0f, 1.0f); // 0=center, 1=edge
        const float yN = std::clamp (posY * 0.01f, 0.0f, 1.0f);
        const float yShift = (yN - 0.5f) * 140.0f;

        if (micIdx == 0)
        {
            // MI-57 Dynamic: mid-presence punch (2.8..3.8kHz), tight low-end rolloff
            const float presGain = (1.0f - xN) * 3.8f + 1.2f;
            micPreFilt1[0].setHighPass (sr, isBass ? 48.0f : 72.0f, 0.72f);
            micPreFilt2[0].setPeaking  (sr, 3150.0f + yShift, presGain, 1.35f);
            const float proxDb = (dist == 0) ? 2.6f : (dist == 1 ? 0.6f : -1.2f);
            micProxFilt[0].setPeaking  (sr, isBass ? 95.0f : 130.0f, proxDb, 0.85f);
            const float lpfHz = (isBass ? 4800.0f : 7400.0f) - xN * (isBass ? 1600.0f : 3400.0f) - dist * 450.0f;
            micConeLpf[0].setLowPass   (sr, std::max (2000.0f, lpfHz), 0.78f);
        }
        else
        {
            // MI-87 Large-diaphragm condenser: wide body + airy detail
            const float airGain = (1.0f - xN * 0.75f) * 2.4f;
            micPreFilt1[1].setHighPass (sr, isBass ? 32.0f : 52.0f, 0.65f);
            micPreFilt2[1].setHighShelf(sr, 5200.0f + yShift, airGain, 0.70f);
            const float proxDb = (dist == 0) ? 3.0f : (dist == 1 ? 0.8f : -0.8f);
            micProxFilt[1].setPeaking  (sr, isBass ? 75.0f : 110.0f, proxDb, 0.75f);
            const float lpfHz = (isBass ? 7800.0f : 10800.0f) - xN * (isBass ? 2400.0f : 4200.0f) - dist * 550.0f;
            micConeLpf[1].setLowPass   (sr, std::max (2800.0f, lpfHz), 0.70f);
        }
    }

    // Synthesize physically grounded multi-pole speaker cabinet IRs (512 taps @ 48kHz scaled to sr)
    void loadBuiltInCab (bool isBass, int idx)
    {
        lastBuiltInBass = isBass;
        lastBuiltInIdx  = idx;

        const double baseSr = 48000.0;
        const int nSamples = 512;
        juce::AudioBuffer<float> irBuf (1, nSamples);
        irBuf.clear();
        float* ir = irBuf.getWritePointer (0);
        ir[0] = 1.0f;

        auto applyBiq = [&] (xvdsp::Biquad& b)
        {
            for (int i = 0; i < nSamples; ++i)
                ir[i] = b.processSample (ir[i]);
        };

        xvdsp::Biquad hp, resLow, midScoop, conePeak, notch1, notch2, lp1, lp2;
        if (! isBass)
        {
            switch (idx)
            {
                case 0: // 4x12 GREEN (G12M Greenback: woody mids, smooth top)
                    hp.setHighPass (baseSr, 88.0f, 0.85f);
                    resLow.setPeaking (baseSr, 112.0f, 3.8f, 1.5f);
                    midScoop.setPeaking (baseSr, 440.0f, -2.6f, 1.1f);
                    conePeak.setPeaking (baseSr, 1650.0f, 3.4f, 1.4f);
                    notch1.setPeaking (baseSr, 2850.0f, -4.5f, 3.2f);
                    notch2.setPeaking (baseSr, 4100.0f, -3.0f, 2.5f);
                    lp1.setLowPass (baseSr, 4700.0f, 0.82f);
                    lp2.setLowPass (baseSr, 5600.0f, 0.70f);
                    break;
                case 1: // 4x12 BRIT V30 (Vintage 30: crisp rock/metal bark)
                    hp.setHighPass (baseSr, 92.0f, 0.92f);
                    resLow.setPeaking (baseSr, 118.0f, 3.2f, 1.6f);
                    midScoop.setPeaking (baseSr, 510.0f, -3.2f, 1.0f);
                    conePeak.setPeaking (baseSr, 2350.0f, 5.2f, 1.6f);
                    notch1.setPeaking (baseSr, 3600.0f, -5.0f, 3.8f);
                    notch2.setPeaking (baseSr, 4800.0f, -3.5f, 2.6f);
                    lp1.setLowPass (baseSr, 5200.0f, 0.88f);
                    lp2.setLowPass (baseSr, 6300.0f, 0.72f);
                    break;
                case 2: // 4x12 RECTO (Oversized 4x12: big sub thump + modern aggression)
                    hp.setHighPass (baseSr, 76.0f, 0.96f);
                    resLow.setPeaking (baseSr, 96.0f, 5.0f, 1.4f);
                    midScoop.setPeaking (baseSr, 470.0f, -4.8f, 0.95f);
                    conePeak.setPeaking (baseSr, 2650.0f, 4.6f, 1.5f);
                    notch1.setPeaking (baseSr, 3900.0f, -4.2f, 3.4f);
                    notch2.setPeaking (baseSr, 5200.0f, -3.8f, 2.5f);
                    lp1.setLowPass (baseSr, 5400.0f, 0.85f);
                    lp2.setLowPass (baseSr, 6600.0f, 0.72f);
                    break;
                case 3: // 4x12 EVH (Tight punch, focused midrange)
                    hp.setHighPass (baseSr, 95.0f, 0.90f);
                    resLow.setPeaking (baseSr, 122.0f, 3.4f, 1.6f);
                    midScoop.setPeaking (baseSr, 580.0f, -2.0f, 1.2f);
                    conePeak.setPeaking (baseSr, 1950.0f, 4.2f, 1.5f);
                    notch1.setPeaking (baseSr, 3300.0f, -4.8f, 3.6f);
                    notch2.setPeaking (baseSr, 4550.0f, -3.2f, 2.5f);
                    lp1.setLowPass (baseSr, 5000.0f, 0.84f);
                    lp2.setLowPass (baseSr, 6100.0f, 0.70f);
                    break;
                case 4: // 2x12 CLASS-A (Open-back Alnico chime)
                    hp.setHighPass (baseSr, 102.0f, 0.78f);
                    resLow.setPeaking (baseSr, 138.0f, 2.4f, 1.2f);
                    midScoop.setPeaking (baseSr, 380.0f, -2.8f, 1.1f);
                    conePeak.setPeaking (baseSr, 2900.0f, 4.8f, 1.3f);
                    notch1.setPeaking (baseSr, 4300.0f, -3.5f, 3.0f);
                    notch2.setPeaking (baseSr, 5800.0f, -3.0f, 2.2f);
                    lp1.setLowPass (baseSr, 6100.0f, 0.78f);
                    lp2.setLowPass (baseSr, 7400.0f, 0.68f);
                    break;
                case 5: // 4x12 TANGERINE (Thick birch low-mids + smooth high-end)
                    hp.setHighPass (baseSr, 82.0f, 0.92f);
                    resLow.setPeaking (baseSr, 108.0f, 4.4f, 1.3f);
                    midScoop.setPeaking (baseSr, 310.0f, 2.2f, 1.0f);
                    conePeak.setPeaking (baseSr, 1750.0f, 3.0f, 1.4f);
                    notch1.setPeaking (baseSr, 3100.0f, -5.0f, 3.2f);
                    notch2.setPeaking (baseSr, 4400.0f, -4.2f, 2.6f);
                    lp1.setLowPass (baseSr, 4600.0f, 0.85f);
                    lp2.setLowPass (baseSr, 5600.0f, 0.72f);
                    break;
                case 6: // 2x12 TWIN (Clean sparkling Jensen headroom)
                    hp.setHighPass (baseSr, 85.0f, 0.75f);
                    resLow.setPeaking (baseSr, 118.0f, 2.8f, 1.1f);
                    midScoop.setPeaking (baseSr, 460.0f, -3.4f, 0.9f);
                    conePeak.setPeaking (baseSr, 3200.0f, 3.8f, 1.2f);
                    notch1.setPeaking (baseSr, 4700.0f, -3.2f, 2.8f);
                    notch2.setPeaking (baseSr, 6200.0f, -2.8f, 2.2f);
                    lp1.setLowPass (baseSr, 6500.0f, 0.76f);
                    lp2.setLowPass (baseSr, 7900.0f, 0.68f);
                    break;
                default: // 7: 4x12 MI CLASSIC (Mint Tolex Original Stack: balanced punch + articulated bite)
                    hp.setHighPass (baseSr, 84.0f, 0.90f);
                    resLow.setPeaking (baseSr, 108.0f, 3.8f, 1.45f);
                    midScoop.setPeaking (baseSr, 460.0f, -2.4f, 1.05f);
                    conePeak.setPeaking (baseSr, 2250.0f, 4.5f, 1.50f);
                    notch1.setPeaking (baseSr, 3550.0f, -4.4f, 3.4f);
                    notch2.setPeaking (baseSr, 4850.0f, -3.2f, 2.5f);
                    lp1.setLowPass (baseSr, 5400.0f, 0.84f);
                    lp2.setLowPass (baseSr, 6500.0f, 0.70f);
                    break;
            }
        }
        else
        {
            switch (idx)
            {
                case 0: // 1x15 VINTAGE (Flip-top warm round 15")
                    hp.setHighPass (baseSr, 44.0f, 0.76f);
                    resLow.setPeaking (baseSr, 82.0f, 4.8f, 1.1f);
                    midScoop.setPeaking (baseSr, 620.0f, -2.0f, 1.0f);
                    conePeak.setPeaking (baseSr, 920.0f, 2.2f, 1.2f);
                    notch1.setPeaking (baseSr, 1950.0f, -4.2f, 2.4f);
                    notch2.setPeaking (baseSr, 2900.0f, -4.0f, 2.2f);
                    lp1.setLowPass (baseSr, 2850.0f, 0.78f);
                    lp2.setLowPass (baseSr, 3700.0f, 0.68f);
                    break;
                case 1: // 4x10 TOURING (Studio punch & clarity)
                    hp.setHighPass (baseSr, 42.0f, 0.80f);
                    resLow.setPeaking (baseSr, 80.0f, 4.0f, 1.2f);
                    midScoop.setPeaking (baseSr, 360.0f, -2.4f, 1.0f);
                    conePeak.setPeaking (baseSr, 2100.0f, 3.4f, 1.3f);
                    notch1.setPeaking (baseSr, 3500.0f, -2.8f, 2.6f);
                    notch2.setPeaking (baseSr, 5000.0f, -2.4f, 2.2f);
                    lp1.setLowPass (baseSr, 5600.0f, 0.74f);
                    lp2.setLowPass (baseSr, 7000.0f, 0.66f);
                    break;
                case 2: // 4x10 HI-PUNCH (Tight fast slap & tweeter clarity)
                    hp.setHighPass (baseSr, 46.0f, 0.82f);
                    resLow.setPeaking (baseSr, 74.0f, 3.8f, 1.3f);
                    midScoop.setPeaking (baseSr, 420.0f, -3.0f, 1.0f);
                    conePeak.setPeaking (baseSr, 2650.0f, 4.2f, 1.2f);
                    notch1.setPeaking (baseSr, 3900.0f, -2.5f, 2.5f);
                    notch2.setPeaking (baseSr, 5400.0f, -2.5f, 2.2f);
                    lp1.setLowPass (baseSr, 6200.0f, 0.74f);
                    lp2.setLowPass (baseSr, 7800.0f, 0.66f);
                    break;
                case 3: // 6x10 HEAVY (Deep modern metal sub + growl)
                    hp.setHighPass (baseSr, 36.0f, 0.90f);
                    resLow.setPeaking (baseSr, 68.0f, 5.2f, 1.3f);
                    midScoop.setPeaking (baseSr, 380.0f, -3.8f, 0.95f);
                    conePeak.setPeaking (baseSr, 1850.0f, 4.0f, 1.4f);
                    notch1.setPeaking (baseSr, 3100.0f, -3.2f, 2.8f);
                    notch2.setPeaking (baseSr, 4700.0f, -3.0f, 2.4f);
                    lp1.setLowPass (baseSr, 5400.0f, 0.78f);
                    lp2.setLowPass (baseSr, 6800.0f, 0.68f);
                    break;
                case 4: // 8x10 CLASSIC (Punchy sealed 10" array)
                    hp.setHighPass (baseSr, 42.0f, 0.85f);
                    resLow.setPeaking (baseSr, 92.0f, 4.4f, 1.2f);
                    midScoop.setPeaking (baseSr, 340.0f, -2.2f, 1.0f);
                    conePeak.setPeaking (baseSr, 1450.0f, 3.2f, 1.3f);
                    notch1.setPeaking (baseSr, 2600.0f, -3.5f, 2.8f);
                    notch2.setPeaking (baseSr, 3800.0f, -3.0f, 2.4f);
                    lp1.setLowPass (baseSr, 4300.0f, 0.78f);
                    lp2.setLowPass (baseSr, 5500.0f, 0.68f);
                    break;
                case 5: // 8x10 TANGERINE (Thick gritty low-mid 8x10)
                    hp.setHighPass (baseSr, 40.0f, 0.88f);
                    resLow.setPeaking (baseSr, 88.0f, 4.6f, 1.2f);
                    midScoop.setPeaking (baseSr, 460.0f, 2.4f, 1.1f);
                    conePeak.setPeaking (baseSr, 1300.0f, 2.8f, 1.3f);
                    notch1.setPeaking (baseSr, 2400.0f, -4.0f, 2.8f);
                    notch2.setPeaking (baseSr, 3600.0f, -3.6f, 2.4f);
                    lp1.setLowPass (baseSr, 3900.0f, 0.80f);
                    lp2.setLowPass (baseSr, 5000.0f, 0.70f);
                    break;
                default: // 6: 8x10 MI CLASSIC (Mint Tolex Original Fridge: deep sub + clear studio punch)
                    hp.setHighPass (baseSr, 40.0f, 0.86f);
                    resLow.setPeaking (baseSr, 86.0f, 4.8f, 1.2f);
                    midScoop.setPeaking (baseSr, 360.0f, -2.0f, 1.0f);
                    conePeak.setPeaking (baseSr, 1550.0f, 3.6f, 1.3f);
                    notch1.setPeaking (baseSr, 2750.0f, -3.3f, 2.7f);
                    notch2.setPeaking (baseSr, 4000.0f, -2.8f, 2.3f);
                    lp1.setLowPass (baseSr, 4500.0f, 0.78f);
                    lp2.setLowPass (baseSr, 5800.0f, 0.68f);
                    break;
            }
        }

        applyBiq (hp);
        applyBiq (resLow);
        applyBiq (midScoop);
        applyBiq (conePeak);
        applyBiq (notch1);
        applyBiq (notch2);
        applyBiq (lp1);
        applyBiq (lp2);

        // Add subtle organic cabinet box reflection tail for natural wooden body
        for (int i = nSamples - 1; i >= 12; --i)
            ir[i] += ir[i - 7] * 0.085f - ir[i - 13] * 0.048f;

        // Smooth fade-out on last 48 samples & RMS normalize
        for (int i = nSamples - 48; i < nSamples; ++i)
        {
            const float u = static_cast<float> (nSamples - 1 - i) / 48.0f;
            ir[i] *= u;
        }
        double e = 0.0;
        for (int i = 0; i < nSamples; ++i)
            e += static_cast<double> (ir[i]) * ir[i];
        const float norm = (e > 1e-8) ? static_cast<float> (0.95 / std::sqrt (e)) : 1.0f;
        for (int i = 0; i < nSamples; ++i)
            ir[i] *= norm;

        cabConv.loadImpulseResponse (std::move (irBuf),
                                     baseSr,
                                     juce::dsp::Convolution::Stereo::no,
                                     juce::dsp::Convolution::Trim::no,
                                     juce::dsp::Convolution::Normalise::no);
    }

    double sr = 48000.0;
    int maxBlock = 512;
    bool lastBuiltInBass = false;
    int  lastBuiltInIdx  = -1;
    bool hasCustomGtrIr  = false;
    bool hasCustomBassIr = false;

    juce::dsp::Convolution cabConv { juce::dsp::Convolution::NonUniform { 512 } };
    juce::dsp::Convolution customConvGtr { juce::dsp::Convolution::NonUniform { 512 } };
    juce::dsp::Convolution customConvBass { juce::dsp::Convolution::NonUniform { 512 } };

    xvdsp::Biquad micPreFilt1[2], micPreFilt2[2], micProxFilt[2], micConeLpf[2];
    std::vector<float> micDelayLine[2];
    int micDelayWrite[2] { 0, 0 };

    std::vector<float> roomBufL[4], roomBufR[4];
    int roomIdxL[4] { 0, 0, 0, 0 };
    int roomIdxR[4] { 0, 0, 0, 0 };
    xvdsp::Biquad roomDampL, roomDampR, roomHpL, roomHpR;

    std::vector<float> workMono;
    juce::AudioBuffer<float> workStereo;
};

} // namespace miamp
