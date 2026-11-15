#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "XVBassDSP.h"
#include "XVCabIRs.h"
#include "XVPitchShifter.h"

namespace miamp
{

enum class PedalType : int
{
    Compresor = 0,
    XV        = 1,
    Chorus    = 2,
    Phaser    = 3,
    Reverb    = 4,
    Count     = 5
};

struct BassControlSnapshot
{
    // Pitch / Drop-Tuner (pre-FX)
    bool  pitchEnabled   = false;
    int   pitchSemitones = 0;
    float pitchCents     = 0.0f;
    float pitchMix       = 100.0f;

    // Bass Amp Head
    bool  ampPower   = true;
    bool  ampStandby = false;
    bool  ampUltraHi = false; // false = ULTRA-LO, true = ULTRA-HI
    bool  ampCabOn   = true;
    float ampGain    = 50.0f;
    float ampBass    = 50.0f;
    float ampMid     = 50.0f;
    float ampFreq    = 50.0f;
    float ampTreble  = 50.0f;

    // Tuner Mute
    bool  tunerMuted = false;

    // Pedalboard order: up to 5 slots (-1 = empty, 0..4 = PedalType)
    std::array<int, 5> chainSlots { 1, -1, -1, -1, -1 };

    // Compresor
    bool  compOn      = true;
    float compSustain = 50.0f;
    float compAttack  = 50.0f;
    float compLevel   = 50.0f;

    // XV Bass (0..100 UI scale)
    bool  xvOn       = true;
    float xvLevel    = 56.0f;
    float xvDrive    = 40.8f;
    float xvLow      = 65.4f;
    float xvHigh     = 53.5f;
    float xvBlend    = 57.6f;
    float xvMid      = 25.8f;
    float xvPresence = 80.4f;
    int   xvCab      = 1; // 0=BASS, 1=PUNK, 2=CLEAN

    // Chorus
    bool  chorusOn    = true;
    float chorusRate  = 50.0f;
    float chorusDepth = 50.0f;
    float chorusMix   = 50.0f;

    // Phaser
    bool  phaserOn       = true;
    float phaserRate     = 50.0f;
    float phaserDepth    = 50.0f;
    float phaserFeedback = 50.0f;

    // Reverb
    bool  reverbOn    = true;
    float reverbDecay = 50.0f;
    float reverbTone  = 50.0f;
    float reverbMix   = 50.0f;
};

class BassCompressorFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        relCoeff = std::exp (-1.0f / static_cast<float> (0.16 * sr));
        env = 0.0f;
        bypassSmooth = 0.0f;
    }
    void reset() noexcept { env = 0.0f; }

    inline float process (float x, bool enabled, float sustain, float attack, float level) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        if (std::fabs (sustain - lastSustain) > 0.25f || std::fabs (attack - lastAttack) > 0.25f)
        {
            lastSustain = sustain;
            lastAttack  = attack;
            const float sus = std::clamp (sustain * 0.01f, 0.0f, 1.0f);
            const float att = std::clamp (attack  * 0.01f, 0.0f, 1.0f);
            const float attTime = 0.002f + att * 0.075f;
            attCoeffCached = std::exp (-1.0f / static_cast<float> (attTime * sr));
            threshCached   = std::pow (10.0f, (-8.0f - sus * 36.0f) / 20.0f);
            ratioCached    = 2.0f + sus * 10.0f;
            susBoostCached = 1.0f + sus * 0.45f;
        }

        const float lvl = std::clamp (level * 0.01f, 0.0f, 1.0f);
        const float ax  = std::fabs (x);
        env = (ax > env) ? (attCoeffCached * env + (1.0f - attCoeffCached) * ax)
                         : (relCoeff * env + (1.0f - relCoeff) * ax);

        float gain = 1.0f;
        if (env > threshCached)
        {
            const float overDb = 20.0f * std::log10 (env / std::max (1e-7f, threshCached));
            const float grDb   = overDb * (1.0f - 1.0f / ratioCached);
            gain = std::pow (10.0f, -grDb / 20.0f);
        }

        const float outGain = (0.65f + lvl * 1.4f) * susBoostCached;
        const float wet = x * gain * outGain;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    float relCoeff = 0.9998f;
    float env = 0.0f;
    float bypassSmooth = 0.0f;
    float lastSustain = -1.0f, lastAttack = -1.0f;
    float attCoeffCached = 0.99f, threshCached = 0.1f, ratioCached = 4.0f, susBoostCached = 1.2f;
};

class BassChorusFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        buf.assign (4096, 0.0f);
        mask = 4095;
        wPos = 0;
        phase = 0.0f;
        bypassSmooth = 0.0f;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); wPos = 0; phase = 0.0f; }

    inline float process (float x, bool enabled, float rate, float depth, float mix) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        buf[static_cast<size_t> (wPos & mask)] = x;
        const float r = std::clamp (rate  * 0.01f, 0.0f, 1.0f);
        const float d = std::clamp (depth * 0.01f, 0.0f, 1.0f);
        const float m = std::clamp (mix   * 0.01f, 0.0f, 1.0f);

        const float lfoHz = 0.18f + r * 4.3f;
        phase += static_cast<float> (lfoHz / sr);
        if (phase >= 1.0f) phase -= 1.0f;

        const float mod = 0.5f + 0.5f * std::sin (2.0f * 3.14159265f * phase);
        const float delaySec = 0.012f + mod * (0.0005f + d * 0.0055f);
        const float delaySmp = delaySec * static_cast<float> (sr);

        const float rp = static_cast<float> (wPos) - delaySmp;
        const int i0 = static_cast<int> (std::floor (rp));
        const float frac = rp - static_cast<float> (i0);
        const float s0 = buf[static_cast<size_t> (i0 & mask)];
        const float s1 = buf[static_cast<size_t> ((i0 + 1) & mask)];
        const float del = s0 + frac * (s1 - s0);

        ++wPos;
        const float wet = x * (1.0f - m * 0.35f) + del * (m * 0.9f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    std::vector<float> buf;
    int mask = 4095, wPos = 0;
    float phase = 0.0f;
    float bypassSmooth = 0.0f;
};

class BassPhaserFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        reset();
        bypassSmooth = 0.0f;
    }
    void reset() noexcept
    {
        z.fill (0.0f);
        fbState = 0.0f;
        phase = 0.0f;
    }

    inline float process (float x, bool enabled, float rate, float depth, float feedback) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        const float r  = std::clamp (rate     * 0.01f, 0.0f, 1.0f);
        const float d  = std::clamp (depth    * 0.01f, 0.0f, 1.0f);
        const float fb = std::clamp (feedback * 0.01f, 0.0f, 1.0f) * 0.72f;

        const float lfoHz = 0.12f + r * 3.8f;
        phase += static_cast<float> (lfoHz / sr);
        if (phase >= 1.0f) phase -= 1.0f;

        const float mod = 0.5f + 0.5f * std::sin (2.0f * 3.14159265f * phase);
        const float fc  = 280.0f + mod * (300.0f + d * 2400.0f);
        const float w0  = std::tan (3.14159265f * std::min (fc, static_cast<float> (sr) * 0.42f) / static_cast<float> (sr));
        const float a   = (1.0f - w0) / (1.0f + w0);

        float sig = x + fbState * fb;
        for (int i = 0; i < 4; ++i)
        {
            const float out = -a * sig + z[static_cast<size_t> (i)];
            z[static_cast<size_t> (i)] = sig + a * out;
            sig = out;
        }
        fbState = std::clamp (sig, -2.0f, 2.0f);
        const float wet = x * 0.72f + sig * 0.72f;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    std::array<float, 4> z { 0, 0, 0, 0 };
    float fbState = 0.0f;
    float phase = 0.0f;
    float bypassSmooth = 0.0f;
};

class BassReverbFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        const double scale = sr / 44100.0;
        constexpr std::array<int, 4> baseCombs { 1116, 1188, 1277, 1356 };
        constexpr std::array<int, 2> baseAp    { 225, 556 };
        for (int i = 0; i < 4; ++i)
        {
            const int len = std::max (64, static_cast<int> (std::round (baseCombs[static_cast<size_t> (i)] * scale)));
            combs[static_cast<size_t> (i)].assign (static_cast<size_t> (len), 0.0f);
            cIdx[static_cast<size_t> (i)] = 0;
            cLp[static_cast<size_t> (i)]  = 0.0f;
        }
        for (int i = 0; i < 2; ++i)
        {
            const int len = std::max (32, static_cast<int> (std::round (baseAp[static_cast<size_t> (i)] * scale)));
            aps[static_cast<size_t> (i)].assign (static_cast<size_t> (len), 0.0f);
            aIdx[static_cast<size_t> (i)] = 0;
        }
        toneLp.setLowPass (sr, 3500.0, 0.7071);
        bypassSmooth = 0.0f;
    }

    void reset() noexcept
    {
        for (auto& c : combs) std::fill (c.begin(), c.end(), 0.0f);
        for (auto& a : aps)   std::fill (a.begin(), a.end(), 0.0f);
        cLp.fill (0.0f);
        toneLp.reset();
    }

    inline float process (float x, bool enabled, float decay, float tone, float mix) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        const float dec = std::clamp (decay * 0.01f, 0.0f, 1.0f);
        const float tn  = std::clamp (tone  * 0.01f, 0.0f, 1.0f);
        const float mx  = std::clamp (mix   * 0.01f, 0.0f, 1.0f);

        if (std::fabs (tn - lastTone) > 0.01f)
        {
            lastTone = tn;
            toneLp.setLowPass (sr, 900.0 * std::pow (10.0, static_cast<double> (tn)), 0.7071);
        }

        const float roomFb = 0.68f + dec * 0.27f;
        const float damp   = 0.35f - tn * 0.24f;
        const float inFeed = x * 0.20f;

        float acc = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto& buf = combs[static_cast<size_t> (i)];
            int&  idx = cIdx[static_cast<size_t> (i)];
            const float y = buf[static_cast<size_t> (idx)];
            cLp[static_cast<size_t> (i)] = y * (1.0f - damp) + cLp[static_cast<size_t> (i)] * damp;
            buf[static_cast<size_t> (idx)] = inFeed + cLp[static_cast<size_t> (i)] * roomFb;
            if (++idx >= static_cast<int> (buf.size())) idx = 0;
            acc += y;
        }

        for (int i = 0; i < 2; ++i)
        {
            auto& buf = aps[static_cast<size_t> (i)];
            int&  idx = aIdx[static_cast<size_t> (i)];
            const float bufOut = buf[static_cast<size_t> (idx)];
            const float y = -acc + bufOut;
            buf[static_cast<size_t> (idx)] = acc + bufOut * 0.5f;
            if (++idx >= static_cast<int> (buf.size())) idx = 0;
            acc = y;
        }

        const float rev = toneLp.processSample (acc);
        const float wet = x * (1.0f - mx * 0.25f) + rev * (mx * 0.95f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    std::array<std::vector<float>, 4> combs;
    std::array<int, 4> cIdx { 0, 0, 0, 0 };
    std::array<float, 4> cLp { 0, 0, 0, 0 };
    std::array<std::vector<float>, 2> aps;
    std::array<int, 2> aIdx { 0, 0 };
    xvdsp::Biquad toneLp;
    float lastTone = -1.0f;
    float bypassSmooth = 0.0f;
};

