#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AmpCatalogSpecs.h"

namespace miamp_rig
{
inline void setParamPlain (juce::AudioProcessorValueTreeState& apvts, const char* id, float plainVal)
{
    if (id == nullptr) return;
    if (auto* p = apvts.getParameter (id))
    {
        const float norm = p->convertTo0to1 (plainVal);
        p->beginChangeGesture();
        p->setValueNotifyingHost (norm);
        p->endChangeGesture();
    }
}

//==============================================================================
// Interactive rotating knob sprite positioned over any amp head panel
class RigHeadKnob final : public juce::Component
{
public:
    RigHeadKnob (juce::AudioProcessorValueTreeState& s,
                 const AmpKnobSpec& specIn,
                 juce::Image spriteIn,
                 std::function<void(const juce::String&)> onHud)
        : apvts (s), spec (specIn), sprite (std::move (spriteIn)), hudCb (std::move (onHud))
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        if (auto* p = apvts.getParameter (spec.paramId))
            defaultVal = p->convertFrom0to1 (p->getDefaultValue());
    }

    void paint (juce::Graphics& g) override
    {
        if (! sprite.isValid() || spec.paramId == nullptr) return;
        const auto* raw = apvts.getRawParameterValue (spec.paramId);
        const float val = raw ? raw->load() : 50.0f;
        const auto* param = apvts.getParameter (spec.paramId);
        const float norm = param ? param->convertTo0to1 (val) : juce::jlimit (0.0f, 1.0f, val * 0.01f);
        const float deg = -132.0f + norm * 264.0f;
        const float rad = juce::degreesToRadians (deg);

        const float w = static_cast<float> (getWidth());
        const float h = static_cast<float> (getHeight());
        const float diam = juce::jmin (w, h);
        const float cx = w * 0.5f, cy = h * 0.5f;

        // Subtle contact shadow
        g.setColour (juce::Colour::fromRGBA (0, 0, 0, 88));
        g.fillEllipse (cx - diam * 0.45f, cy - diam * 0.40f + 1.5f, diam * 0.90f, diam * 0.90f);

        auto tr = juce::AffineTransform::translation (-sprite.getWidth() * 0.5f, -sprite.getHeight() * 0.5f)
                      .scaled (diam / static_cast<float> (sprite.getWidth()),
                               diam / static_cast<float> (sprite.getHeight()))
                      .rotated (rad)
                      .translated (cx, cy);
        g.drawImageTransformed (sprite, tr, false);

        if (isMouseOverOrDragging())
        {
            g.setColour (juce::Colour::fromRGBA (191, 252, 225, 165));
            g.drawEllipse (cx - diam * 0.48f, cy - diam * 0.48f, diam * 0.96f, diam * 0.96f, 1.3f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (spec.paramId == nullptr) return;
        dragging = true;
        dragStartY = e.y;
        dragStartVal = apvts.getRawParameterValue (spec.paramId)->load();
        if (auto* p = apvts.getParameter (spec.paramId))
            p->beginChangeGesture();
        reportHud (dragStartVal);
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging || spec.paramId == nullptr) return;
        const float sens = e.mods.isShiftDown() ? 0.14f : 0.45f;
        const float newVal = juce::jlimit (0.0f, 100.0f, dragStartVal + static_cast<float> (dragStartY - e.y) * sens);
        if (auto* p = apvts.getParameter (spec.paramId))
            p->setValueNotifyingHost (p->convertTo0to1 (newVal));
        reportHud (newVal);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragging && spec.paramId != nullptr)
        {
            dragging = false;
            if (auto* p = apvts.getParameter (spec.paramId))
                p->endChangeGesture();
            repaint();
        }
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setParamPlain (apvts, spec.paramId, defaultVal);
        reportHud (defaultVal);
        repaint();
    }

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
    {
        if (spec.paramId == nullptr) return;
        const float cur = apvts.getRawParameterValue (spec.paramId)->load();
        const float nv = juce::jlimit (0.0f, 100.0f, cur + wheel.deltaY * 14.0f);
        setParamPlain (apvts, spec.paramId, nv);
        reportHud (nv);
        repaint();
    }

private:
    void reportHud (float v)
    {
        if (hudCb)
            hudCb (juce::String (spec.label) + ": " + juce::String (juce::roundToInt (v)) + "%");
    }

