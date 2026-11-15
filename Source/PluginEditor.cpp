#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <BinaryData.h>

namespace
{
    constexpr float kHeadW = 1376.0f;
    constexpr float kHeadH = 768.0f;
    constexpr float kBarH  = 50.0f;

    constexpr float kHeadKnobX[5] = { 398.0f, 483.0f, 568.0f, 653.0f, 738.0f };
    constexpr float kHeadKnobY    = 410.0f;
    constexpr const char* kHeadKnobIds[5] = {
        MiAmpParams::ampGain, MiAmpParams::ampBass, MiAmpParams::ampMid,
        MiAmpParams::ampFreq, MiAmpParams::ampTreble
    };
    constexpr const char* kHeadKnobNames[5] = { "GAIN", "BASS", "MID", "FREQ", "TREBLE" };

    constexpr float kSwitchX[3] = { 258.0f, 323.0f, 822.0f };
    constexpr float kSwitchY    = 414.0f;
    constexpr const char* kSwitchIds[3] = {
        MiAmpParams::ampPower, MiAmpParams::ampUltraHi, MiAmpParams::ampStandby
    };
    constexpr const char* kSwitchNames[3] = { "POWER", "ULTRA", "STANDBY" };

    // XV Bass (1391x1131) normalized coords matching Mi-Amp/index.html
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

    // 700x1000 pedal knob coordinates
    constexpr float kCompPoints[3][2]   = { { 0.244975f, 0.218473f }, { 0.50098f, 0.218473f }, { 0.756985f, 0.218473f } };
    constexpr float kChorusPoints[3][2] = { { 0.20807f,  0.291823f }, { 0.498418f, 0.291823f }, { 0.788766f, 0.291823f } };
    constexpr float kPhaserPoints[3][2] = { { 0.239171f, 0.220286f }, { 0.500f,    0.220286f }, { 0.762712f, 0.220286f } };

    static juce::Colour accentOrange() { return juce::Colour::fromRGB (240, 148, 56); }
    static juce::Colour warmGlass()    { return juce::Colour::fromRGBA (18, 13, 10, 224); }
}

