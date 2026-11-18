#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "PedalCatalogSpecs.h"
#include "PresetExporter.h"
#include "StandaloneAudioBridge.h"
#include <BinaryData.h>
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kStageW = 1550.0f;
    constexpr float kStageH = 768.0f;
    constexpr float kAmpZoom = 1.17f;
    constexpr float kAmpOriginX = 625.0f;
    constexpr float kAmpOriginY = 325.0f;

    // Bass Head (5 knobs, 3 switches)
    constexpr float kBassHeadKnobX[5] = { 398.0f, 483.0f, 568.0f, 653.0f, 738.0f };
    constexpr float kBassHeadKnobY    = 410.0f;
    constexpr const char* kBassHeadKnobIds[5] = {
        MiAmpParams::ampGain, MiAmpParams::ampBass, MiAmpParams::ampMid,
        MiAmpParams::ampFreq, MiAmpParams::ampTreble
    };
    constexpr const char* kBassHeadKnobNames[5] = { "GAIN", "BASS", "MID", "FREQ", "TREBLE" };

    constexpr float kBassSwitchX[3] = { 258.0f, 323.0f, 822.0f };
    constexpr float kBassSwitchY    = 406.0f;
    constexpr const char* kBassSwitchIds[3] = {
        MiAmpParams::ampPower, MiAmpParams::ampUltraHi, MiAmpParams::ampStandby
    };
    constexpr const char* kBassSwitchNames[3] = { "POWER", "ULTRA", "STANDBY" };
    constexpr bool kBassSwitchInvert[3] = { true, false, false };

    // Guitar Head (main_bg.png: 19 knobs, 3 switches)
    constexpr float kGtrXs[9] = { 385.0f, 445.0f, 505.0f, 593.0f, 653.0f, 712.0f, 778.0f, 838.0f, 897.0f };
    constexpr float kGtrScX = 744.0f / 894.0f;
    constexpr float kGtrScY = 416.0f / 490.0f;

    constexpr float kGtrSwitchX[3]    = { 245.5f, 290.0f, 485.0f };
    constexpr float kGtrSwitchY[3]    = { 394.0f, 394.0f, 400.0f };
    constexpr float kGtrSwitchSize[3] = { 28.0f,  28.0f,  18.0f };
    constexpr const char* kGtrSwitchIds[3] = {
        MiAmpParams::gtrPower, MiAmpParams::gtrStandby, MiAmpParams::gtrModeBright
    };
    constexpr const char* kGtrSwitchNames[3] = { "POWER", "STANDBY", "BRIGHT CAP" };
    constexpr bool kGtrSwitchInvert[3] = { true, false, false };

    // XV Bass (1391x1131)
    constexpr float kXvPoints[7][2] = {
        { 0.19482f, 0.196f }, { 0.35226f, 0.196f }, { 0.50683f, 0.196f },
        { 0.66139f, 0.196f }, { 0.81668f, 0.196f },
        { 0.43422f, 0.407f }, { 0.59526f, 0.407f }
    };
    constexpr const char* kXvKnobIds[7] = {
        MiAmpParams::xvLevel, MiAmpParams::xvDrive, MiAmpParams::xvLow,
        MiAmpParams::xvHigh,  MiAmpParams::xvBlend, MiAmpParams::xvMid, MiAmpParams::xvPresence
    };
    constexpr const char* kXvKnobNames[7] = {
        "LEVEL", "DRIVE", "LOW", "HIGH", "BLEND", "MID", "PRESENCE"
    };

    static float knobDeg (float val) noexcept { return -130.0f + juce::jlimit (0.0f, 100.0f, val) * 2.6f; }

    static juce::Point<float> ampZoomPt (float xStage, float yStage) noexcept
    {
        return {
            kAmpOriginX + (xStage - kAmpOriginX) * kAmpZoom,
            kAmpOriginY + (yStage - kAmpOriginY) * kAmpZoom
        };
    }

    static juce::Colour inkColour()  { return juce::Colour::fromRGB (247, 241, 223); }
    static juce::Colour mintBorder() { return juce::Colour::fromRGB (191, 252, 225); }

    static juce::Font sansFont (float size, int style = juce::Font::plain)
    {
        return juce::Font (juce::FontOptions ("Arial", size, style));
    }

    static float cubicBezierIntro (float t) noexcept
    {
        // Smooth approximation of CSS cubic-bezier(.45, 0, .2, 1)
        const float u = juce::jlimit (0.0f, 1.0f, t);
        return u * u * (3.0f - 2.0f * u) * 0.35f + (1.0f - std::pow (1.0f - u, 2.4f)) * 0.65f;
    }

    static float silkEase (float t) noexcept
    {
        // Smooth silk ease matching CSS cubic-bezier(0.25, 0.85, 0.32, 1)
        const float u = juce::jlimit (0.0f, 1.0f, t);
        const float inv = 1.0f - u;
        return 1.0f - inv * inv * inv;
    }
}

juce::String MiAmpAudioProcessorEditor::formatNoteNameOnly (const char* noteName, bool useSolfege)
{
    if (noteName == nullptr || noteName[0] == '\0')
        return "--";
    juce::String s (noteName);
    if (! useSolfege)
        return s;

    const bool sharp = s.containsChar ('#');
    const char letter = s[0];
    juce::String base;
    switch (letter)
    {
        case 'C': base = "Do";  break;
        case 'D': base = "Re";  break;
        case 'E': base = "Mi";  break;
        case 'F': base = "Fa";  break;
        case 'G': base = "Sol"; break;
        case 'A': base = "La";  break;
        case 'B': base = "Si";  break;
        default:  return s;
    }
    return sharp ? (base + "#") : base;
}

juce::String MiAmpAudioProcessorEditor::formatNoteName (const char* noteName, int octave, bool useSolfege)
{
    if (noteName == nullptr || noteName[0] == '\0')
        return "--";
    return formatNoteNameOnly (noteName, useSolfege) + juce::String (octave);
}

//==============================================================================
// GlassLookAndFeel
//==============================================================================
MiAmpAudioProcessorEditor::GlassLookAndFeel::GlassLookAndFeel()
{
    setDefaultSansSerifTypefaceName ("Arial");
}

void MiAmpAudioProcessorEditor::GlassLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto id = b.getComponentID();
    const auto r  = b.getLocalBounds().toFloat().reduced (0.5f);

    if (id == "popoverClose" || id == "popoverBack" || id == "presetLoad")
    {
        if ((id == "popoverClose" || id == "popoverBack") && (highlighted || down))
        {
            g.setColour (juce::Colour::fromRGBA (255, 255, 255, down ? 42 : 24));
            g.fillEllipse (r.reduced (1.0f));
        }
        return;
    }

    if (id == "presetDelete" || id == "presetExport")
    {
        const float diam = juce::jmin (r.getWidth(), r.getHeight());
        const auto cr = r.withSizeKeepingCentre (diam, diam);
        const bool isDel = (id == "presetDelete");
        if (highlighted || down)
        {
            g.setColour (isDel ? juce::Colour::fromRGBA (198, 120, 97, down ? 140 : 102)
                               : juce::Colour::fromRGBA (142, 208, 180, down ? 128 : 90));
            g.fillEllipse (cr);
            g.setColour (isDel ? juce::Colour::fromRGB (255, 222, 209)
                               : juce::Colour::fromRGB (222, 255, 238));
            g.drawEllipse (cr, 1.1f);
        }
        else
        {
            g.setColour (juce::Colour::fromRGBA (255, 255, 255, 11));
            g.fillEllipse (cr);
            g.setColour (juce::Colour::fromRGBA (255, 244, 219, 74));
            g.drawEllipse (cr, 1.0f);
        }
        return;
    }

    if (id == "cabTabCabs" || id == "cabTabMics")
    {
        const bool active = b.getToggleState();
        const float rad = r.getHeight() * 0.5f;
        if (active)
        {
            g.setColour (juce::Colour::fromRGBA (166, 223, 201, highlighted ? 105 : 88));
            g.fillRoundedRectangle (r, rad);
            g.setColour (juce::Colour::fromRGBA (255, 255, 255, 90));
            g.drawRoundedRectangle (r, rad, 1.0f);
        }
        else if (highlighted || down)
        {
            g.setColour (juce::Colour::fromRGBA (255, 250, 236, 36));
            g.fillRoundedRectangle (r, rad);
        }
        return;
    }

    const bool active = b.getToggleState();

    if (active)
    {
        g.setColour (juce::Colour::fromRGBA (154, 216, 192, down ? 128 : (highlighted ? 116 : 92)));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (mintBorder());
        g.drawRoundedRectangle (r, 8.0f, 1.2f);
    }
    else if (highlighted || down)
    {
        g.setColour (juce::Colour::fromRGBA (232, 214, 169, down ? 120 : 96));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.drawRoundedRectangle (r, 8.0f, 1.1f);
    }
    else
    {
        juce::ColourGradient grad (juce::Colour::fromRGBA (255, 252, 242, 41), r.getX(), r.getY(),
                                   juce::Colour::fromRGBA (255, 252, 242, 15), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour::fromRGBA (248, 232, 198, 115));
        g.drawRoundedRectangle (r, 8.0f, 1.0f);
    }
}

void MiAmpAudioProcessorEditor::GlassLookAndFeel::drawButtonText (
    juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    const auto id = b.getComponentID();
    const auto bounds = b.getLocalBounds();

    if (id == "popoverClose")
    {
        g.setFont (sansFont (juce::jlimit (16.0f, 24.0f, static_cast<float> (bounds.getHeight()) * 0.85f), juce::Font::plain));
        g.setColour (juce::Colour::fromRGB (255, 247, 234));
        g.drawText (juce::String::fromUTF8 (u8"\u00d7"), bounds.toFloat(), juce::Justification::centred);
        return;
    }

    if (id == "popoverBack")
    {
        const auto bf = bounds.toFloat();
        const float cx = bf.getCentreX();
        const float cy = bf.getCentreY();
        juce::Path arrow;
        arrow.startNewSubPath (cx + 5.5f, cy);
        arrow.lineTo          (cx - 5.5f, cy);
        arrow.startNewSubPath (cx - 0.5f, cy - 5.0f);
        arrow.lineTo          (cx - 5.5f, cy);
        arrow.lineTo          (cx - 0.5f, cy + 5.0f);
        g.setColour (juce::Colour::fromRGB (255, 247, 234));
        g.strokePath (arrow, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    if (id == "presetDelete")
    {
        // Crisp vector trash can icon for deleting saved presets
        const auto r = bounds.toFloat();
        const float cx = r.getCentreX();
        const float cy = r.getCentreY();
        const float u  = juce::jmin (r.getWidth(), r.getHeight()) * 0.036f;
        const float lw = juce::jmax (1.2f, 1.4f * u);
        g.setColour (juce::Colour::fromRGB (255, 235, 224));
        g.drawLine (cx - 5.4f * u, cy - 3.2f * u, cx + 5.4f * u, cy - 3.2f * u, lw);
        g.drawRect (cx - 2.2f * u, cy - 5.4f * u, 4.4f * u, 2.2f * u, lw * 0.85f);
        g.drawLine (cx - 4.0f * u, cy - 3.2f * u, cx - 3.2f * u, cy + 5.4f * u, lw);
        g.drawLine (cx + 4.0f * u, cy - 3.2f * u, cx + 3.2f * u, cy + 5.4f * u, lw);
        g.drawLine (cx - 3.2f * u, cy + 5.4f * u, cx + 3.2f * u, cy + 5.4f * u, lw);
        g.drawLine (cx - 1.3f * u, cy - 1.0f * u, cx - 1.3f * u, cy + 3.4f * u, lw * 0.85f);
        g.drawLine (cx + 1.3f * u, cy - 1.0f * u, cx + 1.3f * u, cy + 3.4f * u, lw * 0.85f);
        return;
    }

    if (id == "presetExport")
    {
        // Crisp vector download tray + arrow icon
        const auto r = bounds.toFloat();
        const float cx = r.getCentreX();
        const float cy = r.getCentreY();
        const float u  = juce::jmin (r.getWidth(), r.getHeight()) * 0.036f;
        const float lw = juce::jmax (1.3f, 1.5f * u);
        g.setColour (juce::Colour::fromRGB (228, 255, 240));
        g.drawLine (cx, cy - 5.4f * u, cx, cy + 2.2f * u, lw);
        g.drawLine (cx - 3.2f * u, cy - 0.8f * u, cx, cy + 2.5f * u, lw);
        g.drawLine (cx + 3.2f * u, cy - 0.8f * u, cx, cy + 2.5f * u, lw);
        g.drawLine (cx - 5.2f * u, cy + 3.2f * u, cx - 5.2f * u, cy + 5.4f * u, lw);
        g.drawLine (cx - 5.2f * u, cy + 5.4f * u, cx + 5.2f * u, cy + 5.4f * u, lw);
        g.drawLine (cx + 5.2f * u, cy + 3.2f * u, cx + 5.2f * u, cy + 5.4f * u, lw);
        return;
    }

    if (id == "presetLoad")
    {
        const bool sel = b.getToggleState();
        g.setFont (sansFont (juce::jlimit (9.5f, 13.0f, static_cast<float> (bounds.getHeight()) * 0.42f),
                             sel ? juce::Font::bold : juce::Font::plain));
        g.setColour (sel ? juce::Colour::fromRGB (227, 255, 232)
                         : juce::Colour::fromRGB (255, 247, 233));
        g.drawFittedText (b.getButtonText(), bounds.reduced (4, 0), juce::Justification::centredLeft, 1);
        return;
    }

    if (id == "headMenuPresets" || id == "headMenuTuner" || id == "headMenuToneFinder" || id == "headMenuPedals"
        || id == "headMenuCabs" || id == "headMenuStudioFx" || id == "headMenuConfig")
    {
        const float bh = static_cast<float> (bounds.getHeight());
        const float sc = bh / 36.0f;
        const float hFont = juce::jlimit (9.5f, 12.5f, bh * 0.33f);
        g.setFont (sansFont (hFont, juce::Font::bold));

        // Left-aligned text & right-aligned thin icons (matching preview #right-menu .menu-btn)
        const float leftPad = 14.0f * sc;
        const float rightPad = 14.0f * sc;
        auto txtR = juce::Rectangle<float> (static_cast<float> (bounds.getX()) + leftPad,
                                            static_cast<float> (bounds.getY()),
                                            static_cast<float> (bounds.getWidth()) - leftPad - rightPad - 20.0f * sc,
                                            bh);
        if (b.isEnabled())
        {
            g.setColour (juce::Colour::fromRGBA (0, 0, 0, 165));
            g.drawFittedText (b.getButtonText(), txtR.translated (0.0f, 1.0f).toNearestInt(), juce::Justification::centredLeft, 1);
            g.setColour (juce::Colour::fromRGB (255, 250, 240));
        }
        else
        {
            g.setColour (juce::Colour::fromRGBA (255, 250, 240, 115));
        }
        g.drawFittedText (b.getButtonText(), txtR.toNearestInt(), juce::Justification::centredLeft, 1);

        const float sz = 14.0f * sc;
        const float cx = static_cast<float> (bounds.getRight()) - rightPad - sz * 0.5f;
        const float cy = bounds.toFloat().getCentreY();
        const float lw = juce::jmax (1.1f, 1.35f * sc);
        g.setColour (juce::Colour::fromRGBA (244, 234, 214, 195));

        if (id == "headMenuPresets")
        {
            const float x0 = cx - sz * 0.45f, x1 = cx + sz * 0.45f;
            const float ys[3] = { cy - sz * 0.30f, cy, cy + sz * 0.30f };
            const float kx[3] = { cx - sz * 0.15f, cx + sz * 0.18f, cx - sz * 0.06f };
            const float r = sz * 0.10f;
            for (int k = 0; k < 3; ++k)
            {
                g.drawLine (x0, ys[k], kx[k] - r, ys[k], lw);
                g.drawLine (kx[k] + r, ys[k], x1, ys[k], lw);
                g.drawEllipse (kx[k] - r, ys[k] - r, r * 2.0f, r * 2.0f, lw);
            }
        }
        else if (id == "headMenuToneFinder")
        {
            const float r = sz * 0.32f;
            const float lx = cx - sz * 0.10f, ly = cy - sz * 0.10f;
            g.drawEllipse (lx - r, ly - r, r * 2.0f, r * 2.0f, lw);
            g.drawLine (lx + r * 0.72f, ly + r * 0.72f, cx + sz * 0.44f, cy + sz * 0.44f, lw * 1.15f);
        }
        else if (id == "headMenuTuner")
        {
            const float pw = sz * 0.24f;
            const float topY = cy - sz * 0.42f;
            const float uY = cy + sz * 0.04f;
            const float botY = cy + sz * 0.44f;
            juce::Path fork;
            fork.startNewSubPath (cx - pw, topY);
            fork.lineTo (cx - pw, uY - pw * 0.2f);
            fork.lineTo (cx - pw * 0.55f, uY + pw * 0.65f);
            fork.lineTo (cx + pw * 0.55f, uY + pw * 0.65f);
            fork.lineTo (cx + pw, uY - pw * 0.2f);
            fork.lineTo (cx + pw, topY);
            g.strokePath (fork, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine (cx, uY + pw * 0.65f, cx, botY, lw);
        }
        else if (id == "headMenuPedals")
        {
            const float bw = sz * 0.34f, bh2 = sz * 0.44f;
            g.drawRoundedRectangle (juce::Rectangle<float> (cx - bw, cy - bh2, bw * 2.0f, bh2 * 2.0f), sz * 0.10f, lw);
            const float kr = sz * 0.075f;
            g.drawEllipse (cx - bw * 0.44f - kr, cy - bh2 * 0.44f - kr, kr * 2.0f, kr * 2.0f, lw);
            g.drawEllipse (cx + bw * 0.44f - kr, cy - bh2 * 0.44f - kr, kr * 2.0f, kr * 2.0f, lw);
            const float fr = sz * 0.095f;
            g.drawEllipse (cx - fr, cy + bh2 * 0.36f - fr, fr * 2.0f, fr * 2.0f, lw);
        }
        else if (id == "headMenuCabs")
        {
            // 2x2 speaker cabinet icon matching preview
            const float bw = sz * 0.44f;
            g.drawRoundedRectangle (juce::Rectangle<float> (cx - bw, cy - bw, bw * 2.0f, bw * 2.0f), sz * 0.10f, lw);
            const float sr = sz * 0.135f;
            const float off = sz * 0.19f;
            g.drawEllipse (cx - off - sr, cy - off - sr, sr * 2.0f, sr * 2.0f, lw * 0.9f);
            g.drawEllipse (cx + off - sr, cy - off - sr, sr * 2.0f, sr * 2.0f, lw * 0.9f);
            g.drawEllipse (cx - off - sr, cy + off - sr, sr * 2.0f, sr * 2.0f, lw * 0.9f);
            g.drawEllipse (cx + off - sr, cy + off - sr, sr * 2.0f, sr * 2.0f, lw * 0.9f);
        }
        else if (id == "headMenuStudioFx")
        {
            // Stereo waveform / EQ rack icon
            const float x0 = cx - sz * 0.46f, x1 = cx + sz * 0.46f;
            g.drawRoundedRectangle (juce::Rectangle<float> (x0, cy - sz * 0.36f, x1 - x0, sz * 0.72f), sz * 0.10f, lw);
            const float barW = sz * 0.10f;
            const float hs[4] = { 0.36f, 0.56f, 0.28f, 0.48f };
            for (int k = 0; k < 4; ++k)
            {
                const float bx = cx - sz * 0.28f + k * sz * 0.18f;
                const float bh2 = sz * hs[k];
                g.fillRoundedRectangle (bx - barW * 0.5f, cy - bh2 * 0.5f, barW, bh2, barW * 0.4f);
            }
        }
        else // headMenuConfig (gear)
        {
            const float rIn = sz * 0.16f, rMid = sz * 0.32f, rOut = sz * 0.45f;
            juce::Path gear;
            const int nTeeth = 8;
            for (int t = 0; t < nTeeth; ++t)
            {
                const float a0 = (static_cast<float> (t) / nTeeth) * juce::MathConstants<float>::twoPi;
                const float step = juce::MathConstants<float>::twoPi / nTeeth;
                const float angs[4] = { a0 - step * 0.22f, a0 - step * 0.12f, a0 + step * 0.12f, a0 + step * 0.22f };
                const float rads[4] = { rMid, rOut, rOut, rMid };
                for (int j = 0; j < 4; ++j)
                {
                    const float px = cx + std::cos (angs[j]) * rads[j];
                    const float py = cy + std::sin (angs[j]) * rads[j];
                    if (t == 0 && j == 0) gear.startNewSubPath (px, py);
                    else                  gear.lineTo (px, py);
                }
            }
            gear.closeSubPath();
            g.strokePath (gear, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawEllipse (cx - rIn, cy - rIn, rIn * 2.0f, rIn * 2.0f, lw);
        }
        return;
    }

    const float h = juce::jlimit (9.5f, 13.0f, static_cast<float> (b.getHeight()) * 0.34f);
    g.setFont (sansFont (h, (id == "presetSave" || id == "presetFactory" || id == "cabTabCabs" || id == "cabTabMics") && b.getToggleState()
                            ? juce::Font::bold : juce::Font::plain));

    auto textArea = bounds.reduced (6, 2);
    if (id == "presetFactory" && b.getToggleState())
    {
        g.setColour (b.isEnabled() ? juce::Colour::fromRGB (255, 250, 240)
                                   : juce::Colour::fromRGBA (255, 250, 240, 115));
        const float cx = static_cast<float> (textArea.getX()) + 7.0f;
        const float cy = textArea.toFloat().getCentreY();
        juce::Path chk;
        chk.startNewSubPath (cx - 4.0f, cy);
        chk.lineTo (cx - 1.0f, cy + 3.5f);
        chk.lineTo (cx + 5.0f, cy - 3.5f);
        g.strokePath (chk, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        textArea.removeFromLeft (14);
        g.drawFittedText (b.getButtonText(), textArea, juce::Justification::centredLeft, 1);
        return;
    }

    if (b.isEnabled())
    {
        g.setColour (juce::Colour::fromRGBA (0, 0, 0, 165));
        g.drawFittedText (b.getButtonText(), textArea.translated (0, 1), juce::Justification::centred, 1);
        g.setColour (juce::Colour::fromRGB (255, 250, 240));
    }
    else
    {
        g.setColour (juce::Colour::fromRGBA (255, 250, 240, 115));
    }
    g.drawFittedText (b.getButtonText(), textArea, juce::Justification::centred, 1);
}

void MiAmpAudioProcessorEditor::GlassLookAndFeel::drawLinearSlider (
    juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float, float, const juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = static_cast<float> (y) + static_cast<float> (height) * 0.5f;
    const auto track = juce::Rectangle<float> (static_cast<float> (x), cy - 3.0f, static_cast<float> (width), 6.0f);
    g.setColour (juce::Colour::fromRGBA (18, 26, 23, 190));
    g.fillRoundedRectangle (track, 3.0f);
    const float fillW = juce::jlimit (0.0f, track.getWidth(), sliderPos - track.getX());
    g.setColour (juce::Colour::fromRGB (166, 223, 201));
    g.fillRoundedRectangle (track.withWidth (fillW), 3.0f);
    g.setColour (inkColour());
    g.fillEllipse (sliderPos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
}

void MiAmpAudioProcessorEditor::GlassLookAndFeel::drawScrollbar (
    juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
    bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
    bool isMouseOver, bool isMouseDown)
{
    if (thumbSize <= 0)
        return;
    const auto col = juce::Colour::fromRGBA (
        isMouseOver || isMouseDown ? 242 : 232,
        isMouseOver || isMouseDown ? 231 : 220,
        isMouseOver || isMouseDown ? 211 : 200,
        isMouseOver || isMouseDown ? 173 : 110);
    g.setColour (col);
    if (isScrollbarVertical)
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (x) + 1.0f,
                                               static_cast<float> (thumbStartPosition),
                                               juce::jmax (3.0f, static_cast<float> (width) - 2.0f),
                                               static_cast<float> (thumbSize));
        g.fillRoundedRectangle (r, r.getWidth() * 0.5f);
    }
    else
    {
        const auto r = juce::Rectangle<float> (static_cast<float> (thumbStartPosition),
                                               static_cast<float> (y) + 1.0f,
                                               static_cast<float> (thumbSize),
                                               juce::jmax (3.0f, static_cast<float> (height) - 2.0f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    }
}

void MiAmpAudioProcessorEditor::GlassLookAndFeel::fillTextEditorBackground (
    juce::Graphics& g, int width, int height, juce::TextEditor&)
{
    const auto r = juce::Rectangle<float> (0.5f, 0.5f, static_cast<float> (width) - 1.0f, static_cast<float> (height) - 1.0f);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 28)); // #ffffff1c
    g.fillRoundedRectangle (r, 8.0f);
}

void MiAmpAudioProcessorEditor::GlassLookAndFeel::drawTextEditorOutline (
    juce::Graphics& g, int width, int height, juce::TextEditor& te)
{
    const auto r = juce::Rectangle<float> (0.75f, 0.75f, static_cast<float> (width) - 1.5f, static_cast<float> (height) - 1.5f);
    const bool focused = te.hasKeyboardFocus (true);
    g.setColour (focused ? juce::Colour::fromRGB (194, 236, 212)   // #c2ecd4
                         : juce::Colour::fromRGBA (241, 224, 190, 102)); // #f1e0be66
    g.drawRoundedRectangle (r, 8.0f, focused ? 1.8f : 1.0f);
}

//==============================================================================
// ImageRotaryKnob
//==============================================================================
MiAmpAudioProcessorEditor::ImageRotaryKnob::ImageRotaryKnob (
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    const juce::String& labelText,
    juce::Image knobImg,
    juce::Image ringImg,
    KnobKind kind,
    std::function<void(const juce::String&)> onHud,
    std::function<void()> onInteract)
    : apvts (state), pid (paramId), label (labelText),
      image (std::move (knobImg)), ringImage (std::move (ringImg)), knobKind (kind),
      hudCallback (std::move (onHud)), interactCallback (std::move (onInteract))
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::setParam (const juce::String& newId, const juce::String& newLabel, juce::Image newImg)
{
    pid = newId;
    label = newLabel;
    image = std::move (newImg);
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::paint (juce::Graphics& g)
{
    auto* raw = apvts.getRawParameterValue (pid);
    const float val = raw != nullptr ? raw->load() : 50.0f;
    const float deg = knobDeg (val);
    const float rad = juce::degreesToRadians (deg);

    const auto b = getLocalBounds().toFloat();
    const float sz = juce::jmin (b.getWidth(), b.getHeight());
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();

    if (knobKind == KnobKind::GuitarHead || knobKind == KnobKind::BassHead)
    {
        if (knobKind == KnobKind::GuitarHead && ringImage.isValid())
        {
            // .stage:not(.bass) .amp-hit::before { inset: -20% } -> 1.40x knob diameter
            const float ringSz = sz * 1.18f;
            g.drawImage (ringImage,
                         juce::Rectangle<float> (cx - ringSz * 0.5f, cy - ringSz * 0.5f, ringSz, ringSz),
                         juce::RectanglePlacement::stretchToFit);
        }
        else if (knobKind == KnobKind::BassHead)
        {
            // .stage.bass .amp-hit::before { inset: 7%; radial seam blend }
            const float rBlend = sz * 0.43f;
            juce::ColourGradient rg (juce::Colour::fromRGBA (23, 23, 22, 245), cx, cy,
                                     juce::Colour::fromRGBA (35, 34, 32, 0), cx + rBlend, cy, true);
            rg.addColour (0.62, juce::Colour::fromRGBA (27, 27, 26, 240));
            rg.addColour (0.80, juce::Colour::fromRGBA (41, 40, 39, 220));
            g.setGradientFill (rg);
            g.fillEllipse (cx - rBlend, cy - rBlend, rBlend * 2.0f, rBlend * 2.0f);
        }

        // Fixed metallic center cap (width 64%)
        const float capSz = sz * 0.64f;
        if (image.isValid())
        {
            g.drawImage (image,
                         juce::Rectangle<float> (cx - capSz * 0.5f, cy - capSz * 0.5f, capSz, capSz),
                         juce::RectanglePlacement::stretchToFit);
        }

        // Pointer indicator (::after center 9% / 4% 19%)
        const float rInner = sz * 0.22f;
        const float rOuter = sz * 0.41f;
        const float sinA = std::sin (rad);
        const float cosA = -std::cos (rad);
        g.setColour (juce::Colour::fromRGB (247, 241, 221));
        g.drawLine (cx + sinA * rInner, cy + cosA * rInner,
                    cx + sinA * rOuter, cy + cosA * rOuter,
                    juce::jmax (1.6f, sz * 0.042f));

        if (isMouseOver() || dragging)
        {
            g.setColour (juce::Colour::fromRGB (189, 238, 208));
            g.drawEllipse (cx - sz * 0.48f, cy - sz * 0.48f, sz * 0.96f, sz * 0.96f, 2.2f);
        }
    }
    else
    {
        if (image.isValid())
        {
            const float sx = sz / static_cast<float> (image.getWidth());
            const float sy = sz / static_cast<float> (image.getHeight());
            auto tr = juce::AffineTransform::translation (-image.getWidth() * 0.5f, -image.getHeight() * 0.5f)
                          .scaled (sx, sy)
                          .rotated (rad)
                          .translated (cx, cy);
            g.drawImageTransformed (image, tr, false);
        }
        if (isMouseOver() || dragging)
        {
            g.setColour (juce::Colour::fromRGB (234, 250, 222));
            g.drawEllipse (cx - sz * 0.48f, cy - sz * 0.48f, sz * 0.96f, sz * 0.96f, 1.8f);
        }
    }
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseDown (const juce::MouseEvent& e)
{
    if (interactCallback) interactCallback();
    if (auto* raw = apvts.getRawParameterValue (pid))
        dragStartVal = raw->load();
    dragStartY = e.getPosition().y;
    dragging = true;
    if (hudCallback)
        hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 ") + juce::String (juce::roundToInt (dragStartVal)) + "%");
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const float sens = e.mods.isShiftDown() ? 0.25f : 0.72f;
    const float dy = static_cast<float> (dragStartY - e.getPosition().y);
    const float nextVal = juce::jlimit (0.0f, 100.0f, dragStartVal + dy * sens);
    if (auto* p = apvts.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (nextVal));
    if (hudCallback)
        hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 ") + juce::String (juce::roundToInt (nextVal)) + "%");
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (interactCallback) interactCallback();
    if (auto* p = apvts.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (50.0f));
    if (hudCallback)
        hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 50%"));
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (interactCallback) interactCallback();
    if (auto* raw = apvts.getRawParameterValue (pid))
    {
        const float nextVal = juce::jlimit (0.0f, 100.0f, raw->load() + wheel.deltaY * 14.0f);
        if (auto* p = apvts.getParameter (pid))
            p->setValueNotifyingHost (p->convertTo0to1 (nextVal));
        if (hudCallback)
            hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 ") + juce::String (juce::roundToInt (nextVal)) + "%");
        repaint();
    }
}

//==============================================================================
// ImageToggleSwitch (exact HTML polarity + aspect ratio + translate(-46.1%,-51.8%))
//==============================================================================
MiAmpAudioProcessorEditor::ImageToggleSwitch::ImageToggleSwitch (
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    const juce::String& labelText,
    juce::Image upImg,
    juce::Image downImg,
    bool invertDownState,
    std::function<void(const juce::String&)> onHud)
    : apvts (state), pid (paramId), label (labelText),
      imgUp (std::move (upImg)), imgDown (std::move (downImg)),
      invertDown (invertDownState), hudCallback (std::move (onHud))
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MiAmpAudioProcessorEditor::ImageToggleSwitch::paint (juce::Graphics& g)
{
    auto* raw = apvts.getRawParameterValue (pid);
    const bool paramOn = raw != nullptr && raw->load() > 0.5f;
    const bool isDown = invertDown ? ! paramOn : paramOn;
    const auto& img = isDown ? imgDown : imgUp;

    if (img.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImage (img, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
    }

    if (isMouseOver())
    {
        g.setColour (juce::Colour::fromRGBA (213, 255, 232, 180));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 4.0f, 1.4f);
    }
}

void MiAmpAudioProcessorEditor::ImageToggleSwitch::mouseDown (const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter (pid))
    {
        const bool next = p->getValue() < 0.5f;
        p->setValueNotifyingHost (next ? 1.0f : 0.0f);
        if (hudCallback)
        {
            if (pid == MiAmpParams::ampUltraHi)
                hudCallback (juce::String::fromUTF8 (next ? u8"ULTRA \u00b7 ULTRA-HI" : u8"ULTRA \u00b7 ULTRA-LO"));
            else if (pid == MiAmpParams::ampStandby || pid == MiAmpParams::gtrStandby)
                hudCallback (juce::String::fromUTF8 (next ? u8"STANDBY \u00b7 STANDBY" : u8"STANDBY \u00b7 ON"));
            else if (pid == MiAmpParams::gtrModeBright)
                hudCallback (juce::String::fromUTF8 (next ? u8"MODE \u00b7 ABAJO" : u8"MODE \u00b7 ARRIBA"));
            else
                hudCallback (label + juce::String::fromUTF8 (next ? u8" \u00b7 ON" : u8" \u00b7 OFF"));
        }
        repaint();
    }
}

//==============================================================================
// TrashIconButton (.trash-btn circular 44x44)
//==============================================================================
MiAmpAudioProcessorEditor::TrashIconButton::TrashIconButton()
    : juce::Button ("Quitar pedal")
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MiAmpAudioProcessorEditor::TrashIconButton::paintButton (
    juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient grad (
        juce::Colour::fromRGBA (255, 255, 255, down ? 72 : (highlighted ? 60 : 40)), r.getX(), r.getY(),
        juce::Colour::fromRGBA (255, 255, 255, 11), r.getRight(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (r);
    g.setColour (highlighted ? juce::Colours::white.withAlpha (0.9f)
                             : juce::Colour::fromRGBA (255, 244, 212, 140));
    g.drawEllipse (r, 1.1f);

    // Draw crisp trash can icon centered
    const float cx = r.getCentreX();
    const float cy = r.getCentreY();
    const float u = r.getWidth() * 0.038f;
    g.setColour (juce::Colour::fromRGB (248, 241, 223));
    const float lw = juce::jmax (1.2f, 1.5f * u);
    g.drawLine (cx - 6.0f * u, cy - 3.5f * u, cx + 6.0f * u, cy - 3.5f * u, lw);
    g.drawRect (cx - 2.4f * u, cy - 5.8f * u, 4.8f * u, 2.3f * u, lw * 0.85f);
    g.drawLine (cx - 4.5f * u, cy - 3.5f * u, cx - 3.6f * u, cy + 5.8f * u, lw);
    g.drawLine (cx + 4.5f * u, cy - 3.5f * u, cx + 3.6f * u, cy + 5.8f * u, lw);
    g.drawLine (cx - 3.6f * u, cy + 5.8f * u, cx + 3.6f * u, cy + 5.8f * u, lw);
    g.drawLine (cx - 1.5f * u, cy - 1.2f * u, cx - 1.5f * u, cy + 3.8f * u, lw * 0.85f);
    g.drawLine (cx + 1.5f * u, cy - 1.2f * u, cx + 1.5f * u, cy + 3.8f * u, lw * 0.85f);
}

//==============================================================================
// PedalToggleOverlay (2-way or 3-way hardware toggle inside .edit)
//==============================================================================
MiAmpAudioProcessorEditor::PedalToggleOverlay::PedalToggleOverlay (
    juce::AudioProcessorValueTreeState& state,
    juce::Image upImg, juce::Image downImg,
    std::function<void(const juce::String&)> onHud)
    : apvts (state), imgUp (std::move (upImg)), imgDown (std::move (downImg)), hudCallback (std::move (onHud))
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MiAmpAudioProcessorEditor::PedalToggleOverlay::setConfig (
    const juce::String& paramId, const juce::String& labelText, bool isThreeWay,
    const juce::String& opt0, const juce::String& opt1, const juce::String& opt2)
{
    pid = paramId;
    label = labelText;
    threeWay = isThreeWay;
    opts[0] = opt0;
    opts[1] = opt1;
    opts[2] = opt2;
    repaint();
}

void MiAmpAudioProcessorEditor::PedalToggleOverlay::paint (juce::Graphics& g)
{
    auto* raw = apvts.getRawParameterValue (pid);
    const int val = raw != nullptr ? static_cast<int> (std::lround (raw->load())) : 0;
    const auto b = getLocalBounds().toFloat();

    if (threeWay && val == 1)
    {
        // Middle/neutral position: brushed silver nut + centered circular tip
        const float cx = b.getCentreX(), cy = b.getCentreY();
        const float nutR = b.getWidth() * 0.34f;
        g.setColour (juce::Colour::fromRGB (165, 168, 172));
        g.fillEllipse (cx - nutR, cy - nutR, nutR * 2.0f, nutR * 2.0f);
        g.setColour (juce::Colour::fromRGB (70, 74, 80));
        g.drawEllipse (cx - nutR, cy - nutR, nutR * 2.0f, nutR * 2.0f, 1.2f);
        const float tipR = b.getWidth() * 0.19f;
        g.setColour (juce::Colour::fromRGB (228, 232, 238));
        g.fillEllipse (cx - tipR, cy - tipR, tipR * 2.0f, tipR * 2.0f);
        g.setColour (juce::Colour::fromRGB (92, 96, 102));
        g.drawEllipse (cx - tipR, cy - tipR, tipR * 2.0f, tipR * 2.0f, 1.0f);
    }
    else
    {
        const bool isUp = threeWay ? (val >= 2) : (val >= 1);
        const auto& img = isUp ? imgUp : imgDown;
        if (img.isValid())
        {
            g.setOpacity (1.0f);
            g.drawImage (img, b, juce::RectanglePlacement::stretchToFit);
        }
    }

    if (isMouseOver())
    {
        g.setColour (juce::Colour::fromRGBA (213, 255, 232, 180));
        g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 1.3f);
    }
}

void MiAmpAudioProcessorEditor::PedalToggleOverlay::mouseDown (const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter (pid))
    {
        int next = 0;
        if (threeWay)
        {
            const int cur = static_cast<int> (std::lround (apvts.getRawParameterValue (pid)->load()));
            next = (cur + 1) % 3;
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (next)));
        }
        else
        {
            const bool nxt = p->getValue() < 0.5f;
            next = nxt ? 1 : 0;
            p->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
        }
        if (hudCallback)
            hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 ") + opts[std::clamp (next, 0, 2)]);
        repaint();
    }
}

//==============================================================================
// EqFader (7-band graphic EQ vertical fader using ped2_slider_eq_cap.png)
//==============================================================================
MiAmpAudioProcessorEditor::EqFader::EqFader (
    juce::AudioProcessorValueTreeState& state,
    juce::Image capImg,
    std::function<void(const juce::String&)> onHud)
    : apvts (state), capImage (std::move (capImg)), hudCallback (std::move (onHud))
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MiAmpAudioProcessorEditor::EqFader::setParam (const juce::String& paramId, const juce::String& bandLabel)
{
    pid = paramId;
    label = bandLabel;
    repaint();
}

juce::String MiAmpAudioProcessorEditor::EqFader::formatDb (float val) const
{
    const float db = (val * 0.01f - 0.5f) * 24.0f;
    return juce::String (db >= 0.05f ? "+" : "") + juce::String (db, 1) + " dB";
}

void MiAmpAudioProcessorEditor::EqFader::setFromY (int yPx)
{
    const float h = juce::jmax (1.0f, static_cast<float> (getHeight()));
    const float capH = juce::jmax (10.0f, getWidth() * 0.92f);
    const float travel = juce::jmax (1.0f, h - capH);
    const float norm = juce::jlimit (0.0f, 1.0f, 1.0f - (static_cast<float> (yPx) - capH * 0.5f) / travel);
    const float val = norm * 100.0f;
    if (auto* p = apvts.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (val));
    if (hudCallback)
        hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 ") + formatDb (val));
    repaint();
}

void MiAmpAudioProcessorEditor::EqFader::paint (juce::Graphics& g)
{
    auto* raw = apvts.getRawParameterValue (pid);
    const float val = raw != nullptr ? raw->load() : 50.0f;
    const float norm = juce::jlimit (0.0f, 1.0f, val * 0.01f);

    const auto b = getLocalBounds().toFloat();
    const float capW = b.getWidth() * 0.92f;
    const float capH = capW * (capImage.isValid()
                                   ? static_cast<float> (capImage.getHeight()) / juce::jmax (1, capImage.getWidth())
                                   : 0.72f);
    const float travel = juce::jmax (1.0f, b.getHeight() - capH);
    const float cy = (1.0f - norm) * travel + capH * 0.5f;
    const auto capR = juce::Rectangle<float> (b.getCentreX() - capW * 0.5f, cy - capH * 0.5f, capW, capH);

    if (capImage.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImage (capImage, capR, juce::RectanglePlacement::stretchToFit);
    }
    else
    {
        g.setColour (juce::Colour::fromRGB (34, 34, 38));
        g.fillRoundedRectangle (capR, 2.5f);
    }

    if (isMouseOver() || dragging)
    {
        g.setColour (juce::Colour::fromRGBA (234, 250, 222, 195));
        g.drawRoundedRectangle (capR.expanded (1.0f), 2.5f, 1.4f);
    }
}

void MiAmpAudioProcessorEditor::EqFader::mouseDown (const juce::MouseEvent& e)
{
    dragging = true;
    dragStartY = e.getPosition().y;
    if (auto* raw = apvts.getRawParameterValue (pid))
        dragStartVal = raw->load();
    setFromY (e.getPosition().y);
}

void MiAmpAudioProcessorEditor::EqFader::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        setFromY (e.getPosition().y);
}

void MiAmpAudioProcessorEditor::EqFader::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

void MiAmpAudioProcessorEditor::EqFader::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (50.0f));
    if (hudCallback)
        hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 0.0 dB"));
    repaint();
}

