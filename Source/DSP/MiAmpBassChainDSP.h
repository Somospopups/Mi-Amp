#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "XVBassDSP.h"
#include "XVCabIRs.h"
#include "XVPitchShifter.h"
#include "MiAmpBoutiquePedalsDSP.h"

namespace miamp
{

enum class PedalType : int
{
    Compresor  = 0,  // MI-87 COMP
    XV         = 1,  // XV PREAMP (actual)
    Chorus     = 2,  // MI DEEP CHORUS
    Phaser     = 3,  // MI LIME PHASE
    Reverb     = 4,  // MI STUDIO
    BassDriver = 5,  // MI BASS DRIVER
    GoldDrive  = 6,  // MI GOLD
    BassFood   = 7,  // BASS FOOD
    OliveFuzz  = 8,  // MI OLIVE
    Flanger    = 9,  // MI VIOLET
    Delay      = 10, // MI PEARL
    NoiseGate  = 11, // MI SILENT
    Octaver    = 12, // MI POLY OCTAVE
    BassEq     = 13, // MI BASS EQ
    Count      = 14
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

    // Tuner Mute & Direct Pedalboard Mode (Head Bypass)
    bool  tunerMuted = false;
    bool  headBypass = false;

    // Pedalboard order: up to 10 slots (-1 = empty, 0..13 = PedalType)
    std::array<int, 10> chainSlots { 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    // 0: MI-87 COMP (5 knobs: Release, Ratio, Attack, Output, Input)
    bool  compOn      = true;
    float compSustain = 50.0f; // Ratio
    float compAttack  = 50.0f; // Attack
    float compLevel   = 50.0f; // Output
    float compRelease = 45.0f; // Release
    float compInput   = 55.0f; // Input

    // 1: XV Bass (0..100 UI scale)
    bool  xvOn       = true;
    float xvLevel    = 56.0f;
    float xvDrive    = 40.8f;
    float xvLow      = 65.4f;
    float xvHigh     = 53.5f;
    float xvBlend    = 57.6f;
    float xvMid      = 25.8f;
    float xvPresence = 80.4f;
    int   xvCab      = 1; // 0=BASS, 1=PUNK, 2=CLEAN

    // 2: MI DEEP CHORUS (Level, Lo-Cut, Rate, Depth + X-Over)
    bool  chorusOn    = true;
    float chorusRate  = 50.0f;
    float chorusDepth = 50.0f;
    float chorusMix   = 50.0f; // Level
    float chorusLoCut = 40.0f;
    bool  chorusXOver = false;

    // 3: MI LIME PHASE (Rate, Depth, Res, Stage)
    bool  phaserOn       = true;
    float phaserRate     = 50.0f;
    float phaserDepth    = 50.0f;
    float phaserFeedback = 50.0f; // Res
    float phaserStage    = 45.0f; // Stage

    // 4: MI STUDIO (Level, Tone, Decay/Time, Mode)
    bool  reverbOn    = true;
    float reverbDecay = 50.0f;
    float reverbTone  = 50.0f;
    float reverbMix   = 50.0f;
    float reverbMode  = 35.0f;

    // 5: MI BASS DRIVER (Level, Blend, Treble, Presence, Drive, Bass + 3 toggles)
    bool  bddiOn       = false;
    float bddiLevel    = 58.0f, bddiBlend = 62.0f, bddiTreble = 56.0f;
    float bddiPresence = 65.0f, bddiDrive = 48.0f, bddiBass   = 60.0f;
    bool  bddiXlrPad   = false, bddiInBoost = false, bddiOutBoost = false;

    // 6: MI GOLD (Level, EQ, Blend, Gain)
    bool  goldOn    = false;
    float goldLevel = 56.0f, goldEq = 54.0f, goldBlend = 58.0f, goldGain = 52.0f;

    // 7: BASS FOOD (Vol, Treble, Drive, Blend + -10dB toggle)
    bool  foodOn    = false;
    float foodVol   = 58.0f, foodTreble = 54.0f, foodDrive = 42.0f, foodBlend = 55.0f;
    bool  foodPad   = false;

    // 8: MI OLIVE (Volume, Tone, Sustain + 3-way Boost/Norm/Dry)
    bool  oliveOn   = false;
    float oliveVol  = 55.0f, oliveTone = 52.0f, oliveSustain = 62.0f;
    int   oliveMode = 1; // 0=Dry, 1=Norm, 2=Boost

    // 9: MI VIOLET (Manual, Res, Depth, Rate + Mode)
    bool  flangeOn  = false;
    float flangeManual = 48.0f, flangeRes = 45.0f, flangeDepth = 55.0f, flangeRate = 36.0f;
    int   flangeMode = 0;

    // 10: MI PEARL (Level, Feedback, Time, Mode + Subdiv)
    bool  delayOn   = false;
    float delayMix  = 35.0f, delayFb = 36.0f, delayTime = 42.0f, delayMode = 30.0f;
    bool  delaySubdiv = false;

    // 11: MI SILENT (Thresh, Decay, Mode)
    bool  gateOn    = false;
    float gateThresh = 32.0f, gateDecay = 40.0f, gateMode = 50.0f;

    // 12: MI POLY OCTAVE (Dry, Sub, Oct Up)
    bool  octOn     = false;
    float octDry    = 85.0f, octSub = 58.0f, octUp = 28.0f;

    // 13: MI BASS EQ (50, 120, 400, 500, 800, 4.5K, LVL)
    bool  eqOn      = false;
    std::array<float, 7> eqBands { 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f };
};

class BassCompressorFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        env = 0.0f;
        bypassSmooth = 0.0f;
        lastGrDb = 0.0f;
        lastRatio = lastAttack = lastRelease = -1.0f;
    }
    void reset() noexcept { env = 0.0f; lastGrDb = 0.0f; }

