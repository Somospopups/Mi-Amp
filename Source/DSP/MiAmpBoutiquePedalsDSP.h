#pragma once
#include <array>
#include <vector>
#include <cmath>
#include <algorithm>
#include "XVBassDSP.h"

namespace mipedals
{

using BiquadFilter = xvdsp::Biquad;

inline float softTanh (float x) noexcept
{
    return std::tanh (std::clamp (x, -10.0f, 10.0f));
}

//==============================================================================
// 1. Distortion / Bass Overdrive (MI ORANGE & MI GOLD)
//==============================================================================
class BoutiqueDistortionFX
{
public:
    void prepare (double sampleRate, bool bassMode)
    {
        sr = std::max (22050.0, sampleRate);
        isBass = bassMode;
        preHp.reset();
        toneLp.reset();
        toneHp.reset();
        bassTilt.reset();
        preHp.setHighPass (sr, bassMode ? 45.0 : 180.0, 0.7071);
        bypassSmooth = 0.0f;
        lastTone = -999.0f;
    }

    void reset() noexcept
    {
        preHp.reset();
        toneLp.reset();
        toneHp.reset();
        bassTilt.reset();
    }

    // Guitar MI ORANGE: p0=Tone, p1=Level, p2=Dist
    // Bass   MI GOLD:   p0=Level, p1=EQ, p2=Blend, p3=Gain
    inline float process (float x, bool enabled, float p0, float p1, float p2, float p3 = 50.0f) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        float wet = x;
        if (! isBass)
        {
            const float tone  = std::clamp (p0 * 0.01f, 0.0f, 1.0f);
            const float level = std::clamp (p1 * 0.01f, 0.0f, 1.0f);
            const float dist  = std::clamp (p2 * 0.01f, 0.0f, 1.0f);

            if (std::fabs (tone - lastTone) > 0.25f)
            {
                lastTone = tone;
                toneLp.setLowPass (sr, 700.0 + tone * 3400.0, 0.7071);
                toneHp.setHighShelf (sr, 2600.0, -4.0 + tone * 10.0, 0.7071);
            }

            const float pre = preHp.processSample (x) * (1.8f + dist * 24.0f);
            const float pos = std::clamp (pre + 0.08f, -1.0f, 1.0f);
            const float neg = std::clamp (pre - 0.04f, -0.82f, 0.82f);
            const float clipped = softTanh ((pre >= 0.0f ? pos : neg) * 1.45f) * 0.65f;
            wet = toneHp.processSample (toneLp.processSample (clipped)) * (0.25f + level * 1.45f);
        }
        else
        {
            const float level = std::clamp (p0 * 0.01f, 0.0f, 1.0f);
            const float eq    = std::clamp (p1 * 0.01f, 0.0f, 1.0f);
            const float blend = std::clamp (p2 * 0.01f, 0.0f, 1.0f);
            const float gain  = std::clamp (p3 * 0.01f, 0.0f, 1.0f);

            if (std::fabs (eq - lastTone) > 0.25f)
            {
                lastTone = eq;
                toneLp.setLowShelf (sr, 120.0, (0.5 - eq) * 8.0 + 2.0, 0.7071);
                toneHp.setHighShelf (sr, 2200.0, (eq - 0.5) * 11.0, 0.7071);
            }

            const float dirtIn = preHp.processSample (x) * (1.4f + gain * 16.0f);
            const float dirt   = softTanh (dirtIn + 0.06f) * 0.72f;
            const float mixed  = x * (1.0f - blend) + dirt * blend;
            wet = toneHp.processSample (toneLp.processSample (mixed)) * (0.35f + level * 1.4f);
        }

        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    bool isBass = false;
    BiquadFilter preHp, toneLp, toneHp, bassTilt;
    float bypassSmooth = 0.0f, lastTone = -999.0f;
};

//==============================================================================
// 2. Dual Crunch Guitar Preamp (MI DUAL CRUNCH)
//==============================================================================
class BoutiqueDualCrunchFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        preHp.reset();
        toneLp.reset();
        scoopPeak.reset();
        preHp.setHighPass (sr, 95.0, 0.7071);
        lastTone = -999.0f;
        lastScoop = false;
        bypassSmooth = 0.0f;
    }

    void reset() noexcept { preHp.reset(); toneLp.reset(); scoopPeak.reset(); }

    inline float process (float x, bool enabled, float output, float gain, float blend,
                          float tone, float drive, bool scoop) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        if (std::fabs (tone - lastTone) > 0.25f || scoop != lastScoop)
        {
            lastTone  = tone;
            lastScoop = scoop;
            const double tn = std::clamp (tone * 0.01, 0.0, 1.0);
            toneLp.setLowPass (sr, 1200.0 + tn * 4600.0, 0.72);
            scoopPeak.setPeaking (sr, 760.0, scoop ? -6.5 : 1.5, 1.15);
        }

        const float sIn = preHp.processSample (x);
        const float chA = softTanh (sIn * (1.5f + std::clamp (gain * 0.01f, 0.0f, 1.0f) * 10.0f));
        const float dNorm = std::clamp (drive * 0.01f, 0.0f, 1.0f);
        const float chB1 = softTanh (sIn * (2.2f + dNorm * 14.0f) + 0.08f);
        const float chB2 = softTanh (chB1 * 2.2f - 0.05f);

        const float bl = std::clamp (blend * 0.01f, 0.0f, 1.0f);
        const float mixed = chA * (1.0f - bl) + chB2 * bl;
        const float shaped = scoopPeak.processSample (toneLp.processSample (mixed));
        const float outGain = 0.25f + std::clamp (output * 0.01f, 0.0f, 1.0f) * 1.45f;
        const float wet = shaped * outGain * 0.75f;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    BiquadFilter preHp, toneLp, scoopPeak;
    float lastTone = -999.0f, bypassSmooth = 0.0f;
    bool lastScoop = false;
};