void MiAmpAudioProcessorEditor::EqFader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (auto* raw = apvts.getRawParameterValue (pid))
    {
        const float nextVal = juce::jlimit (0.0f, 100.0f, raw->load() + wheel.deltaY * 12.0f);
        if (auto* p = apvts.getParameter (pid))
            p->setValueNotifyingHost (p->convertTo0to1 (nextVal));
        if (hudCallback)
            hudCallback (label + juce::String::fromUTF8 (u8" \u00b7 ") + formatDb (nextVal));
        repaint();
    }
}

//==============================================================================
// PalettePedalButton
//==============================================================================
MiAmpAudioProcessorEditor::PalettePedalButton::PalettePedalButton (
    const juce::String& title, juce::Image img,
    std::function<void()> onClick,
    std::function<void(juce::Point<float>)> onDragMove,
    std::function<void(juce::Point<float>)> onDragEnd)
    : name (title), art (std::move (img)),
      clickFn (std::move (onClick)), dragMoveFn (std::move (onDragMove)), dragEndFn (std::move (onDragEnd))
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MiAmpAudioProcessorEditor::PalettePedalButton::setData (
    const juce::String& title, juce::Image img,
    std::function<void()> onClick,
    std::function<void(juce::Point<float>)> onDragMove,
    std::function<void(juce::Point<float>)> onDragEnd,
    std::function<void(juce::Graphics&, juce::Rectangle<float>)> onPaintArt)
{
    name = title;
    art = std::move (img);
    clickFn = std::move (onClick);
    dragMoveFn = std::move (onDragMove);
    dragEndFn = std::move (onDragEnd);
    paintArtFn = std::move (onPaintArt);
    repaint();
}

void MiAmpAudioProcessorEditor::PalettePedalButton::mouseDown (const juce::MouseEvent& e)
{
    downPos = e.getPosition();
    dragged = false;
}

void MiAmpAudioProcessorEditor::PalettePedalButton::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragged && e.getPosition().getDistanceFrom (downPos) > 7)
        dragged = true;

    if (dragged && dragMoveFn)
    {
        auto* target = (getParentComponent() != nullptr && getParentComponent()->getParentComponent() != nullptr)
                           ? getParentComponent()->getParentComponent()
                           : getParentComponent();
        auto ptInEditor = e.getEventRelativeTo (target).position;
        dragMoveFn (ptInEditor);
    }
}

void MiAmpAudioProcessorEditor::PalettePedalButton::mouseUp (const juce::MouseEvent& e)
{
    if (dragged)
    {
        if (dragEndFn)
        {
            auto* target = (getParentComponent() != nullptr && getParentComponent()->getParentComponent() != nullptr)
                               ? getParentComponent()->getParentComponent()
                               : getParentComponent();
            auto ptInEditor = e.getEventRelativeTo (target).position;
            dragEndFn (ptInEditor);
        }
    }
    else
    {
        if (clickFn) clickFn();
    }
    dragged = false;
}

void MiAmpAudioProcessorEditor::PalettePedalButton::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const bool hover = isMouseOver();

    g.setColour (hover ? juce::Colour::fromRGBA (242, 222, 190, 102)
                       : juce::Colour::fromRGBA (230, 211, 181, alreadyInChain ? 38 : 66));
    g.fillRoundedRectangle (r, 8.0f);
    g.setColour (hover ? juce::Colour::fromRGB (255, 242, 218)
                       : juce::Colour::fromRGBA (255, 245, 224, alreadyInChain ? 140 : 76));
    g.drawRoundedRectangle (r, 8.0f, 1.0f);

    if (paintArtFn)
    {
        auto imgR = r.reduced (4.0f, 3.0f).withTrimmedBottom (15.0f);
        g.setOpacity (1.0f);
        paintArtFn (g, imgR);
    }
    else if (art.isValid())
    {
        auto imgR = r.reduced (6.0f, 4.0f).withTrimmedBottom (16.0f);
        g.setOpacity (1.0f); // never inherit border alpha onto pedal artwork!
        g.drawImage (art, imgR, juce::RectanglePlacement::centred);
    }

    g.setColour (inkColour().withAlpha (alreadyInChain ? 0.95f : 0.90f));
    g.setFont (sansFont (juce::jlimit (7.2f, 9.2f, r.getHeight() * 0.095f), juce::Font::bold));
    g.drawFittedText (name, r.withTop (r.getBottom() - 16.0f).reduced (2.0f, 0.0f).toNearestInt(),
                      juce::Justification::centred, 1);
}

//==============================================================================
// GlassPopover (.popover / .floor-popover)
//==============================================================================
class MiAmpAudioProcessorEditor::GlassPopover final : public juce::Component
{
public:
    GlassPopover (MiAmpAudioProcessor& proc,
                  bool& solfegeRef,
                  std::function<void()> onClose,
                  std::function<void()> onChanged,
                  std::function<void()> onLangChanged,
                  std::function<void()> onManualUpdateCheck)
        : processor (proc), useSolfege (solfegeRef),
          closeFn (std::move (onClose)), changedFn (std::move (onChanged)),
          langChangedFn (std::move (onLangChanged)), manualUpdateFn (std::move (onManualUpdateCheck))
    {
        addAndMakeVisible (btnClose);
        btnClose.setComponentID ("popoverClose");
        btnClose.setButtonText (juce::String::fromUTF8 (u8"\u00d7"));
        btnClose.onClick = [this] { if (closeFn) closeFn(); };

        addChildComponent (btnBack);
        btnBack.setComponentID ("popoverBack");
        btnBack.setButtonText ("");
        btnBack.onClick = [this]
        {
            if (kind == PopoverKind::Presets && presetExportSubView)
            {
                presetExportSubView = false;
                exportStatusMsg.clear();
                setKind (kind, tuner);
            }
            else if (closeFn)
            {
                closeFn();
            }
        };

        // Presets (.save-row matching index.html 1:1)
        addChildComponent (presetNameEdit);
        presetNameEdit.setFont (sansFont (13.0f));
        presetNameEdit.setJustification (juce::Justification::centredLeft);
        presetNameEdit.setIndents (10, 0);
        presetNameEdit.setInputRestrictions (32);
        presetNameEdit.setTextToShowWhenEmpty (miamp_i18n::tr (miamp_i18n::Msg::PresetNamePlaceholder),
                                               juce::Colour::fromRGBA (246, 237, 221, 170));
        presetNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        presetNameEdit.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        presetNameEdit.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        presetNameEdit.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (255, 250, 242));
        presetNameEdit.setColour (juce::CaretComponent::caretColourId, juce::Colour::fromRGB (255, 250, 242));
        presetNameEdit.onReturnKey = [this] { btnSavePreset.triggerClick(); };

        addChildComponent (btnSavePreset);
        btnSavePreset.setComponentID ("presetSave");
        btnSavePreset.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnSave));
        btnSavePreset.onClick = [this]
        {
            const auto name = presetNameEdit.getText().trim();
            if (name.isEmpty())
            {
                presetNameEdit.grabKeyboardFocus();
                return;
            }
            processor.saveUserPreset (name);
            presetNameEdit.clear();
            rebuildPresetList();
            if (changedFn) changedFn();
        };

        addChildComponent (btnImportPreset);
        btnImportPreset.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnImportMiAmp));
        btnImportPreset.onClick = [this]
        {
            activeChooser = std::make_unique<juce::FileChooser> (
                miamp_i18n::tr (miamp_i18n::Msg::ImportChooserTitle),
                juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                "*.miamp;*.xml");
            juce::Component::SafePointer<GlassPopover> safeThis (this);
            activeChooser->launchAsync (
                juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safeThis] (const juce::FileChooser& fc)
                {
                    if (safeThis == nullptr) return;
                    const auto f = fc.getResult();
                    if (f.getFullPathName().isNotEmpty())
                    {
                        const auto txt = f.loadFileAsString();
                        if (safeThis->processor.importStateFromXmlString (txt))
                        {
                            safeThis->rebuildPresetList();
                            if (safeThis->changedFn) safeThis->changedFn();
                        }
                    }
                });
        };

        addChildComponent (presetViewport);
        presetViewport.setViewedComponent (&presetListHolder, false);
        presetViewport.setScrollBarsShown (true, false);
        presetViewport.setScrollBarThickness (6);

        // In-app MI AMP Glass Export Format sub-view (replaces OS PopupMenu)
        addChildComponent (btnExportBack);
        btnExportBack.setComponentID ("presetExportBack");
        btnExportBack.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnBackArrow));
        btnExportBack.onClick = [this]
        {
            presetExportSubView = false;
            exportStatusMsg.clear();
            setKind (kind, tuner);
        };

        addChildComponent (exportViewport);
        exportViewport.setViewedComponent (&exportListHolder, false);
        exportViewport.setScrollBarsShown (true, false);
        exportViewport.setScrollBarThickness (6);

        // Drop
        addChildComponent (btnDropToggle);
        btnDropToggle.onClick = [this]
        {
            if (auto* p = processor.apvts.getParameter (MiAmpParams::pitchOn))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
            syncDropControls();
            if (changedFn) changedFn();
        };

        const int semis[6] = { 0, -1, -2, -3, -4, 12 };
        const char* labels[6] = { "0 (E)", "-1 Eb", "-2 D", "-3 C#", "-4 C", "+12" };
        for (int i = 0; i < 6; ++i)
        {
            addChildComponent (dropChips[i]);
            dropChips[i].setButtonText (labels[i]);
            const int sv = semis[i];
            dropChips[i].onClick = [this, sv]
            {
                if (auto* p = processor.apvts.getParameter (MiAmpParams::pitchSemi))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (sv)));
                if (sv != 0)
                    if (auto* pon = processor.apvts.getParameter (MiAmpParams::pitchOn))
                        pon->setValueNotifyingHost (1.0f);
                syncDropControls();
                if (changedFn) changedFn();
            };
        }

        addChildComponent (slSemi);
        slSemi.setRange (-12.0, 12.0, 1.0);
        slSemi.setSliderStyle (juce::Slider::LinearHorizontal);
        slSemi.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        slSemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::pitchSemi, slSemi);

        addChildComponent (slCents);
        slCents.setRange (-50.0, 50.0, 1.0);
        slCents.setSliderStyle (juce::Slider::LinearHorizontal);
        slCents.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        slCentsAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::pitchCents, slCents);

        addChildComponent (slMix);
        slMix.setRange (0.0, 100.0, 1.0);
        slMix.setSliderStyle (juce::Slider::LinearHorizontal);
        slMix.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        slMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::pitchMix, slMix);

        // Tuner
        addChildComponent (btnTunerNotation);
        btnTunerNotation.onClick = [this]
        {
            useSolfege = ! useSolfege;
            syncTunerButtons();
            if (changedFn) changedFn();
            repaint();
        };
        addChildComponent (btnTunerMute);
        btnTunerMute.onClick = [this]
        {
            if (auto* p = processor.apvts.getParameter (MiAmpParams::tunerMuted))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
            syncTunerButtons();
            if (changedFn) changedFn();
        };

        // Config inside scrollable ConfigHolder
        addChildComponent (configViewport);
        configViewport.setViewedComponent (&configHolder, false);
        configViewport.setScrollBarsShown (true, false);
        configViewport.setScrollBarThickness (6);
        configHolder.owner = this;

        const char* cabLabels[5] = { "VINT 4x12", "MOD 4x12", "OPEN 2x12", "STUD 1x12", "CAB OFF" };
        for (int i = 0; i < 5; ++i)
        {
            configHolder.addChildComponent (cfgCabBtns[i]);
            cfgCabBtns[i].setButtonText (cabLabels[i]);
            cfgCabBtns[i].onClick = [this, i]
            {
                if (processor.isBassMode())
                {
                    if (auto* p = processor.apvts.getParameter (MiAmpParams::ampCabOn))
                        p->setValueNotifyingHost (i == 0 ? 1.0f : 0.0f);
                }
                else
                {
                    if (auto* p = processor.apvts.getParameter (MiAmpParams::gtrCab))
                        p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (i)));
                }
                syncConfigControls();
                if (changedFn) changedFn();
            };
        }

        const char* chLabels[3] = { "CH1 CLEAN", "CH2 CRUNCH", "CH3 PUNK" };
        for (int c = 0; c < 3; ++c)
        {
            configHolder.addChildComponent (cfgChBtns[c]);
            cfgChBtns[c].setButtonText (chLabels[c]);
            cfgChBtns[c].onClick = [this, c]
            {
                if (auto* p = processor.apvts.getParameter (MiAmpParams::gtrChannel))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (c)));
                syncConfigControls();
                if (changedFn) changedFn();
            };
        }

        configHolder.addChildComponent (btnCfgOs4x);
        btnCfgOs4x.onClick = [this]
        {
            if (auto* p = processor.apvts.getParameter (MiAmpParams::gtrOs4x))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
            syncConfigControls();
            if (changedFn) changedFn();
        };

        configHolder.addChildComponent (slGate);
        slGate.setRange (0.0, 100.0, 1.0);
        slGate.setSliderStyle (juce::Slider::LinearHorizontal);
        slGate.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        slGateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::gtrGate, slGate);

        configHolder.addChildComponent (slInput);
        slInput.setRange (0.0, 100.0, 1.0);
        slInput.setSliderStyle (juce::Slider::LinearHorizontal);
        slInput.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        slInputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::gtrInput, slInput);

        // Route mode (CON AMPLI vs SOLO PEDALES)
        configHolder.addChildComponent (btnCfgRouteAmp);
        btnCfgRouteAmp.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnWithAmp));
        btnCfgRouteAmp.onClick = [this]
        {
            processor.setHeadBypassed (processor.isBassMode(), false);
            syncConfigControls();
            if (changedFn) changedFn();
        };

        configHolder.addChildComponent (btnCfgRoutePedals);
        btnCfgRoutePedals.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnPedalsOnly));
        btnCfgRoutePedals.onClick = [this]
        {
            processor.setHeadBypassed (processor.isBassMode(), true);
            syncConfigControls();
            if (changedFn) changedFn();
        };

        // Language selector (ES / EN / PT / FR / DE)
        for (int i = 0; i < 5; ++i)
        {
            const auto langVal = static_cast<miamp_i18n::Lang> (i);
            configHolder.addChildComponent (langBtns[i]);
            langBtns[i].setButtonText (miamp_i18n::langShortCode (langVal));
            langBtns[i].onClick = [this, langVal]
            {
                miamp_i18n::setLang (langVal);
                refreshLanguage();
                if (langChangedFn) langChangedFn();
            };
        }

        // Optional Gemini API Key in CONFIG
        configHolder.addChildComponent (geminiKeyEdit);
        geminiKeyEdit.setFont (sansFont (11.5f));
        geminiKeyEdit.setJustification (juce::Justification::centredLeft);
        geminiKeyEdit.setIndents (8, 0);
        geminiKeyEdit.setPasswordCharacter (0x2022);
        geminiKeyEdit.setTextToShowWhenEmpty (miamp_i18n::tr (miamp_i18n::Msg::CfgGeminiPlaceholder),
                                              juce::Colour::fromRGBA (246, 237, 221, 150));
        geminiKeyEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        geminiKeyEdit.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        geminiKeyEdit.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        geminiKeyEdit.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (255, 250, 242));
        geminiKeyEdit.setColour (juce::CaretComponent::caretColourId, juce::Colour::fromRGB (255, 250, 242));
        geminiKeyEdit.setText (MiAmpAudioProcessor::getGeminiApiKey(), false);

        configHolder.addChildComponent (btnSaveGeminiKey);
        btnSaveGeminiKey.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnSave));
        btnSaveGeminiKey.onClick = [this]
        {
            MiAmpAudioProcessor::setGeminiApiKey (geminiKeyEdit.getText());
            btnSaveGeminiKey.setButtonText (miamp_i18n::tr (
                geminiKeyEdit.getText().trim().isEmpty() ? miamp_i18n::Msg::CfgCleared
                                                         : miamp_i18n::Msg::CfgSavedOk));
            configHolder.repaint();
        };

        configHolder.addChildComponent (btnCfgAudio);
        btnCfgAudio.setComponentID ("presetSave");
        btnCfgAudio.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::CfgAudioBtn));
        btnCfgAudio.onClick = [this]
        {
            miamp_standalone::openAudioSettingsDialog();
            configHolder.repaint();
        };

        configHolder.addChildComponent (btnCheckUpdates);
        btnCheckUpdates.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::CfgCheckUpdatesBtn));
        btnCheckUpdates.onClick = [this]
        {
            updateStatusMsg = miamp_i18n::tr (miamp_i18n::Msg::CfgCheckingUpdates);
            configHolder.repaint();
            if (manualUpdateFn) manualUpdateFn();
        };

        // Tone Finder (BUSCAR TONO)
        addChildComponent (tfQueryEdit);
        tfQueryEdit.setFont (sansFont (12.5f));
        tfQueryEdit.setJustification (juce::Justification::centredLeft);
        tfQueryEdit.setIndents (10, 0);
        tfQueryEdit.setTextToShowWhenEmpty (miamp_i18n::tr (miamp_i18n::Msg::TfPlaceholder),
                                            juce::Colour::fromRGBA (246, 237, 221, 165));
        tfQueryEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        tfQueryEdit.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        tfQueryEdit.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        tfQueryEdit.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (255, 250, 242));
        tfQueryEdit.setColour (juce::CaretComponent::caretColourId, juce::Colour::fromRGB (255, 250, 242));
        tfQueryEdit.onReturnKey = [this] { runToneSearch (tfQueryEdit.getText()); };

        addChildComponent (btnTfSearch);
        btnTfSearch.setComponentID ("presetSave");
        btnTfSearch.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfSearchBtn));
        btnTfSearch.onClick = [this] { runToneSearch (tfQueryEdit.getText()); };

        for (int i = 0; i < 6; ++i)
        {
            addChildComponent (tfChips[i]);
            tfChips[i].onClick = [this, i]
            {
                const juce::String q = tfChips[i].getButtonText();
                tfQueryEdit.setText (q, false);
                runToneSearch (q);
            };
        }

        addChildComponent (btnTfApply);
        btnTfApply.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfApplyBtn));
        btnTfApply.onClick = [this]
        {
            if (lastToneResult.found)
            {
                mipedals::ToneFinderEngine::applyToProcessor (processor, lastToneResult);
                if (changedFn) changedFn();
                syncConfigControls();
            }
        };

        addChildComponent (btnTfSavePreset);
        btnTfSavePreset.setComponentID ("presetSave");
        btnTfSavePreset.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfSavePresetBtn));
        btnTfSavePreset.onClick = [this]
        {
            if (lastToneResult.found)
            {
                processor.saveUserPreset (lastToneResult.title);
                tfSavedNotice = true;
                btnTfSavePreset.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfSavedBtn));
                if (changedFn) changedFn();
                repaint();
            }
        };

        addChildComponent (tfPromptPreview);
        tfPromptPreview.setMultiLine (true, true);
        tfPromptPreview.setReadOnly (true);
        tfPromptPreview.setScrollbarsShown (true);
        tfPromptPreview.setFont (sansFont (10.5f));
        tfPromptPreview.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        tfPromptPreview.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        tfPromptPreview.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        tfPromptPreview.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGBA (255, 248, 232, 230));

        addChildComponent (btnTfCopyPrompt);
        btnTfCopyPrompt.setComponentID ("presetSave");
        btnTfCopyPrompt.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfCopyPromptBtn));
        btnTfCopyPrompt.onClick = [this]
        {
            if (tfPromptText.isNotEmpty())
            {
                juce::SystemClipboard::copyTextToClipboard (tfPromptText);
                btnTfCopyPrompt.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfPromptCopiedBtn));
                repaint();
            }
        };
    }

    ~GlassPopover() override
    {
        if (lookupThread != nullptr)
            lookupThread->stopThread (1500);
    }

    void setKind (PopoverKind k, const xvdsp::TunerReading& tr)
    {
        if (k != PopoverKind::Presets)
        {
            presetExportSubView = false;
            exportStatusMsg.clear();
        }
        kind = k;
        tuner = tr;
        const bool isP  = (k == PopoverKind::Presets);
        const bool showBack = useBackNavigation || (isP && presetExportSubView);
        btnBack.setVisible (showBack);
        btnClose.setVisible (! showBack);
        const bool isPList   = isP && ! presetExportSubView;
        const bool isPExport = isP && presetExportSubView;
        const bool isD  = (k == PopoverKind::Drop);
        const bool isT  = (k == PopoverKind::Tuner);
        const bool isC  = (k == PopoverKind::Config);
        const bool isTF = (k == PopoverKind::ToneFinder);
        const bool showDrop = (isD || isT);
        const bool bass = processor.isBassMode();

        if (isTF)
        {
            static constexpr const char* kGtrChips[6]  = { "Cerati / Soda", "John Mayer", "Metallica", "Pink Floyd", "Slash / GNR", "The Story So Far" };
            static constexpr const char* kBassChips[6] = { "Flea / RHCP", "Tool", "Muse Hysteria", "Cliff / Metallica", "XV Directo", "The Story So Far" };
            for (int i = 0; i < 6; ++i)
                tfChips[i].setButtonText (bass ? kBassChips[i] : kGtrChips[i]);

            if ((lastToneResult.found && lastToneResult.bassMode != bass) || (tfNotFound && tfLastBass != bass))
            {
                lastToneResult = {};
                tfNotFound = false;
                tfSavedNotice = false;
                tfPromptText.clear();
            }
        }

        presetNameEdit.setVisible (isPList);
        btnSavePreset.setVisible (isPList);
        btnImportPreset.setVisible (isPList);
        presetViewport.setVisible (isPList);
        btnExportBack.setVisible (isPExport);
        exportViewport.setVisible (isPExport);

        btnDropToggle.setVisible (showDrop);
        for (auto& c : dropChips) c.setVisible (showDrop);
        slSemi.setVisible (showDrop);
        slCents.setVisible (showDrop);
        slMix.setVisible (showDrop);

        btnTunerNotation.setVisible (isT);
        btnTunerMute.setVisible (isT);

        configViewport.setVisible (isC);
        btnCfgRouteAmp.setVisible (isC);
        btnCfgRoutePedals.setVisible (isC);
        for (int i = 0; i < 5; ++i)
            cfgCabBtns[i].setVisible (isC && (! bass || i < 2));
        for (int c = 0; c < 3; ++c)
            cfgChBtns[c].setVisible (isC && ! bass);
        btnCfgOs4x.setVisible (isC && ! bass);
        slGate.setVisible (isC && ! bass);
        slInput.setVisible (isC && ! bass);
        for (int i = 0; i < 5; ++i)
            langBtns[i].setVisible (isC);
        geminiKeyEdit.setVisible (isC);
        btnSaveGeminiKey.setVisible (isC);
        btnCfgAudio.setVisible (isC && miamp_standalone::isRunningStandalone());
        btnCheckUpdates.setVisible (isC);

        tfQueryEdit.setVisible (isTF);
        btnTfSearch.setVisible (isTF);
        for (auto& c : tfChips) c.setVisible (isTF);
        btnTfSavePreset.setVisible (isTF && ! tfSearching && lastToneResult.found);
        btnTfApply.setVisible      (isTF && ! tfSearching && lastToneResult.found);
        tfPromptPreview.setVisible (isTF && ! tfSearching && tfNotFound);
        btnTfCopyPrompt.setVisible (isTF && ! tfSearching && tfNotFound);

        if (isP) rebuildPresetList();
        if (showDrop) syncDropControls();
        if (isT) syncTunerButtons();
        if (isC)
        {
            geminiKeyEdit.setText (MiAmpAudioProcessor::getGeminiApiKey(), false);
            btnSaveGeminiKey.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnSave));
            syncConfigControls();
        }
        resized();
        repaint();
    }

    void refreshLanguage()
    {
        presetNameEdit.setTextToShowWhenEmpty (miamp_i18n::tr (miamp_i18n::Msg::PresetNamePlaceholder),
                                               juce::Colour::fromRGBA (246, 237, 221, 170));
        btnSavePreset.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnSave));
        btnImportPreset.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnImportMiAmp));
        btnExportBack.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnBackArrow));
        btnCfgRouteAmp.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnWithAmp));
        btnCfgRoutePedals.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnPedalsOnly));
        geminiKeyEdit.setTextToShowWhenEmpty (miamp_i18n::tr (miamp_i18n::Msg::CfgGeminiPlaceholder),
                                              juce::Colour::fromRGBA (246, 237, 221, 150));
        btnSaveGeminiKey.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnSave));
        btnCfgAudio.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::CfgAudioBtn));
        btnCheckUpdates.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::CfgCheckUpdatesBtn));
        tfQueryEdit.setTextToShowWhenEmpty (miamp_i18n::tr (miamp_i18n::Msg::TfPlaceholder),
                                            juce::Colour::fromRGBA (246, 237, 221, 165));
        btnTfSearch.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfSearchBtn));
        btnTfApply.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfApplyBtn));
        btnTfSavePreset.setButtonText (miamp_i18n::tr (tfSavedNotice ? miamp_i18n::Msg::TfSavedBtn
                                                                     : miamp_i18n::Msg::TfSavePresetBtn));
        btnTfCopyPrompt.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfCopyPromptBtn));

        syncDropControls();
        syncTunerButtons();
        syncConfigControls();
        if (kind == PopoverKind::Presets)
        {
            if (presetExportSubView) rebuildExportCards();
            else rebuildPresetList();
        }
        configHolder.repaint();
        repaint();
    }

    void setBackNavigation (bool b) { useBackNavigation = b; }

    float sc() const noexcept { return juce::jmax (0.55f, static_cast<float> (getWidth()) / 309.0f); }
    int px (float v) const noexcept { return juce::roundToInt (v * sc()); }

    int getPreferredHeight (int maxH) const
    {
        int h = maxH;
        if (kind == PopoverKind::Tuner)
            h = 412; // ends cleanly right below slMix (368 + 22 + 22 margin)
        else if (kind == PopoverKind::Drop)
            h = 304; // ends cleanly right below slMix
        else if (kind == PopoverKind::Config)
        {
            const bool bass = processor.isBassMode();
            const bool isStand = miamp_standalone::isRunningStandalone();
            const int updBtnY = isStand ? (bass ? 270 : 450) : (bass ? 218 : 398);
            h = 46 + updBtnY + 34 + 20; // fits full configHolder cleanly without dead space
        }
        else if (kind == PopoverKind::ToneFinder && ! tfSearching && ! lastToneResult.found && ! tfNotFound)
            h = 342; // ends cleanly below intro card with balanced padding; expands up to maxH on search/result
        // Presets & active ToneFinder expand up to maxH (top of menu y=25 down to y=698)
        return juce::jlimit (220, maxH, h);
    }

    void setUpdateStatusMessage (const juce::String& msg)
    {
        updateStatusMsg = msg;
        configHolder.repaint();
    }

    void updateTuner (const xvdsp::TunerReading& tr)
    {
        tuner = tr;
        if (kind == PopoverKind::Tuner) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const float s = sc();
        const auto r = getLocalBounds().toFloat().reduced (0.75f);
        miamp_theme::paintFrostedGlassPanel (g, r, 22.0f * s);

        auto inner = r.reduced (18.0f * s, 16.0f * s);
        g.setColour (inkColour());
        juce::String title = (kind == PopoverKind::Presets)
                                ? (presetExportSubView
                                    ? miamp_i18n::tr (miamp_i18n::Msg::TitleExportPreset)
                                    : miamp_i18n::tr (processor.isBassMode() ? miamp_i18n::Msg::TitlePresetsBass
                                                                             : miamp_i18n::Msg::TitlePresetsGuitar))
                           : (kind == PopoverKind::Drop)       ? miamp_i18n::tr (miamp_i18n::Msg::TitleDropTuner)
                           : (kind == PopoverKind::Config)     ? miamp_i18n::tr (miamp_i18n::Msg::TitleConfig)
                           : (kind == PopoverKind::ToneFinder) ? miamp_i18n::tr (processor.isBassMode() ? miamp_i18n::Msg::TitleToneFinderBass
                                                                                                        : miamp_i18n::Msg::TitleToneFinderGuitar)
                                                               : miamp_i18n::tr (miamp_i18n::Msg::TitleTunerDrop);
        const bool showBack = useBackNavigation || (kind == PopoverKind::Presets && presetExportSubView);
        auto titleBox = inner.removeFromTop (24.0f * s).withTrimmedRight (showBack ? 4.0f * s : 26.0f * s);
        if (showBack)
            titleBox = titleBox.withTrimmedLeft (32.0f * s);
        g.setFont (sansFont (14.5f * s, juce::Font::bold));
        g.drawFittedText (title, titleBox.toNearestInt(), juce::Justification::centredLeft, 1);

        if (kind == PopoverKind::Presets)
        {
            const float bx = 18.0f * s;
            const float bw = static_cast<float> (getWidth()) - 36.0f * s;
            if (presetExportSubView)
            {
                g.setFont (sansFont (11.5f * s, juce::Font::bold));
                g.setColour (juce::Colour::fromRGB (247, 211, 138));
                g.drawFittedText (exportTargetName + miamp_i18n::tr (exportTargetIsBass ? miamp_i18n::Msg::TagBassParens
                                                                                        : miamp_i18n::Msg::TagGuitarParens),
                                  juce::Rectangle<int> (px (18.0f), px (46.0f), getWidth() - px (120.0f), px (22.0f)),
                                  juce::Justification::centredLeft, 1);

                g.setFont (sansFont (10.0f * s));
                g.setColour (juce::Colour::fromRGBA (244, 234, 217, 175));
                g.drawText (miamp_i18n::tr (miamp_i18n::Msg::ExportChooseFormat),
                            juce::Rectangle<float> (bx, 68.0f * s, bw, 15.0f * s),
                            juce::Justification::centredLeft);

                if (exportStatusMsg.isNotEmpty())
                {
                    const float footY = static_cast<float> (getHeight()) - 32.0f * s;
                    g.setColour (juce::Colour::fromRGBA (141, 224, 184, 38));
                    g.fillRoundedRectangle (juce::Rectangle<float> (bx, footY, bw, 22.0f * s), 6.0f * s);
                    g.setColour (juce::Colour::fromRGB (166, 223, 201));
                    g.setFont (sansFont (10.5f * s, juce::Font::bold));
                    g.drawFittedText (exportStatusMsg,
                                      juce::Rectangle<int> (static_cast<int> (bx) + px (6.0f), static_cast<int> (footY),
                                                            static_cast<int> (bw) - px (12.0f), px (22.0f)),
                                      juce::Justification::centredLeft, 1);
                }
            }
            else
            {
                const float bottom = static_cast<float> (getHeight()) - 14.0f * s;
                g.setFont (sansFont (10.0f * s, juce::Font::bold));
                g.setColour (juce::Colour::fromRGBA (244, 234, 217, 184)); // #f4ead9b8
                g.drawText (miamp_i18n::tr (miamp_i18n::Msg::SavePresetHeader),
                            juce::Rectangle<float> (bx, bottom - 102.0f * s, bw, 16.0f * s),
                            juce::Justification::centredLeft);
            }
        }
        else if (kind == PopoverKind::Drop)
        {
            g.setFont (sansFont (12.0f * s));
            g.setColour (inkColour().withAlpha (0.85f));
            const float x = inner.getX();
            const float w = inner.getWidth();
            const float y0 = inner.getY() + 104.0f * s;
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::LabelSemitones), juce::Rectangle<float> (x, y0, w, 16.0f * s), juce::Justification::centredLeft);
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::LabelCents),     juce::Rectangle<float> (x, y0 + 42.0f * s, w, 16.0f * s), juce::Justification::centredLeft);
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::LabelMixPct),    juce::Rectangle<float> (x, y0 + 84.0f * s, w, 16.0f * s), juce::Justification::centredLeft);
        }
        else if (kind == PopoverKind::ToneFinder)
        {
            const float x = 18.0f * s;
            const float w = static_cast<float> (getWidth()) - 36.0f * s;
            g.setFont (sansFont (11.0f * s));
            g.setColour (inkColour().withAlpha (0.88f));
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::TfPromptLabel),
                        juce::Rectangle<float> (x, 42.0f * s, w, 16.0f * s), juce::Justification::centredLeft);

            g.setFont (sansFont (10.0f * s, juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.78f));
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::TfSuggestions),
                        juce::Rectangle<float> (x, 99.0f * s, w, 14.0f * s), juce::Justification::centredLeft);

            const float cardTop = 192.0f * s;
            const float cardBot = static_cast<float> (getHeight()) - (lastToneResult.found ? 82.0f * s : (tfNotFound ? 54.0f * s : 18.0f * s));
            const auto cardR = juce::Rectangle<float> (x, cardTop, w, juce::jmax (72.0f * s, cardBot - cardTop));
            miamp_theme::paintGlassCard (g, cardR, 12.0f * s);

            auto tc = cardR.reduced (12.0f * s, 10.0f * s);
            if (tfSearching)
            {
                g.setFont (sansFont (12.5f * s, juce::Font::bold));
                g.setColour (mintBorder());
                g.drawFittedText (miamp_i18n::tr (miamp_i18n::Msg::TfSearching), tc.toNearestInt(), juce::Justification::centred, 2);
            }
            else if (lastToneResult.found)
            {
                g.setFont (sansFont (9.5f * s, juce::Font::bold));
                g.setColour (mintBorder());
                const juce::String badgeTxt = (lastToneResult.sourceBadge == "Rig Verificado")
                    ? miamp_i18n::tr (miamp_i18n::Msg::TfLocalBadge)
                    : lastToneResult.sourceBadge;
                g.drawText (badgeTxt.toUpperCase(), tc.removeFromTop (13.0f * s), juce::Justification::centredLeft);

                g.setFont (sansFont (13.0f * s, juce::Font::bold));
                g.setColour (inkColour());
                g.drawFittedText (lastToneResult.title, tc.removeFromTop (20.0f * s).toNearestInt(), juce::Justification::centredLeft, 1);

                const juce::String dropStr = (lastToneResult.dropSemi != 0)
                    ? ("Drop " + juce::String (lastToneResult.dropSemi) + " st")
                    : (lastToneResult.suggestedDropSemi != 0
                        ? ("Drop " + juce::String (lastToneResult.suggestedDropSemi) + " opt")
                        : "Std");
                const juce::String rigTxt = lastToneResult.rigSummary.isNotEmpty()
                    ? lastToneResult.rigSummary
                    : miamp_i18n::tr (lastToneResult.headBypass ? miamp_i18n::Msg::BtnPedalsOnly : miamp_i18n::Msg::BtnWithAmp);
                const juce::String meta = rigTxt
                                        + " \u00b7 " + dropStr
                                        + " \u00b7 " + juce::String (static_cast<int> (lastToneResult.chainSlots.size())) + " "
                                        + miamp_i18n::tr (miamp_i18n::Msg::TfPedalsWord);
                g.setFont (sansFont (9.8f * s, juce::Font::bold));
                g.setColour (juce::Colour::fromRGBA (255, 226, 175, 225));
                g.drawFittedText (meta, tc.removeFromTop (16.0f * s).toNearestInt(), juce::Justification::centredLeft, 1);

                tc.removeFromTop (3.0f * s);
                g.setFont (sansFont (10.8f * s));
                g.setColour (inkColour().withAlpha (0.90f));
                g.drawFittedText (lastToneResult.explanation, tc.toNearestInt(), juce::Justification::topLeft, 5);

                g.setFont (sansFont (10.5f * s, juce::Font::bold));
                g.setColour (tfSavedNotice ? mintBorder() : juce::Colour::fromRGBA (255, 240, 210, 225));
                g.drawText (miamp_i18n::tr (tfSavedNotice ? miamp_i18n::Msg::TfSavedNotice
                                                          : miamp_i18n::Msg::TfAskSaveNotice),
                            juce::Rectangle<float> (x, static_cast<float> (getHeight()) - 72.0f * s, w, 18.0f * s),
                            juce::Justification::centred);
            }
            else if (tfNotFound)
            {
                g.setFont (sansFont (10.5f * s, juce::Font::bold));
                g.setColour (juce::Colour::fromRGB (255, 210, 140));
                g.drawFittedText (miamp_i18n::tr (miamp_i18n::Msg::TfNotFound),
                                  tc.removeFromTop (28.0f * s).toNearestInt(), juce::Justification::centredLeft, 2);
            }
            else
            {
                g.setFont (sansFont (11.2f * s));
                g.setColour (inkColour().withAlpha (0.86f));
                g.drawFittedText (miamp_i18n::tr (miamp_i18n::Msg::TfEmptyIntro),
                                  tc.toNearestInt(), juce::Justification::centred, 5);
            }
        }
        else if (kind == PopoverKind::Tuner)
        {
            const bool active = tuner.hasSignal && tuner.freqHz > 20.0f;
            const bool inTune = active && std::fabs (tuner.cents) < 6.0f;
            const juce::String noteStr = active ? MiAmpAudioProcessorEditor::formatNoteName (tuner.noteName, tuner.octave, useSolfege) : "--";
            const float x = 18.0f * s;
            const float w = static_cast<float> (getWidth()) - 36.0f * s;

            g.setFont (sansFont ((useSolfege ? 26.0f : 31.0f) * s, juce::Font::bold));
            g.setColour (active ? (inTune ? mintBorder() : inkColour()) : inkColour().withAlpha (0.62f));
            g.drawText (noteStr, juce::Rectangle<float> (x, 36.0f * s, w, 34.0f * s), juce::Justification::centred);

            g.setFont (sansFont (11.5f * s));
            g.setColour (inkColour().withAlpha (0.88f));
            const juce::String hz = active
                ? (juce::String (tuner.freqHz, 1) + " Hz  (" + (tuner.cents >= 0 ? "+" : "") + juce::String (tuner.cents, 1) + " ct)")
                : miamp_i18n::tr (miamp_i18n::Msg::WaitingSignal);
            g.drawText (hz, juce::Rectangle<float> (x, 70.0f * s, w, 15.0f * s), juce::Justification::centred);

            auto meter = juce::Rectangle<float> (x + 6.0f * s, 89.0f * s, w - 12.0f * s, 11.0f * s);
            juce::ColourGradient mg (juce::Colour::fromRGB (183, 115, 85), meter.getX(), meter.getY(),
                                     juce::Colour::fromRGB (183, 115, 85), meter.getRight(), meter.getY(), false);
            mg.addColour (0.25, juce::Colour::fromRGB (208, 186, 128));
            mg.addColour (0.50, juce::Colour::fromRGB (154, 215, 180));
            mg.addColour (0.75, juce::Colour::fromRGB (208, 186, 128));
            g.setGradientFill (mg);
            g.fillRoundedRectangle (meter, 5.5f * s);

            g.setColour (juce::Colours::white);
            g.fillRect (meter.getCentreX() - 1.5f * s, meter.getY() - 2.0f * s, 3.0f * s, meter.getHeight() + 4.0f * s);

            const float normCents = active ? juce::jlimit (-1.0f, 1.0f, tuner.cents / 50.0f) : 0.0f;
            const float nx = meter.getCentreX() + normCents * (meter.getWidth() * 0.46f);
            g.setColour (juce::Colour::fromRGB (23, 39, 34));
            g.fillRect (nx - 2.0f * s, meter.getY() - 3.0f * s, 4.0f * s, meter.getHeight() + 6.0f * s);

            auto row = juce::Rectangle<float> (x + 2.0f * s, 105.0f * s, w - 4.0f * s, 22.0f * s);
            const char* notesBass[4] = { "E", "A", "D", "G" };
            const int   octsBass[4]  = { 1, 1, 2, 2 };
            const char* notesGtr[6]  = { "E", "A", "D", "G", "B", "E" };
            const int   octsGtr[6]   = { 2, 2, 3, 3, 3, 4 };
            const bool isBass = processor.isBassMode();
            const int nNotes = isBass ? 4 : 6;
            const float cw = row.getWidth() / static_cast<float> (nNotes);
            const juce::String curFormatted = active ? MiAmpAudioProcessorEditor::formatNoteName (tuner.noteName, tuner.octave, useSolfege) : "";
            for (int i = 0; i < nNotes; ++i)
            {
                const juce::String txt = MiAmpAudioProcessorEditor::formatNoteName (
                    isBass ? notesBass[i] : notesGtr[i], isBass ? octsBass[i] : octsGtr[i], useSolfege);
                auto cell = juce::Rectangle<float> (row.getX() + i * cw, row.getY(), cw, row.getHeight()).reduced (2.0f * s, 0.0f);
                const bool hit = active && curFormatted == txt;
                g.setColour (hit ? juce::Colour::fromRGBA (141, 224, 184, 90) : juce::Colour::fromRGBA (255, 255, 255, 26));
                g.fillRoundedRectangle (cell, 6.0f * s);
                g.setColour (hit ? mintBorder() : juce::Colour::fromRGBA (255, 244, 212, 70));
                g.drawRoundedRectangle (cell, 6.0f * s, 1.0f);
                g.setColour (hit ? juce::Colours::white : inkColour().withAlpha (0.90f));
                g.setFont (sansFont ((useSolfege ? 9.5f : (isBass ? 11.5f : 10.5f)) * s, juce::Font::bold));
                g.drawText (txt, cell, juce::Justification::centred);
            }

            // Divider + Integrated Drop section labels (scaled cleanly for all window sizes)
            g.setColour (juce::Colour::fromRGBA (241, 224, 190, 62));
            g.drawLine (x, 163.0f * s, x + w, 163.0f * s, 1.0f);

            g.setFont (sansFont (10.0f * s, juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.82f));
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::LabelDropTune), juce::Rectangle<float> (x, 168.0f * s, w, 14.0f * s), juce::Justification::centredLeft);

            g.setFont (sansFont (10.5f * s));
            g.setColour (inkColour().withAlpha (0.88f));
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::LabelSemitones), juce::Rectangle<float> (x, 268.0f * s, w, 14.0f * s), juce::Justification::centredLeft);
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::LabelCents),     juce::Rectangle<float> (x, 310.0f * s, w, 14.0f * s), juce::Justification::centredLeft);
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::LabelMixPct),    juce::Rectangle<float> (x, 352.0f * s, w, 14.0f * s), juce::Justification::centredLeft);
        }
    }

    void resized() override
    {
        const float s = sc();
        btnClose.setBounds (getWidth() - px (38.0f), px (10.0f), px (26.0f), px (28.0f));
        btnBack.setBounds  (px (14.0f), px (13.0f), px (28.0f), px (28.0f));

        presetNameEdit.setFont (sansFont (13.0f * s));
        presetNameEdit.setIndents (px (10.0f), 0);
        geminiKeyEdit.setFont (sansFont (11.5f * s));
        geminiKeyEdit.setIndents (px (8.0f), 0);
        tfQueryEdit.setFont (sansFont (12.5f * s));
        tfQueryEdit.setIndents (px (10.0f), 0);
        tfPromptPreview.setFont (sansFont (10.5f * s));

        const int tbW = px (48.0f);
        const int tbH = px (22.0f);
        slSemi.setTextBoxStyle  (juce::Slider::TextBoxRight, false, tbW, tbH);
        slCents.setTextBoxStyle (juce::Slider::TextBoxRight, false, tbW, tbH);
        slMix.setTextBoxStyle   (juce::Slider::TextBoxRight, false, tbW, tbH);
        slGate.setTextBoxStyle  (juce::Slider::TextBoxRight, false, tbW, tbH);
        slInput.setTextBoxStyle (juce::Slider::TextBoxRight, false, tbW, tbH);

        auto inner = getLocalBounds().reduced (px (18.0f), 0).withTop (px (48.0f)).withTrimmedBottom (px (14.0f));

        if (kind == PopoverKind::Presets)
        {
            if (presetExportSubView)
            {
                btnExportBack.setBounds (getWidth() - px (98.0f), px (44.0f), px (80.0f), px (24.0f));
                const int bottomTrim = exportStatusMsg.isNotEmpty() ? px (38.0f) : px (14.0f);
                exportViewport.setBounds (px (18.0f), px (88.0f), getWidth() - px (36.0f),
                                          juce::jmax (px (40.0f), getHeight() - px (88.0f) - bottomTrim));
                layoutExportCards();
            }
            else
            {
                auto importRow = inner.removeFromBottom (px (26.0f));
                btnImportPreset.setBounds (importRow);
                inner.removeFromBottom (px (10.0f));
                auto saveRow = inner.removeFromBottom (px (41.0f));
                btnSavePreset.setBounds (saveRow.removeFromRight (px (80.0f)));
                saveRow.removeFromRight (px (6.0f));
                presetNameEdit.setBounds (saveRow);
                inner.removeFromBottom (px (29.0f));
                presetViewport.setBounds (inner);
                layoutPresetButtons();
            }
        }
        else if (kind == PopoverKind::Drop)
        {
            btnDropToggle.setBounds (inner.getX(), inner.getY(), inner.getWidth(), px (32.0f));
            const int cw = inner.getWidth() / 3;
            for (int i = 0; i < 6; ++i)
                dropChips[i].setBounds (inner.getX() + (i % 3) * cw + px (2.0f),
                                        inner.getY() + px (38.0f) + (i / 3) * px (32.0f),
                                        cw - px (4.0f), px (28.0f));
            slSemi.setBounds  (inner.getX(), inner.getY() + px (118.0f), inner.getWidth(), px (24.0f));
            slCents.setBounds (inner.getX(), inner.getY() + px (160.0f), inner.getWidth(), px (24.0f));
            slMix.setBounds   (inner.getX(), inner.getY() + px (202.0f), inner.getWidth(), px (24.0f));
        }
        else if (kind == PopoverKind::Config)
        {
            const bool bass = processor.isBassMode();
            const bool isStand = miamp_standalone::isRunningStandalone();
            const int vpW = getWidth() - px (36.0f);
            const int vpH = juce::jmax (px (80.0f), getHeight() - px (60.0f));
            configViewport.setBounds (px (18.0f), px (46.0f), vpW, vpH);

            const int w = vpW - px (10.0f);
            const int hw = (w - px (6.0f)) / 2;
            btnCfgRouteAmp.setBounds    (0,           px (18.0f), hw, px (24.0f));
            btnCfgRoutePedals.setBounds (hw + px (6), px (18.0f), hw, px (24.0f));

            if (bass)
            {
                cfgCabBtns[0].setBounds (0,           px (66.0f), hw, px (28.0f));
                cfgCabBtns[1].setBounds (hw + px (6), px (66.0f), hw, px (28.0f));
            }
            else
            {
                const int cw = w / 3;
                for (int i = 0; i < 5; ++i)
                    cfgCabBtns[i].setBounds ((i % 3) * cw + px (2.0f),
                                             px (66.0f) + (i / 3) * px (26.0f),
                                             cw - px (4.0f), px (23.0f));
                for (int c = 0; c < 3; ++c)
                    cfgChBtns[c].setBounds (c * cw + px (2.0f), px (140.0f), cw - px (4.0f), px (23.0f));
                btnCfgOs4x.setBounds (0, px (167.0f), w, px (23.0f));
                slGate.setBounds     (0, px (213.0f), w, px (20.0f));
                slInput.setBounds    (0, px (254.0f), w, px (20.0f));
            }

            const int langY = px (bass ? 118.0f : 298.0f);
            const int lw = w / 5;
            for (int i = 0; i < 5; ++i)
                langBtns[i].setBounds (i * lw + 1, langY, lw - 2, px (24.0f));

            const int keyRowY = px (bass ? 166.0f : 346.0f);
            btnSaveGeminiKey.setBounds (w - px (72.0f), keyRowY, px (72.0f), px (28.0f));
            geminiKeyEdit.setBounds    (0,              keyRowY, w - px (78.0f), px (28.0f));

            if (isStand)
                btnCfgAudio.setBounds (0, px (bass ? 218.0f : 398.0f), w, px (28.0f));

            const int updBtnY = px (isStand ? (bass ? 270.0f : 450.0f) : (bass ? 218.0f : 398.0f));
            btnCheckUpdates.setBounds (0, updBtnY, w, px (28.0f));
            configHolder.setSize (w, updBtnY + px (34.0f));
        }
        else if (kind == PopoverKind::ToneFinder)
        {
            const int x = px (18.0f);
            const int w = getWidth() - px (36.0f);
            btnTfSearch.setBounds (x + w - px (72.0f), px (62.0f), px (72.0f), px (30.0f));
            tfQueryEdit.setBounds (x,                  px (62.0f), w - px (78.0f), px (30.0f));

            const int cw = w / 2;
            for (int i = 0; i < 6; ++i)
                tfChips[i].setBounds (x + (i % 2) * cw + px (2.0f),
                                      px (116.0f) + (i / 2) * px (23.0f),
                                      cw - px (4.0f), px (20.0f));

            const int cardBot = getHeight() - (tfNotFound ? px (54.0f) : px (18.0f));
            tfPromptPreview.setBounds (x + px (10.0f), px (224.0f), w - px (20.0f), juce::jmax (px (40.0f), cardBot - px (232.0f)));

            const int saveW = (w - px (6.0f)) * 58 / 100;
            btnTfSavePreset.setBounds (x,                  getHeight() - px (48.0f), saveW, px (32.0f));
            btnTfApply.setBounds      (x + saveW + px (6), getHeight() - px (48.0f), w - saveW - px (6.0f), px (32.0f));
            btnTfCopyPrompt.setBounds (x,                  getHeight() - px (48.0f), w, px (32.0f));
        }
        else if (kind == PopoverKind::Tuner)
        {
            const int x = px (18.0f);
            const int w = getWidth() - px (36.0f);
            const int hw = (w - px (6.0f)) / 2;
            btnTunerNotation.setBounds (x,           px (133.0f), hw, px (24.0f));
            btnTunerMute.setBounds     (x + hw + px (6), px (133.0f), hw, px (24.0f));

            btnDropToggle.setBounds (x, px (187.0f), w, px (24.0f));
            const int cw = w / 3;
            for (int i = 0; i < 6; ++i)
                dropChips[i].setBounds (x + (i % 3) * cw + px (2.0f),
                                        px (215.0f) + (i / 3) * px (25.0f),
                                        cw - px (4.0f), px (21.0f));
            slSemi.setBounds  (x, px (284.0f), w, px (22.0f));
            slCents.setBounds (x, px (326.0f), w, px (22.0f));
            slMix.setBounds   (x, px (368.0f), w, px (22.0f));
        }
    }

