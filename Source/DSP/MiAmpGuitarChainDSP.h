#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>
#include "XVBassDSP.h"
#include "XVPitchShifter.h"
#include "ZenboxCabIRs.h"
#include "MiAmpBoutiquePedalsDSP.h"

namespace zenbox
{

enum class GuitarPedalType : int
{
    Compresor  = 0,  // MI ICE COMP
    Drive      = 1,  // MI GREEN DRIVE
    Fuzz       = 2,  // MI SUSTAIN
    Chorus     = 3,  // MI SKY CHORUS
    Phaser     = 4,  // MI PHASER
    Delay      = 5,  // MI PEARL
    Reverb     = 6,  // MI STUDIO
    Distortion = 7,  // MI ORANGE
    DualCrunch = 8,  // MI DUAL CRUNCH
    Flanger    = 9,  // MI VIOLET
    NoiseGate  = 10, // MI SILENT
    Octaver    = 11, // MI SUB OCTAVE
    GraphicEq  = 12, // MI GRAPHIC EQ
    Count      = 13
};

struct GuitarControlSnapshot
{
    // Global Utility (Zenbox top/side bar)
    float inputGain  = 50.0f; // 0..100 -> -12..+12 dB
    float gateThresh = 18.0f; // 0..100 (0 = off)
    bool  os4x       = true;  // 4x Oversampling
    int   cabIndex   = 1;     // 0..3 = 4 Zenbox Cabs, 4 = CAB OFF

    // Amp switches & Model Selection
    int   headModel  = 0;     // 0=800, 1=Plexi, 2=Recto, 3=6505, 4=AC30, 5=Rocker, 6=BE100, 7=Twin, 8=Zenbox
    bool  useExternalCabEngine = true;
    bool  ampPower   = true;
    bool  ampStandby = false;
    bool  ampModeBright = true; // MODE switch = 70s Bright Cap
    int   activeChannel = 1;    // 0 = CH1 (Clean/Breakup), 1 = CH2 (Zenbox Crunch), 2 = CH3 (Punk Lead)

    // 3 Channels x 6 knobs (Gain, Bass, Mid, Treble, Presence, Master) + Output
    std::array<float, 3> chGain     { 38.0f, 56.0f, 74.0f };
    std::array<float, 3> chBass     { 52.0f, 54.0f, 50.0f };
    std::array<float, 3> chMid      { 50.0f, 58.0f, 52.0f };
    std::array<float, 3> chTreble   { 54.0f, 58.0f, 60.0f };
    std::array<float, 3> chPresence { 50.0f, 56.0f, 62.0f };
    std::array<float, 3> chMaster   { 62.0f, 58.0f, 56.0f };
    float outputKnob = 58.0f;

    // Extended controls for modeled amp heads (v25)
    float resonance  = 50.0f;
    float vol2       = 45.0f;
    float cleanBass  = 52.0f;
    float cleanTreb  = 56.0f;
    float ampReverb  = 0.0f;
    float tremSpeed  = 45.0f;
    float tremDepth  = 0.0f;
    float attenuator = 85.0f;
    bool  bright2    = false;
    bool  crunchSw   = false;
    bool  halfPower  = false;
    int   voiceSw    = 1; // 0..2 (e.g. Clean / Vintage / Modern or Voice)

    // Pre-FX Pitch Shifter, Tuner Mute & Direct Pedalboard Mode (Head Bypass)
    bool  pitchEnabled   = false;
    int   pitchSemitones = 0;
    float pitchCents     = 0.0f;
    float pitchMix       = 100.0f;
    bool  tunerMuted     = false;
    bool  headBypass     = false;

    // Pedalboard slots (-1 = empty, 0..12 = GuitarPedalType)
    std::array<int, 10> chainSlots { 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

    // 0: MI ICE COMP
    bool  compOn = true; float compSustain = 50.0f, compAttack = 45.0f, compLevel = 52.0f, compTone = 52.0f;
    // 1: MI GREEN DRIVE
    bool  driveOn = false; float driveGain = 42.0f, driveTone = 58.0f, driveLevel = 62.0f; bool driveClip = false;
    // 2: MI SUSTAIN
    bool  fuzzOn = false; float fuzzFuzz = 65.0f, fuzzTone = 52.0f, fuzzLevel = 55.0f;
    // 3: MI SKY CHORUS
    bool  chorusOn = false; float chorusRate = 42.0f, chorusDepth = 48.0f, chorusMix = 44.0f, chorusEq = 55.0f; int chorusMode = 0;
    // 4: MI PHASER (1-knob Speed; depth/fb fixed warm classic or preset compat)
    bool  phaserOn = false; float phaserRate = 38.0f, phaserDepth = 62.0f, phaserFeedback = 48.0f;
    // 5: MI PEARL (Delay)
    bool  delayOn = false; float delayTime = 42.0f, delayFeedback = 36.0f, delayMix = 32.0f, delayMode = 30.0f; bool delaySubdiv = false;
    // 6: MI STUDIO (Reverb)
    bool  reverbOn = false; float reverbDecay = 46.0f, reverbTone = 56.0f, reverbMix = 28.0f, reverbMode = 35.0f;
    // 7: MI ORANGE (Distortion)
    bool  distOn = false; float distTone = 52.0f, distLevel = 60.0f, distDrive = 62.0f;
    // 8: MI DUAL CRUNCH (Preamp)
    bool  dualOn = false; float dualOutput = 55.0f, dualGain = 58.0f, dualBlend = 50.0f, dualTone = 56.0f, dualDrive = 60.0f; bool dualScoop = false;
    // 9: MI VIOLET (Flanger)
    bool  flangeOn = false; float flangeManual = 48.0f, flangeRes = 45.0f, flangeDepth = 55.0f, flangeRate = 36.0f; int flangeMode = 0;
    // 10: MI SILENT (Noise Gate)
    bool  gatePedalOn = false; float gatePedalThresh = 32.0f, gatePedalDecay = 40.0f, gatePedalMode = 50.0f;
    // 11: MI SUB OCTAVE (Octaver)
    bool  octOn = false; float octDirect = 85.0f, octUp = 25.0f, octSub1 = 55.0f, octSub2 = 20.0f;
    // 12: MI GRAPHIC EQ (7-Band)
    bool  eqOn = false; std::array<float, 7> eqBands { 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f, 50.0f };
};

//==============================================================================
class GuitarNoiseGate
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = std::max (22050.0, sampleRate);
        attEnv = std::exp (-1.0f / static_cast<float> (0.0012 * sr));
        relEnv = std::exp (-1.0f / static_cast<float> (0.085  * sr));
        gainAtt = std::exp (-1.0f / static_cast<float> (0.0015 * sr));
        gainRel = std::exp (-1.0f / static_cast<float> (0.075  * sr));
        env = 0.0f;
        gateGain = 1.0f;
        holdSamples = 0;
    }