//==============================================================================
// ImageRotaryKnob
//==============================================================================
MiAmpAudioProcessorEditor::ImageRotaryKnob::ImageRotaryKnob (
    juce::AudioProcessorValueTreeState& state,
    const juce::String& paramId,
    const juce::String& labelText,
    juce::Image knobImg,
    bool isBassHeadStyle,
    std::function<void(const juce::String&)> onHud)
    : apvts (state), pid (paramId), label (labelText),
      image (std::move (knobImg)), headStyle (isBassHeadStyle), hudCallback (std::move (onHud))
{
    setRepaintsOnMouseActivity (true);
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::setParamId (const juce::String& newId)
{
    pid = newId;
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::refreshFromParam()
{
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

        // Crisp white radial pointer line like Mi-Amp/index.html
        const float rInner = sz * 0.14f;
        const float rOuter = sz * 0.44f;
        const float sinA = std::sin (rad);
        const float cosA = -std::cos (rad);
        g.setColour (juce::Colour::fromRGBA (255, 255, 255, 240));
        g.drawLine (cx + sinA * rInner, cy + cosA * rInner,
                    cx + sinA * rOuter, cy + cosA * rOuter,
                    juce::jmax (1.8f, sz * 0.045f));
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
        else
        {
            g.setColour (juce::Colours::darkgrey);
            g.fillEllipse (cx - sz * 0.4f, cy - sz * 0.4f, sz * 0.8f, sz * 0.8f);
        }
    }

    if (isMouseOver() || dragging)
    {
        g.setColour (accentOrange().withAlpha (0.55f));
        g.drawEllipse (cx - sz * 0.48f, cy - sz * 0.48f, sz * 0.96f, sz * 0.96f, 1.5f);
    }
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseDown (const juce::MouseEvent& e)
{
    if (auto* raw = apvts.getRawParameterValue (pid))
        dragStartVal = raw->load();
    dragStartY = e.getPosition().y;
    dragging = true;
    if (hudCallback)
        hudCallback (label + ": " + juce::String (juce::roundToInt (dragStartVal)) + "%");
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
        hudCallback (label + ": " + juce::String (juce::roundToInt (nextVal)) + "%");
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter (pid))
        p->setValueNotifyingHost (p->convertTo0to1 (50.0f));
    if (hudCallback)
        hudCallback (label + ": 50%");
    repaint();
}

void MiAmpAudioProcessorEditor::ImageRotaryKnob::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (auto* raw = apvts.getRawParameterValue (pid))
    {
        const float nextVal = juce::jlimit (0.0f, 100.0f, raw->load() + wheel.deltaY * 14.0f);
        if (auto* p = apvts.getParameter (pid))
            p->setValueNotifyingHost (p->convertTo0to1 (nextVal));
        if (hudCallback)
            hudCallback (label + ": " + juce::String (juce::roundToInt (nextVal)) + "%");
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
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void MiAmpAudioProcessorEditor::ImageToggleSwitch::paint (juce::Graphics& g)
{
    auto* raw = apvts.getRawParameterValue (pid);
    const bool on = raw != nullptr && raw->load() > 0.5f;
    const auto& img = on ? imgUp : imgDown;
    if (img.isValid())
        g.drawImage (img, getLocalBounds().toFloat(), juce::RectanglePlacement::centred);
}

void MiAmpAudioProcessorEditor::ImageToggleSwitch::mouseDown (const juce::MouseEvent&)
{
    if (auto* p = apvts.getParameter (pid))
    {
        const bool next = p->getValue() < 0.5f;
        p->setValueNotifyingHost (next ? 1.0f : 0.0f);
        if (hudCallback)
            hudCallback (label + (next ? ": ON" : ": OFF"));
        repaint();
    }
}

//==============================================================================
// ModalOverlay (Presets + Drop Tuner + YIN Tuner)
//==============================================================================
class MiAmpAudioProcessorEditor::ModalOverlay final : public juce::Component
{
public:
    ModalOverlay (MiAmpAudioProcessor& proc,
                  std::function<void()> onClose,
                  std::function<void()> onChanged)
        : processor (proc), closeFn (std::move (onClose)), changedFn (std::move (onChanged))
    {
        addAndMakeVisible (btnClose);
        btnClose.setButtonText ("X");
        btnClose.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (65, 40, 28));
        btnClose.onClick = [this] { if (closeFn) closeFn(); };

        // Presets controls
        addChildComponent (presetNameEdit);
        presetNameEdit.setTextToShowWhenEmpty ("Nombre de tu preset...", juce::Colours::grey);
        presetNameEdit.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGB (28, 22, 18));
        presetNameEdit.setColour (juce::TextEditor::textColourId, juce::Colours::white);

        addChildComponent (btnSavePreset);
        btnSavePreset.setButtonText ("Guardar Preset");
        btnSavePreset.setColour (juce::TextButton::buttonColourId, accentOrange().darker (0.25f));
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

        // Drop controls
        addChildComponent (btnDropEnable);
        btnDropEnable.onClick = [this]
        {
            if (auto* p = processor.apvts.getParameter (MiAmpParams::pitchOn))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
            syncDropControls();
            if (changedFn) changedFn();
        };

        const int semiVals[7] = { 0, -1, -2, -3, -4, -5, 12 };
        const char* semiLbls[7] = { "0 (E)", "-1 Eb", "-2 D", "-3 C#", "-4 C", "-5 B", "+12 Oct" };
        for (int i = 0; i < 7; ++i)
        {
            addChildComponent (dropChips[i]);
            dropChips[i].setButtonText (semiLbls[i]);
            const int sv = semiVals[i];
            dropChips[i].onClick = [this, sv]
            {
                if (auto* p = processor.apvts.getParameter (MiAmpParams::pitchSemi))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (sv)));
                if (sv != 0)
                {
                    if (auto* pon = processor.apvts.getParameter (MiAmpParams::pitchOn))
                        pon->setValueNotifyingHost (1.0f);
                }
                syncDropControls();
                if (changedFn) changedFn();
            };
        }

        addChildComponent (slSemi);
        slSemi.setRange (-12.0, 12.0, 1.0);
        slSemi.setSliderStyle (juce::Slider::LinearHorizontal);
        slSemi.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
        slSemiAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::pitchSemi, slSemi);

        addChildComponent (slCents);
        slCents.setRange (-50.0, 50.0, 1.0);
        slCents.setSliderStyle (juce::Slider::LinearHorizontal);
        slCents.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
        slCentsAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::pitchCents, slCents);

        addChildComponent (slMix);
        slMix.setRange (0.0, 100.0, 1.0);
        slMix.setSliderStyle (juce::Slider::LinearHorizontal);
        slMix.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
        slMixAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, MiAmpParams::pitchMix, slMix);

        // Tuner controls
        addChildComponent (btnTunerMute);
        btnTunerMute.onClick = [this]
        {
            if (auto* p = processor.apvts.getParameter (MiAmpParams::tunerMuted))
                p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
            syncTunerMuteText();
            if (changedFn) changedFn();
        };
    }

    void setMode (ModalMode m, const xvdsp::TunerReading& tr)
    {
        mode = m;
        tuner = tr;

        const bool isP = (mode == ModalMode::Presets);
        const bool isD = (mode == ModalMode::Drop);
        const bool isT = (mode == ModalMode::Tuner);

        presetNameEdit.setVisible (isP);
        btnSavePreset.setVisible (isP);
        presetViewport.setVisible (isP);

        btnDropEnable.setVisible (isD);
        for (auto& b : dropChips) b.setVisible (isD);
        slSemi.setVisible (isD);
        slCents.setVisible (isD);
        slMix.setVisible (isD);

        btnTunerMute.setVisible (isT);

        if (isP) rebuildPresetList();
        if (isD) syncDropControls();
        if (isT) syncTunerMuteText();
        resized();
        repaint();
    }

    void updateTuner (const xvdsp::TunerReading& tr)
    {
        tuner = tr;
        if (mode == ModalMode::Tuner) repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour::fromRGBA (6, 4, 3, 185));

        const auto card = getCardBounds().toFloat();
        g.setColour (juce::Colour::fromRGB (22, 17, 14));
        g.fillRoundedRectangle (card, 14.0f);
        g.setColour (accentOrange().withAlpha (0.45f));
        g.drawRoundedRectangle (card, 14.0f, 1.5f);

        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (19.0f, juce::Font::bold));
        juce::String title = (mode == ModalMode::Presets) ? "PRESETS DE BAJO (MI AMP + XV)"
                           : (mode == ModalMode::Drop)    ? "DROP TUNER (PITCH SHIFTER)"
                                                          : "AFINADOR DE BAJO YIN";
        g.drawText (title, card.reduced (20.0f, 14.0f).removeFromTop (30.0f), juce::Justification::centredLeft);

        if (mode == ModalMode::Drop)
        {
            auto r = card.reduced (24.0f, 64.0f);
            g.setFont (juce::FontOptions (13.0f));
            g.setColour (juce::Colours::lightgrey);
            g.drawText ("Semitonos:", juce::Rectangle<float> (r.getX(), r.getY() + 108.0f, 100.0f, 24.0f), juce::Justification::centredLeft);
            g.drawText ("Cents:",     juce::Rectangle<float> (r.getX(), r.getY() + 150.0f, 100.0f, 24.0f), juce::Justification::centredLeft);
            g.drawText ("Mix (%):",   juce::Rectangle<float> (r.getX(), r.getY() + 192.0f, 100.0f, 24.0f), juce::Justification::centredLeft);
        }
        else if (mode == ModalMode::Tuner)
        {
            auto r = card.reduced (24.0f, 60.0f);
            const bool active = tuner.hasSignal && tuner.freqHz > 20.0f;

            // Note readout
            g.setFont (juce::FontOptions (56.0f, juce::Font::bold));
            const bool inTune = active && std::fabs (tuner.cents) < 6.0f;
            g.setColour (active ? (inTune ? juce::Colour::fromRGB (85, 230, 130) : accentOrange())
                                : juce::Colours::grey);
            const juce::String noteStr = active ? (juce::String (tuner.noteName) + juce::String (tuner.octave)) : "--";
            g.drawText (noteStr, r.removeFromTop (68.0f), juce::Justification::centred);

            g.setFont (juce::FontOptions (15.0f));
            g.setColour (juce::Colours::lightgrey);
            const juce::String hzStr = active
                ? (juce::String (tuner.freqHz, 1) + " Hz   |   "
                   + (tuner.cents >= 0 ? "+" : "") + juce::String (tuner.cents, 1) + " cents")
                : "Toca una cuerda de tu bajo...";
            g.drawText (hzStr, r.removeFromTop (24.0f), juce::Justification::centred);

            // Meter bar + needle
            auto bar = r.removeFromTop (44.0f).reduced (30.0f, 12.0f);
            g.setColour (juce::Colour::fromRGB (38, 30, 24));
            g.fillRoundedRectangle (bar, 6.0f);
            g.setColour (juce::Colour::fromRGB (85, 230, 130).withAlpha (0.4f));
            g.fillRect (bar.getCentreX() - 4.0f, bar.getY(), 8.0f, bar.getHeight());

            const float normCents = active ? juce::jlimit (-1.0f, 1.0f, tuner.cents / 50.0f) : 0.0f;
            const float nx = bar.getCentreX() + normCents * (bar.getWidth() * 0.46f);
            g.setColour (inTune ? juce::Colour::fromRGB (85, 230, 130) : accentOrange());
            g.fillRoundedRectangle (nx - 3.0f, bar.getY() - 4.0f, 6.0f, bar.getHeight() + 8.0f, 3.0f);

            // E1 A1 D2 G2 string markers
            auto stringsRow = r.removeFromTop (36.0f).reduced (40.0f, 4.0f);
            const char* strs[4] = { "E1", "A1", "D2", "G2" };
            const float w4 = stringsRow.getWidth() * 0.25f;
            for (int i = 0; i < 4; ++i)
            {
                auto cell = juce::Rectangle<float> (stringsRow.getX() + i * w4, stringsRow.getY(), w4, stringsRow.getHeight()).reduced (6.0f, 0.0f);
                const bool match = active && (juce::String (tuner.noteName) + juce::String (tuner.octave)) == strs[i];
                g.setColour (match ? accentOrange() : juce::Colour::fromRGB (42, 34, 28));
                g.fillRoundedRectangle (cell, 6.0f);
                g.setColour (match ? juce::Colours::black : juce::Colours::lightgrey);
                g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
                g.drawText (strs[i], cell, juce::Justification::centred);
            }
        }
    }

    void resized() override
    {
        auto card = getCardBounds();
        btnClose.setBounds (card.getRight() - 44, card.getY() + 12, 30, 28);

        if (mode == ModalMode::Presets)
        {
            auto inner = card.reduced (20, 54);
            auto topRow = inner.removeFromTop (34);
            btnSavePreset.setBounds (topRow.removeFromRight (140));
            topRow.removeFromRight (8);
            presetNameEdit.setBounds (topRow);
            inner.removeFromTop (10);
            presetViewport.setBounds (inner);
            layoutPresetButtons();
        }
        else if (mode == ModalMode::Drop)
        {
            auto r = card.reduced (24, 64);
            btnDropEnable.setBounds (r.getX(), r.getY(), 180, 34);
            auto chipsRow = juce::Rectangle<int> (r.getX(), r.getY() + 48, r.getWidth(), 34);
            const int cw = chipsRow.getWidth() / 7;
            for (int i = 0; i < 7; ++i)
                dropChips[i].setBounds (chipsRow.getX() + i * cw + 2, chipsRow.getY(), cw - 4, 32);

            slSemi.setBounds  (r.getX() + 106, r.getY() + 104, r.getWidth() - 106, 30);
            slCents.setBounds (r.getX() + 106, r.getY() + 146, r.getWidth() - 106, 30);
            slMix.setBounds   (r.getX() + 106, r.getY() + 188, r.getWidth() - 106, 30);
        }
        else if (mode == ModalMode::Tuner)
        {
            auto cardB = card.reduced (24, 20);
            btnTunerMute.setBounds (cardB.getCentreX() - 95, cardB.getBottom() - 42, 190, 34);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! getCardBounds().contains (e.getPosition()))
            if (closeFn) closeFn();
    }