private:
    void syncDropControls()
    {
        const bool on = processor.apvts.getRawParameterValue (MiAmpParams::pitchOn)->load() > 0.5f;
        btnDropToggle.setToggleState (on, juce::dontSendNotification);
        btnDropToggle.setButtonText (miamp_i18n::tr (on ? miamp_i18n::Msg::DropActive : miamp_i18n::Msg::DropEnable));

        const int semi = static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::pitchSemi)->load()));
        const int semis[6] = { 0, -1, -2, -3, -4, 12 };
        for (int i = 0; i < 6; ++i)
            dropChips[i].setToggleState (semi == semis[i], juce::dontSendNotification);
    }

    void syncTunerButtons()
    {
        btnTunerNotation.setToggleState (useSolfege, juce::dontSendNotification);
        btnTunerNotation.setButtonText (miamp_i18n::tr (useSolfege ? miamp_i18n::Msg::TunerNotesSolfege
                                                                   : miamp_i18n::Msg::TunerNotesLetters));

        const bool muted = processor.apvts.getRawParameterValue (MiAmpParams::tunerMuted)->load() > 0.5f;
        btnTunerMute.setToggleState (muted, juce::dontSendNotification);
        btnTunerMute.setButtonText (miamp_i18n::tr (muted ? miamp_i18n::Msg::TunerMuteOn
                                                          : miamp_i18n::Msg::TunerMuteOff));
    }

    void syncConfigControls()
    {
        const bool bass = processor.isBassMode();
        const bool headByp = processor.isHeadBypassed (bass);
        btnCfgRouteAmp.setToggleState    (! headByp, juce::dontSendNotification);
        btnCfgRoutePedals.setToggleState (headByp,   juce::dontSendNotification);
        if (bass)
        {
            const bool cabOn = processor.apvts.getRawParameterValue (MiAmpParams::ampCabOn)->load() > 0.5f;
            cfgCabBtns[0].setButtonText (miamp_i18n::tr (miamp_i18n::Msg::CfgCab8x10On));
            cfgCabBtns[1].setButtonText (miamp_i18n::tr (miamp_i18n::Msg::CfgCab8x10Off));
            cfgCabBtns[0].setToggleState (cabOn,   juce::dontSendNotification);
            cfgCabBtns[1].setToggleState (! cabOn, juce::dontSendNotification);
        }
        else
        {
            const juce::String cabOffTxt = miamp_i18n::tr (miamp_i18n::Msg::CfgCabOff);
            const juce::String cabLabels[5] = { "VINT 4x12", "MOD 4x12", "OPEN 2x12", "STUD 1x12", cabOffTxt };
            const int cab = static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::gtrCab)->load()));
            for (int i = 0; i < 5; ++i)
            {
                cfgCabBtns[i].setButtonText (cabLabels[i]);
                cfgCabBtns[i].setToggleState (cab == i, juce::dontSendNotification);
            }
            const int ch = static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::gtrChannel)->load()));
            for (int c = 0; c < 3; ++c)
                cfgChBtns[c].setToggleState (ch == c, juce::dontSendNotification);

            const bool os4x = processor.apvts.getRawParameterValue (MiAmpParams::gtrOs4x)->load() > 0.5f;
            btnCfgOs4x.setToggleState (os4x, juce::dontSendNotification);
            btnCfgOs4x.setButtonText (miamp_i18n::tr (os4x ? miamp_i18n::Msg::CfgOs4xOn : miamp_i18n::Msg::CfgOs4xOff));
        }

        const int curL = static_cast<int> (miamp_i18n::currentLang());
        for (int i = 0; i < 5; ++i)
            langBtns[i].setToggleState (curL == i, juce::dontSendNotification);

        configHolder.repaint();
    }

    void rebuildPresetList()
    {
        presetRows.clear();
        presetListHolder.removeAllChildren();

        auto addRow = [this] (const juce::String& text, bool isActive, bool isUserRow,
                              std::function<void()> openExportCb,
                              std::function<void()> onLoad, std::function<void()> onDel)
        {
            auto row = std::make_unique<PresetRow> (text, isActive, isUserRow, std::move (onLoad), std::move (onDel));
            row->onExport = std::move (openExportCb);
            presetListHolder.addAndMakeVisible (*row);
            presetRows.push_back (std::move (row));
        };

        const auto activeName = processor.getActivePresetName();
        if (processor.isBassMode())
        {
            const auto& userList = processor.getUserPresets();
            for (int i = 0; i < static_cast<int> (userList.size()); ++i)
            {
                const auto& up = userList[static_cast<size_t> (i)];
                const bool isAct = (up.name == activeName);
                addRow (up.name, isAct, true,
                    [this, up, isAct]
                    {
                        auto bs = isAct ? processor.getBassSnapshot()
                                        : mipedals::PresetExporter::snapshotFromBassPreset (processor, up);
                        openExportView (up.name, true, bs, processor.getGuitarSnapshot(),
                                        [this, up, isAct]() { if (! isAct) processor.applyPreset (up); });
                    },
                    [this, i] { processor.loadUserPreset (i); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    [this, i] { processor.deleteUserPreset (i); rebuildPresetList(); });
            }
            for (const auto& fp : MiAmpAudioProcessor::getFactoryPresets())
            {
                const bool isAct = (fp.name == activeName);
                addRow (fp.name, isAct, false,
                    [this, fp, isAct]
                    {
                        auto bs = isAct ? processor.getBassSnapshot()
                                        : mipedals::PresetExporter::snapshotFromBassPreset (processor, fp);
                        openExportView (fp.name, true, bs, processor.getGuitarSnapshot(),
                                        [this, fp, isAct]() { if (! isAct) processor.applyPreset (fp); });
                    },
                    [this, fp] { processor.applyPreset (fp); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    nullptr);
            }
        }
        else
        {
            const auto& userList = processor.getGuitarUserPresets();
            for (int i = 0; i < static_cast<int> (userList.size()); ++i)
            {
                const auto& up = userList[static_cast<size_t> (i)];
                const bool isAct = (up.name == activeName);
                addRow (up.name, isAct, true,
                    [this, up, isAct]
                    {
                        auto gs = isAct ? processor.getGuitarSnapshot()
                                        : mipedals::PresetExporter::snapshotFromGuitarPreset (processor, up);
                        openExportView (up.name, false, processor.getBassSnapshot(), gs,
                                        [this, up, isAct]() { if (! isAct) processor.applyGuitarPreset (up); });
                    },
                    [this, i] { processor.loadUserPreset (i); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    [this, i] { processor.deleteUserPreset (i); rebuildPresetList(); });
            }
            for (const auto& fp : MiAmpAudioProcessor::getGuitarFactoryPresets())
            {
                const bool isAct = (fp.name == activeName);
                addRow (fp.name, isAct, false,
                    [this, fp, isAct]
                    {
                        auto gs = isAct ? processor.getGuitarSnapshot()
                                        : mipedals::PresetExporter::snapshotFromGuitarPreset (processor, fp);
                        openExportView (fp.name, false, processor.getBassSnapshot(), gs,
                                        [this, fp, isAct]() { if (! isAct) processor.applyGuitarPreset (fp); });
                    },
                    [this, fp] { processor.applyGuitarPreset (fp); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    nullptr);
            }
        }

        layoutPresetButtons();
    }

    struct ExportOptionSpec
    {
        mipedals::ExportFormat fmt;
        const char* ext;
        miamp_i18n::Msg titleMsg;
        miamp_i18n::Msg subMsg;
        const char* badge;
        bool highlight = false;
    };

    struct ExportFormatCard final : public juce::Component
    {
        ExportFormatCard (const ExportOptionSpec& s, std::function<void()> onClickFn)
            : spec (s), onClick (std::move (onClickFn))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void mouseEnter (const juce::MouseEvent&) override { hovered = true; repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }
        void mouseUp (const juce::MouseEvent& e) override
        {
            if (getLocalBounds().contains (e.getPosition()) && onClick)
                onClick();
        }

        void paint (juce::Graphics& g) override
        {
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            const auto bgTop = hovered ? juce::Colour::fromRGBA (255, 248, 228, 48)
                                       : (spec.highlight ? juce::Colour::fromRGBA (247, 211, 138, 32)
                                                         : juce::Colour::fromRGBA (255, 255, 255, 24));
            const auto bgBot = hovered ? juce::Colour::fromRGBA (255, 240, 200, 22)
                                       : juce::Colour::fromRGBA (255, 255, 255, 10);
            juce::ColourGradient grad (bgTop, r.getX(), r.getY(), bgBot, r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 9.0f);

            g.setColour (hovered ? juce::Colour::fromRGB (194, 236, 212)
                                 : (spec.highlight ? juce::Colour::fromRGBA (247, 211, 138, 165)
                                                   : juce::Colour::fromRGBA (241, 224, 190, 90)));
            const float s = juce::jmax (0.55f, static_cast<float> (getWidth()) / 263.0f);
            g.drawRoundedRectangle (r, 9.0f * s, hovered ? 1.5f : 1.0f);

            // Right extension badge
            const auto badgeR = juce::Rectangle<float> (r.getRight() - 62.0f * s, r.getY() + 9.0f * s, 54.0f * s, 20.0f * s);
            g.setColour (spec.highlight ? juce::Colour::fromRGBA (247, 211, 138, 46)
                                        : juce::Colour::fromRGBA (166, 223, 201, 34));
            g.fillRoundedRectangle (badgeR, 5.0f * s);
            g.setColour (spec.highlight ? juce::Colour::fromRGB (247, 211, 138)
                                        : juce::Colour::fromRGB (166, 223, 201));
            g.setFont (sansFont (9.5f * s, juce::Font::bold));
            g.drawText (juce::String::fromUTF8 (spec.ext), badgeR, juce::Justification::centred);

            // Title & subtitle
            const float textW = r.getWidth() - 78.0f * s;
            g.setColour (juce::Colour::fromRGB (252, 246, 235));
            g.setFont (sansFont (11.5f * s, juce::Font::bold));
            g.drawFittedText (miamp_i18n::tr (spec.titleMsg),
                              juce::Rectangle<int> (juce::roundToInt (10.0f * s), juce::roundToInt (6.0f * s),
                                                    static_cast<int> (textW), juce::roundToInt (16.0f * s)),
                              juce::Justification::centredLeft, 1);

            g.setColour (juce::Colour::fromRGBA (241, 228, 205, 195));
            g.setFont (sansFont (9.5f * s));
            g.drawFittedText (miamp_i18n::tr (spec.subMsg),
                              juce::Rectangle<int> (juce::roundToInt (10.0f * s), juce::roundToInt (23.0f * s),
                                                    static_cast<int> (textW), juce::roundToInt (14.0f * s)),
                              juce::Justification::centredLeft, 1);
        }

        ExportOptionSpec spec;
        std::function<void()> onClick;
        bool hovered = false;
    };

    struct ProprietaryNoteLabel final : public juce::Component
    {
        void paint (juce::Graphics& g) override
        {
            const float s = juce::jmax (0.55f, static_cast<float> (getWidth()) / 263.0f);
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            miamp_theme::paintGlassSubPanel (g, r, 8.0f * s);
            g.setColour (juce::Colour::fromRGBA (250, 243, 227, 210));
            g.setFont (sansFont (9.2f * s));
            g.drawFittedText (miamp_i18n::tr (miamp_i18n::Msg::ExportProprietaryNote),
                              getLocalBounds().reduced (juce::roundToInt (8.0f * s), juce::roundToInt (5.0f * s)),
                              juce::Justification::centredLeft, 3);
        }
    };

    void openExportView (const juce::String& presetName,
                         bool targetIsBass,
                         const miamp::BassControlSnapshot& bassSnap,
                         const miamp_gtr::GuitarControlSnapshot& gtrSnap,
                         std::function<void()> applyForMiAmpXml)
    {
        exportTargetName     = presetName;
        exportTargetIsBass   = targetIsBass;
        exportTargetBassSnap = bassSnap;
        exportTargetGtrSnap  = gtrSnap;
        exportApplyForXml    = std::move (applyForMiAmpXml);
        exportStatusMsg.clear();
        presetExportSubView  = true;

        rebuildExportCards();
        setKind (PopoverKind::Presets, tuner);
    }

    void rebuildExportCards()
    {
        exportCards.clear();
        exportPropNote.reset();
        exportListHolder.removeAllChildren();

        static const ExportOptionSpec kSpecs[] = {
            { mipedals::ExportFormat::NamA2,        ".nam",   miamp_i18n::Msg::ExpNamA2Title,    miamp_i18n::Msg::ExpNamA2Sub,    "A2", true },
            { mipedals::ExportFormat::NamA1,        ".nam",   miamp_i18n::Msg::ExpNamA1Title,    miamp_i18n::Msg::ExpNamA1Sub,    "A1", false },
            { mipedals::ExportFormat::NamLstm,      ".nam",   miamp_i18n::Msg::ExpNamLstmTitle,  miamp_i18n::Msg::ExpNamLstmSub,  "LSTM", false },
            { mipedals::ExportFormat::AidaX,        ".aidax", miamp_i18n::Msg::ExpAidaXTitle,    miamp_i18n::Msg::ExpAidaXSub,    "AIDA", false },
            { mipedals::ExportFormat::ProteusJson,  ".json",  miamp_i18n::Msg::ExpProteusTitle,  miamp_i18n::Msg::ExpProteusSub,  "PROT", false },
            { mipedals::ExportFormat::NeuralPiJson, ".json",  miamp_i18n::Msg::ExpNeuralPiTitle, miamp_i18n::Msg::ExpNeuralPiSub, "NPI",  false },
            { mipedals::ExportFormat::IrWav24_44k,  ".wav",   miamp_i18n::Msg::ExpIrWavTitle,    miamp_i18n::Msg::ExpIrWavSub,    "IR",   false },
            { mipedals::ExportFormat::BlackboxTxt,  ".txt",   miamp_i18n::Msg::ExpBlackboxTitle, miamp_i18n::Msg::ExpBlackboxSub, "TXT",  false },
            { mipedals::ExportFormat::HelixHlx,     ".hlx",   miamp_i18n::Msg::ExpHelixTitle,    miamp_i18n::Msg::ExpHelixSub,    "HLX",  false },
            { mipedals::ExportFormat::HeadrushRig,  ".rig",   miamp_i18n::Msg::ExpHeadrushTitle, miamp_i18n::Msg::ExpHeadrushSub, "RIG",  false },
            { mipedals::ExportFormat::MiAmpXml,     ".miamp", miamp_i18n::Msg::ExpMiAmpTitle,    miamp_i18n::Msg::ExpMiAmpSub,    "XML",  false }
        };

        for (const auto& sp : kSpecs)
        {
            auto card = std::make_unique<ExportFormatCard> (sp, [this, sp]
            {
                triggerFormatExport (sp.fmt, juce::String::fromUTF8 (sp.ext));
            });
            exportListHolder.addAndMakeVisible (*card);
            exportCards.push_back (std::move (card));
        }

        exportPropNote = std::make_unique<ProprietaryNoteLabel>();
        exportListHolder.addAndMakeVisible (*exportPropNote);
        layoutExportCards();
    }

    void triggerFormatExport (mipedals::ExportFormat fmt, const juce::String& ext)
    {
        const juce::String clean = mipedals::PresetExporter::sanitizeFileName (exportTargetName);
        const auto initFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                  .getChildFile (clean + ext);

        juce::Component::SafePointer<GlassPopover> safeThis (this);
        activeChooser = std::make_unique<juce::FileChooser> (
            miamp_i18n::tr (miamp_i18n::Msg::ExportDialogPrefix) + exportTargetName + " (" + ext + ")",
            initFile,
            "*" + ext);
        activeChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
            [safeThis, fmt, ext] (const juce::FileChooser& fc)
            {
                if (safeThis == nullptr) return;
                const auto out = fc.getResult();
                if (out.getFullPathName().isEmpty()) return;

                bool ok = false;
                float esr = -1.0f;
                const bool bass = safeThis->exportTargetIsBass;
                const auto& bs  = safeThis->exportTargetBassSnap;
                const auto& gs  = safeThis->exportTargetGtrSnap;
                const auto& nm  = safeThis->exportTargetName;

                switch (fmt)
                {
                    case mipedals::ExportFormat::NamA2:        ok = mipedals::PresetExporter::exportNamA2 (bass, bs, gs, nm, out, &esr); break;
                    case mipedals::ExportFormat::NamA1:        ok = mipedals::PresetExporter::exportNamA1 (bass, bs, gs, nm, out, &esr); break;
                    case mipedals::ExportFormat::NamLstm:      ok = mipedals::PresetExporter::exportNamLstm (bass, bs, gs, nm, out, &esr); break;
                    case mipedals::ExportFormat::AidaX:        ok = mipedals::PresetExporter::exportAidaX (bass, bs, gs, nm, out, &esr); break;
                    case mipedals::ExportFormat::ProteusJson:  ok = mipedals::PresetExporter::exportGuitarMlJson (bass, bs, gs, 40, out, &esr); break;
                    case mipedals::ExportFormat::NeuralPiJson: ok = mipedals::PresetExporter::exportGuitarMlJson (bass, bs, gs, 20, out, &esr); break;
                    case mipedals::ExportFormat::IrWav24_44k:  ok = mipedals::PresetExporter::exportIrWav44k24Bit (bass, bs, gs, out); break;
                    case mipedals::ExportFormat::BlackboxTxt:  ok = mipedals::PresetExporter::exportBlackboxGuide (bass, bs, gs, nm, out); break;
                    case mipedals::ExportFormat::HelixHlx:     ok = mipedals::PresetExporter::exportHelixHlx (bass, bs, gs, nm, out); break;
                    case mipedals::ExportFormat::HeadrushRig:  ok = mipedals::PresetExporter::exportHeadrushRig (bass, bs, gs, nm, out); break;
                    case mipedals::ExportFormat::MiAmpXml:
                        if (safeThis->exportApplyForXml) safeThis->exportApplyForXml();
                        if (safeThis->changedFn) safeThis->changedFn();
                        ok = mipedals::PresetExporter::exportMiAmpXml (safeThis->processor, nm, out);
                        break;
                }

                if (ok)
                {
                    safeThis->exportStatusMsg = miamp_i18n::tr (miamp_i18n::Msg::ExportedOkPrefix) + out.getFileName();
                    if (esr >= 0.0f)
                        safeThis->exportStatusMsg += " (ESR " + juce::String (esr, 2) + ")";
                }
                else
                {
                    safeThis->exportStatusMsg = miamp_i18n::tr (miamp_i18n::Msg::ExportSaveError);
                }
                safeThis->resized();
                safeThis->repaint();
            });
    }

    void layoutExportCards()
    {
        const float s = sc();
        const int vw = juce::jmax (120, exportViewport.getWidth() - px (10.0f));
        const int cardH = px (44.0f);
        int y = 0;
        for (auto& c : exportCards)
        {
            c->setBounds (0, y, vw, cardH - px (4.0f));
            y += cardH;
        }
        if (exportPropNote != nullptr)
        {
            exportPropNote->setBounds (0, y + px (2.0f), vw, px (44.0f));
            y += px (50.0f);
        }
        exportListHolder.setSize (vw, juce::jmax (10, y));
    }

    void layoutPresetButtons()
    {
        const int vw = juce::jmax (120, presetViewport.getWidth() - px (10.0f));
        const int rowH = px (42.0f);
        int y = 0;
        for (auto& r : presetRows)
        {
            r->setBounds (0, y, vw, rowH - px (5.0f));
            y += rowH;
        }
        presetListHolder.setSize (vw, juce::jmax (10, y));
    }

    struct PresetRow final : public juce::Component
    {
        PresetRow (const juce::String& title, bool active, bool userRow,
                   std::function<void()> loadCb, std::function<void()> delCb)
            : isUser (userRow)
        {
            addAndMakeVisible (btnLoad);
            btnLoad.setComponentID (userRow ? "presetLoad" : "presetFactory");
            btnLoad.setButtonText (title);
            btnLoad.setToggleState (active, juce::dontSendNotification);
            btnLoad.onClick = std::move (loadCb);

            addAndMakeVisible (btnExport);
            btnExport.setComponentID ("presetExport");
            btnExport.setTooltip (miamp_i18n::tr (miamp_i18n::Msg::TooltipExportPreset));
            btnExport.onClick = [this] { if (onExport) onExport(); };

            if (delCb)
            {
                addAndMakeVisible (btnDel);
                btnDel.setComponentID ("presetDelete");
                btnDel.setTooltip (miamp_i18n::tr (miamp_i18n::Msg::TooltipDeletePreset));
                btnDel.onClick = std::move (delCb);
            }
        }
        void paint (juce::Graphics& g) override
        {
            if (! isUser)
                return;
            const auto r = getLocalBounds().toFloat().reduced (0.5f);
            juce::ColourGradient grad (juce::Colour::fromRGBA (255, 255, 255, 32), r.getX(), r.getY(),
                                       juce::Colour::fromRGBA (255, 255, 255, 11), r.getX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, 8.0f);
            g.setColour (juce::Colour::fromRGBA (241, 224, 190, 85)); // #f1e0be55
            g.drawRoundedRectangle (r, 8.0f, 1.0f);
        }
        void resized() override
        {
            auto b = getLocalBounds();
            if (isUser)
                b = b.reduced (6, 2);
            const int sz = juce::jmin (30, b.getHeight());
            if (btnDel.isVisible())
            {
                btnDel.setBounds (b.removeFromRight (sz).withSizeKeepingCentre (sz, sz));
                b.removeFromRight (4);
            }
            btnExport.setBounds (b.removeFromRight (sz).withSizeKeepingCentre (sz, sz));
            b.removeFromRight (4);
            btnLoad.setBounds (b);
        }
        bool isUser = false;
        juce::TextButton btnLoad, btnExport, btnDel;
        std::function<void()> onExport;
    };

    void runToneSearch (const juce::String& rawQuery)
    {
        const juce::String q = rawQuery.trim();
        if (q.isEmpty())
            return;

        tfSavedNotice = false;
        tfNotFound = false;
        btnTfSavePreset.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfSavePresetBtn));
        btnTfCopyPrompt.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TfCopyPromptBtn));

        const bool forBass = processor.isBassMode();
        tfLastBass = forBass;
        int localScore = 0;
        auto localHit = mipedals::ToneFinderEngine::searchLocalCatalog (q, forBass, localScore);
        const juce::String apiKey = MiAmpAudioProcessor::getGeminiApiKey();

        // Fast local exact match when no Gemini API key is set (or high-confidence curated artist hit)
        if (localHit.found && ((localScore >= 50 && apiKey.isEmpty()) || localScore >= 180))
        {
            lastToneResult = localHit;
            tfSearching = false;
            tfStatusMsg.clear();
            mipedals::ToneFinderEngine::applyToProcessor (processor, lastToneResult);
            setKind (kind, tuner);
            if (changedFn) changedFn();
            return;
        }

        if (lookupThread != nullptr)
            lookupThread->stopThread (800);

        tfSearching = true;
        tfStatusMsg.clear();
        lastToneResult = {};
        setKind (kind, tuner);
        if (changedFn) changedFn();

        juce::Component::SafePointer<GlassPopover> safeThis (this);
        lookupThread = std::make_unique<ToneLookupThread> (q, forBass, apiKey, localHit,
            [safeThis, q, forBass] (mipedals::ToneMatchResult res)
            {
                if (safeThis == nullptr) return;
                safeThis->tfSearching = false;
                if (res.found)
                {
                    safeThis->lastToneResult = res;
                    safeThis->tfNotFound = false;
                    mipedals::ToneFinderEngine::applyToProcessor (safeThis->processor, res);
                }
                else
                {
                    safeThis->lastToneResult = {};
                    safeThis->tfNotFound = true;
                    safeThis->tfPromptText = mipedals::ToneFinderEngine::buildAiPrompt (q, forBass);
                    safeThis->tfPromptPreview.setText (safeThis->tfPromptText, false);
                }
                safeThis->setKind (safeThis->kind, safeThis->tuner);
                if (safeThis->changedFn) safeThis->changedFn();
            });
        lookupThread->startThread();
    }

    struct ToneLookupThread final : public juce::Thread
    {
        ToneLookupThread (juce::String q, bool fb, juce::String k, mipedals::ToneMatchResult fallback,
                          std::function<void(mipedals::ToneMatchResult)> cb)
            : juce::Thread ("MiAmpToneLookup"),
              query (std::move (q)), forBass (fb), apiKey (std::move (k)), localFallback (std::move (fallback)),
              onDone (std::move (cb))
        {}

        void run() override
        {
            mipedals::ToneMatchResult res;
            if (apiKey.isNotEmpty() && ! threadShouldExit())
                res = mipedals::ToneFinderEngine::searchWithGemini (query, forBass, apiKey);
            if (! res.found && ! threadShouldExit())
            {
                if (localFallback.found)
                    res = localFallback;
                else
                    res = mipedals::ToneFinderEngine::searchFreeWeb (query, forBass);
            }

            if (! threadShouldExit() && onDone)
                juce::MessageManager::callAsync ([fn = onDone, r = std::move (res)]() mutable { fn (std::move (r)); });
        }

        juce::String query;
        bool forBass = false;
        juce::String apiKey;
        mipedals::ToneMatchResult localFallback;
        std::function<void(mipedals::ToneMatchResult)> onDone;
    };

    struct ConfigHolderComponent final : public juce::Component
    {
        GlassPopover* owner = nullptr;
        void paint (juce::Graphics& g) override
        {
            if (owner == nullptr) return;
            const float s = owner->sc();
            const bool bass = owner->processor.isBassMode();
            const bool isStand = miamp_standalone::isRunningStandalone();
            const float w = static_cast<float> (getWidth());

            g.setFont (sansFont (10.5f * s, juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.88f));
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::CfgRouteMode),
                        juce::Rectangle<float> (0.0f, 2.0f * s, w, 14.0f * s), juce::Justification::centredLeft);
            g.drawText (miamp_i18n::tr (bass ? miamp_i18n::Msg::CfgBassCab : miamp_i18n::Msg::CfgGuitarCab),
                        juce::Rectangle<float> (0.0f, 50.0f * s, w, 14.0f * s), juce::Justification::centredLeft);

            if (! bass)
            {
                g.drawText (miamp_i18n::tr (miamp_i18n::Msg::CfgHeadChannel),
                            juce::Rectangle<float> (0.0f, 124.0f * s, w, 14.0f * s), juce::Justification::centredLeft);
                g.drawText (miamp_i18n::tr (miamp_i18n::Msg::CfgNoiseGate),
                            juce::Rectangle<float> (0.0f, 198.0f * s, w, 14.0f * s), juce::Justification::centredLeft);
                g.drawText (miamp_i18n::tr (miamp_i18n::Msg::CfgInputTrim),
                            juce::Rectangle<float> (0.0f, 239.0f * s, w, 14.0f * s), juce::Justification::centredLeft);
            }

            const float langLabelY = (bass ? 102.0f : 282.0f) * s;
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::CfgLanguage),
                        juce::Rectangle<float> (0.0f, langLabelY, w, 14.0f * s), juce::Justification::centredLeft);

            const float keyY = (bass ? 150.0f : 330.0f) * s;
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::CfgGeminiKey),
                        juce::Rectangle<float> (0.0f, keyY, w, 14.0f * s), juce::Justification::centredLeft);

            if (isStand)
            {
                const float audioLabelY = (bass ? 202.0f : 382.0f) * s;
                const auto summary = miamp_standalone::getCurrentAudioStatusSummary();
                g.setFont (sansFont (9.8f * s, juce::Font::bold));
                g.setColour (mintBorder().withAlpha (0.92f));
                g.drawFittedText (summary.isNotEmpty() ? summary : miamp_i18n::tr (miamp_i18n::Msg::CfgAudioHeader),
                                  juce::Rectangle<int> (0, juce::roundToInt (audioLabelY), static_cast<int> (w), juce::roundToInt (14.0f * s)),
                                  juce::Justification::centredLeft, 1);
            }

            const float updLabelY = (isStand ? (bass ? 254.0f : 434.0f) : (bass ? 202.0f : 382.0f)) * s;
            const juce::String verTxt = miamp_i18n::tr (miamp_i18n::Msg::CfgUpdatesHeader)
                                      + "  \u00b7  v" + miamp_updater::currentVersionString()
                                      + (owner->updateStatusMsg.isNotEmpty() ? (" (" + owner->updateStatusMsg + ")") : juce::String());
            g.setFont (sansFont (9.8f * s, juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.85f));
            g.drawFittedText (verTxt,
                              juce::Rectangle<int> (0, juce::roundToInt (updLabelY), static_cast<int> (w), juce::roundToInt (14.0f * s)),
                              juce::Justification::centredLeft, 1);
        }
    };

    MiAmpAudioProcessor& processor;
    bool& useSolfege;
    std::function<void()> closeFn, changedFn, langChangedFn, manualUpdateFn;
    PopoverKind kind = PopoverKind::None;
    xvdsp::TunerReading tuner;

    juce::TextButton btnClose, btnBack;
    juce::TextEditor presetNameEdit;
    juce::TextButton btnSavePreset, btnImportPreset;
    juce::Viewport presetViewport;
    juce::Component presetListHolder;
    std::vector<std::unique_ptr<PresetRow>> presetRows;
    std::unique_ptr<juce::FileChooser> activeChooser;

    bool useBackNavigation = true;
    bool presetExportSubView = false;
    juce::TextButton btnExportBack;
    juce::Viewport exportViewport;
    juce::Component exportListHolder;
    std::vector<std::unique_ptr<ExportFormatCard>> exportCards;
    std::unique_ptr<ProprietaryNoteLabel> exportPropNote;
    juce::String exportTargetName, exportStatusMsg;
    bool exportTargetIsBass = false;
    miamp::BassControlSnapshot exportTargetBassSnap;
    miamp_gtr::GuitarControlSnapshot exportTargetGtrSnap;
    std::function<void()> exportApplyForXml;

    juce::TextButton btnDropToggle;
    juce::TextButton dropChips[6];
    juce::Slider slSemi, slCents, slMix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slSemiAttach, slCentsAttach, slMixAttach;

    juce::TextButton btnTunerNotation, btnTunerMute;

    juce::Viewport configViewport;
    ConfigHolderComponent configHolder;
    juce::TextButton btnCfgRouteAmp, btnCfgRoutePedals;
    juce::TextButton cfgCabBtns[5];
    juce::TextButton cfgChBtns[3];
    juce::TextButton btnCfgOs4x;
    juce::Slider slGate, slInput;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slGateAttach, slInputAttach;
    juce::TextButton langBtns[5];
    juce::TextEditor geminiKeyEdit;
    juce::TextButton btnSaveGeminiKey, btnCfgAudio, btnCheckUpdates;
    juce::String updateStatusMsg;

    juce::TextEditor tfQueryEdit, tfPromptPreview;
    juce::TextButton btnTfSearch, btnTfApply, btnTfSavePreset, btnTfCopyPrompt;
    juce::TextButton tfChips[6];
    mipedals::ToneMatchResult lastToneResult;
    bool tfSearching = false;
    bool tfNotFound = false;
    bool tfSavedNotice = false;
    bool tfLastBass = false;
    juce::String tfStatusMsg, tfPromptText;
    std::unique_ptr<ToneLookupThread> lookupThread;
};

