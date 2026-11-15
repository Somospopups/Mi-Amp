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

    // Guitar Head (main_bg.png: 19 knobs, 3 switches matching index.html)
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

    // Palette orders
    constexpr int kBassPaletteOrder[5]   = { 2, 3, 0, 4, 1 };             // Chorus, Phaser, Compresor, Reverb, XV Bass
    constexpr int kGuitarPaletteOrder[7] = { 1, 2, 3, 4, 0, 5, 6 };       // Drive, Fuzz, Chorus, Phaser, Compresor, Delay, Reverb

    static juce::Point<float> ampZoomPt (float xStage, float yStage) noexcept
    {
        return {
            kAmpOriginX + (xStage - kAmpOriginX) * kAmpZoom,
            kAmpOriginY + (yStage - kAmpOriginY) * kAmpZoom
        };
    }

    static juce::Colour inkColour()  { return juce::Colour::fromRGB (247, 241, 223); }
    static juce::Colour mintBorder() { return juce::Colour::fromRGB (191, 252, 225); }
}

//==============================================================================
// GlassLookAndFeel
//==============================================================================
void MiAmpAudioProcessorEditor::GlassLookAndFeel::drawButtonBackground (
    juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
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
    const float h = juce::jlimit (9.5f, 14.0f, static_cast<float> (b.getHeight()) * 0.35f);
    g.setFont (juce::Font (juce::FontOptions (h, juce::Font::plain)));
    g.setColour (b.isEnabled() ? juce::Colour::fromRGB (250, 243, 227)
                               : juce::Colour::fromRGBA (250, 243, 227, 105));
    g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 2), juce::Justification::centred, 1);
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

//==============================================================================
// ImageRotaryKnob
//==============================================================================
MiAmpAudioProcessorEditor::ImageRotaryKnob::ImageRotaryKnob (
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    const juce::String& labelText,
    juce::Image knobImg,
    bool isHeadStyle,
    std::function<void(const juce::String&)> onHud,
    std::function<void()> onInteract)
    : apvts (state), pid (paramId), label (labelText),
      image (std::move (knobImg)), headStyle (isHeadStyle),
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
    const float norm = juce::jlimit (0.0f, 1.0f, val * 0.01f);
    const float deg = -135.0f + norm * 270.0f;
    const float rad = juce::degreesToRadians (deg);

    const auto b = getLocalBounds().toFloat();
    const float sz = juce::jmin (b.getWidth(), b.getHeight());
    const float cx = b.getCentreX();
    const float cy = b.getCentreY();

    if (headStyle)
    {
        const float capSz = sz * 0.64f;
        if (image.isValid())
        {
            const float sx = capSz / static_cast<float> (image.getWidth());
            const float sy = capSz / static_cast<float> (image.getHeight());
            auto tr = juce::AffineTransform::translation (-image.getWidth() * 0.5f, -image.getHeight() * 0.5f)
                          .scaled (sx, sy)
                          .rotated (rad)
                          .translated (cx, cy);
            g.drawImageTransformed (image, tr, false);
        }

        const float rInner = sz * 0.14f;
        const float rOuter = sz * 0.44f;
        const float sinA = std::sin (rad);
        const float cosA = -std::cos (rad);
        g.setColour (juce::Colour::fromRGBA (255, 255, 255, 240));
        g.drawLine (cx + sinA * rInner, cy + cosA * rInner,
                    cx + sinA * rOuter, cy + cosA * rOuter,
                    juce::jmax (1.5f, sz * 0.045f));

        if (isMouseOver() || dragging)
        {
            g.setColour (juce::Colour::fromRGB (189, 238, 208));
            g.drawEllipse (cx - sz * 0.47f, cy - sz * 0.47f, sz * 0.94f, sz * 0.94f, 2.0f);
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
        hudCallback (label + "  " + juce::String (juce::roundToInt (dragStartVal)) + "%");
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
        hudCallback (label + "  " + juce::String (juce::roundToInt (nextVal)) + "%");
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
        hudCallback (label + "  50%");
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
            hudCallback (label + "  " + juce::String (juce::roundToInt (nextVal)) + "%");
        repaint();
    }
}

//==============================================================================
// ImageToggleSwitch
//==============================================================================
MiAmpAudioProcessorEditor::ImageToggleSwitch::ImageToggleSwitch (
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    const juce::String& labelText,
    juce::Image upImg,
    juce::Image downImg,
    std::function<void(const juce::String&)> onHud)
    : apvts (state), pid (paramId), label (labelText),
      imgUp (std::move (upImg)), imgDown (std::move (downImg)), hudCallback (std::move (onHud))
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MiAmpAudioProcessorEditor::ImageToggleSwitch::setConfig (const juce::String& newId, const juce::String& newLabel)
{
    pid = newId;
    label = newLabel;
    repaint();
}

void MiAmpAudioProcessorEditor::ImageToggleSwitch::paint (juce::Graphics& g)
{
    auto* raw = apvts.getRawParameterValue (pid);
    const bool on = raw != nullptr && raw->load() > 0.5f;
    const auto& img = on ? imgUp : imgDown;
    if (img.isValid())
        g.drawImage (img, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);

    if (isMouseOver())
    {
        g.setColour (juce::Colour::fromRGBA (189, 238, 208, 170));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 6.0f, 1.5f);
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
                hudCallback (next ? "ULTRA-HI" : "ULTRA-LO");
            else if (pid == MiAmpParams::gtrModeBright)
                hudCallback (next ? "BRIGHT CAP ON" : "BRIGHT CAP OFF");
            else
                hudCallback (label + (next ? " ON" : " OFF"));
        }
        repaint();
    }
}

//==============================================================================
// PalettePedalButton (supports click OR drag directly onto pedalboard!)
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
        auto imgR = r.reduced (6.0f, 5.0f).withTrimmedBottom (16.0f);
        g.drawImage (art, imgR, juce::RectanglePlacement::centred);
    }

    g.setColour (inkColour().withAlpha (alreadyInChain ? 0.95f : 0.90f));
    g.setFont (juce::Font (juce::FontOptions (juce::jlimit (8.0f, 10.5f, r.getHeight() * 0.11f), juce::Font::bold)));
    g.drawText (name, r.withTop (r.getBottom() - 18.0f), juce::Justification::centred);
}