class BassHeadProcessor
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        inHpf.setHighPass (sr, 32.0, 0.7071);
        flLo.setLowShelf (sr, 65.0, 0.0);
        flMid.setPeaking (sr, 450.0, 0.0, 0.9);
        flHi.setHighShelf (sr, 4200.0, 0.0);
        eqBass.setLowShelf (sr, 80.0, 0.0);
        eqMid.setPeaking (sr, 600.0, 0.0, 1.2);
        eqTreble.setHighShelf (sr, 3200.0, 0.0);
        cab.prepare (sr);
        muteSmooth = 1.0f;
        cabSmooth  = 1.0f;
        lastUltraHi = false;
        lastBass = lastMid = lastFreq = lastTreble = -1.0f;
    }

    void reset() noexcept
    {
        inHpf.reset(); flLo.reset(); flMid.reset(); flHi.reset();
        eqBass.reset(); eqMid.reset(); eqTreble.reset();
        cab.reset();
    }

    inline float process (float x, const BassControlSnapshot& snap, bool useHeadCab) noexcept
    {
        const bool activeOut = snap.ampPower && ! snap.ampStandby && ! snap.tunerMuted;
        muteSmooth += 0.006f * ((activeOut ? 1.0f : 0.0f) - muteSmooth);

        if (snap.ampUltraHi != lastUltraHi
            || std::fabs (snap.ampBass - lastBass) > 0.25f
            || std::fabs (snap.ampMid - lastMid) > 0.25f
            || std::fabs (snap.ampFreq - lastFreq) > 0.25f
            || std::fabs (snap.ampTreble - lastTreble) > 0.25f)
        {
            lastUltraHi = snap.ampUltraHi;
            lastBass    = snap.ampBass;
            lastMid     = snap.ampMid;
            lastFreq    = snap.ampFreq;
            lastTreble  = snap.ampTreble;

            if (snap.ampUltraHi)
            {
                flLo.setLowShelf (sr, 65.0, 0.0);
                flMid.setPeaking (sr, 450.0, 0.0, 0.9);
                flHi.setHighShelf (sr, 4200.0, 4.8);
            }
            else
            {
                flLo.setLowShelf (sr, 65.0, 4.0);
                flMid.setPeaking (sr, 450.0, -2.5, 0.9);
                flHi.setHighShelf (sr, 4200.0, 0.0);
            }

            eqBass.setLowShelf (sr, 80.0, (snap.ampBass * 0.01 - 0.5) * 18.0);
            const double midHz = 220.0 * std::pow (2800.0 / 220.0, snap.ampFreq * 0.01);
            eqMid.setPeaking (sr, midHz, (snap.ampMid * 0.01 - 0.5) * 18.0, 1.2);
            eqTreble.setHighShelf (sr, 3200.0, (snap.ampTreble * 0.01 - 0.5) * 18.0);
        }

        const float gn = std::clamp (snap.ampGain * 0.01f, 0.0f, 1.0f);
        const float driveScale = 0.55f + gn * 2.6f;
        const float compGain   = 1.0f / (0.65f + gn * 0.55f);

        float s = inHpf.processSample (x);
        const float v = s * driveScale;
        s = (std::tanh (v * 3.4f + 0.12f * v * std::fabs (v)) * 0.85f) * compGain;
        s = flHi.processSample (flMid.processSample (flLo.processSample (s)));
        s = eqTreble.processSample (eqMid.processSample (eqBass.processSample (s)));

        cabSmooth += 0.008f * ((useHeadCab ? 1.0f : 0.0f) - cabSmooth);
        const float cabSig = cab.processSample (s, 0) * 0.85f;
        const float mixed  = cabSig * cabSmooth + s * (1.0f - cabSmooth);

        return std::tanh (mixed * 0.95f) * 0.94f * muteSmooth;
    }

private:
    double sr = 48000.0;
    xvdsp::Biquad inHpf, flLo, flMid, flHi, eqBass, eqMid, eqTreble;
    xvdsp::CabIrConvolver cab;
    float muteSmooth = 1.0f, cabSmooth = 1.0f;
    bool  lastUltraHi = false;
    float lastBass = -1.0f, lastMid = -1.0f, lastFreq = -1.0f, lastTreble = -1.0f;
};

class MiAmpBassChannel
{
public:
    void prepare (double sampleRate)
    {
        pitcher.prepare (sampleRate);
        comp.prepare (sampleRate);
        xv.prepare (sampleRate);
        chorus.prepare (sampleRate);
        phaser.prepare (sampleRate);
        reverb.prepare (sampleRate);
        head.prepare (sampleRate);
    }

    void reset() noexcept
    {
        pitcher.reset(); comp.reset(); xv.reset();
        chorus.reset(); phaser.reset(); reverb.reset(); head.reset();
    }