//==============================================================================
// 3. Clean Headroom Bass Overdrive (BASS FOOD)
//==============================================================================
class BoutiqueBassFoodFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        trebleShelf.reset();
        midPush.reset();
        midPush.setPeaking (sr, 680.0, 2.5, 0.9);
        lastTreble = -999.0f;
        bypassSmooth = 0.0f;
    }

    void reset() noexcept { trebleShelf.reset(); midPush.reset(); }

    inline float process (float x, bool enabled, float vol, float treble, float drive,
                          float blend, bool pad10db) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        if (std::fabs (treble - lastTreble) > 0.25f)
        {
            lastTreble = treble;
            const double tr = std::clamp (treble * 0.01, 0.0, 1.0);
            trebleShelf.setHighShelf (sr, 1850.0, (tr - 0.5) * 13.0, 0.7071);
        }

        const float padMul = pad10db ? 0.3162f : 1.0f;
        const float drv = std::clamp (drive * 0.01f, 0.0f, 1.0f);
        const float bl  = std::clamp (blend * 0.01f, 0.0f, 1.0f);
        const float v   = std::clamp (vol * 0.01f, 0.0f, 1.0f);

        const float inScaled = x * padMul;
        const float driven = softTanh (midPush.processSample (inScaled) * (1.2f + drv * 8.5f)) * 0.78f;
        const float mixed = inScaled * (1.0f - bl * 0.85f) + driven * (0.25f + bl * 0.85f);
        const float wet = trebleShelf.processSample (mixed) * (0.35f + v * 1.45f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    BiquadFilter trebleShelf, midPush;
    float lastTreble = -999.0f, bypassSmooth = 0.0f;
};