    juce::AudioProcessorValueTreeState& apvts;
    AmpKnobSpec spec;
    juce::Image sprite;
    std::function<void(const juce::String&)> hudCb;
    float defaultVal = 50.0f;
    float dragStartVal = 50.0f;
    int dragStartY = 0;
    bool dragging = false;
};

//==============================================================================
// Interactive toggle / rocker / push / slide switch over amp head panel
class RigHeadSwitch final : public juce::Component
{
public:
    RigHeadSwitch (juce::AudioProcessorValueTreeState& s,
                   const AmpSwitchSpec& specIn,
                   juce::Image togUp, juce::Image togDn,
                   std::function<void(const juce::String&)> onHud)
        : apvts (s), spec (specIn), imgUp (std::move (togUp)), imgDn (std::move (togDn)), hudCb (std::move (onHud))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float rawVal = spec.paramId ? apvts.getRawParameterValue (spec.paramId)->load() : 0.0f;
        const bool on = spec.invertLogic ? (rawVal < 0.5f) : (rawVal >= 0.5f);
        const auto& img = on ? imgUp : imgDn;

        if (img.isValid())
        {
            g.drawImage (img, b, juce::RectanglePlacement::centred);
        }
        else
        {
            g.setColour (on ? juce::Colour::fromRGB (110, 232, 192) : juce::Colour::fromRGB (72, 74, 80));
            g.fillEllipse (b.reduced (2.0f));
        }

        if (isMouseOver())
        {
            g.setColour (juce::Colour::fromRGBA (191, 252, 225, 165));
            g.drawRoundedRectangle (b.reduced (1.0f), 4.0f, 1.1f);
        }
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (spec.paramId == nullptr) return;
        const float cur = apvts.getRawParameterValue (spec.paramId)->load();
        float nextVal = 0.0f;
        juce::String stateLabel;

        switch (spec.action)
        {
            case AmpSwitchAction::BoolParam:
            {
                const bool nextBool = (cur < 0.5f);
                nextVal = nextBool ? 1.0f : 0.0f;
                const bool logicOn = spec.invertLogic ? ! nextBool : nextBool;
                stateLabel = logicOn ? "ON" : "OFF";
                break;
            }
            case AmpSwitchAction::ChannelCycle3:
            {
                const int ic = (static_cast<int> (std::lround (cur)) + 1) % 3;
                nextVal = static_cast<float> (ic);
                stateLabel = (ic == 0 ? "CLEAN" : (ic == 1 ? "VINTAGE" : "MODERN"));
                break;
            }
            case AmpSwitchAction::ChannelToggle01:
            {
                const int ic = static_cast<int> (std::lround (cur));
                const int nxt = (ic == 0) ? 2 : 0;
                nextVal = static_cast<float> (nxt);
                stateLabel = (nxt == 0 ? "CLEAN" : "LEAD/DIRTY");
                break;
            }
            case AmpSwitchAction::VoiceCycle3:
            {
                const int iv = (static_cast<int> (std::lround (cur)) + 1) % 3;
                nextVal = static_cast<float> (iv);
                stateLabel = (iv == 0 ? "VINTAGE" : (iv == 1 ? "BE" : "HBE"));
                break;
            }
        }

        setParamPlain (apvts, spec.paramId, nextVal);
        if (hudCb)
            hudCb (juce::String (spec.label) + ": " + stateLabel);
        repaint();
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    AmpSwitchSpec spec;
    juce::Image imgUp, imgDn;
    std::function<void(const juce::String&)> hudCb;
};