//==============================================================================
// UpdateOverlayComponent (Centered glass modal for automatic updates)
//==============================================================================
class MiAmpAudioProcessorEditor::UpdateOverlayComponent final : public juce::Component
{
public:
    UpdateOverlayComponent (MiAmpAudioProcessorEditor& ed) : editor (ed)
    {
        setInterceptsMouseClicks (true, true);
    }

    void paint (juce::Graphics& g) override
    {
        const float s = editor.stageScale();
        g.setColour (juce::Colour::fromRGBA (10, 16, 17, 200));
        g.fillRect (getLocalBounds());

        const auto card = editor.toScreen (465, 240, 620, 250);
        miamp_theme::paintFrostedGlassPanel (g, card, 20.0f * s);

        g.setColour (mintBorder());
        g.setFont (sansFont (juce::jmax (10.5f, 12.5f * s), juce::Font::bold));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::UpdModalTitle),
                    editor.toScreen (495, 260, 560, 22), juce::Justification::centred);

        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (14.0f, 18.0f * s), juce::Font::bold));
        const juce::String verLine = miamp_i18n::tr (miamp_i18n::Msg::UpdAvailableTitle) + editor.pendingUpdate.remoteVersion;
        g.drawText (verLine, editor.toScreen (495, 290, 560, 28), juce::Justification::centred);

        juce::String subLine;
        if (editor.updateFailed)
            subLine = miamp_i18n::tr (miamp_i18n::Msg::UpdFailed);
        else if (editor.updateInstalling)
            subLine = miamp_i18n::tr (miamp_i18n::Msg::UpdInstalling);
        else if (editor.updateDownloading)
            subLine = miamp_i18n::tr (miamp_i18n::Msg::UpdDownloading)
                    + " " + juce::String (juce::roundToInt (editor.updateProgress * 100.0f)) + "%";
        else
            subLine = miamp_i18n::tr (miamp_i18n::Msg::UpdDawWarning);

        g.setColour (inkColour().withAlpha (0.88f));
        g.setFont (sansFont (juce::jmax (11.0f, 13.0f * s)));
        g.drawFittedText (subLine, editor.toScreenInt (505, 326, 540, 44), juce::Justification::centred, 2);

        if (editor.updateDownloading || editor.updateInstalling)
        {
            const auto bar = editor.toScreen (525, 384, 500, 12);
            g.setColour (juce::Colour::fromRGBA (255, 255, 255, 30));
            g.fillRoundedRectangle (bar, 6.0f * s);
            const float fillW = bar.getWidth() * juce::jlimit (0.04f, 1.0f, editor.updateProgress);
            g.setColour (mintBorder());
            g.fillRoundedRectangle (bar.withWidth (fillW), 6.0f * s);
        }
    }

private:
    MiAmpAudioProcessorEditor& editor;
};

//==============================================================================
// MiAmpAudioProcessorEditor
//==============================================================================
MiAmpAudioProcessorEditor::MiAmpAudioProcessorEditor (MiAmpAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&glassLaf);
    setWantsKeyboardFocus (true);
    loadImages();

    for (auto* b : { &btnSkipIntro, &btnCreditClose,
                     &btnInstGuitar, &btnInstBass, &btnMainHeads, &btnMainCabs, &btnMainPresets, &btnMainTuner, &btnMainToneFinder, &btnMainPedals, &btnMainStudioFx, &btnMainConsole,
                     &btnDockDrop, &btnDockConfig, &btnViewCloseUp, &btnViewRigMics,
                     &btnAmpPrev, &btnAmpNext, &btnAmpBrowse, &btnCabPrev, &btnCabNext, &btnCabBrowse, &btnCabLoadWav,
                     &btnMic1On, &btnMic1Dist, &btnMic1Phase, &btnMic2On, &btnMic2Dist, &btnMic2Phase,
                     &btnConsoleStudioFx, &btnConsolePower,
                     &btnEditClose, &btnMoveLeft, &btnMoveRight,
                     &btnHeadModeAmp, &btnHeadModePedals, &btnPaletteLeft, &btnPaletteRight,
                     &btnFloorPresets, &btnFloorTuner, &btnFloorToneFinder, &btnFloorDrop, &btnFloorConfig, &btnBackToAmp,
                     &btnFloorTunerMute, &btnFloorTunerNotation })
    {
        b->setLookAndFeel (&glassLaf);
    }

    addAndMakeVisible (btnSkipIntro);
    btnSkipIntro.onClick = [this] { finishIntro(); };

    addChildComponent (btnCreditClose);
    btnCreditClose.onClick = [this] { creditOpen = false; btnCreditClose.setVisible (false); repaint(); };

    addAndMakeVisible (btnInstGuitar);
    btnInstGuitar.onClick = [this] { switchInstrument (false); };
    addAndMakeVisible (btnInstBass);
    btnInstBass.onClick = [this] { switchInstrument (true); };

    addAndMakeVisible (btnMainPresets);
    btnMainPresets.setComponentID ("headMenuPresets");
    btnMainPresets.onClick = [this] { togglePopover (PopoverKind::Presets); };
    addAndMakeVisible (btnMainTuner);
    btnMainTuner.setComponentID ("headMenuTuner");
    btnMainTuner.onClick = [this] { togglePopover (PopoverKind::Tuner); };
    addAndMakeVisible (btnMainToneFinder);
    btnMainToneFinder.setComponentID ("headMenuToneFinder");
    btnMainToneFinder.onClick = [this] { togglePopover (PopoverKind::ToneFinder); };
    addAndMakeVisible (btnMainPedals);
    btnMainPedals.setComponentID ("headMenuPedals");
    btnMainPedals.onClick = [this] { closeFloatingOverlays(); setFloorMode (true); };

    addAndMakeVisible (btnMainCabs);
    btnMainCabs.setComponentID ("headMenuCabs");
    btnMainCabs.onClick = [this]
    {
        const bool next = ! cabDrawerOpen;
        closeFloatingOverlays (next ? "cabDrawer" : nullptr);
        cabDrawerOpen = next;
        if (! next) cabMicZoom = false;
        syncHeadControls();
        layoutAll();
        repaint();
    };

    addAndMakeVisible (btnMainStudioFx);
    btnMainStudioFx.setComponentID ("headMenuStudioFx");
    btnMainStudioFx.onClick = [this]
    {
        if (studioFxModal != nullptr)
        {
            const bool openNext = ! studioFxModal->isOpenTarget();
            if (openNext)
            {
                closeFloatingOverlays ("studioFx");
                studioFxModal->openAnimated();
            }
            else
            {
                studioFxModal->closeAnimated();
            }
            syncHeadControls();
            layoutAll();
        }
    };

    addChildComponent (btnDockDrop); // Drop is now integrated inside AFINADOR
    btnDockDrop.onClick = [this] { togglePopover (PopoverKind::Tuner); };
    addAndMakeVisible (btnDockConfig);
    btnDockConfig.setComponentID ("headMenuConfig");
    btnDockConfig.onClick = [this] { togglePopover (PopoverKind::Config); };

    // View mode & legacy menu buttons kept hidden so right menu matches 7-button preview layout
    addChildComponent (btnViewCloseUp);
    addChildComponent (btnViewRigMics);
    addChildComponent (btnMainHeads);
    addChildComponent (btnMainConsole);
    btnViewCloseUp.onClick = [this] { closeUpView = true;  syncHeadControls(); layoutAll(); repaint(); };
    btnViewRigMics.onClick = [this] { closeUpView = false; syncHeadControls(); layoutAll(); repaint(); };

    // Mirrored side head-selector arrows + left cabinet/mics frosted drawer
    btnHeadSidePrev = std::make_unique<miamp_rig::HeadSideArrowButton> (false);
    btnHeadSideNext = std::make_unique<miamp_rig::HeadSideArrowButton> (true);
    btnHeadSidePrev->onClick = [this] { cycleActiveHead (-1); };
    btnHeadSideNext->onClick = [this] { cycleActiveHead (+1); };
    addAndMakeVisible (*btnHeadSidePrev);
    addAndMakeVisible (*btnHeadSideNext);

    cabPickerPanel = std::make_unique<miamp_rig::CabPickerPanel> (
        audioProcessor,
        [this]
        {
            syncHeadControls();
            layoutAll();
            repaint();
        },
        [this]
        {
            cabDrawerOpen = false;
            cabMicZoom = false;
            syncHeadControls();
            layoutAll();
            repaint();
        },
        [this] (bool micMode)
        {
            cabMicZoom = micMode;
            layoutAll();
            repaint();
        },
        [this] { if (rigBrowser) rigBrowser->openWavChooser(); });
    cabPickerPanel->setLookAndFeel (&glassLaf);
    addChildComponent (*cabPickerPanel);

    // Bottom Analog Console Strip Controls
    auto cycleHead = [this] (int delta)
    {
        const bool bass = audioProcessor.isBassMode();
        const char* id = bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel;
        const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (id)->load()));
        const int nxt = (cur + delta + 9) % 9;
        miamp_rig::setParamPlain (audioProcessor.apvts, id, static_cast<float> (nxt));
        syncHeadControls();
        layoutAll();
        repaint();
    };
    auto cycleCab = [this] (int delta)
    {
        const bool bass = audioProcessor.isBassMode();
        const char* id = bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab;
        const int total = bass ? 9 : 10;
        const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (id)->load()));
        const int nxt = (cur + delta + total) % total;
        miamp_rig::setParamPlain (audioProcessor.apvts, id, static_cast<float> (nxt));
        syncHeadControls();
        layoutAll();
        repaint();
    };

    addAndMakeVisible (btnAmpPrev);   btnAmpPrev.onClick   = [cycleHead] { cycleHead (-1); };
    addAndMakeVisible (btnAmpNext);   btnAmpNext.onClick   = [cycleHead] { cycleHead (+1); };
    addAndMakeVisible (btnAmpBrowse); btnAmpBrowse.onClick = [this] { closeFloatingOverlays ("rigBrowser"); if (rigBrowser) rigBrowser->openMode (miamp_rig::RigBrowserOverlay::Mode::Heads); syncHeadControls(); };

    addAndMakeVisible (btnCabPrev);    btnCabPrev.onClick    = [cycleCab] { cycleCab (-1); };
    addAndMakeVisible (btnCabNext);    btnCabNext.onClick    = [cycleCab] { cycleCab (+1); };
    addAndMakeVisible (btnCabBrowse);  btnCabBrowse.onClick  = [this] { closeFloatingOverlays ("rigBrowser"); if (rigBrowser) rigBrowser->openMode (miamp_rig::RigBrowserOverlay::Mode::Cabs); syncHeadControls(); };
    addAndMakeVisible (btnCabLoadWav); btnCabLoadWav.onClick = [this] { closeFloatingOverlays ("rigBrowser"); if (rigBrowser) rigBrowser->openWavChooser(); syncHeadControls(); };

    auto bindMicToggle = [this] (juce::TextButton& b, const char* id)
    {
        b.setClickingTogglesState (true);
        b.onClick = [this, &b, id] { miamp_rig::setParamPlain (audioProcessor.apvts, id, b.getToggleState() ? 1.0f : 0.0f); syncHeadControls(); layoutAll(); repaint(); };
        addAndMakeVisible (b);
    };
    bindMicToggle (btnMic1On, MiAmpParams::mic1On);
    bindMicToggle (btnMic1Phase, MiAmpParams::mic1Phase);
    bindMicToggle (btnMic2On, MiAmpParams::mic2On);
    bindMicToggle (btnMic2Phase, MiAmpParams::mic2Phase);

    auto cycleMicDist = [this] (const char* id)
    {
        const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (id)->load()));
        miamp_rig::setParamPlain (audioProcessor.apvts, id, static_cast<float> ((cur + 1) % 3));
        syncHeadControls();
        layoutAll();
        repaint();
    };
    addAndMakeVisible (btnMic1Dist); btnMic1Dist.onClick = [cycleMicDist] { cycleMicDist (MiAmpParams::mic1Dist); };
    addAndMakeVisible (btnMic2Dist); btnMic2Dist.onClick = [cycleMicDist] { cycleMicDist (MiAmpParams::mic2Dist); };

    addAndMakeVisible (btnConsoleStudioFx);
    btnConsoleStudioFx.onClick = [this]
    {
        if (studioFxModal != nullptr)
        {
            const bool openNext = ! studioFxModal->isOpenTarget();
            if (openNext)
            {
                closeFloatingOverlays ("studioFx");
                studioFxModal->openAnimated();
            }
            else
            {
                studioFxModal->closeAnimated();
            }
            syncHeadControls();
            layoutAll();
        }
    };
    addAndMakeVisible (btnConsolePower);
    btnConsolePower.setClickingTogglesState (true);
    btnConsolePower.onClick = [this]
    {
        const bool bass = audioProcessor.isBassMode();
        miamp_rig::setParamPlain (audioProcessor.apvts, bass ? MiAmpParams::ampPower : MiAmpParams::gtrPower,
                                  btnConsolePower.getToggleState() ? 1.0f : 0.0f);
        syncHeadControls();
        repaint();
    };

    const char* conNames[5] = { "GAIN", "BASS", "MID", "TREBLE", "MASTER" };
    const char* conIds[5]   = { MiAmpParams::gtrChGain[1], MiAmpParams::gtrChBass[1], MiAmpParams::gtrChMid[1],
                                MiAmpParams::gtrChTreble[1], MiAmpParams::gtrChMaster[1] };
    for (int i = 0; i < 5; ++i)
    {
        conKnobs[i] = std::make_unique<miamp_rig::StudioMiniKnob> (audioProcessor.apvts, conIds[i], conNames[i], "%");
        addAndMakeVisible (*conKnobs[i]);
    }
    conMicMixKnob = std::make_unique<miamp_rig::StudioMiniKnob> (audioProcessor.apvts, MiAmpParams::cabMicMix, "MIC MIX", "%");
    conRoomKnob   = std::make_unique<miamp_rig::StudioMiniKnob> (audioProcessor.apvts, MiAmpParams::cabRoom, "ROOM", "%");
    addAndMakeVisible (*conMicMixKnob);
    addAndMakeVisible (*conRoomKnob);

    // Layer 3 Bare Draggable Microphones (MI-57 & MI-87)
    mic1Body = std::make_unique<miamp_rig::RigMicBody> (audioProcessor.apvts, false, imgs.mic57,
        [this] (const juce::String& t) { showHud (t); },
        [this] { layoutAll(); repaint(); });
    mic2Body = std::make_unique<miamp_rig::RigMicBody> (audioProcessor.apvts, true, imgs.mic87,
        [this] (const juce::String& t) { showHud (t); },
        [this] { layoutAll(); repaint(); });
    addChildComponent (*mic1Body);
    addChildComponent (*mic2Body);

    // Bass Head Knobs & Switches
    for (int i = 0; i < 5; ++i)
    {
        bassHeadKnobs[i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, kBassHeadKnobIds[i], kBassHeadKnobNames[i],
            imgs.bassKnobCap, juce::Image(), KnobKind::BassHead,
            [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*bassHeadKnobs[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        bassHeadSwitches[i] = std::make_unique<ImageToggleSwitch> (
            audioProcessor.apvts, kBassSwitchIds[i], kBassSwitchNames[i],
            imgs.toggleUp, imgs.toggleDown, kBassSwitchInvert[i],
            [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*bassHeadSwitches[i]);
    }

    // Guitar Head Knobs (19) & Switches (3)
    const char* topNames[3] = { "GAIN", "TREBLE", "MID" };
    const char* botNames[3] = { "BASS", "PRESENCE", "MASTER" };
    for (int i = 0; i < 9; ++i)
    {
        const int ch = i / 3;
        const int col = i % 3;
        const char* topId = (col == 0) ? MiAmpParams::gtrChGain[ch]
                          : (col == 1) ? MiAmpParams::gtrChTreble[ch]
                                       : MiAmpParams::gtrChMid[ch];
        const char* botId = (col == 0) ? MiAmpParams::gtrChBass[ch]
                          : (col == 1) ? MiAmpParams::gtrChPresence[ch]
                                       : MiAmpParams::gtrChMaster[ch];

        auto switchChFn = [this, ch]
        {
            if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::gtrChannel))
                p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (ch)));
        };

        guitarHeadKnobs[i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, topId,
            "CH" + juce::String (ch + 1) + " " + topNames[col],
            imgs.bassKnobCap, imgs.guitarKnobRing, KnobKind::GuitarHead,
            [this] (const juce::String& t) { showHud (t); }, switchChFn);
        addChildComponent (*guitarHeadKnobs[i]);

        guitarHeadKnobs[9 + i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, botId,
            "CH" + juce::String (ch + 1) + " " + botNames[col],
            imgs.bassKnobCap, imgs.guitarKnobRing, KnobKind::GuitarHead,
            [this] (const juce::String& t) { showHud (t); }, switchChFn);
        addChildComponent (*guitarHeadKnobs[9 + i]);
    }
    guitarHeadKnobs[18] = std::make_unique<ImageRotaryKnob> (
        audioProcessor.apvts, MiAmpParams::gtrOutput, "OUTPUT",
        imgs.bassKnobCap, imgs.guitarKnobRing, KnobKind::GuitarHead,
        [this] (const juce::String& t) { showHud (t); });
    addChildComponent (*guitarHeadKnobs[18]);

    for (int i = 0; i < 3; ++i)
    {
        guitarHeadSwitches[i] = std::make_unique<ImageToggleSwitch> (
            audioProcessor.apvts, kGtrSwitchIds[i], kGtrSwitchNames[i],
            imgs.toggleUp, imgs.toggleDown, kGtrSwitchInvert[i],
            [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*guitarHeadSwitches[i]);
    }

    // Floor editor controls
    for (int i = 0; i < 7; ++i)
    {
        editorKnobs[i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, kXvKnobIds[i], kXvKnobNames[i],
            imgs.xvKnobPtr, juce::Image(), KnobKind::Pedal,
            [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*editorKnobs[i]);

        eqFaders[i] = std::make_unique<EqFader> (
            audioProcessor.apvts, imgs.pedSliderCap,
            [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*eqFaders[i]);
    }

    for (int i = 0; i < 3; ++i)
    {
        pedalToggles[i] = std::make_unique<PedalToggleOverlay> (
            audioProcessor.apvts, imgs.toggleUp, imgs.toggleDown,
            [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*pedalToggles[i]);
    }

    addChildComponent (btnEditClose);
    btnEditClose.setComponentID ("popoverClose");
    btnEditClose.onClick = [this] { editorOpen = false; syncPedalEditorControls(); repaint(); };

    addChildComponent (btnMoveLeft);
    btnMoveLeft.onClick = [this]
    {
        const bool bass = audioProcessor.isBassMode();
        const int sel = bass ? selectedBassPedal : selectedGuitarPedal;
        audioProcessor.movePedalInChain (bass, sel, -1);
        syncPedalEditorControls();
        repaint();
    };
    addChildComponent (btnMoveRight);
    btnMoveRight.onClick = [this]
    {
        const bool bass = audioProcessor.isBassMode();
        const int sel = bass ? selectedBassPedal : selectedGuitarPedal;
        audioProcessor.movePedalInChain (bass, sel, +1);
        syncPedalEditorControls();
        repaint();
    };
    addChildComponent (btnTrashPedal);
    btnTrashPedal.onClick = [this]
    {
        const bool bass = audioProcessor.isBassMode();
        int& sel = bass ? selectedBassPedal : selectedGuitarPedal;
        audioProcessor.removePedalFromChain (bass, sel);
        auto ch = audioProcessor.getActiveChain (bass);
        if (! ch.empty()) sel = ch.front();
        else editorOpen = false;
        syncPedalEditorControls();
        repaint();
    };

    addChildComponent (btnHeadModeAmp);
    btnHeadModeAmp.onClick = [this]
    {
        audioProcessor.setHeadBypassed (audioProcessor.isBassMode(), false);
        showHud (miamp_i18n::tr (miamp_i18n::Msg::HudWithAmp));
        syncHeadControls();
        repaint();
    };
    addChildComponent (btnHeadModePedals);
    btnHeadModePedals.onClick = [this]
    {
        audioProcessor.setHeadBypassed (audioProcessor.isBassMode(), true);
        showHud (miamp_i18n::tr (miamp_i18n::Msg::HudPedalsOnly));
        syncHeadControls();
        repaint();
    };

    addChildComponent (paletteStrip);
    paletteStrip.setInterceptsMouseClicks (false, true);
    for (int idx = 0; idx < 14; ++idx)
    {
        paletteItems[idx] = std::make_unique<PalettePedalButton> ("", juce::Image(), nullptr);
        paletteStrip.addAndMakeVisible (*paletteItems[idx]);
    }

    addChildComponent (btnPaletteLeft);
    btnPaletteLeft.onClick = [this]
    {
        paletteScrollTarget = juce::jmax (0.0f, std::round (paletteScrollTarget / 121.0f) * 121.0f - 363.0f);
    };
    addChildComponent (btnPaletteRight);
    btnPaletteRight.onClick = [this]
    {
        const int nPal = audioProcessor.isBassMode() ? 14 : 13;
        const float maxScroll = juce::jmax (0.0f, static_cast<float> (nPal - 8) * 121.0f);
        paletteScrollTarget = juce::jmin (maxScroll, std::round (paletteScrollTarget / 121.0f) * 121.0f + 363.0f);
    };

    addChildComponent (btnFloorPresets);
    btnFloorPresets.onClick = [this] { togglePopover (PopoverKind::Presets); };
    addChildComponent (btnFloorTuner);
    btnFloorTuner.onClick = [this]
    {
        floorTunerOpen = ! floorTunerOpen;
        if (floorTunerOpen && activePopover != PopoverKind::None)
            togglePopover (PopoverKind::None);
        syncPedalEditorControls();
        layoutAll();
        repaint();
    };
    addChildComponent (btnFloorToneFinder);
    btnFloorToneFinder.onClick = [this] { togglePopover (PopoverKind::ToneFinder); };
    addChildComponent (btnFloorDrop);
    btnFloorDrop.onClick = [this] { togglePopover (PopoverKind::Drop); };
    addChildComponent (btnFloorConfig);
    btnFloorConfig.onClick = [this] { togglePopover (PopoverKind::Config); };
    addChildComponent (btnBackToAmp);
    btnBackToAmp.onClick = [this] { setFloorMode (false); };

    addChildComponent (btnFloorTunerNotation);
    btnFloorTunerNotation.onClick = [this]
    {
        useSolfege = ! useSolfege;
        repaint();
    };
    addChildComponent (btnFloorTunerMute);
    btnFloorTunerMute.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::tunerMuted))
            p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
        repaint();
    };

    popover = std::make_unique<GlassPopover> (
        audioProcessor, useSolfege,
        [this] { togglePopover (PopoverKind::None); },
        [this] { syncHeadControls(); syncPedalEditorControls(); layoutAll(); repaint(); },
        [this] { applyLanguageTexts(); },
        [this] { startAutoUpdateCheck (true); });
    popover->setLookAndFeel (&glassLaf);
    addChildComponent (*popover);

    updateOverlay = std::make_unique<UpdateOverlayComponent> (*this);
    addChildComponent (*updateOverlay);

    rigBrowser = std::make_unique<miamp_rig::RigBrowserOverlay> (
        audioProcessor,
        [this] { syncHeadControls(); layoutAll(); repaint(); },
        [this] { if (rigBrowser) rigBrowser->closeAnimated(); });
    rigBrowser->setLookAndFeel (&glassLaf);
    addChildComponent (*rigBrowser);

    studioFxModal = std::make_unique<miamp_rig::StudioFxModal> (
        audioProcessor,
        [this] { if (studioFxModal) studioFxModal->closeAnimated(); syncHeadControls(); });
    studioFxModal->setLookAndFeel (&glassLaf);
    addChildComponent (*studioFxModal);

    updateOverlay->addChildComponent (btnUpdateNow);
    btnUpdateNow.setLookAndFeel (&glassLaf);
    btnUpdateNow.setComponentID ("presetSave");
    btnUpdateNow.onClick = [this] { beginAutoUpdateDownload (pendingUpdate); };

    updateOverlay->addChildComponent (btnUpdateLater);
    btnUpdateLater.setLookAndFeel (&glassLaf);
    btnUpdateLater.onClick = [this] { dismissUpdateModal(); };

    applyLanguageTexts();

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (kStageW / kStageH);

    setResizable (true, true);
    setResizeLimits (775, 384, 2325, 1152);
    setSize (1240, 614);

    // Start with the 2.9s intro animation matching index.html!
    introRunning = true;
    introElapsed = 0.0f;
    headSceneYOffsetStage = 1156.0f;
    setFloorMode (false);
    startTimerHz (60);

    if (miamp_standalone::isRunningStandalone())
        juce::MessageManager::callAsync ([] { miamp_standalone::ensureAudioInputActive(); });

    if (! miamp_updater::startupCheckDoneFlag().exchange (true))
    {
        juce::Component::SafePointer<MiAmpAudioProcessorEditor> safeEd (this);
        juce::MessageManager::callAsync ([safeEd]
        {
            if (safeEd != nullptr)
                safeEd->startAutoUpdateCheck (false);
        });
    }
}