//==============================================================================
// 4. Analog Tube Preamp DI (MI BASS DRIVER)
//==============================================================================
class BoutiqueBassDriverDIFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        bassShelf.reset();
        trebleShelf.reset();
        presPeak.reset();
        cabLp.reset();
        cabLp.setLowPass (sr, 4800.0, 0.75);
        lastBass = lastTreble = lastPres = -999.0f;
        bypassSmooth = 0.0f;
    }

    void reset() noexcept { bassShelf.reset(); trebleShelf.reset(); presPeak.reset(); cabLp.reset(); }

    inline float process (float x, bool enabled, float level, float blend, float treble,
                          float presence, float drive, float bass,
                          bool xlrPad, bool inBoost, bool outBoost) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        if (std::fabs (bass - lastBass) > 0.25f || std::fabs (treble - lastTreble) > 0.25f || std::fabs (presence - lastPres) > 0.25f)
        {
            lastBass = bass; lastTreble = treble; lastPres = presence;
            const double bDb = (std::clamp (bass * 0.01, 0.0, 1.0) - 0.5) * 24.0;
            const double tDb = (std::clamp (treble * 0.01, 0.0, 1.0) - 0.5) * 24.0;
            const double pDb = (std::clamp (presence * 0.01, 0.0, 1.0) - 0.5) * 18.0;
            bassShelf.setLowShelf (sr, 95.0, bDb, 0.7071);
            trebleShelf.setHighShelf (sr, 3000.0, tDb, 0.7071);
            presPeak.setPeaking (sr, 1850.0, pDb, 0.9);
        }

        const float inMul  = inBoost ? 1.85f : 1.0f;
        const float drv    = std::clamp (drive * 0.01f, 0.0f, 1.0f);
        const float bl     = std::clamp (blend * 0.01f, 0.0f, 1.0f);
        const float lvl    = std::clamp (level * 0.01f, 0.0f, 1.0f);
        const float xIn    = x * inMul;

        const float s1 = softTanh (xIn * (1.4f + drv * 11.5f) + 0.06f);
        const float s2 = cabLp.processSample (presPeak.processSample (s1));
        const float mixed = xIn * (1.0f - bl) + s2 * bl;
        const float eqd   = trebleShelf.processSample (bassShelf.processSample (mixed));

        const float outMul = (0.35f + lvl * 1.45f) * (outBoost ? 1.45f : 1.0f) * (xlrPad ? 0.50f : 1.0f);
        const float wet = softTanh (eqd * outMul * 0.85f);
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    BiquadFilter bassShelf, trebleShelf, presPeak, cabLp;
    float lastBass = -999.0f, lastTreble = -999.0f, lastPres = -999.0f;
    float bypassSmooth = 0.0f;
};

//==============================================================================
// 5. Analog Flanger (MI VIOLET)
//==============================================================================
class BoutiqueFlangerFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        const size_t cap = static_cast<size_t> (0.04 * sr) + 16;
        buf.assign (cap, 0.0f);
        wr = 0;
        phase = 0.0f;
        fbState = 0.0f;
        bypassSmooth = 0.0f;
    }

    void reset() noexcept
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        wr = 0; phase = 0.0f; fbState = 0.0f;
    }

    inline float process (float x, bool enabled, float manual, float res, float depth, float rate, int mode) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);
        if (buf.empty()) return x;

        const float rNorm = std::clamp (rate * 0.01f, 0.0f, 1.0f);
        const float dNorm = std::clamp (depth * 0.01f, 0.0f, 1.0f);
        const float mNorm = std::clamp (manual * 0.01f, 0.0f, 1.0f);
        const float fNorm = std::clamp (res * 0.01f, 0.0f, 1.0f);

        const float hz = (mode == 2) ? (0.06f + rNorm * 1.4f) : (0.12f + rNorm * 4.5f);
        phase += hz / static_cast<float> (sr);
        if (phase >= 1.0f) phase -= 1.0f;

        const float tri = 1.0f - 4.0f * std::fabs (phase - 0.5f);
        const float centerMs = 0.8f + mNorm * 5.2f;
        const float sweepMs  = dNorm * (mode == 1 ? 4.8f : 3.2f);
        const float delMs    = std::clamp (centerMs + tri * sweepMs, 0.35f, 18.0f);
        const float delSamp  = delMs * 0.001f * static_cast<float> (sr);

        const float fbAmt = (mode == 1 ? 0.82f : 0.68f) * (fNorm * 2.0f - 1.0f) * 0.72f;
        buf[wr] = x + std::clamp (fbState * fbAmt, -0.95f, 0.95f);

        const float n = static_cast<float> (buf.size());
        float rp = static_cast<float> (wr) - delSamp;
        while (rp < 0.0f) rp += n;
        const size_t i0 = static_cast<size_t> (rp) % buf.size();
        const size_t i1 = (i0 + 1) % buf.size();
        const float frac = rp - std::floor (rp);
        const float del = buf[i0] * (1.0f - frac) + buf[i1] * frac;
        fbState = del;

        wr = (wr + 1) % buf.size();
        const float wet = 0.58f * x + 0.52f * del;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    std::vector<float> buf;
    size_t wr = 0;
    float phase = 0.0f, fbState = 0.0f, bypassSmooth = 0.0f;
};

