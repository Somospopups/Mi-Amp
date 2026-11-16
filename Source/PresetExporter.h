#pragma once
#include "PluginProcessor.h"
#include "DSP/ZenboxCabIRs.h"
#include "DSP/XVBassDSP.h"
#include "DSP/MiAmpBoutiquePedalsDSP.h"
#include "DSP/MiAmpGuitarChainDSP.h"
#include "DSP/MiAmpBassChainDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <random>
#include <sstream>
#include <vector>

namespace mipedals
{

enum class ExportFormat
{
    NamA2        = 1,   // NAM A2 SlimmableContainer (Lite 3ch + Full 8ch) (.nam)
    NamA1        = 2,   // NAM A1 WaveNet (.nam)
    NamLstm      = 3,   // NAM LSTM (.nam)
    AidaX        = 4,   // AIDA-X / MOD Dwarf / Genome (.aidax)
    ProteusJson  = 5,   // GuitarML Proteus (.json)
    NeuralPiJson = 6,   // GuitarML NeuralPi (.json)
    IrWav24_44k  = 7,   // M-Vave Blackbox & universal 44.1kHz / 24-bit IR (.wav)
    BlackboxTxt  = 8,   // M-Vave Blackbox module mapping guide (.txt)
    HelixHlx     = 9,   // Line 6 Helix / HX Stomp / POD Go (.hlx JSON)
    HeadrushRig  = 10,  // Headrush (.rig JSON)
    MiAmpXml     = 11   // Portable full MI AMP preset (.miamp)
};

class PresetExporter
{
public:
    static juce::String sanitizeFileName (const juce::String& raw)
    {
        juce::String out;
        const std::string s (raw.toRawUTF8());
        for (unsigned char c : s)
        {
            if (std::isalnum (c) || c == ' ' || c == '-' || c == '_')
            {
                const char ch[2] = { static_cast<char> (c), '\0' };
                out += juce::String (ch);
            }
            else if (c == '.')
            {
                out += "_";
            }
        }
        out = out.trim();
        return out.isEmpty() ? juce::String ("MI_AMP_Preset") : out;
    }

    static bool writeTextFile (const juce::File& f, const juce::String& content)
    {
        const std::string utf8 (content.toRawUTF8());
        std::ofstream os (f.getFullPathName().toRawUTF8(), std::ios::binary | std::ios::trunc);
        if (! os)
            return false;
        os.write (utf8.data(), static_cast<std::streamsize> (utf8.size()));
        return os.good();
    }

    // Build snapshots from a factory/user preset without altering live audio
    static miamp::BassControlSnapshot snapshotFromBassPreset (const MiAmpAudioProcessor& proc, const MiAmpBassPreset& p)
    {
        auto s = proc.getBassSnapshot();
        s.headBypass = p.headBypass;
        s.ampGain    = p.ampGain;
        s.ampBass    = p.ampBass;
        s.ampMid     = p.ampMid;
        s.ampFreq    = p.ampFreq;
        s.ampTreble  = p.ampTreble;
        s.ampUltraHi = p.ampUltraHi;
        s.chainSlots = p.slots;
        s.compOn      = p.compOn;
        s.compSustain = p.compSus;
        s.compAttack  = p.compAtt;
        s.compLevel   = p.compLvl;
        s.xvOn    = p.xvOn;
        s.xvLevel = p.xvLvl;
        s.xvDrive = p.xvDrv;
        s.xvLow   = p.xvLow;
        s.xvHigh  = p.xvHigh;
        s.xvBlend = p.xvBlend;
        s.xvMid   = p.xvMid;
        s.xvPresence = p.xvPres;
        s.xvCab   = p.xvCab;
        s.chorusOn    = p.chorusOn;
        s.chorusRate  = p.chRate;
        s.chorusDepth = p.chDepth;
        s.chorusMix   = p.chMix;
        s.phaserOn       = p.phaserOn;
        s.phaserRate     = p.phRate;
        s.phaserDepth    = p.phDepth;
        s.phaserFeedback = p.phFb;
        s.reverbOn    = p.reverbOn;
        s.reverbDecay = p.rvDec;
        s.reverbTone  = p.rvTone;
        s.reverbMix   = p.rvMix;
        auto hasSlot = [&](int id) { return std::find (p.slots.begin(), p.slots.end(), id) != p.slots.end(); };
        s.bddiOn  = hasSlot (5);
        s.goldOn  = hasSlot (6);
        s.foodOn  = hasSlot (7);
        s.oliveOn = hasSlot (8);
        s.flangeOn = hasSlot (9);
        s.delayOn  = hasSlot (10);
        s.gateOn   = hasSlot (11);
        s.octOn    = hasSlot (12);
        s.eqOn     = hasSlot (13);
        return s;
    }

    static zenbox::GuitarControlSnapshot snapshotFromGuitarPreset (const MiAmpAudioProcessor& proc, const MiAmpGuitarPreset& p)
    {
        auto s = proc.getGuitarSnapshot();
        s.headBypass    = p.headBypass;
        s.activeChannel = p.channel;
        s.cabIndex      = p.cab;
        s.ampModeBright = p.bright;
        const size_t c  = static_cast<size_t> (std::clamp (p.channel, 0, 2));
        s.chGain[c]     = p.gain;
        s.chBass[c]     = p.bass;
        s.chMid[c]      = p.mid;
        s.chTreble[c]   = p.treble;
        s.chPresence[c] = p.presence;
        s.chMaster[c]   = p.master;
        s.outputKnob    = p.output;
        s.chainSlots    = p.slots;
        s.compOn      = p.compOn;
        s.compSustain = p.compSus;
        s.compAttack  = p.compAtt;
        s.compLevel   = p.compLvl;
        s.driveOn    = p.driveOn;
        s.driveGain  = p.driveGain;
        s.driveTone  = p.driveTone;
        s.driveLevel = p.driveLvl;
        s.fuzzOn    = p.fuzzOn;
        s.fuzzFuzz  = p.fuzzFuzz;
        s.fuzzTone  = p.fuzzTone;
        s.fuzzLevel = p.fuzzLvl;
        s.chorusOn    = p.chorusOn;
        s.chorusRate  = p.chRate;
        s.chorusDepth = p.chDepth;
        s.chorusMix   = p.chMix;
        s.phaserOn       = p.phaserOn;
        s.phaserRate     = p.phRate;
        s.phaserDepth    = p.phDepth;
        s.phaserFeedback = p.phFb;
        s.delayOn       = p.delayOn;
        s.delayTime     = p.dlTime;
        s.delayFeedback = p.dlFb;
        s.delayMix      = p.dlMix;
        s.reverbOn    = p.reverbOn;
        s.reverbDecay = p.rvDec;
        s.reverbTone  = p.rvTone;
        s.reverbMix   = p.rvMix;
        auto hasSlot = [&](int id) { return std::find (p.slots.begin(), p.slots.end(), id) != p.slots.end(); };
        s.distOn      = hasSlot (7);
        s.dualOn      = hasSlot (8);
        s.flangeOn    = hasSlot (9);
        s.gatePedalOn = hasSlot (10);
        s.octOn       = hasSlot (11);
        s.eqOn        = hasSlot (12);
        return s;
    }

    // -------------------------------------------------------------------------
    // Offline DSP Capture Helper (48 kHz) — captures real MI AMP chain behavior
    // -------------------------------------------------------------------------
    struct CaptureDataset
    {
        std::vector<float> x;
        std::vector<float> y;
        float inputPeak  = 0.0f;
        float targetRms  = 0.0f;
        float targetPeak = 0.0f;
        float loudnessDb = -20.0f;
    };