    inline float process (float x, bool enabled, float ratioPct, float attack, float output,
                          float release = 45.0f, float input = 55.0f) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        if (std::fabs (ratioPct - lastRatio) > 0.25f || std::fabs (attack - lastAttack) > 0.25f || std::fabs (release - lastRelease) > 0.25f)
        {
            lastRatio = ratioPct; lastAttack = attack; lastRelease = release;
            const float sus = std::clamp (ratioPct * 0.01f, 0.0f, 1.0f);
            const float att = std::clamp (attack   * 0.01f, 0.0f, 1.0f);
            const float rel = std::clamp (release  * 0.01f, 0.0f, 1.0f);
            attCoeffCached = std::exp (-1.0f / static_cast<float> ((0.0015f + att * 0.065f) * sr));
            relCoeffCached = std::exp (-1.0f / static_cast<float> ((0.040f  + rel * 0.420f) * sr));
            threshCached   = std::pow (10.0f, (-10.0f - sus * 24.0f) / 20.0f);
            ratioCached    = 2.0f + sus * 14.0f;
        }

        const float inGain = 0.50f + std::clamp (input * 0.01f, 0.0f, 1.0f) * 1.65f;
        const float lvl    = std::clamp (output * 0.01f, 0.0f, 1.0f);
        const float xin    = x * inGain;
        const float ax     = std::fabs (xin);
        env = (ax > env) ? (attCoeffCached * env + (1.0f - attCoeffCached) * ax)
                         : (relCoeffCached * env + (1.0f - relCoeffCached) * ax);

        float gain = 1.0f;
        float grDb = 0.0f;
        if (env > threshCached)
        {
            const float overDb = 20.0f * std::log10 (env / std::max (1e-7f, threshCached));
            grDb = overDb * (1.0f - 1.0f / ratioCached);
            gain = std::pow (10.0f, -grDb / 20.0f);
        }
        lastGrDb = enabled ? grDb : 0.0f;

        const float outGain = 0.55f + lvl * 1.45f;
        const float wet = xin * gain * outGain;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

    float getGrDb() const noexcept { return lastGrDb; }

private:
    double sr = 48000.0;
    float env = 0.0f, bypassSmooth = 0.0f, lastGrDb = 0.0f;
    float lastRatio = -1.0f, lastAttack = -1.0f, lastRelease = -1.0f;
    float attCoeffCached = 0.99f, relCoeffCached = 0.9998f, threshCached = 0.1f, ratioCached = 4.0f;
};

class BassChorusFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        buf.assign (4096, 0.0f);
        mask = 4095; wPos = 0; phase = 0.0f; bypassSmooth = 0.0f;
        hpWet.reset(); lastLoCut = -1.0f; lastXOver = false;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); wPos = 0; phase = 0.0f; hpWet.reset(); }

    inline float process (float x, bool enabled, float rate, float depth, float mix,
                          float loCut = 40.0f, bool xOver = false) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        if (std::fabs (loCut - lastLoCut) > 0.25f || xOver != lastXOver)
        {
            lastLoCut = loCut; lastXOver = xOver;
            const double fc = (xOver ? 120.0 : 35.0) + std::clamp (loCut * 0.01, 0.0, 1.0) * 240.0;
            hpWet.setHighPass (sr, fc, 0.7071);
        }

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
        const float del = hpWet.processSample (s0 + frac * (s1 - s0));

        ++wPos;
        const float wet = x * (1.0f - m * 0.35f) + del * (m * 0.9f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    std::vector<float> buf;
    xvdsp::Biquad hpWet;
    int mask = 4095, wPos = 0;
    float phase = 0.0f, bypassSmooth = 0.0f, lastLoCut = -1.0f;
    bool lastXOver = false;
};

class BassPhaserFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        reset(); bypassSmooth = 0.0f;
    }
    void reset() noexcept { z.fill (0.0f); fbState = 0.0f; phase = 0.0f; }

    inline float process (float x, bool enabled, float rate, float depth, float feedback, float stage = 45.0f) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        const float r  = std::clamp (rate     * 0.01f, 0.0f, 1.0f);
        const float d  = std::clamp (depth    * 0.01f, 0.0f, 1.0f);
        const float fb = std::clamp (feedback * 0.01f, 0.0f, 1.0f) * 0.72f;
        const int stages = (stage > 66.0f) ? 8 : (stage > 33.0f ? 6 : 4);

        const float lfoHz = 0.12f + r * 3.8f;
        phase += static_cast<float> (lfoHz / sr);
        if (phase >= 1.0f) phase -= 1.0f;

        const float mod = 0.5f + 0.5f * std::sin (2.0f * 3.14159265f * phase);
        const float fc  = 240.0f + mod * (280.0f + d * 2400.0f);
        const float w0  = std::tan (3.14159265f * std::min (fc, static_cast<float> (sr) * 0.42f) / static_cast<float> (sr));
        const float a   = (1.0f - w0) / (1.0f + w0);

        float sig = x + fbState * fb;
        for (int i = 0; i < stages; ++i)
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
    std::array<float, 8> z { 0, 0, 0, 0, 0, 0, 0, 0 };
    float fbState = 0.0f, phase = 0.0f, bypassSmooth = 0.0f;
};

class BassOliveFuzzFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        toneLp.reset(); bassBoost.reset();
        bassBoost.setLowShelf (sr, 110.0, 6.0, 0.7071);
        lastTone = -1.0f; bypassSmooth = 0.0f;
    }
    void reset() noexcept { toneLp.reset(); bassBoost.reset(); }

    inline float process (float x, bool enabled, float volume, float tone, float sustain, int mode) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);
        if (std::fabs (tone - lastTone) > 0.25f)
        {
            lastTone = tone;
            toneLp.setLowPass (sr, 750.0 + std::clamp (tone * 0.01, 0.0, 1.0) * 4400.0, 0.7071);
        }
        const float sus = std::clamp (sustain * 0.01f, 0.0f, 1.0f);
        const float vol = std::clamp (volume  * 0.01f, 0.0f, 1.0f);
        const float sIn = (mode == 2) ? bassBoost.processSample (x) : x;
        const float f1  = std::tanh (sIn * (4.0f + sus * 28.0f) + 0.14f) - std::tanh (0.14f);
        const float f2  = toneLp.processSample (std::tanh (f1 * 2.1f) * 0.48f);
        const float out = (mode == 0) ? (x * 0.45f + f2 * 0.75f) : f2;
        const float wet = out * (0.45f + vol * 1.45f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    xvdsp::Biquad toneLp, bassBoost;
    float lastTone = -1.0f, bypassSmooth = 0.0f;
};

class BassDelayFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        bufSize = static_cast<int> (1.2 * sr) + 16;
        buf.assign (static_cast<size_t> (bufSize), 0.0f);
        writePos = 0;
        fbLp.setLowPass (sr, 3200.0, 0.7071);
        bypassSmooth = 0.0f;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; fbLp.reset(); }

    inline float process (float x, bool enabled, float time, float feedback, float mix, float mode, bool subdiv) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        const float tm = std::clamp (time     * 0.01f, 0.0f, 1.0f);
        const float fb = std::clamp (feedback * 0.01f, 0.0f, 1.0f) * 0.80f;
        const float mx = std::clamp (mix      * 0.01f, 0.0f, 1.0f);
        const int mIdx = std::clamp (static_cast<int> (std::round (mode * 0.02f)), 0, 2);

        const double subScale = subdiv ? 0.75 : 1.0;
        const double dSmp = std::clamp ((0.065 + tm * 0.685) * subScale * sr, 1.0, static_cast<double> (bufSize - 4));
        double rPos = static_cast<double> (writePos) - dSmp;
        while (rPos < 0.0) rPos += static_cast<double> (bufSize);
        const int i0 = static_cast<int> (rPos);
        const int i1 = (i0 + 1) % bufSize;
        const float frac = static_cast<float> (rPos - static_cast<double> (i0));
        const float tap = buf[static_cast<size_t> (i0)] * (1.0f - frac) + buf[static_cast<size_t> (i1)] * frac;

        const float fbSig = (mIdx == 0) ? tap : fbLp.processSample (tap);
        buf[static_cast<size_t> (writePos)] = std::tanh (x + fbSig * fb);
        if (++writePos >= bufSize) writePos = 0;

        const float wet = x * (1.0f - mx * 0.15f) + tap * (mx * 0.85f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    std::vector<float> buf;
    int bufSize = 60000, writePos = 0;
    xvdsp::Biquad fbLp;
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

    inline float process (float x, bool enabled, float decay, float tone, float mix, float mode = 40.0f) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        const float dec = std::clamp (decay * 0.01f, 0.0f, 1.0f);
        const float tn  = std::clamp (tone  * 0.01f, 0.0f, 1.0f);
        const float mx  = std::clamp (mix   * 0.01f, 0.0f, 1.0f);
        const float mScale = (mode > 66.0f) ? 0.05f : (mode < 33.0f ? -0.04f : 0.0f);

        if (std::fabs (tn - lastTone) > 0.01f)
        {
            lastTone = tn;
            toneLp.setLowPass (sr, 900.0 * std::pow (10.0, static_cast<double> (tn)), 0.7071);
        }

        const float roomFb = std::clamp (0.68f + dec * 0.27f + mScale, 0.45f, 0.96f);
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
        goldDist.prepare (sampleRate, true);
        bassFood.prepare (sampleRate);
        bassDriver.prepare (sampleRate);
        xv.prepare (sampleRate);
        oliveFuzz.prepare (sampleRate);
        comp.prepare (sampleRate);
        phaser.prepare (sampleRate);
        chorus.prepare (sampleRate);
        flanger.prepare (sampleRate);
        delay.prepare (sampleRate);
        gate.prepare (sampleRate);
        octaver.prepare (sampleRate);
        eq7.prepare (sampleRate, true);
        reverb.prepare (sampleRate);
        head.prepare (sampleRate);
    }

    void reset() noexcept
    {
        pitcher.reset(); goldDist.reset(); bassFood.reset(); bassDriver.reset(); xv.reset();
        oliveFuzz.reset(); comp.reset(); phaser.reset(); chorus.reset(); flanger.reset();
        delay.reset(); gate.reset(); octaver.reset(); eq7.reset(); reverb.reset(); head.reset();
        headBypassSmooth = 0.0f;
        directMuteSmooth = 1.0f;
    }

    float getCompGrDb() const noexcept { return comp.getGrDb(); }

    inline float processSample (float rawIn, const BassControlSnapshot& snap) noexcept
    {
        pitcher.setParameters (snap.pitchEnabled, snap.pitchSemitones, snap.pitchCents, snap.pitchMix);
        float s = pitcher.processSample (rawIn);

        bool xvCabActive = false;
        std::array<bool, static_cast<size_t> (PedalType::Count)> seen {};

        for (int slot = 0; slot < 10; ++slot)
        {
            const int id = snap.chainSlots[static_cast<size_t> (slot)];
            if (id < 0 || id >= static_cast<int> (PedalType::Count) || seen[static_cast<size_t> (id)])
                continue;
            seen[static_cast<size_t> (id)] = true;

            switch (static_cast<PedalType> (id))
            {
                case PedalType::Compresor:
                    s = comp.process (s, snap.compOn, snap.compSustain, snap.compAttack, snap.compLevel, snap.compRelease, snap.compInput);
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
                    xvSnap.pitchEnabled = false;
                    xv.updateParameters (xvSnap);
                    s = xv.processSample (s);
                    if (snap.xvOn && (snap.xvCab == 0 || snap.xvCab == 1))
                        xvCabActive = true;
                    break;
                }
                case PedalType::Chorus:
                    s = chorus.process (s, snap.chorusOn, snap.chorusRate, snap.chorusDepth, snap.chorusMix, snap.chorusLoCut, snap.chorusXOver);
                    break;
                case PedalType::Phaser:
                    s = phaser.process (s, snap.phaserOn, snap.phaserRate, snap.phaserDepth, snap.phaserFeedback, snap.phaserStage);
                    break;
                case PedalType::Reverb:
                    s = reverb.process (s, snap.reverbOn, snap.reverbDecay, snap.reverbTone, snap.reverbMix, snap.reverbMode);
                    break;
                case PedalType::BassDriver:
                    s = bassDriver.process (s, snap.bddiOn, snap.bddiLevel, snap.bddiBlend, snap.bddiTreble,
                                            snap.bddiPresence, snap.bddiDrive, snap.bddiBass,
                                            snap.bddiXlrPad, snap.bddiInBoost, snap.bddiOutBoost);
                    break;
                case PedalType::GoldDrive:
                    s = goldDist.process (s, snap.goldOn, snap.goldLevel, snap.goldEq, snap.goldBlend, snap.goldGain);
                    break;
                case PedalType::BassFood:
                    s = bassFood.process (s, snap.foodOn, snap.foodVol, snap.foodTreble, snap.foodDrive, snap.foodBlend, snap.foodPad);
                    break;
                case PedalType::OliveFuzz:
                    s = oliveFuzz.process (s, snap.oliveOn, snap.oliveVol, snap.oliveTone, snap.oliveSustain, snap.oliveMode);
                    break;
                case PedalType::Flanger:
                    s = flanger.process (s, snap.flangeOn, snap.flangeManual, snap.flangeRes, snap.flangeDepth, snap.flangeRate, snap.flangeMode);
                    break;
                case PedalType::Delay:
                    s = delay.process (s, snap.delayOn, snap.delayTime, snap.delayFb, snap.delayMix, snap.delayMode, snap.delaySubdiv);
                    break;
                case PedalType::NoiseGate:
                    s = gate.process (s, snap.gateOn, snap.gateThresh, snap.gateDecay, snap.gateMode);
                    break;
                case PedalType::Octaver:
                    s = octaver.process (s, snap.octOn, snap.octDry, snap.octUp, snap.octSub);
                    break;
                case PedalType::BassEq:
                    s = eq7.process (s, snap.eqOn, snap.eqBands);
                    break;
                default:
                    break;
            }
        }

        const bool useHeadCab = snap.ampCabOn && ! xvCabActive;
        const float ampOut = head.process (s, snap, useHeadCab);

        directMuteSmooth += 0.006f * ((snap.tunerMuted ? 0.0f : 1.0f) - directMuteSmooth);
        headBypassSmooth += 0.008f * ((snap.headBypass ? 1.0f : 0.0f) - headBypassSmooth);
        const float directOut = s * directMuteSmooth;
        return ampOut * (1.0f - headBypassSmooth) + directOut * headBypassSmooth;
    }

private:
    float headBypassSmooth = 0.0f;
    float directMuteSmooth = 1.0f;
    xvdsp::XVBassPitchShifter pitcher;
    mipedals::BoutiqueDistortionFX goldDist;
    mipedals::BoutiqueBassFoodFX   bassFood;
    mipedals::BoutiqueBassDriverDIFX bassDriver;
    xvdsp::XVChannelProcessor xv;
    BassOliveFuzzFX      oliveFuzz;
    BassCompressorFX     comp;
    BassPhaserFX         phaser;
    BassChorusFX         chorus;
    mipedals::BoutiqueFlangerFX    flanger;
    BassDelayFX          delay;
    mipedals::BoutiqueNoiseGateFX  gate;
    mipedals::BoutiqueOctaverFX    octaver;
    mipedals::Boutique7BandEQFX    eq7;
    BassReverbFX         reverb;
    BassHeadProcessor    head;
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

    float getLastCompGrDb() const noexcept
    {
        return channels.empty() ? 0.0f : channels[0].getCompGrDb();
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