//==============================================================================
// Interactive linear EQ fader over PRO-450 / M-900 rack head panels
class RigHeadSlider final : public juce::Component
{
public:
    RigHeadSlider (juce::AudioProcessorValueTreeState& s,
                   const AmpSliderSpec& specIn,
                   juce::Image capIn,
                   std::function<void(const juce::String&)> onHud)
        : apvts (s), spec (specIn), capImg (std::move (capIn)), hudCb (std::move (onHud))
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float cx = b.getCentreX();
        g.setColour (juce::Colour::fromRGBA (10, 12, 14, 195));
        g.fillRoundedRectangle (cx - 1.5f, b.getY() + 2.0f, 3.0f, b.getHeight() - 4.0f, 1.5f);

        const float val = spec.paramId ? apvts.getRawParameterValue (spec.paramId)->load() : 50.0f;
        const float norm = juce::jlimit (0.0f, 1.0f, val * 0.01f);
        const float capH = juce::jmax (7.0f, b.getWidth() * 0.62f);
        const float cy = (b.getY() + capH * 0.5f) + (1.0f - norm) * juce::jmax (1.0f, b.getHeight() - capH);
        const auto cr = juce::Rectangle<float> (cx - b.getWidth() * 0.45f, cy - capH * 0.5f, b.getWidth() * 0.90f, capH);
        if (capImg.isValid())
            g.drawImage (capImg, cr, juce::RectanglePlacement::stretchToFit);
        else
        {
            g.setColour (juce::Colour::fromRGB (225, 228, 232));
            g.fillRoundedRectangle (cr, 2.0f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { updateFromY (e.y); }
    void mouseDrag (const juce::MouseEvent& e) override { updateFromY (e.y); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setParamPlain (apvts, spec.paramId, 50.0f);
        if (hudCb) hudCb (juce::String (spec.label) + ": 0.0 dB");
        repaint();
    }

private:
    void updateFromY (int yPx)
    {
        const float h = static_cast<float> (juce::jmax (12, getHeight()));
        const float norm = juce::jlimit (0.0f, 1.0f, 1.0f - static_cast<float> (yPx) / h);
        const float val = norm * 100.0f;
        setParamPlain (apvts, spec.paramId, val);
        if (hudCb)
        {
            const float db = (norm - 0.5f) * 20.0f;
            hudCb (juce::String (spec.label) + ": " + (db >= 0.0f ? "+" : "") + juce::String (db, 1) + " dB");
        }
        repaint();
    }

    juce::AudioProcessorValueTreeState& apvts;
    AmpSliderSpec spec;
    juce::Image capImg;
    std::function<void(const juce::String&)> hudCb;
};

//==============================================================================
// Bare draggable Microphone Body (MI-57 / MI-87) in front of cabinet grille
class RigMicBody final : public juce::Component
{
public:
    RigMicBody (juce::AudioProcessorValueTreeState& s,
                bool isCondenser87,
                juce::Image imgIn,
                std::function<void(const juce::String&)> onHud,
                std::function<void()> onMoved)
        : apvts (s), is87 (isCondenser87), micImg (std::move (imgIn)), hudCb (std::move (onHud)), movedCb (std::move (onMoved))
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    }

    void setCabGrilleScreenRect (juce::Rectangle<float> r) { grilleScreen = r; }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const bool on = apvts.getRawParameterValue (is87 ? MiAmpParams::mic2On : MiAmpParams::mic1On)->load() >= 0.5f;

        // Soft ambient drop shadow beneath bare mic body
        g.setColour (juce::Colour::fromRGBA (0, 0, 0, on ? 95 : 40));
        g.fillEllipse (b.reduced (b.getWidth() * 0.14f, b.getHeight() * 0.18f).translated (2.0f, 5.0f));

        g.setOpacity (on ? 1.0f : 0.36f);
        if (micImg.isValid())
            g.drawImage (micImg, b, juce::RectanglePlacement::centred);
        g.setOpacity (1.0f);

        if (isMouseOverOrDragging())
        {
            g.setColour (juce::Colour::fromRGBA (191, 252, 225, 195));
            g.drawRoundedRectangle (b.reduced (1.5f), 8.0f, 1.4f);
            const auto tag = b.withHeight (16.0f).withY (b.getBottom() - 16.0f);
            g.setColour (juce::Colour::fromRGBA (22, 26, 24, 210));
            g.fillRoundedRectangle (tag, 4.0f);
            g.setColour (juce::Colour::fromRGB (240, 252, 245));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 9.5f, juce::Font::bold)));
            g.drawText (is87 ? "MI-87" : "MI-57", tag, juce::Justification::centred);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragging = true;
        downPos = e.getScreenPosition();
        startPx = apvts.getRawParameterValue (is87 ? MiAmpParams::mic2PosX : MiAmpParams::mic1PosX)->load();
        startPy = apvts.getRawParameterValue (is87 ? MiAmpParams::mic2PosY : MiAmpParams::mic1PosY)->load();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! dragging) return;
        const auto d = e.getScreenPosition() - downPos;
        const float gw = juce::jmax (60.0f, grilleScreen.getWidth());
        const float gh = juce::jmax (60.0f, grilleScreen.getHeight());
        const float nx = juce::jlimit (0.0f, 100.0f, startPx + (static_cast<float> (d.x) / gw) * 100.0f);
        const float ny = juce::jlimit (0.0f, 100.0f, startPy + (static_cast<float> (d.y) / gh) * 100.0f);
        setParamPlain (apvts, is87 ? MiAmpParams::mic2PosX : MiAmpParams::mic1PosX, nx);
        setParamPlain (apvts, is87 ? MiAmpParams::mic2PosY : MiAmpParams::mic1PosY, ny);
        if (hudCb)
            hudCb ((is87 ? juce::String ("MI-87 ") : juce::String ("MI-57 "))
                   + "X:" + juce::String (juce::roundToInt (nx)) + " Y:" + juce::String (juce::roundToInt (ny)));
        if (movedCb) movedCb();
    }

    void mouseUp (const juce::MouseEvent&) override { dragging = false; }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setParamPlain (apvts, is87 ? MiAmpParams::mic2PosX : MiAmpParams::mic1PosX, is87 ? 60.0f : 42.0f);
        setParamPlain (apvts, is87 ? MiAmpParams::mic2PosY : MiAmpParams::mic1PosY, 50.0f);
        if (movedCb) movedCb();
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    bool is87 = false;
    juce::Image micImg;
    std::function<void(const juce::String&)> hudCb;
    std::function<void()> movedCb;
    juce::Rectangle<float> grilleScreen { 0.0f, 0.0f, 240.0f, 240.0f };
    juce::Point<int> downPos;
    float startPx = 50.0f, startPy = 50.0f;
    bool dragging = false;
};