//==============================================================================
// GlassPopover
//==============================================================================
class MiAmpAudioProcessorEditor::GlassPopover final : public juce::Component
{
public:
    GlassPopover (MiAmpAudioProcessor& proc,
                  std::function<void()> onClose,
                  std::function<void()> onChanged)
        : processor (proc), closeFn (std::move (onClose)), changedFn (std::move (onChanged))
    {
        addAndMakeVisible (btnClose);
        btnClose.setButtonText ("x");
        btnClose.onClick = [this] { if (closeFn) closeFn(); };

        addChildComponent (presetNameEdit);
        presetNameEdit.setTextToShowWhenEmpty ("Nuevo preset...", juce::Colours::lightgrey);
        presetNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGBA (27, 39, 33, 185));
        presetNameEdit.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGBA (241, 224, 190, 102));
        presetNameEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);

        addChildComponent (btnSavePreset);
        btnSavePreset.setButtonText ("Guardar");
        btnSavePreset.onClick = [this]
        {
            processor.saveUserPreset (presetNameEdit.getText());
            presetNameEdit.clear();
            rebuildPresetList();
            if (changedFn) changedFn();
        };

        addChildComponent (presetViewport);
        presetViewport.setViewedComponent (&presetListHolder, false);
        presetViewport.setScrollBarsShown (true, false);

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

        addChildComponent (btnTunerMute);
        btnTunerMute.onClick = [this]
        {
            if (auto* p = processor.apvts.getParameter (MiAmpParams::tunerMuted))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
            syncTunerMute();
            if (changedFn) changedFn();
        };
    }

    void setKind (PopoverKind k, const xvdsp::TunerReading& tr)
    {
        kind = k;
        tuner = tr;
        const bool isP = (k == PopoverKind::Presets);
        const bool isD = (k == PopoverKind::Drop);
        const bool isT = (k == PopoverKind::Tuner);

        presetNameEdit.setVisible (isP);
        btnSavePreset.setVisible (isP);
        presetViewport.setVisible (isP);

        btnDropToggle.setVisible (isD);
        for (auto& c : dropChips) c.setVisible (isD);
        slSemi.setVisible (isD);
        slCents.setVisible (isD);
        slMix.setVisible (isD);

        btnTunerMute.setVisible (isT);

        if (isP) rebuildPresetList();
        if (isD) syncDropControls();
        if (isT) syncTunerMute();
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
        g.setFont (juce::Font (juce::FontOptions (15.5f, juce::Font::bold)));
        juce::String title = (kind == PopoverKind::Presets)
                                ? (processor.isBassMode() ? "PRESETS · BAJO" : "PRESETS · ZENBOX")
                           : (kind == PopoverKind::Drop) ? "DROP TUNER"
                                                         : "AFINADOR";
        g.drawText (title, inner.removeFromTop (24.0f), juce::Justification::centredLeft);

        if (kind == PopoverKind::Drop)
        {
            g.setFont (juce::Font (juce::FontOptions (12.0f)));
            g.setColour (inkColour().withAlpha (0.85f));
            const float x = inner.getX();
            const float w = inner.getWidth();
            const float y0 = inner.getY() + 104.0f;
            g.drawText ("Semitonos", juce::Rectangle<float> (x, y0, w, 16.0f), juce::Justification::centredLeft);
            g.drawText ("Cents",     juce::Rectangle<float> (x, y0 + 42.0f, w, 16.0f), juce::Justification::centredLeft);
            g.drawText ("Mix %",     juce::Rectangle<float> (x, y0 + 84.0f, w, 16.0f), juce::Justification::centredLeft);
        }
        else if (kind == PopoverKind::Tuner)
        {
            const bool active = tuner.hasSignal && tuner.freqHz > 20.0f;
            const bool inTune = active && std::fabs (tuner.cents) < 6.0f;
            const juce::String noteStr = active ? (juce::String (tuner.noteName) + juce::String (tuner.octave)) : "--";

            g.setFont (juce::Font (juce::FontOptions (52.0f, juce::Font::bold)));
            g.setColour (active ? (inTune ? mintBorder() : inkColour()) : inkColour().withAlpha (0.55f));
            g.drawText (noteStr, inner.removeFromTop (64.0f), juce::Justification::centred);

            g.setFont (juce::Font (juce::FontOptions (13.0f)));
            g.setColour (inkColour().withAlpha (0.85f));
            const juce::String hz = active
                ? (juce::String (tuner.freqHz, 1) + " Hz  (" + (tuner.cents >= 0 ? "+" : "") + juce::String (tuner.cents, 1) + " ct)")
                : "Esperando señal";
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

            auto row = inner.removeFromTop (32.0f).reduced (4.0f, 4.0f);
            const char* strsBass[4] = { "E1", "A1", "D2", "G2" };
            const char* strsGtr[6]  = { "E2", "A2", "D3", "G3", "B3", "E4" };
            const bool isBass = processor.isBassMode();
            const int nNotes = isBass ? 4 : 6;
            const float cw = row.getWidth() / static_cast<float> (nNotes);
            for (int i = 0; i < nNotes; ++i)
            {
                const char* txt = isBass ? strsBass[i] : strsGtr[i];
                auto cell = juce::Rectangle<float> (row.getX() + i * cw, row.getY(), cw, row.getHeight()).reduced (2.0f, 0.0f);
                const bool hit = active && (juce::String (tuner.noteName) + juce::String (tuner.octave)) == txt;
                g.setColour (hit ? juce::Colour::fromRGBA (141, 224, 184, 90) : juce::Colour::fromRGBA (255, 255, 255, 22));
                g.fillRoundedRectangle (cell, 6.0f);
                g.setColour (hit ? mintBorder() : juce::Colour::fromRGBA (255, 244, 212, 60));
                g.drawRoundedRectangle (cell, 6.0f, 1.0f);
                g.setColour (hit ? juce::Colours::white : inkColour().withAlpha (0.85f));
                g.setFont (juce::Font (juce::FontOptions (isBass ? 11.5f : 10.5f, juce::Font::bold)));
                g.drawText (txt, cell, juce::Justification::centred);
            }
        }
    }

    void resized() override
    {
        btnClose.setBounds (getWidth() - 38, 12, 26, 26);
        auto inner = getLocalBounds().reduced (16, 46);

        if (kind == PopoverKind::Presets)
        {
            auto saveRow = inner.removeFromBottom (34);
            btnSavePreset.setBounds (saveRow.removeFromRight (78));
            saveRow.removeFromRight (6);
            presetNameEdit.setBounds (saveRow);
            inner.removeFromBottom (8);
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
        else if (kind == PopoverKind::Tuner)
        {
            btnTunerMute.setBounds (inner.getX(), getHeight() - 48, inner.getWidth(), 34);
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

    void syncTunerMute()
    {
        const bool muted = processor.apvts.getRawParameterValue (MiAmpParams::tunerMuted)->load() > 0.5f;
        btnTunerMute.setToggleState (muted, juce::dontSendNotification);
        btnTunerMute.setButtonText (muted ? "MUTE AL AFINAR: ON" : "MUTE AL AFINAR: OFF");
    }

    void rebuildPresetList()
    {
        presetRows.clear();
        presetListHolder.removeAllChildren();

        auto addRow = [this] (const juce::String& text, bool isActive,
                              std::function<void()> onLoad, std::function<void()> onDel)
        {
            auto row = std::make_unique<PresetRow> (text, isActive, std::move (onLoad), std::move (onDel));
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
                addRow (up.name, up.name == activeName,
                    [this, i] { processor.loadUserPreset (i); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    [this, i] { processor.deleteUserPreset (i); rebuildPresetList(); });
            }
            for (const auto& fp : MiAmpAudioProcessor::getFactoryPresets())
            {
                addRow (fp.name, fp.name == activeName,
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
                addRow (up.name, up.name == activeName,
                    [this, i] { processor.loadUserPreset (i); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    [this, i] { processor.deleteUserPreset (i); rebuildPresetList(); });
            }
            for (const auto& fp : MiAmpAudioProcessor::getGuitarFactoryPresets())
            {
                addRow (fp.name, fp.name == activeName,
                    [this, fp] { processor.applyGuitarPreset (fp); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                    nullptr);
            }
        }

        layoutPresetButtons();
    }

    void layoutPresetButtons()
    {
        const int vw = juce::jmax (160, presetViewport.getWidth() - 12);
        const int rowH = 36;
        int y = 0;
        for (auto& r : presetRows)
        {
            r->setBounds (0, y, vw, rowH - 4);
            y += rowH;
        }
        presetListHolder.setSize (vw, juce::jmax (10, y));
    }

    struct PresetRow final : public juce::Component
    {
        PresetRow (const juce::String& title, bool active,
                   std::function<void()> loadCb, std::function<void()> delCb)
        {
            addAndMakeVisible (btnLoad);
            btnLoad.setButtonText (title);
            btnLoad.setToggleState (active, juce::dontSendNotification);
            btnLoad.onClick = std::move (loadCb);
            if (delCb)
            {
                addAndMakeVisible (btnDel);
                btnDel.setButtonText ("x");
                btnDel.onClick = std::move (delCb);
            }
        }
        void resized() override
        {
            auto b = getLocalBounds();
            if (btnDel.isVisible())
            {
                btnDel.setBounds (b.removeFromRight (32));
                b.removeFromRight (4);
            }
            btnLoad.setBounds (b);
        }
        juce::TextButton btnLoad, btnDel;
    };

    MiAmpAudioProcessor& processor;
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

    juce::TextButton btnTunerMute;
};

//==============================================================================
// MiAmpAudioProcessorEditor
//==============================================================================
MiAmpAudioProcessorEditor::MiAmpAudioProcessorEditor (MiAmpAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    setLookAndFeel (&glassLaf);
    loadImages();

    for (auto* b : { &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainPedals,
                     &btnDockCab, &btnDockDrop, &btnDockCh, &btnDockOs, &btnDockGate,
                     &btnEditClose, &btnMoveLeft, &btnMoveRight, &btnRemovePedal,
                     &btnFloorPresets, &btnFloorTuner, &btnFloorCab, &btnFloorDrop, &btnBackToAmp, &btnFloorTunerMute })
    {
        b->setLookAndFeel (&glassLaf);
    }

    // Instrument switch (.instrument-switch)
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

    // Top-left dock (.audio-dock)
    addAndMakeVisible (btnDockCab);
    btnDockCab.onClick = [this]
    {
        if (audioProcessor.isBassMode())
        {
            if (auto* cab = audioProcessor.apvts.getParameter (MiAmpParams::ampCabOn))
            {
                const bool nxt = cab->getValue() < 0.5f;
                cab->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
                showHud (nxt ? "CAJA 8x10 ON" : "CAJA 8x10 OFF");
            }
        }
        else
        {
            const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrCab)->load()));
            const int nxt = (cur + 1) % 5;
            if (auto* cab = audioProcessor.apvts.getParameter (MiAmpParams::gtrCab))
                cab->setValueNotifyingHost (cab->convertTo0to1 (static_cast<float> (nxt)));
            const char* names[5] = { "CAB: VINT 4x12", "CAB: MOD 4x12", "CAB: OPEN 2x12", "CAB: STUD 1x12", "CAB: OFF" };
            showHud (names[nxt]);
        }
    };

    addAndMakeVisible (btnDockCh);
    btnDockCh.onClick = [this]
    {
        const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrChannel)->load()));
        const int nxt = (cur + 1) % 3;
        if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::gtrChannel))
            p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (nxt)));
        const char* names[3] = { "CH1 CLEAN/EDGE", "CH2 CRUNCH 70s", "CH3 PUNK/HIGH" };
        showHud (names[nxt]);
    };

    addAndMakeVisible (btnDockOs);
    btnDockOs.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::gtrOs4x))
        {
            const bool nxt = p->getValue() < 0.5f;
            p->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
            showHud (nxt ? "4x OVERSAMPLING ON" : "1x OVERSAMPLING");
        }
    };

    addAndMakeVisible (btnDockGate);
    btnDockGate.onClick = [this]
    {
        const float cur = audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrGate)->load();
        const float nxt = (cur < 5.0f) ? 18.0f : (cur < 28.0f ? 35.0f : (cur < 48.0f ? 55.0f : 0.0f));
        if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::gtrGate))
            p->setValueNotifyingHost (p->convertTo0to1 (nxt));
        showHud (nxt < 1.0f ? "GATE OFF" : ("GATE " + juce::String (juce::roundToInt (nxt)) + "%"));
    };

    addAndMakeVisible (btnDockDrop);
    btnDockDrop.onClick = [this] { togglePopover (PopoverKind::Drop); };

    // Bass Head Knobs & Switches
    for (int i = 0; i < 5; ++i)
    {
        bassHeadKnobs[i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, kBassHeadKnobIds[i], kBassHeadKnobNames[i],
            imgs.bassKnobCap, true, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*bassHeadKnobs[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        bassHeadSwitches[i] = std::make_unique<ImageToggleSwitch> (
            audioProcessor.apvts, kBassSwitchIds[i], kBassSwitchNames[i],
            imgs.toggleUp, imgs.toggleDown, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*bassHeadSwitches[i]);
    }

    // Guitar Head Knobs (19 total: 9 top row CH1..3 Gain/Treble/Mid, 9 bottom row Bass/Presence/Master, 1 Output)
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
            imgs.bassKnobCap, true, [this] (const juce::String& t) { showHud (t); }, switchChFn);
        addChildComponent (*guitarHeadKnobs[i]);

        guitarHeadKnobs[9 + i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, botId,
            "CH" + juce::String (ch + 1) + " " + botNames[col],
            imgs.bassKnobCap, true, [this] (const juce::String& t) { showHud (t); }, switchChFn);
        addChildComponent (*guitarHeadKnobs[9 + i]);
    }
    guitarHeadKnobs[18] = std::make_unique<ImageRotaryKnob> (
        audioProcessor.apvts, MiAmpParams::gtrOutput, "OUTPUT",
        imgs.bassKnobCap, true, [this] (const juce::String& t) { showHud (t); });
    addChildComponent (*guitarHeadKnobs[18]);

    for (int i = 0; i < 3; ++i)
    {
        guitarHeadSwitches[i] = std::make_unique<ImageToggleSwitch> (
            audioProcessor.apvts, kGtrSwitchIds[i], kGtrSwitchNames[i],
            imgs.toggleUp, imgs.toggleDown, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*guitarHeadSwitches[i]);
    }

    // Floor editor controls
    for (int i = 0; i < 7; ++i)
    {
        editorKnobs[i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, kXvKnobIds[i], kXvKnobNames[i],
            imgs.xvKnobPtr, false, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*editorKnobs[i]);
    }

    addChildComponent (btnEditClose);
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
    addChildComponent (btnRemovePedal);
    btnRemovePedal.onClick = [this]
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

    // Floor palette items (up to 7 slots, dynamically mapped per instrument)
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
        syncPedalEditorControls();
        layoutAll();
        repaint();
    };
    addChildComponent (btnFloorCab);
    btnFloorCab.onClick = btnDockCab.onClick;
    addChildComponent (btnFloorDrop);
    btnFloorDrop.onClick = [this] { togglePopover (PopoverKind::Drop); };
    addChildComponent (btnBackToAmp);
    btnBackToAmp.onClick = [this] { setFloorMode (false); };

    addChildComponent (btnFloorTunerMute);
    btnFloorTunerMute.onClick = [this]
    {
        if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::tunerMuted))
            p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
        repaint();
    };

    popover = std::make_unique<GlassPopover> (
        audioProcessor,
        [this] { togglePopover (PopoverKind::None); },
        [this] { syncHeadControls(); syncPedalEditorControls(); repaint(); });
    popover->setLookAndFeel (&glassLaf);
    addChildComponent (*popover);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio (kStageW / kStageH);

    setResizable (true, true);
    setResizeLimits (775, 384, 2325, 1152);
    setSize (1240, 614);

    setFloorMode (false);
    startTimerHz (30);
}