MiAmpAudioProcessorEditor::~MiAmpAudioProcessorEditor()
{
    stopTimer();
    if (updateCheckThread != nullptr)
        updateCheckThread->stopThread (800);
    if (updateDownloadThread != nullptr)
        updateDownloadThread->stopThread (1200);
    for (auto* b : { &btnSkipIntro, &btnCreditClose,
                     &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainToneFinder, &btnMainPedals, &btnMainCabs, &btnMainStudioFx,
                     &btnDockDrop, &btnDockConfig, &btnViewCloseUp, &btnViewRigMics,
                     &btnAmpPrev, &btnAmpNext, &btnAmpBrowse, &btnCabPrev, &btnCabNext, &btnCabBrowse, &btnCabLoadWav,
                     &btnMic1On, &btnMic1Dist, &btnMic1Phase, &btnMic2On, &btnMic2Dist, &btnMic2Phase,
                     &btnConsoleStudioFx, &btnConsolePower,
                     &btnEditClose, &btnMoveLeft, &btnMoveRight,
                     &btnHeadModeAmp, &btnHeadModePedals, &btnPaletteLeft, &btnPaletteRight,
                     &btnFloorPresets, &btnFloorTuner, &btnFloorToneFinder, &btnFloorDrop, &btnFloorConfig, &btnBackToAmp,
                     &btnFloorTunerMute, &btnFloorTunerNotation, &btnUpdateNow, &btnUpdateLater })
    {
        b->setLookAndFeel (nullptr);
    }
    if (cabPickerPanel != nullptr)
        cabPickerPanel->setLookAndFeel (nullptr);
    if (rigBrowser != nullptr)
        rigBrowser->setLookAndFeel (nullptr);
    if (studioFxModal != nullptr)
        studioFxModal->setLookAndFeel (nullptr);
    if (popover != nullptr)
        popover->setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MiAmpAudioProcessorEditor::loadImages()
{
    auto img = [] (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); };

    imgs.introSky       = img (BinaryData::intro_sky_png,        BinaryData::intro_sky_pngSize);
    imgs.introLogo      = img (BinaryData::intro_logo_png,       BinaryData::intro_logo_pngSize);
    imgs.popupsCredit   = img (BinaryData::popups_credit_png,    BinaryData::popups_credit_pngSize);
    imgs.guitarKnobRing = img (BinaryData::guitar_knob_ring_png, BinaryData::guitar_knob_ring_pngSize);
    imgs.skateparkTall  = img (BinaryData::skatepark_tall_capa0_jpg, BinaryData::skatepark_tall_capa0_jpgSize);
    imgs.mic57          = img (BinaryData::mic_mi57_clean_png,   BinaryData::mic_mi57_clean_pngSize);
    imgs.mic87          = img (BinaryData::mic_mi87_clean_png,   BinaryData::mic_mi87_clean_pngSize);

    imgs.guitarBg    = img (BinaryData::main_bg_png,       BinaryData::main_bg_pngSize);
    imgs.bassBg      = img (BinaryData::bass_bg_png,       BinaryData::bass_bg_pngSize);
    imgs.bassKnobCap = img (BinaryData::bass_knob_cap_png, BinaryData::bass_knob_cap_pngSize);
    imgs.toggleUp    = img (BinaryData::toggle_up_png,     BinaryData::toggle_up_pngSize);
    imgs.toggleDown  = img (BinaryData::toggle_down_png,   BinaryData::toggle_down_pngSize);

    imgs.floorBg      = img (BinaryData::floor_bg_png,      BinaryData::floor_bg_pngSize);
    imgs.boardSurface = img (BinaryData::board_surface_png, BinaryData::board_surface_pngSize);

    imgs.xvActive   = img (BinaryData::xv_active_png,    BinaryData::xv_active_pngSize);
    imgs.xvBypass   = img (BinaryData::xv_bypass_png,    BinaryData::xv_bypass_pngSize);
    imgs.xvKnobPtr  = img (BinaryData::xv_knob_ptr_png,  BinaryData::xv_knob_ptr_pngSize);
    imgs.xvCabBass  = img (BinaryData::xv_cab_bass_png,  BinaryData::xv_cab_bass_pngSize);
    imgs.xvCabPunk  = img (BinaryData::xv_cab_punk_png,  BinaryData::xv_cab_punk_pngSize);
    imgs.xvCabClean = img (BinaryData::xv_cab_clean_png, BinaryData::xv_cab_clean_pngSize);

    auto loadPed = [&](ArtKey key, const void* bodyD, int bodySz, const void* knobD, int knobSz)
    {
        const int idx = static_cast<int> (key);
        imgs.pedBodies[idx] = img (bodyD, bodySz);
        if (knobD != nullptr && knobSz > 0)
            imgs.pedKnobs[idx] = img (knobD, knobSz);
    };

    loadPed (ArtKey::MiOrange,     BinaryData::ped2_mi_orange_body_png,      BinaryData::ped2_mi_orange_body_pngSize,
                                   BinaryData::ped2_mi_orange_knob_png,      BinaryData::ped2_mi_orange_knob_pngSize);
    loadPed (ArtKey::MiGreenDrive, BinaryData::ped2_mi_green_drive_body_png, BinaryData::ped2_mi_green_drive_body_pngSize,
                                   BinaryData::ped2_mi_green_drive_knob_png, BinaryData::ped2_mi_green_drive_knob_pngSize);
    loadPed (ArtKey::MiDualCrunch, BinaryData::ped2_mi_dual_crunch_body_png, BinaryData::ped2_mi_dual_crunch_body_pngSize,
                                   BinaryData::ped2_mi_dual_crunch_knob_png, BinaryData::ped2_mi_dual_crunch_knob_pngSize);
    loadPed (ArtKey::MiSustain,    BinaryData::ped2_mi_sustain_body_png,     BinaryData::ped2_mi_sustain_body_pngSize,
                                   BinaryData::ped2_mi_sustain_knob_png,     BinaryData::ped2_mi_sustain_knob_pngSize);
    loadPed (ArtKey::MiIceComp,    BinaryData::ped2_mi_ice_comp_body_png,    BinaryData::ped2_mi_ice_comp_body_pngSize,
                                   BinaryData::ped2_mi_ice_comp_knob_png,    BinaryData::ped2_mi_ice_comp_knob_pngSize);
    loadPed (ArtKey::MiPhaser,     BinaryData::ped2_mi_phaser_body_png,      BinaryData::ped2_mi_phaser_body_pngSize,
                                   BinaryData::ped2_mi_phaser_knob_png,      BinaryData::ped2_mi_phaser_knob_pngSize);
    loadPed (ArtKey::MiSkyChorus,  BinaryData::ped2_mi_sky_chorus_body_png,  BinaryData::ped2_mi_sky_chorus_body_pngSize,
                                   BinaryData::ped2_mi_sky_chorus_knob_png,  BinaryData::ped2_mi_sky_chorus_knob_pngSize);
    loadPed (ArtKey::MiViolet,     BinaryData::ped2_mi_violet_body_png,      BinaryData::ped2_mi_violet_body_pngSize,
                                   BinaryData::ped2_mi_violet_knob_png,      BinaryData::ped2_mi_violet_knob_pngSize);
    loadPed (ArtKey::MiPearl,      BinaryData::ped2_mi_pearl_body_png,       BinaryData::ped2_mi_pearl_body_pngSize,
                                   BinaryData::ped2_mi_pearl_knob_png,       BinaryData::ped2_mi_pearl_knob_pngSize);
    loadPed (ArtKey::MiSilent,     BinaryData::ped2_mi_silent_body_png,      BinaryData::ped2_mi_silent_body_pngSize,
                                   BinaryData::ped2_mi_silent_knob_png,      BinaryData::ped2_mi_silent_knob_pngSize);
    loadPed (ArtKey::MiSubOctave,  BinaryData::ped2_mi_sub_octave_body_png,  BinaryData::ped2_mi_sub_octave_body_pngSize,
                                   BinaryData::ped2_mi_sub_octave_knob_png,  BinaryData::ped2_mi_sub_octave_knob_pngSize);
    loadPed (ArtKey::MiGraphicEq,  BinaryData::ped2_mi_graphic_eq_body_png,  BinaryData::ped2_mi_graphic_eq_body_pngSize,
                                   nullptr, 0);
    loadPed (ArtKey::MiStudio,     BinaryData::ped2_mi_studio_body_png,      BinaryData::ped2_mi_studio_body_pngSize,
                                   BinaryData::ped2_mi_studio_knob_png,      BinaryData::ped2_mi_studio_knob_pngSize);
    loadPed (ArtKey::MiGold,       BinaryData::ped2_mi_gold_body_png,        BinaryData::ped2_mi_gold_body_pngSize,
                                   BinaryData::ped2_mi_gold_knob_png,        BinaryData::ped2_mi_gold_knob_pngSize);
    loadPed (ArtKey::BassFood,     BinaryData::ped2_bass_food_body_png,      BinaryData::ped2_bass_food_body_pngSize,
                                   BinaryData::ped2_bass_food_knob_png,      BinaryData::ped2_bass_food_knob_pngSize);
    loadPed (ArtKey::MiBassDriver, BinaryData::ped2_mi_bass_driver_body_png, BinaryData::ped2_mi_bass_driver_body_pngSize,
                                   BinaryData::ped2_mi_bass_driver_knob_png, BinaryData::ped2_mi_bass_driver_knob_pngSize);
    loadPed (ArtKey::MiOlive,      BinaryData::ped2_mi_olive_body_png,       BinaryData::ped2_mi_olive_body_pngSize,
                                   BinaryData::ped2_mi_olive_knob_png,       BinaryData::ped2_mi_olive_knob_pngSize);
    loadPed (ArtKey::Mi87,         BinaryData::ped2_mi_87_body_png,          BinaryData::ped2_mi_87_body_pngSize,
                                   BinaryData::ped2_mi_87_knob_png,          BinaryData::ped2_mi_87_knob_pngSize);
    loadPed (ArtKey::MiLimePhase,  BinaryData::ped2_mi_lime_phase_body_png,  BinaryData::ped2_mi_lime_phase_body_pngSize,
                                   BinaryData::ped2_mi_lime_phase_knob_png,  BinaryData::ped2_mi_lime_phase_knob_pngSize);
    loadPed (ArtKey::MiDeepChorus, BinaryData::ped2_mi_deep_chorus_body_png, BinaryData::ped2_mi_deep_chorus_body_pngSize,
                                   BinaryData::ped2_mi_deep_chorus_knob_png, BinaryData::ped2_mi_deep_chorus_knob_pngSize);
    loadPed (ArtKey::MiPolyOctave, BinaryData::ped2_mi_poly_octave_body_png, BinaryData::ped2_mi_poly_octave_body_pngSize,
                                   BinaryData::ped2_mi_poly_octave_knob_png, BinaryData::ped2_mi_poly_octave_knob_pngSize);
    loadPed (ArtKey::MiBassEq,     BinaryData::ped2_mi_bass_eq_body_png,     BinaryData::ped2_mi_bass_eq_body_pngSize,
                                   nullptr, 0);

    imgs.pedSliderCap = img (BinaryData::ped2_slider_eq_cap_png, BinaryData::ped2_slider_eq_cap_pngSize);
}

juce::Rectangle<float> MiAmpAudioProcessorEditor::toScreen (float x, float y, float w, float h) const noexcept
{
    const float s = stageScale();
    return { x * s, y * s, w * s, h * s };
}

juce::Rectangle<int> MiAmpAudioProcessorEditor::toScreenInt (float x, float y, float w, float h) const noexcept
{
    return toScreen (x, y, w, h).toNearestInt();
}

juce::Point<float> MiAmpAudioProcessorEditor::editorPtToStage (juce::Point<float> editorPt) const noexcept
{
    const float s = juce::jmax (0.001f, stageScale());
    return { editorPt.x / s, editorPt.y / s };
}

juce::Point<float> MiAmpAudioProcessorEditor::applyBoardTransform (juce::Point<float> p) const noexcept
{
    // .board-group { transform-origin: 1045px 345px; transform: translate(tx, ty) scale(boardScale); }
    const float ox = 1045.0f, oy = 345.0f;
    return {
        ox + (p.x - ox) * boardScale + boardTx,
        oy + (p.y - oy) * boardScale + boardTy
    };
}

juce::Rectangle<float> MiAmpAudioProcessorEditor::applyBoardTransform (juce::Rectangle<float> r) const noexcept
{
    const auto tl = applyBoardTransform (juce::Point<float> (r.getX(), r.getY()));
    return { tl.x, tl.y, r.getWidth() * boardScale, r.getHeight() * boardScale };
}

void MiAmpAudioProcessorEditor::finishIntro()
{
    if (! introRunning) return;
    introRunning = false;
    headSceneYOffsetStage = 0.0f;
    btnSkipIntro.setVisible (false);
    for (auto* b : { &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainToneFinder, &btnMainPedals, &btnMainCabs, &btnMainStudioFx, &btnDockConfig })
        b->setAlpha (1.0f);
    if (miamp_standalone::isRunningStandalone())
        miamp_standalone::ensureAudioInputActive();
    syncHeadControls();
    layoutAll();
    repaint();
}

juce::String MiAmpAudioProcessorEditor::pedalName (bool bassMode, int type)
{
    return mipedalui::getPedalSpec (bassMode, type).name;
}

const juce::Image& MiAmpAudioProcessorEditor::pedalImage (bool bassMode, int type, bool active) const
{
    const auto& spec = mipedalui::getPedalSpec (bassMode, type);
    if (spec.artKey < 0)
        return active ? imgs.xvActive : imgs.xvBypass;
    return imgs.pedBodies[spec.artKey];
}

const char* MiAmpAudioProcessorEditor::pedalBypassParamId (bool bassMode, int type)
{
    return mipedalui::getPedalSpec (bassMode, type).bypassParamId;
}

void MiAmpAudioProcessorEditor::applyLanguageTexts()
{
    btnSkipIntro.setButtonText       (miamp_i18n::tr (miamp_i18n::Msg::BtnSkip));
    btnCreditClose.setButtonText     (miamp_i18n::tr (miamp_i18n::Msg::BtnClose));
    btnInstGuitar.setButtonText      (miamp_i18n::tr (miamp_i18n::Msg::BtnGuitar));
    btnInstBass.setButtonText        (miamp_i18n::tr (miamp_i18n::Msg::BtnBass));
    btnMainPresets.setButtonText     (miamp_i18n::tr (miamp_i18n::Msg::BtnPresets));
    btnMainTuner.setButtonText       (miamp_i18n::tr (miamp_i18n::Msg::BtnTuner));
    btnMainToneFinder.setButtonText  (miamp_i18n::tr (miamp_i18n::Msg::BtnToneFinder));
    btnMainPedals.setButtonText      (miamp_i18n::tr (miamp_i18n::Msg::BtnPedals));
    btnMainCabs.setButtonText        (miamp_i18n::tr (miamp_i18n::Msg::BtnCabs));
    btnMainStudioFx.setButtonText    (miamp_i18n::tr (miamp_i18n::Msg::BtnStudioFx));
    btnDockConfig.setButtonText      (miamp_i18n::tr (miamp_i18n::Msg::BtnConfig));

    btnHeadModeAmp.setButtonText     (miamp_i18n::tr (miamp_i18n::Msg::BtnWithAmp));
    btnHeadModePedals.setButtonText  (miamp_i18n::tr (miamp_i18n::Msg::BtnPedalsOnly));
    btnFloorPresets.setButtonText    (miamp_i18n::tr (miamp_i18n::Msg::BtnPresets));
    btnFloorTuner.setButtonText      (miamp_i18n::tr (miamp_i18n::Msg::BtnTuner));
    btnFloorToneFinder.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnToneFinder));
    btnFloorConfig.setButtonText     (miamp_i18n::tr (miamp_i18n::Msg::BtnConfig));
    btnBackToAmp.setButtonText       (miamp_i18n::tr (miamp_i18n::Msg::BtnBackToAmp));
    btnFloorTunerMute.setButtonText  (miamp_i18n::tr (miamp_i18n::Msg::TunerMuteShort));
    btnFloorTunerNotation.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::TunerNotesSolfege));

    btnUpdateNow.setButtonText       (miamp_i18n::tr (miamp_i18n::Msg::UpdBtnNow));
    btnUpdateLater.setButtonText     (miamp_i18n::tr (updateDownloading ? miamp_i18n::Msg::UpdBtnCancel
                                                                        : miamp_i18n::Msg::UpdBtnLater));
    if (popover != nullptr)
        popover->refreshLanguage();
    repaint();
}

void MiAmpAudioProcessorEditor::startAutoUpdateCheck (bool manualFromConfig)
{
    if (updateCheckThread != nullptr && updateCheckThread->isThreadRunning())
        return;

    juce::Component::SafePointer<MiAmpAudioProcessorEditor> safeEd (this);
    updateCheckThread = std::make_unique<miamp_updater::CheckThread> (
        [safeEd, manualFromConfig] (miamp_updater::UpdateInfo info)
        {
            if (safeEd == nullptr) return;
            if (! info.networkOk)
            {
                if (manualFromConfig && safeEd->popover != nullptr)
                    safeEd->popover->setUpdateStatusMessage (miamp_i18n::tr (miamp_i18n::Msg::CfgUpdateCheckErr));
                return;
            }

            if (info.available)
            {
                safeEd->pendingUpdate = info;
                safeEd->updateFailed = false;
                safeEd->updateInstalling = false;
                safeEd->updateProgress = 0.0f;

                if (miamp_standalone::isRunningStandalone())
                {
                    if (safeEd->introRunning)
                        safeEd->finishIntro();
                    safeEd->beginAutoUpdateDownload (info);
                }
                else
                {
                    safeEd->updateModalVisible = true;
                    safeEd->updateDownloading = false;
                    safeEd->btnUpdateNow.setVisible (true);
                    safeEd->btnUpdateLater.setVisible (true);
                    safeEd->btnUpdateLater.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::UpdBtnLater));
                    if (safeEd->updateOverlay != nullptr)
                    {
                        safeEd->updateOverlay->setVisible (true);
                        safeEd->updateOverlay->toFront (true);
                    }
                    safeEd->layoutAll();
                    safeEd->repaint();
                }
            }
            else if (manualFromConfig && safeEd->popover != nullptr)
            {
                safeEd->popover->setUpdateStatusMessage (miamp_i18n::tr (miamp_i18n::Msg::CfgUpToDate));
            }
        },
        manualFromConfig);
    updateCheckThread->startThread();
}

void MiAmpAudioProcessorEditor::beginAutoUpdateDownload (const miamp_updater::UpdateInfo& info)
{
    if (updateDownloadThread != nullptr && updateDownloadThread->isThreadRunning())
        return;

    updateModalVisible = true;
    updateDownloading  = true;
    updateInstalling   = false;
    updateFailed       = false;
    updateProgress     = 0.04f;
    btnUpdateNow.setVisible (false);
    btnUpdateLater.setVisible (true);
    btnUpdateLater.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::UpdBtnCancel));
    if (updateOverlay != nullptr)
    {
        updateOverlay->setVisible (true);
        updateOverlay->toFront (true);
    }
    layoutAll();
    repaint();

    juce::Component::SafePointer<MiAmpAudioProcessorEditor> safeEd (this);
    updateDownloadThread = std::make_unique<miamp_updater::DownloadThread> (
        info.downloadUrl, info.remoteBuild,
        [safeEd] (float prog)
        {
            if (safeEd == nullptr || ! safeEd->updateDownloading) return;
            safeEd->updateProgress = prog;
            if (safeEd->updateOverlay != nullptr)
                safeEd->updateOverlay->repaint();
        },
        [safeEd] (bool ok, const juce::File& file)
        {
            if (safeEd == nullptr || ! safeEd->updateModalVisible) return;
            safeEd->updateDownloading = false;
            if (! ok)
            {
                safeEd->updateFailed = true;
                safeEd->btnUpdateNow.setVisible (false);
                safeEd->btnUpdateLater.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnClose));
                safeEd->layoutAll();
                if (safeEd->updateOverlay != nullptr)
                    safeEd->updateOverlay->repaint();
                return;
            }

            if (miamp_standalone::isRunningStandalone())
            {
                safeEd->updateInstalling = true;
                safeEd->updateProgress = 1.0f;
                safeEd->btnUpdateLater.setVisible (false);
                if (safeEd->updateOverlay != nullptr)
                    safeEd->updateOverlay->repaint();
                if (! miamp_updater::launchStandaloneSilentUpdateAndQuit (file))
                {
                    file.startAsProcess();
                    safeEd->dismissUpdateModal();
                }
            }
            else
            {
                file.startAsProcess();
                safeEd->dismissUpdateModal();
            }
        });
    updateDownloadThread->startThread();
}

void MiAmpAudioProcessorEditor::dismissUpdateModal()
{
    if (updateDownloadThread != nullptr && updateDownloadThread->isThreadRunning())
        updateDownloadThread->signalThreadShouldExit();
    updateModalVisible = false;
    updateDownloading  = false;
    updateInstalling   = false;
    updateFailed       = false;
    btnUpdateNow.setVisible (false);
    btnUpdateLater.setVisible (false);
    if (updateOverlay != nullptr)
        updateOverlay->setVisible (false);
    repaint();
}

void MiAmpAudioProcessorEditor::showHud (const juce::String& text)
{
    hudText = text;
    hudTicks = 110; // ~1.8s at 60Hz
    repaint();
}

void MiAmpAudioProcessorEditor::closeFloatingOverlays (const char* except)
{
    const juce::String ex (except != nullptr ? except : "");
    if (ex != "popover" && activePopover != PopoverKind::None)
    {
        activePopover = PopoverKind::None;
        popoverClosing = true;
    }
    if (ex != "studioFx" && studioFxModal != nullptr && studioFxModal->isOpenTarget())
        studioFxModal->closeAnimated();
    if (ex != "rigBrowser" && rigBrowser != nullptr && rigBrowser->isOpenTarget())
        rigBrowser->closeAnimated();
    if (ex != "cabDrawer" && cabDrawerOpen)
    {
        cabDrawerOpen = false;
        cabMicZoom = false;
    }
}

void MiAmpAudioProcessorEditor::cycleActiveHead (int delta)
{
    if (isFloorMode || introRunning) return;
    const bool bass = audioProcessor.isBassMode();
    const auto& headList = bass ? miamp_rig::getBassHeadSpecs() : miamp_rig::getGuitarHeadSpecs();
    const char* id = bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel;
    const int n = static_cast<int> (headList.size());
    const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (id)->load()));
    const int nxt = (cur + delta + n) % n;
    miamp_rig::setParamPlain (audioProcessor.apvts, id, static_cast<float> (nxt));
    showHud (headList[static_cast<size_t> (nxt)].displayName);
    syncHeadControls();
    layoutAll();
    repaint();
}

bool MiAmpAudioProcessorEditor::keyPressed (const juce::KeyPress& key)
{
    if (creditOpen)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            creditOpen = false;
            btnCreditClose.setVisible (false);
            repaint();
            return true;
        }
        return false;
    }
    if (isFloorMode || introRunning)
        return false;

    if (key == juce::KeyPress::escapeKey)
    {
        closeFloatingOverlays();
        consoleOpen = false;
        closeUpView = true;
        syncHeadControls();
        layoutAll();
        repaint();
        return true;
    }

    const bool modalBlocking = (studioFxModal != nullptr && studioFxModal->isVisible())
                            || (rigBrowser != nullptr && rigBrowser->isVisible());
    if (! modalBlocking)
    {
        if (key == juce::KeyPress::leftKey)  { cycleActiveHead (-1); return true; }
        if (key == juce::KeyPress::rightKey) { cycleActiveHead (+1); return true; }
    }
    return false;
}

void MiAmpAudioProcessorEditor::switchInstrument (bool toBass)
{
    if (audioProcessor.isBassMode() == toBass)
        return;

    if (! isFloorMode && ! introRunning && getWidth() > 0 && getHeight() > 0)
    {
        instOutSnapshot = juce::Image (juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics snapG (instOutSnapshot);
        paintRigOnly (snapG, 0.0f, 1.0f);
        instSwitching = true;
        instSwitchAnim = 0.0f;
    }
    else
    {
        instSwitching = false;
        instSwitchAnim = 1.0f;
        instOutSnapshot = {};
    }

    audioProcessor.setInstrumentMode (toBass);
    if (activePopover != PopoverKind::None && popover != nullptr)
        popover->setKind (activePopover, tunerReading);
    cabMicZoom = false;
    animPedalHasPos.fill (false);
    paletteScrollTarget = 0.0f;
    paletteScrollCur = 0.0f;
    showHud (miamp_i18n::tr (toBass ? miamp_i18n::Msg::HudBass : miamp_i18n::Msg::HudGuitar));
    syncHeadControls();
    syncPedalEditorControls();
    layoutAll();
    repaint();
}

void MiAmpAudioProcessorEditor::setFloorMode (bool floor)
{
    if (floor && introRunning)
        finishIntro();

    isFloorMode = floor;
    instSwitching = false;
    instSwitchAnim = 1.0f;
    instOutSnapshot = {};
    closeFloatingOverlays();
    if (popover != nullptr) popover->setVisible (false);
    popoverClosing = false;
    popoverAnim = 0.0f;
    rightMenuAnim = 1.0f;
    if (studioFxModal != nullptr) studioFxModal->forceCloseImmediate();
    if (rigBrowser != nullptr) rigBrowser->forceCloseImmediate();
    cabDrawerAnim = 0.0f;
    draggingPedalActive = false;
    dragLiftScale = 1.0f;
    animPedalHasPos.fill (false);

    const bool bass = audioProcessor.isBassMode();
    if (isFloorMode)
    {
        auto ch = audioProcessor.getActiveChain (bass);
        int& sel = bass ? selectedBassPedal : selectedGuitarPedal;
        if (! ch.empty() && std::find (ch.begin(), ch.end(), sel) == ch.end())
            sel = ch.front();
        editorOpen = ! ch.empty();
    }

    syncHeadControls();
    syncPedalEditorControls();
    layoutAll();
    repaint();
}

void MiAmpAudioProcessorEditor::computeRigStageLayout (
    juce::Rectangle<float>& cabRectOut,
    juce::Rectangle<float>& headRectOut,
    juce::Rectangle<float>& grilleRectOut) const
{
    const bool bass = audioProcessor.isBassMode();
    const auto& headList = bass ? miamp_rig::getBassHeadSpecs() : miamp_rig::getGuitarHeadSpecs();
    const auto& cabList  = bass ? miamp_rig::getBassCabSpecs()  : miamp_rig::getGuitarCabSpecs();

    const int headIdx = juce::jlimit (0, static_cast<int> (headList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel)->load())));
    const int cabIdx = juce::jlimit (0, static_cast<int> (cabList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));

    const auto& hSpec = headList[static_cast<size_t> (headIdx)];
    const auto& cSpec = cabList[static_cast<size_t> (cabIdx)];
    const int visualCabIdx = cSpec.isOff ? (bass ? 6 : 7) : cabIdx;
    const auto& visualCab = cabList[static_cast<size_t> (juce::jlimit (0, static_cast<int> (cabList.size()) - 1, visualCabIdx))];

    const auto hImg = juce::ImageCache::getFromMemory (hSpec.chassisData, hSpec.chassisSize);
    const float hAspect = static_cast<float> (hImg.getWidth()) / juce::jmax (1, hImg.getHeight());

    struct CabSeatMetrics { float topRatio; float seatRatio; };
    const auto getCabSeatMetrics = [&] () -> CabSeatMetrics
    {
        if (! bass)
        {
            switch (visualCabIdx)
            {
                case 0: return { 0.820f, 0.036f }; // 4x12 green
                case 1: return { 0.802f, 0.036f }; // 4x12 britv30 (side handles)
                case 2: return { 0.822f, 0.036f }; // 4x12 recto
                case 3: return { 0.831f, 0.036f }; // 4x12 evh
                case 4: return { 0.840f, 0.034f }; // 2x12 classa
                case 5: return { 0.825f, 0.036f }; // 4x12 tangerine
                case 6: return { 0.843f, 0.029f }; // 2x12 twin
                case 7:
                case 8: return { 0.820f, 0.036f }; // 4x12 MI CLASSIC (Mint) + Custom IR
                default: return { 0.820f, 0.036f };
            }
        }
        switch (visualCabIdx)
        {
            case 0: return { 0.836f, 0.037f }; // 1x15 b15
            case 1: return { 0.816f, 0.036f }; // 4x10 aguilar
            case 2: return { 0.819f, 0.036f }; // 4x10 gk
            case 3: return { 0.845f, 0.039f }; // 6x10 heavy
            case 4: return { 0.905f, 0.043f }; // 8x10 classic
            case 5: return { 0.905f, 0.043f }; // 8x10 tangerine
            case 6:
            case 7: return { 0.905f, 0.043f }; // 8x10 MI CLASSIC (Mint) + Custom IR
            default: return { 0.905f, 0.043f };
        }
    };
    const auto cabMetrics = getCabSeatMetrics();
    const float topRatio = cabMetrics.topRatio;
    const float seatRatio = cabMetrics.seatRatio;

    if (closeUpView)
    {
        const float centerX = juce::jmap (cabDrawerAnim, bass ? 665.0f : 675.0f, 870.0f);
        const float actualW = juce::jmap (cabDrawerAnim, bass ? 960.0f : 860.0f, 680.0f);
        const float hh = actualW / juce::jmax (0.5f, hAspect);
        const float cabTopY = juce::jmap (cabDrawerAnim, 555.0f, 505.0f);
        float seatDepth = 0.0f;

        if (visualCab.imageData != nullptr && visualCab.imageSize > 0)
        {
            const auto cImg = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
            const float aspect = static_cast<float> (cImg.getWidth()) / juce::jmax (1, cImg.getHeight());
            const float cw = actualW / topRatio;
            const float ch = cw / juce::jmax (0.2f, aspect);
            seatDepth = cw * seatRatio;
            cabRectOut = { centerX - cw * 0.5f, cabTopY, cw, ch };
            grilleRectOut = {};
        }
        else
        {
            cabRectOut = { centerX - 240.0f, cabTopY + 12.0f, 480.0f, 42.0f };
            grilleRectOut = {};
        }

        const float headTop = cabTopY - hh + seatDepth;
        headRectOut = { centerX - actualW * 0.5f, headTop, actualW, hh };
        return;
    }

    const float centerX = juce::jmap (cabDrawerAnim, 640.0f, 870.0f);
    const float floorY  = consoleOpen ? 618.0f : 720.0f;
    const float rigScale = consoleOpen ? juce::jmap (cabDrawerAnim, 1.0f, 0.96f)
                                       : juce::jmap (cabDrawerAnim, 1.15f, 1.06f);

    float cabTopY = floorY - 38.0f;
    float cabW = 480.0f * rigScale;

    if (visualCab.imageData != nullptr && visualCab.imageSize > 0)
    {
        const auto cImg = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
        const float aspect = static_cast<float> (cImg.getWidth()) / juce::jmax (1, cImg.getHeight());
        float ch = 336.0f * rigScale * visualCab.heightScale;
        float cw = ch * aspect;
        const float maxW = 450.0f * rigScale * visualCab.widthScale;
        if (cw > maxW)
        {
            cw = maxW;
            ch = cw / juce::jmax (0.2f, aspect);
        }
        cabW = cw;
        cabTopY = floorY - ch;
        cabRectOut = { centerX - cw * 0.5f, cabTopY, cw, ch };
        grilleRectOut = cSpec.isOff ? juce::Rectangle<float>{} : juce::Rectangle<float>{
            cabRectOut.getX() + visualCab.grilleX * cabRectOut.getWidth(),
            cabRectOut.getY() + visualCab.grilleY * cabRectOut.getHeight(),
            visualCab.grilleW * cabRectOut.getWidth(),
            visualCab.grilleH * cabRectOut.getHeight()
        };
    }
    else
    {
        cabRectOut = { centerX - 180.0f, floorY - 38.0f, 360.0f, 38.0f };
        grilleRectOut = {};
    }

    // Exact same width as cabinet top surface, seated INSIDE the 3D top depth of the cabinet
    const float actualW = cabW * topRatio;
    const float hh = actualW / juce::jmax (0.5f, hAspect);
    const float seatDepth = cabW * seatRatio;
    headRectOut = { centerX - actualW * 0.5f, cabTopY - hh + seatDepth, actualW, hh };
}

void MiAmpAudioProcessorEditor::rebuildRigHeadControls()
{
    const bool bass = audioProcessor.isBassMode();
    const auto& headList = bass ? miamp_rig::getBassHeadSpecs() : miamp_rig::getGuitarHeadSpecs();
    const int headIdx = juce::jlimit (0, static_cast<int> (headList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel)->load())));

    if (headIdx == lastBuiltRigModel && bass == lastBuiltRigBass)
        return;

    lastBuiltRigModel = headIdx;
    lastBuiltRigBass  = bass;

    rigKnobs.clear();
    rigSwitches.clear();
    rigSliders.clear();

    const auto& hSpec = headList[static_cast<size_t> (headIdx)];
    if (hSpec.useClassicOverlay)
        return;

    const auto knobSprite = juce::ImageCache::getFromMemory (hSpec.knobSpriteData, hSpec.knobSpriteSize);
    for (const auto& ks : hSpec.knobs)
    {
        auto k = std::make_unique<miamp_rig::RigHeadKnob> (audioProcessor.apvts, ks, knobSprite, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*k);
        rigKnobs.push_back (std::move (k));
    }

    auto switchImgs = [] (miamp_rig::AmpSwitchKind kind) -> std::pair<juce::Image, juce::Image>
    {
        using K = miamp_rig::AmpSwitchKind;
        switch (kind)
        {
            case K::RockerRed:   return { juce::ImageCache::getFromMemory (BinaryData::sw_rocker_red_on_png,   BinaryData::sw_rocker_red_on_pngSize),
                                          juce::ImageCache::getFromMemory (BinaryData::sw_rocker_red_off_png,  BinaryData::sw_rocker_red_off_pngSize) };
            case K::RockerBlack: return { juce::ImageCache::getFromMemory (BinaryData::sw_rocker_black_on_png, BinaryData::sw_rocker_black_on_pngSize),
                                          juce::ImageCache::getFromMemory (BinaryData::sw_rocker_black_off_png,BinaryData::sw_rocker_black_off_pngSize) };
            case K::PushRound:   return { juce::ImageCache::getFromMemory (BinaryData::sw_push_round_on_png,   BinaryData::sw_push_round_on_pngSize),
                                          juce::ImageCache::getFromMemory (BinaryData::sw_push_round_off_png,  BinaryData::sw_push_round_off_pngSize) };
            case K::PushSquare:  return { juce::ImageCache::getFromMemory (BinaryData::sw_push_square_on_png,  BinaryData::sw_push_square_on_pngSize),
                                          juce::ImageCache::getFromMemory (BinaryData::sw_push_square_off_png, BinaryData::sw_push_square_off_pngSize) };
            case K::SlideBlack:  return { juce::ImageCache::getFromMemory (BinaryData::sw_slide_black_on_png,  BinaryData::sw_slide_black_on_pngSize),
                                          juce::ImageCache::getFromMemory (BinaryData::sw_slide_black_off_png, BinaryData::sw_slide_black_off_pngSize) };
            case K::ToggleSilver:
            default:             return { juce::ImageCache::getFromMemory (BinaryData::sw_toggle_silver_on_png,  BinaryData::sw_toggle_silver_on_pngSize),
                                          juce::ImageCache::getFromMemory (BinaryData::sw_toggle_silver_off_png, BinaryData::sw_toggle_silver_off_pngSize) };
        }
    };

    for (const auto& sw : hSpec.switches)
    {
        auto [onImg, offImg] = switchImgs (sw.kind);
        auto s = std::make_unique<miamp_rig::RigHeadSwitch> (audioProcessor.apvts, sw, onImg, offImg, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*s);
        rigSwitches.push_back (std::move (s));
    }

    const auto hqFaderCap = juce::ImageCache::getFromMemory (BinaryData::fader_cap_hq_png, BinaryData::fader_cap_hq_pngSize);
    for (const auto& sl : hSpec.sliders)
    {
        auto f = std::make_unique<miamp_rig::RigHeadSlider> (audioProcessor.apvts, sl, hqFaderCap, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*f);
        rigSliders.push_back (std::move (f));
    }
}

