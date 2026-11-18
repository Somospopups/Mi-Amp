#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MiAmpGlassTheme.h"
#include "RigStageComponents.h"
#include <cmath>

namespace miamp_rig
{
//==============================================================================
// Vector rotary knob for STUDIO FX modal (crisp warm-gold/mint dial)
class StudioMiniKnob final : public juce::Component
{
public:
    StudioMiniKnob (juce::AudioProcessorValueTreeState& s,
                    const char* paramIdIn,
                    const juce::String& labelIn,
                    const juce::String& unitIn = "",
                    juce::Colour accentIn = juce::Colour::fromRGB (166, 223, 201))
        : apvts (s), pid (paramIdIn), label (labelIn), unit (unitIn), accent (accentIn)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        if (auto* p = apvts.getParameter (pid))
        {
            const auto r = p->getNormalisableRange();
            minV = r.start;
            maxV = r.end;
            defV = p->convertFrom0to1 (p->getDefaultValue());
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float diam = juce::jmin (b.getWidth() - 4.0f, b.getHeight() - 26.0f);
        if (diam <= 8.0f) return;

        const float cx = b.getCentreX();
        const float cy = b.getY() + diam * 0.5f + 2.0f;
        const auto* raw = apvts.getRawParameterValue (pid);
        const float val = raw ? raw->load() : defV;
        const auto* p = apvts.getParameter (pid);
        const float norm = p ? p->convertTo0to1 (val) : 0.5f;

        const float startAng = juce::degreesToRadians (-135.0f);
        const float endAng   = juce::degreesToRadians (135.0f);
        const float curAng   = startAng + norm * (endAng - startAng);

        // 11-tick dial bezel + track arc + active arc + silver knob sprite (matching preview)
        const float rTickIn  = diam * 0.44f;
        const float rTickOut = diam * 0.495f;
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 92));
        for (int ti = 0; ti <= 10; ++ti)
        {
            const float td = juce::degreesToRadians (-135.0f + ti * 27.0f);
            const float st = std::sin (td), ct = std::cos (td);
            g.drawLine (cx + st * rTickIn, cy - ct * rTickIn, cx + st * rTickOut, cy - ct * rTickOut, 1.15f);
        }

        juce::Path bgArc, valArc;
        const float rArc = diam * 0.405f;
        bgArc.addCentredArc (cx, cy, rArc, rArc, 0.0f, startAng, endAng, true);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 44));
        g.strokePath (bgArc, juce::PathStrokeType (2.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (norm > 0.012f)
        {
            valArc.addCentredArc (cx, cy, rArc, rArc, 0.0f, startAng, curAng, true);
            g.setColour (accent);
            g.strokePath (valArc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        const float spriteDiam = diam * 0.72f;
        const auto silverKnob = juce::ImageCache::getFromMemory (BinaryData::knob_aguilar_silver_png, BinaryData::knob_aguilar_silver_pngSize);
        if (silverKnob.isValid())
        {
            g.setColour (juce::Colour::fromRGBA (0, 0, 0, 115));
            g.fillEllipse (cx - spriteDiam * 0.48f, cy - spriteDiam * 0.42f + 2.2f, spriteDiam * 0.96f, spriteDiam * 0.96f);

            auto tr = juce::AffineTransform::translation (-silverKnob.getWidth() * 0.5f, -silverKnob.getHeight() * 0.5f)
                          .scaled (spriteDiam / static_cast<float> (silverKnob.getWidth()),
                                   spriteDiam / static_cast<float> (silverKnob.getHeight()))
                          .rotated (curAng)
                          .translated (cx, cy);
            g.drawImageTransformed (silverKnob, tr, false);

            if (isMouseOverOrDragging())
            {
                g.setColour (accent.withAlpha (0.48f));
                g.drawEllipse (cx - spriteDiam * 0.51f, cy - spriteDiam * 0.51f, spriteDiam * 1.02f, spriteDiam * 1.02f, 1.2f);
            }
        }
        else
        {
            const float bodyR = spriteDiam * 0.5f;
            juce::ColourGradient dg (juce::Colour::fromRGB (68, 70, 64), cx - bodyR, cy - bodyR,
                                     juce::Colour::fromRGB (24, 26, 24), cx + bodyR, cy + bodyR, false);
            g.setGradientFill (dg);
            g.fillEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f);
            g.setColour (juce::Colour::fromRGBA (255, 243, 210, 110));
            g.drawEllipse (cx - bodyR, cy - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);
            const float px = cx + std::sin (curAng) * (bodyR * 0.78f);
            const float py = cy - std::cos (curAng) * (bodyR * 0.78f);
            g.setColour (juce::Colour::fromRGB (250, 243, 227));
            g.drawLine (cx + std::sin (curAng) * (bodyR * 0.25f), cy - std::cos (curAng) * (bodyR * 0.25f), px, py, 2.0f);
        }

        // Label + numeric value
        g.setColour (juce::Colour::fromRGBA (250, 243, 227, 200));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
        g.drawText (label, b.getX(), cy + diam * 0.50f, b.getWidth(), 12.0f, juce::Justification::centred, false);

        g.setColour (accent.withAlpha (0.95f));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 9.0f, juce::Font::plain)));
        const juce::String vStr = (std::fabs (val) >= 100.0f ? juce::String (juce::roundToInt (val)) : juce::String (val, 1)) + unit;
        g.drawText (vStr, b.getX(), cy + diam * 0.50f + 11.0f, b.getWidth(), 11.0f, juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = true;
        downY = e.y;
        startNorm = apvts.getParameter (pid) ? apvts.getParameter (pid)->convertTo0to1 (apvts.getRawParameterValue (pid)->load()) : 0.5f;
        if (auto* p = apvts.getParameter (pid)) p->beginChangeGesture();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const float sens = e.mods.isShiftDown() ? 0.002f : 0.006f;
        const float nn = juce::jlimit (0.0f, 1.0f, startNorm + static_cast<float> (downY - e.y) * sens);
        if (auto* p = apvts.getParameter (pid)) p->setValueNotifyingHost (nn);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging)
        {
            dragging = false;
            if (auto* p = apvts.getParameter (pid)) p->endChangeGesture();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setParamPlain (apvts, pid, defV);
        repaint();
    }

    void setParam (const char* newId, const juce::String& newLabel)
    {
        if (pid == newId && label == newLabel) return;
        pid = newId;
        label = newLabel;
        if (auto* p = apvts.getParameter (pid))
        {
            const auto r = p->getNormalisableRange();
            minV = r.start; maxV = r.end;
            defV = p->convertFrom0to1 (p->getDefaultValue());
        }
        repaint();
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    const char* pid;
    juce::String label, unit;
    juce::Colour accent;
    float minV = 0.0f, maxV = 100.0f, defV = 50.0f, startNorm = 0.5f;
    int downY = 0;
    bool dragging = false;
};