MiAmpAudioProcessorEditor::~MiAmpAudioProcessorEditor()
{
    stopTimer();
    for (auto* b : { &btnInstGuitar, &btnInstBass, &btnMainPresets, &btnMainTuner, &btnMainPedals,
                     &btnDockCab, &btnDockDrop, &btnDockCh, &btnDockOs, &btnDockGate,
                     &btnEditClose, &btnMoveLeft, &btnMoveRight, &btnRemovePedal,
                     &btnFloorPresets, &btnFloorTuner, &btnFloorCab, &btnFloorDrop, &btnBackToAmp, &btnFloorTunerMute })
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
    hudTicks = 55;
    repaint();
}

void MiAmpAudioProcessorEditor::switchInstrument (bool toBass)
{
    if (audioProcessor.isBassMode() == toBass)
        return;

    audioProcessor.setInstrumentMode (toBass);
    activePopover = PopoverKind::None;
    popover->setVisible (false);
    showHud (toBass ? "MODO BAJO · XV EDITION" : "MODO GUITARRA · ZENBOX");
    syncHeadControls();
    syncPedalEditorControls();
    layoutAll();
    repaint();
}

void MiAmpAudioProcessorEditor::setFloorMode (bool floor)
{
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

    btnInstGuitar.setToggleState (! bass, juce::dontSendNotification);
    btnInstBass.setToggleState   (bass,   juce::dontSendNotification);

    for (auto* b : { &btnInstGuitar, &btnInstBass,
                     &btnMainPresets, &btnMainTuner, &btnMainPedals, &btnDockCab, &btnDockDrop })
        b->setVisible (! isFloorMode);

    btnDockCh.setVisible   (! isFloorMode && ! bass);
    btnDockOs.setVisible   (! isFloorMode && ! bass);
    btnDockGate.setVisible (! isFloorMode && ! bass);

    for (auto& k : bassHeadKnobs)      k->setVisible (! isFloorMode && bass);
    for (auto& s : bassHeadSwitches)   s->setVisible (! isFloorMode && bass);
    for (auto& k : guitarHeadKnobs)    k->setVisible (! isFloorMode && ! bass);
    for (auto& s : guitarHeadSwitches) s->setVisible (! isFloorMode && ! bass);

    btnFloorPresets.setVisible (isFloorMode);
    btnFloorTuner.setVisible   (isFloorMode);
    btnFloorCab.setVisible     (isFloorMode);
    btnFloorDrop.setVisible    (isFloorMode);
    btnBackToAmp.setVisible    (isFloorMode);
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
    btnRemovePedal.setVisible (showEd);

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
                editorOpen = true;
                syncPedalEditorControls();
                repaint();
            },
            [this, bass, pt] (juce::Point<float> editorPt)
            {
                draggingPedalActive = true;
                draggingFromPalette = true;
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
                        editorOpen = true;
                    }
                }
                draggingPedalActive = false;
                draggingFromPalette = false;
                draggingPedalType   = -1;
                dragInsertIndex     = -1;
                syncPedalEditorControls();
                repaint();
            });
        paletteItems[idx]->setInChain (inChain);
        paletteItems[idx]->setVisible (isFloorMode && ! floorTunerOpen);
    }

    btnFloorTunerMute.setVisible (isFloorMode && floorTunerOpen);

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
    tunerReading = audioProcessor.tuner.analyze();
    if (activePopover == PopoverKind::Tuner && popover != nullptr)
        popover->updateTuner (tunerReading);

    if (hudTicks > 0 && --hudTicks == 0)
        hudText.clear();

    const bool bass = audioProcessor.isBassMode();
    if (bass)
    {
        const bool cabOn = audioProcessor.apvts.getRawParameterValue (MiAmpParams::ampCabOn)->load() > 0.5f;
        btnDockCab.setButtonText ("CAB 8x10");
        btnDockCab.setToggleState (cabOn, juce::dontSendNotification);
        btnFloorCab.setButtonText ("CAB 8x10");
        btnFloorCab.setToggleState (cabOn, juce::dontSendNotification);
    }
    else
    {
        const int cab = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrCab)->load()));
        const char* cabLabels[5] = { "VINT 4x12", "MOD 4x12", "OPEN 2x12", "STUD 1x12", "CAB OFF" };
        btnDockCab.setButtonText (cabLabels[cab]);
        btnDockCab.setToggleState (cab < 4, juce::dontSendNotification);
        btnFloorCab.setButtonText (cabLabels[cab]);
        btnFloorCab.setToggleState (cab < 4, juce::dontSendNotification);

        const int ch = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrChannel)->load()));
        btnDockCh.setButtonText ("CH" + juce::String (ch + 1));
        btnDockCh.setToggleState (true, juce::dontSendNotification);

        const bool os4x = audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrOs4x)->load() > 0.5f;
        btnDockOs.setToggleState (os4x, juce::dontSendNotification);

        const int gate = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrGate)->load()));
        btnDockGate.setButtonText (gate <= 0 ? "GATE OFF" : ("GATE " + juce::String (gate)));
        btnDockGate.setToggleState (gate > 0, juce::dontSendNotification);
    }

    const bool dropOn = audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchOn)->load() > 0.5f;
    const int semi = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchSemi)->load()));
    const juce::String dropTxt = dropOn ? ("DROP " + juce::String (semi > 0 ? "+" : "") + juce::String (semi)) : "DROP";
    btnDockDrop.setButtonText (dropTxt);
    btnDockDrop.setToggleState (dropOn || activePopover == PopoverKind::Drop, juce::dontSendNotification);
    btnFloorDrop.setButtonText (dropOn ? (juce::String (semi > 0 ? "+" : "") + juce::String (semi)) : "DROP");
    btnFloorDrop.setToggleState (dropOn || activePopover == PopoverKind::Drop, juce::dontSendNotification);

    btnMainPresets.setToggleState (activePopover == PopoverKind::Presets, juce::dontSendNotification);
    btnMainTuner.setToggleState   (activePopover == PopoverKind::Tuner,   juce::dontSendNotification);
    btnFloorPresets.setToggleState (activePopover == PopoverKind::Presets, juce::dontSendNotification);
    btnFloorTuner.setToggleState  (floorTunerOpen, juce::dontSendNotification);

    const bool muted = audioProcessor.apvts.getRawParameterValue (MiAmpParams::tunerMuted)->load() > 0.5f;
    btnFloorTunerMute.setToggleState (muted, juce::dontSendNotification);
    btnFloorTunerMute.setButtonText (muted ? "MUTE: ON" : "MUTE: OFF");

    repaint();
}