//==============================================================================
// 6. Noise Gate Pedal (MI SILENT)
//==============================================================================
class BoutiqueNoiseGateFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        attEnv = std::exp (-1.0f / static_cast<float> (0.001 * sr));
        gAtt   = std::exp (-1.0f / static_cast<float> (0.0015 * sr));
        env = 0.0f;
        gateGain = 1.0f;
        holdSamp = 0;
        isOpen = true;
    }

    void reset() noexcept { env = 0.0f; gateGain = 1.0f; holdSamp = 0; isOpen = true; }

    inline float process (float x, bool enabled, float thresh, float decay, float mode) noexcept
    {
        if (! enabled)
        {
            gateGain += 0.02f * (1.0f - gateGain);
            return x * gateGain;
        }

        const float dNorm = std::clamp (decay * 0.01f, 0.0f, 1.0f);
        const float tNorm = std::clamp (thresh * 0.01f, 0.0f, 1.0f);
        const int mIdx = std::clamp (static_cast<int> (std::round (mode * 0.02f)), 0, 2);

        const float relTime = 0.025f + dNorm * 0.25f;
        const float relCoeff = std::exp (-1.0f / static_cast<float> (relTime * sr));

        const float ax = std::fabs (x);
        env = (ax > env) ? (attEnv * env + (1.0f - attEnv) * ax)
                         : (relCoeff * env + (1.0f - relCoeff) * ax);

        const float openDb  = -75.0f + tNorm * 52.0f;
        const float openThr = std::pow (10.0f, openDb / 20.0f);
        const float closeThr = openThr * 0.62f;

        if (env >= openThr)
        {
            isOpen = true;
            holdSamp = static_cast<int> ((0.02f + dNorm * 0.08f) * sr);
        }
        else if (env < closeThr)
        {
            if (holdSamp > 0) --holdSamp;
            else isOpen = false;
        }

        const float floorGain = (mIdx == 0) ? 0.08f : (mIdx == 1 ? 0.01f : 0.0f);
        const float target = isOpen ? 1.0f : floorGain;
        gateGain = (target > gateGain) ? (gAtt * gateGain + (1.0f - gAtt) * target)
                                       : (relCoeff * gateGain + (1.0f - relCoeff) * target);
        return x * gateGain;
    }

private:
    double sr = 48000.0;
    float attEnv = 0.98f, gAtt = 0.98f, env = 0.0f, gateGain = 1.0f;
    int holdSamp = 0;
    bool isOpen = true;
};

//==============================================================================
// 7. Poly/Sub Octaver (MI SUB OCTAVE & MI POLY OCTAVE)
//==============================================================================
class BoutiqueOctaverFX
{
public:
    void prepare (double sampleRate)
    {
        sr = std::max (22050.0, sampleRate);
        inLp.reset();
        sub1Lp.reset();
        sub2Lp.reset();
        upBp.reset();
        inLp.setLowPass (sr, 360.0, 0.7071);
        sub1Lp.setLowPass (sr, 260.0, 0.7071);
        sub2Lp.setLowPass (sr, 150.0, 0.7071);
        upBp.setBandPass (sr, 950.0, 0.8);
        reset();
    }

    void reset() noexcept
    {
        inLp.reset(); sub1Lp.reset(); sub2Lp.reset(); upBp.reset();
        prevFilt = 0.0f;
        ff1 = 1.0f; ff2 = 1.0f;
        env = 0.0f;
        bypassSmooth = 0.0f;
    }