    static CaptureDataset captureChain (bool bass,
                                        const miamp::BassControlSnapshot& bassIn,
                                        const zenbox::GuitarControlSnapshot& gtrIn,
                                        int lengthSamples = 384)
    {
        constexpr double kSr = 48000.0;
        constexpr int kPrewarm = 1024;
        const int total = kPrewarm + lengthSamples;

        std::vector<float> sig (static_cast<size_t> (total), 0.0f);
        for (int n = 0; n < total; ++n)
        {
            const float t = static_cast<float> (n) / static_cast<float> (kSr);
            const float env = 0.12f + 0.68f * (0.5f - 0.5f * std::cos (6.2831853f * static_cast<float> (n) / 512.0f));
            const float s = env * (0.48f * std::sin (6.2831853f * 110.0f * t)
                                 + 0.28f * std::sin (6.2831853f * 246.94f * t)
                                 + 0.16f * std::sin (6.2831853f * 587.33f * t)
                                 + 0.08f * std::sin (6.2831853f * 1480.0f * t));
            sig[static_cast<size_t> (n)] = std::clamp (s, -0.95f, 0.95f);
        }

        std::vector<float> out (static_cast<size_t> (total), 0.0f);
        if (bass)
        {
            auto snap = bassIn;
            snap.tunerMuted   = false;
            snap.ampPower     = true;
            snap.ampStandby   = false;
            snap.gateOn       = false;
            snap.pitchEnabled = false;
            miamp::MiAmpBassChannel chain;
            chain.prepare (kSr);
            for (int n = 0; n < total; ++n)
                out[static_cast<size_t> (n)] = chain.processSample (sig[static_cast<size_t> (n)], snap);
        }
        else
        {
            auto snap = gtrIn;
            snap.tunerMuted   = false;
            snap.ampPower     = true;
            snap.ampStandby   = false;
            snap.gateThresh   = 0.0f;
            snap.gatePedalOn  = false;
            snap.pitchEnabled = false;
            zenbox::MiAmpGuitarChannel chain;
            chain.prepare (kSr, 0);
            for (int n = 0; n < total; ++n)
                out[static_cast<size_t> (n)] = chain.processSample (sig[static_cast<size_t> (n)], snap);
        }

        CaptureDataset ds;
        ds.x.assign (sig.begin() + kPrewarm, sig.end());
        ds.y.assign (out.begin() + kPrewarm, out.end());

        double sumSq = 0.0;
        for (int i = 0; i < lengthSamples; ++i)
        {
            const float xv = ds.x[static_cast<size_t> (i)];
            const float yv = std::isfinite (ds.y[static_cast<size_t> (i)]) ? ds.y[static_cast<size_t> (i)] : 0.0f;
            ds.y[static_cast<size_t> (i)] = std::clamp (yv, -2.5f, 2.5f);
            ds.inputPeak  = std::max (ds.inputPeak, std::fabs (xv));
            ds.targetPeak = std::max (ds.targetPeak, std::fabs (ds.y[static_cast<size_t> (i)]));
            sumSq += static_cast<double> (ds.y[static_cast<size_t> (i)]) * ds.y[static_cast<size_t> (i)];
        }
        ds.targetRms = static_cast<float> (std::sqrt (sumSq / std::max (1, lengthSamples)));
        if (ds.targetRms < 1.0e-4f)
        {
            // Fallback clean unity pass if chain was muted/off
            ds.y = ds.x;
            ds.targetRms = 0.35f;
            ds.targetPeak = ds.inputPeak;
        }
        ds.loudnessDb = static_cast<float> (20.0 * std::log10 (std::max (1.0e-6f, ds.targetRms)));
        return ds;
    }

    // -------------------------------------------------------------------------
    // Compact Adam optimizer + LSTM / WaveNet distillers
    // -------------------------------------------------------------------------
    struct TrainedLstm
    {
        int H = 12;
        bool useSkip = false;
        // Row-major: W_ih (4H x 1), W_hh (4H x H), bias (4H), lin_w (H), lin_b (1)
        std::vector<float> W_ih;
        std::vector<float> W_hh;
        std::vector<float> bias;
        std::vector<float> lin_w;
        float lin_b = 0.0f;
        float esr = 1.0f;
    };

    static inline float sigmoidf (float x) noexcept
    {
        if (x > 16.0f) return 1.0f;
        if (x < -16.0f) return 0.0f;
        return 1.0f / (1.0f + std::exp (-x));
    }

    static TrainedLstm trainLstm (const CaptureDataset& ds, int hiddenSize, bool useSkip, int epochs, float lr)
    {
        const int H = hiddenSize;
        const int G = 4 * H;
        const int T = static_cast<int> (ds.x.size());

        TrainedLstm m;
        m.H = H;
        m.useSkip = useSkip;
        m.W_ih.assign (static_cast<size_t> (G), 0.0f);
        m.W_hh.assign (static_cast<size_t> (G * H), 0.0f);
        m.bias.assign (static_cast<size_t> (G), 0.0f);
        m.lin_w.assign (static_cast<size_t> (H), 0.0f);

        std::mt19937 rng (42u + static_cast<unsigned> (H) * 7u);
        const float scale = 0.18f / std::sqrt (static_cast<float> (H));
        std::uniform_real_distribution<float> dist (-scale, scale);
        for (float& v : m.W_ih)  v = dist (rng);
        for (float& v : m.W_hh)  v = dist (rng);
        for (float& v : m.lin_w) v = dist (rng);
        for (int j = 0; j < H; ++j)
            m.bias[static_cast<size_t> (H + j)] = 1.0f; // forget gate bias = +1

        const int nWih = G;
        const int nWhh = G * H;
        const int nBias = G;
        const int nLinW = H;
        const int nTotal = nWih + nWhh + nBias + nLinW + 1;

        std::vector<float> grad (static_cast<size_t> (nTotal), 0.0f);
        std::vector<float> m1   (static_cast<size_t> (nTotal), 0.0f);
        std::vector<float> v2   (static_cast<size_t> (nTotal), 0.0f);

        // Forward cache across time
        std::vector<float> iGate (static_cast<size_t> (T * H));
        std::vector<float> fGate (static_cast<size_t> (T * H));
        std::vector<float> gGate (static_cast<size_t> (T * H));
        std::vector<float> oGate (static_cast<size_t> (T * H));
        std::vector<float> cArr  (static_cast<size_t> ((T + 1) * H), 0.0f);
        std::vector<float> hArr  (static_cast<size_t> ((T + 1) * H), 0.0f);
        std::vector<float> tanhC (static_cast<size_t> (T * H));
        std::vector<float> yPred (static_cast<size_t> (T));

        std::vector<float> dhNext (static_cast<size_t> (H), 0.0f);
        std::vector<float> dcNext (static_cast<size_t> (H), 0.0f);
        std::vector<float> difgo  (static_cast<size_t> (G), 0.0f);
        std::vector<float> pre    (static_cast<size_t> (G), 0.0f);

        float targetVar = 0.0f;
        for (float yv : ds.y) targetVar += yv * yv;
        targetVar = std::max (1.0e-6f, targetVar);

        for (int ep = 1; ep <= epochs; ++ep)
        {
            std::fill (grad.begin(), grad.end(), 0.0f);
            std::fill (cArr.begin(), cArr.end(), 0.0f);
            std::fill (hArr.begin(), hArr.end(), 0.0f);

            float lossSum = 0.0f;
            for (int t = 0; t < T; ++t)
            {
                const float xt = ds.x[static_cast<size_t> (t)];
                const float* hPrev = &hArr[static_cast<size_t> (t * H)];
                const float* cPrev = &cArr[static_cast<size_t> (t * H)];
                float* hCur = &hArr[static_cast<size_t> ((t + 1) * H)];
                float* cCur = &cArr[static_cast<size_t> ((t + 1) * H)];

                for (int r = 0; r < G; ++r)
                {
                    float acc = m.bias[static_cast<size_t> (r)] + m.W_ih[static_cast<size_t> (r)] * xt;
                    const float* row = &m.W_hh[static_cast<size_t> (r * H)];
                    for (int k = 0; k < H; ++k)
                        acc += row[k] * hPrev[k];
                    pre[static_cast<size_t> (r)] = acc;
                }

                float yh = m.lin_b + (useSkip ? xt : 0.0f);
                for (int j = 0; j < H; ++j)
                {
                    const float ig = sigmoidf (pre[static_cast<size_t> (j)]);
                    const float fg = sigmoidf (pre[static_cast<size_t> (H + j)]);
                    const float gg = std::tanh (pre[static_cast<size_t> (2 * H + j)]);
                    const float og = sigmoidf (pre[static_cast<size_t> (3 * H + j)]);
                    const float cj = fg * cPrev[j] + ig * gg;
                    const float tc = std::tanh (cj);
                    const float hj = og * tc;

                    const size_t idx = static_cast<size_t> (t * H + j);
                    iGate[idx] = ig;
                    fGate[idx] = fg;
                    gGate[idx] = gg;
                    oGate[idx] = og;
                    tanhC[idx] = tc;
                    cCur[j] = cj;
                    hCur[j] = hj;
                    yh += m.lin_w[static_cast<size_t> (j)] * hj;
                }
                yPred[static_cast<size_t> (t)] = yh;
                const float err = yh - ds.y[static_cast<size_t> (t)];
                lossSum += err * err;
            }
            m.esr = lossSum / targetVar;

            // Backward pass (BPTT)
            std::fill (dhNext.begin(), dhNext.end(), 0.0f);
            std::fill (dcNext.begin(), dcNext.end(), 0.0f);

            const float invT = 2.0f / static_cast<float> (T);
            float* gWih  = grad.data();
            float* gWhh  = grad.data() + nWih;
            float* gBias = grad.data() + nWih + nWhh;
            float* gLinW = grad.data() + nWih + nWhh + nBias;
            float* gLinB = grad.data() + nWih + nWhh + nBias + nLinW;

            for (int t = T - 1; t >= 0; --t)
            {
                const float xt = ds.x[static_cast<size_t> (t)];
                const float* hPrev = &hArr[static_cast<size_t> (t * H)];
                const float* cPrev = &cArr[static_cast<size_t> (t * H)];
                const float* hCur  = &hArr[static_cast<size_t> ((t + 1) * H)];

                const float dy = invT * (yPred[static_cast<size_t> (t)] - ds.y[static_cast<size_t> (t)]);
                *gLinB += dy;

                for (int j = 0; j < H; ++j)
                {
                    gLinW[j] += dy * hCur[j];
                    const float dh = dy * m.lin_w[static_cast<size_t> (j)] + dhNext[static_cast<size_t> (j)];
                    const size_t idx = static_cast<size_t> (t * H + j);
                    const float ig = iGate[idx];
                    const float fg = fGate[idx];
                    const float gg = gGate[idx];
                    const float og = oGate[idx];
                    const float tc = tanhC[idx];

                    const float do_ = dh * tc * og * (1.0f - og);
                    const float dc  = dh * og * (1.0f - tc * tc) + dcNext[static_cast<size_t> (j)];
                    const float di_ = dc * gg * ig * (1.0f - ig);
                    const float df_ = dc * cPrev[j] * fg * (1.0f - fg);
                    const float dg_ = dc * ig * (1.0f - gg * gg);

                    difgo[static_cast<size_t> (j)]         = di_;
                    difgo[static_cast<size_t> (H + j)]     = df_;
                    difgo[static_cast<size_t> (2 * H + j)] = dg_;
                    difgo[static_cast<size_t> (3 * H + j)] = do_;
                    dcNext[static_cast<size_t> (j)] = dc * fg;
                }

                std::fill (dhNext.begin(), dhNext.end(), 0.0f);
                for (int r = 0; r < G; ++r)
                {
                    const float dpre = difgo[static_cast<size_t> (r)];
                    gBias[r] += dpre;
                    gWih[r]  += dpre * xt;
                    float* gRow = &gWhh[r * H];
                    const float* wRow = &m.W_hh[static_cast<size_t> (r * H)];
                    for (int k = 0; k < H; ++k)
                    {
                        gRow[k] += dpre * hPrev[k];
                        dhNext[static_cast<size_t> (k)] += dpre * wRow[k];
                    }
                }
            }

            // Adam step with cosine decay & gradient clipping
            const float stepLr = lr * (0.05f + 0.95f * 0.5f * (1.0f + std::cos (3.14159265f * static_cast<float> (ep) / static_cast<float> (epochs))));
            const float b1 = 0.9f, b2 = 0.999f, eps = 1.0e-8f;
            const float bc1 = 1.0f - std::pow (b1, static_cast<float> (ep));
            const float bc2 = 1.0f - std::pow (b2, static_cast<float> (ep));

            auto applyAdam = [&](float* p, int off, int cnt)
            {
                for (int k = 0; k < cnt; ++k)
                {
                    const size_t gi = static_cast<size_t> (off + k);
                    const float g = std::clamp (grad[gi], -1.0f, 1.0f);
                    m1[gi] = b1 * m1[gi] + (1.0f - b1) * g;
                    v2[gi] = b2 * v2[gi] + (1.0f - b2) * g * g;
                    const float mh = m1[gi] / bc1;
                    const float vh = v2[gi] / bc2;
                    p[k] -= stepLr * mh / (std::sqrt (vh) + eps);
                }
            };
            applyAdam (m.W_ih.data(),  0, nWih);
            applyAdam (m.W_hh.data(),  nWih, nWhh);
            applyAdam (m.bias.data(),  nWih + nWhh, nBias);
            applyAdam (m.lin_w.data(), nWih + nWhh + nBias, nLinW);
            applyAdam (&m.lin_b,       nWih + nWhh + nBias + nLinW, 1);
        }
        return m;
    }