//==============================================================================
// Interactive 5-Band Parametric EQ + Real-Time FFT Spectrum Canvas
class StudioEqCanvas final : public juce::Component
{
public:
    explicit StudioEqCanvas (MiAmpAudioProcessor& proc,
                             std::function<void(int, float, float)> onBand = nullptr)
        : processor (proc), bandCb (std::move (onBand)) {}

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour::fromRGB (18, 22, 20));
        g.fillRoundedRectangle (b, 10.0f);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 68));
        g.drawRoundedRectangle (b, 10.0f, 1.0f);

        const auto plot = b.reduced (10.0f, 10.0f);

        // Log grid lines (50, 100, 250, 500, 1k, 2k, 5k, 10k)
        static constexpr float kGridHz[8] = { 50.f, 100.f, 250.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f };
        g.setFont (juce::Font (juce::FontOptions ("Arial", 8.5f, juce::Font::plain)));
        for (float hz : kGridHz)
        {
            const float x = hzToX (hz, plot);
            g.setColour (juce::Colour::fromRGBA (255, 243, 210, 22));
            g.drawVerticalLine (juce::roundToInt (x), plot.getY(), plot.getBottom());
            g.setColour (juce::Colour::fromRGBA (250, 243, 227, 110));
            const juce::String lbl = (hz >= 1000.f) ? (juce::String (static_cast<int> (hz / 1000.f)) + "k") : juce::String (static_cast<int> (hz));
            g.drawText (lbl, x - 14.0f, plot.getBottom() - 12.0f, 28.0f, 11.0f, juce::Justification::centred);
        }

        // 0 dB / +-12 dB lines
        for (float db : { -12.0f, -6.0f, 0.0f, 6.0f, 12.0f })
        {
            const float y = dbToY (db, plot);
            g.setColour (juce::Colour::fromRGBA (255, 243, 210, db == 0.0f ? 62 : 20));
            g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
        }

        // Real-time 48-bin FFT Spectrum Analyzer
        const bool analOn = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqAnal)->load() >= 0.5f;
        if (analOn)
        {
            juce::Path specPath;
            specPath.startNewSubPath (plot.getX(), plot.getBottom());
            for (int i = 0; i < 48; ++i)
            {
                const float norm = processor.getStudioEqSpectrumBin (i);
                const float x = plot.getX() + (static_cast<float> (i) / 47.0f) * plot.getWidth();
                const float y = plot.getBottom() - norm * plot.getHeight() * 0.92f;
                specPath.lineTo (x, y);
            }
            specPath.lineTo (plot.getRight(), plot.getBottom());
            specPath.closeSubPath();
            g.setColour (juce::Colour::fromRGBA (142, 208, 180, 44));
            g.fillPath (specPath);
            g.setColour (juce::Colour::fromRGBA (166, 223, 201, 115));
            g.strokePath (specPath, juce::PathStrokeType (1.2f));
        }

        // Composite frequency response curve (HPF + 5 Peaking Bands + LPF)
        const float hpf = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqHpf)->load();
        const float lpf = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqLpf)->load();
        float freqs[5], gains[5], qs[5];
        for (int i = 0; i < 5; ++i)
        {
            freqs[i] = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqFreq[i])->load();
            gains[i] = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqGain[i])->load();
            qs[i]    = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqQ[i])->load();
        }

        juce::Path curve;
        constexpr int kSteps = 120;
        for (int s = 0; s <= kSteps; ++s)
        {
            const float normX = static_cast<float> (s) / static_cast<float> (kSteps);
            const float f = 20.0f * std::pow (1000.0f, normX); // 20 Hz .. 20 kHz
            float db = 0.0f;

            // 12 dB/oct HPF & LPF magnitude approximation
            if (hpf > 21.0f)
            {
                const float r = hpf / f;
                db -= 10.0f * std::log10 (1.0f + r * r * r * r);
            }
            if (lpf < 19800.0f)
            {
                const float r = f / lpf;
                db -= 10.0f * std::log10 (1.0f + r * r * r * r);
            }
            // 5 Bell bands
            for (int bIdx = 0; bIdx < 5; ++bIdx)
            {
                const float logRatio = std::log2 (std::max (1e-3f, f / freqs[bIdx]));
                const float bw = 1.0f / std::max (0.25f, qs[bIdx]);
                db += gains[bIdx] * std::exp (-3.2f * (logRatio * logRatio) / (bw * bw));
            }

            const float px = plot.getX() + normX * plot.getWidth();
            const float py = dbToY (juce::jlimit (-16.0f, 16.0f, db), plot);
            if (s == 0) curve.startNewSubPath (px, py);
            else        curve.lineTo (px, py);
        }

        const bool eqOn = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqOn)->load() >= 0.5f;
        g.setColour (eqOn ? juce::Colour::fromRGB (244, 210, 128) : juce::Colour::fromRGBA (200, 195, 180, 95));
        g.strokePath (curve, juce::PathStrokeType (2.2f));

        // 5 Draggable Nodes
        static const juce::Colour kNodeCols[5] = {
            juce::Colour::fromRGB (126, 255, 189),
            juce::Colour::fromRGB (136, 212, 255),
            juce::Colour::fromRGB (255, 209, 102),
            juce::Colour::fromRGB (255, 159, 104),
            juce::Colour::fromRGB (242, 132, 182)
        };
        for (int i = 0; i < 5; ++i)
        {
            const float nx = hzToX (freqs[i], plot);
            const float ny = dbToY (gains[i], plot);
            const bool sel = (i == activeNode);
            if (sel && draggingNode)
            {
                g.setColour (kNodeCols[i].withAlpha (0.85f));
                g.drawEllipse (nx - 10.5f, ny - 10.5f, 21.0f, 21.0f, 1.5f);
            }
            g.setColour (kNodeCols[i].withAlpha (eqOn ? 0.95f : 0.45f));
            g.fillEllipse (nx - (sel ? 7.5f : 6.0f), ny - (sel ? 7.5f : 6.0f), sel ? 15.0f : 12.0f, sel ? 15.0f : 12.0f);
            g.setColour (juce::Colours::white.withAlpha (sel ? 0.95f : 0.55f));
            g.drawEllipse (nx - (sel ? 7.5f : 6.0f), ny - (sel ? 7.5f : 6.0f), sel ? 15.0f : 12.0f, sel ? 15.0f : 12.0f, 1.3f);
        }

        // Inline tooltip right inside the EQ canvas while dragging a band (never over MULTI-COMP)
        if (draggingNode && activeNode >= 0 && activeNode < 5)
        {
            const float bf = freqs[activeNode];
            const float bg = gains[activeNode];
            const juce::String fStr = (bf >= 1000.0f) ? (juce::String (bf / 1000.0f, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".") + "k")
                                                      : juce::String (juce::roundToInt (bf));
            const juce::String txt = "B" + juce::String (activeNode + 1) + ": " + fStr + "Hz / "
                                   + (bg >= 0.0f ? "+" : "") + juce::String (bg, 1) + "dB";
            const float nx = hzToX (bf, plot);
            const float ny = dbToY (bg, plot);
            const float tw = 128.0f, th = 22.0f;
            const float bx = juce::jlimit (plot.getX() + 4.0f, plot.getRight() - tw - 4.0f, nx - tw * 0.5f);
            const float by = (ny > plot.getY() + 34.0f) ? (ny - th - 12.0f) : (ny + 14.0f);
            const auto pill = juce::Rectangle<float> (bx, by, tw, th);
            g.setColour (juce::Colour::fromRGBA (14, 17, 16, 236));
            g.fillRoundedRectangle (pill, 6.0f);
            g.setColour (kNodeCols[activeNode]);
            g.drawRoundedRectangle (pill, 6.0f, 1.2f);
            g.setColour (juce::Colour::fromRGB (250, 243, 227));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 10.5f, juce::Font::bold)));
            g.drawText (txt, pill, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto plot = getLocalBounds().toFloat().reduced (10.0f, 10.0f);
        activeNode = -1;
        float bestDist = 26.0f;
        for (int i = 0; i < 5; ++i)
        {
            const float fx = hzToX (processor.apvts.getRawParameterValue (MiAmpParams::sfxEqFreq[i])->load(), plot);
            const float fy = dbToY (processor.apvts.getRawParameterValue (MiAmpParams::sfxEqGain[i])->load(), plot);
            const float d = e.position.getDistanceFrom ({ fx, fy });
            if (d < bestDist) { bestDist = d; activeNode = i; }
        }
        draggingNode = (activeNode >= 0);
        if (draggingNode) updateNodeFromPt (e.position, plot);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (activeNode < 0 || ! draggingNode) return;
        const auto plot = getLocalBounds().toFloat().reduced (10.0f, 10.0f);
        updateNodeFromPt (e.position, plot);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (draggingNode)
        {
            draggingNode = false;
            repaint();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (activeNode >= 0)
        {
            setParamPlain (processor.apvts, MiAmpParams::sfxEqGain[activeNode], 0.0f);
            if (bandCb)
                bandCb (activeNode, processor.apvts.getRawParameterValue (MiAmpParams::sfxEqFreq[activeNode])->load(), 0.0f);
            repaint();
        }
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (activeNode < 0) return;
        const float curQ = processor.apvts.getRawParameterValue (MiAmpParams::sfxEqQ[activeNode])->load();
        setParamPlain (processor.apvts, MiAmpParams::sfxEqQ[activeNode], juce::jlimit (0.3f, 8.0f, curQ + wheel.deltaY * 0.6f));
        repaint();
    }

private:
    static float hzToX (float hz, juce::Rectangle<float> plot)
    {
        const float t = std::log (juce::jlimit (20.0f, 20000.0f, hz) / 20.0f) / std::log (1000.0f);
        return plot.getX() + t * plot.getWidth();
    }
    static float xToHz (float x, juce::Rectangle<float> plot)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
        return juce::jlimit (30.0f, 16000.0f, 20.0f * std::pow (1000.0f, t));
    }
    static float dbToY (float db, juce::Rectangle<float> plot)
    {
        const float t = (16.0f - juce::jlimit (-16.0f, 16.0f, db)) / 32.0f;
        return plot.getY() + t * plot.getHeight();
    }
    static float yToDb (float y, juce::Rectangle<float> plot)
    {
        const float t = juce::jlimit (0.0f, 1.0f, (y - plot.getY()) / juce::jmax (1.0f, plot.getHeight()));
        return juce::jlimit (-15.0f, 15.0f, 16.0f - t * 32.0f);
    }

    void updateNodeFromPt (juce::Point<float> pt, juce::Rectangle<float> plot)
    {
        const float f = xToHz (pt.x, plot);
        const float gDb = yToDb (pt.y, plot);
        setParamPlain (processor.apvts, MiAmpParams::sfxEqFreq[activeNode], f);
        setParamPlain (processor.apvts, MiAmpParams::sfxEqGain[activeNode], gDb);
        if (bandCb) bandCb (activeNode, f, gDb);
    }

    MiAmpAudioProcessor& processor;
    std::function<void(int, float, float)> bandCb;
    int activeNode = 2;
    bool draggingNode = false;
};