    inline float process (float x, bool enabled, float direct, float octUp, float sub1, float sub2 = 0.0f) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        const float ax = std::fabs (x);
        env = (ax > env) ? (0.985f * env + 0.015f * ax) : (0.9992f * env + 0.0008f * ax);

        const float f = inLp.processSample (x);
        if (prevFilt <= 0.0f && f > 0.0f)
        {
            ff1 = -ff1;
            if (ff1 > 0.0f)
                ff2 = -ff2;
        }
        prevFilt = f;

        const float s1 = sub1Lp.processSample (ff1 * env * 1.45f);
        const float s2 = sub2Lp.processSample (ff2 * env * 1.45f);
        const float up = upBp.processSample (std::fabs (x) - env * 0.45f) * 1.65f;

        const float dMix  = std::clamp (direct * 0.01f, 0.0f, 1.0f) * 1.15f;
        const float uMix  = std::clamp (octUp  * 0.01f, 0.0f, 1.0f) * 0.85f;
        const float s1Mix = std::clamp (sub1   * 0.01f, 0.0f, 1.0f) * 0.95f;
        const float s2Mix = std::clamp (sub2   * 0.01f, 0.0f, 1.0f) * 0.85f;

        const float wet = x * dMix + up * uMix + s1 * s1Mix + s2 * s2Mix;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    BiquadFilter inLp, sub1Lp, sub2Lp, upBp;
    float prevFilt = 0.0f, ff1 = 1.0f, ff2 = 1.0f, env = 0.0f, bypassSmooth = 0.0f;
};

//==============================================================================
// 8. 7-Band Graphic EQ (MI GRAPHIC EQ & MI BASS EQ)
//==============================================================================
class Boutique7BandEQFX
{
public:
    void prepare (double sampleRate, bool bassMode)
    {
        sr = std::max (22050.0, sampleRate);
        isBass = bassMode;
        static constexpr double gFreqs[6] = { 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0 };
        static constexpr double bFreqs[6] = {  50.0, 120.0, 400.0, 500.0,  800.0, 4500.0 };
        for (int i = 0; i < 6; ++i)
        {
            freqs[i] = bassMode ? bFreqs[i] : gFreqs[i];
            bands[i].reset();
            lastVals[i] = -999.0f;
        }
        bypassSmooth = 0.0f;
    }

    void reset() noexcept
    {
        for (auto& b : bands) b.reset();
    }

    inline float process (float x, bool enabled, const std::array<float, 7>& sliders) noexcept
    {
        const float targetBypass = enabled ? 0.0f : 1.0f;
        bypassSmooth += 0.008f * (targetBypass - bypassSmooth);

        float s = x;
        for (int i = 0; i < 6; ++i)
        {
            if (std::fabs (sliders[i] - lastVals[i]) > 0.2f)
            {
                lastVals[i] = sliders[i];
                const double db = (std::clamp (sliders[i] * 0.01, 0.0, 1.0) - 0.5) * 24.0;
                if (i == 0)      bands[i].setLowShelf  (sr, freqs[i], db, 0.75);
                else if (i == 5) bands[i].setHighShelf (sr, freqs[i], db, 0.75);
                else             bands[i].setPeaking   (sr, freqs[i], db, 1.35);
            }
            s = bands[i].processSample (s);
        }

        const float lvlDb = (std::clamp (sliders[6] * 0.01f, 0.0f, 1.0f) - 0.5f) * 24.0f;
        const float lvlGain = std::pow (10.0f, lvlDb / 20.0f);
        const float wet = s * lvlGain;
        return wet * (1.0f - bypassSmooth) + x * bypassSmooth;
    }

private:
    double sr = 48000.0;
    bool isBass = false;
    std::array<double, 6> freqs { 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0 };
    std::array<BiquadFilter, 6> bands;
    std::array<float, 6> lastVals { -999.0f, -999.0f, -999.0f, -999.0f, -999.0f, -999.0f };
    float bypassSmooth = 0.0f;
};

} // namespace mipedals