void MiAmpAudioProcessorEditor::syncHeadControls()
{
    const bool bass = audioProcessor.isBassMode();
    rebuildRigHeadControls();

    const bool showHeadUi  = ! isFloorMode && (! introRunning || introElapsed >= 2.42f);
    const bool showAmpFace = ! isFloorMode && (! introRunning || headSceneYOffsetStage < 680.0f);
    const float uiAlpha    = introRunning ? juce::jlimit (0.0f, 1.0f, (introElapsed - 2.42f) / 0.40f) : 1.0f;

    btnInstGuitar.setToggleState (! bass, juce::dontSendNotification);
    btnInstBass.setToggleState   (bass,   juce::dontSendNotification);

    const bool showRightMenu = showHeadUi && (activePopover == PopoverKind::None || rightMenuAnim > 0.01f);
    const float menuFade   = uiAlpha * silkEase (rightMenuAnim);
    for (auto* b : { &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainToneFinder, &btnMainPedals,
                     &btnMainCabs, &btnMainStudioFx, &btnDockConfig })
    {
        b->setVisible (showRightMenu);
        b->setAlpha (showRightMenu ? menuFade : 1.0f);
    }
    btnMainHeads.setVisible   (false);
    btnMainConsole.setVisible (false);
    btnViewCloseUp.setVisible (false);
    btnViewRigMics.setVisible (false);
    btnMainCabs.setToggleState (cabDrawerOpen, juce::dontSendNotification);
    btnDockDrop.setVisible (false);

    // Bottom Analog Console Strip visibility & state (hidden unless consoleOpen is toggled on)
    const bool showConsole = showHeadUi && consoleOpen;
    for (auto* b : { &btnAmpPrev, &btnAmpNext, &btnAmpBrowse, &btnCabPrev, &btnCabNext, &btnCabBrowse, &btnCabLoadWav,
                     &btnMic1On, &btnMic1Dist, &btnMic1Phase, &btnMic2On, &btnMic2Dist, &btnMic2Phase,
                     &btnConsoleStudioFx, &btnConsolePower })
    {
        b->setVisible (showConsole);
        b->setAlpha (showConsole ? uiAlpha : 1.0f);
    }
    for (int i = 0; i < 5; ++i)
        if (conKnobs[i]) { conKnobs[i]->setVisible (showConsole); conKnobs[i]->setAlpha (showConsole ? uiAlpha : 1.0f); }
    if (conMicMixKnob) { conMicMixKnob->setVisible (showConsole); conMicMixKnob->setAlpha (showConsole ? uiAlpha : 1.0f); }
    if (conRoomKnob)   { conRoomKnob->setVisible (showConsole);   conRoomKnob->setAlpha (showConsole ? uiAlpha : 1.0f); }

    const auto& headList = bass ? miamp_rig::getBassHeadSpecs() : miamp_rig::getGuitarHeadSpecs();
    const int headIdx = juce::jlimit (0, static_cast<int> (headList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel)->load())));
    const auto& hSpec = headList[static_cast<size_t> (headIdx)];
    const bool useClassic = hSpec.useClassicOverlay;

    if (btnHeadSidePrev && btnHeadSideNext)
    {
        const int nHeads = static_cast<int> (headList.size());
        const auto& prevSpec = headList[static_cast<size_t> ((headIdx - 1 + nHeads) % nHeads)];
        const auto& nextSpec = headList[static_cast<size_t> ((headIdx + 1) % nHeads)];
        btnHeadSidePrev->setTipText (juce::String ("< ") + prevSpec.displayName);
        btnHeadSideNext->setTipText (juce::String (nextSpec.displayName) + " >");
        btnHeadSidePrev->setVisible (showHeadUi && showAmpFace);
        btnHeadSideNext->setVisible (showHeadUi && showAmpFace);
        btnHeadSidePrev->setAlpha (uiAlpha);
        btnHeadSideNext->setAlpha (uiAlpha);
    }

    const bool showCabDrawer = showHeadUi && (cabDrawerOpen || cabDrawerAnim > 0.01f);
    if (cabPickerPanel)
    {
        cabPickerPanel->setVisible (showCabDrawer);
        if (showCabDrawer)
        {
            cabPickerPanel->setAlpha (silkEase (cabDrawerAnim));
            cabPickerPanel->setMicZoom (cabMicZoom);
            if (cabDrawerOpen)
                cabPickerPanel->refresh();
        }
    }

    const auto& cabList = bass ? miamp_rig::getBassCabSpecs() : miamp_rig::getGuitarCabSpecs();
    const int cabIdx = juce::jlimit (0, static_cast<int> (cabList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
    const bool cabIsOff = cabList[static_cast<size_t> (cabIdx)].isOff;

    float instInAlpha = 1.0f;
    if (instSwitching)
    {
        const float tSec = instSwitchAnim * 0.62f;
        const float uIn = juce::jlimit (0.0f, 1.0f, (tSec - 0.045f) / (0.62f - 0.045f));
        instInAlpha = silkEase (uIn);
    }

    // Show interactive controls directly on the active amp head
    for (auto& k : rigKnobs)    { k->setVisible (showAmpFace && ! useClassic); k->setAlpha (instInAlpha); }
    for (auto& s : rigSwitches) { s->setVisible (showAmpFace && ! useClassic); s->setAlpha (instInAlpha); }
    for (auto& f : rigSliders)  { f->setVisible (showAmpFace && ! useClassic); f->setAlpha (instInAlpha); }

    for (auto& k : bassHeadKnobs)      { k->setVisible (showAmpFace && useClassic && bass);   k->setAlpha (instInAlpha); }
    for (auto& s : bassHeadSwitches)   { s->setVisible (showAmpFace && useClassic && bass);   s->setAlpha (instInAlpha); }
    for (auto& k : guitarHeadKnobs)    { k->setVisible (showAmpFace && useClassic && ! bass); k->setAlpha (instInAlpha); }
    for (auto& s : guitarHeadSwitches) { s->setVisible (showAmpFace && useClassic && ! bass); s->setAlpha (instInAlpha); }

    // Layer 3 bare mics visible when cabinet is not OFF and in Rig & Mics view
    const bool showMics = showAmpFace && ! cabIsOff && ! closeUpView;
    if (mic1Body) mic1Body->setVisible (showMics);
    if (mic2Body) mic2Body->setVisible (showMics);

    // Update Console Quick-Access Tone Knobs to track active instrument & channel
    if (bass)
    {
        const char* bIds[5] = { MiAmpParams::ampGain, MiAmpParams::ampBass, MiAmpParams::ampMid, MiAmpParams::ampTreble,
                                (headIdx == 8 ? MiAmpParams::ampFreq : MiAmpParams::bassMaster) };
        const char* bLbl[5] = { "GAIN", "BASS", "MID", "TREBLE", (headIdx == 8 ? "FREQ" : "MASTER") };
        for (int i = 0; i < 5; ++i)
            if (conKnobs[i]) conKnobs[i]->setParam (bIds[i], bLbl[i]);
    }
    else
    {
        const int ch = juce::jlimit (0, 2, static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrChannel)->load())));
        const char* gIds[5] = { MiAmpParams::gtrChGain[ch], MiAmpParams::gtrChBass[ch], MiAmpParams::gtrChMid[ch],
                                MiAmpParams::gtrChTreble[ch], MiAmpParams::gtrChMaster[ch] };
        const char* gLbl[5] = { "GAIN", "BASS", "MID", "TREBLE", "MASTER" };
        for (int i = 0; i < 5; ++i)
            if (conKnobs[i]) conKnobs[i]->setParam (gIds[i], gLbl[i]);
    }

    // Update mic & power console button labels/toggles
    static constexpr const char* kDistTags[3] = { "CLS", "MID", "FAR" };
    const bool m1On = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic1On)->load() >= 0.5f;
    const bool m2On = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic2On)->load() >= 0.5f;
    const bool m1Ph = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic1Phase)->load() >= 0.5f;
    const bool m2Ph = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic2Phase)->load() >= 0.5f;
    const int  m1D  = juce::jlimit (0, 2, static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic1Dist)->load())));
    const int  m2D  = juce::jlimit (0, 2, static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic2Dist)->load())));
    const bool pwr  = audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::ampPower : MiAmpParams::gtrPower)->load() >= 0.5f;

    btnMic1On.setToggleState    (m1On, juce::dontSendNotification);
    btnMic1Phase.setToggleState (m1Ph, juce::dontSendNotification);
    btnMic1Dist.setButtonText   (kDistTags[m1D]);
    btnMic2On.setToggleState    (m2On, juce::dontSendNotification);
    btnMic2Phase.setToggleState (m2Ph, juce::dontSendNotification);
    btnMic2Dist.setButtonText   (kDistTags[m2D]);
    btnConsolePower.setToggleState (pwr, juce::dontSendNotification);
    btnConsolePower.setButtonText  (pwr ? "POWER ON" : "POWER OFF");

    const bool sfxActive = (audioProcessor.apvts.getRawParameterValue (MiAmpParams::sfxCompOn)->load() >= 0.5f)
                        || (audioProcessor.apvts.getRawParameterValue (MiAmpParams::sfxEqOn)->load()   >= 0.5f)
                        || (audioProcessor.apvts.getRawParameterValue (MiAmpParams::sfxDlyOn)->load()  >= 0.5f)
                        || (audioProcessor.apvts.getRawParameterValue (MiAmpParams::sfxRevOn)->load()  >= 0.5f);
    btnMainStudioFx.setToggleState    (sfxActive || (studioFxModal && studioFxModal->isVisible()), juce::dontSendNotification);
    btnConsoleStudioFx.setToggleState (sfxActive || (studioFxModal && studioFxModal->isVisible()), juce::dontSendNotification);

    const bool headByp = audioProcessor.isHeadBypassed (bass);
    btnHeadModeAmp.setVisible    (isFloorMode);
    btnHeadModePedals.setVisible (isFloorMode);
    btnHeadModeAmp.setToggleState    (! headByp, juce::dontSendNotification);
    btnHeadModePedals.setToggleState (headByp,   juce::dontSendNotification);

    btnFloorPresets.setVisible    (isFloorMode);
    btnFloorTuner.setVisible      (isFloorMode);
    btnFloorToneFinder.setVisible (isFloorMode);
    btnFloorDrop.setVisible       (false);
    btnFloorConfig.setVisible     (false);
    btnBackToAmp.setVisible       (isFloorMode);
    btnSkipIntro.setVisible       (introRunning && ! isFloorMode && introElapsed < 2.38f);
}

void MiAmpAudioProcessorEditor::togglePopover (PopoverKind kind)
{
    if (activePopover == kind || kind == PopoverKind::None)
    {
        if (activePopover != PopoverKind::None)
        {
            activePopover = PopoverKind::None;
            popoverClosing = true;
        }
        if (! isFloorMode)
            syncHeadControls();
    }
    else
    {
        if (! isFloorMode)
            closeFloatingOverlays ("popover");
        // In floor mode, opening Presets/Drop/Config/ToneFinder closes the left editor & slides board left
        if (isFloorMode && (kind == PopoverKind::Presets || kind == PopoverKind::Drop
                         || kind == PopoverKind::Config || kind == PopoverKind::ToneFinder))
        {
            editorOpen = false;
            floorTunerOpen = false;
            syncPedalEditorControls();
        }
        const bool wasClosed = (activePopover == PopoverKind::None && ! popover->isVisible());
        activePopover = kind;
        popoverClosing = false;
        if (wasClosed)
            popoverAnim = 0.0f;
        popover->setBackNavigation (! isFloorMode);
        popover->setKind (kind, tunerReading);
        popover->setAlpha (silkEase (popoverAnim));
        popover->setVisible (true);
        if (! isFloorMode)
            syncHeadControls();
        layoutAll();
        popover->toFront (true);
    }
    repaint();
}

void MiAmpAudioProcessorEditor::syncPedalEditorControls()
{
    const bool bass = audioProcessor.isBassMode();
    auto chain = audioProcessor.getActiveChain (bass);
    int& sel = bass ? selectedBassPedal : selectedGuitarPedal;

    const bool hasSelected = std::find (chain.begin(), chain.end(), sel) != chain.end();
    if (isFloorMode && ! hasSelected && ! chain.empty())
        sel = chain.front();

    const bool showEd = isFloorMode && editorOpen && ! chain.empty();

    btnEditClose.setVisible (showEd);
    btnMoveLeft.setVisible (showEd);
    btnMoveRight.setVisible (showEd);
    btnTrashPedal.setVisible (showEd);

    const int nPalette = bass ? 14 : 13;
    for (int idx = 0; idx < 14; ++idx)
    {
        if (idx >= nPalette)
        {
            paletteItems[idx]->setVisible (false);
            continue;
        }
        const int pt = bass ? mipedalui::kBassPaletteOrder[idx] : mipedalui::kGuitarPaletteOrder[idx];
        const auto& spec = mipedalui::getPedalSpec (bass, pt);
        const bool inChain = std::find (chain.begin(), chain.end(), pt) != chain.end();
        paletteItems[idx]->setData (
            spec.shortName,
            pedalImage (bass, pt, true),
            [this, bass, pt]
            {
                audioProcessor.addPedalToChain (bass, pt, -1);
                if (bass) selectedBassPedal = pt; else selectedGuitarPedal = pt;
                if (activePopover != PopoverKind::None) togglePopover (PopoverKind::None);
                editorOpen = true;
                syncPedalEditorControls();
                repaint();
            },
            [this, bass, pt] (juce::Point<float> editorPt)
            {
                if (! draggingPedalActive)
                {
                    dragStagePos = editorPtToStage (editorPt);
                    dragRenderPos = dragStagePos;
                    dragLiftScale = 1.0f;
                }
                else
                {
                    dragStagePos = editorPtToStage (editorPt);
                }
                draggingPedalActive = true;
                draggingFromPalette = true;
                draggingOutsideBoard = false;
                draggingPedalType   = pt;
                dragInsertIndex = computeDropInsertIndex (bass, dragStagePos, pt);
                repaint();
            },
            [this, bass, pt] (juce::Point<float> editorPt)
            {
                if (draggingPedalActive)
                {
                    const auto stPt = editorPtToStage (editorPt);
                    const int idxIns = computeDropInsertIndex (bass, stPt, pt);
                    if (idxIns >= 0)
                    {
                        audioProcessor.addPedalToChain (bass, pt, idxIns);
                        if (bass) selectedBassPedal = pt; else selectedGuitarPedal = pt;
                        if (activePopover != PopoverKind::None) togglePopover (PopoverKind::None);
                        editorOpen = true;
                    }
                }
                draggingPedalActive = false;
                draggingFromPalette = false;
                draggingOutsideBoard = false;
                draggingPedalType   = -1;
                dragInsertIndex     = -1;
                syncPedalEditorControls();
                repaint();
            },
            [this, bass, pt] (juce::Graphics& g, juce::Rectangle<float> box)
            {
                const auto& body = pedalImage (bass, pt, true);
                if (! body.isValid()) return;
                const float iw = static_cast<float> (body.getWidth());
                const float ih = static_cast<float> (body.getHeight());
                const float sc = juce::jmin (box.getWidth() / iw, box.getHeight() / ih);
                const float dw = iw * sc, dh = ih * sc;
                const juce::Rectangle<float> fitted (box.getCentreX() - dw * 0.5f, box.getCentreY() - dh * 0.5f, dw, dh);
                g.drawImage (body, fitted, juce::RectanglePlacement::stretchToFit);
                paintMiniPedalKnobs (g, bass, pt, fitted);
            });
        paletteItems[idx]->setInChain (inChain);
        paletteItems[idx]->setVisible (isFloorMode && ! floorTunerOpen);
    }

    const bool showPal = isFloorMode && ! floorTunerOpen;
    paletteStrip.setVisible    (showPal);
    btnPaletteLeft.setVisible  (showPal);
    btnPaletteRight.setVisible (showPal);

    btnFloorTunerNotation.setVisible (isFloorMode && floorTunerOpen);
    btnFloorTunerMute.setVisible     (isFloorMode && floorTunerOpen);

    if (! showEd)
    {
        for (auto& k : editorKnobs) k->setVisible (false);
        for (auto& f : eqFaders)    f->setVisible (false);
        for (auto& t : pedalToggles) t->setVisible (false);
        return;
    }

    auto it = std::find (chain.begin(), chain.end(), sel);
    const int posIdx = (it != chain.end()) ? static_cast<int> (std::distance (chain.begin(), it)) : 0;
    btnMoveLeft.setEnabled (posIdx > 0);
    btnMoveRight.setEnabled (posIdx + 1 < static_cast<int> (chain.size()));

    const auto& selSpec = mipedalui::getPedalSpec (bass, sel);
    const juce::Image knobSprite = (selSpec.artKey < 0) ? imgs.xvKnobPtr : imgs.pedKnobs[selSpec.artKey];

    for (int i = 0; i < 7; ++i)
    {
        if (i < selSpec.numKnobs && ! selSpec.eq.hasEq)
        {
            editorKnobs[i]->setParam (selSpec.knobs[static_cast<size_t> (i)].paramId,
                                      selSpec.knobs[static_cast<size_t> (i)].label,
                                      knobSprite);
            editorKnobs[i]->setVisible (true);
        }
        else
        {
            editorKnobs[i]->setVisible (false);
        }

        if (selSpec.eq.hasEq)
        {
            eqFaders[i]->setParam (selSpec.eq.paramIds[static_cast<size_t> (i)],
                                   selSpec.eq.bandLabels[static_cast<size_t> (i)]);
            eqFaders[i]->setVisible (true);
        }
        else
        {
            eqFaders[i]->setVisible (false);
        }
    }

    for (int i = 0; i < 3; ++i)
    {
        if (i < selSpec.numToggles)
        {
            const auto& ts = selSpec.toggles[static_cast<size_t> (i)];
            pedalToggles[i]->setConfig (ts.paramId, ts.label, ts.threeWay, ts.opt0, ts.opt1, ts.opt2);
            pedalToggles[i]->setVisible (true);
        }
        else
        {
            pedalToggles[i]->setVisible (false);
        }
    }

    if (popover->isVisible())
        popover->toFront (true);

    layoutAll();
}

void MiAmpAudioProcessorEditor::timerCallback()
{
    constexpr float dt = 1.0f / 60.0f;

    if (introRunning && ! isFloorMode)
    {
        introElapsed += dt;
        const float tn = introElapsed / 2.9f;
        if (tn >= 1.0f)
        {
            finishIntro();
        }
        else
        {
            const float prevY = headSceneYOffsetStage;
            if (tn <= 0.34f)
                headSceneYOffsetStage = 1156.0f;
            else
                headSceneYOffsetStage = 1156.0f * (1.0f - cubicBezierIntro ((tn - 0.34f) / 0.66f));

            syncHeadControls();
            if (std::fabs (headSceneYOffsetStage - prevY) > 0.1f)
                layoutAll();
            else
                repaint();
        }
    }

    // #4 Elevación de Estudio Suave (0.62s)
    if (instSwitching)
    {
        instSwitchAnim += dt / 0.62f;
        if (instSwitchAnim >= 1.0f)
        {
            instSwitchAnim = 1.0f;
            instSwitching = false;
            instOutSnapshot = {};
        }
        syncHeadControls();
        layoutAll();
    }

    // Smooth popup & overlay animations (Cajas drawer, right menu, GlassPopover, Studio FX, RigBrowser)
    if (studioFxModal != nullptr)
        studioFxModal->tickAnimation (dt);
    if (rigBrowser != nullptr)
        rigBrowser->tickAnimation (dt);

    {
        const float targetPop = (activePopover != PopoverKind::None) ? 1.0f : 0.0f;
        if (std::fabs (targetPop - popoverAnim) > 0.003f)
        {
            const float step = dt / 0.30f;
            popoverAnim = (targetPop > popoverAnim) ? juce::jmin (1.0f, popoverAnim + step)
                                                    : juce::jmax (0.0f, popoverAnim - step);
            if (popover != nullptr)
                popover->setAlpha (silkEase (popoverAnim));
            layoutAll();
        }
        else if (popoverClosing && popoverAnim <= 0.01f)
        {
            popoverClosing = false;
            if (popover != nullptr)
                popover->setVisible (false);
        }

        const float targetMenu = (! isFloorMode && activePopover == PopoverKind::None) ? 1.0f : (isFloorMode ? 1.0f : 0.0f);
        if (std::fabs (targetMenu - rightMenuAnim) > 0.003f)
        {
            const float step = dt / 0.26f;
            rightMenuAnim = (targetMenu > rightMenuAnim) ? juce::jmin (1.0f, rightMenuAnim + step)
                                                         : juce::jmax (0.0f, rightMenuAnim - step);
            syncHeadControls();
            layoutAll();
        }
    }

    if (! isFloorMode)
    {
        const float targetDrawer = cabDrawerOpen ? 1.0f : 0.0f;
        if (std::fabs (targetDrawer - cabDrawerAnim) > 0.002f)
        {
            const float step = dt / 0.32f;
            cabDrawerAnim = cabDrawerOpen ? juce::jmin (1.0f, cabDrawerAnim + step)
                                          : juce::jmax (0.0f, cabDrawerAnim - step);
            if (std::fabs (targetDrawer - cabDrawerAnim) <= 0.002f)
                cabDrawerAnim = targetDrawer;
            syncHeadControls();
            layoutAll();
        }
    }

    // Standalone safety notice (no-interface startup mute or acoustic feedback auto-mute)
    if (miamp_standalone::isRunningStandalone() && ! introRunning)
    {
        if (miamp_standalone::wasInputMutedForFeedback() && ! standaloneFeedbackNoticeShown)
        {
            standaloneFeedbackNoticeShown = true;
            showHud (miamp_i18n::tr (miamp_i18n::Msg::StandaloneFeedbackMuted));
        }
        else if (! miamp_standalone::wasInputMutedForFeedback()
                 && miamp_standalone::isInputBlockedNoInterface()
                 && ! standaloneStartupNoticeShown)
        {
            standaloneStartupNoticeShown = true;
            showHud (miamp_i18n::tr (miamp_i18n::Msg::StandaloneNoInterfaceMuted));
        }
        else if (! miamp_standalone::isInputBlockedNoInterface())
        {
            standaloneStartupNoticeShown = false;
        }
        if (! miamp_standalone::wasInputMutedForFeedback())
            standaloneFeedbackNoticeShown = false;
    }

    // Smooth .board-group & per-pedal slot animations at 60Hz
    if (isFloorMode)
    {
        const bool popFloor = (activePopover == PopoverKind::Presets
                            || activePopover == PopoverKind::Drop
                            || activePopover == PopoverKind::Config
                            || activePopover == PopoverKind::ToneFinder);
        const bool expanded = ! editorOpen && ! popFloor;

        const float targetTx    = popFloor ? -390.0f : (expanded ? -271.5f : 0.0f);
        const float targetTy    = popFloor ?    0.0f : (expanded ?    6.0f : 0.0f);
        const float targetScale = popFloor ?   0.94f : (expanded ?   1.06f : 1.0f);

        boardTx    += 0.16f * (targetTx    - boardTx);
        boardTy    += 0.16f * (targetTy    - boardTy);
        boardScale += 0.16f * (targetScale - boardScale);

        const int nPal = audioProcessor.isBassMode() ? 14 : 13;
        const float maxScroll = juce::jmax (0.0f, static_cast<float> (nPal - 8) * 121.0f);
        paletteScrollTarget = juce::jlimit (0.0f, maxScroll, std::round (paletteScrollTarget / 121.0f) * 121.0f);
        if (std::fabs (paletteScrollTarget - paletteScrollCur) > 0.5f)
        {
            paletteScrollCur += 0.32f * (paletteScrollTarget - paletteScrollCur);
            syncPalettePositions();
        }
        else if (paletteScrollCur != paletteScrollTarget)
        {
            paletteScrollCur = paletteScrollTarget;
            syncPalettePositions();
        }
        btnPaletteLeft.setEnabled  (paletteScrollTarget > 1.0f);
        btnPaletteRight.setEnabled (paletteScrollTarget < maxScroll - 1.0f);

        const bool bass = audioProcessor.isBassMode();
        auto chain = audioProcessor.getActiveChain (bass);

        // Compute target slot for each pedal, opening space smoothly during drag!
        std::vector<int> layoutChain;
        if (draggingPedalActive && draggingPedalType >= 0 && ! draggingOutsideBoard && dragInsertIndex >= 0)
        {
            for (int pt : chain)
                if (pt != draggingPedalType)
                    layoutChain.push_back (pt);
            const int ins = juce::jlimit (0, static_cast<int> (layoutChain.size()), dragInsertIndex);
            layoutChain.insert (layoutChain.begin() + ins, draggingPedalType);
        }
        else
        {
            layoutChain = chain;
        }

        const auto targetSlots = computeBoardSlotRects (bass, layoutChain);
        std::array<bool, 14> seen {};
        for (size_t i = 0; i < layoutChain.size(); ++i)
        {
            const int pt = layoutChain[i];
            if (pt < 0 || pt >= 14) continue;
            seen[static_cast<size_t> (pt)] = true;
            const auto& tr = targetSlots[i];
            if (! animPedalHasPos[static_cast<size_t> (pt)])
            {
                animPedalRects[static_cast<size_t> (pt)] = tr;
                animPedalHasPos[static_cast<size_t> (pt)] = true;
            }
            else
            {
                auto& cur = animPedalRects[static_cast<size_t> (pt)];
                cur = juce::Rectangle<float> (
                    cur.getX()      + 0.24f * (tr.getX()      - cur.getX()),
                    cur.getY()      + 0.24f * (tr.getY()      - cur.getY()),
                    cur.getWidth()  + 0.24f * (tr.getWidth()  - cur.getWidth()),
                    cur.getHeight() + 0.24f * (tr.getHeight() - cur.getHeight()));
            }
        }
        for (size_t k = 0; k < 14; ++k)
            if (! seen[k])
                animPedalHasPos[k] = false;

        if (draggingPedalActive)
        {
            dragRenderPos.x += 0.48f * (dragStagePos.x - dragRenderPos.x);
            dragRenderPos.y += 0.48f * (dragStagePos.y - dragRenderPos.y);
            dragLiftScale   += 0.28f * (1.09f - dragLiftScale);
        }
        else
        {
            dragLiftScale   += 0.28f * (1.0f - dragLiftScale);
        }
    }

    tunerReading = audioProcessor.tuner.analyze();
    if (activePopover == PopoverKind::Tuner && popover != nullptr)
        popover->updateTuner (tunerReading);

    if (hudTicks > 0 && --hudTicks == 0)
        hudText.clear();

    if (studioFxModal != nullptr && studioFxModal->isVisible())
        studioFxModal->repaint();

    const bool dropOn = audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchOn)->load() > 0.5f;
    const int semi = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchSemi)->load()));
    const juce::String dropTxt = dropOn ? ("DROP " + juce::String (semi > 0 ? "+" : "") + juce::String (semi)) : "DROP";
    btnDockDrop.setButtonText (dropTxt);
    btnDockDrop.setToggleState (dropOn || activePopover == PopoverKind::Drop, juce::dontSendNotification);
    btnDockConfig.setToggleState (activePopover == PopoverKind::Config, juce::dontSendNotification);

    btnFloorDrop.setToggleState   (dropOn || activePopover == PopoverKind::Drop, juce::dontSendNotification);
    btnFloorConfig.setToggleState (activePopover == PopoverKind::Config, juce::dontSendNotification);

    btnMainPresets.setToggleState     (activePopover == PopoverKind::Presets,    juce::dontSendNotification);
    btnMainTuner.setToggleState       (activePopover == PopoverKind::Tuner,      juce::dontSendNotification);
    btnMainToneFinder.setToggleState  (activePopover == PopoverKind::ToneFinder, juce::dontSendNotification);
    btnMainCabs.setToggleState        (cabDrawerOpen,                            juce::dontSendNotification);
    btnFloorPresets.setToggleState    (activePopover == PopoverKind::Presets,    juce::dontSendNotification);
    btnFloorToneFinder.setToggleState (activePopover == PopoverKind::ToneFinder, juce::dontSendNotification);
    btnFloorTuner.setToggleState      (floorTunerOpen, juce::dontSendNotification);

    const bool muted = audioProcessor.apvts.getRawParameterValue (MiAmpParams::tunerMuted)->load() > 0.5f;
    btnFloorTunerMute.setToggleState (muted, juce::dontSendNotification);
    btnFloorTunerMute.setButtonText (muted ? "MUTE: ON" : "MUTE: OFF");

    btnFloorTunerNotation.setToggleState (useSolfege, juce::dontSendNotification);
    btnFloorTunerNotation.setButtonText (useSolfege ? "DO RE MI" : "C D E");

    repaint();
}

void MiAmpAudioProcessorEditor::resized()
{
    layoutAll();
}

juce::Rectangle<float> MiAmpAudioProcessorEditor::computeEditorPedalStageRect() const
{
    // Inside .edit (35, 28, 420, 577) -> cap height to 356px so tall pedals (MI-87) never touch bottom parameter readouts
    const bool bass = audioProcessor.isBassMode();
    const int sel = bass ? selectedBassPedal : selectedGuitarPedal;
    const auto& spec = mipedalui::getPedalSpec (bass, sel);
    const float maxW = 344.0f;
    const float maxH = 356.0f;
    const float sc = juce::jmin (maxW / static_cast<float> (spec.natW),
                                 maxH / static_cast<float> (spec.natH));
    const float w = static_cast<float> (spec.natW) * sc;
    const float h = static_cast<float> (spec.natH) * sc;
    return { 35.0f + (420.0f - w) * 0.5f, 28.0f + 46.0f + (maxH - h) * 0.5f, w, h };
}

std::vector<juce::Rectangle<float>> MiAmpAudioProcessorEditor::computeBoardSlotRects (bool bassMode, const std::vector<int>& chain) const
{
    std::vector<juce::Rectangle<float>> rects;
    const int count = static_cast<int> (chain.size());
    if (count <= 0) return rects;

    // Velcro interior of board_surface.png: X=[619.5 .. 1470.4], Y=[107.5 .. 587.8]
    // Equal left & right margin (24px each) and equal top & bottom margin (2 lines only, top-left first!)
    const float leftX   = 643.5f;
    const float rightX  = 1446.4f;
    const float usableW = rightX - leftX; // 802.9f
    const float rowY[2] = { 239.65f, 455.65f };

    auto slotDims = [bassMode](int pt) -> juce::Point<float>
    {
        const auto& spec = mipedalui::getPedalSpec (bassMode, pt);
        if (spec.artKey < 0)
            return { 224.0f, 182.0f };
        if (spec.isWide)
        {
            const float w = (spec.natH > 700) ? 186.0f : 196.0f;
            return { w, w * (static_cast<float> (spec.natH) / static_cast<float> (spec.natW)) };
        }
        const float h = 192.0f;
        const float w = juce::jlimit (78.0f, 134.0f, h * (static_cast<float> (spec.natW) / static_cast<float> (spec.natH)));
        return { w, h };
    };

    // Strictly max 2 rows (top & bottom), always filling top row from left to right first
    std::vector<std::vector<int>> rows (2);
    float used0 = 0.0f;
    int splitIdx = 0;
    for (; splitIdx < count; ++splitIdx)
    {
        const float w = slotDims (chain[static_cast<size_t> (splitIdx)]).x;
        const bool tooManyLeftForRow1 = (count - splitIdx) > 5;
        if (rows[0].size() >= 5 || (! rows[0].empty() && ! tooManyLeftForRow1 && used0 + 18.0f + w > usableW))
            break;
        used0 += (rows[0].empty() ? 0.0f : 18.0f) + w;
        rows[0].push_back (splitIdx);
    }
    for (int i = splitIdx; i < count; ++i)
        rows[1].push_back (i);

    if (rows[1].empty())
        rows.pop_back();

    rects.resize (static_cast<size_t> (count));
    for (size_t ri = 0; ri < rows.size(); ++ri)
    {
        const auto& row = rows[ri];
        float total = 0.0f;
        for (int idx : row)
            total += slotDims (chain[static_cast<size_t> (idx)]).x;

        const float minGap = 16.0f;
        const float minTotalW = total + minGap * static_cast<float> (row.size() > 1 ? row.size() - 1 : 0);
        const float fitScale = (minTotalW > usableW) ? (usableW / minTotalW) : 1.0f;
        total *= fitScale;

        const bool fillFullWidth = (row.size() >= 2) &&
            (row.size() == 5 || (ri == 0 && rows.size() > 1) || minTotalW >= usableW);

        const float space = (row.size() <= 1) ? 0.0f
                          : (fillFullWidth ? (usableW - total) / static_cast<float> (row.size() - 1)
                                           : 34.0f);

        float x = leftX;
        const float rowCenterY = rowY[ri];
        for (int idx : row)
        {
            const auto d = slotDims (chain[static_cast<size_t> (idx)]);
            const float dw = d.x * fitScale;
            const float dh = d.y * fitScale;
            rects[static_cast<size_t> (idx)] = juce::Rectangle<float> (x, rowCenterY - dh * 0.5f, dw, dh);
            x += dw + space;
        }
    }
    return rects;
}

bool MiAmpAudioProcessorEditor::isOutsideBoardViewport (juce::Point<float> stagePt) const noexcept
{
    const auto boardHitR = applyBoardTransform (juce::Rectangle<float> (590.0f, 100.0f, 910.0f, 490.0f));
    return ! boardHitR.contains (stagePt);
}

int MiAmpAudioProcessorEditor::computeDropInsertIndex (bool bassMode, juce::Point<float> stagePt, int excludePedal) const
{
    if (isOutsideBoardViewport (stagePt))
        return -1;

    auto chain = audioProcessor.getActiveChain (bassMode);
    if (chain.empty()) return 0;

    auto slots = computeBoardSlotRects (bassMode, chain);
    int bestSlot = 0;
    float bestDist = 1e9f;
    int idx = 0;
    for (size_t i = 0; i < chain.size(); ++i)
    {
        const auto r = applyBoardTransform (slots[i]);
        const float d = std::hypot (r.getCentreX() - stagePt.x, r.getCentreY() - stagePt.y);
        if (d < bestDist)
        {
            bestDist = d;
            bestSlot = idx + (stagePt.x > r.getCentreX() ? 1 : 0);
        }
        if (chain[i] != excludePedal)
            ++idx;
    }
    return std::clamp (bestSlot, 0, idx);
}