    inline float processSample (float rawIn, const BassControlSnapshot& snap) noexcept
    {
        pitcher.setParameters (snap.pitchEnabled, snap.pitchSemitones, snap.pitchCents, snap.pitchMix);
        float s = pitcher.processSample (rawIn);

        bool xvCabActive = false;
        std::array<bool, 5> seen { false, false, false, false, false };

        for (int slot = 0; slot < 5; ++slot)
        {
            const int id = snap.chainSlots[static_cast<size_t> (slot)];
            if (id < 0 || id >= 5 || seen[static_cast<size_t> (id)])
                continue;
            seen[static_cast<size_t> (id)] = true;

            switch (static_cast<PedalType> (id))
            {
                case PedalType::Compresor:
                    s = comp.process (s, snap.compOn, snap.compSustain, snap.compAttack, snap.compLevel);
                    break;
                case PedalType::XV:
                {
                    xvdsp::PedalControlSnapshot xvSnap;
                    xvSnap.level        = snap.xvLevel * 0.1f;
                    xvSnap.drive        = snap.xvDrive * 0.1f;
                    xvSnap.lowDb        = -12.0f + snap.xvLow * 0.24f;
                    xvSnap.highDb       = -12.0f + snap.xvHigh * 0.24f;
                    xvSnap.blend        = snap.xvBlend * 0.01f;
                    xvSnap.midDb        = -12.0f + snap.xvMid * 0.24f;
                    xvSnap.presenceDb   = -12.0f + snap.xvPresence * 0.24f;
                    xvSnap.cab          = static_cast<xvdsp::CabMode> (std::clamp (snap.xvCab, 0, 2));
                    xvSnap.bypassed     = ! snap.xvOn;
                    xvSnap.tunerMuted   = false;
                    xvSnap.pitchEnabled = false; // handled once at input stage
                    xv.updateParameters (xvSnap);
                    s = xv.processSample (s);
                    if (snap.xvOn && (snap.xvCab == 0 || snap.xvCab == 1))
                        xvCabActive = true;
                    break;
                }
                case PedalType::Chorus:
                    s = chorus.process (s, snap.chorusOn, snap.chorusRate, snap.chorusDepth, snap.chorusMix);
                    break;
                case PedalType::Phaser:
                    s = phaser.process (s, snap.phaserOn, snap.phaserRate, snap.phaserDepth, snap.phaserFeedback);
                    break;
                case PedalType::Reverb:
                    s = reverb.process (s, snap.reverbOn, snap.reverbDecay, snap.reverbTone, snap.reverbMix);
                    break;
                default:
                    break;
            }
        }

        const bool useHeadCab = snap.ampCabOn && ! xvCabActive;
        return head.process (s, snap, useHeadCab);
    }

private:
    xvdsp::XVBassPitchShifter pitcher;
    BassCompressorFX comp;
    xvdsp::XVChannelProcessor xv;
    BassChorusFX chorus;
    BassPhaserFX phaser;
    BassReverbFX reverb;
    BassHeadProcessor head;
};

class MiAmpBassChainDSP
{
public:
    void prepare (double sampleRate, int /*samplesPerBlock*/, int numChannels)
    {
        const int chCount = std::clamp (numChannels, 1, 2);
        channels.resize (static_cast<size_t> (chCount));
        for (auto& ch : channels)
            ch.prepare (sampleRate);
    }

    void reset() noexcept
    {
        for (auto& ch : channels)
            ch.reset();
    }

    template <typename AudioBufferType>
    void process (AudioBufferType& buffer, const BassControlSnapshot& snap) noexcept
    {
        const int numCh = std::min (static_cast<int> (channels.size()), buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        if (numCh <= 0 || numSamples <= 0)
            return;

        if (numCh == 2 && std::memcmp (buffer.getReadPointer (0),
                                       buffer.getReadPointer (1),
                                       static_cast<size_t> (numSamples) * sizeof (float)) == 0)
        {
            float* data0 = buffer.getWritePointer (0);
            for (int i = 0; i < numSamples; ++i)
                data0[i] = channels[0].processSample (data0[i], snap);
            std::memcpy (buffer.getWritePointer (1), data0, static_cast<size_t> (numSamples) * sizeof (float));
            return;
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = channels[static_cast<size_t> (ch)].processSample (data[i], snap);
        }
    }

private:
    std::vector<MiAmpBassChannel> channels;
};

} // namespace miamp