    void reset() noexcept { env = 0.0f; gateGain = 1.0f; holdSamples = 0; }

    inline float process (float x, float threshPct) noexcept
    {
        if (threshPct <= 1.0f)
        {
            gateGain += 0.01f * (1.0f - gateGain);
            return x * gateGain;
        }

        const float ax = std::fabs (x);
        env = (ax > env) ? (attEnv * env + (1.0f - attEnv) * ax)
                         : (relEnv * env + (1.0f - relEnv) * ax);

        const float tNorm = std::clamp (threshPct * 0.01f, 0.0f, 1.0f);
        const float openDb  = -78.0f + tNorm * 52.0f; // -78..-26 dBFS
        const float openThr = std::pow (10.0f, openDb / 20.0f);
        const float closeThr = openThr * 0.65f;

        if (env >= openThr)
        {
            isOpen = true;
            holdSamples = static_cast<int> (0.045 * sr);
        }
        else if (env < closeThr)
        {
            if (holdSamples > 0) --holdSamples;
            else isOpen = false;
        }

        const float target = isOpen ? 1.0f : 0.002f;
        gateGain = (target > gateGain) ? (gainAtt * gateGain + (1.0f - gainAtt) * target)
                                       : (gainRel * gateGain + (1.0f - gainRel) * target);
        return x * gateGain;
    }

private:
    double sr = 48000.0;
    float attEnv = 0.98f, relEnv = 0.999f, gainAtt = 0.98f, gainRel = 0.999f;
    float env = 0.0f, gateGain = 1.0f;
    int holdSamples = 0;
    bool isOpen = false;
};

//==============================================================================
class GuitarCompFX
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = std::max (22050.0, sampleRate);
        relCoeff = std::exp (-1.0f / static_cast<float> (0.14 * sr));
        env = 0.0f; bypassSmooth = 0.0f;
        lastSus = lastAtt = lastTone = -1.0f;
        toneShelf.reset();
    }
    void reset() noexcept { env = 0.0f; toneShelf.reset(); }

    inline float process (float x, bool enabled, float sustain, float attack, float level, float tone = 50.0f) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        if (std::fabs (sustain - lastSus) > 0.25f || std::fabs (attack - lastAtt) > 0.25f)
        {
            lastSus = sustain; lastAtt = attack;
            const float sus = std::clamp (sustain * 0.01f, 0.0f, 1.0f);
            const float att = std::clamp (attack  * 0.01f, 0.0f, 1.0f);
            attCoeff = std::exp (-1.0f / static_cast<float> ((0.002 + att * 0.065) * sr));
            thresh   = std::pow (10.0f, (-10.0f - sus * 32.0f) / 20.0f);
            ratio    = 2.0f + sus * 8.0f;
            makeup   = 1.0f + sus * 0.55f;
        }
        if (std::fabs (tone - lastTone) > 0.25f)
        {
            lastTone = tone;
            toneShelf.setHighShelf (sr, 2200.0, (std::clamp (tone * 0.01, 0.0, 1.0) - 0.5) * 10.0, 0.7071);
        }
        const float ax = std::fabs (x);
        env = (ax > env) ? (attCoeff * env + (1.0f - attCoeff) * ax)
                         : (relCoeff * env + (1.0f - relCoeff) * ax);
        float gr = 1.0f;
        if (env > thresh)
        {
            const float overDb = 20.0f * std::log10 (env / std::max (1e-7f, thresh));
            gr = std::pow (10.0f, -(overDb * (1.0f - 1.0f / ratio)) / 20.0f);
        }
        const float outG = (0.6f + std::clamp (level * 0.01f, 0.0f, 1.0f) * 1.25f) * makeup;
        const float wet = toneShelf.processSample (x * gr * outG);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    xvdsp::Biquad toneShelf;
    float relCoeff = 0.9998f, env = 0.0f, bypassSmooth = 0.0f;
    float lastSus = -1.0f, lastAtt = -1.0f, lastTone = -1.0f, attCoeff = 0.99f, thresh = 0.1f, ratio = 4.0f, makeup = 1.2f;
};

//==============================================================================
// Zenbox Screamer / Classic Overdrive Pedal (MI GREEN DRIVE)
class GuitarDriveFX
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = std::max (22050.0, sampleRate);
        preHpf.setHighPass (sr, 220.0, 0.7071);
        midHump.setPeaking (sr, 760.0, 4.2, 0.85);
        postLpf.setLowPass (sr, 4200.0, 0.7071);
        bypassSmooth = 0.0f;
        lastTone = -1.0f;
    }
    void reset() noexcept { preHpf.reset(); midHump.reset(); postLpf.reset(); }

    inline float process (float x, bool enabled, float gain, float tone, float level, bool asymClip = false) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        if (std::fabs (tone - lastTone) > 0.25f)
        {
            lastTone = tone;
            const double t = std::clamp (tone * 0.01, 0.0, 1.0);
            postLpf.setLowPass (sr, 1600.0 + t * 5200.0, 0.7071);
        }
        const float g = std::clamp (gain  * 0.01f, 0.0f, 1.0f);
        const float l = std::clamp (level * 0.01f, 0.0f, 1.0f);

        float d = midHump.processSample (preHpf.processSample (x));
        const float drv = 1.8f + g * 18.0f;
        const float s = d * drv;
        float clip = asymClip
            ? ((s >= 0.0f) ? std::tanh (s * 1.35f) : (std::tanh (s * 0.78f) * 0.88f))
            : std::tanh (s * 1.05f);
        clip = postLpf.processSample (clip * 0.55f + d * 0.18f);
        const float wet = clip * (0.55f + l * 1.45f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    xvdsp::Biquad preHpf, midHump, postLpf;
    float lastTone = -1.0f, bypassSmooth = 0.0f;
};

//==============================================================================
class GuitarFuzzFX
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = std::max (22050.0, sampleRate);
        preLp.setLowPass (sr, 4800.0, 0.7071);
        toneLp.setLowPass (sr, 3200.0, 0.7071);
        subShelf.setLowShelf (sr, 140.0, 3.2);
        bypassSmooth = 0.0f;
        lastTone = -1.0f;
    }
    void reset() noexcept { preLp.reset(); toneLp.reset(); subShelf.reset(); }

    inline float process (float x, bool enabled, float fuzz, float tone, float level) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        if (std::fabs (tone - lastTone) > 0.25f)
        {
            lastTone = tone;
            const double t = std::clamp (tone * 0.01, 0.0, 1.0);
            toneLp.setLowPass (sr, 900.0 + t * 4800.0, 0.7071);
        }
        const float f = std::clamp (fuzz  * 0.01f, 0.0f, 1.0f);
        const float l = std::clamp (level * 0.01f, 0.0f, 1.0f);

        float s = subShelf.processSample (preLp.processSample (x));
        const float drv = 4.0f + f * 32.0f;
        // Asymmetric germanium bias fuzz
        float y = std::tanh (s * drv + 0.18f) - std::tanh (0.18f);
        y = std::tanh (y * 2.2f) * 0.45f;
        y = toneLp.processSample (y) * (0.50f + l * 1.35f);
        return y * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    xvdsp::Biquad preLp, toneLp, subShelf;
    float lastTone = -1.0f, bypassSmooth = 0.0f;
};