    // -------------------------------------------------------------------------
    // WaveNet Trainer (supports both NAM A1 0.5.0 and NAM A2 0.7.0 A2Fast shape)
    // -------------------------------------------------------------------------
    struct WaveNetSpec
    {
        int C = 3;
        std::vector<int> kernels;
        std::vector<int> dilations;
        int headKernel = 1;
        bool leakyRelu = false; // false = Tanh (A1), true = LeakyReLU(0.01) (A2)
    };

    struct TrainedWaveNet
    {
        WaveNetSpec spec;
        std::vector<float> rechannel;           // C
        struct LayerW
        {
            std::vector<float> convW;           // C * C * K (out i, in j, tap k)
            std::vector<float> convB;           // C
            std::vector<float> mixinW;          // C
            std::vector<float> l1x1W;           // C * C (out i, in j)
            std::vector<float> l1x1B;           // C
        };
        std::vector<LayerW> layers;
        std::vector<float> headW;               // C * headKernel (in j, tap k)
        float headB = 0.0f;
        float headScale = 1.0f;
        float esr = 1.0f;

        std::vector<float> flattenWeights() const
        {
            std::vector<float> w;
            for (float v : rechannel) w.push_back (v);
            for (const auto& L : layers)
            {
                for (float v : L.convW)  w.push_back (v);
                for (float v : L.convB)  w.push_back (v);
                for (float v : L.mixinW) w.push_back (v);
                for (float v : L.l1x1W)  w.push_back (v);
                for (float v : L.l1x1B)  w.push_back (v);
            }
            for (float v : headW) w.push_back (v);
            w.push_back (headB);
            w.push_back (headScale);
            return w;
        }
    };