private:
    juce::Rectangle<int> getCardBounds() const
    {
        const int w = juce::jmin (640, getWidth() - 40);
        const int h = (mode == ModalMode::Presets) ? juce::jmin (440, getHeight() - 40) : 330;
        return juce::Rectangle<int> ((getWidth() - w) / 2, (getHeight() - h) / 2, w, h);
    }

    void syncDropControls()
    {
        const bool on = processor.apvts.getRawParameterValue (MiAmpParams::pitchOn)->load() > 0.5f;
        btnDropEnable.setButtonText (on ? "DROP PITCH: ACTIVADO" : "DROP PITCH: APAGADO");
        btnDropEnable.setColour (juce::TextButton::buttonColourId,
                                 on ? accentOrange().darker (0.15f) : juce::Colour::fromRGB (55, 45, 38));
    }

    void syncTunerMuteText()
    {
        const bool muted = processor.apvts.getRawParameterValue (MiAmpParams::tunerMuted)->load() > 0.5f;
        btnTunerMute.setButtonText (muted ? "MUTE AL AFINAR: ON" : "MUTE AL AFINAR: OFF");
        btnTunerMute.setColour (juce::TextButton::buttonColourId,
                                muted ? juce::Colour::fromRGB (180, 50, 45) : juce::Colour::fromRGB (55, 45, 38));
    }

    void rebuildPresetList()
    {
        presetRows.clear();
        presetListHolder.removeAllChildren();

        auto addRow = [this] (const juce::String& text, const juce::String& hint, bool isActive,
                              std::function<void()> onLoad, std::function<void()> onDel)
        {
            auto row = std::make_unique<PresetRow> (text, hint, isActive, std::move (onLoad), std::move (onDel));
            presetListHolder.addAndMakeVisible (*row);
            presetRows.push_back (std::move (row));
        };

        const auto activeName = processor.getActivePresetName();
        const auto& userList = processor.getUserPresets();
        for (int i = 0; i < static_cast<int> (userList.size()); ++i)
        {
            const auto& up = userList[static_cast<size_t> (i)];
            addRow ("★ " + up.name, "Preset guardado por vos", up.name == activeName,
                [this, i] { processor.loadUserPreset (i); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                [this, i] { processor.deleteUserPreset (i); rebuildPresetList(); });
        }

        auto factory = MiAmpAudioProcessor::getFactoryPresets();
        for (const auto& fp : factory)
        {
            addRow (fp.name, fp.hint, fp.name == activeName,
                [this, fp] { processor.applyPreset (fp); if (changedFn) changedFn(); if (closeFn) closeFn(); },
                nullptr);
        }

        layoutPresetButtons();
    }

    void layoutPresetButtons()
    {
        const int vw = juce::jmax (200, presetViewport.getWidth() - 16);
        const int rowH = 38;
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
        PresetRow (const juce::String& title, const juce::String& hint, bool active,
                   std::function<void()> loadCb, std::function<void()> delCb)
        {
            addAndMakeVisible (btnLoad);
            btnLoad.setButtonText (title + (hint.isNotEmpty() ? ("  —  " + hint) : ""));
            btnLoad.setColour (juce::TextButton::buttonColourId,
                               active ? accentOrange().darker (0.25f) : juce::Colour::fromRGB (40, 32, 26));
            btnLoad.onClick = std::move (loadCb);

            if (delCb)
            {
                addAndMakeVisible (btnDel);
                btnDel.setButtonText ("Borrar");
                btnDel.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (130, 40, 35));
                btnDel.onClick = std::move (delCb);
            }
        }
        void resized() override
        {
            auto b = getLocalBounds();
            if (btnDel.isVisible())
            {
                btnDel.setBounds (b.removeFromRight (68));
                b.removeFromRight (6);
            }
            btnLoad.setBounds (b);
        }
        juce::TextButton btnLoad, btnDel;
    };

    MiAmpAudioProcessor& processor;
    std::function<void()> closeFn, changedFn;
    ModalMode mode = ModalMode::None;
    xvdsp::TunerReading tuner;

    juce::TextButton btnClose;
    juce::TextEditor presetNameEdit;
    juce::TextButton btnSavePreset;
    juce::Viewport presetViewport;
    juce::Component presetListHolder;
    std::vector<std::unique_ptr<PresetRow>> presetRows;

    juce::TextButton btnDropEnable;
    juce::TextButton dropChips[7];
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
    loadImages();

    // Top bar buttons
    for (auto* b : { &btnHead, &btnPedals, &btnPresets, &btnDrop, &btnTuner, &btnCab })
    {
        addAndMakeVisible (b);
        b->setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    }

    btnHead.onClick    = [this] { setViewMode (ViewMode::Head); };
    btnPedals.onClick  = [this] { setViewMode (ViewMode::Pedals); };
    btnPresets.onClick = [this] { setModalMode (modalMode == ModalMode::Presets ? ModalMode::None : ModalMode::Presets); };
    btnDrop.onClick    = [this] { setModalMode (modalMode == ModalMode::Drop    ? ModalMode::None : ModalMode::Drop); };
    btnTuner.onClick   = [this] { setModalMode (modalMode == ModalMode::Tuner   ? ModalMode::None : ModalMode::Tuner); };
    btnCab.onClick     = [this]
    {
        if (auto* cab = audioProcessor.apvts.getParameter (MiAmpParams::ampCabOn))
        {
            const bool nxt = cab->getValue() < 0.5f;
            cab->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
            showHud (nxt ? "Caja 8x10: ACTIVA" : "Caja 8x10: OFF");
        }
    };

    // Bass Head knobs & switches
    for (int i = 0; i < 5; ++i)
    {
        headKnobs[i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, kHeadKnobIds[i], kHeadKnobNames[i],
            imgs.bassKnobCap, true, [this] (const juce::String& t) { showHud (t); });
        addAndMakeVisible (*headKnobs[i]);
    }
    for (int i = 0; i < 3; ++i)
    {
        headSwitches[i] = std::make_unique<ImageToggleSwitch> (
            audioProcessor.apvts, kSwitchIds[i], kSwitchNames[i],
            imgs.toggleUp, imgs.toggleDown, [this] (const juce::String& t) { showHud (t); });
        addAndMakeVisible (*headSwitches[i]);
    }

    // Pedal editor knobs (max 7 for XV)
    for (int i = 0; i < 7; ++i)
    {
        pedalKnobs[i] = std::make_unique<ImageRotaryKnob> (
            audioProcessor.apvts, kXvKnobIds[i], kXvKnobNames[i],
            imgs.xvKnobPtr, false, [this] (const juce::String& t) { showHud (t); });
        addChildComponent (*pedalKnobs[i]);
    }

    for (auto* b : { &btnMoveLeft, &btnMoveRight, &btnRemovePedal })
    {
        addChildComponent (b);
        b->setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGB (44, 34, 28));
    }
    btnMoveLeft.onClick    = [this] { audioProcessor.movePedalInChain (selectedPedalType, -1); syncPedalEditorControls(); repaint(); };
    btnMoveRight.onClick   = [this] { audioProcessor.movePedalInChain (selectedPedalType, +1); syncPedalEditorControls(); repaint(); };
    btnRemovePedal.onClick = [this]
    {
        audioProcessor.removePedalFromChain (selectedPedalType);
        auto ch = audioProcessor.getActiveChain();
        if (! ch.empty()) selectedPedalType = ch.front();
        syncPedalEditorControls();
        repaint();
    };

    for (int t = 0; t < 5; ++t)
    {
        addChildComponent (addButtons[t]);
        addButtons[t].setButtonText ("+ " + pedalName (t));
        addButtons[t].onClick = [this, t]
        {
            if (audioProcessor.addPedalToChain (t))
                selectedPedalType = t;
            else
                selectedPedalType = t;
            syncPedalEditorControls();
            repaint();
        };
    }

    modalOverlay = std::make_unique<ModalOverlay> (
        audioProcessor,
        [this] { setModalMode (ModalMode::None); },
        [this] { syncPedalEditorControls(); repaint(); });
    addChildComponent (*modalOverlay);

    setResizable (true, true);
    setResizeLimits (900, 480, 2400, 1400);
    setSize (1240, 650);

    setViewMode (ViewMode::Head);
    startTimerHz (30);
}