//==============================================================================
// Full Post-Amp Stereo STUDIO FX Modal (Multi-Comp, Studio EQ, Digital Delay, Digital Reverb)
class StudioFxModal final : public juce::Component
{
public:
    StudioFxModal (MiAmpAudioProcessor& proc, std::function<void()> onClose)
        : processor (proc), closeCb (std::move (onClose)),
          eqCanvas (proc, [this] (int bIdx, float freqHz, float gainDb)
          {
              eqReadoutBand = bIdx;
              eqReadoutText = "B" + juce::String (bIdx + 1) + " \u00b7 " + juce::String (juce::roundToInt (freqHz))
                            + "Hz \u00b7 " + (gainDb >= 0.0f ? "+" : "") + juce::String (gainDb, 1) + "dB";
              repaint();
          })
    {
        btnClose.setComponentID ("popoverClose");
        btnClose.onClick = [this] { if (closeCb) closeCb(); };
        addAndMakeVisible (btnClose);

        auto bindToggle = [this] (juce::TextButton& b, const char* id)
        {
            b.setClickingTogglesState (true);
            b.onClick = [this, &b, id] { setParamPlain (processor.apvts, id, b.getToggleState() ? 1.0f : 0.0f); repaint(); };
            addAndMakeVisible (b);
        };

        // 1. MULTI-COMP controls
        bindToggle (btnCompOn, MiAmpParams::sfxCompOn);
        compMixKnob  = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompPar, "MIX", "%");
        compXLoKnob  = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompXLow, "X-LOW", "Hz");
        compXHiKnob  = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompXHigh, "X-HI", "Hz");
        addAndMakeVisible (*compMixKnob); addAndMakeVisible (*compXLoKnob); addAndMakeVisible (*compXHiKnob);
        for (int b = 0; b < 3; ++b)
        {
            btnCompBOn[b].setButtonText ("ON");
            btnCompBSolo[b].setButtonText ("S");
            bindToggle (btnCompBOn[b], MiAmpParams::sfxCompBOn[b]);
            bindToggle (btnCompBSolo[b], MiAmpParams::sfxCompBSol[b]);
            compThr[b] = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompBThr[b], "THR", "dB");
            compRat[b] = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompBRat[b], "RAT", ":1");
            compAtt[b] = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompBAtt[b], "ATT", "ms");
            compRel[b] = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompBRel[b], "REL", "ms");
            compGn[b]  = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxCompBGn[b],  "GAIN", "dB");
            addAndMakeVisible (*compThr[b]); addAndMakeVisible (*compRat[b]);
            addAndMakeVisible (*compAtt[b]); addAndMakeVisible (*compRel[b]);
            addAndMakeVisible (*compGn[b]);
        }

        // 2. STUDIO EQ controls
        bindToggle (btnEqOn, MiAmpParams::sfxEqOn);
        bindToggle (btnEqAnal, MiAmpParams::sfxEqAnal);
        addAndMakeVisible (eqCanvas);
        eqHpfKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxEqHpf, "HPF", "Hz", juce::Colour::fromRGB (244, 210, 128));
        eqLpfKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxEqLpf, "LPF", "Hz", juce::Colour::fromRGB (244, 210, 128));
        eqOutKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxEqOut, "OUT", "dB", juce::Colour::fromRGB (244, 210, 128));
        addAndMakeVisible (*eqHpfKnob); addAndMakeVisible (*eqLpfKnob); addAndMakeVisible (*eqOutKnob);

        // 3. DIGITAL DELAY controls
        bindToggle (btnDlyOn, MiAmpParams::sfxDlyOn);
        bindToggle (btnDlyPP, MiAmpParams::sfxDlyPP);
        bindToggle (btnDlySync, MiAmpParams::sfxDlySync);
        btnDlySubdiv.onClick = [this]
        {
            const int cur = static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::sfxDlySub)->load()));
            setParamPlain (processor.apvts, MiAmpParams::sfxDlySub, static_cast<float> ((cur + 1) % 5));
            syncControls();
        };
        addAndMakeVisible (btnDlySubdiv);
        dlyTimeKnob  = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxDlyTime,  "TIME", "ms", juce::Colour::fromRGB (134, 196, 240));
        dlyFbKnob    = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxDlyFb,    "F.BACK", "%", juce::Colour::fromRGB (134, 196, 240));
        dlyLoCutKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxDlyLoCut, "LO-CUT", "Hz", juce::Colour::fromRGB (134, 196, 240));
        dlyHiCutKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxDlyHiCut, "HI-CUT", "Hz", juce::Colour::fromRGB (134, 196, 240));
        dlyMixKnob   = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxDlyMix,   "MIX", "%", juce::Colour::fromRGB (134, 196, 240));
        addAndMakeVisible (*dlyTimeKnob); addAndMakeVisible (*dlyFbKnob);
        addAndMakeVisible (*dlyLoCutKnob); addAndMakeVisible (*dlyHiCutKnob); addAndMakeVisible (*dlyMixKnob);

        // 4. DIGITAL REVERB controls
        bindToggle (btnRevOn, MiAmpParams::sfxRevOn);
        static constexpr const char* kRevModeNames[4] = { "PLATE", "HALL", "ROOM", "AMBIENT" };
        for (int m = 0; m < 4; ++m)
        {
            btnRevMode[m].setButtonText (kRevModeNames[m]);
            btnRevMode[m].onClick = [this, m]
            {
                setParamPlain (processor.apvts, MiAmpParams::sfxRevMode, static_cast<float> (m));
                syncControls();
            };
            addAndMakeVisible (btnRevMode[m]);
        }
        revPreKnob   = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxRevPreDly, "PRE-DLY", "ms", juce::Colour::fromRGB (214, 158, 240));
        revDecayKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxRevDecay,  "DECAY", "%", juce::Colour::fromRGB (214, 158, 240));
        revDampKnob  = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxRevDamp,   "DAMP", "%", juce::Colour::fromRGB (214, 158, 240));
        revWidthKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxRevWidth,  "WIDTH", "%", juce::Colour::fromRGB (214, 158, 240));
        revMixKnob   = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::sfxRevMix,    "MIX", "%", juce::Colour::fromRGB (214, 158, 240));
        addAndMakeVisible (*revPreKnob); addAndMakeVisible (*revDecayKnob);
        addAndMakeVisible (*revDampKnob); addAndMakeVisible (*revWidthKnob); addAndMakeVisible (*revMixKnob);
    }

    void syncControls()
    {
        auto g = [this] (const char* id) { return processor.apvts.getRawParameterValue (id)->load(); };
        btnCompOn.setToggleState  (g (MiAmpParams::sfxCompOn) >= 0.5f, juce::dontSendNotification);
        btnEqOn.setToggleState    (g (MiAmpParams::sfxEqOn)   >= 0.5f, juce::dontSendNotification);
        btnEqAnal.setToggleState  (g (MiAmpParams::sfxEqAnal) >= 0.5f, juce::dontSendNotification);
        btnDlyOn.setToggleState   (g (MiAmpParams::sfxDlyOn)  >= 0.5f, juce::dontSendNotification);
        btnDlyPP.setToggleState   (g (MiAmpParams::sfxDlyPP)  >= 0.5f, juce::dontSendNotification);
        btnDlySync.setToggleState (g (MiAmpParams::sfxDlySync)>= 0.5f, juce::dontSendNotification);
        btnRevOn.setToggleState   (g (MiAmpParams::sfxRevOn)  >= 0.5f, juce::dontSendNotification);

        for (int b = 0; b < 3; ++b)
        {
            btnCompBOn[b].setToggleState   (g (MiAmpParams::sfxCompBOn[b])  >= 0.5f, juce::dontSendNotification);
            btnCompBSolo[b].setToggleState (g (MiAmpParams::sfxCompBSol[b]) >= 0.5f, juce::dontSendNotification);
        }

        static constexpr const char* kSubNames[5] = { "1/4", "1/8.", "1/8", "1/8T", "1/16" };
        const int sub = juce::jlimit (0, 4, static_cast<int> (std::lround (g (MiAmpParams::sfxDlySub))));
        btnDlySubdiv.setButtonText (juce::String ("SYNC: ") + kSubNames[sub]);

        const int rMode = juce::jlimit (0, 3, static_cast<int> (std::lround (g (MiAmpParams::sfxRevMode))));
        for (int m = 0; m < 4; ++m)
            btnRevMode[m].setToggleState (m == rMode, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour::fromRGBA (10, 12, 11, 165));
        g.fillRect (b);

        const auto card = b.reduced (b.getWidth() * 0.045f, b.getHeight() * 0.072f);
        miamp_theme::paintFrostedGlassPanel (g, card, 22.0f);

        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 19.0f, juce::Font::bold)));
        g.drawText ("MI STUDIO FX  -  POST-AMP STEREO MASTERING & AMBIENCE",
                    card.getX() + 24.0f, card.getY() + 12.0f, card.getWidth() - 90.0f, 28.0f,
                    juce::Justification::centredLeft);

        auto panels = computePanels (card);
        static const char* kTitles[4] = { "MI MULTI-COMP  (3-BAND)", "MI STUDIO EQ",
                                          "MI DIGITAL DELAY",        "MI DIGITAL REVERB" };
        for (int p = 0; p < 4; ++p)
        {
            const auto& pr = panels[static_cast<size_t> (p)];
            miamp_theme::paintGlassSubPanel (g, pr, 14.0f);
            g.setColour (juce::Colour::fromRGB (189, 238, 208));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 13.5f, juce::Font::bold)));
            g.drawText (kTitles[p], pr.getX() + 16.0f, pr.getY() + 9.0f, 150.0f, 22.0f,
                        juce::Justification::centredLeft);
        }

        // Live EQ band readout pill inside MI STUDIO EQ header (never overlaps MI MULTI-COMP)
        if (eqReadoutText.isNotEmpty() && eqReadoutBand >= 0 && eqReadoutBand < 5)
        {
            static const juce::Colour kEqCols[5] = {
                juce::Colour::fromRGB (126, 255, 189),
                juce::Colour::fromRGB (136, 212, 255),
                juce::Colour::fromRGB (255, 209, 102),
                juce::Colour::fromRGB (255, 159, 104),
                juce::Colour::fromRGB (242, 132, 182)
            };
            const auto& eR = panels[1];
            const auto pill = juce::Rectangle<float> (eR.getX() + 148.0f, eR.getY() + 9.0f, 184.0f, 22.0f);
            g.setColour (juce::Colour::fromRGBA (12, 16, 15, 220));
            g.fillRoundedRectangle (pill, 6.0f);
            g.setColour (kEqCols[eqReadoutBand].withAlpha (0.75f));
            g.drawRoundedRectangle (pill, 6.0f, 1.0f);
            g.setColour (juce::Colour::fromRGB (250, 243, 227));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 10.5f, juce::Font::bold)));
            g.drawText (eqReadoutText, pill, juce::Justification::centred);
        }

        // Draw live GR meters inside MI MULTI-COMP panel
        const auto& compR = panels[0];
        const float colW = (compR.getWidth() - 120.0f) / 3.0f;
        static const char* kBands[3] = { "LOW", "MID", "HIGH" };
        for (int i = 0; i < 3; ++i)
        {
            const float bx = compR.getX() + 14.0f + i * colW;
            const float by = compR.getY() + 42.0f;
            const auto bandBox = juce::Rectangle<float> (bx, by, colW - 8.0f, compR.getHeight() - 54.0f);
            g.setColour (juce::Colour::fromRGBA (255, 255, 255, 14));
            g.fillRoundedRectangle (bandBox, 8.0f);
            g.setColour (juce::Colour::fromRGB (166, 223, 201));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 11.5f, juce::Font::bold)));
            g.drawText (kBands[i], bandBox.getX() + 8.0f, bandBox.getY() + 5.0f, 44.0f, 16.0f, juce::Justification::centredLeft);

            // GR meter bar
            const float grDb = processor.getStudioCompGrDb (i);
            const float grFrac = juce::jlimit (0.0f, 1.0f, grDb / 18.0f);
            const auto grTrack = juce::Rectangle<float> (bandBox.getRight() - 14.0f, bandBox.getY() + 28.0f, 7.0f, bandBox.getHeight() - 36.0f);
            g.setColour (juce::Colour::fromRGBA (10, 12, 11, 210));
            g.fillRoundedRectangle (grTrack, 3.0f);
            if (grFrac > 0.01f)
            {
                const float hFill = grTrack.getHeight() * grFrac;
                g.setColour (juce::Colour::fromRGB (244, 162, 92));
                g.fillRoundedRectangle (grTrack.getX(), grTrack.getY(), grTrack.getWidth(), hFill, 3.0f);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto card = b.reduced (b.getWidth() * 0.045f, b.getHeight() * 0.072f);
        if (! card.contains (e.position) && closeCb)
            closeCb();
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        const auto card = b.reduced (b.getWidth() * 0.045f, b.getHeight() * 0.072f);
        btnClose.setBounds (juce::roundToInt (card.getRight() - 42.0f), juce::roundToInt (card.getY() + 11.0f), 30, 30);

        auto panels = computePanels (card);

        // 1. MULTI-COMP
        const auto& cR = panels[0];
        btnCompOn.setBounds (juce::roundToInt (cR.getRight() - 72.0f), juce::roundToInt (cR.getY() + 8.0f), 58, 24);
        const float colW = (cR.getWidth() - 120.0f) / 3.0f;
        for (int i = 0; i < 3; ++i)
        {
            const float bx = cR.getX() + 14.0f + i * colW;
            const float by = cR.getY() + 42.0f;
            const float bw = colW - 22.0f;
            const float bh = cR.getHeight() - 54.0f;
            btnCompBOn[i].setBounds   (juce::roundToInt (bx + bw - 54.0f), juce::roundToInt (by + 4.0f), 26, 18);
            btnCompBSolo[i].setBounds (juce::roundToInt (bx + bw - 25.0f), juce::roundToInt (by + 4.0f), 22, 18);

            const float kw = bw * 0.48f;
            const float kh = (bh - 28.0f) * 0.33f;
            compThr[i]->setBounds (juce::roundToInt (bx + 2.0f),        juce::roundToInt (by + 24.0f),            juce::roundToInt (kw), juce::roundToInt (kh));
            compRat[i]->setBounds (juce::roundToInt (bx + 2.0f + kw),   juce::roundToInt (by + 24.0f),            juce::roundToInt (kw), juce::roundToInt (kh));
            compAtt[i]->setBounds (juce::roundToInt (bx + 2.0f),        juce::roundToInt (by + 24.0f + kh),       juce::roundToInt (kw), juce::roundToInt (kh));
            compRel[i]->setBounds (juce::roundToInt (bx + 2.0f + kw),   juce::roundToInt (by + 24.0f + kh),       juce::roundToInt (kw), juce::roundToInt (kh));
            compGn[i]->setBounds  (juce::roundToInt (bx + bw * 0.25f),  juce::roundToInt (by + 24.0f + kh * 2.0f),juce::roundToInt (kw), juce::roundToInt (kh));
        }
        const float masterX = cR.getRight() - 96.0f;
        const float masterH = (cR.getHeight() - 48.0f) / 3.0f;
        compXLoKnob->setBounds (juce::roundToInt (masterX), juce::roundToInt (cR.getY() + 40.0f),                  84, juce::roundToInt (masterH));
        compXHiKnob->setBounds (juce::roundToInt (masterX), juce::roundToInt (cR.getY() + 40.0f + masterH),        84, juce::roundToInt (masterH));
        compMixKnob->setBounds (juce::roundToInt (masterX), juce::roundToInt (cR.getY() + 40.0f + masterH * 2.0f), 84, juce::roundToInt (masterH));

        // 2. STUDIO EQ
        const auto& eR = panels[1];
        btnEqAnal.setBounds (juce::roundToInt (eR.getRight() - 152.0f), juce::roundToInt (eR.getY() + 8.0f), 72, 24);
        btnEqOn.setBounds   (juce::roundToInt (eR.getRight() - 72.0f),  juce::roundToInt (eR.getY() + 8.0f), 58, 24);
        eqCanvas.setBounds  (juce::roundToInt (eR.getX() + 14.0f), juce::roundToInt (eR.getY() + 42.0f),
                             juce::roundToInt (eR.getWidth() - 114.0f), juce::roundToInt (eR.getHeight() - 56.0f));
        const float eqKx = eR.getRight() - 92.0f;
        const float eqKh = (eR.getHeight() - 48.0f) / 3.0f;
        eqHpfKnob->setBounds (juce::roundToInt (eqKx), juce::roundToInt (eR.getY() + 40.0f),              80, juce::roundToInt (eqKh));
        eqLpfKnob->setBounds (juce::roundToInt (eqKx), juce::roundToInt (eR.getY() + 40.0f + eqKh),       80, juce::roundToInt (eqKh));
        eqOutKnob->setBounds (juce::roundToInt (eqKx), juce::roundToInt (eR.getY() + 40.0f + eqKh * 2.0f),80, juce::roundToInt (eqKh));

        // 3. DIGITAL DELAY
        const auto& dR = panels[2];
        btnDlyOn.setBounds     (juce::roundToInt (dR.getRight() - 72.0f),  juce::roundToInt (dR.getY() + 8.0f), 58, 24);
        btnDlyPP.setBounds     (juce::roundToInt (dR.getX() + 16.0f),      juce::roundToInt (dR.getY() + 42.0f), 96, 24);
        btnDlySync.setBounds   (juce::roundToInt (dR.getX() + 120.0f),     juce::roundToInt (dR.getY() + 42.0f), 72, 24);
        btnDlySubdiv.setBounds (juce::roundToInt (dR.getX() + 200.0f),     juce::roundToInt (dR.getY() + 42.0f), 110, 24);
        const float dkW = (dR.getWidth() - 32.0f) / 5.0f;
        const float dkY = dR.getY() + 74.0f;
        const float dkH = dR.getBottom() - dkY - 10.0f;
        dlyTimeKnob->setBounds  (juce::roundToInt (dR.getX() + 16.0f + dkW * 0.0f), juce::roundToInt (dkY), juce::roundToInt (dkW), juce::roundToInt (dkH));
        dlyFbKnob->setBounds    (juce::roundToInt (dR.getX() + 16.0f + dkW * 1.0f), juce::roundToInt (dkY), juce::roundToInt (dkW), juce::roundToInt (dkH));
        dlyLoCutKnob->setBounds (juce::roundToInt (dR.getX() + 16.0f + dkW * 2.0f), juce::roundToInt (dkY), juce::roundToInt (dkW), juce::roundToInt (dkH));
        dlyHiCutKnob->setBounds (juce::roundToInt (dR.getX() + 16.0f + dkW * 3.0f), juce::roundToInt (dkY), juce::roundToInt (dkW), juce::roundToInt (dkH));
        dlyMixKnob->setBounds   (juce::roundToInt (dR.getX() + 16.0f + dkW * 4.0f), juce::roundToInt (dkY), juce::roundToInt (dkW), juce::roundToInt (dkH));

        // 4. DIGITAL REVERB
        const auto& rR = panels[3];
        btnRevOn.setBounds (juce::roundToInt (rR.getRight() - 72.0f), juce::roundToInt (rR.getY() + 8.0f), 58, 24);
        const float rmW = (rR.getWidth() - 50.0f) / 4.0f;
        for (int m = 0; m < 4; ++m)
            btnRevMode[m].setBounds (juce::roundToInt (rR.getX() + 16.0f + m * (rmW + 6.0f)), juce::roundToInt (rR.getY() + 42.0f),
                                     juce::roundToInt (rmW), 24);
        const float rkW = (rR.getWidth() - 32.0f) / 5.0f;
        const float rkY = rR.getY() + 74.0f;
        const float rkH = rR.getBottom() - rkY - 10.0f;
        revPreKnob->setBounds   (juce::roundToInt (rR.getX() + 16.0f + rkW * 0.0f), juce::roundToInt (rkY), juce::roundToInt (rkW), juce::roundToInt (rkH));
        revDecayKnob->setBounds (juce::roundToInt (rR.getX() + 16.0f + rkW * 1.0f), juce::roundToInt (rkY), juce::roundToInt (rkW), juce::roundToInt (rkH));
        revDampKnob->setBounds  (juce::roundToInt (rR.getX() + 16.0f + rkW * 2.0f), juce::roundToInt (rkY), juce::roundToInt (rkW), juce::roundToInt (rkH));
        revWidthKnob->setBounds (juce::roundToInt (rR.getX() + 16.0f + rkW * 3.0f), juce::roundToInt (rkY), juce::roundToInt (rkW), juce::roundToInt (rkH));
        revMixKnob->setBounds   (juce::roundToInt (rR.getX() + 16.0f + rkW * 4.0f), juce::roundToInt (rkY), juce::roundToInt (rkW), juce::roundToInt (rkH));
    }