void MiAmpAudioProcessorEditor::resized()
{
    layoutAll();
}

juce::Rectangle<float> MiAmpAudioProcessorEditor::computeEditorPedalStageRect() const
{
    const bool bass = audioProcessor.isBassMode();
    if (bass && selectedBassPedal == 1)
    {
        const float w = 365.0f;
        const float h = w * (1131.0f / 1391.0f);
        return { 35.0f + (420.0f - w) * 0.5f, 28.0f + 58.0f, w, h };
    }
    const float w = 266.0f;
    const float h = 380.0f;
    return { 35.0f + (420.0f - w) * 0.5f, 28.0f + 46.0f, w, h };
}

std::vector<juce::Rectangle<float>> MiAmpAudioProcessorEditor::computeBoardSlotRects (bool bassMode, const std::vector<int>& chain) const
{
    std::vector<juce::Rectangle<float>> rects;
    const int count = static_cast<int> (chain.size());
    if (count <= 0) return rects;

    const float offsetX = editorOpen ? 0.0f : -260.0f;
    const float vx = 621.0f + offsetX;
    const float vy = 153.0f;
    const float vw = 830.0f;
    const float vh = 435.0f;

    const float gap = (count >= 6) ? 11.0f : (count >= 5 ? 14.0f : 26.0f);
    float totalW = 0.0f;
    for (int i = 0; i < count; ++i)
        totalW += (bassMode && chain[static_cast<size_t> (i)] == 1) ? 210.0f : 124.0f;
    totalW += gap * static_cast<float> (juce::jmax (0, count - 1));

    const float fitScale = juce::jmin (1.0f, (vw - 24.0f) / juce::jmax (1.0f, totalW));
    float curX = vx + (vw - totalW * fitScale) * 0.5f;

    for (int i = 0; i < count; ++i)
    {
        const bool isXv = (bassMode && chain[static_cast<size_t> (i)] == 1);
        const float pw = (isXv ? 210.0f : 124.0f) * fitScale;
        const float ph = (isXv ? 171.0f : 207.0f) * fitScale;
        rects.emplace_back (curX, vy + (vh - ph) * 0.5f, pw, ph);
        curX += pw + gap * fitScale;
    }
    return rects;
}