void MiAmpAudioProcessorEditor::layoutAll()
{
    const bool bass = audioProcessor.isBassMode();

    btnSkipIntro.setBounds   (toScreenInt (1550 - 31 - 96, 25, 96, 36));
    btnCreditClose.setBounds (toScreenInt (775 - 48, 520, 96, 36));
    if (updateOverlay != nullptr)
        updateOverlay->setBounds (getLocalBounds());
    if (updateModalVisible)
    {
        if (btnUpdateNow.isVisible())
        {
            btnUpdateNow.setBounds   (toScreenInt (555, 424, 210, 34));
            btnUpdateLater.setBounds (toScreenInt (785, 424, 210, 34));
        }
        else if (btnUpdateLater.isVisible())
        {
            btnUpdateLater.setBounds (toScreenInt (625, 424, 300, 34));
        }
    }

    if (rigBrowser != nullptr)
        rigBrowser->setBounds (getLocalBounds());
    if (studioFxModal != nullptr)
        studioFxModal->setBounds (getLocalBounds());

    if (! isFloorMode)
    {
        const float ampYOff = introRunning ? headSceneYOffsetStage : 0.0f;
        const float menuDy  = (1.0f - silkEase (rightMenuAnim)) * 6.0f;
        const float drawerEase = silkEase (cabDrawerAnim);

        // Centered right glass panel (.menu 1291, 173, 224, 422) on main screen
        btnInstGuitar.setBounds      (toScreenInt (1306, 188.0f + menuDy, 94,  36));
        btnInstBass.setBounds        (toScreenInt (1404, 188.0f + menuDy, 94,  36));
        btnMainPresets.setBounds     (toScreenInt (1303, 232.0f + menuDy, 200, 36));
        btnMainTuner.setBounds       (toScreenInt (1303, 274.0f + menuDy, 200, 36));
        btnMainToneFinder.setBounds  (toScreenInt (1303, 316.0f + menuDy, 200, 36));
        btnMainPedals.setBounds      (toScreenInt (1303, 358.0f + menuDy, 200, 36));
        btnMainCabs.setBounds        (toScreenInt (1303, 400.0f + menuDy, 200, 36));
        btnMainStudioFx.setBounds    (toScreenInt (1303, 442.0f + menuDy, 200, 36));
        btnDockConfig.setBounds      (toScreenInt (1303, 484.0f + menuDy, 200, 36));

        if (cabPickerPanel)
            cabPickerPanel->setBounds (toScreenInt (28.0f - (1.0f - drawerEase) * 14.0f, 25, 420, consoleOpen ? 595 : 716));

        // Fixed Slim-Pill (Cápsula Fina) head-selector arrows: completely stationary between Guitar & Bass!
        const float aw      = juce::jmap (cabDrawerAnim, 30.0f,   28.0f);
        const float ah      = juce::jmap (cabDrawerAnim, 66.0f,   60.0f);
        const float axLeft  = juce::jmap (cabDrawerAnim, 133.0f,  486.0f);
        const float axRight = juce::jmap (cabDrawerAnim, 1167.0f, 1226.0f);
        const float ay      = juce::jmap (cabDrawerAnim, 344.0f,  335.0f) + ampYOff;
        if (btnHeadSidePrev) btnHeadSidePrev->setBounds (toScreenInt (axLeft,  ay, aw, ah));
        if (btnHeadSideNext) btnHeadSideNext->setBounds (toScreenInt (axRight, ay, aw, ah));

        // Bottom Analog Console Strip (y = 636..752)
        btnAmpPrev.setBounds   (toScreenInt (46,  688, 28, 28));
        btnAmpBrowse.setBounds (toScreenInt (78,  688, 184, 28));
        btnAmpNext.setBounds   (toScreenInt (266, 688, 28, 28));

        for (int i = 0; i < 5; ++i)
            if (conKnobs[i])
                conKnobs[i]->setBounds (toScreenInt (314.0f + i * 74.0f, 650.0f, 70.0f, 88.0f));

        btnCabPrev.setBounds    (toScreenInt (706, 688, 28, 28));
        btnCabBrowse.setBounds  (toScreenInt (738, 688, 152, 28));
        btnCabNext.setBounds    (toScreenInt (894, 688, 28, 28));
        btnCabLoadWav.setBounds (toScreenInt (926, 688, 52, 28));

        btnMic1On.setBounds    (toScreenInt (996,  652, 62, 24));
        btnMic1Dist.setBounds  (toScreenInt (1062, 652, 42, 24));
        btnMic1Phase.setBounds (toScreenInt (1108, 652, 28, 24));

        btnMic2On.setBounds    (toScreenInt (996,  682, 62, 24));
        btnMic2Dist.setBounds  (toScreenInt (1062, 682, 42, 24));
        btnMic2Phase.setBounds (toScreenInt (1108, 682, 28, 24));

        if (conMicMixKnob) conMicMixKnob->setBounds (toScreenInt (1152, 650, 74, 88));
        if (conRoomKnob)   conRoomKnob->setBounds   (toScreenInt (1232, 650, 74, 88));

        btnConsoleStudioFx.setBounds (toScreenInt (1324, 652, 174, 38));
        btnConsolePower.setBounds    (toScreenInt (1324, 696, 174, 38));

        // Modular Rig Layers 1, 2, 3
        float instDyIn = 0.0f;
        if (instSwitching)
        {
            const float tSec = instSwitchAnim * 0.62f;
            const float uIn = juce::jlimit (0.0f, 1.0f, (tSec - 0.045f) / (0.62f - 0.045f));
            instDyIn = 8.0f * (1.0f - silkEase (uIn));
        }

        juce::Rectangle<float> cabR, headR, grilleR;
        computeRigStageLayout (cabR, headR, grilleR);
        headR = headR.translated (0.0f, ampYOff + instDyIn);
        grilleR = grilleR.translated (0.0f, ampYOff + instDyIn);

        const auto& headList = bass ? miamp_rig::getBassHeadSpecs() : miamp_rig::getGuitarHeadSpecs();
        const int headIdx = juce::jlimit (0, static_cast<int> (headList.size()) - 1,
            static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel)->load())));
        const auto& hSpec = headList[static_cast<size_t> (headIdx)];

        if (! hSpec.useClassicOverlay)
        {
            for (size_t i = 0; i < rigKnobs.size() && i < hSpec.knobs.size(); ++i)
            {
                const auto& ks = hSpec.knobs[i];
                const float d = headR.getWidth() * ks.diamW;
                const float cx = headR.getX() + ks.cx * headR.getWidth();
                const float cy = headR.getY() + ks.cy * headR.getHeight();
                rigKnobs[i]->setBounds (toScreenInt (cx - d * 0.5f, cy - d * 0.5f, d, d));
            }
            for (size_t i = 0; i < rigSwitches.size() && i < hSpec.switches.size(); ++i)
            {
                const auto& sw = hSpec.switches[i];
                const float swW = headR.getWidth() * sw.w * 1.15f;
                const float swH = swW;
                const float cx = headR.getX() + sw.cx * headR.getWidth();
                const float cy = headR.getY() + sw.cy * headR.getHeight();
                rigSwitches[i]->setBounds (toScreenInt (cx - swW * 0.5f, cy - swH * 0.5f, swW, swH));
            }
            for (size_t i = 0; i < rigSliders.size() && i < hSpec.sliders.size(); ++i)
            {
                const auto& sl = hSpec.sliders[i];
                const float slW = headR.getWidth() * 0.0175f;
                const float y0 = headR.getY() + sl.yTop * headR.getHeight();
                const float y1 = headR.getY() + sl.yBot * headR.getHeight();
                const float cx = headR.getX() + sl.cx * headR.getWidth();
                rigSliders[i]->setBounds (toScreenInt (cx - slW * 0.5f, y0, slW, y1 - y0));
            }
        }
        else if (bass)
        {
            // Classic Mint Bass Head (bass_head_mint.png: 5 knobs, 3 switches)
            const float knobDiam = headR.getWidth() * 0.072f;
            static constexpr float kClassicBx[5] = { 0.2918f, 0.4052f, 0.5183f, 0.6312f, 0.7448f };
            for (int i = 0; i < 5; ++i)
            {
                const float cx = headR.getX() + kClassicBx[i] * headR.getWidth();
                const float cy = headR.getY() + 0.7944f * headR.getHeight();
                bassHeadKnobs[i]->setBounds (toScreenInt (cx - knobDiam * 0.5f, cy - knobDiam * 0.5f, knobDiam, knobDiam));
            }
            static constexpr float kClassicSwX[3] = { 0.098f, 0.174f, 0.864f };
            for (int i = 0; i < 3; ++i)
            {
                const float swW = headR.getWidth() * 0.038f;
                const float swH = swW * (194.0f / 160.0f);
                const float cx = headR.getX() + kClassicSwX[i] * headR.getWidth();
                const float cy = headR.getY() + 0.790f * headR.getHeight();
                bassHeadSwitches[i]->setBounds (toScreenInt (cx - swW * 0.5f, cy - swH * 0.5147f, swW, swH));
            }
        }
        else
        {
            // Classic MI AMP Mint Guitar Head (head_miamp_mint.png: 19 knobs, 3 switches)
            const float wStage = headR.getWidth() * 0.043f;
            static constexpr float kZenCols[9] = { 0.228f, 0.292f, 0.355f, 0.456f, 0.519f, 0.581f, 0.662f, 0.725f, 0.788f };
            for (int i = 0; i < 9; ++i)
            {
                const float cx = headR.getX() + kZenCols[i] * headR.getWidth();
                const float cyTop = headR.getY() + 0.688f * headR.getHeight();
                const float cyBot = headR.getY() + 0.830f * headR.getHeight();
                guitarHeadKnobs[i]->setBounds     (toScreenInt (cx - wStage * 0.5f, cyTop - wStage * 0.5f, wStage, wStage));
                guitarHeadKnobs[9 + i]->setBounds (toScreenInt (cx - wStage * 0.5f, cyBot - wStage * 0.5f, wStage, wStage));
            }
            const float cxOut = headR.getX() + 0.881f * headR.getWidth();
            const float cyOut = headR.getY() + 0.712f * headR.getHeight();
            guitarHeadKnobs[18]->setBounds (toScreenInt (cxOut - wStage * 0.5f, cyOut - wStage * 0.5f, wStage, wStage));

            static constexpr float kZenSwX[3] = { 0.072f, 0.132f, 0.398f };
            static constexpr float kZenSwY[3] = { 0.750f, 0.750f, 0.768f };
            static constexpr float kZenSwS[3] = { 0.036f, 0.036f, 0.022f };
            for (int i = 0; i < 3; ++i)
            {
                const float swW = headR.getWidth() * kZenSwS[i];
                const float swH = swW * (194.0f / 160.0f);
                const float cx = headR.getX() + kZenSwX[i] * headR.getWidth();
                const float cy = headR.getY() + kZenSwY[i] * headR.getHeight();
                guitarHeadSwitches[i]->setBounds (toScreenInt (cx - swW * 0.5f, cy - swH * 0.5147f, swW, swH));
            }
        }

        // Layer 3 Bare Draggable Microphones (MI-57 + MI-87) in front of cabinet grille
        if (! grilleR.isEmpty())
        {
            const auto grilleScreen = toScreen (grilleR.getX(), grilleR.getY(), grilleR.getWidth(), grilleR.getHeight());
            static constexpr float kDistScale[3] = { 1.0f, 0.86f, 0.72f };

            const float m1X = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f;
            const float m1Y = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f;
            const int   m1D = juce::jlimit (0, 2, static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic1Dist)->load())));
            const float m1W = 122.0f * kDistScale[m1D];
            const float m1H = m1W * (147.0f / 340.0f);
            const float m1Cx = grilleR.getX() + m1X * grilleR.getWidth();
            const float m1Cy = grilleR.getY() + m1Y * grilleR.getHeight();
            if (mic1Body)
            {
                mic1Body->setCabGrilleScreenRect (grilleScreen);
                mic1Body->setBounds (toScreenInt (m1Cx - m1W * 0.5f, m1Cy - m1H * 0.5f, m1W, m1H));
            }

            const float m2X = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f;
            const float m2Y = audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f;
            const int   m2D = juce::jlimit (0, 2, static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::mic2Dist)->load())));
            const float m2H = 128.0f * kDistScale[m2D];
            const float m2W = m2H * (120.0f / 340.0f);
            const float m2Cx = grilleR.getX() + m2X * grilleR.getWidth();
            const float m2Cy = grilleR.getY() + m2Y * grilleR.getHeight();
            if (mic2Body)
            {
                mic2Body->setCabGrilleScreenRect (grilleScreen);
                mic2Body->setBounds (toScreenInt (m2Cx - m2W * 0.5f, m2Cy - m2H * 0.5f, m2W, m2H));
            }
        }

        // Main screen right popover: vertically centered on the 768px stage + smooth open/close animation
        if (popover->isVisible())
        {
            const int popH = popover->getPreferredHeight (673);
            const float popEase = silkEase (popoverAnim);
            const float popY = (768.0f - static_cast<float> (popH)) * 0.5f + (1.0f - popEase) * 10.0f;
            popover->setBounds (toScreenInt (1206.0f, popY, 309.0f, static_cast<float> (popH)));
        }
    }
    else
    {
        // Top-right Amp vs Pedals-Only segmented switch (above board)
        btnHeadModeAmp.setBounds    (toScreenInt (1236, 30, 134, 30));
        btnHeadModePedals.setBounds (toScreenInt (1374, 30, 136, 30));

        // .edit controls (35, 28, 420, 577)
        btnEditClose.setBounds   (toScreenInt (412, 38,  30, 30));
        btnMoveLeft.setBounds    (toScreenInt (57,  553, 36, 34));
        btnMoveRight.setBounds   (toScreenInt (175, 553, 36, 34));
        btnTrashPedal.setBounds  (toScreenInt (392, 544, 44, 44)); // circular 44x44 .trash-btn

        const auto pedR = computeEditorPedalStageRect();
        const int sel = bass ? selectedBassPedal : selectedGuitarPedal;
        const auto& selSpec = mipedalui::getPedalSpec (bass, sel);

        for (int i = 0; i < selSpec.numKnobs && ! selSpec.eq.hasEq; ++i)
        {
            const auto& ks = selSpec.knobs[static_cast<size_t> (i)];
            const float kSz = pedR.getWidth() * ks.diamW;
            const float cx = pedR.getX() + ks.cx * pedR.getWidth();
            const float cy = pedR.getY() + ks.cy * pedR.getHeight();
            editorKnobs[i]->setBounds (toScreenInt (cx - kSz * 0.5f, cy - kSz * 0.5f, kSz, kSz));
        }

        if (selSpec.eq.hasEq)
        {
            const float trTop = pedR.getY() + selSpec.eq.yTop * pedR.getHeight();
            const float trBot = pedR.getY() + selSpec.eq.yBot * pedR.getHeight();
            const float fW = pedR.getWidth() * 0.078f;
            for (int b = 0; b < 7; ++b)
            {
                const float cx = pedR.getX() + selSpec.eq.xs[static_cast<size_t> (b)] * pedR.getWidth();
                eqFaders[b]->setBounds (toScreenInt (cx - fW * 0.5f, trTop, fW, trBot - trTop));
            }
        }

        for (int i = 0; i < selSpec.numToggles; ++i)
        {
            const auto& ts = selSpec.toggles[static_cast<size_t> (i)];
            const float swW = pedR.getWidth() * (selSpec.isWide ? 0.056f : 0.082f);
            const float swH = swW * (194.0f / 160.0f);
            const float cx = pedR.getX() + ts.cx * pedR.getWidth();
            const float cy = pedR.getY() + ts.cy * pedR.getHeight();
            pedalToggles[i]->setBounds (toScreenInt (cx - swW * 0.50f, cy - swH * 0.5147f, swW, swH));
        }

        // Smooth scrollable .palette carousel (8 whole pedal cards of 110px + 11px gap = 957px exact)
        btnPaletteLeft.setBounds  (toScreenInt (205, 656, 26, 58));
        btnPaletteRight.setBounds (toScreenInt (1204, 656, 26, 58));
        paletteStrip.setBounds    (toScreenInt (239, 632, 957, 108));
        syncPalettePositions();

        btnFloorTunerNotation.setBounds (toScreenInt (1030, 640, 175, 36));
        btnFloorTunerMute.setBounds     (toScreenInt (1030, 684, 175, 39));

        // .palette-nav inside dock (36, 623, 1478, 125, r=22):
        // 14px top/bottom margin (637..733), 28px right margin (x_end=1486, well clear of r=22 corner), 16px gap from '>' arrow
        btnFloorPresets.setBounds    (toScreenInt (1246, 637, 117, 28));
        btnFloorTuner.setBounds      (toScreenInt (1369, 637, 117, 28));
        btnFloorToneFinder.setBounds (toScreenInt (1246, 671, 240, 28));
        btnBackToAmp.setBounds       (toScreenInt (1246, 705, 240, 28));

        if (popover->isVisible())
        {
            const int popH = popover->getPreferredHeight (524);
            const float popEase = silkEase (popoverAnim);
            const float popY = 82.0f + (1.0f - popEase) * 10.0f;
            popover->setBounds (toScreenInt (1206.0f, popY, 309.0f, static_cast<float> (popH)));
        }
    }
}

void MiAmpAudioProcessorEditor::syncPalettePositions()
{
    const bool bass = audioProcessor.isBassMode();
    const int nPal = bass ? 14 : 13;
    const float s = stageScale();
    const bool showPal = isFloorMode && ! floorTunerOpen;
    const float cardW = 110.0f * s;
    const float cardH = 104.0f * s;
    const float step  = 121.0f * s;
    const float stripW = 957.0f * s;
    const float offX  = -paletteScrollCur * s;
    for (int idx = 0; idx < nPal; ++idx)
    {
        const float xf = offX + static_cast<float> (idx) * step;
        const bool wholeInside = showPal && (xf >= -2.0f * s) && (xf + cardW <= stripW + 2.0f * s);
        paletteItems[idx]->setVisible (wholeInside);
        paletteItems[idx]->setBounds (juce::roundToInt (xf), juce::roundToInt (2.0f * s),
                                      juce::roundToInt (cardW), juce::roundToInt (cardH));
    }
}

void MiAmpAudioProcessorEditor::paintGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius, float alpha) const
{
    miamp_theme::paintFrostedGlassPanel (g, r, radius * stageScale(), alpha);
}

void MiAmpAudioProcessorEditor::paintMiniPedalKnobs (juce::Graphics& g, bool bassMode, int type, juce::Rectangle<float> pedalRect) const
{
    const auto& spec = mipedalui::getPedalSpec (bassMode, type);

    // 1. Knobs (for all non-EQ pedals including XV)
    if (! spec.eq.hasEq)
    {
        const juce::Image& kImg = (spec.artKey < 0) ? imgs.xvKnobPtr : imgs.pedKnobs[spec.artKey];
        if (kImg.isValid())
        {
            for (int i = 0; i < spec.numKnobs; ++i)
            {
                const auto& ks = spec.knobs[static_cast<size_t> (i)];
                const float kSz = pedalRect.getWidth() * ks.diamW;
                const float val = audioProcessor.apvts.getRawParameterValue (ks.paramId)->load();
                const float rad = juce::degreesToRadians (knobDeg (val));
                const float cx = pedalRect.getX() + ks.cx * pedalRect.getWidth();
                const float cy = pedalRect.getY() + ks.cy * pedalRect.getHeight();
                auto tr = juce::AffineTransform::translation (-kImg.getWidth() * 0.5f, -kImg.getHeight() * 0.5f)
                              .scaled (kSz / kImg.getWidth(), kSz / kImg.getHeight())
                              .rotated (rad)
                              .translated (cx, cy);
                g.drawImageTransformed (kImg, tr, false);
            }
        }
    }
    else
    {
        // 2. 7-band EQ mini fader caps on board & palette thumbnails
        const float trTop = pedalRect.getY() + spec.eq.yTop * pedalRect.getHeight();
        const float trBot = pedalRect.getY() + spec.eq.yBot * pedalRect.getHeight();
        const float capW = pedalRect.getWidth() * 0.072f;
        const float capH = capW * (imgs.pedSliderCap.isValid()
                                       ? static_cast<float> (imgs.pedSliderCap.getHeight()) / juce::jmax (1, imgs.pedSliderCap.getWidth())
                                       : 0.72f);
        const float travel = juce::jmax (1.0f, (trBot - trTop) - capH);
        for (int b = 0; b < 7; ++b)
        {
            const float val = audioProcessor.apvts.getRawParameterValue (spec.eq.paramIds[static_cast<size_t> (b)])->load();
            const float norm = juce::jlimit (0.0f, 1.0f, val * 0.01f);
            const float cx = pedalRect.getX() + spec.eq.xs[static_cast<size_t> (b)] * pedalRect.getWidth();
            const float cy = trTop + (1.0f - norm) * travel + capH * 0.5f;
            const auto capR = juce::Rectangle<float> (cx - capW * 0.5f, cy - capH * 0.5f, capW, capH);
            if (imgs.pedSliderCap.isValid())
                g.drawImage (imgs.pedSliderCap, capR, juce::RectanglePlacement::stretchToFit);
            else
            {
                g.setColour (juce::Colour::fromRGB (34, 34, 38));
                g.fillRoundedRectangle (capR, 1.5f);
            }
        }
    }

    // 3. Mini toggle switches (for board & palette thumbnails)
    for (int i = 0; i < spec.numToggles; ++i)
    {
        const auto& ts = spec.toggles[static_cast<size_t> (i)];
        const int val = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (ts.paramId)->load()));
        const float swW = pedalRect.getWidth() * (spec.isWide ? 0.056f : 0.082f);
        const float swH = swW * (194.0f / 160.0f);
        const float cx = pedalRect.getX() + ts.cx * pedalRect.getWidth();
        const float cy = pedalRect.getY() + ts.cy * pedalRect.getHeight();
        if (ts.threeWay && val == 1)
        {
            const float nutR = swW * 0.34f;
            g.setColour (juce::Colour::fromRGB (165, 168, 172));
            g.fillEllipse (cx - nutR, cy - nutR, nutR * 2.0f, nutR * 2.0f);
            const float tipR = swW * 0.19f;
            g.setColour (juce::Colour::fromRGB (228, 232, 238));
            g.fillEllipse (cx - tipR, cy - tipR, tipR * 2.0f, tipR * 2.0f);
        }
        else
        {
            const bool isUp = ts.threeWay ? (val >= 2) : (val >= 1);
            const auto& img = isUp ? imgs.toggleUp : imgs.toggleDown;
            if (img.isValid())
                g.drawImage (img, juce::Rectangle<float> (cx - swW * 0.50f, cy - swH * 0.5147f, swW, swH),
                             juce::RectanglePlacement::stretchToFit);
        }
    }

    // 4. XV Cab icon badge
    if (spec.artKey < 0)
    {
        const int cabIdx = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::xvCab)->load()));
        const auto& cabImg = (cabIdx == 0) ? imgs.xvCabBass : (cabIdx == 1 ? imgs.xvCabPunk : imgs.xvCabClean);
        if (cabImg.isValid())
        {
            const auto cr = juce::Rectangle<float> (
                pedalRect.getX() + 0.7117f * pedalRect.getWidth(),
                pedalRect.getY() + 0.328f  * pedalRect.getHeight(),
                0.1294f * pedalRect.getWidth(),
                0.1294f * pedalRect.getWidth() * (static_cast<float> (cabImg.getHeight()) / juce::jmax (1, cabImg.getWidth())));
            g.drawImage (cabImg, cr, juce::RectanglePlacement::stretchToFit);
        }
    }

    // 5. Dynamic LED / GR meter state overlays
    const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bassMode, type))->load() > 0.5f;
    paintPedalStateOverlays (g, bassMode, type, pedalRect, active);
}

void MiAmpAudioProcessorEditor::paintPedalStateOverlays (juce::Graphics& g, bool bassMode, int type,
                                                          juce::Rectangle<float> pedalRect, bool active) const
{
    const auto& spec = mipedalui::getPedalSpec (bassMode, type);

    // ON/BYPASS LED: all ped2_* sprites have a baked lit LED; cover with dark unlit jewel when bypassed
    if (spec.artKey >= 0 && ! active && spec.ledDiamW > 0.0f)
    {
        const float cx = pedalRect.getX() + spec.ledCx * pedalRect.getWidth();
        const float cy = pedalRect.getY() + spec.ledCy * pedalRect.getHeight();
        const float d = pedalRect.getWidth() * spec.ledDiamW * 0.42f;
        const auto lr = juce::Rectangle<float> (cx - d * 0.5f, cy - d * 0.5f, d, d);
        juce::ColourGradient lg (juce::Colour::fromRGB (90, 42, 42), lr.getX() + d * 0.35f, lr.getY() + d * 0.30f,
                                 juce::Colour::fromRGB (26, 20, 22), lr.getRight(), lr.getBottom(), true);
        g.setGradientFill (lg);
        g.fillEllipse (lr);
        g.setColour (juce::Colour::fromRGBA (16, 14, 16, 210));
        g.drawEllipse (lr, juce::jmax (0.8f, d * 0.12f));
    }

    // MI-87 COMP live 10-LED Gain Reduction meter
    if (spec.hasGrMeter)
    {
        const float grDb = active ? audioProcessor.bassCompGrDb.load (std::memory_order_relaxed) : 0.0f;
        const int litCount = active ? juce::jlimit (0, 10, static_cast<int> (std::floor (grDb / 1.4f + 0.35f))) : 0;
        const float d = pedalRect.getWidth() * 0.036f;
        for (int i = 0; i < 10; ++i)
        {
            const float cx = pedalRect.getX() + spec.grLeds[static_cast<size_t> (i)][0] * pedalRect.getWidth();
            const float cy = pedalRect.getY() + spec.grLeds[static_cast<size_t> (i)][1] * pedalRect.getHeight();
            const auto dotR = juce::Rectangle<float> (cx - d * 0.5f, cy - d * 0.5f, d, d);
            const bool lit = (i < litCount);
            if (! lit)
            {
                g.setColour (juce::Colour::fromRGB (28, 32, 30));
                g.fillEllipse (dotR);
                g.setColour (juce::Colour::fromRGBA (12, 14, 14, 210));
                g.drawEllipse (dotR, 0.7f);
            }
            else
            {
                const juce::Colour col = (i < 6) ? juce::Colour::fromRGB (92, 245, 132)
                                       : (i < 8) ? juce::Colour::fromRGB (250, 206, 72)
                                                 : juce::Colour::fromRGB (255, 76, 66);
                g.setColour (col.withAlpha (0.36f));
                g.fillEllipse (dotR.expanded (d * 0.38f));
                g.setColour (col);
                g.fillEllipse (dotR);
            }
        }
    }
}

void MiAmpAudioProcessorEditor::paintRigOnly (juce::Graphics& g, float yOff, float opacity)
{
    if (opacity <= 0.005f)
        return;

    const bool bass = audioProcessor.isBassMode();
    const float s = stageScale();
    const bool useLayer = (opacity < 0.999f);
    if (useLayer)
        g.beginTransparencyLayer (opacity);

    // Layer 1 (Cabinet) & Layer 2 (Amp Head) seated flush with realistic contact shadows
    juce::Rectangle<float> cabR, headR, grilleR;
    computeRigStageLayout (cabR, headR, grilleR);
    cabR  = cabR.translated  (0.0f, yOff);
    headR = headR.translated (0.0f, yOff);

    const auto& cabList  = bass ? miamp_rig::getBassCabSpecs()  : miamp_rig::getGuitarCabSpecs();
    const auto& headList = bass ? miamp_rig::getBassHeadSpecs() : miamp_rig::getGuitarHeadSpecs();
    const int cabIdx = juce::jlimit (0, static_cast<int> (cabList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
    const int headIdx = juce::jlimit (0, static_cast<int> (headList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel)->load())));
    const auto& cSpec = cabList[static_cast<size_t> (cabIdx)];
    const auto& hSpec = headList[static_cast<size_t> (headIdx)];

    const int visualCabIdx = cSpec.isOff ? (bass ? 6 : 7) : cabIdx;
    const auto& visualCab = cabList[static_cast<size_t> (juce::jlimit (0, static_cast<int> (cabList.size()) - 1, visualCabIdx))];

    // Ground contact shadow beneath cabinet base
    {
        const auto shR = toScreen (cabR.getX() - 18.0f, cabR.getBottom() - 10.0f, cabR.getWidth() + 36.0f, 24.0f);
        juce::ColourGradient sg (juce::Colour::fromRGBA (12, 10, 8, 165), shR.getCentreX(), shR.getCentreY(),
                                 juce::Colour::fromRGBA (12, 10, 8, 0),   shR.getRight(),   shR.getCentreY(), true);
        g.setGradientFill (sg);
        g.fillEllipse (shR);
    }

    // Draw Layer 1 Cabinet (dimmed fallback cabinet when CABINET OFF / DIRECT AMP / DI)
    if (visualCab.imageData != nullptr && visualCab.imageSize > 0)
    {
        auto cImg = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
        if (cImg.isValid())
        {
            if (cSpec.isOff)
            {
                auto dimmed = miamp_rig::makeDimmedCabImage (cImg, 0.44f);
                g.setOpacity (0.94f);
                g.drawImage (dimmed, toScreen (cabR.getX(), cabR.getY(), cabR.getWidth(), cabR.getHeight()),
                             juce::RectanglePlacement::stretchToFit, false);
                g.setOpacity (1.0f);
            }
            else
            {
                g.drawImage (cImg, toScreen (cabR.getX(), cabR.getY(), cabR.getWidth(), cabR.getHeight()),
                             juce::RectanglePlacement::stretchToFit);
            }
        }

        // Clean subtle cast shadow under head + front foot contact AO
        {
            const auto hSh = toScreen (headR.getX() + headR.getWidth() * 0.04f, headR.getBottom() - 28.0f, headR.getWidth() * 0.92f, 24.0f);
            juce::ColourGradient bellyGrad (juce::Colour::fromRGBA (4, 4, 4, 130), hSh.getCentreX(), hSh.getCentreY(),
                                            juce::Colour::fromRGBA (4, 4, 4, 0),   hSh.getRight(),   hSh.getCentreY(), true);
            g.setGradientFill (bellyGrad);
            g.fillEllipse (hSh);

            const float fw = headR.getWidth() * 0.085f;
            for (const float fx : { 0.102f, 0.898f })
            {
                const auto fSh = toScreen (headR.getX() + headR.getWidth() * fx - fw * 0.5f, headR.getBottom() - 5.5f, fw, 11.0f);
                juce::ColourGradient footGrad (juce::Colour::fromRGBA (0, 0, 0, 210), fSh.getCentreX(), fSh.getCentreY(),
                                               juce::Colour::fromRGBA (0, 0, 0, 0),   fSh.getRight(),   fSh.getCentreY(), true);
                g.setGradientFill (footGrad);
                g.fillEllipse (fSh);
            }
        }
    }

    if (cSpec.isOff)
    {
        const float badgeW = 310.0f;
        const float badgeH = 34.0f;
        const float desiredTop = juce::jmap (cabDrawerAnim, 148.0f, 132.0f);
        const float maxTop = consoleOpen ? juce::jmax (84.0f, 596.0f - cabR.getY()) : juce::jmax (120.0f, 716.0f - cabR.getY());
        const float badgeY = closeUpView ? (cabR.getY() + juce::jmin (desiredTop, maxTop)) : (cabR.getY() + cabR.getHeight() * 0.54f - badgeH * 0.5f);
        const auto diBadge = toScreen (cabR.getCentreX() - badgeW * 0.5f, badgeY, badgeW, badgeH);
        g.setColour (juce::Colour::fromRGBA (14, 16, 15, 214));
        g.fillRoundedRectangle (diBadge, 10.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 145));
        g.drawRoundedRectangle (diBadge, 10.0f * s, 1.2f);
        g.setColour (inkColour().withAlpha (0.92f));
        g.setFont (sansFont (juce::jmax (10.0f, 12.0f * s), juce::Font::bold));
        g.drawText ("CABINET OFF  (DIRECT AMP / DI)", diBadge, juce::Justification::centred);
    }

    // Draw Layer 2 Amp Head
    {
        const auto hImg = juce::ImageCache::getFromMemory (hSpec.chassisData, hSpec.chassisSize);
        if (hImg.isValid())
            g.drawImage (hImg, toScreen (headR.getX(), headR.getY(), headR.getWidth(), headR.getHeight()),
                         juce::RectanglePlacement::stretchToFit);
    }

    if (useLayer)
        g.endTransparencyLayer();
}

void MiAmpAudioProcessorEditor::paintHeadScene (juce::Graphics& g, float yOff)
{
    const bool bass = audioProcessor.isBassMode();
    const float s = stageScale();

    // Layer 0: Single tall continuous Venice Beach Skatepark background (1550x1924)
    // Fixed 1.18f closeUpView zoom (zero background zoom jump between Guitar and Bass)
    if (imgs.skateparkTall.isValid())
    {
        const float fullW = static_cast<float> (imgs.skateparkTall.getWidth());
        const float fullH = 768.0f;
        const float maxCamY = juce::jmax (1.0f, static_cast<float> (imgs.skateparkTall.getHeight()) - fullH);
        const float camY = juce::jlimit (0.0f, maxCamY, maxCamY - yOff);
        const float introDown = 1.0f - juce::jlimit (0.0f, 1.0f, yOff / maxCamY);
        const float zoom = closeUpView ? (1.0f + 0.18f * introDown) : 1.0f;
        const float srcW = fullW / zoom;
        const float srcH = fullH / zoom;
        const float panOffsetPx = -52.0f * cabDrawerAnim;
        const float srcX = juce::jlimit (0.0f, fullW - srcW, (fullW - srcW) * 0.5f + panOffsetPx);
        const float srcY = juce::jlimit (0.0f, static_cast<float> (imgs.skateparkTall.getHeight()) - srcH,
                                         camY + (fullH - srcH) * (1.0f / 3.0f));
        auto tr = juce::AffineTransform::translation (-srcX, -srcY)
                      .scaled (static_cast<float> (getWidth())  / srcW,
                               static_cast<float> (getHeight()) / srcH);
        g.drawImageTransformed (imgs.skateparkTall, tr, false);
    }

    const auto& cabList  = bass ? miamp_rig::getBassCabSpecs()  : miamp_rig::getGuitarCabSpecs();
    const auto& headList = bass ? miamp_rig::getBassHeadSpecs() : miamp_rig::getGuitarHeadSpecs();
    const int cabIdx = juce::jlimit (0, static_cast<int> (cabList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
    const int headIdx = juce::jlimit (0, static_cast<int> (headList.size()) - 1,
        static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel)->load())));
    const auto& cSpec = cabList[static_cast<size_t> (cabIdx)];
    const auto& hSpec = headList[static_cast<size_t> (headIdx)];

    // #4 Elevación de Estudio Suave (0.62s) crossfade between outgoing & incoming rig
    if (instSwitching && instOutSnapshot.isValid())
    {
        const float tSec = instSwitchAnim * 0.62f;
        const float uOut = juce::jlimit (0.0f, 1.0f, tSec / 0.50f);
        const float easeOut = silkEase (uOut);
        const float opOut = 1.0f - easeOut;
        if (opOut > 0.008f)
        {
            const float scOut = 1.0f - 0.004f * easeOut;
            const float dyOut = -5.0f * easeOut * s;
            const float cx = getWidth() * 0.5f;
            const float cy = getHeight() * 0.5f;
            auto trOut = juce::AffineTransform::translation (-cx, -cy)
                             .scaled (scOut, scOut)
                             .translated (cx, cy + dyOut);
            g.setOpacity (opOut);
            g.drawImageTransformed (instOutSnapshot, trOut, false);
            g.setOpacity (1.0f);
        }

        const float uIn = juce::jlimit (0.0f, 1.0f, (tSec - 0.045f) / (0.62f - 0.045f));
        const float easeIn = silkEase (uIn);
        const float dyIn = 8.0f * (1.0f - easeIn);
        paintRigOnly (g, yOff + dyIn, easeIn);
    }
    else
    {
        paintRigOnly (g, yOff, 1.0f);
    }

    const bool pwr = audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::ampPower : MiAmpParams::gtrPower)->load() >= 0.5f;
    const bool standby = audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::ampStandby : MiAmpParams::gtrStandby)->load() >= 0.5f;

    if (introRunning && introElapsed < 2.42f)
        return;

    const float uiAlpha = introRunning ? juce::jlimit (0.0f, 1.0f, (introElapsed - 2.42f) / 0.40f) : 1.0f;
    const float menuEase = silkEase (rightMenuAnim);
    const float menuAlpha = uiAlpha * menuEase;

    if (activePopover == PopoverKind::None || menuEase > 0.01f)
    {
    const float menuDy = (1.0f - menuEase) * 6.0f;
    // Layer 4a: Centered right glass panel (.menu 1291, 173, 224, 422) on main screen
    paintGlassPanel (g, toScreen (1291.0f, 173.0f + menuDy, 224.0f, 422.0f), 22.0f, menuAlpha);

    auto swBox = toScreen (1305.0f, 187.0f + menuDy, 196.0f, 38.0f);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, static_cast<uint8_t> (24.0f * menuAlpha)));
    g.fillRoundedRectangle (swBox, 8.0f * s);
    g.setColour (juce::Colour::fromRGBA (241, 224, 190, static_cast<uint8_t> (102.0f * menuAlpha)));
    g.drawRoundedRectangle (swBox, 8.0f * s, 1.0f);

    // Integrated status LED + IN / OUT meters inside right panel (1304, 534, 198, 44)
    auto meterBox = toScreen (1304.0f, 534.0f + menuDy, 198.0f, 44.0f);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, static_cast<uint8_t> (16.0f * menuAlpha)));
    g.fillRoundedRectangle (meterBox, 8.0f * s);
    g.setColour (juce::Colour::fromRGBA (241, 224, 190, static_cast<uint8_t> (74.0f * menuAlpha)));
    g.drawRoundedRectangle (meterBox, 8.0f * s, 1.0f);

    const bool fbMuted   = miamp_standalone::isRunningStandalone() && miamp_standalone::wasInputMutedForFeedback();
    const bool noIfMuted = miamp_standalone::isRunningStandalone() && miamp_standalone::isInputBlockedNoInterface();
    const bool clip      = audioProcessor.isClipping.load();
    const juce::Colour ledCol = (clip || fbMuted) ? juce::Colour::fromRGB (255, 94, 94)
                              : (noIfMuted || ! pwr || standby) ? juce::Colour::fromRGB (247, 197, 107)
                                                                : juce::Colour::fromRGB (126, 255, 189);
    g.setColour (ledCol.withAlpha (menuAlpha));
    g.fillEllipse (toScreen (1315.0f, 551.5f + menuDy, 9.0f, 9.0f));

    if (fbMuted || noIfMuted)
    {
        g.setColour (ledCol.withAlpha (0.95f * menuAlpha));
        g.setFont (sansFont (juce::jmax (8.0f, 9.8f * s), juce::Font::bold));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::CfgAudioNoInterfaceShort),
                    toScreen (1332.0f, 538.0f + menuDy, 162.0f, 18.0f), juce::Justification::centredLeft, true);
        g.setColour (inkColour().withAlpha (0.76f * menuAlpha));
        g.setFont (sansFont (juce::jmax (7.2f, 8.4f * s), juce::Font::plain));
        g.drawText ("CONFIG -> AUDIO",
                    toScreen (1332.0f, 555.0f + menuDy, 162.0f, 16.0f), juce::Justification::centredLeft, true);
    }
    else
    {
        const float inFrac  = juce::jlimit (0.0f, 1.0f, audioProcessor.inputRms.load() * 3.5f);
        const float outFrac = juce::jlimit (0.0f, 1.0f, audioProcessor.outputRms.load() * 3.5f);
        g.setColour (inkColour().withAlpha (0.80f * menuAlpha));
        g.setFont (sansFont (juce::jmax (7.5f, 9.0f * s), juce::Font::bold));
        g.drawText ("IN",  toScreen (1331.0f, 542.0f + menuDy, 22.0f, 11.0f), juce::Justification::centredLeft);
        g.drawText ("OUT", toScreen (1331.0f, 556.0f + menuDy, 24.0f, 11.0f), juce::Justification::centredLeft);

        auto drawMeter = [&] (float y, float frac)
        {
            auto tr = toScreen (1359.0f, y + menuDy, 130.0f, 5.5f);
            g.setColour (juce::Colour::fromRGBA (15, 22, 20, static_cast<uint8_t> (165.0f * menuAlpha)));
            g.fillRoundedRectangle (tr, 3.0f * s);
            g.setColour (juce::Colour::fromRGBA (255, 244, 212, static_cast<uint8_t> (56.0f * menuAlpha)));
            g.drawRoundedRectangle (tr, 3.0f * s, 0.8f);
            if (frac > 0.01f)
            {
                g.setColour ((frac > 0.85f ? juce::Colour::fromRGB (255, 107, 91)
                                           : juce::Colour::fromRGB (136, 226, 181)).withAlpha (menuAlpha));
                g.fillRoundedRectangle (tr.withWidth (tr.getWidth() * frac), 3.0f * s);
            }
        };
        drawMeter (545.0f, inFrac);
        drawMeter (559.0f, outFrac);
    }
    }

    // Layer 4b: Bottom Analog Studio Console Strip (34, 636, 1482, 114) - only when consoleOpen
    if (consoleOpen)
    {
        const auto consoleR = toScreen (34, 636, 1482, 114);
        juce::ColourGradient cg (juce::Colour::fromRGBA (54, 52, 46, static_cast<uint8_t> (232.0f * uiAlpha)), consoleR.getX(), consoleR.getY(),
                                 juce::Colour::fromRGBA (24, 26, 24, static_cast<uint8_t> (244.0f * uiAlpha)), consoleR.getX(), consoleR.getBottom(), false);
        g.setGradientFill (cg);
        g.fillRoundedRectangle (consoleR, 18.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, static_cast<uint8_t> (150.0f * uiAlpha)));
        g.drawRoundedRectangle (consoleR, 18.0f * s, 1.5f);

        // Dividers between console modules
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, static_cast<uint8_t> (46.0f * uiAlpha)));
        for (float dx : { 304.0f, 694.0f, 986.0f, 1312.0f })
            g.drawVerticalLine (juce::roundToInt (dx * s), consoleR.getY() + 10.0f * s, consoleR.getBottom() - 10.0f * s);

        // Section labels & active model names
        g.setColour (juce::Colour::fromRGBA (191, 252, 225, static_cast<uint8_t> (210.0f * uiAlpha)));
        g.setFont (sansFont (juce::jmax (8.5f, 10.0f * s), juce::Font::bold));
        g.drawText ("AMP HEAD",   toScreen (46,  644, 248, 15), juce::Justification::centredLeft);
        g.drawText ("CABINET / IR", toScreen (706, 644, 272, 15), juce::Justification::centredLeft);
        g.drawText ("DUAL MICS",  toScreen (996, 640, 140, 12), juce::Justification::centredLeft);

        g.setColour (inkColour().withAlpha (uiAlpha));
        g.setFont (sansFont (juce::jmax (11.0f, 14.5f * s), juce::Font::bold));
        g.drawText (hSpec.displayName, toScreen (46, 661, 248, 22), juce::Justification::centredLeft, true);

        const juce::String cabLabel = (cSpec.isCustomIr && audioProcessor.getCustomCabIRName (bass).isNotEmpty())
            ? ("IR: " + audioProcessor.getCustomCabIRName (bass))
            : juce::String (cSpec.displayName);
        g.drawText (cabLabel, toScreen (706, 661, 272, 22), juce::Justification::centredLeft, true);

        g.setColour (inkColour().withAlpha (0.68f * uiAlpha));
        g.setFont (sansFont (juce::jmax (8.5f, 10.5f * s), juce::Font::plain));
        g.drawText (hSpec.subtitle, toScreen (46,  722, 248, 16), juce::Justification::centredLeft, true);
        g.drawText (cSpec.subtitle, toScreen (706, 722, 272, 16), juce::Justification::centredLeft, true);
        g.drawText ("Drag mics on cab", toScreen (996, 718, 145, 16), juce::Justification::centredLeft, true);
    }

    if (hudText.isNotEmpty())
    {
        const float hudCx = 685.0f + cabDrawerAnim * 185.0f;
        const float hudW  = juce::jlimit (270.0f, 620.0f, static_cast<float> (hudText.length()) * 7.2f + 44.0f);
        auto hudR = toScreen (hudCx - hudW * 0.5f, 18.0f, hudW, 34.0f);
        paintGlassPanel (g, hudR, 11.0f);
        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (10.0f, 12.2f * s), juce::Font::bold));
        g.drawText (hudText, hudR, juce::Justification::centred);
    }
}