MiAmpAudioProcessorEditor::~MiAmpAudioProcessorEditor()
{
    stopTimer();
}

void MiAmpAudioProcessorEditor::loadImages()
{
    auto img = [] (const void* data, int size) { return juce::ImageCache::getFromMemory (data, size); };

    imgs.bassBg      = img (BinaryData::bass_bg_png,      BinaryData::bass_bg_pngSize);
    imgs.bassKnobCap = img (BinaryData::bass_knob_cap_png, BinaryData::bass_knob_cap_pngSize);
    imgs.toggleUp    = img (BinaryData::toggle_up_png,    BinaryData::toggle_up_pngSize);
    imgs.toggleDown  = img (BinaryData::toggle_down_png,  BinaryData::toggle_down_pngSize);

    imgs.floorBg      = img (BinaryData::floor_bg_png,      BinaryData::floor_bg_pngSize);
    imgs.boardSurface = img (BinaryData::board_surface_png, BinaryData::board_surface_pngSize);

    imgs.xvActive   = img (BinaryData::xv_active_png,    BinaryData::xv_active_pngSize);
    imgs.xvBypass   = img (BinaryData::xv_bypass_png,    BinaryData::xv_bypass_pngSize);
    imgs.xvKnobPtr  = img (BinaryData::xv_knob_ptr_png,  BinaryData::xv_knob_ptr_pngSize);
    imgs.xvCabBass  = img (BinaryData::xv_cab_bass_png,  BinaryData::xv_cab_bass_pngSize);
    imgs.xvCabPunk  = img (BinaryData::xv_cab_punk_png,  BinaryData::xv_cab_punk_pngSize);
    imgs.xvCabClean = img (BinaryData::xv_cab_clean_png, BinaryData::xv_cab_clean_pngSize);

    imgs.compActive = img (BinaryData::pedal_compresor_active_png, BinaryData::pedal_compresor_active_pngSize);
    imgs.compOff    = img (BinaryData::pedal_compresor_off_png,    BinaryData::pedal_compresor_off_pngSize);
    imgs.chActive   = img (BinaryData::pedal_chorus_active_png,    BinaryData::pedal_chorus_active_pngSize);
    imgs.chOff      = img (BinaryData::pedal_chorus_off_png,       BinaryData::pedal_chorus_off_pngSize);
    imgs.phActive   = img (BinaryData::pedal_phaser_active_png,    BinaryData::pedal_phaser_active_pngSize);
    imgs.phOff      = img (BinaryData::pedal_phaser_off_png,       BinaryData::pedal_phaser_off_pngSize);
    imgs.rvActive   = img (BinaryData::pedal_reverb_active_png,    BinaryData::pedal_reverb_active_pngSize);
    imgs.rvOff      = img (BinaryData::pedal_reverb_off_png,       BinaryData::pedal_reverb_off_pngSize);

    imgs.compKnob[0] = img (BinaryData::knob_compresor_0_png, BinaryData::knob_compresor_0_pngSize);
    imgs.compKnob[1] = img (BinaryData::knob_compresor_1_png, BinaryData::knob_compresor_1_pngSize);
    imgs.compKnob[2] = img (BinaryData::knob_compresor_2_png, BinaryData::knob_compresor_2_pngSize);
    imgs.chKnob[0]   = img (BinaryData::knob_chorus_0_png,    BinaryData::knob_chorus_0_pngSize);
    imgs.chKnob[1]   = img (BinaryData::knob_chorus_1_png,    BinaryData::knob_chorus_1_pngSize);
    imgs.chKnob[2]   = img (BinaryData::knob_chorus_2_png,    BinaryData::knob_chorus_2_pngSize);
    imgs.phKnob[0]   = img (BinaryData::knob_phaser_0_png,    BinaryData::knob_phaser_0_pngSize);
    imgs.phKnob[1]   = img (BinaryData::knob_phaser_1_png,    BinaryData::knob_phaser_1_pngSize);
    imgs.phKnob[2]   = img (BinaryData::knob_phaser_2_png,    BinaryData::knob_phaser_2_pngSize);
}