    static TrainedWaveNet trainWaveNet (const CaptureDataset& ds, const WaveNetSpec& spec, int epochs, float lr)
    {
        const int C = spec.C;
        const int Lnum = static_cast<int> (spec.kernels.size());
        const int HK = spec.headKernel;
        const int T = static_cast<int> (ds.x.size());

        TrainedWaveNet net;
        net.spec = spec;
        net.rechannel.assign (static_cast<size_t> (C), 0.0f);
        net.layers.resize (static_cast<size_t> (Lnum));

        std::mt19937 rng (1337u + static_cast<unsigned> (C + Lnum));
        std::uniform_real_distribution<float> dist (-0.06f, 0.06f);
        std::uniform_real_distribution<float> distSmall (-0.025f, 0.025f);

        for (int c = 0; c < C; ++c)
            net.rechannel[static_cast<size_t> (c)] = (c == 0) ? 0.5f : dist (rng);

        int nParams = C; // rechannel
        for (int l = 0; l < Lnum; ++l)
        {
            const int K = spec.kernels[static_cast<size_t> (l)];
            auto& lw = net.layers[static_cast<size_t> (l)];
            lw.convW.assign  (static_cast<size_t> (C * C * K), 0.0f);
            lw.convB.assign  (static_cast<size_t> (C), 0.0f);
            lw.mixinW.assign (static_cast<size_t> (C), 0.0f);
            lw.l1x1W.assign  (static_cast<size_t> (C * C), 0.0f);
            lw.l1x1B.assign  (static_cast<size_t> (C), 0.0f);
            for (float& v : lw.convW)  v = distSmall (rng);
            for (float& v : lw.mixinW) v = dist (rng);
            for (float& v : lw.l1x1W)  v = distSmall (rng);
            nParams += C * C * K + C + C + C * C + C;
        }
        net.headW.assign (static_cast<size_t> (C * HK), 0.0f);
        for (float& v : net.headW) v = dist (rng);
        nParams += C * HK + 1; // headW + headB (headScale fixed at 1.0)

        std::vector<float> grad (static_cast<size_t> (nParams), 0.0f);
        std::vector<float> m1   (static_cast<size_t> (nParams), 0.0f);
        std::vector<float> v2   (static_cast<size_t> (nParams), 0.0f);

        // Activation buffers
        std::vector<std::vector<float>> layerIn  (static_cast<size_t> (Lnum + 1), std::vector<float> (static_cast<size_t> (C * T), 0.0f));
        std::vector<std::vector<float>> preAct   (static_cast<size_t> (Lnum),     std::vector<float> (static_cast<size_t> (C * T), 0.0f));
        std::vector<std::vector<float>> postAct  (static_cast<size_t> (Lnum),     std::vector<float> (static_cast<size_t> (C * T), 0.0f));
        std::vector<float> headSum (static_cast<size_t> (C * T), 0.0f);
        std::vector<float> yPred   (static_cast<size_t> (T), 0.0f);

        std::vector<float> dLayerIn (static_cast<size_t> (C * T), 0.0f);
        std::vector<float> dHeadSum (static_cast<size_t> (C * T), 0.0f);
        std::vector<float> dAct     (static_cast<size_t> (C * T), 0.0f);
        std::vector<float> dPre     (static_cast<size_t> (C * T), 0.0f);
        std::vector<float> dL1x1    (static_cast<size_t> (C * T), 0.0f);

        float targetVar = 0.0f;
        for (float yv : ds.y) targetVar += yv * yv;
        targetVar = std::max (1.0e-6f, targetVar);

        for (int ep = 1; ep <= epochs; ++ep)
        {
            std::fill (grad.begin(), grad.end(), 0.0f);
            std::fill (headSum.begin(), headSum.end(), 0.0f);

            // Rechannel: layerIn[0][c, t] = rechannel[c] * x[t]
            for (int c = 0; c < C; ++c)
                for (int t = 0; t < T; ++t)
                    layerIn[0][static_cast<size_t> (c * T + t)] = net.rechannel[static_cast<size_t> (c)] * ds.x[static_cast<size_t> (t)];

            // Layers forward
            for (int l = 0; l < Lnum; ++l)
            {
                const int K = spec.kernels[static_cast<size_t> (l)];
                const int D = spec.dilations[static_cast<size_t> (l)];
                const auto& lw = net.layers[static_cast<size_t> (l)];
                const auto& inBuf = layerIn[static_cast<size_t> (l)];
                auto& nextBuf = layerIn[static_cast<size_t> (l + 1)];
                auto& pBuf = preAct[static_cast<size_t> (l)];
                auto& aBuf = postAct[static_cast<size_t> (l)];

                for (int i = 0; i < C; ++i)
                {
                    const float b = lw.convB[static_cast<size_t> (i)];
                    const float mw = lw.mixinW[static_cast<size_t> (i)];
                    for (int t = 0; t < T; ++t)
                    {
                        float acc = b + mw * ds.x[static_cast<size_t> (t)];
                        for (int j = 0; j < C; ++j)
                        {
                            for (int k = 0; k < K; ++k)
                            {
                                const int tSrc = t - (K - 1 - k) * D;
                                if (tSrc >= 0)
                                {
                                    const size_t wIdx = static_cast<size_t> ((i * C + j) * K + k);
                                    acc += lw.convW[wIdx] * inBuf[static_cast<size_t> (j * T + tSrc)];
                                }
                            }
                        }
                        const size_t ct = static_cast<size_t> (i * T + t);
                        pBuf[ct] = acc;
                        const float actVal = spec.leakyRelu ? (acc >= 0.0f ? acc : 0.01f * acc) : std::tanh (acc);
                        aBuf[ct] = actVal;
                        headSum[ct] += actVal;
                    }
                }

                // Residual: next = in + l1x1(aBuf)
                for (int i = 0; i < C; ++i)
                {
                    const float lb = lw.l1x1B[static_cast<size_t> (i)];
                    for (int t = 0; t < T; ++t)
                    {
                        float acc = lb;
                        for (int j = 0; j < C; ++j)
                            acc += lw.l1x1W[static_cast<size_t> (i * C + j)] * aBuf[static_cast<size_t> (j * T + t)];
                        const size_t ct = static_cast<size_t> (i * T + t);
                        nextBuf[ct] = inBuf[ct] + acc;
                    }
                }
            }

            // Head rechannel (causal Conv1D: C -> 1, kernel HK)
            float lossSum = 0.0f;
            for (int t = 0; t < T; ++t)
            {
                float yv = net.headB;
                for (int j = 0; j < C; ++j)
                {
                    for (int k = 0; k < HK; ++k)
                    {
                        const int tSrc = t - (HK - 1 - k);
                        if (tSrc >= 0)
                            yv += net.headW[static_cast<size_t> (j * HK + k)] * headSum[static_cast<size_t> (j * T + tSrc)];
                    }
                }
                yv *= net.headScale;
                yPred[static_cast<size_t> (t)] = yv;
                const float err = yv - ds.y[static_cast<size_t> (t)];
                lossSum += err * err;
            }
            net.esr = lossSum / targetVar;

            // Backward pass
            std::fill (dLayerIn.begin(), dLayerIn.end(), 0.0f);
            std::fill (dHeadSum.begin(), dHeadSum.end(), 0.0f);

            const int headOff = nParams - (C * HK + 1);
            float* gHeadW = grad.data() + headOff;
            float* gHeadB = grad.data() + headOff + C * HK;

            const float invT = 2.0f / static_cast<float> (T);
            for (int t = 0; t < T; ++t)
            {
                const float dy = invT * (yPred[static_cast<size_t> (t)] - ds.y[static_cast<size_t> (t)]) * net.headScale;
                *gHeadB += dy;
                for (int j = 0; j < C; ++j)
                {
                    for (int k = 0; k < HK; ++k)
                    {
                        const int tSrc = t - (HK - 1 - k);
                        if (tSrc >= 0)
                        {
                            const size_t hwIdx = static_cast<size_t> (j * HK + k);
                            gHeadW[hwIdx] += dy * headSum[static_cast<size_t> (j * T + tSrc)];
                            dHeadSum[static_cast<size_t> (j * T + tSrc)] += dy * net.headW[hwIdx];
                        }
                    }
                }
            }

            // Calculate layer offsets in grad
            std::vector<int> layerOffs (static_cast<size_t> (Lnum));
            int curOff = C;
            for (int l = 0; l < Lnum; ++l)
            {
                layerOffs[static_cast<size_t> (l)] = curOff;
                const int K = spec.kernels[static_cast<size_t> (l)];
                curOff += C * C * K + C + C + C * C + C;
            }

            for (int l = Lnum - 1; l >= 0; --l)
            {
                const int K = spec.kernels[static_cast<size_t> (l)];
                const int D = spec.dilations[static_cast<size_t> (l)];
                const auto& lw = net.layers[static_cast<size_t> (l)];
                const auto& inBuf = layerIn[static_cast<size_t> (l)];
                const auto& pBuf  = preAct[static_cast<size_t> (l)];
                const auto& aBuf  = postAct[static_cast<size_t> (l)];

                const int base = layerOffs[static_cast<size_t> (l)];
                float* gConvW  = grad.data() + base;
                float* gConvB  = gConvW + C * C * K;
                float* gMixinW = gConvB + C;
                float* gL1x1W  = gMixinW + C;
                float* gL1x1B  = gL1x1W + C * C;

                // dLayerIn holds dL/d(layerIn[l+1])
                std::fill (dAct.begin(), dAct.end(), 0.0f);
                for (int i = 0; i < C; ++i)
                {
                    float lbGrad = 0.0f;
                    for (int t = 0; t < T; ++t)
                    {
                        const float dNext = dLayerIn[static_cast<size_t> (i * T + t)];
                        lbGrad += dNext;
                        for (int j = 0; j < C; ++j)
                        {
                            const size_t wIdx = static_cast<size_t> (i * C + j);
                            const size_t jt = static_cast<size_t> (j * T + t);
                            gL1x1W[wIdx] += dNext * aBuf[jt];
                            dAct[jt] += dNext * lw.l1x1W[wIdx];
                        }
                    }
                    gL1x1B[i] += lbGrad;
                }

                for (size_t ct = 0; ct < static_cast<size_t> (C * T); ++ct)
                {
                    const float da = dAct[ct] + dHeadSum[ct];
                    const float dActPrime = spec.leakyRelu
                        ? (pBuf[ct] >= 0.0f ? 1.0f : 0.01f)
                        : (1.0f - aBuf[ct] * aBuf[ct]);
                    dPre[ct] = da * dActPrime;
                }

                // Backprop through conv + mixin + residual
                // Note: dLayerIn currently holds residual gradient (already = dNext)
                for (int i = 0; i < C; ++i)
                {
                    float bGrad = 0.0f;
                    float mGrad = 0.0f;
                    for (int t = 0; t < T; ++t)
                    {
                        const float dp = dPre[static_cast<size_t> (i * T + t)];
                        bGrad += dp;
                        mGrad += dp * ds.x[static_cast<size_t> (t)];
                        for (int j = 0; j < C; ++j)
                        {
                            for (int k = 0; k < K; ++k)
                            {
                                const int tSrc = t - (K - 1 - k) * D;
                                if (tSrc >= 0)
                                {
                                    const size_t wIdx = static_cast<size_t> ((i * C + j) * K + k);
                                    const size_t jts = static_cast<size_t> (j * T + tSrc);
                                    gConvW[wIdx] += dp * inBuf[jts];
                                    dLayerIn[jts] += dp * lw.convW[wIdx];
                                }
                            }
                        }
                    }
                    gConvB[i]  += bGrad;
                    gMixinW[i] += mGrad;
                }
            }

            // Rechannel grad
            for (int c = 0; c < C; ++c)
            {
                float rg = 0.0f;
                for (int t = 0; t < T; ++t)
                    rg += dLayerIn[static_cast<size_t> (c * T + t)] * ds.x[static_cast<size_t> (t)];
                grad[static_cast<size_t> (c)] += rg;
            }

            // Adam update
            const float stepLr = lr * (0.05f + 0.95f * 0.5f * (1.0f + std::cos (3.14159265f * static_cast<float> (ep) / static_cast<float> (epochs))));
            const float b1 = 0.9f, b2 = 0.999f, eps = 1.0e-8f;
            const float bc1 = 1.0f - std::pow (b1, static_cast<float> (ep));
            const float bc2 = 1.0f - std::pow (b2, static_cast<float> (ep));

            auto upd = [&](float* p, int off, int cnt)
            {
                for (int k = 0; k < cnt; ++k)
                {
                    const size_t gi = static_cast<size_t> (off + k);
                    const float g = std::clamp (grad[gi], -1.0f, 1.0f);
                    m1[gi] = b1 * m1[gi] + (1.0f - b1) * g;
                    v2[gi] = b2 * v2[gi] + (1.0f - b2) * g * g;
                    p[k] -= stepLr * (m1[gi] / bc1) / (std::sqrt (v2[gi] / bc2) + eps);
                }
            };
            upd (net.rechannel.data(), 0, C);
            for (int l = 0; l < Lnum; ++l)
            {
                const int K = spec.kernels[static_cast<size_t> (l)];
                const int b = layerOffs[static_cast<size_t> (l)];
                auto& lw = net.layers[static_cast<size_t> (l)];
                upd (lw.convW.data(),  b, C * C * K);
                upd (lw.convB.data(),  b + C * C * K, C);
                upd (lw.mixinW.data(), b + C * C * K + C, C);
                upd (lw.l1x1W.data(),  b + C * C * K + 2 * C, C * C);
                upd (lw.l1x1B.data(),  b + C * C * K + 2 * C + C * C, C);
            }
            upd (net.headW.data(), headOff, C * HK);
            upd (&net.headB, headOff + C * HK, 1);
        }
        return net;
    }