//==============================================================================
class GuitarChorusFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        bufSize = std::max (1024, static_cast<int> (0.06 * sr));
        buf.assign (static_cast<size_t> (bufSize), 0.0f);
        writePos = 0; phase = 0.0; bypassSmooth = 0.0f;
        eqShelf.reset(); lastEq = -1.0f;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; eqShelf.reset(); }

    inline float process (float x, bool enabled, float rate, float depth, float mix, int chIdx,
                          float eq = 55.0f, int mode = 0) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        const float rt = std::clamp (rate  * 0.01f, 0.0f, 1.0f);
        const float dp = std::clamp (depth * 0.01f, 0.0f, 1.0f);
        const float mx = std::clamp (mix   * 0.01f, 0.0f, 1.0f);

        if (std::fabs (eq - lastEq) > 0.25f)
        {
            lastEq = eq;
            eqShelf.setHighShelf (sr, 2400.0, (std::clamp (eq * 0.01, 0.0, 1.0) - 0.5) * 10.0, 0.7071);
        }

        const double hz = 0.18 + rt * 3.8;
        phase += 2.0 * xvdsp::kPi * hz / sr;
        if (phase >= 2.0 * xvdsp::kPi) phase -= 2.0 * xvdsp::kPi;

        const double lfo = std::sin (phase + (chIdx == 1 ? 1.5707963 : 0.0));
        const double dMs = (mode == 1 ? 14.5 : 11.0) + lfo * (dp * 5.2);
        const double dSmp = std::clamp (dMs * 0.001 * sr, 1.0, static_cast<double> (bufSize - 3));

        buf[static_cast<size_t> (writePos)] = x;
        double rPos = static_cast<double> (writePos) - dSmp;
        while (rPos < 0.0) rPos += static_cast<double> (bufSize);
        const int i0 = static_cast<int> (rPos);
        const int i1 = (i0 + 1) % bufSize;
        const float frac = static_cast<float> (rPos - static_cast<double> (i0));
        const float delayed = eqShelf.processSample (buf[static_cast<size_t> (i0)] * (1.0f - frac) + buf[static_cast<size_t> (i1)] * frac);
        if (++writePos >= bufSize) writePos = 0;

        const float wet = (mode == 2)
            ? (x * (1.0f - mx * 0.85f) + delayed * (mx * 0.95f))
            : (x * (1.0f - mx * 0.35f) + delayed * (mx * 0.72f));
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0, phase = 0.0;
    std::vector<float> buf;
    xvdsp::Biquad eqShelf;
    int bufSize = 2880, writePos = 0;
    float bypassSmooth = 0.0f, lastEq = -1.0f;
};

//==============================================================================
class GuitarPhaserFX
{
public:
    void prepare (double sampleRate) noexcept
    {
        sr = std::max (22050.0, sampleRate);
        phase = 0.0; fbState = 0.0f; bypassSmooth = 0.0f;
        ap.fill (0.0f);
    }
    void reset() noexcept { ap.fill (0.0f); fbState = 0.0f; }

    inline float process (float x, bool enabled, float rate, float depth, float feedback) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        const float rt = std::clamp (rate     * 0.01f, 0.0f, 1.0f);
        const float dp = std::clamp (depth    * 0.01f, 0.0f, 1.0f);
        const float fb = std::clamp (feedback * 0.01f, 0.0f, 1.0f) * 0.74f;

        const double hz = 0.14 + rt * 4.2;
        phase += 2.0 * xvdsp::kPi * hz / sr;
        if (phase >= 2.0 * xvdsp::kPi) phase -= 2.0 * xvdsp::kPi;

        const double lfo = 0.5 * (1.0 + std::sin (phase));
        const double fc = 240.0 * std::pow (3800.0 / 240.0, lfo * dp);
        const double t  = std::tan (xvdsp::kPi * std::min (fc, sr * 0.42) / sr);
        const float  k  = static_cast<float> ((t - 1.0) / (t + 1.0));

        float s = x + fbState * fb;
        for (int i = 0; i < 4; ++i)
        {
            const float in = s;
            const float y  = -k * in + ap[static_cast<size_t> (i)];
            ap[static_cast<size_t> (i)] = in + k * y;
            s = y;
        }
        fbState = std::clamp (s, -1.5f, 1.5f);
        const float wet = 0.65f * x + 0.55f * s;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0, phase = 0.0;
    std::array<float, 4> ap { 0, 0, 0, 0 };
    float fbState = 0.0f, bypassSmooth = 0.0f;
};

//==============================================================================
class GuitarDelayFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        bufSize = static_cast<int> (1.2 * sr) + 16;
        buf.assign (static_cast<size_t> (bufSize), 0.0f);
        writePos = 0;
        fbLp.setLowPass (sr, 3400.0, 0.7071);
        bypassSmooth = 0.0f;
    }
    void reset() noexcept { std::fill (buf.begin(), buf.end(), 0.0f); writePos = 0; fbLp.reset(); }

    inline float process (float x, bool enabled, float time, float feedback, float mix,
                          float mode = 30.0f, bool subdiv = false) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        const float tm = std::clamp (time     * 0.01f, 0.0f, 1.0f);
        const float fb = std::clamp (feedback * 0.01f, 0.0f, 1.0f) * 0.80f;
        const float mx = std::clamp (mix      * 0.01f, 0.0f, 1.0f);
        const int   mIdx = std::clamp (static_cast<int> (std::round (mode * 0.02f)), 0, 2);

        const double subScale = subdiv ? 0.75 : 1.0;
        const double dSmp = std::clamp ((0.065 + tm * 0.685) * subScale * sr, 1.0, static_cast<double> (bufSize - 4));
        double rPos = static_cast<double> (writePos) - dSmp;
        while (rPos < 0.0) rPos += static_cast<double> (bufSize);
        const int i0 = static_cast<int> (rPos);
        const int i1 = (i0 + 1) % bufSize;
        const float frac = static_cast<float> (rPos - static_cast<double> (i0));
        const float tap = buf[static_cast<size_t> (i0)] * (1.0f - frac) + buf[static_cast<size_t> (i1)] * frac;

        const float fbSig = (mIdx == 0) ? tap : fbLp.processSample (tap);
        buf[static_cast<size_t> (writePos)] = std::tanh (x + fbSig * fb * (mIdx == 2 ? 0.96f : 1.0f));
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