int MiAmpAudioProcessorEditor::computeDropInsertIndex (bool bassMode, juce::Point<float> stagePt, int excludePedal) const
{
    const float offsetX = editorOpen ? 0.0f : -260.0f;
    const auto boardHitR = juce::Rectangle<float> (570.0f + offsetX, 85.0f, 950.0f, 520.0f);
    if (! boardHitR.contains (stagePt))
        return -1;

    auto chain = audioProcessor.getActiveChain (bassMode);
    if (chain.empty()) return 0;

    // Build list without excluded pedal for stable index calculation
    auto slots = computeBoardSlotRects (bassMode, chain);
    int idx = 0;
    for (size_t i = 0; i < chain.size(); ++i)
    {
        if (chain[i] == excludePedal) continue;
        if (stagePt.x > slots[i].getCentreX())
            ++idx;
    }
    return idx;
}

void MiAmpAudioProcessorEditor::layoutAll()
{
    const bool bass = audioProcessor.isBassMode();

    if (! isFloorMode)
    {
        if (bass)
        {
            // .audio-dock (35, 25, w=368, h=52)
            btnDockCab.setBounds  (toScreenInt (46, 33, 94, 36));
            btnDockDrop.setBounds (toScreenInt (308, 33, 84, 36));
        }
        else
        {
            // Expanded Zenbox dock (35, 25, w=638, h=52)
            btnDockCab.setBounds  (toScreenInt (44,  33, 102, 36));
            btnDockCh.setBounds   (toScreenInt (152, 33, 52,  36));
            btnDockOs.setBounds   (toScreenInt (210, 33, 66,  36));
            btnDockGate.setBounds (toScreenInt (282, 33, 80,  36));
            btnDockDrop.setBounds (toScreenInt (580, 33, 84,  36));
        }

        // .menu (right: 35, top: 395, width: 225) -> x = 1290
        btnInstGuitar.setBounds  (toScreenInt (1308, 415, 92, 35));
        btnInstBass.setBounds    (toScreenInt (1404, 415, 92, 35));
        btnMainPresets.setBounds (toScreenInt (1305, 464, 195, 41));
        btnMainTuner.setBounds   (toScreenInt (1305, 513, 195, 41));
        btnMainPedals.setBounds  (toScreenInt (1305, 562, 195, 41));

        if (bass)
        {
            const float knobDiam = 58.0f * kAmpZoom;
            for (int i = 0; i < 5; ++i)
            {
                const float xStage = kBassHeadKnobX[i] * (kStageW / 1376.0f);
                const auto pt = ampZoomPt (xStage, kBassHeadKnobY);
                bassHeadKnobs[i]->setBounds (toScreenInt (pt.x - knobDiam * 0.5f, pt.y - knobDiam * 0.5f, knobDiam, knobDiam));
            }

            const float swW = 28.0f * kAmpZoom;
            const float swH = 54.0f * kAmpZoom;
            for (int i = 0; i < 3; ++i)
            {
                const float xStage = kBassSwitchX[i] * (kStageW / 1376.0f);
                const auto pt = ampZoomPt (xStage, kBassSwitchY);
                bassHeadSwitches[i]->setBounds (toScreenInt (pt.x - swW * 0.5f, pt.y - swH * 0.5f, swW, swH));
            }
        }
        else
        {
            // Exact index.html formula for Guitar knobs (rad=19, 1.18x)
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
                const float swW = kGtrSwitchSize[i] * 0.92f * kAmpZoom;
                const float swH = kGtrSwitchSize[i] * 1.45f * kAmpZoom;
                guitarHeadSwitches[i]->setBounds (toScreenInt (pt.x - swW * 0.5f, pt.y - swH * 0.5f, swW, swH));
            }
        }

        if (popover->isVisible())
            popover->setBounds (toScreenInt (1215, 25, 300, 358));
    }
    else
    {
        // .edit controls (35, 28, 420, 577)
        btnEditClose.setBounds   (toScreenInt (412, 38, 30, 30));
        btnMoveLeft.setBounds    (toScreenInt (57,  554, 38, 34));
        btnMoveRight.setBounds   (toScreenInt (148, 554, 38, 34));
        btnRemovePedal.setBounds (toScreenInt (358, 554, 78, 34));

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

        // .palette (36, 623, 1478, 125)
        if (bass)
        {
            float px = 268.0f;
            for (int idx = 0; idx < 5; ++idx)
            {
                const float iw = (idx == 4) ? 145.0f : 104.0f;
                paletteItems[idx]->setBounds (toScreenInt (px, 635, iw, 100));
                px += iw + 16.0f;
            }
        }
        else
        {
            float px = 258.0f;
            for (int idx = 0; idx < 7; ++idx)
            {
                const float iw = 122.0f;
                paletteItems[idx]->setBounds (toScreenInt (px, 635, iw, 100));
                px += iw + 16.0f;
            }
        }

        btnFloorTunerMute.setBounds (toScreenInt (1055, 666, 155, 39));

        btnFloorPresets.setBounds (toScreenInt (1246, 630, 122, 34));
        btnFloorTuner.setBounds   (toScreenInt (1374, 630, 122, 34));
        btnFloorCab.setBounds     (toScreenInt (1246, 668, 122, 34));
        btnFloorDrop.setBounds    (toScreenInt (1374, 668, 122, 34));
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
            const float rad = juce::degreesToRadians (-135.0f + juce::jlimit (0.0f, 1.0f, val * 0.01f) * 270.0f);
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
        const float rad = juce::degreesToRadians (-135.0f + juce::jlimit (0.0f, 1.0f, val * 0.01f) * 270.0f);
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

void MiAmpAudioProcessorEditor::paintHeadScene (juce::Graphics& g)
{
    const bool bass = audioProcessor.isBassMode();
    const auto bgStage = juce::Rectangle<float> (
        kAmpOriginX - kAmpOriginX * kAmpZoom,
        kAmpOriginY - kAmpOriginY * kAmpZoom,
        kStageW * kAmpZoom,
        kStageH * kAmpZoom);

    const auto& bgImg = bass ? imgs.bassBg : imgs.guitarBg;
    if (bgImg.isValid())
        g.drawImage (bgImg, toScreen (bgStage.getX(), bgStage.getY(), bgStage.getWidth(), bgStage.getHeight()),
                     juce::RectanglePlacement::stretchToFit);

    const bool pwr = audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::ampPower : MiAmpParams::gtrPower)->load() > 0.5f;
    const bool standby = audioProcessor.apvts.getRawParameterValue (bass ? MiAmpParams::ampStandby : MiAmpParams::gtrStandby)->load() > 0.5f;

    if (bass && ! pwr)
    {
        const auto pt = ampZoomPt (967.0f, 354.0f);
        const float d = 14.0f * kAmpZoom;
        g.setColour (juce::Colour::fromRGB (35, 10, 10));
        g.fillEllipse (toScreen (pt.x, pt.y, d, d));
    }

    // Active channel indicator pill on Guitar head (CH1 / CH2 / CH3 subtle glow)
    const float s = stageScale();
    if (! bass)
    {
        const int ch = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::gtrChannel)->load()));
        const float chCenterXs[3] = { 445.0f, 653.0f, 838.0f };
        const float cx = (183.0f + (chCenterXs[ch] - 180.0f) * kGtrScX) * (kStageW / 1376.0f);
        const float cy = 99.0f + (342.0f - 50.0f) * kGtrScY;
        const auto pt = ampZoomPt (cx, cy);
        g.setColour (juce::Colour::fromRGBA (142, 240, 194, 210));
        g.fillEllipse (toScreen (pt.x - 4.5f, pt.y - 4.5f, 9.0f, 9.0f));
    }

    // Top-left .audio-dock
    const float dockW = bass ? 368.0f : 638.0f;
    const float meterX0 = bass ? 148.0f : 374.0f;
    paintGlassPanel (g, toScreen (35, 25, dockW, 52), 15.0f);

    const bool clip = audioProcessor.isClipping.load();
    const juce::Colour ledCol = clip ? juce::Colour::fromRGB (255, 94, 94)
                                     : (! pwr || standby ? juce::Colour::fromRGB (247, 197, 107)
                                                         : juce::Colour::fromRGB (126, 255, 189));
    g.setColour (ledCol);
    g.fillEllipse (toScreen (meterX0, 46, 10, 10));

    const float inFrac  = juce::jlimit (0.0f, 1.0f, audioProcessor.inputRms.load() * 3.5f);
    const float outFrac = juce::jlimit (0.0f, 1.0f, audioProcessor.outputRms.load() * 3.5f);
    g.setColour (inkColour().withAlpha (0.78f));
    g.setFont (juce::Font (juce::FontOptions (juce::jmax (8.0f, 9.5f * s), juce::Font::bold)));
    g.drawText ("IN",  toScreen (meterX0 + 22.0f, 36, 24, 12), juce::Justification::centredLeft);
    g.drawText ("OUT", toScreen (meterX0 + 22.0f, 52, 26, 12), juce::Justification::centredLeft);

    auto drawMeter = [&] (float y, float frac)
    {
        auto tr = toScreen (meterX0 + 50.0f, y, 96, 6);
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

    // Right .menu (1290, 395, 225, 226)
    paintGlassPanel (g, toScreen (1290, 395, 225, 226), 22.0f);
    auto swBox = toScreen (1305, 412, 195, 41);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 24));
    g.fillRoundedRectangle (swBox, 8.0f * s);
    g.setColour (juce::Colour::fromRGBA (241, 224, 190, 102));
    g.drawRoundedRectangle (swBox, 8.0f * s, 1.0f);

    if (hudText.isNotEmpty())
    {
        auto hudR = toScreen (35, 92, 240, 36);
        paintGlassPanel (g, hudR, 11.0f);
        g.setColour (inkColour());
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, 13.0f * s), juce::Font::bold)));
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

    const float boardOffX = showEd ? 0.0f : -260.0f;
    const auto boardR = toScreen (585.79f + boardOffX, 95.73f, 921.48f, 499.54f);
    if (imgs.boardSurface.isValid())
        g.drawImage (imgs.boardSurface, boardR, juce::RectanglePlacement::stretchToFit);

    const auto slots = computeBoardSlotRects (bass, chain);

    if (chain.empty())
    {
        g.setColour (juce::Colour::fromRGBA (242, 232, 212, 180));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (13.0f, 20.0f * s), juce::Font::bold)));
        g.drawText ("TU PEDALBOARD ESTÁ VACÍA — Hacé clic o arrastrá abajo para agregar",
                    boardR, juce::Justification::centred);
    }
    else
    {
        for (size_t i = 0; i < chain.size(); ++i)
        {
            const int pt = chain[i];
            const bool isBeingDragged = draggingPedalActive && ! draggingFromPalette && (pt == draggingPedalType);
            const auto sr = toScreen (slots[i].getX(), slots[i].getY(), slots[i].getWidth(), slots[i].getHeight());
            const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, pt))->load() > 0.5f;
            const auto& pImg = pedalImage (bass, pt, active);

            if (isBeingDragged)
                g.setOpacity (0.28f);

            if (pImg.isValid())
                g.drawImage (pImg, sr, juce::RectanglePlacement::stretchToFit);

            paintMiniPedalKnobs (g, bass, pt, sr);
            g.setOpacity (1.0f);

            if (pt == sel && ! isBeingDragged)
            {
                g.setColour (juce::Colour::fromRGB (255, 223, 152));
                g.drawRoundedRectangle (sr.expanded (3.0f * s), 8.0f * s, 2.0f * s);
            }
        }
    }

    // Drag & Drop live insertion line + floating pedal ghost
    if (draggingPedalActive && draggingPedalType >= 0)
    {
        if (dragInsertIndex >= 0)
        {
            float lineXStage = boardR.getCentreX() / s;
            float lineYStage = 220.0f;
            float lineHStage = 280.0f;

            if (! slots.empty())
            {
                if (dragInsertIndex == 0)
                    lineXStage = slots.front().getX() - 10.0f;
                else if (dragInsertIndex >= static_cast<int> (slots.size()))
                    lineXStage = slots.back().getRight() + 10.0f;
                else
                    lineXStage = 0.5f * (slots[static_cast<size_t> (dragInsertIndex - 1)].getRight()
                                         + slots[static_cast<size_t> (dragInsertIndex)].getX());
            }

            const auto lr = toScreen (lineXStage - 3.0f, lineYStage, 6.0f, lineHStage);
            g.setColour (mintBorder().withAlpha (0.95f));
            g.fillRoundedRectangle (lr, 3.0f * s);
        }

        const bool isXv = (bass && draggingPedalType == 1);
        const float gw = isXv ? 175.0f : 110.0f;
        const float gh = isXv ? 142.0f : 182.0f;
        const auto ghostR = toScreen (dragStagePos.x - gw * 0.5f, dragStagePos.y - gh * 0.5f, gw, gh);
        const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (bass, draggingPedalType))->load() > 0.5f;
        const auto& gImg = pedalImage (bass, draggingPedalType, active);
        if (gImg.isValid())
        {
            g.setOpacity (0.85f);
            g.drawImage (gImg, ghostR, juce::RectanglePlacement::stretchToFit);
            g.setOpacity (1.0f);
        }
    }

    // Left .edit glass card (35, 28, 420, 577)
    if (showEd)
    {
        paintGlassPanel (g, toScreen (35, 28, 420, 577), 22.0f);
        g.setColour (inkColour());
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (14.0f, 19.5f * s), juce::Font::bold)));
        g.drawText (pedalName (bass, sel).toUpperCase(),
                    toScreen (60, 42, 320, 28), juce::Justification::centredLeft);

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

        auto it = std::find (chain.begin(), chain.end(), sel);
        const int posIdx = (it != chain.end()) ? static_cast<int> (std::distance (chain.begin(), it)) + 1 : 1;
        g.setColour (inkColour().withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, 12.0f * s), juce::Font::plain)));
        g.drawText (juce::String (posIdx) + "/" + juce::String (chain.size()),
                    toScreen (96, 554, 52, 34), juce::Justification::centred);
        g.drawText ("Clic switch: bypass · Arrastrá pedales en la board",
                    toScreen (56, 514, 378, 28), juce::Justification::centred);
    }

    // Bottom .palette glass bar (36, 623, 1478, 125)
    paintGlassPanel (g, toScreen (36, 623, 1478, 125), 22.0f);
    if (! floorTunerOpen)
    {
        g.setColour (inkColour());
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, 13.5f * s), juce::Font::bold)));
        g.drawText ("AGREGAR PEDALES", toScreen (56, 650, 195, 24), juce::Justification::centredLeft);
        g.setColour (inkColour().withAlpha (0.82f));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.0f, 11.5f * s), juce::Font::plain)));
        g.drawText ("Clic o arrastrá hacia la board", toScreen (56, 676, 195, 34), juce::Justification::topLeft);
    }
    else
    {
        const bool active = tunerReading.hasSignal && tunerReading.freqHz > 20.0f;
        const bool inTune = active && std::fabs (tunerReading.cents) < 6.0f;
        const juce::String noteStr = active ? (juce::String (tunerReading.noteName) + juce::String (tunerReading.octave)) : "--";

        auto noteBox = toScreen (56, 637, 130, 96);
        g.setColour (juce::Colour::fromRGBA (15, 24, 22, 130));
        g.fillRoundedRectangle (noteBox, 12.0f * s);
        g.setColour (active ? (inTune ? mintBorder() : inkColour()) : inkColour().withAlpha (0.6f));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (24.0f, 44.0f * s), juce::Font::bold)));
        g.drawText (noteStr, noteBox, juce::Justification::centred);

        g.setColour (inkColour());
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, 13.0f * s), juce::Font::bold)));
        g.drawText ("AFINADOR YIN", toScreen (210, 640, 220, 20), juce::Justification::centredLeft);
        const juce::String hzStr = active
            ? (juce::String (tunerReading.freqHz, 1) + " Hz  (" + (tunerReading.cents >= 0 ? "+" : "") + juce::String (tunerReading.cents, 1) + " ct)")
            : (bass ? "Tocá una cuerda de tu bajo..." : "Tocá una cuerda de tu guitarra...");
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, 13.0f * s))));
        g.drawText (hzStr, toScreen (650, 640, 360, 20), juce::Justification::centredRight);

        auto meter = toScreen (225, 678, 770, 14);
        juce::ColourGradient mg (juce::Colour::fromRGB (183, 115, 85), meter.getX(), meter.getY(),
                                 juce::Colour::fromRGB (183, 115, 85), meter.getRight(), meter.getY(), false);
        mg.addColour (0.25, juce::Colour::fromRGB (208, 186, 128));
        mg.addColour (0.50, juce::Colour::fromRGB (154, 215, 180));
        mg.addColour (0.75, juce::Colour::fromRGB (208, 186, 128));
        g.setGradientFill (mg);
        g.fillRoundedRectangle (meter, 7.0f * s);
        g.setColour (juce::Colours::white);
        g.fillRect (meter.getCentreX() - 1.5f, meter.getY() - 4.0f, 3.0f, meter.getHeight() + 8.0f);

        const float normCents = active ? juce::jlimit (-1.0f, 1.0f, tunerReading.cents / 50.0f) : 0.0f;
        const float nx = meter.getCentreX() + normCents * (meter.getWidth() * 0.48f);
        g.setColour (juce::Colour::fromRGB (23, 39, 34));
        g.fillRect (nx - 2.5f, meter.getY() - 5.0f, 5.0f, meter.getHeight() + 10.0f);
    }

    if (hudText.isNotEmpty())
    {
        auto hudR = toScreen (475, 28, 240, 36);
        paintGlassPanel (g, hudR, 11.0f);
        g.setColour (inkColour());
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, 13.0f * s), juce::Font::bold)));
        g.drawText (hudText, hudR, juce::Justification::centred);
    }
}

void MiAmpAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (17, 27, 28));
    if (! isFloorMode)
        paintHeadScene (g);
    else
        paintFloorScene (g);
}

void MiAmpAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    const float s = stageScale();
    const auto stagePt = juce::Point<float> (e.position.x / s, e.position.y / s);
    const bool bass = audioProcessor.isBassMode();

    if (! isFloorMode)
    {
        const float meterX0 = bass ? 140.0f : 366.0f;
        if (juce::Rectangle<float> (meterX0, 25, 165, 52).contains (stagePt))
            audioProcessor.isClipping.store (false, std::memory_order_relaxed);
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
                showHud (juce::String ("XV CAB ") + names[nxt]);
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
                showHud (pedalName (bass, sel) + (nxt ? " ON" : " BYPASS"));
                repaint();
                return;
            }
        }
    }

    const auto slots = computeBoardSlotRects (bass, chain);
    for (size_t i = 0; i < chain.size(); ++i)
    {
        if (slots[i].contains (stagePt))
        {
            dragStartPx = e.getPosition();
            dragStagePos = stagePt;
            draggingPedalType = chain[i];
            draggingFromPalette = false;
            draggingPedalActive = false;
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

    if (! draggingPedalActive && e.getPosition().getDistanceFrom (dragStartPx) > 6)
        draggingPedalActive = true;

    if (draggingPedalActive)
    {
        const bool bass = audioProcessor.isBassMode();
        dragStagePos = editorPtToStage (e.position);
        dragInsertIndex = computeDropInsertIndex (bass, dragStagePos, draggingPedalType);
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
        if (dragInsertIndex >= 0)
        {
            audioProcessor.reorderPedalToIndex (bass, draggingPedalType, dragInsertIndex);
            sel = draggingPedalType;
            editorOpen = true;
            showHud (pedalName (bass, draggingPedalType) + " MOVIDO");
        }
    }
    else
    {
        sel = draggingPedalType;
        editorOpen = true;
    }

    draggingPedalActive = false;
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
        if (slots[i].contains (stagePt))
        {
            if (auto* p = audioProcessor.apvts.getParameter (pedalBypassParamId (bass, chain[i])))
            {
                const bool nxt = p->getValue() < 0.5f;
                p->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
                showHud (pedalName (bass, chain[i]) + (nxt ? " ON" : " BYPASS"));
                repaint();
                return;
            }
        }
    }
}