    static TrainedWaveNet embedLiteIntoFullA2 (const TrainedWaveNet& lite, int fullC = 8)
    {
        TrainedWaveNet full;
        full.spec = lite.spec;
        full.spec.C = fullC;
        const int liteC = lite.spec.C;
        const int Lnum = static_cast<int> (lite.layers.size());
        const int HK = lite.spec.headKernel;

        full.rechannel.assign (static_cast<size_t> (fullC), 0.0f);
        for (int c = 0; c < liteC; ++c)
            full.rechannel[static_cast<size_t> (c)] = lite.rechannel[static_cast<size_t> (c)];

        full.layers.resize (static_cast<size_t> (Lnum));
        for (int l = 0; l < Lnum; ++l)
        {
            const int K = lite.spec.kernels[static_cast<size_t> (l)];
            const auto& src = lite.layers[static_cast<size_t> (l)];
            auto& dst = full.layers[static_cast<size_t> (l)];
            dst.convW.assign  (static_cast<size_t> (fullC * fullC * K), 0.0f);
            dst.convB.assign  (static_cast<size_t> (fullC), 0.0f);
            dst.mixinW.assign (static_cast<size_t> (fullC), 0.0f);
            dst.l1x1W.assign  (static_cast<size_t> (fullC * fullC), 0.0f);
            dst.l1x1B.assign  (static_cast<size_t> (fullC), 0.0f);

            for (int i = 0; i < liteC; ++i)
            {
                dst.convB[static_cast<size_t> (i)]  = src.convB[static_cast<size_t> (i)];
                dst.mixinW[static_cast<size_t> (i)] = src.mixinW[static_cast<size_t> (i)];
                dst.l1x1B[static_cast<size_t> (i)]  = src.l1x1B[static_cast<size_t> (i)];
                for (int j = 0; j < liteC; ++j)
                {
                    dst.l1x1W[static_cast<size_t> (i * fullC + j)] = src.l1x1W[static_cast<size_t> (i * liteC + j)];
                    for (int k = 0; k < K; ++k)
                        dst.convW[static_cast<size_t> ((i * fullC + j) * K + k)] = src.convW[static_cast<size_t> ((i * liteC + j) * K + k)];
                }
            }
        }
        full.headW.assign (static_cast<size_t> (fullC * HK), 0.0f);
        for (int j = 0; j < liteC; ++j)
            for (int k = 0; k < HK; ++k)
                full.headW[static_cast<size_t> (j * HK + k)] = lite.headW[static_cast<size_t> (j * HK + k)];
        full.headB = lite.headB;
        full.headScale = lite.headScale;
        full.esr = lite.esr;
        return full;
    }

    static std::string jsonEscape (const std::string& in)
    {
        std::string out;
        out.reserve (in.size() + 8);
        for (char c : in)
        {
            if (c == '\\') out += "\\\\";
            else if (c == '"') out += "\\\"";
            else if (c == '\n') out += "\\n";
            else if (c == '\r') out += "\\r";
            else if (c == '\t') out += "\\t";
            else out += c;
        }
        return out;
    }

    static void appendFloatArray (std::ostringstream& ss, const std::vector<float>& arr)
    {
        ss << "[";
        for (size_t i = 0; i < arr.size(); ++i)
        {
            if (i > 0) ss << ",";
            const float v = std::isfinite (arr[i]) ? arr[i] : 0.0f;
            ss << v;
        }
        ss << "]";
    }

    static std::string buildA2LayerJson (const TrainedWaveNet& net)
    {
        const int C = net.spec.C;
        const int L = static_cast<int> (net.spec.kernels.size());
        std::ostringstream ss;
        ss << std::setprecision (8);
        ss << "{\"version\":\"0.7.0\",\"metadata\":{\"name\":\"MI AMP Capture\",\"modeled_by\":\"MI AMP\",\"loudness\":-18.0,\"gain\":1.0,\"input_level_dbu\":12.0,\"output_level_dbu\":12.0},";
        ss << "\"architecture\":\"WaveNet\",\"config\":{\"layers\":[{";
        ss << "\"input_size\":1,\"condition_size\":1,";
        ss << "\"head\":{\"out_channels\":1,\"kernel_size\":16,\"bias\":true},";
        ss << "\"channels\":" << C << ",";
        ss << "\"kernel_sizes\":[";
        for (int i = 0; i < L; ++i) ss << (i ? "," : "") << net.spec.kernels[static_cast<size_t> (i)];
        ss << "],\"dilations\":[";
        for (int i = 0; i < L; ++i) ss << (i ? "," : "") << net.spec.dilations[static_cast<size_t> (i)];
        ss << "],\"activation\":[";
        for (int i = 0; i < L; ++i) ss << (i ? "," : "") << "{\"type\":\"LeakyReLU\",\"negative_slope\":0.01}";
        ss << "],\"bottleneck\":" << C << ",";
        ss << "\"head1x1\":{\"active\":false,\"out_channels\":1,\"groups\":1},";
        ss << "\"layer1x1\":{\"active\":true,\"groups\":1},";
        ss << "\"groups_input\":1,\"groups_input_mixin\":1,";
        ss << "\"conv_pre_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"conv_post_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"input_mixin_pre_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"input_mixin_post_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"activation_pre_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"activation_post_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"layer1x1_post_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"head1x1_post_film\":{\"active\":false,\"shift\":true,\"groups\":1},";
        ss << "\"gating_mode\":[";
        for (int i = 0; i < L; ++i) ss << (i ? "," : "") << "\"none\"";
        ss << "],\"secondary_activation\":[";
        for (int i = 0; i < L; ++i) ss << (i ? "," : "") << "null";
        ss << "],\"slimmable\":null}],\"head\":null,\"head_scale\":" << net.headScale << "},\"weights\":";
        appendFloatArray (ss, net.flattenWeights());
        ss << ",\"sample_rate\":48000}";
        return ss.str();
    }