juce::String MiAmpAudioProcessorEditor::pedalName (int type)
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

const juce::Image& MiAmpAudioProcessorEditor::pedalImage (int type, bool active) const
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

const char* MiAmpAudioProcessorEditor::pedalBypassParamId (int type)
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

void MiAmpAudioProcessorEditor::showHud (const juce::String& text)
{
    hudText = text;
    hudTicks = 55;
    repaint();
}

void MiAmpAudioProcessorEditor::setViewMode (ViewMode m)
{
    viewMode = m;
    const bool isHead = (viewMode == ViewMode::Head);

    for (auto& k : headKnobs)    k->setVisible (isHead);
    for (auto& s : headSwitches) s->setVisible (isHead);

    syncPedalEditorControls();
    resized();
    repaint();
}

void MiAmpAudioProcessorEditor::setModalMode (ModalMode m)
{
    modalMode = m;
    if (modalMode == ModalMode::None)
    {
        modalOverlay->setVisible (false);
    }
    else
    {
        modalOverlay->setBounds (0, static_cast<int> (kBarH), getWidth(), getHeight() - static_cast<int> (kBarH));
        modalOverlay->setMode (modalMode, tunerReading);
        modalOverlay->setVisible (true);
        modalOverlay->toFront (true);
    }
    repaint();
}

void MiAmpAudioProcessorEditor::syncPedalEditorControls()
{
    const bool isPed = (viewMode == ViewMode::Pedals);
    auto chain = audioProcessor.getActiveChain();
    const bool hasSelected = std::find (chain.begin(), chain.end(), selectedPedalType) != chain.end();

    if (isPed && ! hasSelected && ! chain.empty())
        selectedPedalType = chain.front();

    const bool showEd = isPed && ! chain.empty();

    btnMoveLeft.setVisible (showEd);
    btnMoveRight.setVisible (showEd);
    btnRemovePedal.setVisible (showEd);

    for (int t = 0; t < 5; ++t)
    {
        addButtons[t].setVisible (isPed);
        const bool inChain = std::find (chain.begin(), chain.end(), t) != chain.end();
        addButtons[t].setColour (juce::TextButton::buttonColourId,
                                 inChain ? juce::Colour::fromRGB (38, 32, 27)
                                         : accentOrange().darker (0.3f));
    }

    if (! showEd)
    {
        for (auto& k : pedalKnobs) k->setVisible (false);
        return;
    }

    if (selectedPedalType == 1)
    {
        for (int i = 0; i < 7; ++i)
        {
            pedalKnobs[i] = std::make_unique<ImageRotaryKnob> (
                audioProcessor.apvts, kXvKnobIds[i], kXvKnobNames[i],
                imgs.xvKnobPtr, false, [this] (const juce::String& s) { showHud (s); });
            addAndMakeVisible (*pedalKnobs[i]);
        }
    }
    else
    {
        const char* ids[3] = {};
        const char* names[3] = {};
        const juce::Image* kImgs = imgs.compKnob;

        if (selectedPedalType == 0)
        {
            ids[0] = MiAmpParams::compSustain; ids[1] = MiAmpParams::compAttack; ids[2] = MiAmpParams::compLevel;
            names[0] = "SUSTAIN"; names[1] = "ATTACK"; names[2] = "LEVEL";
            kImgs = imgs.compKnob;
        }
        else if (selectedPedalType == 2)
        {
            ids[0] = MiAmpParams::chorusRate; ids[1] = MiAmpParams::chorusDepth; ids[2] = MiAmpParams::chorusMix;
            names[0] = "RATE"; names[1] = "DEPTH"; names[2] = "MIX";
            kImgs = imgs.chKnob;
        }
        else if (selectedPedalType == 3)
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

        for (int i = 0; i < 3; ++i)
        {
            pedalKnobs[i] = std::make_unique<ImageRotaryKnob> (
                audioProcessor.apvts, ids[i], names[i],
                kImgs[i], false, [this] (const juce::String& s) { showHud (s); });
            addAndMakeVisible (*pedalKnobs[i]);
        }
        for (int i = 3; i < 7; ++i)
            pedalKnobs[i]->setVisible (false);
    }

    if (modalOverlay != nullptr && modalOverlay->isVisible())
        modalOverlay->toFront (true);

    layoutPedalControls();
}