//==============================================================================
// Visual Amp Head & Cabinet Selector Overlay (Glass style)
class RigBrowserOverlay final : public juce::Component
{
public:
    enum class Mode { Heads, Cabs };

    RigBrowserOverlay (MiAmpAudioProcessor& proc,
                       std::function<void()> onSelectChanged,
                       std::function<void()> onClose)
        : processor (proc), selectCb (std::move (onSelectChanged)), closeCb (std::move (onClose))
    {
        btnClose.setComponentID ("popoverClose");
        btnClose.onClick = [this] { if (closeCb) closeCb(); };
        addAndMakeVisible (btnClose);

        btnLoadWav.onClick = [this] { openWavChooser(); };
        addAndMakeVisible (btnLoadWav);
    }

    void openMode (Mode m)
    {
        mode = m;
        setVisible (true);
        toFront (true);
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        g.setColour (juce::Colour::fromRGBA (12, 14, 13, 152));
        g.fillRect (b);

        const auto card = b.reduced (b.getWidth() * 0.09f, b.getHeight() * 0.08f);
        juce::ColourGradient grad (juce::Colour::fromRGBA (239, 222, 183, 86), card.getX(), card.getY(),
                                   juce::Colour::fromRGBA (42, 44, 38, 226), card.getRight(), card.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (card, 22.0f);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 175));
        g.drawRoundedRectangle (card, 22.0f, 1.5f);

