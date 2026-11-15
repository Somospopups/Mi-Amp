#include "PluginProcessor.h"
#include "PluginEditor.h"
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
    constexpr float kBassSwitchY    = 414.0f;
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
    constexpr float kGtrSwitchSize[3] = { 30.0f,  30.0f,  17.0f };
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

    // 700x1000 pedal knob coordinates from Mi-Amp/index.html
    constexpr float kDrivePoints[3][2]  = { { 0.255218f, 0.220509f }, { 0.499051f, 0.220509f }, { 0.745731f, 0.220509f } };
    constexpr float kCompPoints[3][2]   = { { 0.244975f, 0.218473f }, { 0.50098f,  0.218473f }, { 0.756985f, 0.218473f } };
    constexpr float kChorusPoints[3][2] = { { 0.20807f,  0.291823f }, { 0.498418f, 0.291823f }, { 0.788766f, 0.291823f } };
    constexpr float kPhaserPoints[3][2] = { { 0.239171f, 0.220286f }, { 0.500f,    0.220286f }, { 0.762712f, 0.220286f } };

    constexpr int kBassPaletteOrder[5]   = { 2, 3, 0, 4, 1 };       // Chorus, Phaser, Compresor, Reverb, XV Bass
    constexpr int kGuitarPaletteOrder[7] = { 1, 2, 3, 4, 0, 5, 6 }; // Drive, Fuzz, Chorus, Phaser, Compresor, Delay, Reverb

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

    if (id == "popoverClose" || id == "presetLoad")
    {
        if (id == "popoverClose" && (highlighted || down))
        {
            g.setColour (juce::Colour::fromRGBA (255, 255, 255, down ? 42 : 24));
            g.fillEllipse (r.reduced (1.0f));
        }
        return;
    }

    if (id == "presetDelete")
    {
        const float diam = juce::jmin (r.getWidth(), r.getHeight());
        const auto cr = r.withSizeKeepingCentre (diam, diam);
        if (highlighted || down)
        {
            g.setColour (juce::Colour::fromRGBA (198, 120, 97, down ? 140 : 102));
            g.fillEllipse (cr);
            g.setColour (juce::Colour::fromRGB (255, 222, 209));
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

    const bool active = b.getToggleState();

    if (active)
    {
        g.setColour (juce::Colour::fromRGBA (164, 217, 196, down ? 120 : (highlighted ? 110 : 84)));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (mintBorder());
        g.drawRoundedRectangle (r, 8.0f, 1.2f);
    }
    else if (highlighted || down)
    {
        g.setColour (juce::Colour::fromRGBA (232, 214, 169, down ? 115 : 91));
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.drawRoundedRectangle (r, 8.0f, 1.1f);
    }
    else
    {
        juce::ColourGradient grad (juce::Colour::fromRGBA (255, 255, 255, 34), r.getX(), r.getY(),
                                   juce::Colour::fromRGBA (255, 255, 255, 11), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 8.0f);
        g.setColour (juce::Colour::fromRGBA (241, 224, 190, 102));
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
        g.setFont (sansFont (24.0f, juce::Font::plain));
        g.setColour (juce::Colour::fromRGB (255, 247, 234));
        g.drawText (juce::String::fromUTF8 (u8"\u00d7"), bounds.toFloat(), juce::Justification::centred);
        return;
    }

    if (id == "presetDelete")
    {
        g.setFont (sansFont (18.0f, juce::Font::plain));
        g.setColour (juce::Colour::fromRGB (255, 244, 230));
        g.drawText (juce::String::fromUTF8 (u8"\u00d7"), bounds.toFloat(), juce::Justification::centred);
        return;
    }

    if (id == "presetLoad")
    {
        const bool sel = b.getToggleState();
        g.setFont (sansFont (13.0f, sel ? juce::Font::bold : juce::Font::plain));
        g.setColour (sel ? juce::Colour::fromRGB (227, 255, 232)
                         : juce::Colour::fromRGB (255, 247, 233));
        g.drawFittedText (b.getButtonText(), bounds.reduced (4, 0), juce::Justification::centredLeft, 1);
        return;
    }

    if (id == "floorDropIcon")
    {
        const auto c = bounds.toFloat().getCentre();
        const float sz = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.28f;
        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        juce::Path p;
        p.startNewSubPath (c.x, c.y - sz);
        p.lineTo (c.x, c.y + sz);
        p.startNewSubPath (c.x - sz * 0.55f, c.y - sz * 0.42f);
        p.lineTo (c.x, c.y - sz);
        p.lineTo (c.x + sz * 0.55f, c.y - sz * 0.42f);
        p.startNewSubPath (c.x - sz * 0.55f, c.y + sz * 0.42f);
        p.lineTo (c.x, c.y + sz);
        p.lineTo (c.x + sz * 0.55f, c.y + sz * 0.42f);
        g.strokePath (p, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    if (id == "floorCfgIcon")
    {
        const auto c = bounds.toFloat().getCentre();
        const float rOut = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.28f;
        const float rIn  = rOut * 0.44f;
        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        g.drawEllipse (c.x - rIn, c.y - rIn, rIn * 2.0f, rIn * 2.0f, 1.8f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = static_cast<float> (i) * juce::MathConstants<float>::pi * 0.25f;
            g.drawLine (c.x + std::cos (a) * (rOut * 0.62f), c.y + std::sin (a) * (rOut * 0.62f),
                        c.x + std::cos (a) * rOut,          c.y + std::sin (a) * rOut, 2.2f);
        }
        return;
    }

    const float h = juce::jlimit (9.5f, 13.0f, static_cast<float> (b.getHeight()) * 0.34f);
    g.setFont (sansFont (h, (id == "presetSave" || id == "presetFactory") && b.getToggleState()
                            ? juce::Font::bold : juce::Font::plain));
    g.setColour (b.isEnabled() ? juce::Colour::fromRGB (250, 243, 227)
                               : juce::Colour::fromRGBA (250, 243, 227, 105));

    auto textArea = bounds.reduced (6, 2);
    if (id == "presetFactory" && b.getToggleState())
    {
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
        const float w = static_cast<float> (getWidth());
        const float h = w * (static_cast<float> (img.getHeight()) / juce::jmax (1, img.getWidth()));
        const float x = w * 0.5f - w * 0.461f;
        const float y = static_cast<float> (getHeight()) * 0.5f - h * 0.518f;
        g.setOpacity (0.88f);
        g.drawImage (img, juce::Rectangle<float> (x, y, w, h), juce::RectanglePlacement::stretchToFit);
        g.setOpacity (1.0f);
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
    std::function<void(juce::Point<float>)> onDragEnd)
{
    name = title;
    art = std::move (img);
    clickFn = std::move (onClick);
    dragMoveFn = std::move (onDragMove);
    dragEndFn = std::move (onDragEnd);
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
        auto ptInParent = e.getEventRelativeTo (getParentComponent()).position;
        dragMoveFn (ptInParent);
    }
}

void MiAmpAudioProcessorEditor::PalettePedalButton::mouseUp (const juce::MouseEvent& e)
{
    if (dragged)
    {
        if (dragEndFn)
        {
            auto ptInParent = e.getEventRelativeTo (getParentComponent()).position;
            dragEndFn (ptInParent);
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

    if (art.isValid())
    {
        auto imgR = r.reduced (6.0f, 4.0f).withTrimmedBottom (16.0f);
        g.drawImage (art, imgR, juce::RectanglePlacement::centred);
    }

    g.setColour (inkColour().withAlpha (alreadyInChain ? 0.95f : 0.90f));
    g.setFont (sansFont (juce::jlimit (8.0f, 10.0f, r.getHeight() * 0.105f), juce::Font::bold));
    g.drawText (name, r.withTop (r.getBottom() - 18.0f), juce::Justification::centred);
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
                  std::function<void()> onChanged)
        : processor (proc), useSolfege (solfegeRef),
          closeFn (std::move (onClose)), changedFn (std::move (onChanged))
    {
        addAndMakeVisible (btnClose);
        btnClose.setComponentID ("popoverClose");
        btnClose.setButtonText (juce::String::fromUTF8 (u8"\u00d7"));
        btnClose.onClick = [this] { if (closeFn) closeFn(); };

        // Presets (.save-row matching index.html 1:1)
        addChildComponent (presetNameEdit);
        presetNameEdit.setFont (sansFont (13.0f));
        presetNameEdit.setJustification (juce::Justification::centredLeft);
        presetNameEdit.setIndents (10, 0);
        presetNameEdit.setInputRestrictions (32);
        presetNameEdit.setTextToShowWhenEmpty (juce::String::fromUTF8 (u8"Nombre del preset"),
                                               juce::Colour::fromRGBA (246, 237, 221, 170));
        presetNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        presetNameEdit.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        presetNameEdit.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        presetNameEdit.setColour (juce::TextEditor::textColourId, juce::Colour::fromRGB (255, 250, 242));
        presetNameEdit.setColour (juce::CaretComponent::caretColourId, juce::Colour::fromRGB (255, 250, 242));
        presetNameEdit.onReturnKey = [this] { btnSavePreset.triggerClick(); };

        addChildComponent (btnSavePreset);
        btnSavePreset.setComponentID ("presetSave");
        btnSavePreset.setButtonText ("GUARDAR");
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

        addChildComponent (presetViewport);
        presetViewport.setViewedComponent (&presetListHolder, false);
        presetViewport.setScrollBarsShown (true, false);
        presetViewport.setScrollBarThickness (6);

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

        // Config (Cab + Zenbox Channel + 4x OS + Gate + Input)
        const char* cabLabels[5] = { "VINT 4x12", "MOD 4x12", "OPEN 2x12", "STUD 1x12", "CAB OFF" };
        for (int i = 0; i < 5; ++i)
        {
            addChildComponent (cfgCabBtns[i]);
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
            addChildComponent (cfgChBtns[c]);
            cfgChBtns[c].setButtonText (chLabels[c]);
            cfgChBtns[c].onClick = [this, c]
            {
                if (auto* p = processor.apvts.getParameter (MiAmpParams::gtrChannel))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (c)));
                syncConfigControls();
                if (changedFn) changedFn();
            };
        }

        addChildComponent (btnCfgOs4x);
        btnCfgOs4x.onClick = [this]
        {
            if (auto* p = processor.apvts.getParameter (MiAmpParams::gtrOs4x))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
            syncConfigControls();
            if (changedFn) changedFn();
        };

        addChildComponent (slGate);
        slGate.setRange (0.0, 100.0, 1.0);
        slGate.setSliderStyle (juce::Slider::LinearHorizontal);
        slGate.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        slGateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::gtrGate, slGate);

        addChildComponent (slInput);
        slInput.setRange (0.0, 100.0, 1.0);
        slInput.setSliderStyle (juce::Slider::LinearHorizontal);
        slInput.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 22);
        slInputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::gtrInput, slInput);
    }

    void setKind (PopoverKind k, const xvdsp::TunerReading& tr)
    {
        kind = k;
        tuner = tr;
        const bool isP = (k == PopoverKind::Presets);
        const bool isD = (k == PopoverKind::Drop);
        const bool isT = (k == PopoverKind::Tuner);
        const bool isC = (k == PopoverKind::Config);
        const bool bass = processor.isBassMode();

        presetNameEdit.setVisible (isP);
        btnSavePreset.setVisible (isP);
        presetViewport.setVisible (isP);

        btnDropToggle.setVisible (isD);
        for (auto& c : dropChips) c.setVisible (isD);
        slSemi.setVisible (isD);
        slCents.setVisible (isD);
        slMix.setVisible (isD);

        btnTunerNotation.setVisible (isT);
        btnTunerMute.setVisible (isT);

        for (int i = 0; i < 5; ++i)
            cfgCabBtns[i].setVisible (isC && (! bass || i < 2));
        for (int c = 0; c < 3; ++c)
            cfgChBtns[c].setVisible (isC && ! bass);
        btnCfgOs4x.setVisible (isC && ! bass);
        slGate.setVisible (isC && ! bass);
        slInput.setVisible (isC && ! bass);

        if (isP) rebuildPresetList();
        if (isD) syncDropControls();
        if (isT) syncTunerButtons();
        if (isC) syncConfigControls();
        resized();
        repaint();
    }

    void updateTuner (const xvdsp::TunerReading& tr)
    {
        tuner = tr;
        if (kind == PopoverKind::Tuner) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.75f);
        juce::ColourGradient grad (juce::Colour::fromRGBA (239, 222, 183, 88), r.getX(), r.getY(),
                                   juce::Colour::fromRGBA (45, 48, 40, 215), r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, 22.0f);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 158));
        g.drawRoundedRectangle (r, 22.0f, 1.5f);

        auto inner = r.reduced (18.0f, 16.0f);
        g.setColour (inkColour());
        g.setFont (sansFont (16.0f, juce::Font::bold));
        juce::String title = (kind == PopoverKind::Presets)
                                ? (processor.isBassMode() ? juce::String::fromUTF8 (u8"PRESETS \u00b7 BAJO")
                                                          : juce::String::fromUTF8 (u8"PRESETS \u00b7 GUITARRA"))
                           : (kind == PopoverKind::Drop)   ? "DROP TUNER"
                           : (kind == PopoverKind::Config) ? juce::String::fromUTF8 (u8"CONFIGURACI\u00d3N")
                                                           : "AFINADOR";
        g.drawText (title, inner.removeFromTop (24.0f).withTrimmedRight (26.0f), juce::Justification::centredLeft);

        if (kind == PopoverKind::Presets)
        {
            const auto b = getLocalBounds().toFloat().reduced (18.0f, 14.0f);
            g.setFont (sansFont (10.0f, juce::Font::bold));
            g.setColour (juce::Colour::fromRGBA (244, 234, 217, 184)); // #f4ead9b8
            g.drawText ("GUARDAR PRESET",
                        juce::Rectangle<float> (b.getX(), b.getBottom() - 88.0f, b.getWidth(), 16.0f),
                        juce::Justification::centredLeft);
            g.setFont (sansFont (11.5f));
            g.setColour (inkColour().withAlpha (0.80f));
            g.drawText ("Un preset reemplaza la cadena y ajustes actuales.",
                        juce::Rectangle<float> (b.getX(), b.getBottom() - 24.0f, b.getWidth(), 22.0f),
                        juce::Justification::centredLeft);
        }
        else if (kind == PopoverKind::Drop)
        {
            g.setFont (sansFont (12.0f));
            g.setColour (inkColour().withAlpha (0.85f));
            const float x = inner.getX();
            const float w = inner.getWidth();
            const float y0 = inner.getY() + 104.0f;
            g.drawText ("Semitonos", juce::Rectangle<float> (x, y0, w, 16.0f), juce::Justification::centredLeft);
            g.drawText ("Cents",     juce::Rectangle<float> (x, y0 + 42.0f, w, 16.0f), juce::Justification::centredLeft);
            g.drawText ("Mix %",     juce::Rectangle<float> (x, y0 + 84.0f, w, 16.0f), juce::Justification::centredLeft);
        }
        else if (kind == PopoverKind::Config)
        {
            const bool bass = processor.isBassMode();
            g.setFont (sansFont (12.0f, juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.88f));
            const float x = inner.getX();
            const float w = inner.getWidth();
            g.drawText (bass ? "CABINA 8x10" : "CABINA DE GUITARRA", juce::Rectangle<float> (x, inner.getY() + 6.0f, w, 18.0f), juce::Justification::centredLeft);
            if (! bass)
            {
                g.drawText ("CANAL DEL CABEZAL", juce::Rectangle<float> (x, inner.getY() + 106.0f, w, 18.0f), juce::Justification::centredLeft);
                g.drawText ("NOISE GATE",        juce::Rectangle<float> (x, inner.getY() + 206.0f, w, 18.0f), juce::Justification::centredLeft);
                g.drawText ("INPUT TRIM",        juce::Rectangle<float> (x, inner.getY() + 258.0f, w, 18.0f), juce::Justification::centredLeft);
            }
        }
        else if (kind == PopoverKind::Tuner)
        {
            const bool active = tuner.hasSignal && tuner.freqHz > 20.0f;
            const bool inTune = active && std::fabs (tuner.cents) < 6.0f;
            const juce::String noteStr = active ? MiAmpAudioProcessorEditor::formatNoteName (tuner.noteName, tuner.octave, useSolfege) : "--";

            g.setFont (sansFont (useSolfege ? 42.0f : 52.0f, juce::Font::bold));
            g.setColour (active ? (inTune ? mintBorder() : inkColour()) : inkColour().withAlpha (0.55f));
            g.drawText (noteStr, inner.removeFromTop (64.0f), juce::Justification::centred);

            g.setFont (sansFont (13.0f));
            g.setColour (inkColour().withAlpha (0.85f));
            const juce::String hz = active
                ? (juce::String (tuner.freqHz, 1) + " Hz  (" + (tuner.cents >= 0 ? "+" : "") + juce::String (tuner.cents, 1) + " ct)")
                : juce::String::fromUTF8 (u8"Esperando se\u00f1al");
            g.drawText (hz, inner.removeFromTop (20.0f), juce::Justification::centred);

            auto meter = inner.removeFromTop (30.0f).reduced (6.0f, 7.0f);
            juce::ColourGradient mg (juce::Colour::fromRGB (183, 115, 85), meter.getX(), meter.getY(),
                                     juce::Colour::fromRGB (183, 115, 85), meter.getRight(), meter.getY(), false);
            mg.addColour (0.25, juce::Colour::fromRGB (208, 186, 128));
            mg.addColour (0.50, juce::Colour::fromRGB (154, 215, 180));
            mg.addColour (0.75, juce::Colour::fromRGB (208, 186, 128));
            g.setGradientFill (mg);
            g.fillRoundedRectangle (meter, 7.5f);

            g.setColour (juce::Colours::white);
            g.fillRect (meter.getCentreX() - 1.5f, meter.getY() - 4.0f, 3.0f, meter.getHeight() + 8.0f);

            const float normCents = active ? juce::jlimit (-1.0f, 1.0f, tuner.cents / 50.0f) : 0.0f;
            const float nx = meter.getCentreX() + normCents * (meter.getWidth() * 0.46f);
            g.setColour (juce::Colour::fromRGB (23, 39, 34));
            g.fillRect (nx - 2.0f, meter.getY() - 5.0f, 4.0f, meter.getHeight() + 10.0f);

            auto row = inner.removeFromTop (32.0f).reduced (2.0f, 4.0f);
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
                auto cell = juce::Rectangle<float> (row.getX() + i * cw, row.getY(), cw, row.getHeight()).reduced (2.0f, 0.0f);
                const bool hit = active && curFormatted == txt;
                g.setColour (hit ? juce::Colour::fromRGBA (141, 224, 184, 90) : juce::Colour::fromRGBA (255, 255, 255, 22));
                g.fillRoundedRectangle (cell, 6.0f);
                g.setColour (hit ? mintBorder() : juce::Colour::fromRGBA (255, 244, 212, 60));
                g.drawRoundedRectangle (cell, 6.0f, 1.0f);
                g.setColour (hit ? juce::Colours::white : inkColour().withAlpha (0.85f));
                g.setFont (sansFont (useSolfege ? 9.5f : (isBass ? 11.5f : 10.5f), juce::Font::bold));
                g.drawText (txt, cell, juce::Justification::centred);
            }
        }
    }

    void resized() override
    {
        btnClose.setBounds (getWidth() - 38, 10, 26, 28);
        auto inner = getLocalBounds().reduced (18, 46);

        if (kind == PopoverKind::Presets)
        {
            inner.removeFromBottom (16); // hint text area below .save-row
            auto saveRow = inner.removeFromBottom (41);
            btnSavePreset.setBounds (saveRow.removeFromRight (80));
            saveRow.removeFromRight (6);
            presetNameEdit.setBounds (saveRow);
            inner.removeFromBottom (26); // space for GUARDAR PRESET section title
            presetViewport.setBounds (inner);
            layoutPresetButtons();
        }
        else if (kind == PopoverKind::Drop)
        {
            btnDropToggle.setBounds (inner.getX(), inner.getY(), inner.getWidth(), 32);
            const int cw = inner.getWidth() / 3;
            for (int i = 0; i < 6; ++i)
                dropChips[i].setBounds (inner.getX() + (i % 3) * cw + 2,
                                        inner.getY() + 38 + (i / 3) * 32,
                                        cw - 4, 28);
            slSemi.setBounds  (inner.getX(), inner.getY() + 118, inner.getWidth(), 24);
            slCents.setBounds (inner.getX(), inner.getY() + 160, inner.getWidth(), 24);
            slMix.setBounds   (inner.getX(), inner.getY() + 202, inner.getWidth(), 24);
        }
        else if (kind == PopoverKind::Config)
        {
            const bool bass = processor.isBassMode();
            if (bass)
            {
                const int hw = (inner.getWidth() - 6) / 2;
                cfgCabBtns[0].setBounds (inner.getX(),          inner.getY() + 26, hw, 34);
                cfgCabBtns[1].setBounds (inner.getX() + hw + 6, inner.getY() + 26, hw, 34);
            }
            else
            {
                const int cw = inner.getWidth() / 3;
                for (int i = 0; i < 5; ++i)
                    cfgCabBtns[i].setBounds (inner.getX() + (i % 3) * cw + 2,
                                             inner.getY() + 24 + (i / 3) * 34,
                                             cw - 4, 30);
                for (int c = 0; c < 3; ++c)
                    cfgChBtns[c].setBounds (inner.getX() + c * cw + 2, inner.getY() + 124, cw - 4, 30);
                btnCfgOs4x.setBounds (inner.getX(), inner.getY() + 162, inner.getWidth(), 32);
                slGate.setBounds     (inner.getX(), inner.getY() + 224, inner.getWidth(), 24);
                slInput.setBounds    (inner.getX(), inner.getY() + 276, inner.getWidth(), 24);
            }
        }
        else if (kind == PopoverKind::Tuner)
        {
            btnTunerNotation.setBounds (inner.getX(), getHeight() - 90, inner.getWidth(), 32);
            btnTunerMute.setBounds     (inner.getX(), getHeight() - 50, inner.getWidth(), 34);
        }
    }