    // -------------------------------------------------------------------------
    // Export 1: NAM A2 (0.7.0 SlimmableContainer: Lite 3ch + Full 8ch)
    // -------------------------------------------------------------------------
    static bool exportNamA2 (bool bass,
                             const miamp::BassControlSnapshot& bassSnap,
                             const zenbox::GuitarControlSnapshot& gtrSnap,
                             const juce::String& presetName,
                             const juce::File& outFile,
                             float* outEsr = nullptr)
    {
        const auto ds = captureChain (bass, bassSnap, gtrSnap, 384);
        WaveNetSpec spec;
        spec.C = 3;
        spec.kernels   = { 6,6,6,6,6,6,6, 6,6,6,6,6,6,6, 15,15, 6,6,6,6,6,6,6 };
        spec.dilations = { 1,3,7,17,41,101,239, 1,3,7,17,41,101,239, 1,13, 1,3,7,17,41,101,239 };
        spec.headKernel = 16;
        spec.leakyRelu = true;

        const auto lite = trainWaveNet (ds, spec, 55, 0.032f);
        const auto full = embedLiteIntoFullA2 (lite, 8);
        if (outEsr) *outEsr = lite.esr;

        std::ostringstream ss;
        ss << std::setprecision (8);
        ss << "{\n";
        ss << "  \"version\": \"0.7.0\",\n";
        ss << "  \"metadata\": {\"name\": \"" << jsonEscape (presetName.toRawUTF8()) << "\", \"modeled_by\": \"MI AMP\", \"gear_type\": \"rig\", \"tone_type\": \"" << (bass ? "bass" : "guitar") << "\", \"loudness\": " << ds.loudnessDb << ", \"validation_esr\": " << lite.esr << "},\n";
        ss << "  \"architecture\": \"SlimmableContainer\",\n";
        ss << "  \"config\": {\"submodels\": [\n";
        ss << "    {\"max_value\": 0.5, \"model\": " << buildA2LayerJson (lite) << "},\n";
        ss << "    {\"max_value\": 1.0, \"model\": " << buildA2LayerJson (full) << "}\n";
        ss << "  ]}\n";
        ss << "}\n";
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 2: NAM A1 (0.5.0 WaveNet)
    // -------------------------------------------------------------------------
    static bool exportNamA1 (bool bass,
                             const miamp::BassControlSnapshot& bassSnap,
                             const zenbox::GuitarControlSnapshot& gtrSnap,
                             const juce::String& presetName,
                             const juce::File& outFile,
                             float* outEsr = nullptr)
    {
        const auto ds = captureChain (bass, bassSnap, gtrSnap, 384);
        WaveNetSpec spec;
        spec.C = 4;
        spec.kernels    = { 3, 3, 3, 3, 3, 3 };
        spec.dilations  = { 1, 2, 4, 8, 16, 32 };
        spec.headKernel = 1;
        spec.leakyRelu  = false;

        const auto net = trainWaveNet (ds, spec, 75, 0.035f);
        if (outEsr) *outEsr = net.esr;

        std::ostringstream ss;
        ss << std::setprecision (8);
        ss << "{\n";
        ss << "  \"version\": \"0.5.0\",\n";
        ss << "  \"metadata\": {\"name\": \"" << jsonEscape (presetName.toRawUTF8()) << "\", \"modeled_by\": \"MI AMP\", \"gear_type\": \"rig\", \"tone_type\": \"" << (bass ? "bass" : "guitar") << "\", \"loudness\": " << ds.loudnessDb << ", \"validation_esr\": " << net.esr << "},\n";
        ss << "  \"architecture\": \"WaveNet\",\n";
        ss << "  \"config\": {\n";
        ss << "    \"layers\": [{\n";
        ss << "      \"input_size\": 1, \"condition_size\": 1, \"head_size\": 1,\n";
        ss << "      \"channels\": 4, \"kernel_size\": 3,\n";
        ss << "      \"dilations\": [1, 2, 4, 8, 16, 32],\n";
        ss << "      \"activation\": \"Tanh\", \"gated\": false, \"head_bias\": true\n";
        ss << "    }],\n";
        ss << "    \"head\": null, \"head_scale\": " << net.headScale << "\n";
        ss << "  },\n";
        ss << "  \"weights\": ";
        appendFloatArray (ss, net.flattenWeights());
        ss << ",\n  \"sample_rate\": 48000\n}\n";
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 3: NAM LSTM (0.5.4 LSTM)
    // -------------------------------------------------------------------------
    static bool exportNamLstm (bool bass,
                               const miamp::BassControlSnapshot& bassSnap,
                               const zenbox::GuitarControlSnapshot& gtrSnap,
                               const juce::String& presetName,
                               const juce::File& outFile,
                               float* outEsr = nullptr)
    {
        const auto ds = captureChain (bass, bassSnap, gtrSnap, 384);
        constexpr int H = 12;
        const auto m = trainLstm (ds, H, false, 80, 0.025f);
        if (outEsr) *outEsr = m.esr;

        // NAM LSTM weights: row-major _w of shape (4H, input_size + H), _b (4H), init_h (H), init_c (H), head_w (H), head_b (1)
        std::vector<float> flat;
        const int G = 4 * H;
        for (int r = 0; r < G; ++r)
        {
            flat.push_back (m.W_ih[static_cast<size_t> (r)]);
            for (int k = 0; k < H; ++k)
                flat.push_back (m.W_hh[static_cast<size_t> (r * H + k)]);
        }
        flat.insert (flat.end(), m.bias.begin(), m.bias.end());
        flat.insert (flat.end(), static_cast<size_t> (H), 0.0f); // initial h
        flat.insert (flat.end(), static_cast<size_t> (H), 0.0f); // initial c
        flat.insert (flat.end(), m.lin_w.begin(), m.lin_w.end());
        flat.push_back (m.lin_b);

        std::ostringstream ss;
        ss << std::setprecision (8);
        ss << "{\n";
        ss << "  \"version\": \"0.5.4\",\n";
        ss << "  \"metadata\": {\"name\": \"" << jsonEscape (presetName.toRawUTF8()) << "\", \"modeled_by\": \"MI AMP\", \"gear_type\": \"rig\", \"tone_type\": \"" << (bass ? "bass" : "guitar") << "\", \"loudness\": " << ds.loudnessDb << ", \"validation_esr\": " << m.esr << "},\n";
        ss << "  \"architecture\": \"LSTM\",\n";
        ss << "  \"config\": {\"input_size\": 1, \"hidden_size\": " << H << ", \"num_layers\": 1},\n";
        ss << "  \"weights\": ";
        appendFloatArray (ss, flat);
        ss << ",\n  \"sample_rate\": 48000\n}\n";
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 4: AIDA-X (.aidax RTNeural Keras LSTM-12 + Dense)
    // -------------------------------------------------------------------------
    static bool exportAidaX (bool bass,
                             const miamp::BassControlSnapshot& bassSnap,
                             const zenbox::GuitarControlSnapshot& gtrSnap,
                             const juce::String& presetName,
                             const juce::File& outFile,
                             float* outEsr = nullptr)
    {
        const auto ds = captureChain (bass, bassSnap, gtrSnap, 384);
        constexpr int H = 12;
        constexpr int G = 4 * H;
        const auto m = trainLstm (ds, H, false, 80, 0.025f);
        if (outEsr) *outEsr = m.esr;

        std::ostringstream ss;
        ss << std::setprecision (8);
        ss << "{\n  \"in_shape\": [null, null, 1],\n  \"layers\": [\n    {\n";
        ss << "      \"type\": \"lstm\",\n      \"activation\": \"\",\n      \"shape\": [null, null, " << H << "],\n";
        ss << "      \"weights\": [\n        [";
        appendFloatArray (ss, m.W_ih);
        ss << "],\n        [";
        // Keras W_hh is (H, 4H) = transpose of PyTorch W_hh (4H, H)
        for (int j = 0; j < H; ++j)
        {
            if (j > 0) ss << ",";
            std::vector<float> col (static_cast<size_t> (G));
            for (int r = 0; r < G; ++r)
                col[static_cast<size_t> (r)] = m.W_hh[static_cast<size_t> (r * H + j)];
            appendFloatArray (ss, col);
        }
        ss << "],\n        ";
        appendFloatArray (ss, m.bias);
        ss << "\n      ]\n    },\n    {\n";
        ss << "      \"type\": \"dense\",\n      \"activation\": \"\",\n      \"shape\": [null, null, 1],\n";
        ss << "      \"weights\": [\n        [";
        for (int j = 0; j < H; ++j)
            ss << (j ? "," : "") << "[" << m.lin_w[static_cast<size_t> (j)] << "]";
        ss << "],\n        [" << m.lin_b << "]\n      ]\n    }\n  ],\n";
        ss << "  \"metadata\": {\n";
        ss << "    \"name\": \"" << jsonEscape (presetName.toRawUTF8()) << "\",\n";
        ss << "    \"samplerate\": \"48000\",\n";
        ss << "    \"source\": \"MI AMP\",\n";
        ss << "    \"style\": \"" << (bass ? "bass" : "guitar") << "\",\n";
        ss << "    \"author\": \"MI AMP\",\n";
        ss << "    \"esr\": " << m.esr << "\n  }\n}\n";
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 5 & 6: GuitarML Proteus (H=40) & NeuralPi (H=20) (.json)
    // -------------------------------------------------------------------------
    static bool exportGuitarMlJson (bool bass,
                                    const miamp::BassControlSnapshot& bassSnap,
                                    const zenbox::GuitarControlSnapshot& gtrSnap,
                                    int hiddenSize,
                                    const juce::File& outFile,
                                    float* outEsr = nullptr)
    {
        const auto ds = captureChain (bass, bassSnap, gtrSnap, 320);
        const int epochs = (hiddenSize >= 32) ? 55 : 75;
        const auto m = trainLstm (ds, hiddenSize, false, epochs, 0.025f);
        if (outEsr) *outEsr = m.esr;

        const int H = hiddenSize;
        const int G = 4 * H;
        std::ostringstream ss;
        ss << std::setprecision (8);
        ss << "{\"model_data\": {\"model\": \"SimpleRNN\", \"input_size\": 1, \"skip\": 0, \"output_size\": 1, \"unit_type\": \"LSTM\", \"num_layers\": 1, \"hidden_size\": " << H << ", \"bias_fl\": true}, \"state_dict\": {";

        ss << "\"rec.weight_ih_l0\": [";
        for (int r = 0; r < G; ++r)
            ss << (r ? ", " : "") << "[" << m.W_ih[static_cast<size_t> (r)] << "]";
        ss << "], \"rec.weight_hh_l0\": [";
        for (int r = 0; r < G; ++r)
        {
            if (r > 0) ss << ", ";
            std::vector<float> row (m.W_hh.begin() + r * H, m.W_hh.begin() + (r + 1) * H);
            appendFloatArray (ss, row);
        }
        ss << "], \"rec.bias_ih_l0\": ";
        appendFloatArray (ss, m.bias);
        ss << ", \"rec.bias_hh_l0\": ";
        std::vector<float> zeroBias (static_cast<size_t> (G), 0.0f);
        appendFloatArray (ss, zeroBias);
        ss << ", \"lin.weight\": [";
        appendFloatArray (ss, m.lin_w);
        ss << "], \"lin.bias\": [" << m.lin_b << "]}}";
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 7: Universal 44.1 kHz / 24-bit mono Impulse Response (.wav)
    // -------------------------------------------------------------------------
    static bool exportIrWav44k24Bit (bool bass,
                                     const miamp::BassControlSnapshot& snap,
                                     const zenbox::GuitarControlSnapshot& gsnap,
                                     const juce::File& outFile)
    {
        constexpr int kSr  = 44100;
        constexpr int kLen = 1024;
        std::vector<float> buf (kLen, 0.0f);
        buf[0] = 1.0f;

        if (bass)
        {
            const bool hasEq = snap.eqOn && std::find (snap.chainSlots.begin(), snap.chainSlots.end(), 13) != snap.chainSlots.end();
            const bool hasXv = snap.xvOn && std::find (snap.chainSlots.begin(), snap.chainSlots.end(), 1)  != snap.chainSlots.end();

            mipedals::Boutique7BandEQFX eq7;
            eq7.prepare (kSr, true);

            xvdsp::Biquad flLo, flMid, flHi, eqBass, eqMid, eqTreble;
            if (snap.ampUltraHi)
            {
                flLo.setLowShelf (kSr, 65.0, 0.0);
                flMid.setPeaking (kSr, 450.0, 0.0, 0.9);
                flHi.setHighShelf (kSr, 4200.0, 4.8);
            }
            else
            {
                flLo.setLowShelf (kSr, 65.0, 4.0);
                flMid.setPeaking (kSr, 450.0, -2.5, 0.9);
                flHi.setHighShelf (kSr, 4200.0, 0.0);
            }
            eqBass.setLowShelf (kSr, 80.0, (snap.ampBass * 0.01 - 0.5) * 18.0);
            const double midHz = 220.0 * std::pow (2800.0 / 220.0, snap.ampFreq * 0.01);
            eqMid.setPeaking (kSr, midHz, (snap.ampMid * 0.01 - 0.5) * 18.0, 1.2);
            eqTreble.setHighShelf (kSr, 3200.0, (snap.ampTreble * 0.01 - 0.5) * 18.0);

            xvdsp::CabIrConvolver cab;
            cab.prepare (kSr);
            const int cabMode = hasXv ? std::clamp (snap.xvCab, 0, 2) : 0;

            for (int i = 0; i < kLen; ++i)
            {
                float s = buf[static_cast<size_t> (i)];
                if (hasEq)
                    s = eq7.process (s, true, snap.eqBands);
                if (! snap.headBypass)
                {
                    s = flHi.processSample (flMid.processSample (flLo.processSample (s)));
                    s = eqTreble.processSample (eqMid.processSample (eqBass.processSample (s)));
                    if (snap.ampCabOn)
                        s = cab.processSample (s, cabMode);
                }
                else if (hasXv)
                {
                    s = cab.processSample (s, cabMode);
                }
                buf[static_cast<size_t> (i)] = s;
            }
        }
        else
        {
            const bool hasEq = gsnap.eqOn && std::find (gsnap.chainSlots.begin(), gsnap.chainSlots.end(), 12) != gsnap.chainSlots.end();
            mipedals::Boutique7BandEQFX eq7;
            eq7.prepare (kSr, false);

            zenbox::ZenboxCabConvolver cab;
            cab.prepare (kSr);
            const int cabIdx = std::clamp (gsnap.cabIndex, 0, 4);
            const int ch = std::clamp (gsnap.activeChannel, 0, 2);

            xvdsp::Biquad toneBass, toneMid, toneTreble, tonePres;
            toneBass.setLowShelf (kSr, 110.0, (gsnap.chBass[static_cast<size_t> (ch)] * 0.01 - 0.5) * 12.0);
            toneMid.setPeaking   (kSr, 750.0, (gsnap.chMid[static_cast<size_t> (ch)] * 0.01 - 0.5) * 10.0, 1.0);
            toneTreble.setHighShelf (kSr, 2600.0, (gsnap.chTreble[static_cast<size_t> (ch)] * 0.01 - 0.5) * 12.0);
            tonePres.setPeaking  (kSr, 4400.0, (gsnap.chPresence[static_cast<size_t> (ch)] * 0.01 - 0.5) * 8.0, 0.8);

            for (int i = 0; i < kLen; ++i)
            {
                float s = buf[static_cast<size_t> (i)];
                if (hasEq)
                    s = eq7.process (s, true, gsnap.eqBands);
                if (! gsnap.headBypass)
                {
                    s = tonePres.processSample (toneTreble.processSample (toneMid.processSample (toneBass.processSample (s))));
                    if (cabIdx < 4)
                        s = cab.processSample (s, cabIdx);
                }
                else if (cabIdx < 4)
                {
                    s = cab.processSample (s, cabIdx);
                }
                buf[static_cast<size_t> (i)] = s;
            }
        }

        float peak = 0.0f;
        for (float v : buf) peak = std::max (peak, std::fabs (v));
        const float norm = (peak > 1.0e-6f) ? (0.89f / peak) : 1.0f;

        std::ofstream os (outFile.getFullPathName().toRawUTF8(), std::ios::binary | std::ios::trunc);
        if (! os) return false;

        auto writeU32 = [&](uint32_t v) { uint8_t b[4] = { (uint8_t)(v&255), (uint8_t)((v>>8)&255), (uint8_t)((v>>16)&255), (uint8_t)((v>>24)&255) }; os.write ((char*)b, 4); };
        auto writeU16 = [&](uint16_t v) { uint8_t b[2] = { (uint8_t)(v&255), (uint8_t)((v>>8)&255) }; os.write ((char*)b, 2); };

        const uint32_t dataBytes = kLen * 3u;
        os.write ("RIFF", 4); writeU32 (36u + dataBytes); os.write ("WAVE", 4);
        os.write ("fmt ", 4); writeU32 (16u); writeU16 (1u); writeU16 (1u);
        writeU32 (kSr); writeU32 (kSr * 3u); writeU16 (3u); writeU16 (24u);
        os.write ("data", 4); writeU32 (dataBytes);

        for (float v : buf)
        {
            const double scaled = std::clamp (static_cast<double> (v * norm), -1.0, 1.0) * 8388607.0;
            const int32_t s24 = static_cast<int32_t> (std::llround (scaled));
            uint8_t b[3] = {
                static_cast<uint8_t> (s24 & 0xff),
                static_cast<uint8_t> ((s24 >> 8) & 0xff),
                static_cast<uint8_t> ((s24 >> 16) & 0xff)
            };
            os.write (reinterpret_cast<const char*> (b), 3);
        }
        return os.good();
    }

    // -------------------------------------------------------------------------
    // Export 8: M-Vave Blackbox guide (.txt)
    // -------------------------------------------------------------------------
    static bool exportBlackboxGuide (bool bass,
                                     const miamp::BassControlSnapshot& s,
                                     const zenbox::GuitarControlSnapshot& gs,
                                     const juce::String& presetName,
                                     const juce::File& outFile)
    {
        std::ostringstream ss;
        ss << "====================================================================\n";
        ss << "  MI AMP -> M-VAVE BLACKBOX / PEDALERA DIGITAL (GUIA DE MAPEO)\n";
        ss << "  Preset: " << presetName.toRawUTF8() << " (" << (bass ? "BAJO" : "GUITARRA") << ")\n";
        ss << "====================================================================\n\n";
        ss << "TIP M-VAVE BLACKBOX:\n";
        ss << "  - Si usas el firmware oficial (M-EFCS / ANNlab): exporta en .nam A2\n";
        ss << "    y convertilo con ANNlab, o exporta el .wav IR 44.1kHz para IR CAB.\n";
        ss << "  - Si usas Juice-Box en tu Blackbox: carga directo el .nam A2!\n\n";

        if (bass)
        {
            ss << "1. CADENA DE MODULOS:\n";
            ss << "   - NR (Noise Gate): " << (s.gateOn ? "ON" : "OFF") << " | Thresh " << (int)s.gateThresh << "%\n";
            ss << "   - FX/COMP: " << (s.compOn ? "ON" : "OFF") << " | Sus " << (int)s.compSustain << "% | Lvl " << (int)s.compLevel << "%\n";
            ss << "   - AMP (" << (s.headBypass ? "BYPASS / SOLO PEDALES" : "BASS HEAD") << "): Gain " << (int)s.ampGain
               << " | Bass " << (int)s.ampBass << " | Mid " << (int)s.ampMid << " (" << (int)s.ampFreq << "%) | Treble " << (int)s.ampTreble << "\n";
            ss << "   - CAB IR: " << (s.ampCabOn ? "ON (8x10 Bass)" : "OFF") << "\n";
            ss << "   - MOD: Chorus " << (s.chorusOn ? "ON" : "OFF") << " | Phaser " << (s.phaserOn ? "ON" : "OFF") << " | Flanger " << (s.flangeOn ? "ON" : "OFF") << "\n";
            ss << "   - DLY: " << (s.delayOn ? "ON" : "OFF") << " (" << (int)s.delayTime << "% Time, " << (int)s.delayMix << "% Mix)\n";
            ss << "   - REV: " << (s.reverbOn ? "ON" : "OFF") << " (" << (int)s.reverbDecay << "% Dec, " << (int)s.reverbMix << "% Mix)\n";
        }
        else
        {
            const int ch = std::clamp (gs.activeChannel, 0, 2);
            ss << "1. CADENA DE MODULOS:\n";
            ss << "   - NR (Noise Gate): Thresh " << (int)gs.gateThresh << "%\n";
            ss << "   - FX/COMP: " << (gs.compOn ? "ON" : "OFF") << " | Sus " << (int)gs.compSustain << "% | Lvl " << (int)gs.compLevel << "%\n";
            ss << "   - BOOST/DRIVE: " << (gs.driveOn ? "ON" : "OFF") << " (Gain " << (int)gs.driveGain << "%) | Dist " << (gs.distOn ? "ON" : "OFF") << "\n";
            ss << "   - AMP (" << (gs.headBypass ? "BYPASS / SOLO PEDALES" : ("CH" + std::to_string (ch + 1))) << "): Gain " << (int)gs.chGain[static_cast<size_t> (ch)]
               << " | Bass " << (int)gs.chBass[static_cast<size_t> (ch)] << " | Mid " << (int)gs.chMid[static_cast<size_t> (ch)]
               << " | Treble " << (int)gs.chTreble[static_cast<size_t> (ch)] << " | Pres " << (int)gs.chPresence[static_cast<size_t> (ch)] << "\n";
            ss << "   - CAB IR: " << (gs.cabIndex < 4 ? ("Cab #" + std::to_string (gs.cabIndex + 1)) : "OFF") << "\n";
            ss << "   - DLY: " << (gs.delayOn ? "ON" : "OFF") << " (" << (int)gs.delayTime << "% Time, " << (int)gs.delayMix << "% Mix)\n";
            ss << "   - REV: " << (gs.reverbOn ? "ON" : "OFF") << " (" << (int)gs.reverbDecay << "% Dec, " << (int)gs.reverbMix << "% Mix)\n";
        }
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 9: Line 6 Helix (.hlx)
    // -------------------------------------------------------------------------
    static bool exportHelixHlx (bool bass,
                                const miamp::BassControlSnapshot& s,
                                const zenbox::GuitarControlSnapshot& gs,
                                const juce::String& presetName,
                                const juce::File& outFile)
    {
        auto n01 = [](float v) { return std::clamp (v * 0.01f, 0.0f, 1.0f); };
        const int ch = std::clamp (gs.activeChannel, 0, 2);

        std::ostringstream ss;
        ss << std::fixed << std::setprecision (3);
        ss << "{\n  \"schema\": \"L6Preset\",\n  \"version\": 6,\n";
        ss << "  \"meta\": {\"name\": \"" << jsonEscape (presetName.toRawUTF8()) << "\", \"application\": \"MI AMP\", \"build_sha\": \"miamp-v18\"},\n";
        ss << "  \"data\": {\"device\": 2162689, \"tone\": {\"dsp0\": {\n";
        ss << "    \"block0\": {\"@model\": \"HD2_GateNoiseGate\", \"@enabled\": " << ((bass ? s.gateOn : (gs.gateThresh > 2.0f)) ? "true" : "false") << ", \"@path\": 0, \"@position\": 0, \"@type\": 0, \"Threshold\": -48.0, \"Decay\": 0.25},\n";
        ss << "    \"block1\": {\"@model\": \"HD2_CompressorDeluxeComp\", \"@enabled\": " << ((bass ? s.compOn : gs.compOn) ? "true" : "false") << ", \"@path\": 0, \"@position\": 1, \"@type\": 0, \"Threshold\": -24.0, \"Ratio\": 0.45, \"Level\": " << n01 (bass ? s.compLevel : gs.compLevel) << "},\n";
        ss << "    \"block2\": {\"@model\": \"" << (bass ? "HD2_DistObsidian7000" : "HD2_DistScream808") << "\", \"@enabled\": " << ((bass ? (s.xvOn || s.bddiOn || s.goldOn) : (gs.driveOn || gs.distOn)) ? "true" : "false") << ", \"@path\": 0, \"@position\": 2, \"@type\": 0, \"Gain\": " << n01 (bass ? s.xvDrive : gs.driveGain) << ", \"Tone\": " << n01 (bass ? s.xvMid : gs.driveTone) << ", \"Level\": " << n01 (bass ? s.xvLevel : gs.driveLevel) << "},\n";
        ss << "    \"block3\": {\"@model\": \"" << (bass ? "HD2_AmpSVBeastNrm" : (ch == 0 ? "HD2_AmpUSDoubleNrm" : "HD2_AmpBritPlexiJump")) << "\", \"@enabled\": " << ((bass ? !s.headBypass : !gs.headBypass) ? "true" : "false") << ", \"@path\": 0, \"@position\": 3, \"@type\": 1, \"Drive\": " << n01 (bass ? s.ampGain : gs.chGain[static_cast<size_t> (ch)]) << ", \"Bass\": " << n01 (bass ? s.ampBass : gs.chBass[static_cast<size_t> (ch)]) << ", \"Mid\": " << n01 (bass ? s.ampMid : gs.chMid[static_cast<size_t> (ch)]) << ", \"Treble\": " << n01 (bass ? s.ampTreble : gs.chTreble[static_cast<size_t> (ch)]) << ", \"Master\": " << n01 (bass ? 65.0f : gs.chMaster[static_cast<size_t> (ch)]) << "},\n";
        ss << "    \"block4\": {\"@model\": \"" << (bass ? "HD2_Cab8x10SVBeast" : "HD2_Cab4x12Greenback25") << "\", \"@enabled\": " << ((bass ? s.ampCabOn : (gs.cabIndex < 4)) ? "true" : "false") << ", \"@path\": 0, \"@position\": 4, \"@type\": 2},\n";
        ss << "    \"block5\": {\"@model\": \"HD2_DelaySimpleDelay\", \"@enabled\": " << ((bass ? s.delayOn : gs.delayOn) ? "true" : "false") << ", \"@path\": 0, \"@position\": 5, \"@type\": 7, \"Time\": " << n01 (bass ? s.delayTime : gs.delayTime) << ", \"Feedback\": " << n01 (bass ? s.delayFb : gs.delayFeedback) << ", \"Mix\": " << n01 (bass ? s.delayMix : gs.delayMix) << "},\n";
        ss << "    \"block6\": {\"@model\": \"HD2_ReverbPlate\", \"@enabled\": " << ((bass ? s.reverbOn : gs.reverbOn) ? "true" : "false") << ", \"@path\": 0, \"@position\": 6, \"@type\": 7, \"Decay\": " << n01 (bass ? s.reverbDecay : gs.reverbDecay) << ", \"Mix\": " << n01 (bass ? s.reverbMix : gs.reverbMix) << "}\n";
        ss << "  }, \"global\": {\"@model\": \"@global_params\"}}}\n}\n";
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 10: HeadRush (.rig)
    // -------------------------------------------------------------------------
    static bool exportHeadrushRig (bool bass,
                                   const miamp::BassControlSnapshot& s,
                                   const zenbox::GuitarControlSnapshot& gs,
                                   const juce::String& presetName,
                                   const juce::File& outFile)
    {
        auto n01 = [](float v) { return std::clamp (v * 0.01f, 0.0f, 1.0f); };
        const int ch = std::clamp (gs.activeChannel, 0, 2);

        std::ostringstream ss;
        ss << std::fixed << std::setprecision (3);
        ss << "{\n  \"rigName\": \"" << jsonEscape (presetName.toRawUTF8()) << "\",\n  \"author\": \"MI AMP\",\n";
        ss << "  \"instrument\": \"" << (bass ? "Bass" : "Guitar") << "\",\n";
        ss << "  \"blocks\": [\n";
        ss << "    {\"slot\": \"COMP\", \"model\": \"Gray Comp\", \"enabled\": " << ((bass ? s.compOn : gs.compOn) ? "true" : "false") << ", \"sustain\": " << n01 (bass ? s.compSustain : gs.compSustain) << "},\n";
        ss << "    {\"slot\": \"DIST\", \"model\": \"" << (bass ? "Dark Drive" : "Green JRC-OD") << "\", \"enabled\": " << ((bass ? (s.xvOn || s.bddiOn) : (gs.driveOn || gs.distOn)) ? "true" : "false") << ", \"drive\": " << n01 (bass ? s.xvDrive : gs.driveGain) << "},\n";
        ss << "    {\"slot\": \"AMP\", \"model\": \"" << (bass ? "78 Ampeg SVT" : "68 Plexi 100W") << "\", \"enabled\": " << ((bass ? !s.headBypass : !gs.headBypass) ? "true" : "false") << ", \"gain\": " << n01 (bass ? s.ampGain : gs.chGain[static_cast<size_t> (ch)]) << ", \"bass\": " << n01 (bass ? s.ampBass : gs.chBass[static_cast<size_t> (ch)]) << ", \"mid\": " << n01 (bass ? s.ampMid : gs.chMid[static_cast<size_t> (ch)]) << ", \"treble\": " << n01 (bass ? s.ampTreble : gs.chTreble[static_cast<size_t> (ch)]) << "},\n";
        ss << "    {\"slot\": \"CAB\", \"model\": \"" << (bass ? "8x10 Bass Cab" : "4x12 Greenback") << "\", \"enabled\": " << ((bass ? s.ampCabOn : (gs.cabIndex < 4)) ? "true" : "false") << "},\n";
        ss << "    {\"slot\": \"DELAY\", \"model\": \"Tape Echo\", \"enabled\": " << ((bass ? s.delayOn : gs.delayOn) ? "true" : "false") << ", \"mix\": " << n01 (bass ? s.delayMix : gs.delayMix) << "},\n";
        ss << "    {\"slot\": \"REVERB\", \"model\": \"Eleven Reverb\", \"enabled\": " << ((bass ? s.reverbOn : gs.reverbOn) ? "true" : "false") << ", \"mix\": " << n01 (bass ? s.reverbMix : gs.reverbMix) << "}\n";
        ss << "  ]\n}\n";
        return writeTextFile (outFile, juce::String::fromUTF8 (ss.str().c_str()));
    }

    // -------------------------------------------------------------------------
    // Export 11: MI AMP (.miamp XML)
    // -------------------------------------------------------------------------
    static bool exportMiAmpXml (MiAmpAudioProcessor& proc, const juce::String& presetName, const juce::File& outFile)
    {
        return writeTextFile (outFile, proc.exportStateAsXmlString (presetName));
    }
};

} // namespace mipedals