//==============================================================================
class GuitarReverbFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        const double sc = sr / 44100.0;
        constexpr std::array<int, 4> baseC { 1116, 1188, 1277, 1356 };
        constexpr std::array<int, 2> baseA { 225, 556 };
        for (int i = 0; i < 4; ++i)
        {
            combs[static_cast<size_t> (i)].assign (std::max (64, static_cast<int> (std::round (baseC[static_cast<size_t> (i)] * sc))), 0.0f);
            cIdx[static_cast<size_t> (i)] = 0; cLp[static_cast<size_t> (i)] = 0.0f;
        }
        for (int i = 0; i < 2; ++i)
        {
            aps[static_cast<size_t> (i)].assign (std::max (32, static_cast<int> (std::round (baseA[static_cast<size_t> (i)] * sc))), 0.0f);
            aIdx[static_cast<size_t> (i)] = 0;
        }
        toneLp.setLowPass (sr, 3600.0, 0.7071);
        bypassSmooth = 0.0f; lastTone = -1.0f;
    }
    void reset() noexcept
    {
        for (auto& c : combs) std::fill (c.begin(), c.end(), 0.0f);
        for (auto& a : aps)   std::fill (a.begin(), a.end(), 0.0f);
        cLp.fill (0.0f); toneLp.reset();
    }

    inline float process (float x, bool enabled, float decay, float tone, float mix, float mode = 35.0f) noexcept
    {
        bypassSmooth += 0.008f * ((enabled ? 0.0f : 1.0f) - bypassSmooth);
        const float dec = std::clamp (decay * 0.01f, 0.0f, 1.0f);
        const float tn  = std::clamp (tone  * 0.01f, 0.0f, 1.0f);
        const float mx  = std::clamp (mix   * 0.01f, 0.0f, 1.0f);
        const int   mIdx = std::clamp (static_cast<int> (std::round (mode * 0.02f)), 0, 2);
        if (std::fabs (tn - lastTone) > 0.01f)
        {
            lastTone = tn;
            toneLp.setLowPass (sr, 1100.0 * std::pow (8.5, static_cast<double> (tn)), 0.7071);
        }
        const float modeBoost = (mIdx == 0) ? -0.05f : (mIdx == 1 ? 0.03f : 0.01f);
        const float roomFb = std::clamp (0.65f + dec * 0.27f + modeBoost, 0.45f, 0.94f);
        const float damp = 0.34f - tn * 0.22f;
        const float feed = x * 0.18f;
        float acc = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            auto& b = combs[static_cast<size_t> (i)];
            int& ix = cIdx[static_cast<size_t> (i)];
            const float y = b[static_cast<size_t> (ix)];
            cLp[static_cast<size_t> (i)] = y * (1.0f - damp) + cLp[static_cast<size_t> (i)] * damp;
            b[static_cast<size_t> (ix)] = feed + cLp[static_cast<size_t> (i)] * roomFb;
            if (++ix >= static_cast<int> (b.size())) ix = 0;
            acc += y;
        }
        for (int i = 0; i < 2; ++i)
        {
            auto& b = aps[static_cast<size_t> (i)];
            int& ix = aIdx[static_cast<size_t> (i)];
            const float bo = b[static_cast<size_t> (ix)];
            const float y = -acc + bo;
            b[static_cast<size_t> (ix)] = acc + bo * 0.5f;
            if (++ix >= static_cast<int> (b.size())) ix = 0;
            acc = y;
        }
        const float rev = toneLp.processSample (acc);
        const float wet = x * (1.0f - mx * 0.20f) + rev * (mx * 0.85f);
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
    float lastTone = -1.0f, bypassSmooth = 0.0f;
};

//==============================================================================
// 512-Tap Zero-Latency Guitar Cabinet Convolver (4 Zenbox Cabs, auto-resampled)
class ZenboxCabConvolver
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        resampleIr (kCab0_Vintage4x12, irs[0]);
        resampleIr (kCab1_Modern4x12,  irs[1]);
        resampleIr (kCab2_Open2x12,    irs[2]);
        resampleIr (kCab3_Studio1x12,  irs[3]);
        ring.fill (0.0f);
        ringIdx = 0;
    }

    void reset() noexcept { ring.fill (0.0f); ringIdx = 0; }

    inline float processSample (float x, int cabIndex) noexcept
    {
        ring[static_cast<size_t> (ringIdx)] = x;
        float acc = 0.0f;
        const auto& h = irs[static_cast<size_t> (std::clamp (cabIndex, 0, 3))];
        int idx = ringIdx;
        for (int k = 0; k < kCabIrLength; ++k)
        {
            acc += ring[static_cast<size_t> (idx)] * h[static_cast<size_t> (k)];
            if (--idx < 0) idx = kCabIrLength - 1;
        }
        if (++ringIdx >= kCabIrLength) ringIdx = 0;
        return acc;
    }

private:
    void resampleIr (const std::array<float, kCabIrLength>& src, std::array<float, kCabIrLength>& dst) const
    {
        const double ratio = static_cast<double> (kCabIrSampleRate) / sr;
        double energy = 0.0;
        for (int i = 0; i < kCabIrLength; ++i)
        {
            const double pos = i * ratio;
            const int i0 = static_cast<int> (pos);
            if (i0 >= kCabIrLength - 1) { dst[static_cast<size_t> (i)] = 0.0f; continue; }
            const float f = static_cast<float> (pos - i0);
            const float v = src[static_cast<size_t> (i0)] * (1.0f - f) + src[static_cast<size_t> (i0 + 1)] * f;
            dst[static_cast<size_t> (i)] = v;
            energy += static_cast<double> (v) * v;
        }
        const float scale = (energy > 1e-8) ? static_cast<float> (1.0 / (std::sqrt (energy) * 1.35)) : 1.0f;
        for (auto& v : dst) v *= scale;
    }

    double sr = 48000.0;
    std::array<std::array<float, kCabIrLength>, 4> irs {};
    std::array<float, kCabIrLength> ring {};
    int ringIdx = 0;
};