        const bool bass = processor.isBassMode();
        const juce::String title = (mode == Mode::Heads)
            ? (bass ? "SELECT BASS AMP HEAD" : "SELECT GUITAR AMP HEAD")
            : (bass ? "SELECT BASS CABINET / IR" : "SELECT GUITAR CABINET / IR");

        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        g.setFont (juce::Font (juce::FontOptions ("Arial", 20.0f, juce::Font::bold)));
        g.drawText (title, card.getX() + 26.0f, card.getY() + 16.0f, card.getWidth() - 190.0f, 32.0f,
                    juce::Justification::centredLeft);

        const int curIdx = getCurrentIndex();
        const auto cells = computeCells (card);

        for (size_t i = 0; i < cells.size(); ++i)
        {
            const auto& r = cells[i];
            const bool sel = (static_cast<int> (i) == curIdx);
            g.setColour (sel ? juce::Colour::fromRGBA (164, 217, 196, 78) : juce::Colour::fromRGBA (255, 255, 255, 18));
            g.fillRoundedRectangle (r, 12.0f);
            g.setColour (sel ? juce::Colour::fromRGB (191, 252, 225) : juce::Colour::fromRGBA (255, 243, 210, 68));
            g.drawRoundedRectangle (r, 12.0f, sel ? 1.8f : 1.0f);

            const auto imgBox = r.reduced (10.0f, 8.0f).withTrimmedBottom (34.0f);
            juce::Image thumb;
            juce::String name, sub;
            bool isOff = false, isWav = false;

            if (mode == Mode::Heads)
            {
                const auto& list = bass ? getBassHeadSpecs() : getGuitarHeadSpecs();
                const auto& sp = list[i];
                thumb = juce::ImageCache::getFromMemory (sp.chassisData, sp.chassisSize);
                name = sp.displayName;
                sub  = sp.subtitle;
            }
            else
            {
                const auto& list = bass ? getBassCabSpecs() : getGuitarCabSpecs();
                const auto& sp = list[i];
                if (sp.imageData != nullptr && sp.imageSize > 0)
                    thumb = juce::ImageCache::getFromMemory (sp.imageData, sp.imageSize);
                name = sp.displayName;
                sub  = sp.isCustomIr
                    ? (processor.getCustomCabIRName (bass).isNotEmpty() ? processor.getCustomCabIRName (bass) : juce::String (sp.subtitle))
                    : juce::String (sp.subtitle);
                isOff = sp.isOff;
                isWav = sp.isCustomIr;
            }

            if (thumb.isValid())
            {
                g.setOpacity (1.0f);
                g.drawImage (thumb, imgBox, juce::RectanglePlacement::centred);
            }
            else if (isOff)
            {
                g.setColour (juce::Colour::fromRGBA (250, 243, 227, 130));
                g.setFont (juce::Font (juce::FontOptions ("Arial", 18.0f, juce::Font::bold)));
                g.drawText ("BYPASS / DI", imgBox, juce::Justification::centred);
            }

            if (isWav)
            {
                g.setColour (juce::Colour::fromRGBA (142, 208, 180, 210));
                g.setFont (juce::Font (juce::FontOptions ("Arial", 11.0f, juce::Font::bold)));
                g.drawText (".WAV IR", imgBox.withHeight (18.0f), juce::Justification::topRight);
            }

            g.setColour (juce::Colour::fromRGB (250, 243, 227));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 13.5f, juce::Font::bold)));
            g.drawText (name, r.getX() + 10.0f, r.getBottom() - 36.0f, r.getWidth() - 20.0f, 18.0f,
                        juce::Justification::centred, true);

            g.setColour (juce::Colour::fromRGBA (250, 243, 227, 175));
            g.setFont (juce::Font (juce::FontOptions ("Arial", 10.5f, juce::Font::plain)));
            g.drawText (sub, r.getX() + 10.0f, r.getBottom() - 19.0f, r.getWidth() - 20.0f, 14.0f,
                        juce::Justification::centred, true);
        }
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        const auto card = b.reduced (b.getWidth() * 0.09f, b.getHeight() * 0.08f);
        btnClose.setBounds (juce::roundToInt (card.getRight() - 44.0f), juce::roundToInt (card.getY() + 14.0f), 30, 30);
        btnLoadWav.setVisible (mode == Mode::Cabs);
        btnLoadWav.setBounds (juce::roundToInt (card.getRight() - 210.0f), juce::roundToInt (card.getY() + 16.0f), 152, 28);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto card = b.reduced (b.getWidth() * 0.09f, b.getHeight() * 0.08f);
        if (! card.contains (e.position))
        {
            if (closeCb) closeCb();
            return;
        }

        const auto cells = computeCells (card);
        const bool bass = processor.isBassMode();
        for (size_t i = 0; i < cells.size(); ++i)
        {
            if (cells[i].contains (e.position))
            {
                if (mode == Mode::Heads)
                {
                    setParamPlain (processor.apvts, bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel, static_cast<float> (i));
                    if (selectCb) selectCb();
                    if (closeCb) closeCb();
                }
                else
                {
                    const auto& list = bass ? getBassCabSpecs() : getGuitarCabSpecs();
                    if (list[i].isCustomIr && processor.getCustomCabIRName (bass).isEmpty())
                    {
                        setParamPlain (processor.apvts, bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab, static_cast<float> (i));
                        openWavChooser();
                    }
                    else
                    {
                        setParamPlain (processor.apvts, bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab, static_cast<float> (i));
                        if (selectCb) selectCb();
                        if (closeCb) closeCb();
                    }
                }
                return;
            }
        }
    }

    void openWavChooser()
    {
        const bool bass = processor.isBassMode();
        fileChooser = std::make_unique<juce::FileChooser> ("Select Impulse Response (.wav)", juce::File(), "*.wav");
        fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this, bass] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile() && processor.loadCustomCabIR (bass, f))
                {
                    const int customIdx = bass ? 7 : 8;
                    setParamPlain (processor.apvts, bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab, static_cast<float> (customIdx));
                    if (selectCb) selectCb();
                    repaint();
                }
            });
    }