void MiAmpAudioProcessorEditor::timerCallback()
{
    tunerReading = audioProcessor.tuner.analyze();
    if (modalMode == ModalMode::Tuner && modalOverlay != nullptr)
        modalOverlay->updateTuner (tunerReading);

    if (hudTicks > 0 && --hudTicks == 0)
        hudText.clear();

    // Update top bar button states
    btnHead.setColour (juce::TextButton::buttonColourId,
                       viewMode == ViewMode::Head ? accentOrange().darker (0.2f) : juce::Colour::fromRGB (38, 30, 24));
    btnPedals.setColour (juce::TextButton::buttonColourId,
                         viewMode == ViewMode::Pedals ? accentOrange().darker (0.2f) : juce::Colour::fromRGB (38, 30, 24));

    const bool dropOn = audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchOn)->load() > 0.5f;
    const int semi = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::pitchSemi)->load()));
    btnDrop.setButtonText (dropOn ? ("DROP " + juce::String (semi > 0 ? "+" : "") + juce::String (semi)) : "DROP");
    btnDrop.setColour (juce::TextButton::buttonColourId,
                       dropOn ? accentOrange().darker (0.2f) : juce::Colour::fromRGB (38, 30, 24));

    const bool cabOn = audioProcessor.apvts.getRawParameterValue (MiAmpParams::ampCabOn)->load() > 0.5f;
    btnCab.setColour (juce::TextButton::buttonColourId,
                      cabOn ? juce::Colour::fromRGB (58, 92, 62) : juce::Colour::fromRGB (70, 42, 36));

    btnPresets.setButtonText ("PRESETS: " + audioProcessor.getActivePresetName().substring (0, 18));

    repaint();
}

void MiAmpAudioProcessorEditor::resized()
{
    // Top bar
    const int w = getWidth();
    int x = 14;
    const int y = 10;
    const int h = 30;
    btnHead.setBounds    (x, y, 86,  h); x += 92;
    btnPedals.setBounds  (x, y, 86,  h); x += 100;
    btnPresets.setBounds (x, y, 210, h); x += 218;
    btnDrop.setBounds    (x, y, 90,  h); x += 96;
    btnTuner.setBounds   (x, y, 92,  h); x += 98;
    btnCab.setBounds     (x, y, 88,  h);

    if (viewMode == ViewMode::Head)
        layoutHeadControls();
    else
        layoutPedalControls();

    if (modalOverlay != nullptr && modalOverlay->isVisible())
        modalOverlay->setBounds (0, static_cast<int> (kBarH), w, getHeight() - static_cast<int> (kBarH));
}

void MiAmpAudioProcessorEditor::layoutHeadControls()
{
    const auto area = juce::Rectangle<float> (0.0f, kBarH, static_cast<float> (getWidth()), static_cast<float> (getHeight()) - kBarH);
    const float knobPx = juce::jmax (30.0f, area.getWidth() * (58.0f / kHeadW));

    for (int i = 0; i < 5; ++i)
    {
        const float cx = area.getX() + (kHeadKnobX[i] / kHeadW) * area.getWidth();
        const float cy = area.getY() + (kHeadKnobY / kHeadH) * area.getHeight();
        headKnobs[i]->setBounds (juce::roundToInt (cx - knobPx * 0.5f),
                                 juce::roundToInt (cy - knobPx * 0.5f),
                                 juce::roundToInt (knobPx),
                                 juce::roundToInt (knobPx));
    }

    const float swW = area.getWidth()  * (26.0f / kHeadW);
    const float swH = area.getHeight() * (50.0f / kHeadH);
    for (int i = 0; i < 3; ++i)
    {
        const float cx = area.getX() + (kSwitchX[i] / kHeadW) * area.getWidth();
        const float cy = area.getY() + (kSwitchY    / kHeadH) * area.getHeight();
        headSwitches[i]->setBounds (juce::roundToInt (cx - swW * 0.5f),
                                    juce::roundToInt (cy - swH * 0.5f),
                                    juce::roundToInt (swW),
                                    juce::roundToInt (swH));
    }
}

juce::Rectangle<float> MiAmpAudioProcessorEditor::computeEditorPedalRect (juce::Rectangle<float> area) const
{
    auto leftArea = area.withWidth (area.getWidth() * 0.44f).reduced (24.0f, 34.0f);
    leftArea.removeFromBottom (46.0f);
    const float aspect = (selectedPedalType == 1) ? (1391.0f / 1131.0f) : 0.70f;
    float w = leftArea.getWidth();
    float h = w / aspect;
    if (h > leftArea.getHeight())
    {
        h = leftArea.getHeight();
        w = h * aspect;
    }
    return juce::Rectangle<float> (leftArea.getCentreX() - w * 0.5f,
                                   leftArea.getCentreY() - h * 0.5f, w, h);
}

std::vector<juce::Rectangle<float>> MiAmpAudioProcessorEditor::computeSlotRects (juce::Rectangle<float> boardRect, int count) const
{
    std::vector<juce::Rectangle<float>> rects;
    if (count <= 0) return rects;
    const float slotW = juce::jmin (boardRect.getWidth() / static_cast<float> (juce::jmax (count, 2)),
                                    boardRect.getHeight() * 0.85f);
    const float totalW = slotW * static_cast<float> (count);
    const float startX = boardRect.getCentreX() - totalW * 0.5f;

    for (int i = 0; i < count; ++i)
    {
        const float cellX = startX + i * slotW;
        const float cellW = slotW * 0.90f;
        const float cellH = juce::jmin (boardRect.getHeight() * 0.82f, cellW * 1.42f);
        rects.emplace_back (cellX + (slotW - cellW) * 0.5f,
                            boardRect.getCentreY() - cellH * 0.5f,
                            cellW, cellH);
    }
    return rects;
}

void MiAmpAudioProcessorEditor::layoutPedalControls()
{
    const auto area = juce::Rectangle<float> (0.0f, kBarH, static_cast<float> (getWidth()), static_cast<float> (getHeight()) - kBarH);
    const auto pedalR = computeEditorPedalRect (area);

    // Bottom palette buttons
    const int palY = getHeight() - 42;
    const int palX0 = static_cast<int> (area.getWidth() * 0.45f);
    const int palW = (getWidth() - palX0 - 20) / 5;
    for (int t = 0; t < 5; ++t)
        addButtons[t].setBounds (palX0 + t * palW + 3, palY, palW - 6, 30);

    // Chain reorder / remove buttons under selected pedal
    const int edBtnY = static_cast<int> (pedalR.getBottom() + 8.0f);
    btnMoveLeft.setBounds    (juce::roundToInt (pedalR.getCentreX() - 98.0f), edBtnY, 44, 28);
    btnMoveRight.setBounds   (juce::roundToInt (pedalR.getCentreX() - 48.0f), edBtnY, 44, 28);
    btnRemovePedal.setBounds (juce::roundToInt (pedalR.getCentreX() + 4.0f),  edBtnY, 94, 28);

    if (selectedPedalType == 1)
    {
        const float kSize = pedalR.getWidth() * 0.116f;
        for (int i = 0; i < 7; ++i)
        {
            const float cx = pedalR.getX() + kXvPoints[i][0] * pedalR.getWidth();
            const float cy = pedalR.getY() + kXvPoints[i][1] * pedalR.getHeight();
            pedalKnobs[i]->setBounds (juce::roundToInt (cx - kSize * 0.5f),
                                      juce::roundToInt (cy - kSize * 0.5f),
                                      juce::roundToInt (kSize),
                                      juce::roundToInt (kSize));
        }
    }
    else
    {
        const float (*pts)[2] = kCompPoints;
        float sizeFrac = 0.2133f;
        if (selectedPedalType == 2 || selectedPedalType == 4) { pts = kChorusPoints; sizeFrac = 0.2484f; }
        else if (selectedPedalType == 3) { pts = kPhaserPoints; sizeFrac = 0.2185f; }

        const float kSize = pedalR.getWidth() * sizeFrac;
        for (int i = 0; i < 3; ++i)
        {
            const float cx = pedalR.getX() + pts[i][0] * pedalR.getWidth();
            const float cy = pedalR.getY() + pts[i][1] * pedalR.getHeight();
            pedalKnobs[i]->setBounds (juce::roundToInt (cx - kSize * 0.5f),
                                      juce::roundToInt (cy - kSize * 0.5f),
                                      juce::roundToInt (kSize),
                                      juce::roundToInt (kSize));
        }
    }
}