//==============================================================================
// Multi-Model Guitar Amp Head Processor (8 Classic Modeled Amps + Zenbox Mint)
class ZenboxHeadProcessor
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        const double osSr = sr * 4.0;

        inPreHpf.setHighPass   (sr, 100.0, 0.7071);
        brightShelf.setHighShelf (sr, 2600.0, 0.0);
        preMidFocus.setPeaking (sr, 1250.0, 2.2, 0.8);
        preMudCut.setPeaking   (sr, 275.0, -2.4, 1.1);
        jcmBarkFilter.setPeaking (sr, 1750.0, 3.6, 0.9);
        rectoThump.setPeaking  (sr, 110.0,  2.2, 1.1);
        resonanceFilter.setPeaking (sr, 90.0, 0.0, 1.1);
        toneCutLpf.setLowPass  (sr, 18000.0, 0.7071);

        millerLpfOs.setLowPass (osSr, 6400.0, 0.7071);
        aaLpfOs1.setLowPass    (osSr, std::min (19000.0, sr * 0.42), 0.7071);
        aaLpfOs2.setLowPass    (osSr, std::min (19000.0, sr * 0.42), 0.7071);
        aaLpf1x.setLowPass     (sr,   std::min (18500.0, sr * 0.43), 0.7071);

        eqBass.setLowShelf     (sr, 95.0,   0.0);
        eqMid.setPeaking       (sr, 650.0,  0.0, 1.0);
        eqTreble.setHighShelf  (sr, 2500.0, 0.0);
        eqPresence.setHighShelf (sr, 3800.0, 0.0);

        springRev.prepare (sr);
        cab.prepare (sr);
        reset();
        lastCh = -1;
        lastModel = -1;
        lastBright = false;
        lastGain = lastBass = lastMid = lastTreble = lastPresence = lastRes = -1.0f;
    }

    void reset() noexcept
    {
        inPreHpf.reset(); brightShelf.reset(); preMidFocus.reset();
        preMudCut.reset(); jcmBarkFilter.reset(); rectoThump.reset();
        resonanceFilter.reset(); toneCutLpf.reset();
        millerLpfOs.reset(); aaLpfOs1.reset(); aaLpfOs2.reset(); aaLpf1x.reset();
        eqBass.reset(); eqMid.reset(); eqTreble.reset(); eqPresence.reset();
        springRev.reset();
        cab.reset();
        prevIn = 0.0f;
        prevJcmIn = 0.0f;
        sagEnv = 0.0f;
        tremPhase = 0.0f;
    }

    inline float process (float x, const GuitarControlSnapshot& snap) noexcept
    {
        const bool activeOut = snap.ampPower && ! snap.ampStandby && ! snap.tunerMuted;
        muteSmooth += 0.006f * ((activeOut ? 1.0f : 0.0f) - muteSmooth);

        const int model = std::clamp (snap.headModel, 0, 8);
        const int ch = std::clamp (snap.activeChannel, 0, 2);

        // Read unified controls mapped cleanly per model
        const float gVal = snap.chGain[static_cast<size_t> (ch)];
        const float bVal = (model == 5 && ch == 0) ? snap.cleanBass : snap.chBass[static_cast<size_t> (ch)];
        const float mVal = snap.chMid[static_cast<size_t> (ch)];
        const float tVal = (model == 5 && ch == 0) ? snap.cleanTreb : snap.chTreble[static_cast<size_t> (ch)];
        const float pVal = snap.chPresence[static_cast<size_t> (ch)];
        const float mast = (model == 5 && ch == 0) ? snap.vol2
                         : (model == 6 && ch == 0) ? snap.vol2
                         : snap.chMaster[static_cast<size_t> (ch)];

        if (model != lastModel || ch != lastCh || snap.ampModeBright != lastBright
            || std::fabs (gVal - lastGain) > 0.25f
            || std::fabs (bVal - lastBass) > 0.25f
            || std::fabs (mVal - lastMid)  > 0.25f
            || std::fabs (tVal - lastTreble) > 0.25f
            || std::fabs (pVal - lastPresence) > 0.25f
            || std::fabs (snap.resonance - lastRes) > 0.25f)
        {
            lastModel = model;
            lastCh = ch;
            lastBright = snap.ampModeBright;
            lastGain = gVal; lastBass = bVal; lastMid = mVal; lastTreble = tVal; lastPresence = pVal; lastRes = snap.resonance;

            const double gn = std::clamp (gVal * 0.01, 0.0, 1.0);
            double hpfHz = 95.0, midFocusHz = 1350.0, midFocusDb = 2.0, mudCutDb = -1.6;
            double bassHz = 95.0, midHz = 650.0, trebHz = 2450.0, presHz = 3850.0;
            double brightDb = snap.ampModeBright ? (4.4 * (1.0 - gn * 0.65)) : 0.0;
            double resDb = 0.0;
            double toneCutHz = 18000.0;

            switch (model)
            {
                case 0: // MI AMP 800 (JCM800 2203: tight british bite)
                    hpfHz = 112.0; midFocusHz = 1650.0; midFocusDb = 3.2; mudCutDb = -2.0;
                    bassHz = 100.0; midHz = 720.0; trebHz = 2500.0; presHz = 4100.0;
                    break;
                case 1: // MI AMP PLEXI (1959SLP: open warm vintage mid-punch)
                    hpfHz = 72.0; midFocusHz = 1150.0; midFocusDb = 1.8; mudCutDb = -0.8;
                    bassHz = 90.0; midHz = 620.0; trebHz = 2300.0; presHz = 3600.0;
                    brightDb = snap.ampModeBright ? (5.2 * (1.0 - gn * 0.60)) : 0.0;
                    break;
                case 2: // MI AMP RECTO (Dual Rectifier: massive low-end & modern scoop)
                    hpfHz = 82.0; midFocusHz = 1550.0; midFocusDb = 2.8; mudCutDb = -2.6;
                    bassHz = 85.0; midHz = 560.0; trebHz = 2650.0; presHz = 4300.0;
                    break;
                case 3: // MI AMP 6505 (HG-120: super-tight high-gain metal + resonance)
                    hpfHz = (ch == 0 && ! snap.crunchSw) ? 88.0 : 128.0;
                    midFocusHz = 1720.0; midFocusDb = 3.8; mudCutDb = -3.0;
                    bassHz = 95.0; midHz = 780.0; trebHz = 2700.0; presHz = 4400.0;
                    resDb = (snap.resonance * 0.01 - 0.5) * 14.0;
                    break;
                case 4: // MI AMP AC-30 (Class A Top Boost chime + Tone Cut)
                    hpfHz = 98.0; midFocusHz = 1950.0; midFocusDb = 2.6; mudCutDb = -1.2;
                    bassHz = 110.0; midHz = 900.0; trebHz = 2900.0; presHz = 4600.0;
                    toneCutHz = 11000.0 - (pVal * 0.01) * 7800.0; // Presence acts as Tone Cut on AC30
                    break;
                case 5: // MI AMP ROCKER (Thick British low-mid fuzz-edge crunch)
                    hpfHz = 78.0; midFocusHz = 980.0; midFocusDb = 2.8; mudCutDb = -0.6;
                    bassHz = 105.0; midHz = 540.0; trebHz = 2150.0; presHz = 3400.0;
                    break;
                case 6: // MI AMP BE-100 (Modded British Boutique clarity)
                    hpfHz = 108.0; midFocusHz = 1580.0; midFocusDb = 3.0; mudCutDb = -2.2;
                    bassHz = 92.0; midHz = 680.0; trebHz = 2550.0; presHz = 4000.0;
                    break;
                case 7: // MI AMP TWIN (Blackface sparkling clean headroom)
                    hpfHz = 62.0; midFocusHz = 1100.0; midFocusDb = 0.0; mudCutDb = -1.4;
                    bassHz = 82.0; midHz = 480.0; trebHz = 3100.0; presHz = 4800.0;
                    brightDb = (snap.ampModeBright || snap.bright2) ? 4.8 : 0.0;
                    break;
                default: // 8: MI AMP ZENBOX (Original v24 signature voicing)
                    hpfHz = (ch == 0) ? 78.0 : (ch == 1 ? 105.0 : 118.0);
                    midFocusHz = (ch == 2 ? 1550.0 : 1200.0);
                    midFocusDb = (ch == 0) ? 1.0 : (ch == 1 ? 2.8 : 3.4);
                    mudCutDb = (ch == 2 ? -2.8 : (ch == 1 ? -1.4 : -0.6));
                    midHz = (ch == 2 ? 720.0 : 640.0);
                    brightDb = snap.ampModeBright ? ((ch == 0 ? 5.6 : 4.4) * (1.0 - gn * 0.65)) : 0.0;
                    break;
            }

            inPreHpf.setHighPass (sr, hpfHz, 0.7071);
            preMidFocus.setPeaking (sr, midFocusHz, midFocusDb, 0.85);
            preMudCut.setPeaking (sr, 275.0, mudCutDb, 1.1);
            brightShelf.setHighShelf (sr, 2750.0, brightDb);

            eqBass.setLowShelf     (sr, bassHz, (bVal * 0.01 - 0.5) * 16.0);
            eqMid.setPeaking       (sr, midHz,  (mVal * 0.01 - 0.5) * 15.0, 0.95);
            eqTreble.setHighShelf  (sr, trebHz, (tVal * 0.01 - 0.5) * 16.0);
            eqPresence.setHighShelf (sr, presHz, (model == 4) ? 0.0 : (pVal * 0.01 - 0.5) * 14.0);
            resonanceFilter.setPeaking (sr, 90.0, resDb, 1.1);
            toneCutLpf.setLowPass  (sr, toneCutHz, 0.7071);
        }

        float s = preMudCut.processSample (preMidFocus.processSample (brightShelf.processSample (inPreHpf.processSample (x))));
        const float jcmDriveIn = ((model == 8 && ch == 2) || model == 0 || model == 6) ? jcmBarkFilter.processSample (s) : 0.0f;

        // Tube power-supply sag (stronger on Recto spongy / Plexi / AC30, tighter on 6505 / Twin)
        sagEnv += 0.0025f * (std::fabs (s) - sagEnv);
        const float sagAmt = (model == 7) ? 0.12f : (model == 3 ? 0.25f : (model == 1 || model == 4 ? 0.48f : 0.38f));
        const float sagScale = 1.0f / (1.0f + sagEnv * sagAmt);

        const float gn = std::clamp (gVal * 0.01f, 0.0f, 1.0f);
        float drive1 = 2.0f, drive2 = 2.0f;
        float symBias = -0.14f;

        switch (model)
        {
            case 0: // 800
                drive1 = 1.8f + gn * 11.0f; drive2 = 1.5f + gn * 4.4f; symBias = -0.15f; break;
            case 1: // PLEXI (Vol I + Vol II parallel jumpered feel!)
            {
                const float v2 = std::clamp (snap.vol2 * 0.01f, 0.0f, 1.0f);
                drive1 = 1.3f + (gn * 0.72f + v2 * 0.45f) * 8.5f;
                drive2 = 1.2f + (gn * 0.70f + v2 * 0.30f) * 3.2f;
                symBias = 0.06f;
                break;
            }
            case 2: // RECTO (ch0=Clean, ch1=Vintage, ch2=Modern)
                drive1 = (ch == 0) ? (1.1f + gn * 4.0f) : (ch == 1 ? (2.8f + gn * 14.0f) : (4.8f + gn * 24.0f));
                drive2 = (ch == 0) ? (1.0f + gn * 1.2f) : (ch == 1 ? (1.8f + gn * 5.6f)  : (2.9f + gn * 8.8f));
                symBias = (ch == 0) ? 0.04f : -0.22f;
                break;
            case 3: // 6505 (Rhythm + Crunch, or Lead)
            {
                const bool highChannel = (ch >= 1) || snap.crunchSw;
                drive1 = highChannel ? (4.6f + gn * 25.0f) : (1.25f + gn * 5.2f);
                drive2 = highChannel ? (2.6f + gn * 8.6f)  : (1.1f  + gn * 1.8f);
                symBias = highChannel ? -0.18f : 0.05f;
                break;
            }
            case 4: // AC-30 (Normal + Top Boost blend, Class A harmonic)
            {
                const float normV = std::clamp (snap.vol2 * 0.01f, 0.0f, 1.0f);
                drive1 = 1.25f + (gn * 0.75f + normV * 0.40f) * 7.8f;
                drive2 = 1.15f + (gn * 0.65f + normV * 0.25f) * 2.8f;
                symBias = 0.16f; // even-order Class A chime!
                break;
            }
            case 5: // ROCKER (Clean vs Dirty)
                drive1 = (ch == 0) ? (1.1f + gn * 3.2f) : (3.0f + gn * 16.0f);
                drive2 = (ch == 0) ? (1.0f + gn * 1.0f) : (1.9f + gn * 5.8f);
                symBias = (ch == 0) ? 0.04f : -0.16f;
                break;
            case 6: // BE-100 (Clean vs BE/HBE)
            {
                const float hbeBoost = (snap.voiceSw >= 2) ? 1.45f : (snap.voiceSw == 0 ? 0.72f : 1.0f);
                drive1 = (ch == 0) ? (1.15f + gn * 3.8f) : (2.8f + gn * 15.5f * hbeBoost);
                drive2 = (ch == 0) ? (1.0f  + gn * 1.2f) : (1.8f + gn * 5.5f * hbeBoost);
                symBias = (ch == 0) ? 0.05f : -0.14f;
                break;
            }
            case 7: // TWIN (Clean headroom; breaks up gently only above 75%)
                drive1 = 0.85f + gn * 2.6f;
                drive2 = 0.92f + gn * 0.95f;
                symBias = 0.03f;
                break;
            default: // 8: ZENBOX
                drive1 = (ch == 0) ? (1.15f + gn * 4.5f) : (ch == 1 ? (2.4f + gn * 11.5f) : (4.4f + gn * 22.0f));
                drive2 = (ch == 0) ? (1.0f  + gn * 1.4f) : (ch == 1 ? (1.65f + gn * 4.8f) : (2.7f + gn * 8.2f));
                symBias = (ch == 0) ? 0.06f : -0.18f;
                break;
        }

        auto shapeNonLinear = [this, drive1, drive2, symBias, model, ch, sagScale] (float in, float jcmIn) noexcept -> float
        {
            const float x1 = in * drive1 * sagScale;
            float s1 = std::tanh (x1 + 0.14f) - std::tanh (0.14f);
            s1 = millerLpfOs.processSample (s1);

            const float x2 = s1 * drive2;
            float s2 = std::tanh (x2 + symBias) - std::tanh (symBias);

            if ((model == 8 && ch == 2) || (model == 2 && ch == 2) || model == 3 || model == 6)
            {
                const float st3 = std::tanh (s2 * 1.80f + 0.10f) - std::tanh (0.10f);
                const float jcm = std::tanh (jcmIn * (2.4f + drive1 * 0.20f) + 0.08f) - std::tanh (0.08f);
                return st3 * 0.45f + jcm * 0.22f;
            }
            if (model == 0)
            {
                const float jcm = std::tanh (jcmIn * (2.2f + drive1 * 0.18f) + 0.06f) - std::tanh (0.06f);
                return s2 * 0.42f + jcm * 0.22f;
            }
            return s2 * ((model == 7 || ch == 0) ? 0.68f : 0.54f);
        };

        float preOut = 0.0f;
        if (snap.os4x)
        {
            for (int k = 1; k <= 4; ++k)
            {
                const float f = 0.25f * static_cast<float> (k);
                const float interp = prevIn + (s - prevIn) * f;
                const float interpJcm = prevJcmIn + (jcmDriveIn - prevJcmIn) * f;
                float y = shapeNonLinear (interp, interpJcm);
                y = aaLpfOs2.processSample (aaLpfOs1.processSample (y));
                preOut = y;
            }
            prevIn = s;
            prevJcmIn = jcmDriveIn;
        }
        else
        {
            preOut = aaLpf1x.processSample (shapeNonLinear (s, jcmDriveIn));
            prevIn = s;
            prevJcmIn = jcmDriveIn;
        }

        // Tone Stack + Power Amp Saturation + Resonance/Presence/ToneCut
        float ampSig = eqTreble.processSample (eqMid.processSample (eqBass.processSample (preOut)));
        if (model == 2 || (model == 8 && ch == 2))
            ampSig = rectoThump.processSample (ampSig);
        if (model == 3)
            ampSig = resonanceFilter.processSample (ampSig);

        const float halfScale = snap.halfPower ? 0.82f : 1.0f;
        const float mastGain = (0.45f + std::clamp (mast * 0.01f, 0.0f, 1.0f) * 1.55f) * halfScale;
        ampSig = std::tanh (ampSig * mastGain * 1.12f) * 0.88f;
        ampSig = eqPresence.processSample (ampSig);
        if (model == 4)
            ampSig = toneCutLpf.processSample (ampSig);

        // Built-in Optical Tremolo (AC-30 & Twin)
        if ((model == 4 || model == 7) && snap.tremDepth > 1.0f)
        {
            const float hz = 1.2f + (snap.tremSpeed * 0.01f) * 9.5f;
            tremPhase += hz / static_cast<float> (sr);
            if (tremPhase >= 1.0f) tremPhase -= 1.0f;
            const float lfo = 0.5f + 0.5f * std::sin (tremPhase * 6.2831853f);
            const float mod = 1.0f - (snap.tremDepth * 0.01f) * 0.72f * lfo;
            ampSig *= mod;
        }

        // Built-in Amp Spring Reverb (AC-30, Rocker, Twin)
        if ((model == 4 || model == 5 || model == 7) && snap.ampReverb > 1.0f)
        {
            ampSig = springRev.process (ampSig, true, 48.0f, 54.0f, snap.ampReverb * 0.65f, 30.0f);
        }

        // Attenuator trim (Rocker) or Output Knob
        const float attenTrim = (model == 5) ? (0.35f + (snap.attenuator * 0.01f) * 0.75f) : 1.0f;
        const float outTrim = std::pow (10.0f, ((snap.outputKnob * 0.01f - 0.5f) * 18.0f) / 20.0f) * attenTrim;

        if (snap.useExternalCabEngine)
        {
            return std::tanh (ampSig * outTrim * 0.90f) * 0.94f * muteSmooth;
        }

        // Legacy internal 4-cab convolver (if ever invoked standalone)
        const bool cabActive = (snap.cabIndex >= 0 && snap.cabIndex <= 3);
        cabSmooth += 0.008f * ((cabActive ? 1.0f : 0.0f) - cabSmooth);
        const float cabSig = cab.processSample (ampSig, cabActive ? snap.cabIndex : 1);
        const float mixed  = cabSig * cabSmooth + ampSig * 0.55f * (1.0f - cabSmooth);
        return std::tanh (mixed * outTrim * 0.95f) * 0.94f * muteSmooth;
    }