private:
    int getCurrentIndex() const
    {
        const bool bass = processor.isBassMode();
        const char* id = (mode == Mode::Heads)
            ? (bass ? MiAmpParams::bassHeadModel : MiAmpParams::gtrHeadModel)
            : (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab);
        return static_cast<int> (std::lround (processor.apvts.getRawParameterValue (id)->load()));
    }

    std::vector<juce::Rectangle<float>> computeCells (juce::Rectangle<float> card) const
    {
        const bool bass = processor.isBassMode();
        const int count = (mode == Mode::Heads) ? 9 : (bass ? 9 : 10);
        const int cols = 3;
        const int rows = (count + cols - 1) / cols;
        auto area = card.reduced (24.0f, 18.0f).withTrimmedTop (42.0f);
        const float gap = 14.0f;
        const float cw = (area.getWidth() - gap * (cols - 1)) / cols;
        const float ch = (area.getHeight() - gap * (rows - 1)) / rows;
        std::vector<juce::Rectangle<float>> out;
        for (int i = 0; i < count; ++i)
        {
            const int r = i / cols, c = i % cols;
            out.emplace_back (area.getX() + c * (cw + gap), area.getY() + r * (ch + gap), cw, ch);
        }
        return out;
    }

    MiAmpAudioProcessor& processor;
    Mode mode = Mode::Heads;
    std::function<void()> selectCb, closeCb;
    juce::TextButton btnClose { juce::String::fromUTF8 (u8"\u00d7") };
    juce::TextButton btnLoadWav { "LOAD .WAV IR..." };
    std::unique_ptr<juce::FileChooser> fileChooser;
};
} // namespace miamp_rig