void MiAmpAudioProcessorEditor::paintHeadBackground (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (imgs.bassBg.isValid())
        g.drawImage (imgs.bassBg, area, juce::RectanglePlacement::stretchToFit);

    // Darken red power LED if Power switch is OFF
    const bool pwr = audioProcessor.apvts.getRawParameterValue (MiAmpParams::ampPower)->load() > 0.5f;
    if (! pwr)
    {
        const float lx = area.getX() + (967.0f / 1550.0f) * area.getWidth();
        const float ly = area.getY() + (354.0f / 768.0f)  * area.getHeight();
        const float lr = area.getWidth() * (14.0f / 1550.0f);
        g.setColour (juce::Colour::fromRGB (35, 10, 10));
        g.fillEllipse (lx, ly, lr, lr);
    }
}

void MiAmpAudioProcessorEditor::paintPedalboardView (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (imgs.floorBg.isValid())
        g.drawImage (imgs.floorBg, area, juce::RectanglePlacement::stretchToFit);
    else
        g.fillAll (juce::Colour::fromRGB (20, 16, 14));

    auto chain = audioProcessor.getActiveChain();

    // Right side: Pedalboard surface
    auto rightArea = area.withLeft (area.getWidth() * 0.45f).reduced (16.0f, 18.0f);
    rightArea.removeFromBottom (42.0f);
    if (imgs.boardSurface.isValid())
        g.drawImage (imgs.boardSurface, rightArea, juce::RectanglePlacement::stretchToFit);

    if (chain.empty())
    {
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (15.0f));
        g.drawText ("Pedalboard vacio. Agrega un pedal abajo (+ Compresor, + XV Bass, ...)",
                    rightArea, juce::Justification::centred);
        return;
    }

    // Left side: Selected Pedal chassis + XV Cab switch
    const auto pedalR = computeEditorPedalRect (area);
    const bool selActive = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (selectedPedalType))->load() > 0.5f;
    const auto& edImg = pedalImage (selectedPedalType, selActive);
    if (edImg.isValid())
        g.drawImage (edImg, pedalR, juce::RectanglePlacement::stretchToFit);

    if (selectedPedalType == 1)
    {
        const int cabIdx = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::xvCab)->load()));
        const auto& cabImg = (cabIdx == 0) ? imgs.xvCabBass : (cabIdx == 1 ? imgs.xvCabPunk : imgs.xvCabClean);
        if (cabImg.isValid())
        {
            const auto cabR = juce::Rectangle<float> (
                pedalR.getX() + 0.7117f * pedalR.getWidth(),
                pedalR.getY() + 0.328f  * pedalR.getHeight(),
                0.1294f * pedalR.getWidth(),
                0.1294f * pedalR.getWidth() * (static_cast<float> (cabImg.getHeight()) / juce::jmax (1, cabImg.getWidth())));
            g.drawImage (cabImg, cabR, juce::RectanglePlacement::stretchToFit);
        }
    }

    // Board slots
    const auto boardInner = rightArea.reduced (20.0f, 22.0f);
    const auto slots = computeSlotRects (boardInner, static_cast<int> (chain.size()));
    for (size_t i = 0; i < chain.size(); ++i)
    {
        const int t = chain[i];
        auto r = slots[i];
        const bool active = audioProcessor.apvts.getRawParameterValue (pedalBypassParamId (t))->load() > 0.5f;
        const bool isSel = (t == selectedPedalType);

        if (t == 1)
        {
            // Keep XV square-ish aspect inside slot
            const float targetH = r.getWidth() * (1131.0f / 1391.0f);
            r = juce::Rectangle<float> (r.getX(), r.getCentreY() - targetH * 0.5f, r.getWidth(), targetH);
        }

        const auto& pImg = pedalImage (t, active);
        if (pImg.isValid())
            g.drawImage (pImg, r, juce::RectanglePlacement::stretchToFit);

        if (isSel)
        {
            g.setColour (accentOrange());
            g.drawRoundedRectangle (r.expanded (4.0f), 8.0f, 2.2f);
        }

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        g.drawText (juce::String (i + 1) + ". " + pedalName (t),
                    juce::Rectangle<float> (r.getX(), r.getBottom() + 4.0f, r.getWidth(), 18.0f),
                    juce::Justification::centred);
    }
}

void MiAmpAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour::fromRGB (14, 11, 9));

    const auto area = juce::Rectangle<float> (0.0f, kBarH,
                                              static_cast<float> (getWidth()),
                                              static_cast<float> (getHeight()) - kBarH);

    if (viewMode == ViewMode::Head)
        paintHeadBackground (g, area);
    else
        paintPedalboardView (g, area);

    // Top bar warm glass
    g.setColour (warmGlass());
    g.fillRect (0.0f, 0.0f, static_cast<float> (getWidth()), kBarH);
    g.setColour (juce::Colour::fromRGBA (255, 255, 255, 28));
    g.drawLine (0.0f, kBarH, static_cast<float> (getWidth()), kBarH, 1.0f);

    // Live IN/OUT meter on top-right
    const float mrX = static_cast<float> (getWidth()) - 184.0f;
    if (mrX > 690.0f)
    {
        auto meterBox = juce::Rectangle<float> (mrX, 10.0f, 170.0f, 30.0f);
        g.setColour (juce::Colour::fromRGB (28, 22, 18));
        g.fillRoundedRectangle (meterBox, 8.0f);

        const float inDb  = juce::jlimit (0.0f, 1.0f, audioProcessor.inputRms.load() * 3.5f);
        const float outDb = juce::jlimit (0.0f, 1.0f, audioProcessor.outputRms.load() * 3.5f);
        g.setColour (juce::Colours::lightgrey);
        g.setFont (juce::FontOptions (10.5f, juce::Font::bold));
        g.drawText ("IN",  juce::Rectangle<float> (mrX + 8.0f, 12.0f, 22.0f, 12.0f), juce::Justification::centredLeft);
        g.drawText ("OUT", juce::Rectangle<float> (mrX + 8.0f, 25.0f, 24.0f, 12.0f), juce::Justification::centredLeft);

        g.setColour (juce::Colour::fromRGB (46, 38, 32));
        g.fillRoundedRectangle (mrX + 34.0f, 15.0f, 90.0f, 6.0f, 3.0f);
        g.fillRoundedRectangle (mrX + 34.0f, 28.0f, 90.0f, 6.0f, 3.0f);

        g.setColour (juce::Colour::fromRGB (92, 214, 130));
        g.fillRoundedRectangle (mrX + 34.0f, 15.0f, 90.0f * inDb, 6.0f, 3.0f);
        g.setColour (accentOrange());
        g.fillRoundedRectangle (mrX + 34.0f, 28.0f, 90.0f * outDb, 6.0f, 3.0f);

        const bool clip = audioProcessor.isClipping.load();
        g.setColour (clip ? juce::Colour::fromRGB (230, 58, 50) : juce::Colour::fromRGB (48, 38, 32));
        g.fillRoundedRectangle (mrX + 130.0f, 14.0f, 34.0f, 22.0f, 4.0f);
        g.setColour (clip ? juce::Colours::white : juce::Colours::grey);
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
        g.drawText ("CLIP", juce::Rectangle<float> (mrX + 130.0f, 14.0f, 34.0f, 22.0f), juce::Justification::centred);
    }

    // Transient value HUD
    if (hudText.isNotEmpty())
    {
        auto hudR = juce::Rectangle<float> (static_cast<float> (getWidth()) * 0.5f - 120.0f, kBarH + 12.0f, 240.0f, 30.0f);
        g.setColour (juce::Colour::fromRGBA (14, 10, 8, 225));
        g.fillRoundedRectangle (hudR, 15.0f);
        g.setColour (accentOrange().withAlpha (0.7f));
        g.drawRoundedRectangle (hudR, 15.0f, 1.2f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (hudText, hudR, juce::Justification::centred);
    }
}

void MiAmpAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    // Click on CLIP pill clears clip LED
    if (e.y < static_cast<int> (kBarH) && e.x > getWidth() - 60)
    {
        audioProcessor.isClipping.store (false, std::memory_order_relaxed);
        repaint();
        return;
    }

    if (viewMode != ViewMode::Pedals) return;

    const auto area = juce::Rectangle<float> (0.0f, kBarH,
                                              static_cast<float> (getWidth()),
                                              static_cast<float> (getHeight()) - kBarH);
    const auto pos = e.position;

    // 1. Click on selected pedal editor's XV cab switch or footswitch
    auto chain = audioProcessor.getActiveChain();
    if (! chain.empty())
    {
        const auto pedalR = computeEditorPedalRect (area);
        if (selectedPedalType == 1)
        {
            const auto cabR = juce::Rectangle<float> (
                pedalR.getX() + 0.70f * pedalR.getWidth(),
                pedalR.getY() + 0.31f * pedalR.getHeight(),
                0.15f * pedalR.getWidth(), 0.22f * pedalR.getHeight());
            if (cabR.contains (pos))
            {
                const int cur = static_cast<int> (std::lround (audioProcessor.apvts.getRawParameterValue (MiAmpParams::xvCab)->load()));
                const int nxt = (cur + 1) % 3;
                if (auto* p = audioProcessor.apvts.getParameter (MiAmpParams::xvCab))
                    p->setValueNotifyingHost (p->convertTo0to1 (static_cast<float> (nxt)));
                const char* names[3] = { "BASS", "PUNK", "CLEAN" };
                showHud (juce::String ("XV Modo: ") + names[nxt]);
                repaint();
                return;
            }
        }

        const auto swR = (selectedPedalType == 1)
            ? juce::Rectangle<float> (pedalR.getX() + 0.10f * pedalR.getWidth(), pedalR.getY() + 0.62f * pedalR.getHeight(),
                                      0.20f * pedalR.getWidth(), 0.24f * pedalR.getHeight())
            : juce::Rectangle<float> (pedalR.getX() + 0.20f * pedalR.getWidth(), pedalR.getY() + 0.62f * pedalR.getHeight(),
                                      0.60f * pedalR.getWidth(), 0.28f * pedalR.getHeight());
        if (swR.contains (pos))
        {
            if (auto* p = audioProcessor.apvts.getParameter (pedalBypassParamId (selectedPedalType)))
            {
                const bool nxt = p->getValue() < 0.5f;
                p->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
                showHud (pedalName (selectedPedalType) + (nxt ? ": ON" : ": BYPASS"));
                repaint();
                return;
            }
        }
    }

    // 2. Click on pedalboard slot -> select pedal
    auto rightArea = area.withLeft (area.getWidth() * 0.45f).reduced (16.0f, 18.0f);
    rightArea.removeFromBottom (42.0f);
    const auto slots = computeSlotRects (rightArea.reduced (20.0f, 22.0f), static_cast<int> (chain.size()));
    for (size_t i = 0; i < chain.size(); ++i)
    {
        if (slots[i].contains (pos))
        {
            selectedPedalType = chain[i];
            syncPedalEditorControls();
            repaint();
            return;
        }
    }
}

void MiAmpAudioProcessorEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (viewMode != ViewMode::Pedals) return;
    const auto area = juce::Rectangle<float> (0.0f, kBarH,
                                              static_cast<float> (getWidth()),
                                              static_cast<float> (getHeight()) - kBarH);
    auto chain = audioProcessor.getActiveChain();
    auto rightArea = area.withLeft (area.getWidth() * 0.45f).reduced (16.0f, 18.0f);
    rightArea.removeFromBottom (42.0f);
    const auto slots = computeSlotRects (rightArea.reduced (20.0f, 22.0f), static_cast<int> (chain.size()));
    for (size_t i = 0; i < chain.size(); ++i)
    {
        if (slots[i].contains (e.position))
        {
            if (auto* p = audioProcessor.apvts.getParameter (pedalBypassParamId (chain[i])))
            {
                const bool nxt = p->getValue() < 0.5f;
                p->setValueNotifyingHost (nxt ? 1.0f : 0.0f);
                showHud (pedalName (chain[i]) + (nxt ? ": ON" : ": BYPASS"));
                repaint();
                return;
            }
        }
    }
}