private:
    double sr = 48000.0;
    xvdsp::Biquad inPreHpf, brightShelf, preMidFocus, preMudCut, jcmBarkFilter, rectoThump, resonanceFilter, toneCutLpf;
    xvdsp::Biquad millerLpfOs, aaLpfOs1, aaLpfOs2, aaLpf1x;
    xvdsp::Biquad eqBass, eqMid, eqTreble, eqPresence;
    GuitarReverbFX springRev;
    ZenboxCabConvolver cab;

    float prevIn = 0.0f, prevJcmIn = 0.0f, sagEnv = 0.0f, tremPhase = 0.0f;
    float muteSmooth = 1.0f, cabSmooth = 1.0f;
    int   lastCh = -1, lastModel = -1;
    bool  lastBright = false;
    float lastGain = -1.0f, lastBass = -1.0f, lastMid = -1.0f, lastTreble = -1.0f, lastPresence = -1.0f, lastRes = -1.0f;
};

//==============================================================================
class MiAmpGuitarChannel
{
public:
    void prepare (double sampleRate, int channelIndex)
    {
        chIdx = channelIndex;
        pitcher.prepare (sampleRate);
        gate.prepare (sampleRate);
        comp.prepare (sampleRate);
        drive.prepare (sampleRate);
        fuzz.prepare (sampleRate);
        chorus.prepare (sampleRate);
        phaser.prepare (sampleRate);
        delay.prepare (sampleRate);
        reverb.prepare (sampleRate);
        dist.prepare (sampleRate, false);
        dualCrunch.prepare (sampleRate);
        flanger.prepare (sampleRate);
        gatePedal.prepare (sampleRate);
        octaver.prepare (sampleRate);
        eq7.prepare (sampleRate, false);
        head.prepare (sampleRate);
    }