void MiAmpAudioProcessorEditor::paintFloorScene (juce::Graphics& g)
{
    const bool bass = audioProcessor.isBassMode();
    if (imgs.floorBg.isValid())
        g.drawImage (imgs.floorBg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);

    const float s = stageScale();
    auto chain = audioProcessor.getActiveChain (bass);
    const int sel = bass ? selectedBassPedal : selectedGuitarPedal;
    const bool showEd = editorOpen && ! chain.empty();

    // Transformed .board-group (surface + pedals move together cleanly!)
    const auto boardR = applyBoardTransform (juce::Rectangle<float> (585.79f, 95.73f, 921.48f, 499.54f));
    const auto boardScreenR = toScreen (boardR.getX(), boardR.getY(), boardR.getWidth(), boardR.getHeight());
    if (imgs.boardSurface.isValid())
        g.drawImage (imgs.boardSurface, boardScreenR, juce::RectanglePlacement::stretchToFit);

    const auto slots = computeBoardSlotRects (bass, chain);

    if (chain.empty())
    {
        g.setColour (juce::Colour::fromRGBA (242, 232, 212, 180));
        g.setFont (sansFont (juce::jmax (13.0f, 20.0f * s), juce::Font::bold));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::BoardEmptyHint),
                    boardScreenR, juce::Justification::centred);
    }
    else
    {
        for (size_t i = 0; i < chain.size(); ++i)
        {
            const int pt = chain[i];
            const bool isBeingDragged = draggingPedalActive && ! draggingFromPalette && (pt == draggingPedalType);
            const auto& baseSlot = (pt >= 0 && pt < 14 && animPedalHasPos[static_cast<size_t> (pt)])
                                       ? animPedalRects[static_cast<size_t> (pt)]
                                       : slots[i];
            const auto slotT = applyBoardTransform (baseSlot);
            const auto sr = toScreen (slotT.getX(), slotT.getY(), slotT.getWidth(), slotT.getHeight());
            const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, pt))->load() > 0.5f;
            const auto& pImg = pedalImage (bass, pt, active);

            // Preserve each pedal's natural chassis aspect ratio centered in its slot
            const auto& pSpec = mipedalui::getPedalSpec (bass, pt);
            const float scFit = juce::jmin (sr.getWidth() / static_cast<float> (pSpec.natW),
                                            sr.getHeight() / static_cast<float> (pSpec.natH));
            const float artW = static_cast<float> (pSpec.natW) * scFit;
            const float artH = static_cast<float> (pSpec.natH) * scFit;
            const auto artR = juce::Rectangle<float> (sr.getCentreX() - artW * 0.5f, sr.getCentreY() - artH * 0.5f, artW, artH);

            // Dim bypassed pedals visually on the board so active vs bypassed is immediately clear
            g.setOpacity (isBeingDragged ? 0.25f : 1.0f);

            if (pImg.isValid())
                g.drawImage (pImg, artR, juce::RectanglePlacement::stretchToFit);

            paintMiniPedalKnobs (g, bass, pt, artR);

            if (! active && ! isBeingDragged)
            {
                g.setColour (juce::Colour::fromRGBA (8, 12, 14, 138));
                g.fillRoundedRectangle (artR, 8.0f * s);
                paintPedalStateOverlays (g, bass, pt, artR, false);
            }
            g.setOpacity (1.0f);

            if (pt == sel && showEd && ! isBeingDragged)
            {
                g.setColour (juce::Colour::fromRGB (255, 223, 152));
                g.drawRoundedRectangle (artR.expanded (3.0f * s), 7.0f * s, 2.0f * s);
            }
        }
    }

    // Smooth Drag & Drop insertion indicator + lifted floating pedal with drop shadow
    if (draggingPedalActive && draggingPedalType >= 0)
    {
        if (dragInsertIndex >= 0 && ! draggingOutsideBoard)
        {
            const auto& guideRect = (draggingPedalType < 14 && animPedalHasPos[static_cast<size_t> (draggingPedalType)])
                                        ? applyBoardTransform (animPedalRects[static_cast<size_t> (draggingPedalType)])
                                        : applyBoardTransform (juce::Rectangle<float> (621.0f, 153.0f, 123.0f, 192.0f));
            const auto gr = toScreen (guideRect.getX(), guideRect.getY(), guideRect.getWidth(), guideRect.getHeight());
            g.setColour (mintBorder().withAlpha (0.28f));
            g.fillRoundedRectangle (gr, 8.0f * s);
            g.setColour (mintBorder().withAlpha (0.85f));
            g.drawRoundedRectangle (gr, 8.0f * s, 2.0f * s);
        }

        const auto& gSpec = mipedalui::getPedalSpec (bass, draggingPedalType);
        const float baseW = (gSpec.artKey < 0) ? 224.0f : (gSpec.isWide ? 190.0f : 118.0f);
        const float gw = baseW * boardScale * dragLiftScale;
        const float gh = gw * (static_cast<float> (gSpec.natH) / static_cast<float> (gSpec.natW));
        const auto ghostR = toScreen (dragRenderPos.x - gw * 0.5f, dragRenderPos.y - gh * 0.5f - (dragLiftScale - 1.0f) * 65.0f, gw, gh);

        // Soft elevation shadow underneath lifted pedal
        g.setColour (juce::Colour::fromRGBA (0, 0, 0, 105));
        g.fillRoundedRectangle (ghostR.translated (4.0f * s, 12.0f * s).reduced (4.0f * s), 10.0f * s);

        const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, draggingPedalType))->load() > 0.5f;
        const auto& gImg = pedalImage (bass, draggingPedalType, active);
        if (gImg.isValid())
        {
            g.setOpacity (0.96f);
            g.drawImage (gImg, ghostR, juce::RectanglePlacement::stretchToFit);
            paintMiniPedalKnobs (g, bass, draggingPedalType, ghostR);
            g.setOpacity (1.0f);
        }

        if (draggingOutsideBoard && ! draggingFromPalette)
        {
            g.setColour (juce::Colour::fromRGB (242, 177, 163));
            g.drawRoundedRectangle (ghostR, 8.0f * s, 3.0f * s);
            const juce::Rectangle<float> tagR (ghostR.getX() - 20.0f * s, ghostR.getBottom() + 4.0f * s,
                                               ghostR.getWidth() + 40.0f * s, 22.0f * s);
            g.setColour (juce::Colour::fromRGBA (120, 26, 22, 220));
            g.fillRoundedRectangle (tagR, 6.0f * s);
            g.setColour (juce::Colours::white);
            g.setFont (sansFont (juce::jmax (9.0f, 11.0f * s), juce::Font::bold));
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::ReleaseToRemove), tagR, juce::Justification::centred);
        }
    }

    // Left .edit glass card (35, 28, 420, 577)
    if (showEd)
    {
        paintGlassPanel (g, toScreen (35, 28, 420, 577), 22.0f);
        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (14.0f, 18.0f * s), juce::Font::bold));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::EditPrefix) + pedalName (bass, sel).toUpperCase(),
                    toScreen (56, 42, 344, 28), juce::Justification::centredLeft);

        const auto pedStage = computeEditorPedalStageRect();
        const auto pedR = toScreen (pedStage.getX(), pedStage.getY(), pedStage.getWidth(), pedStage.getHeight());
        const bool selActive = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, sel))->load() > 0.5f;
        const auto& edImg = pedalImage (bass, sel, selActive);
        if (edImg.isValid())
            g.drawImage (edImg, pedR, juce::RectanglePlacement::stretchToFit);

        const auto& selSpec = mipedalui::getPedalSpec (bass, sel);
        if (selSpec.artKey < 0)
        {
            const int cabIdx = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::xvCab)->load()));
            const auto& cabImg = (cabIdx == 0) ? imgs.xvCabBass : (cabIdx == 1 ? imgs.xvCabPunk : imgs.xvCabClean);
            if (cabImg.isValid())
            {
                const auto cr = juce::Rectangle<float> (
                    pedR.getX() + 0.7117f * pedR.getWidth(),
                    pedR.getY() + 0.328f  * pedR.getHeight(),
                    0.1294f * pedR.getWidth(),
                    0.1294f * pedR.getWidth() * (static_cast<float> (cabImg.getHeight()) / juce::jmax (1, cabImg.getWidth())));
                g.drawImage (cabImg, cr, juce::RectanglePlacement::stretchToFit);
            }
        }
        else
        {
            paintPedalStateOverlays (g, bass, sel, pedR, selActive);
        }

        // Value readout row below stage (clean margin below pedal so tall chassis never overlap text)
        if (selSpec.eq.hasEq)
        {
            g.setFont (sansFont (juce::jmax (8.0f, 9.5f * s), juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.92f));
            const float x0 = 48.0f, rw = 394.0f / 7.0f;
            for (int b = 0; b < 7; ++b)
            {
                const float v = audioProcessor.apvts.getRawParameterValue (selSpec.eq.paramIds[static_cast<size_t> (b)])->load();
                const float db = (v * 0.01f - 0.5f) * 24.0f;
                const juce::String dbStr = (db >= 0.05f ? "+" : "") + juce::String (db, 0);
                g.drawText (juce::String (selSpec.eq.bandLabels[static_cast<size_t> (b)]) + "\n" + dbStr,
                            toScreen (x0 + b * rw, 462.0f, rw, 30.0f), juce::Justification::centred);
            }
        }
        else if (selSpec.numKnobs > 4)
        {
            g.setFont (sansFont (juce::jmax (8.0f, 9.4f * s), juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.92f));
            const int cols = 4;
            const float x0 = 50.0f, rw = 390.0f / static_cast<float> (cols);
            for (int i = 0; i < selSpec.numKnobs; ++i)
            {
                const auto& ks = selSpec.knobs[static_cast<size_t> (i)];
                const int v = juce::roundToInt (audioProcessor.apvts.getRawParameterValue (ks.paramId)->load());
                const int col = i % cols, row = i / cols;
                const juce::String txt = juce::String (ks.label).substring (0, 8) + " " + juce::String (v);
                g.drawText (txt, toScreen (x0 + col * rw, 458.0f + row * 20.0f, rw, 18.0f), juce::Justification::centred);
            }
        }
        else if (selSpec.numKnobs > 0)
        {
            g.setFont (sansFont (juce::jmax (9.5f, 11.2f * s), juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.94f));
            const float cw = 378.0f / static_cast<float> (selSpec.numKnobs);
            for (int i = 0; i < selSpec.numKnobs; ++i)
            {
                const auto& ks = selSpec.knobs[static_cast<size_t> (i)];
                const int v = juce::roundToInt (audioProcessor.apvts.getRawParameterValue (ks.paramId)->load());
                g.drawText (juce::String (ks.label) + " " + juce::String (v),
                            toScreen (56.0f + i * cw, 472.0f, cw, 22.0f), juce::Justification::centred);
            }
        }

        auto it = std::find (chain.begin(), chain.end(), sel);
        const int posIdx = (it != chain.end()) ? static_cast<int> (std::distance (chain.begin(), it)) + 1 : 1;
        g.setColour (inkColour().withAlpha (0.85f));
        g.setFont (sansFont (juce::jmax (9.5f, 11.0f * s), juce::Font::plain));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::ChainPrefix) + juce::String (posIdx) + "/" + juce::String (chain.size()),
                    toScreen (96, 553, 76, 34), juce::Justification::centred);
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::DragKnobHint) + miamp_i18n::tr (selActive ? miamp_i18n::Msg::StateActive : miamp_i18n::Msg::StateBypass),
                    toScreen (56, 514, 330, 26), juce::Justification::centredLeft);
    }

    // Top-right glass pill for CON AMPLI | SOLO PEDALES
    paintGlassPanel (g, toScreen (1230, 26, 284, 38), 14.0f);

    // Bottom .palette glass bar (36, 623, 1478, 125)
    paintGlassPanel (g, toScreen (36, 623, 1478, 125), 22.0f);
    if (! floorTunerOpen)
    {
        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (10.0f, 13.0f * s), juce::Font::bold));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::PaletteTitle), toScreen (52, 650, 152, 24), juce::Justification::centredLeft);
        g.setColour (inkColour().withAlpha (0.82f));
        g.setFont (sansFont (juce::jmax (9.0f, 11.0f * s), juce::Font::plain));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::PaletteSubtitle), toScreen (52, 676, 152, 34), juce::Justification::topLeft);
    }
    else
    {
        // Inline #floorTuner inside .palette with Do Re Mi support + reference notes!
        const bool active = tunerReading.hasSignal && tunerReading.freqHz > 20.0f;
        const bool inTune = active && std::fabs (tunerReading.cents) < 6.0f;
        const juce::String noteStr = active ? formatNoteName (tunerReading.noteName, tunerReading.octave, useSolfege) : "--";

        auto noteBox = toScreen (56, 637, 136, 96);
        miamp_theme::paintGlassCard (g, noteBox, 12.0f * s);
        g.setColour (active ? (inTune ? mintBorder() : inkColour()) : inkColour().withAlpha (0.6f));
        g.setFont (sansFont (juce::jmax (20.0f, (useSolfege ? 34.0f : 44.0f) * s), juce::Font::bold));
        g.drawText (noteStr, noteBox, juce::Justification::centred);

        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (10.0f, 13.0f * s), juce::Font::bold));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::TunerLabel), toScreen (210, 638, 220, 20), juce::Justification::centredLeft);
        const juce::String hzStr = active
            ? (juce::String (tunerReading.freqHz, 1) + " Hz  (" + (tunerReading.cents >= 0 ? "+" : "") + juce::String (tunerReading.cents, 1) + " ct)")
            : miamp_i18n::tr (miamp_i18n::Msg::WaitingSignal);
        g.setFont (sansFont (juce::jmax (10.0f, 13.0f * s)));
        g.drawText (hzStr, toScreen (650, 638, 345, 20), juce::Justification::centredRight);

        auto meter = toScreen (225, 670, 760, 13);
        juce::ColourGradient mg (juce::Colour::fromRGB (183, 115, 85), meter.getX(), meter.getY(),
                                 juce::Colour::fromRGB (183, 115, 85), meter.getRight(), meter.getY(), false);
        mg.addColour (0.25, juce::Colour::fromRGB (208, 186, 128));
        mg.addColour (0.50, juce::Colour::fromRGB (154, 215, 180));
        mg.addColour (0.75, juce::Colour::fromRGB (208, 186, 128));
        g.setGradientFill (mg);
        g.fillRoundedRectangle (meter, 6.5f * s);
        g.setColour (juce::Colours::white);
        g.fillRect (meter.getCentreX() - 1.5f, meter.getY() - 4.0f, 3.0f, meter.getHeight() + 8.0f);

        const float normCents = active ? juce::jlimit (-1.0f, 1.0f, tunerReading.cents / 50.0f) : 0.0f;
        const float nx = meter.getCentreX() + normCents * (meter.getWidth() * 0.48f);
        g.setColour (juce::Colour::fromRGB (23, 39, 34));
        g.fillRect (nx - 2.5f, meter.getY() - 5.0f, 5.0f, meter.getHeight() + 10.0f);

        // Reference strings row below tuner bar
        const char* notesBass[4] = { "E", "A", "D", "G" };
        const int   octsBass[4]  = { 1, 1, 2, 2 };
        const char* notesGtr[6]  = { "E", "A", "D", "G", "B", "E" };
        const int   octsGtr[6]   = { 2, 2, 3, 3, 3, 4 };
        const int nNotes = bass ? 4 : 6;
        const float cellW = 760.0f / static_cast<float> (nNotes);
        for (int i = 0; i < nNotes; ++i)
        {
            const juce::String txt = formatNoteName (bass ? notesBass[i] : notesGtr[i],
                                                     bass ? octsBass[i]  : octsGtr[i], useSolfege);
            auto cr = toScreen (225.0f + i * cellW + 3.0f, 695.0f, cellW - 6.0f, 26.0f);
            const bool hit = active && noteStr == txt;
            g.setColour (hit ? juce::Colour::fromRGBA (141, 224, 184, 90) : juce::Colour::fromRGBA (255, 255, 255, 22));
            g.fillRoundedRectangle (cr, 6.0f * s);
            g.setColour (hit ? mintBorder() : juce::Colour::fromRGBA (255, 244, 212, 60));
            g.drawRoundedRectangle (cr, 6.0f * s, 1.0f);
            g.setColour (hit ? juce::Colours::white : inkColour().withAlpha (0.88f));
            g.setFont (sansFont (juce::jmax (9.5f, 11.5f * s), juce::Font::bold));
            g.drawText (txt, cr, juce::Justification::centred);
        }
    }

    if (hudText.isNotEmpty())
    {
        auto hudR = toScreen (475, 28, 240, 36);
        paintGlassPanel (g, hudR, 11.0f);
        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (10.0f, 13.0f * s), juce::Font::bold));
        g.drawText (hudText, hudR, juce::Justification::centred);
    }
}

void MiAmpAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (82, 110, 121)); // .holder sky/slate background
    if (! isFloorMode)
        paintHeadScene (g, headSceneYOffsetStage);
    else
        paintFloorScene (g);
}

void MiAmpAudioProcessorEditor::paintOverChildren (juce::Graphics& g)
{
    const float s = stageScale();

    // 1. Intro Logo over single continuous upper sky of skatepark_tall_capa0.jpg (no separate sky stitching!)
    if (introRunning && ! isFloorMode)
    {
        const float tn = juce::jlimit (0.0f, 1.0f, introElapsed / 2.9f);
        float logoAlpha = 0.0f;
        float logoDy = 0.0f;
        if (tn >= 0.04f && tn < 0.20f)
        {
            const float u = (tn - 0.04f) / 0.16f;
            logoAlpha = u;
            logoDy = 10.0f * (1.0f - u);
        }
        else if (tn >= 0.20f && tn < 0.52f)
        {
            logoAlpha = 1.0f;
        }
        else if (tn >= 0.52f && tn < 0.74f)
        {
            const float u = (tn - 0.52f) / 0.22f;
            logoAlpha = 1.0f - u;
            logoDy = -24.0f * u;
        }

        if (logoAlpha > 0.01f && imgs.introLogo.isValid())
        {
            const float camY = juce::jlimit (0.0f, 1156.0f, 1156.0f - headSceneYOffsetStage);
            const float lw = kStageW * 0.58f;
            const float lh = lw * (static_cast<float> (imgs.introLogo.getHeight()) / juce::jmax (1, imgs.introLogo.getWidth()));
            const auto lr = toScreen (kStageW * 0.21f, (220.0f - camY * 0.45f) + logoDy, lw, lh);
            g.setOpacity (logoAlpha);
            g.drawImage (imgs.introLogo, lr, juce::RectanglePlacement::stretchToFit);
            g.setOpacity (1.0f);
        }
    }

    // Hover tooltip pills beside mirrored head-selector arrows
    if (! isFloorMode && ! introRunning && ! creditOpen)
    {
        for (auto* arrow : { btnHeadSidePrev.get(), btnHeadSideNext.get() })
        {
            if (arrow != nullptr && arrow->isVisible() && arrow->isMouseOver() && arrow->getTipText().isNotEmpty())
            {
                const auto ab = arrow->getBounds().toFloat();
                const float tw = juce::jmax (110.0f * s, arrow->getTipText().length() * 6.8f * s + 22.0f * s);
                const float th = 26.0f * s;
                const float tx = arrow->isRightSide() ? (ab.getX() - tw - 8.0f * s) : (ab.getRight() + 8.0f * s);
                const float ty = ab.getCentreY() - th * 0.5f;
                const auto tipR = juce::Rectangle<float> (tx, ty, tw, th);
                g.setColour (juce::Colour::fromRGBA (14, 17, 16, 232));
                g.fillRoundedRectangle (tipR, 8.0f * s);
                g.setColour (juce::Colour::fromRGBA (255, 243, 210, 110));
                g.drawRoundedRectangle (tipR, 8.0f * s, 1.0f);
                g.setColour (juce::Colour::fromRGB (250, 243, 227));
                g.setFont (sansFont (juce::jmax (9.5f, 10.5f * s), juce::Font::bold));
                g.drawText (arrow->getTipText(), tipR, juce::Justification::centred);
            }
        }
    }

    // 2. Credit Easter Egg Overlay (.credit-overlay)
    if (creditOpen)
    {
        g.setColour (juce::Colour::fromRGBA (14, 22, 23, 215));
        g.fillRect (getLocalBounds());

        g.setColour (inkColour().withAlpha (0.85f));
        g.setFont (sansFont (juce::jmax (12.0f, 16.0f * s), juce::Font::bold));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::DevelopedBy), toScreen (475, 220, 600, 32), juce::Justification::centred);

        if (imgs.popupsCredit.isValid())
        {
            const float cw = 440.0f;
            const float ch = cw * (static_cast<float> (imgs.popupsCredit.getHeight()) / juce::jmax (1, imgs.popupsCredit.getWidth()));
            g.drawImage (imgs.popupsCredit, toScreen (775.0f - cw * 0.5f, 270.0f, cw, ch), juce::RectanglePlacement::centred);
        }
    }
}

void MiAmpAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const float s = stageScale();
    const auto stagePt = juce::Point<float> (e.position.x / s, e.position.y / s);
    const bool bass = audioProcessor.isBassMode();

    if (creditOpen)
        return;

    if (! isFloorMode)
    {
        if (introRunning) return;

        // Clear CLIP on meter click inside unified right panel
        if (activePopover == PopoverKind::None && juce::Rectangle<float> (1304, 534, 198, 44).contains (stagePt))
        {
            audioProcessor.isClipping.store (false, std::memory_order_relaxed);
            return;
        }
        // Clicking the AMP or CABINET info header on the console opens the visual browser
        if (consoleOpen && juce::Rectangle<float> (42, 642, 256, 42).contains (stagePt))
        {
            closeFloatingOverlays ("rigBrowser");
            if (rigBrowser) rigBrowser->openMode (miamp_rig::RigBrowserOverlay::Mode::Heads);
            syncHeadControls();
            return;
        }
        if (consoleOpen && juce::Rectangle<float> (702, 642, 278, 42).contains (stagePt))
        {
            closeFloatingOverlays ("rigBrowser");
            if (rigBrowser) rigBrowser->openMode (miamp_rig::RigBrowserOverlay::Mode::Cabs);
            syncHeadControls();
            return;
        }

        // Click outside panels/head/cab returns smoothly to clean Primer Plano view
        const bool inRightMenu = (activePopover == PopoverKind::None) && juce::Rectangle<float> (1291, 173, 224, 422).contains (stagePt);
        const bool inCabDrawer = cabDrawerOpen && juce::Rectangle<float> (28, 25, 420, consoleOpen ? 595.0f : 716.0f).contains (stagePt);
        const bool inConsole   = consoleOpen && juce::Rectangle<float> (34, 636, 1482, 114).contains (stagePt);
        bool inPopover = false;
        if (activePopover != PopoverKind::None && popover != nullptr)
        {
            const int popH = popover->getPreferredHeight (673);
            const float popY = (768.0f - static_cast<float> (popH)) * 0.5f;
            inPopover = juce::Rectangle<float> (1206.0f, popY, 309.0f, static_cast<float> (popH)).contains (stagePt);
        }

        juce::Rectangle<float> cabR, headR, grilleR;
        computeRigStageLayout (cabR, headR, grilleR);
        const bool inRig = headR.contains (stagePt) || cabR.contains (stagePt);

        if (! inRightMenu && ! inCabDrawer && ! inConsole && ! inPopover && ! inRig)
        {
            if (activePopover != PopoverKind::None || cabDrawerOpen || consoleOpen || ! closeUpView)
            {
                closeFloatingOverlays();
                consoleOpen = false;
                closeUpView = true;
                syncHeadControls();
                layoutAll();
                repaint();
            }
        }
        return;
    }

    auto chain = audioProcessor.getActiveChain (bass);
    int& sel = bass ? selectedBassPedal : selectedGuitarPedal;

    if (editorOpen && ! chain.empty())
    {
        const auto pedR = computeEditorPedalStageRect();
        if (bass && sel == 1)
        {
            const auto cabR = juce::Rectangle<float> (
                pedR.getX() + 0.70f * pedR.getWidth(),
                pedR.getY() + 0.31f * pedR.getHeight(),
                0.15f * pedR.getWidth(), 0.22f * pedR.getHeight());
            if (cabR.contains (stagePt))
            {
                const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::xvCab)->load()));
                const int nxt = (cur + 1) % 3;
                if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::xvCab))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (nxt)));
                const char* names[3] = { "BASS", "PUNK", "CLEAN" };
                showHud (juce::String::fromUTF8 (u8"XV CAB \u00b7 ") + names[nxt]);
                repaint();
                return;
            }
        }

        const auto& selSpec = mipedalui::getPedalSpec (bass, sel);
        const auto swR = juce::Rectangle<float> (
            pedR.getX() + selSpec.swX * pedR.getWidth(),
            pedR.getY() + selSpec.swY * pedR.getHeight(),
            selSpec.swW * pedR.getWidth(),
            selSpec.swH * pedR.getHeight());
        if (swR.contains (stagePt))
        {
            if (auto* p = audioProcessor.apvts.getParameter (pedalBypassParamId (bass, sel)))
            {
                const bool nxt = p->getValue() < 0.5f;
                p->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
                showHud (pedalName (bass, sel) + juce::String::fromUTF8 (nxt ? u8" \u00b7 ON" : u8" \u00b7 BYPASS"));
                repaint();
                return;
            }
        }
    }

    const auto slots = computeBoardSlotRects (bass, chain);
    for (size_t i = 0; i < chain.size(); ++i)
    {
        const int pt = chain[i];
        const auto& hitRect = (pt >= 0 && pt < 14 && animPedalHasPos[static_cast<size_t> (pt)])
                                  ? animPedalRects[static_cast<size_t> (pt)]
                                  : slots[i];
        if (applyBoardTransform (hitRect).contains (stagePt))
        {
            dragStartPx = e.getPosition();
            dragStagePos = stagePt;
            dragRenderPos = stagePt;
            dragLiftScale = 1.0f;
            draggingPedalType = pt;
            draggingFromPalette = false;
            draggingPedalActive = false;
            draggingOutsideBoard = false;
            dragInsertIndex = -1;
            return;
        }
    }

    draggingPedalType = -1;

    // Clicking any empty area outside pedals, left editor, dock, top-right switch, and popover closes the left editor
    const bool inLeftEditor = editorOpen && juce::Rectangle<float> (35, 28, 420, 577).contains (stagePt);
    const bool inBottomDock = juce::Rectangle<float> (36, 623, 1478, 125).contains (stagePt);
    const bool inTopRight   = juce::Rectangle<float> (1230, 26, 284, 38).contains (stagePt);
    const bool inPopover    = (activePopover != PopoverKind::None) && juce::Rectangle<float> (1215, 110, 300, 474).contains (stagePt);

    if (! inLeftEditor && ! inBottomDock && ! inTopRight && ! inPopover)
    {
        bool changed = false;
        if (editorOpen)
        {
            editorOpen = false;
            syncPedalEditorControls();
            changed = true;
        }
        if (activePopover != PopoverKind::None)
        {
            togglePopover (PopoverKind::None);
            changed = true;
        }
        if (changed)
            repaint();
    }
}

void MiAmpAudioProcessorEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (! isFloorMode || floorTunerOpen)
        return;

    const auto stagePt = editorPtToStage (e.position);
    if (juce::Rectangle<float> (200, 623, 1040, 125).contains (stagePt))
    {
        const int nPal = audioProcessor.isBassMode() ? 14 : 13;
        const float maxScroll = juce::jmax (0.0f, static_cast<float> (nPal - 8) * 121.0f);
        const float delta = (std::fabs (wheel.deltaX) > std::fabs (wheel.deltaY)) ? -wheel.deltaX : -wheel.deltaY;
        const int stepDir = (delta > 0.05f) ? 1 : (delta < -0.05f ? -1 : 0);
        if (stepDir != 0)
            paletteScrollTarget = juce::jlimit (0.0f, maxScroll,
                std::round (paletteScrollTarget / 121.0f) * 121.0f + static_cast<float> (stepDir) * 121.0f);
    }
}

void MiAmpAudioProcessorEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (! isFloorMode || draggingPedalType < 0 || draggingFromPalette)
        return;

    if (! draggingPedalActive && e.getPosition().getDistanceFrom (dragStartPx) > 8)
        draggingPedalActive = true;

    if (draggingPedalActive)
    {
        const bool bass = audioProcessor.isBassMode();
        dragStagePos = editorPtToStage (e.position);
        draggingOutsideBoard = isOutsideBoardViewport (dragStagePos);
        dragInsertIndex = draggingOutsideBoard ? -1 : computeDropInsertIndex (bass, dragStagePos, draggingPedalType);
        repaint();
    }
}

void MiAmpAudioProcessorEditor::mouseUp (const juce::MouseEvent&)
{
    if (! isFloorMode || draggingPedalType < 0 || draggingFromPalette)
    {
        draggingPedalType = -1;
        return;
    }

    const bool bass = audioProcessor.isBassMode();
    int& sel = bass ? selectedBassPedal : selectedGuitarPedal;

    if (draggingPedalActive)
    {
        if (draggingOutsideBoard)
        {
            audioProcessor.removePedalFromChain (bass, draggingPedalType);
            auto ch = audioProcessor.getActiveChain (bass);
            if (sel == draggingPedalType)
            {
                if (! ch.empty()) sel = ch.front();
                else editorOpen = false;
            }
            showHud (pedalName (bass, draggingPedalType) + miamp_i18n::tr (miamp_i18n::Msg::HudRemoved));
        }
        else if (dragInsertIndex >= 0)
        {
            audioProcessor.reorderPedalToIndex (bass, draggingPedalType, dragInsertIndex);
            sel = draggingPedalType;
            if (activePopover != PopoverKind::None)
                togglePopover (PopoverKind::None);
            editorOpen = true;
            showHud (pedalName (bass, draggingPedalType) + miamp_i18n::tr (miamp_i18n::Msg::HudMoved));
        }
    }
    else
    {
        sel = draggingPedalType;
        if (activePopover != PopoverKind::None)
            togglePopover (PopoverKind::None);
        editorOpen = true;
    }

    draggingPedalActive = false;
    draggingOutsideBoard = false;
    draggingPedalType = -1;
    dragInsertIndex = -1;
    syncPedalEditorControls();
    repaint();
}

void MiAmpAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! isFloorMode) return;
    const float s = stageScale();
    const auto stagePt = juce::Point<float> (e.position.x / s, e.position.y / s);
    const bool bass = audioProcessor.isBassMode();

    auto chain = audioProcessor.getActiveChain (bass);
    const auto slots = computeBoardSlotRects (bass, chain);
    for (size_t i = 0; i < chain.size(); ++i)
    {
        if (applyBoardTransform (slots[i]).contains (stagePt))
        {
            if (auto* p = audioProcessor.apvts.getParameter (pedalBypassParamId (bass, chain[i])))
            {
                const bool nxt = p->getValue() < 0.5f;
                p->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
                showHud (pedalName (bass, chain[i]) + juce::String::fromUTF8 (nxt ? u8" \u00b7 ON" : u8" \u00b7 BYPASS"));
                repaint();
                return;
            }
        }
    }
}