private:
    std::array<juce::Rectangle<float>, 4> computePanels (juce::Rectangle<float> card) const
    {
        auto area = card.reduced (18.0f, 14.0f).withTrimmedTop (32.0f);
        const float gap = 12.0f;
        const float pw = (area.getWidth() - gap) * 0.5f;
        const float topH = (area.getHeight() - gap) * 0.625f;
        const float botH = area.getHeight() - gap - topH;
        return {
            juce::Rectangle<float> (area.getX(),            area.getY(),            pw, topH),
            juce::Rectangle<float> (area.getX() + pw + gap, area.getY(),            pw, topH),
            juce::Rectangle<float> (area.getX(),            area.getY() + topH + gap, pw, botH),
            juce::Rectangle<float> (area.getX() + pw + gap, area.getY() + topH + gap, pw, botH)
        };
    }

    MiAmpAudioProcessor& processor;
    std::function<void()> closeCb;
    juce::String eqReadoutText;
    int eqReadoutBand = -1;
    juce::TextButton btnClose { juce::String::fromUTF8 (u8"\u00d7") };

    juce::TextButton btnCompOn { "ON" }, btnCompBOn[3], btnCompBSolo[3];
    std::unique_ptr<StudioMiniKnob> compMixKnob, compXLoKnob, compXHiKnob;
    std::unique_ptr<StudioMiniKnob> compThr[3], compRat[3], compAtt[3], compRel[3], compGn[3];

    juce::TextButton btnEqOn { "ON" }, btnEqAnal { "FFT" };
    StudioEqCanvas eqCanvas;
    std::unique_ptr<StudioMiniKnob> eqHpfKnob, eqLpfKnob, eqOutKnob;

    juce::TextButton btnDlyOn { "ON" }, btnDlyPP { "PING-PONG" }, btnDlySync { "SYNC" }, btnDlySubdiv { "SYNC: 1/8" };
    std::unique_ptr<StudioMiniKnob> dlyTimeKnob, dlyFbKnob, dlyLoCutKnob, dlyHiCutKnob, dlyMixKnob;

    juce::TextButton btnRevOn { "ON" };
    juce::TextButton btnRevMode[4];
    std::unique_ptr<StudioMiniKnob> revPreKnob, revDecayKnob, revDampKnob, revWidthKnob, revMixKnob;
};
} // namespace miamp_rig