    void reset() noexcept
    {
        pitcher.reset(); gate.reset();
        comp.reset(); drive.reset(); fuzz.reset();
        chorus.reset(); phaser.reset(); delay.reset(); reverb.reset();
        dist.reset(); dualCrunch.reset(); flanger.reset();
        gatePedal.reset(); octaver.reset(); eq7.reset();
        head.reset();
        headBypassSmooth = 0.0f;
        directMuteSmooth = 1.0f;
    }

    inline float processSample (float x, const GuitarControlSnapshot& snap) noexcept
    {
        const float inTrim = std::pow (10.0f, ((snap.inputGain * 0.01f - 0.5f) * 24.0f) / 20.0f);
        float s = x * inTrim;

        s = gate.process (s, snap.gateThresh);

        pitcher.setParameters (snap.pitchEnabled, snap.pitchSemitones, snap.pitchCents, snap.pitchMix);
        s = pitcher.processSample (s);

        std::array<bool, 13> seen {};
        for (int slot : snap.chainSlots)
        {
            if (slot < 0 || slot >= 13 || seen[static_cast<size_t> (slot)]) continue;
            seen[static_cast<size_t> (slot)] = true;
            switch (static_cast<GuitarPedalType> (slot))
            {
                case GuitarPedalType::Compresor:
                    s = comp.process (s, snap.compOn, snap.compSustain, snap.compAttack, snap.compLevel, snap.compTone);
                    break;
                case GuitarPedalType::Drive:
                    s = drive.process (s, snap.driveOn, snap.driveGain, snap.driveTone, snap.driveLevel, snap.driveClip);
                    break;
                case GuitarPedalType::Fuzz:
                    s = fuzz.process (s, snap.fuzzOn, snap.fuzzFuzz, snap.fuzzTone, snap.fuzzLevel);
                    break;
                case GuitarPedalType::Chorus:
                    s = chorus.process (s, snap.chorusOn, snap.chorusRate, snap.chorusDepth, snap.chorusMix, chIdx, snap.chorusEq, snap.chorusMode);
                    break;
                case GuitarPedalType::Phaser:
                    s = phaser.process (s, snap.phaserOn, snap.phaserRate, snap.phaserDepth, snap.phaserFeedback);
                    break;
                case GuitarPedalType::Delay:
                    s = delay.process (s, snap.delayOn, snap.delayTime, snap.delayFeedback, snap.delayMix, snap.delayMode, snap.delaySubdiv);
                    break;
                case GuitarPedalType::Reverb:
                    s = reverb.process (s, snap.reverbOn, snap.reverbDecay, snap.reverbTone, snap.reverbMix, snap.reverbMode);
                    break;
                case GuitarPedalType::Distortion:
                    s = dist.process (s, snap.distOn, snap.distTone, snap.distLevel, snap.distDrive);
                    break;
                case GuitarPedalType::DualCrunch:
                    s = dualCrunch.process (s, snap.dualOn, snap.dualOutput, snap.dualGain, snap.dualBlend, snap.dualTone, snap.dualDrive, snap.dualScoop);
                    break;
                case GuitarPedalType::Flanger:
                    s = flanger.process (s, snap.flangeOn, snap.flangeManual, snap.flangeRes, snap.flangeDepth, snap.flangeRate, snap.flangeMode);
                    break;
                case GuitarPedalType::NoiseGate:
                    s = gatePedal.process (s, snap.gatePedalOn, snap.gatePedalThresh, snap.gatePedalDecay, snap.gatePedalMode);
                    break;
                case GuitarPedalType::Octaver:
                    s = octaver.process (s, snap.octOn, snap.octDirect, snap.octUp, snap.octSub1, snap.octSub2);
                    break;
                case GuitarPedalType::GraphicEq:
                    s = eq7.process (s, snap.eqOn, snap.eqBands);
                    break;
                default:
                    break;
            }
        }

        const float ampOut = head.process (s, snap);
        directMuteSmooth += 0.006f * ((snap.tunerMuted ? 0.0f : 1.0f) - directMuteSmooth);
        headBypassSmooth += 0.008f * ((snap.headBypass ? 1.0f : 0.0f) - headBypassSmooth);
        const float directOut = s * directMuteSmooth;
        return ampOut * (1.0f - headBypassSmooth) + directOut * headBypassSmooth;
    }

private:
    float headBypassSmooth = 0.0f;
    float directMuteSmooth = 1.0f;
    int chIdx = 0;
    xvdsp::XVBassPitchShifter pitcher;
    GuitarNoiseGate gate;
    GuitarCompFX comp;
    GuitarDriveFX drive;
    GuitarFuzzFX fuzz;
    GuitarChorusFX chorus;
    GuitarPhaserFX phaser;
    GuitarDelayFX delay;
    GuitarReverbFX reverb;
    mipedals::BoutiqueDistortionFX dist;
    mipedals::BoutiqueDualCrunchFX dualCrunch;
    mipedals::BoutiqueFlangerFX flanger;
    mipedals::BoutiqueNoiseGateFX gatePedal;
    mipedals::BoutiqueOctaverFX octaver;
    mipedals::Boutique7BandEQFX eq7;
    ZenboxHeadProcessor head;
};

class MiAmpGuitarChainDSP
{
public:
    void prepare (double sampleRate, int /*samplesPerBlock*/, int numChannels)
    {
        const int chCount = std::clamp (numChannels, 1, 2);
        channels.resize (static_cast<size_t> (chCount));
        for (int i = 0; i < chCount; ++i)
            channels[static_cast<size_t> (i)].prepare (sampleRate, i);
    }

    void reset() noexcept
    {
        for (auto& ch : channels)
            ch.reset();
    }

    template <typename AudioBufferType>
    void process (AudioBufferType& buffer, const GuitarControlSnapshot& snap) noexcept
    {
        const int numCh = std::min (static_cast<int> (channels.size()), buffer.getNumChannels());
        const int numSamples = buffer.getNumSamples();
        if (numCh <= 0 || numSamples <= 0)
            return;

        for (int ch = 0; ch < numCh; ++ch)
        {
            float* data = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = channels[static_cast<size_t> (ch)].processSample (data[i], snap);
        }
    }

private:
    std::vector<MiAmpGuitarChannel> channels;
};

} // namespace zenbox