private:
    void syncDropControls()
    {
        const bool on = processor.apvts.getRawParameterValue (MiAmpParams::pitchOn)->load() > 0.5f;
        btnDropToggle.setToggleState (on, juce::dontSendNotification);
        btnDropToggle.setButtonText (on ? "DROP ACTIVADO" : "ACTIVAR DROP");

        const int semi = static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::pitchSemi)->load()));
        const int semis[6] = { 0, -1, -2, -3, -4, 12 };
        for (int i = 0; i < 6; ++i)
            dropChips[i].setToggleState (semi == semis[i], juce::dontSendNotification);
    }

    void syncTunerButtons()
    {
        btnTunerNotation.setToggleState (useSolfege, juce::dontSendNotification);
        btnTunerNotation.setButtonText (useSolfege ? "NOTAS: DO RE MI FA SOL LA SI" : "NOTAS: C D E F G A B");

        const bool muted = processor.apvts.getRawParameterValue (MiAmpParams::tunerMuted)->load() > 0.5f;
        btnTunerMute.setToggleState (muted, juce::dontSendNotification);
        btnTunerMute.setButtonText (muted ? "MUTE AL AFINAR: ON" : "MUTE AL AFINAR: OFF");
    }

    void syncConfigControls()
    {
        const bool bass = processor.isBassMode();
        if (bass)
        {
            const bool cabOn = processor.apvts.getRawParameterValue (MiAmpParams::ampCabOn)->load() > 0.5f;
            cfgCabBtns[0].setButtonText ("CAB 8x10 ON");
            cfgCabBtns[1].setButtonText ("CAB 8x10 OFF");
            cfgCabBtns[0].setToggleState (cabOn,   juce::dontSendNotification);
            cfgCabBtns[1].setToggleState (! cabOn, juce::dontSendNotification);
        }
        else
        {
            const char* cabLabels[5] = { "VINT 4x12", "MOD 4x12", "OPEN 2x12", "STUD 1x12", "CAB OFF" };
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
            btnCfgOs4x.setButtonText (os4x ? "4x OVERSAMPLING: ON" : "4x OVERSAMPLING: OFF");
        }
    }

    void rebuildPresetList()
    {
        presetRows.clear();
        presetListHolder.removeAllChildren();

        auto addRow = [this] (const juce::String& text, bool isActive, bool isUserRow,
                              std::function<void()> onLoad, std::function<void()> onDel)
        {
            auto row = std::make_unique<PresetRow> (text, isActive, isUserRow, std::move (onLoad), std::move (onDel));
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
                addRow (up.name, up.name == activeName, true,
                    [this, i] { processor.loadUserPreset (i); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    [this, i] { processor.deleteUserPreset (i); rebuildPresetList(); });
            }
            for (const auto& fp : MiAmpAudioProcessor::getFactoryPresets())
            {
                addRow (fp.name, fp.name == activeName, false,
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
                addRow (up.name, up.name == activeName, true,
                    [this, i] { processor.loadUserPreset (i); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    [this, i] { processor.deleteUserPreset (i); rebuildPresetList(); });
            }
            for (const auto& fp : MiAmpAudioProcessor::getGuitarFactoryPresets())
            {
                addRow (fp.name, fp.name == activeName, false,
                    [this, fp] { processor.applyGuitarPreset (fp); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    nullptr);
            }
        }

        layoutPresetButtons();
    }

    void layoutPresetButtons()
    {
        const int vw = juce::jmax (160, presetViewport.getWidth() - 10);
        const int rowH = 42;
        int y = 0;
        for (auto& r : presetRows)
        {
            r->setBounds (0, y, vw, rowH - 5);
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
            if (delCb)
            {
                addAndMakeVisible (btnDel);
                btnDel.setComponentID ("presetDelete");
                btnDel.setButtonText (juce::String::fromUTF8 (u8"\u00d7"));
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
            if (btnDel.isVisible())
            {
                const int sz = juce::jmin (32, b.getHeight());
                btnDel.setBounds (b.removeFromRight (sz).withSizeKeepingCentre (sz, sz));
                b.removeFromRight (4);
            }
            btnLoad.setBounds (b);
        }
        bool isUser = false;
        juce::TextButton btnLoad, btnDel;
    };

    MiAmpAudioProcessor& processor;
    bool& useSolfege;
    std::function<void()> closeFn, changedFn;
    PopoverKind kind = PopoverKind::None;
    xvdsp::TunerReading tuner;

    juce::TextButton btnClose;
    juce::TextEditor presetNameEdit;
    juce::TextButton btnSavePreset;
    juce::Viewport presetViewport;
    juce::Component presetListHolder;
    std::vector<std::unique_ptr<PresetRow>> presetRows;

    juce::TextButton btnDropToggle;
    juce::TextButton dropChips[6];
    juce::Slider slSemi, slCents, slMix;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slSemiAttach, slCentsAttach, slMixAttach;

    juce::TextButton btnTunerNotation, btnTunerMute;

    juce::TextButton cfgCabBtns[5];
    juce::TextButton cfgChBtns[3];
    juce::TextButton btnCfgOs4x;
    juce::Slider slGate, slInput;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> slGateAttach, slInputAttach;
};

//==============================================================================
// MiAmpAudioProcessorEditor
//==============================================================================
MiAmpAudioProcessorEditor::MiAmpAudioProcessorEditor (MiAmpAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&glassLaf);
    loadImages();

    for (auto* b : { &btnSkipIntro, &btnCreditClose,
                     &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainPedals,
                     &btnDockDrop, &btnDockConfig,
                     &btnEditClose, &btnMoveLeft, &btnMoveRight,
                     &btnFloorPresets, &btnFloorTuner, &btnFloorDrop, &btnFloorConfig, &btnBackToAmp,
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
    btnMainPresets.onClick = [this] { togglePopover (PopoverKind::Presets); };
    addAndMakeVisible (btnMainTuner);
    btnMainTuner.onClick = [this] { togglePopover (PopoverKind::Tuner); };
    addAndMakeVisible (btnMainPedals);
    btnMainPedals.onClick = [this] { setFloorMode (true); };

    addAndMakeVisible (btnDockDrop);
    btnDockDrop.onClick = [this] { togglePopover (PopoverKind::Drop); };
    addAndMakeVisible (btnDockConfig);
    btnDockConfig.onClick = [this] { togglePopover (PopoverKind::Config); };

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

    for (int idx = 0; idx < 7; ++idx)
    {
        paletteItems[idx] = std::make_unique<PalettePedalButton> ("", juce::Image(), nullptr);
        addChildComponent (*paletteItems[idx]);
    }

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
    addChildComponent (btnFloorDrop);
    btnFloorDrop.setComponentID ("floorDropIcon");
    btnFloorDrop.onClick = [this] { togglePopover (PopoverKind::Drop); };
    addChildComponent (btnFloorConfig);
    btnFloorConfig.setComponentID ("floorCfgIcon");
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
        [this] { syncHeadControls(); syncPedalEditorControls(); repaint(); });
    popover->setLookAndFeel (&glassLaf);
    addChildComponent (*popover);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (kStageW / kStageH);

    setResizable (true, true);
    setResizeLimits (775, 384, 2325, 1152);
    setSize (1240, 614);

    // Start with the 2.9s intro animation matching index.html!
    introRunning = true;
    introElapsed = 0.0f;
    headSceneYOffsetStage = kStageH;
    setFloorMode (false);
    startTimerHz (60);
}

MiAmpAudioProcessorEditor::~MiAmpAudioProcessorEditor()
{
    stopTimer();
    for (auto* b : { &btnSkipIntro, &btnCreditClose,
                     &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainPedals,
                     &btnDockDrop, &btnDockConfig,
                     &btnEditClose, &btnMoveLeft, &btnMoveRight,
                     &btnFloorPresets, &btnFloorTuner, &btnFloorDrop, &btnFloorConfig, &btnBackToAmp,
                     &btnFloorTunerMute, &btnFloorTunerNotation })
    {
        b->setLookAndFeel (nullptr);
    }
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

    imgs.compActive  = img (BinaryData::pedal_compresor_active_png, BinaryData::pedal_compresor_active_pngSize);
    imgs.compOff     = img (BinaryData::pedal_compresor_off_png,    BinaryData::pedal_compresor_off_pngSize);
    imgs.driveActive = img (BinaryData::pedal_drive_active_png,     BinaryData::pedal_drive_active_pngSize);
    imgs.driveOff    = img (BinaryData::pedal_drive_off_png,        BinaryData::pedal_drive_off_pngSize);
    imgs.fuzzActive  = img (BinaryData::pedal_fuzz_active_png,      BinaryData::pedal_fuzz_active_pngSize);
    imgs.fuzzOff     = img (BinaryData::pedal_fuzz_off_png,         BinaryData::pedal_fuzz_off_pngSize);
    imgs.chActive    = img (BinaryData::pedal_chorus_active_png,    BinaryData::pedal_chorus_active_pngSize);
    imgs.chOff       = img (BinaryData::pedal_chorus_off_png,       BinaryData::pedal_chorus_off_pngSize);
    imgs.phActive    = img (BinaryData::pedal_phaser_active_png,    BinaryData::pedal_phaser_active_pngSize);
    imgs.phOff       = img (BinaryData::pedal_phaser_off_png,       BinaryData::pedal_phaser_off_pngSize);
    imgs.delayActive = img (BinaryData::pedal_delay_active_png,     BinaryData::pedal_delay_active_pngSize);
    imgs.delayOff    = img (BinaryData::pedal_delay_off_png,        BinaryData::pedal_delay_off_pngSize);
    imgs.rvActive    = img (BinaryData::pedal_reverb_active_png,    BinaryData::pedal_reverb_active_pngSize);
    imgs.rvOff       = img (BinaryData::pedal_reverb_off_png,       BinaryData::pedal_reverb_off_pngSize);

    imgs.compKnob[0]  = img (BinaryData::knob_compresor_0_png, BinaryData::knob_compresor_0_pngSize);
    imgs.compKnob[1]  = img (BinaryData::knob_compresor_1_png, BinaryData::knob_compresor_1_pngSize);
    imgs.compKnob[2]  = img (BinaryData::knob_compresor_2_png, BinaryData::knob_compresor_2_pngSize);
    imgs.driveKnob[0] = img (BinaryData::knob_drive_0_png,     BinaryData::knob_drive_0_pngSize);
    imgs.driveKnob[1] = img (BinaryData::knob_drive_1_png,     BinaryData::knob_drive_1_pngSize);
    imgs.driveKnob[2] = img (BinaryData::knob_drive_2_png,     BinaryData::knob_drive_2_pngSize);
    imgs.chKnob[0]    = img (BinaryData::knob_chorus_0_png,    BinaryData::knob_chorus_0_pngSize);
    imgs.chKnob[1]    = img (BinaryData::knob_chorus_1_png,    BinaryData::knob_chorus_1_pngSize);
    imgs.chKnob[2]    = img (BinaryData::knob_chorus_2_png,    BinaryData::knob_chorus_2_pngSize);
    imgs.phKnob[0]    = img (BinaryData::knob_phaser_0_png,    BinaryData::knob_phaser_0_pngSize);
    imgs.phKnob[1]    = img (BinaryData::knob_phaser_1_png,    BinaryData::knob_phaser_1_pngSize);
    imgs.phKnob[2]    = img (BinaryData::knob_phaser_2_png,    BinaryData::knob_phaser_2_pngSize);
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
    syncHeadControls();
    layoutAll();
    repaint();
}

juce::String MiAmpAudioProcessorEditor::pedalName (bool bassMode, int type)
{
    if (bassMode)
    {
        switch (type)
        {
            case 0: return "Compresor";
            case 1: return "XV Bass";
            case 2: return "Chorus";
            case 3: return "Phaser";
            case 4: return "Reverb";
            default: return "Pedal";
        }
    }
    switch (type)
    {
        case 0: return "Compresor";
        case 1: return "Drive";
        case 2: return "Fuzz";
        case 3: return "Chorus";
        case 4: return "Phaser";
        case 5: return "Delay";
        case 6: return "Reverb";
        default: return "Pedal";
    }
}

const juce::Image& MiAmpAudioProcessorEditor::pedalImage (bool bassMode, int type, bool active) const
{
    if (bassMode)
    {
        switch (type)
        {
            case 0: return active ? imgs.compActive : imgs.compOff;
            case 1: return active ? imgs.xvActive   : imgs.xvBypass;
            case 2: return active ? imgs.chActive   : imgs.chOff;
            case 3: return active ? imgs.phActive   : imgs.phOff;
            case 4:
            default: return active ? imgs.rvActive  : imgs.rvOff;
        }
    }
    switch (type)
    {
        case 0: return active ? imgs.compActive  : imgs.compOff;
        case 1: return active ? imgs.driveActive : imgs.driveOff;
        case 2: return active ? imgs.fuzzActive  : imgs.fuzzOff;
        case 3: return active ? imgs.chActive    : imgs.chOff;
        case 4: return active ? imgs.phActive    : imgs.phOff;
        case 5: return active ? imgs.delayActive : imgs.delayOff;
        case 6:
        default: return active ? imgs.rvActive   : imgs.rvOff;
    }
}

const char* MiAmpAudioProcessorEditor::pedalBypassParamId (bool bassMode, int type)
{
    if (bassMode)
    {
        switch (type)
        {
            case 0: return MiAmpParams::compOn;
            case 1: return MiAmpParams::xvOn;
            case 2: return MiAmpParams::chorusOn;
            case 3: return MiAmpParams::phaserOn;
            case 4:
            default: return MiAmpParams::reverbOn;
        }
    }
    switch (type)
    {
        case 0: return MiAmpParams::gtrCompOn;
        case 1: return MiAmpParams::gtrDriveOn;
        case 2: return MiAmpParams::gtrFuzzOn;
        case 3: return MiAmpParams::gtrChorusOn;
        case 4: return MiAmpParams::gtrPhaserOn;
        case 5: return MiAmpParams::gtrDelayOn;
        case 6:
        default: return MiAmpParams::gtrReverbOn;
    }
}

void MiAmpAudioProcessorEditor::showHud (const juce::String& text)
{
    hudText = text;
    hudTicks = 110; // ~1.8s at 60Hz
    repaint();
}

void MiAmpAudioProcessorEditor::switchInstrument (bool toBass)
{
    if (audioProcessor.isBassMode() == toBass)
        return;

    audioProcessor.setInstrumentMode (toBass);
    activePopover = PopoverKind::None;
    popover->setVisible (false);
    showHud (juce::String::fromUTF8 (toBass ? u8"BAJO \u00b7 XV EDITION" : u8"GUITARRA \u00b7 ZENBOX / ENEMA"));
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
    activePopover = PopoverKind::None;
    popover->setVisible (false);
    draggingPedalActive = false;

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

void MiAmpAudioProcessorEditor::syncHeadControls()
{
    const bool bass = audioProcessor.isBassMode();
    const bool showHead = ! isFloorMode && (! introRunning || introElapsed >= 2.50f);

    btnInstGuitar.setToggleState (! bass, juce::dontSendNotification);
    btnInstBass.setToggleState   (bass,   juce::dontSendNotification);

    for (auto* b : { &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainPedals })
        b->setVisible (showHead);

    for (auto* b : { &btnDockDrop, &btnDockConfig })
        b->setVisible (! isFloorMode && ! introRunning);

    for (auto& k : bassHeadKnobs)      k->setVisible (! isFloorMode && ! introRunning && bass);
    for (auto& s : bassHeadSwitches)   s->setVisible (! isFloorMode && ! introRunning && bass);
    for (auto& k : guitarHeadKnobs)    k->setVisible (! isFloorMode && ! introRunning && ! bass);
    for (auto& s : guitarHeadSwitches) s->setVisible (! isFloorMode && ! introRunning && ! bass);

    btnFloorPresets.setVisible (isFloorMode);
    btnFloorTuner.setVisible   (isFloorMode);
    btnFloorDrop.setVisible    (isFloorMode);
    btnFloorConfig.setVisible  (isFloorMode);
    btnBackToAmp.setVisible    (isFloorMode);
    btnSkipIntro.setVisible    (introRunning && ! isFloorMode);
}

void MiAmpAudioProcessorEditor::togglePopover (PopoverKind kind)
{
    if (activePopover == kind || kind == PopoverKind::None)
    {
        activePopover = PopoverKind::None;
        popover->setVisible (false);
    }
    else
    {
        // In floor mode, opening Presets/Drop/Config closes the left editor & slides board left (exact index.html behavior!)
        if (isFloorMode && (kind == PopoverKind::Presets || kind == PopoverKind::Drop || kind == PopoverKind::Config))
        {
            editorOpen = false;
            floorTunerOpen = false;
            syncPedalEditorControls();
        }
        activePopover = kind;
        popover->setKind (kind, tunerReading);
        popover->setVisible (true);
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

    const int nPalette = bass ? 5 : 7;
    for (int idx = 0; idx < 7; ++idx)
    {
        if (idx >= nPalette)
        {
            paletteItems[idx]->setVisible (false);
            continue;
        }
        const int pt = bass ? kBassPaletteOrder[idx] : kGuitarPaletteOrder[idx];
        const bool inChain = std::find (chain.begin(), chain.end(), pt) != chain.end();
        paletteItems[idx]->setData (
            pedalName (bass, pt).toUpperCase(),
            pedalImage (bass, pt, true),
            [this, bass, pt]
            {
                audioProcessor.addPedalToChain (bass, pt, -1);
                if (bass) selectedBassPedal = pt; else selectedGuitarPedal = pt;
                if (activePopover == PopoverKind::Presets) togglePopover (PopoverKind::None);
                editorOpen = true;
                syncPedalEditorControls();
                repaint();
            },
            [this, bass, pt] (juce::Point<float> editorPt)
            {
                draggingPedalActive = true;
                draggingFromPalette = true;
                draggingOutsideBoard = false;
                draggingPedalType   = pt;
                dragStagePos = editorPtToStage (editorPt);
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
                        if (activePopover == PopoverKind::Presets) togglePopover (PopoverKind::None);
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
            });
        paletteItems[idx]->setInChain (inChain);
        paletteItems[idx]->setVisible (isFloorMode && ! floorTunerOpen);
    }

    btnFloorTunerNotation.setVisible (isFloorMode && floorTunerOpen);
    btnFloorTunerMute.setVisible     (isFloorMode && floorTunerOpen);

    if (! showEd)
    {
        for (auto& k : editorKnobs) k->setVisible (false);
        return;
    }

    auto it = std::find (chain.begin(), chain.end(), sel);
    const int posIdx = (it != chain.end()) ? static_cast<int> (std::distance (chain.begin(), it)) : 0;
    btnMoveLeft.setEnabled (posIdx > 0);
    btnMoveRight.setEnabled (posIdx + 1 < static_cast<int> (chain.size()));

    if (bass && sel == 1)
    {
        for (int i = 0; i < 7; ++i)
        {
            editorKnobs[i]->setParam (kXvKnobIds[i], kXvKnobNames[i], imgs.xvKnobPtr);
            editorKnobs[i]->setVisible (true);
        }
    }
    else
    {
        const char* ids[3] = {};
        const char* names[3] = {};
        const juce::Image* kImgs = imgs.compKnob;

        if (bass)
        {
            if (sel == 0)
            {
                ids[0] = MiAmpParams::compSustain; ids[1] = MiAmpParams::compAttack; ids[2] = MiAmpParams::compLevel;
                names[0] = "SUSTAIN"; names[1] = "ATTACK"; names[2] = "LEVEL";
                kImgs = imgs.compKnob;
            }
            else if (sel == 2)
            {
                ids[0] = MiAmpParams::chorusRate; ids[1] = MiAmpParams::chorusDepth; ids[2] = MiAmpParams::chorusMix;
                names[0] = "RATE"; names[1] = "DEPTH"; names[2] = "MIX";
                kImgs = imgs.chKnob;
            }
            else if (sel == 3)
            {
                ids[0] = MiAmpParams::phaserRate; ids[1] = MiAmpParams::phaserDepth; ids[2] = MiAmpParams::phaserFeedback;
                names[0] = "RATE"; names[1] = "DEPTH"; names[2] = "FEEDBACK";
                kImgs = imgs.phKnob;
            }
            else
            {
                ids[0] = MiAmpParams::reverbDecay; ids[1] = MiAmpParams::reverbTone; ids[2] = MiAmpParams::reverbMix;
                names[0] = "DECAY"; names[1] = "TONE"; names[2] = "MIX";
                kImgs = imgs.chKnob;
            }
        }
        else
        {
            switch (sel)
            {
                case 0:
                    ids[0] = MiAmpParams::gtrCompSus; ids[1] = MiAmpParams::gtrCompAtt; ids[2] = MiAmpParams::gtrCompLvl;
                    names[0] = "SUSTAIN"; names[1] = "ATTACK"; names[2] = "LEVEL";
                    kImgs = imgs.compKnob;
                    break;
                case 1:
                    ids[0] = MiAmpParams::gtrDriveGain; ids[1] = MiAmpParams::gtrDriveTone; ids[2] = MiAmpParams::gtrDriveLvl;
                    names[0] = "GAIN"; names[1] = "TONE"; names[2] = "LEVEL";
                    kImgs = imgs.driveKnob;
                    break;
                case 2:
                    ids[0] = MiAmpParams::gtrFuzzFuzz; ids[1] = MiAmpParams::gtrFuzzTone; ids[2] = MiAmpParams::gtrFuzzLvl;
                    names[0] = "FUZZ"; names[1] = "TONE"; names[2] = "LEVEL";
                    kImgs = imgs.driveKnob;
                    break;
                case 3:
                    ids[0] = MiAmpParams::gtrChorusRate; ids[1] = MiAmpParams::gtrChorusDepth; ids[2] = MiAmpParams::gtrChorusMix;
                    names[0] = "RATE"; names[1] = "DEPTH"; names[2] = "MIX";
                    kImgs = imgs.chKnob;
                    break;
                case 4:
                    ids[0] = MiAmpParams::gtrPhaserRate; ids[1] = MiAmpParams::gtrPhaserDepth; ids[2] = MiAmpParams::gtrPhaserFb;
                    names[0] = "RATE"; names[1] = "DEPTH"; names[2] = "FEEDBACK";
                    kImgs = imgs.phKnob;
                    break;
                case 5:
                    ids[0] = MiAmpParams::gtrDelayTime; ids[1] = MiAmpParams::gtrDelayFb; ids[2] = MiAmpParams::gtrDelayMix;
                    names[0] = "TIME"; names[1] = "FEEDBACK"; names[2] = "MIX";
                    kImgs = imgs.phKnob;
                    break;
                case 6:
                default:
                    ids[0] = MiAmpParams::gtrReverbDec; ids[1] = MiAmpParams::gtrReverbTone; ids[2] = MiAmpParams::gtrReverbMix;
                    names[0] = "DECAY"; names[1] = "TONE"; names[2] = "MIX";
                    kImgs = imgs.chKnob;
                    break;
            }
        }

        for (int i = 0; i < 3; ++i)
        {
            editorKnobs[i]->setParam (ids[i], names[i], kImgs[i]);
            editorKnobs[i]->setVisible (true);
        }
        for (int i = 3; i < 7; ++i)
            editorKnobs[i]->setVisible (false);
    }

    if (popover->isVisible())
        popover->toFront (true);

    layoutAll();
}

void MiAmpAudioProcessorEditor::timerCallback()
{
    if (introRunning && ! isFloorMode)
    {
        introElapsed += 1.0f / 60.0f;
        const float tn = introElapsed / 2.9f;
        if (tn >= 1.0f)
        {
            finishIntro();
        }
        else
        {
            const float prevY = headSceneYOffsetStage;
            if (tn <= 0.46f)
                headSceneYOffsetStage = kStageH;
            else
                headSceneYOffsetStage = kStageH * (1.0f - cubicBezierIntro ((tn - 0.46f) / 0.54f));

            if (introElapsed >= 2.50f && ! btnMainPresets.isVisible())
                syncHeadControls();

            if (std::fabs (headSceneYOffsetStage - prevY) > 0.1f)
                layoutAll();
        }
    }

    // Smooth .board-group transition matching CSS transition(.55s cubic-bezier)
    if (isFloorMode)
    {
        const bool popFloor = (activePopover == PopoverKind::Presets
                            || activePopover == PopoverKind::Drop
                            || activePopover == PopoverKind::Config);
        const bool expanded = ! editorOpen && ! popFloor;

        const float targetTx    = popFloor ? -330.0f : (expanded ? -260.0f : 0.0f);
        const float targetTy    = popFloor ?    0.0f : (expanded ?  -25.0f : 0.0f);
        const float targetScale = popFloor ?    1.0f : (expanded ?   1.12f : 1.0f);

        boardTx    += 0.16f * (targetTx    - boardTx);
        boardTy    += 0.16f * (targetTy    - boardTy);
        boardScale += 0.16f * (targetScale - boardScale);
    }

    tunerReading = audioProcessor.tuner.analyze();
    if (activePopover == PopoverKind::Tuner && popover != nullptr)
        popover->updateTuner (tunerReading);

    if (hudTicks > 0 && --hudTicks == 0)
        hudText.clear();

    const bool dropOn = audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchOn)->load() > 0.5f;
    const int semi = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchSemi)->load()));
    const juce::String dropTxt = dropOn ? ("DROP " + juce::String (semi > 0 ? "+" : "") + juce::String (semi)) : "DROP";
    btnDockDrop.setButtonText (dropTxt);
    btnDockDrop.setToggleState (dropOn || activePopover == PopoverKind::Drop, juce::dontSendNotification);
    btnDockConfig.setToggleState (activePopover == PopoverKind::Config, juce::dontSendNotification);

    btnFloorDrop.setToggleState   (dropOn || activePopover == PopoverKind::Drop, juce::dontSendNotification);
    btnFloorConfig.setToggleState (activePopover == PopoverKind::Config, juce::dontSendNotification);

    btnMainPresets.setToggleState  (activePopover == PopoverKind::Presets, juce::dontSendNotification);
    btnMainTuner.setToggleState    (activePopover == PopoverKind::Tuner,   juce::dontSendNotification);
    btnFloorPresets.setToggleState (activePopover == PopoverKind::Presets, juce::dontSendNotification);
    btnFloorTuner.setToggleState   (floorTunerOpen, juce::dontSendNotification);

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
    // Inside .edit (35, 28, 420, 577) -> .physical: 285x405; .xv-physical: 365px wide
    const bool bass = audioProcessor.isBassMode();
    if (bass && selectedBassPedal == 1)
    {
        const float w = 365.0f;
        const float h = w * (1131.0f / 1391.0f);
        return { 35.0f + (420.0f - w) * 0.5f, 28.0f + 56.0f, w, h };
    }
    const float w = 285.0f;
    const float h = 405.0f;
    return { 35.0f + (420.0f - w) * 0.5f, 28.0f + 46.0f, w, h };
}

std::vector<juce::Rectangle<float>> MiAmpAudioProcessorEditor::computeBoardSlotRects (bool bassMode, const std::vector<int>& chain) const
{
    std::vector<juce::Rectangle<float>> rects;
    const int count = static_cast<int> (chain.size());
    if (count <= 0) return rects;

    // Exact renderBoard() from index.html: 2 rows (max 5 per row / 818px), w = XV?224:123, h = 192, rowGap = 210
    const float vx = 621.0f;
    const float vy = 153.0f;
    const float rowGap = 210.0f;

    std::vector<std::vector<int>> rows;
    std::vector<int> cur;
    float used = 0.0f;
    for (int i = 0; i < count; ++i)
    {
        const float w = (bassMode && chain[static_cast<size_t> (i)] == 1) ? 224.0f : 123.0f;
        if (static_cast<int> (cur.size()) >= 5 || (! cur.empty() && used + 20.0f + w > 818.0f))
        {
            rows.push_back (cur);
            cur.clear();
            used = 0.0f;
        }
        used += (cur.empty() ? 0.0f : 20.0f) + w;
        cur.push_back (i);
    }
    if (! cur.empty())
        rows.push_back (cur);

    rects.resize (static_cast<size_t> (count));
    for (size_t ri = 0; ri < rows.size(); ++ri)
    {
        const auto& row = rows[ri];
        float total = 0.0f;
        for (int idx : row)
            total += (bassMode && chain[static_cast<size_t> (idx)] == 1) ? 224.0f : 123.0f;

        const float space = (row.size() > 1)
            ? juce::jmin (34.0f, (818.0f - total) / static_cast<float> (row.size() - 1))
            : 0.0f;

        float x = vx + 8.0f;
        const float y = vy + 8.0f + static_cast<float> (ri) * rowGap;
        for (int idx : row)
        {
            const bool isXv = (bassMode && chain[static_cast<size_t> (idx)] == 1);
            const float w = isXv ? 224.0f : 123.0f;
            const float h = isXv ? 182.0f : 192.0f;
            rects[static_cast<size_t> (idx)] = juce::Rectangle<float> (x, y + (isXv ? 5.0f : 0.0f), w, h);
            x += w + space;
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

    if (! isFloorMode)
    {
        // .audio-dock (35, 25, w=368, h=52) - exact index.html size for both Guitar & Bass!
        btnDockDrop.setBounds   (toScreenInt (240, 33, 72, 36));
        btnDockConfig.setBounds (toScreenInt (318, 33, 74, 36));

        // .menu TOP RIGHT (1290, 25, 225, 226) matching index.html!
        btnInstGuitar.setBounds  (toScreenInt (1308, 45,  92, 35));
        btnInstBass.setBounds    (toScreenInt (1404, 45,  92, 35));
        btnMainPresets.setBounds (toScreenInt (1305, 94,  195, 41));
        btnMainTuner.setBounds   (toScreenInt (1305, 143, 195, 41));
        btnMainPedals.setBounds  (toScreenInt (1305, 192, 195, 41));

        if (bass)
        {
            const float knobDiam = 58.0f * kAmpZoom;
            for (int i = 0; i < 5; ++i)
            {
                const float xStage = kBassHeadKnobX[i] * (kStageW / 1376.0f);
                const auto pt = ampZoomPt (xStage, kBassHeadKnobY);
                bassHeadKnobs[i]->setBounds (toScreenInt (pt.x - knobDiam * 0.5f, pt.y - knobDiam * 0.5f, knobDiam, knobDiam));
            }

            for (int i = 0; i < 3; ++i)
            {
                const float xStage = kBassSwitchX[i] * (kStageW / 1376.0f);
                const auto pt = ampZoomPt (xStage, kBassSwitchY);
                const float swSz = 40.0f * kAmpZoom;
                bassHeadSwitches[i]->setBounds (toScreenInt (pt.x - swSz * 0.5f, pt.y - swSz * 0.5f, swSz, swSz));
            }
        }
        else
        {
            const float wStage = 1.18f * (19.0f * 2.0f * kGtrScX * (kStageW / 1376.0f)) * kAmpZoom;
            for (int i = 0; i < 9; ++i)
            {
                const float cxTop = (183.0f + (kGtrXs[i] - 180.0f) * kGtrScX) * (kStageW / 1376.0f);
                const float cyTop = 99.0f + (380.0f - 50.0f) * kGtrScY;
                const auto ptTop = ampZoomPt (cxTop, cyTop);
                guitarHeadKnobs[i]->setBounds (toScreenInt (ptTop.x - wStage * 0.5f, ptTop.y - wStage * 0.5f, wStage, wStage));

                const float cyBot = 99.0f + (445.0f - 50.0f) * kGtrScY;
                const auto ptBot = ampZoomPt (cxTop, cyBot);
                guitarHeadKnobs[9 + i]->setBounds (toScreenInt (ptBot.x - wStage * 0.5f, ptBot.y - wStage * 0.5f, wStage, wStage));
            }
            const float cxOut = (183.0f + (960.0f - 180.0f) * kGtrScX) * (kStageW / 1376.0f);
            const float cyOut = 99.0f + (380.0f - 50.0f) * kGtrScY;
            const auto ptOut = ampZoomPt (cxOut, cyOut);
            guitarHeadKnobs[18]->setBounds (toScreenInt (ptOut.x - wStage * 0.5f, ptOut.y - wStage * 0.5f, wStage, wStage));

            for (int i = 0; i < 3; ++i)
            {
                const float xStage = kGtrSwitchX[i] * (kStageW / 1376.0f);
                const auto pt = ampZoomPt (xStage, kGtrSwitchY[i]);
                const float swSz = kGtrSwitchSize[i] * kAmpZoom;
                guitarHeadSwitches[i]->setBounds (toScreenInt (pt.x - swSz * 0.5f, pt.y - swSz * 0.5f, swSz, swSz));
            }
        }

        // .popover sits BELOW .menu (right: 35 -> x=1215, top: 269, w=300, h=474)
        if (popover->isVisible())
            popover->setBounds (toScreenInt (1215, 269, 300, 474));
    }
    else
    {
        // .edit controls (35, 28, 420, 577)
        btnEditClose.setBounds   (toScreenInt (412, 38,  30, 30));
        btnMoveLeft.setBounds    (toScreenInt (57,  553, 36, 34));
        btnMoveRight.setBounds   (toScreenInt (175, 553, 36, 34));
        btnTrashPedal.setBounds  (toScreenInt (392, 544, 44, 44)); // circular 44x44 .trash-btn

        const auto pedR = computeEditorPedalStageRect();
        const int sel = bass ? selectedBassPedal : selectedGuitarPedal;
        if (bass && sel == 1)
        {
            const float kSz = pedR.getWidth() * 0.116f;
            for (int i = 0; i < 7; ++i)
            {
                const float cx = pedR.getX() + kXvPoints[i][0] * pedR.getWidth();
                const float cy = pedR.getY() + kXvPoints[i][1] * pedR.getHeight();
                editorKnobs[i]->setBounds (toScreenInt (cx - kSz * 0.5f, cy - kSz * 0.5f, kSz, kSz));
            }
        }
        else
        {
            const float (*pts)[2] = kCompPoints;
            float sizeFrac = 0.213338f;
            if (bass)
            {
                if (sel == 2 || sel == 4) { pts = kChorusPoints; sizeFrac = 0.248418f; }
                else if (sel == 3)        { pts = kPhaserPoints; sizeFrac = 0.218456f; }
            }
            else
            {
                if (sel == 1 || sel == 2)      { pts = kDrivePoints;  sizeFrac = 0.208729f; }
                else if (sel == 3 || sel == 6) { pts = kChorusPoints; sizeFrac = 0.248418f; }
                else if (sel == 4 || sel == 5) { pts = kPhaserPoints; sizeFrac = 0.218456f; }
            }

            const float kSz = pedR.getWidth() * sizeFrac;
            for (int i = 0; i < 3; ++i)
            {
                const float cx = pedR.getX() + pts[i][0] * pedR.getWidth();
                const float cy = pedR.getY() + pts[i][1] * pedR.getHeight();
                editorKnobs[i]->setBounds (toScreenInt (cx - kSz * 0.5f, cy - kSz * 0.5f, kSz, kSz));
            }
        }

        // .palette (36, 623, 1478, 125) -> 100x100 buttons (145x100 for XV)
        float px = 266.0f;
        const int nPal = bass ? 5 : 7;
        for (int idx = 0; idx < nPal; ++idx)
        {
            const float iw = (bass && idx == 4) ? 145.0f : 100.0f;
            paletteItems[idx]->setBounds (toScreenInt (px, 635, iw, 100));
            px += iw + 12.0f;
        }

        btnFloorTunerNotation.setBounds (toScreenInt (1030, 640, 175, 36));
        btnFloorTunerMute.setBounds     (toScreenInt (1030, 684, 175, 39));

        // .palette-nav (right side of .palette, w=250 -> x=1246..1496)
        btnFloorPresets.setBounds (toScreenInt (1246, 630, 122, 34));
        btnFloorTuner.setBounds   (toScreenInt (1374, 630, 122, 34));
        btnFloorDrop.setBounds    (toScreenInt (1246, 668, 122, 34));
        btnFloorConfig.setBounds  (toScreenInt (1374, 668, 122, 34));
        btnBackToAmp.setBounds    (toScreenInt (1246, 706, 250, 34));

        if (popover->isVisible())
            popover->setBounds (toScreenInt (1215, 110, 300, 474));
    }
}

void MiAmpAudioProcessorEditor::paintGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius) const
{
    const float rad = radius * stageScale();
    juce::ColourGradient grad (juce::Colour::fromRGBA (239, 222, 183, 74), r.getX(), r.getY(),
                               juce::Colour::fromRGBA (53, 54, 45, 178), r.getRight(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (r, rad);
    g.setColour (juce::Colour::fromRGBA (255, 243, 210, 158));
    g.drawRoundedRectangle (r, rad, 1.5f);
}

void MiAmpAudioProcessorEditor::paintMiniPedalKnobs (juce::Graphics& g, bool bassMode, int type, juce::Rectangle<float> pedalRect) const
{
    if (bassMode && type == 1)
    {
        const float kSz = pedalRect.getWidth() * 0.116f;
        for (int i = 0; i < 7; ++i)
        {
            const float val = audioProcessor.apvts.getRawParameterValue (kXvKnobIds[i])->load();
            const float rad = juce::degreesToRadians (knobDeg (val));
            const float cx = pedalRect.getX() + kXvPoints[i][0] * pedalRect.getWidth();
            const float cy = pedalRect.getY() + kXvPoints[i][1] * pedalRect.getHeight();
            if (imgs.xvKnobPtr.isValid())
            {
                auto tr = juce::AffineTransform::translation (-imgs.xvKnobPtr.getWidth() * 0.5f, -imgs.xvKnobPtr.getHeight() * 0.5f)
                              .scaled (kSz / imgs.xvKnobPtr.getWidth(), kSz / imgs.xvKnobPtr.getHeight())
                              .rotated (rad)
                              .translated (cx, cy);
                g.drawImageTransformed (imgs.xvKnobPtr, tr, false);
            }
        }
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
        return;
    }

    const float (*pts)[2] = kCompPoints;
    float sizeFrac = 0.213338f;
    const char* ids[3] = {};
    const juce::Image* kImgs = imgs.compKnob;

    if (bassMode)
    {
        if (type == 0)      { ids[0] = MiAmpParams::compSustain; ids[1] = MiAmpParams::compAttack; ids[2] = MiAmpParams::compLevel; kImgs = imgs.compKnob; }
        else if (type == 2) { pts = kChorusPoints; sizeFrac = 0.248418f; ids[0] = MiAmpParams::chorusRate; ids[1] = MiAmpParams::chorusDepth; ids[2] = MiAmpParams::chorusMix; kImgs = imgs.chKnob; }
        else if (type == 3) { pts = kPhaserPoints; sizeFrac = 0.218456f; ids[0] = MiAmpParams::phaserRate; ids[1] = MiAmpParams::phaserDepth; ids[2] = MiAmpParams::phaserFeedback; kImgs = imgs.phKnob; }
        else                { pts = kChorusPoints; sizeFrac = 0.248418f; ids[0] = MiAmpParams::reverbDecay; ids[1] = MiAmpParams::reverbTone; ids[2] = MiAmpParams::reverbMix; kImgs = imgs.chKnob; }
    }
    else
    {
        switch (type)
        {
            case 0: ids[0] = MiAmpParams::gtrCompSus; ids[1] = MiAmpParams::gtrCompAtt; ids[2] = MiAmpParams::gtrCompLvl; kImgs = imgs.compKnob; break;
            case 1: pts = kDrivePoints;  sizeFrac = 0.208729f; ids[0] = MiAmpParams::gtrDriveGain; ids[1] = MiAmpParams::gtrDriveTone; ids[2] = MiAmpParams::gtrDriveLvl; kImgs = imgs.driveKnob; break;
            case 2: pts = kDrivePoints;  sizeFrac = 0.208729f; ids[0] = MiAmpParams::gtrFuzzFuzz;  ids[1] = MiAmpParams::gtrFuzzTone;  ids[2] = MiAmpParams::gtrFuzzLvl;  kImgs = imgs.driveKnob; break;
            case 3: pts = kChorusPoints; sizeFrac = 0.248418f; ids[0] = MiAmpParams::gtrChorusRate;ids[1] = MiAmpParams::gtrChorusDepth;ids[2] = MiAmpParams::gtrChorusMix; kImgs = imgs.chKnob; break;
            case 4: pts = kPhaserPoints; sizeFrac = 0.218456f; ids[0] = MiAmpParams::gtrPhaserRate;ids[1] = MiAmpParams::gtrPhaserDepth;ids[2] = MiAmpParams::gtrPhaserFb;  kImgs = imgs.phKnob; break;
            case 5: pts = kPhaserPoints; sizeFrac = 0.218456f; ids[0] = MiAmpParams::gtrDelayTime; ids[1] = MiAmpParams::gtrDelayFb;   ids[2] = MiAmpParams::gtrDelayMix; kImgs = imgs.phKnob; break;
            case 6:
            default: pts = kChorusPoints; sizeFrac = 0.248418f; ids[0] = MiAmpParams::gtrReverbDec; ids[1] = MiAmpParams::gtrReverbTone; ids[2] = MiAmpParams::gtrReverbMix; kImgs = imgs.chKnob; break;
        }
    }

    const float kSz = pedalRect.getWidth() * sizeFrac;
    for (int i = 0; i < 3; ++i)
    {
        const float val = audioProcessor.apvts.getRawParameterValue (ids[i])->load();
        const float rad = juce::degreesToRadians (knobDeg (val));
        const float cx = pedalRect.getX() + pts[i][0] * pedalRect.getWidth();
        const float cy = pedalRect.getY() + pts[i][1] * pedalRect.getHeight();
        if (kImgs[i].isValid())
        {
            auto tr = juce::AffineTransform::translation (-kImgs[i].getWidth() * 0.5f, -kImgs[i].getHeight() * 0.5f)
                          .scaled (kSz / kImgs[i].getWidth(), kSz / kImgs[i].getHeight())
                          .rotated (rad)
                          .translated (cx, cy);
            g.drawImageTransformed (kImgs[i], tr, false);
        }
    }
}

void MiAmpAudioProcessorEditor::paintHeadScene (juce::Graphics& g, float yOff)
{
    const bool bass = audioProcessor.isBassMode();
    const auto bgStage = juce::Rectangle<float> (
        kAmpOriginX - kAmpOriginX * kAmpZoom,
        kAmpOriginY - kAmpOriginY * kAmpZoom + yOff,
        kStageW * kAmpZoom,
        kStageH * kAmpZoom);

    const auto& bgImg = bass ? imgs.bassBg : imgs.guitarBg;
    if (bgImg.isValid())
        g.drawImage (bgImg, toScreen (bgStage.getX(), bgStage.getY(), bgStage.getWidth(), bgStage.getHeight()),
                     juce::RectanglePlacement::stretchToFit);

    const bool pwr = audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::ampPower : MiAmpParams::gtrPower)->load() > 0.5f;
    const bool standby = audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::ampStandby : MiAmpParams::gtrStandby)->load() > 0.5f;

    // Exact .amp-power-led (left:967, top:354, 19x19 metallic dark) when Power is OFF in Bass
    if (bass && ! pwr)
    {
        const auto pt = ampZoomPt (967.0f, 354.0f);
        const float d = 19.0f * kAmpZoom;
        const auto lr = toScreen (pt.x, pt.y + yOff, d, d);
        juce::ColourGradient lg (juce::Colour::fromRGB (164, 154, 144), lr.getX() + lr.getWidth() * 0.38f, lr.getY() + lr.getHeight() * 0.30f,
                                 juce::Colour::fromRGB (52, 51, 47),    lr.getRight(), lr.getBottom(), true);
        lg.addColour (0.45, juce::Colour::fromRGB (97, 93, 88));
        g.setGradientFill (lg);
        g.fillEllipse (lr);
    }

    if (introRunning)
        return;

    // Top-left .audio-dock (35, 25, 368, 52) — identical in Guitar & Bass!
    paintGlassPanel (g, toScreen (35, 25, 368, 52), 15.0f);

    const bool clip = audioProcessor.isClipping.load();
    const juce::Colour ledCol = clip ? juce::Colour::fromRGB (255, 94, 94)
                                     : (! pwr || standby ? juce::Colour::fromRGB (247, 197, 107)
                                                         : juce::Colour::fromRGB (126, 255, 189));
    g.setColour (ledCol);
    g.fillEllipse (toScreen (50, 46, 10, 10));

    const float s = stageScale();
    const float inFrac  = juce::jlimit (0.0f, 1.0f, audioProcessor.inputRms.load() * 3.5f);
    const float outFrac = juce::jlimit (0.0f, 1.0f, audioProcessor.outputRms.load() * 3.5f);
    g.setColour (inkColour().withAlpha (0.78f));
    g.setFont (sansFont (juce::jmax (8.0f, 9.5f * s), juce::Font::bold));
    g.drawText ("IN",  toScreen (72, 36, 24, 12), juce::Justification::centredLeft);
    g.drawText ("OUT", toScreen (72, 52, 26, 12), juce::Justification::centredLeft);

    auto drawMeter = [&] (float y, float frac)
    {
        auto tr = toScreen (100, y, 128, 6);
        g.setColour (juce::Colour::fromRGBA (15, 22, 20, 165));
        g.fillRoundedRectangle (tr, 3.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 244, 212, 56));
        g.drawRoundedRectangle (tr, 3.0f * s, 0.8f);
        if (frac > 0.01f)
        {
            g.setColour (frac > 0.85f ? juce::Colour::fromRGB (255, 107, 91)
                                      : juce::Colour::fromRGB (136, 226, 181));
            g.fillRoundedRectangle (tr.withWidth (tr.getWidth() * frac), 3.0f * s);
        }
    };
    drawMeter (39.0f, inFrac);
    drawMeter (55.0f, outFrac);

    // Right .menu TOP RIGHT (1290, 25, 225, 226)
    paintGlassPanel (g, toScreen (1290, 25, 225, 226), 22.0f);
    auto swBox = toScreen (1305, 42, 195, 41);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 24));
    g.fillRoundedRectangle (swBox, 8.0f * s);
    g.setColour (juce::Colour::fromRGBA (241, 224, 190, 102));
    g.drawRoundedRectangle (swBox, 8.0f * s, 1.0f);

    if (hudText.isNotEmpty())
    {
        auto hudR = toScreen (35, 92, 245, 34);
        paintGlassPanel (g, hudR, 11.0f);
        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (10.0f, 12.5f * s), juce::Font::bold));
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
        g.drawText (juce::String::fromUTF8 (u8"TU PEDALBOARD EST\u00c1 VAC\u00cdA \u2014 Arrastr\u00e1 un pedal desde abajo"),
                    boardScreenR, juce::Justification::centred);
    }
    else
    {
        for (size_t i = 0; i < chain.size(); ++i)
        {
            const int pt = chain[i];
            const bool isBeingDragged = draggingPedalActive && ! draggingFromPalette && (pt == draggingPedalType);
            const auto slotT = applyBoardTransform (slots[i]);
            const auto sr = toScreen (slotT.getX(), slotT.getY(), slotT.getWidth(), slotT.getHeight());
            const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, pt))->load() > 0.5f;
            const auto& pImg = pedalImage (bass, pt, active);

            // Fit non-XV pedal artwork cleanly in 700:1000 ratio centered inside 123x192 unit
            auto artR = sr;
            if (! (bass && pt == 1))
            {
                const float artW = sr.getWidth();
                const float artH = artW * (1000.0f / 700.0f);
                artR = juce::Rectangle<float> (sr.getX(), sr.getCentreY() - artH * 0.5f, artW, artH);
            }

            if (isBeingDragged)
                g.setOpacity (0.30f);
            else if (! active)
                g.setOpacity (0.65f); // .unit.off brightness(.65)

            if (pImg.isValid())
                g.drawImage (pImg, artR, juce::RectanglePlacement::stretchToFit);

            paintMiniPedalKnobs (g, bass, pt, artR);
            g.setOpacity (1.0f);

            if (pt == sel && showEd && ! isBeingDragged)
            {
                g.setColour (juce::Colour::fromRGB (255, 223, 152));
                g.drawRoundedRectangle (sr.expanded (3.0f * s), 7.0f * s, 2.0f * s);
            }
        }
    }

    // Drag & Drop insertion line + floating pedal ghost + SOLTÁ PARA QUITAR
    if (draggingPedalActive && draggingPedalType >= 0)
    {
        if (dragInsertIndex >= 0 && ! draggingOutsideBoard)
        {
            const auto vpT = applyBoardTransform (juce::Rectangle<float> (621.0f, 153.0f, 830.0f, 435.0f));
            float lineX = vpT.getCentreX();
            float lineY = vpT.getY() + 25.0f;
            float lineH = 180.0f * boardScale;

            if (! slots.empty())
            {
                if (dragInsertIndex == 0)
                {
                    const auto r0 = applyBoardTransform (slots.front());
                    lineX = r0.getX() - 10.0f;
                    lineY = r0.getY();
                }
                else if (dragInsertIndex >= static_cast<int> (slots.size()))
                {
                    const auto rl = applyBoardTransform (slots.back());
                    lineX = rl.getRight() + 10.0f;
                    lineY = rl.getY();
                }
                else
                {
                    const auto ra = applyBoardTransform (slots[static_cast<size_t> (dragInsertIndex - 1)]);
                    const auto rb = applyBoardTransform (slots[static_cast<size_t> (dragInsertIndex)]);
                    lineX = 0.5f * (ra.getRight() + rb.getX());
                    lineY = rb.getY();
                }
            }
            const auto lr = toScreen (lineX - 3.0f, lineY, 6.0f, lineH);
            g.setColour (mintBorder().withAlpha (0.95f));
            g.fillRoundedRectangle (lr, 3.0f * s);
        }

        const bool isXv = (bass && draggingPedalType == 1);
        const float gw = (isXv ? 224.0f : 123.0f) * boardScale;
        const float gh = (isXv ? 182.0f : 176.0f) * boardScale;
        const auto ghostR = toScreen (dragStagePos.x - gw * 0.5f, dragStagePos.y - gh * 0.5f, gw, gh);
        const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, draggingPedalType))->load() > 0.5f;
        const auto& gImg = pedalImage (bass, draggingPedalType, active);
        if (gImg.isValid())
        {
            g.setOpacity (0.90f);
            g.drawImage (gImg, ghostR, juce::RectanglePlacement::stretchToFit);
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
            g.drawText (juce::String::fromUTF8 (u8"SOLT\u00c1 PARA QUITAR"), tagR, juce::Justification::centred);
        }
    }

    // Left .edit glass card (35, 28, 420, 577)
    if (showEd)
    {
        paintGlassPanel (g, toScreen (35, 28, 420, 577), 22.0f);
        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (14.0f, 19.0f * s), juce::Font::bold));
        g.drawText ("EDITAR " + pedalName (bass, sel).toUpperCase(),
                    toScreen (60, 42, 330, 28), juce::Justification::centredLeft);

        const auto pedStage = computeEditorPedalStageRect();
        const auto pedR = toScreen (pedStage.getX(), pedStage.getY(), pedStage.getWidth(), pedStage.getHeight());
        const bool selActive = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, sel))->load() > 0.5f;
        const auto& edImg = pedalImage (bass, sel, selActive);
        if (edImg.isValid())
            g.drawImage (edImg, pedR, juce::RectanglePlacement::stretchToFit);

        if (bass && sel == 1)
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
        else if (! selActive)
        {
            // .physical .off-cover { left: 73%; top: 74%; width: 9%; }
            const float covSz = pedR.getWidth() * 0.09f;
            g.setColour (juce::Colour::fromRGB (58, 48, 48));
            g.fillEllipse (pedR.getX() + 0.73f * pedR.getWidth(),
                           pedR.getY() + 0.74f * pedR.getHeight(),
                           covSz, covSz);
        }

        // Value readout row (.value-readout / .xv-values) matching index.html!
        if (bass && sel == 1)
        {
            g.setFont (sansFont (juce::jmax (9.0f, 10.5f * s), juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.92f));
            const float x0 = 56.0f, rw = 378.0f / 4.0f;
            for (int i = 0; i < 7; ++i)
            {
                const float v = audioProcessor.apvts.getRawParameterValue (kXvKnobIds[i])->load();
                const int col = i % 4, row = i / 4;
                const juce::String txt = juce::String (kXvKnobNames[i]).substring (0, 4) + " " + juce::String (juce::roundToInt (v));
                g.drawText (txt, toScreen (x0 + col * rw, 390.0f + row * 22.0f, rw, 20.0f), juce::Justification::centred);
            }
        }
        else
        {
            const char* kNames[3] = { "P1", "P2", "P3" };
            const char* kIds[3]   = {};
            if (bass)
            {
                if (sel == 0)      { kNames[0]="SUSTAIN"; kNames[1]="ATTACK"; kNames[2]="LEVEL";    kIds[0]=MiAmpParams::compSustain; kIds[1]=MiAmpParams::compAttack; kIds[2]=MiAmpParams::compLevel; }
                else if (sel == 2) { kNames[0]="RATE";    kNames[1]="DEPTH";  kNames[2]="MIX";      kIds[0]=MiAmpParams::chorusRate;  kIds[1]=MiAmpParams::chorusDepth;kIds[2]=MiAmpParams::chorusMix; }
                else if (sel == 3) { kNames[0]="RATE";    kNames[1]="DEPTH";  kNames[2]="FEEDBACK"; kIds[0]=MiAmpParams::phaserRate;  kIds[1]=MiAmpParams::phaserDepth;kIds[2]=MiAmpParams::phaserFeedback; }
                else               { kNames[0]="DECAY";   kNames[1]="TONE";   kNames[2]="MIX";      kIds[0]=MiAmpParams::reverbDecay; kIds[1]=MiAmpParams::reverbTone; kIds[2]=MiAmpParams::reverbMix; }
            }
            else
            {
                switch (sel)
                {
                    case 0: kNames[0]="SUSTAIN"; kNames[1]="ATTACK"; kNames[2]="LEVEL";    kIds[0]=MiAmpParams::gtrCompSus;    kIds[1]=MiAmpParams::gtrCompAtt;    kIds[2]=MiAmpParams::gtrCompLvl; break;
                    case 1: kNames[0]="GAIN";    kNames[1]="TONE";   kNames[2]="LEVEL";    kIds[0]=MiAmpParams::gtrDriveGain;  kIds[1]=MiAmpParams::gtrDriveTone;  kIds[2]=MiAmpParams::gtrDriveLvl; break;
                    case 2: kNames[0]="FUZZ";    kNames[1]="TONE";   kNames[2]="LEVEL";    kIds[0]=MiAmpParams::gtrFuzzFuzz;   kIds[1]=MiAmpParams::gtrFuzzTone;   kIds[2]=MiAmpParams::gtrFuzzLvl; break;
                    case 3: kNames[0]="RATE";    kNames[1]="DEPTH";  kNames[2]="MIX";      kIds[0]=MiAmpParams::gtrChorusRate; kIds[1]=MiAmpParams::gtrChorusDepth;kIds[2]=MiAmpParams::gtrChorusMix; break;
                    case 4: kNames[0]="RATE";    kNames[1]="DEPTH";  kNames[2]="FEEDBACK"; kIds[0]=MiAmpParams::gtrPhaserRate; kIds[1]=MiAmpParams::gtrPhaserDepth;kIds[2]=MiAmpParams::gtrPhaserFb; break;
                    case 5: kNames[0]="TIME";    kNames[1]="FEEDBACK";kNames[2]="MIX";     kIds[0]=MiAmpParams::gtrDelayTime;  kIds[1]=MiAmpParams::gtrDelayFb;    kIds[2]=MiAmpParams::gtrDelayMix; break;
                    default:kNames[0]="DECAY";   kNames[1]="TONE";   kNames[2]="MIX";      kIds[0]=MiAmpParams::gtrReverbDec;  kIds[1]=MiAmpParams::gtrReverbTone; kIds[2]=MiAmpParams::gtrReverbMix; break;
                }
            }
            g.setFont (sansFont (juce::jmax (10.0f, 12.5f * s), juce::Font::bold));
            g.setColour (inkColour().withAlpha (0.94f));
            const float cw = 378.0f / 3.0f;
            for (int i = 0; i < 3; ++i)
            {
                const int v = juce::roundToInt (audioProcessor.apvts.getRawParameterValue (kIds[i])->load());
                g.drawText (juce::String (kNames[i]) + " " + juce::String (v),
                            toScreen (56.0f + i * cw, 486.0f, cw, 22.0f), juce::Justification::centred);
            }
        }

        auto it = std::find (chain.begin(), chain.end(), sel);
        const int posIdx = (it != chain.end()) ? static_cast<int> (std::distance (chain.begin(), it)) + 1 : 1;
        g.setColour (inkColour().withAlpha (0.85f));
        g.setFont (sansFont (juce::jmax (9.5f, 11.0f * s), juce::Font::plain));
        g.drawText ("CADENA " + juce::String (posIdx) + "/" + juce::String (chain.size()),
                    toScreen (96, 553, 76, 34), juce::Justification::centred);
        g.drawText (juce::String::fromUTF8 (u8"Arrastr\u00e1 una perilla \u00b7 ") + juce::String (selActive ? "ACTIVO" : "BYPASS"),
                    toScreen (56, 514, 330, 26), juce::Justification::centredLeft);
    }

    // Bottom .palette glass bar (36, 623, 1478, 125)
    paintGlassPanel (g, toScreen (36, 623, 1478, 125), 22.0f);
    if (! floorTunerOpen)
    {
        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (10.0f, 13.5f * s), juce::Font::bold));
        g.drawText (juce::String::fromUTF8 (u8"ARRASTR\u00c1 LOS PEDALES"), toScreen (56, 650, 200, 24), juce::Justification::centredLeft);
        g.setColour (inkColour().withAlpha (0.82f));
        g.setFont (sansFont (juce::jmax (9.0f, 11.5f * s), juce::Font::plain));
        g.drawText (juce::String::fromUTF8 (u8"o hac\u00e9 clic para agregar"), toScreen (56, 676, 200, 34), juce::Justification::topLeft);
    }
    else
    {
        // Inline #floorTuner inside .palette with Do Re Mi support + reference notes!
        const bool active = tunerReading.hasSignal && tunerReading.freqHz > 20.0f;
        const bool inTune = active && std::fabs (tunerReading.cents) < 6.0f;
        const juce::String noteStr = active ? formatNoteName (tunerReading.noteName, tunerReading.octave, useSolfege) : "--";

        auto noteBox = toScreen (56, 637, 136, 96);
        g.setColour (juce::Colour::fromRGBA (15, 24, 22, 130));
        g.fillRoundedRectangle (noteBox, 12.0f * s);
        g.setColour (active ? (inTune ? mintBorder() : inkColour()) : inkColour().withAlpha (0.6f));
        g.setFont (sansFont (juce::jmax (20.0f, (useSolfege ? 34.0f : 44.0f) * s), juce::Font::bold));
        g.drawText (noteStr, noteBox, juce::Justification::centred);

        g.setColour (inkColour());
        g.setFont (sansFont (juce::jmax (10.0f, 13.0f * s), juce::Font::bold));
        g.drawText ("AFINADOR", toScreen (210, 638, 220, 20), juce::Justification::centredLeft);
        const juce::String hzStr = active
            ? (juce::String (tunerReading.freqHz, 1) + " Hz  (" + (tunerReading.cents >= 0 ? "+" : "") + juce::String (tunerReading.cents, 1) + " ct)")
            : juce::String::fromUTF8 (u8"Esperando se\u00f1al");
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

    // 1. Intro Sky + MI AMP Logo overlay (2.9s matching index.html!)
    if (introRunning && ! isFloorMode)
    {
        const float tn = juce::jlimit (0.0f, 1.0f, introElapsed / 2.9f);
        float skyAlpha = 1.0f;
        if (tn > 0.88f)
            skyAlpha = 1.0f - (tn - 0.88f) / 0.12f;

        const float skyTopStage = headSceneYOffsetStage - kStageH;
        const auto skyR = toScreen (0.0f, skyTopStage, kStageW, kStageH + 80.0f);

        g.setOpacity (skyAlpha);
        if (imgs.introSky.isValid())
            g.drawImage (imgs.introSky, skyR, juce::RectanglePlacement::stretchToFit);

        // Logo fade in 6%..22%, hold 22%..55%, fade out 55%..80%
        float logoAlpha = 0.0f;
        float logoDy = 0.0f;
        if (tn >= 0.06f && tn < 0.22f)
        {
            const float u = (tn - 0.06f) / 0.16f;
            logoAlpha = u;
            logoDy = 10.0f * (1.0f - u);
        }
        else if (tn >= 0.22f && tn < 0.55f)
        {
            logoAlpha = 1.0f;
        }
        else if (tn >= 0.55f && tn < 0.80f)
        {
            const float u = (tn - 0.55f) / 0.25f;
            logoAlpha = 1.0f - u;
            logoDy = -14.0f * u;
        }

        if (logoAlpha > 0.01f && imgs.introLogo.isValid())
        {
            const float lw = kStageW * 0.64f;
            const float lh = lw * (static_cast<float> (imgs.introLogo.getHeight()) / juce::jmax (1, imgs.introLogo.getWidth()));
            const auto lr = toScreen (kStageW * 0.18f, skyTopStage + kStageH * 0.28f + logoDy, lw, lh);
            g.setOpacity (logoAlpha * skyAlpha);
            g.drawImage (imgs.introLogo, lr, juce::RectanglePlacement::stretchToFit);
        }
        g.setOpacity (1.0f);
    }

    // 2. Credit Easter Egg Overlay (.credit-overlay)
    if (creditOpen)
    {
        g.setColour (juce::Colour::fromRGBA (14, 22, 23, 215));
        g.fillRect (getLocalBounds());

        g.setColour (inkColour().withAlpha (0.85f));
        g.setFont (sansFont (juce::jmax (12.0f, 16.0f * s), juce::Font::bold));
        g.drawText ("DESARROLLADO POR", toScreen (475, 220, 600, 32), juce::Justification::centred);

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

        // Clear CLIP on meter click
        if (juce::Rectangle<float> (45, 25, 190, 52).contains (stagePt))
        {
            audioProcessor.isClipping.store (false, std::memory_order_relaxed);
            return;
        }
        // Easter Egg: .brand-trigger (420, 185, 450, 95)
        if (juce::Rectangle<float> (420, 185, 450, 95).contains (stagePt))
        {
            creditOpen = true;
            btnCreditClose.setVisible (true);
            btnCreditClose.toFront (true);
            repaint();
            return;
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

        const auto swR = (bass && sel == 1)
            ? juce::Rectangle<float> (pedR.getX() + 0.10f * pedR.getWidth(), pedR.getY() + 0.62f * pedR.getHeight(),
                                      0.20f * pedR.getWidth(), 0.24f * pedR.getHeight())
            : juce::Rectangle<float> (pedR.getX() + 0.20f * pedR.getWidth(), pedR.getY() + 0.62f * pedR.getHeight(),
                                      0.60f * pedR.getWidth(), 0.28f * pedR.getHeight());
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
        if (applyBoardTransform (slots[i]).contains (stagePt))
        {
            dragStartPx = e.getPosition();
            dragStagePos = stagePt;
            draggingPedalType = chain[i];
            draggingFromPalette = false;
            draggingPedalActive = false;
            draggingOutsideBoard = false;
            dragInsertIndex = -1;
            return;
        }
    }

    draggingPedalType = -1;
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
            showHud (pedalName (bass, draggingPedalType) + " QUITADO");
        }
        else if (dragInsertIndex >= 0)
        {
            audioProcessor.reorderPedalToIndex (bass, draggingPedalType, dragInsertIndex);
            sel = draggingPedalType;
            if (activePopover != PopoverKind::Presets && activePopover != PopoverKind::Drop && activePopover != PopoverKind::Config)
                editorOpen = true;
            showHud (pedalName (bass, draggingPedalType) + " MOVIDO");
        }
    }
    else
    {
        sel = draggingPedalType;
        if (activePopover == PopoverKind::Presets || activePopover == PopoverKind::Drop || activePopover == PopoverKind::Config)
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
