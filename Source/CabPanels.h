#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AmpCatalogSpecs.h"
#include "MiAmpGlassTheme.h"
#include "RigStageComponents.h"
#include "StudioFxModal.h"
#include <cmath>
#include <functional>
#include <vector>

namespace miamp_rig
{
//==============================================================================
// Mirrored Glass Side Arrow Button (< / >) flanking the Amp Head
class HeadSideArrowButton final : public juce::Button
{
public:
    explicit HeadSideArrowButton (bool rightSide)
        : juce::Button (rightSide ? "headNext" : "headPrev"), isRight (rightSide)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setTipText (const juce::String& t) { tip = t; }
    const juce::String& getTipText() const noexcept { return tip; }
    bool isRightSide() const noexcept { return isRight; }

    void paintButton (juce::Graphics& g, bool highlighted, bool down) override
    {
        const auto b = getLocalBounds().toFloat();
        const float sc = juce::jmin (b.getWidth() / 30.0f, b.getHeight() / 66.0f);

        auto drawR = b.reduced (1.0f);
        if (down)
            drawR = drawR.reduced (1.2f);

        const float rad = drawR.getWidth() * 0.5f;

        if (highlighted)
        {
            g.setColour (juce::Colour::fromRGBA (166, 223, 201, down ? 92 : 68));
            g.fillRoundedRectangle (drawR, rad);
            g.setColour (juce::Colour::fromRGB (166, 223, 201));
            g.drawRoundedRectangle (drawR, rad, 1.4f);
        }
        else
        {
            miamp_theme::paintFrostedGlassPanel (g, drawR, rad, 1.0f, 1.25f);
        }

        // Single clean slim chevron
        const float vw = drawR.getWidth() * 0.42f;
        const float vh = drawR.getWidth() * 0.72f;
        const float cx = drawR.getCentreX();
        const float cy = drawR.getCentreY();
        const float dx = isRight ? (vw * 0.42f) : (-vw * 0.42f);

        juce::Path chev;
        chev.startNewSubPath (cx - dx, cy - vh * 0.5f);
        chev.lineTo        (cx + dx, cy);
        chev.lineTo        (cx - dx, cy + vh * 0.5f);

        g.setColour (highlighted ? juce::Colour::fromRGB (225, 255, 242) : juce::Colour::fromRGB (255, 250, 240));
        g.strokePath (chev, juce::PathStrokeType (juce::jmax (1.8f, 2.3f * sc),
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

private:
    bool isRight = false;
    juce::String tip;
};

//==============================================================================
// Helpers: dimmed cab (only used on stage rig), plus dedicated .WAV IR & DIRECT DI icons
inline juce::Image makeDimmedCabImage (const juce::Image& src, float brightness = 0.42f)
{
    if (! src.isValid()) return {};
    auto dim = src.convertedToFormat (juce::Image::ARGB);
    dim.desaturate();
    juce::Image::BitmapData bd (dim, juce::Image::BitmapData::readWrite);
    const int mult = juce::roundToInt (juce::jlimit (0.0f, 1.0f, brightness) * 256.0f);
    for (int y = 0; y < bd.height; ++y)
    {
        auto* row = bd.getLinePointer (y);
        for (int x = 0; x < bd.width; ++x)
        {
            auto* px = row + x * bd.pixelStride;
            px[juce::PixelARGB::indexR] = static_cast<juce::uint8> ((px[juce::PixelARGB::indexR] * mult) >> 8);
            px[juce::PixelARGB::indexG] = static_cast<juce::uint8> ((px[juce::PixelARGB::indexG] * mult) >> 8);
            px[juce::PixelARGB::indexB] = static_cast<juce::uint8> ((px[juce::PixelARGB::indexB] * mult) >> 8);
        }
    }
    return dim;
}

inline void paintCabOffSign (juce::Graphics& g, juce::Point<float> centre, float diam)
{
    const float r = diam * 0.375f;
    const float stroke = juce::jmax (2.5f, diam * 0.105f);
    const float d = r * 0.7071f;
    g.setColour (juce::Colour::fromRGBA (0, 0, 0, 175));
    g.drawEllipse (centre.x - r, centre.y - r + 1.5f, r * 2.0f, r * 2.0f, stroke + 1.5f);
    g.setColour (juce::Colour::fromRGB (239, 68, 68));
    g.drawEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f, stroke);
    g.drawLine (centre.x - d, centre.y - d, centre.x + d, centre.y + d, stroke);
}

inline void paintWavIrIcon (juce::Graphics& g, juce::Rectangle<float> box)
{
    const float sz = juce::jmin (box.getWidth(), box.getHeight());
    const auto r = juce::Rectangle<float> (sz, sz).withCentre (box.getCentre());
    const float u = sz / 64.0f;

    const auto frame = juce::Rectangle<float> (r.getX() + 6.0f * u, r.getY() + 10.0f * u, 52.0f * u, 44.0f * u);
    g.setColour (juce::Colour::fromRGBA (166, 223, 201, 36));
    g.fillRoundedRectangle (frame, 10.0f * u);
    g.setColour (juce::Colour::fromRGB (166, 223, 201));
    g.drawRoundedRectangle (frame, 10.0f * u, juce::jmax (1.4f, 2.2f * u));

    static constexpr float pts[11][2] = {
        {13, 30}, {18, 30}, {21, 18}, {25, 42}, {29, 15}, {33, 45}, {37, 21}, {41, 38}, {44, 27}, {47, 30}, {51, 30}
    };
    juce::Path wave;
    wave.startNewSubPath (r.getX() + pts[0][0] * u, r.getY() + pts[0][1] * u);
    for (int i = 1; i < 11; ++i)
        wave.lineTo (r.getX() + pts[i][0] * u, r.getY() + pts[i][1] * u);
    g.setColour (juce::Colour::fromRGB (250, 243, 227));
    g.strokePath (wave, juce::PathStrokeType (juce::jmax (1.6f, 2.5f * u), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colour::fromRGB (166, 223, 201));
    g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.5f, 8.6f * u), juce::Font::bold)));
    g.drawText (".WAV IR", frame.getX(), frame.getBottom() - 14.0f * u, frame.getWidth(), 11.0f * u, juce::Justification::centred);
}

inline void paintDirectDiIcon (juce::Graphics& g, juce::Rectangle<float> box)
{
    const float sz = juce::jmin (box.getWidth(), box.getHeight());
    const auto r = juce::Rectangle<float> (sz, sz).withCentre (box.getCentre());
    const float u = sz / 64.0f;
    const auto cx = r.getX() + 32.0f * u;
    const auto cy = r.getY() + 27.0f * u;
    const float rad = 18.0f * u;
    const float d = rad * 0.7071f;
    const float sw = juce::jmax (2.0f, 3.4f * u);

    g.setColour (juce::Colour::fromRGBA (239, 68, 68, 32));
    g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
    g.setColour (juce::Colour::fromRGB (239, 68, 68));
    g.drawEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f, sw);
    g.drawLine (cx - d, cy - d, cx + d, cy + d, sw);

    g.setColour (juce::Colour::fromRGB (252, 165, 165));
    g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.5f, 8.4f * u), juce::Font::bold)));
    g.drawText ("DIRECT DI", r.getX(), r.getY() + 49.0f * u, r.getWidth(), 12.0f * u, juce::Justification::centred);
}

//==============================================================================
// Unified Left Frosted-Glass Cabinet & Dual-Mic Panel: [ CAJAS | MICRÓFONOS ]
class CabPickerPanel final : public juce::Component
{
public:
    CabPickerPanel (MiAmpAudioProcessor& proc,
                    std::function<void()> onChanged,
                    std::function<void()> onClose,
                    std::function<void(bool)> onModeChanged,
                    std::function<void()> onOpenWav)
        : processor (proc),
          changedCb (std::move (onChanged)),
          closeCb (std::move (onClose)),
          modeCb (std::move (onModeChanged)),
          wavCb (std::move (onOpenWav))
    {
        btnClose.setComponentID ("popoverClose");
        btnClose.onClick = [this] { if (closeCb) closeCb(); };
        addAndMakeVisible (btnClose);

        btnTabCabs.setComponentID ("cabTabCabs");
        btnTabMics.setComponentID ("cabTabMics");
        btnTabCabs.setClickingTogglesState (false);
        btnTabMics.setClickingTogglesState (false);
        btnTabCabs.onClick = [this] { setModeAndNotify (false); };
        btnTabMics.onClick = [this] { setModeAndNotify (true); };
        addAndMakeVisible (btnTabCabs);
        addAndMakeVisible (btnTabMics);

        btnHeroWav.onClick = [this] { if (wavCb) wavCb(); };
        addAndMakeVisible (btnHeroWav);

        btnBackCabs.onClick = [this] { setModeAndNotify (false); };
        addChildComponent (btnBackCabs);

        auto bindTog = [this] (juce::TextButton& b, const char* id)
        {
            b.setClickingTogglesState (true);
            b.onClick = [this, &b, id]
            {
                setParamPlain (processor.apvts, id, b.getToggleState() ? 1.0f : 0.0f);
                if (changedCb) changedCb();
                refresh();
            };
            addChildComponent (b);
        };
        bindTog (btnMic1On, MiAmpParams::mic1On);
        bindTog (btnMic1Phase, MiAmpParams::mic1Phase);
        bindTog (btnMic2On, MiAmpParams::mic2On);
        bindTog (btnMic2Phase, MiAmpParams::mic2Phase);

        auto bindDist = [this] (juce::TextButton& b, const char* id)
        {
            b.onClick = [this, id]
            {
                const int cur = static_cast<int> (std::lround (processor.apvts.getRawParameterValue (id)->load()));
                setParamPlain (processor.apvts, id, static_cast<float> ((cur + 1) % 3));
                if (changedCb) changedCb();
                refresh();
            };
            addChildComponent (b);
        };
        bindDist (btnMic1Dist, MiAmpParams::mic1Dist);
        bindDist (btnMic2Dist, MiAmpParams::mic2Dist);

        micMixKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::cabMicMix, "MIC MIX", "%");
        roomKnob   = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::cabRoom,   "ROOM", "%");
        addChildComponent (*micMixKnob);
        addChildComponent (*roomKnob);

        syncVisibility();
    }

    void setMicZoom (bool zoom)
    {
        if (cabMicZoom == zoom) return;
        cabMicZoom = zoom;
        syncVisibility();
        resized();
        repaint();
    }

    bool isMicZoom() const noexcept { return cabMicZoom; }

    void refresh()
    {
        static constexpr const char* kDistTags[3] = { "CLOSE", "MED", "FAR" };
        btnMic1On.setToggleState    (processor.apvts.getRawParameterValue (MiAmpParams::mic1On)->load() >= 0.5f, juce::dontSendNotification);
        btnMic1Phase.setToggleState (processor.apvts.getRawParameterValue (MiAmpParams::mic1Phase)->load() >= 0.5f, juce::dontSendNotification);
        btnMic1Dist.setButtonText   (kDistTags[juce::jlimit (0, 2, static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::mic1Dist)->load())))]);

        btnMic2On.setToggleState    (processor.apvts.getRawParameterValue (MiAmpParams::mic2On)->load() >= 0.5f, juce::dontSendNotification);
        btnMic2Phase.setToggleState (processor.apvts.getRawParameterValue (MiAmpParams::mic2Phase)->load() >= 0.5f, juce::dontSendNotification);
        btnMic2Dist.setButtonText   (kDistTags[juce::jlimit (0, 2, static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::mic2Dist)->load())))]);

        syncVisibility();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float s  = b.getWidth() / 420.0f;
        const float sy = b.getHeight() / 716.0f;

        // 1. Canonical Frosted Glass Panel ("Vidrio Claro")
        miamp_theme::paintFrostedGlassPanel (g, b, 20.0f * s);

        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& fallbackCab = cabs[static_cast<size_t> (bass ? 6 : 7)];
        const auto& visualCab = cSpec.isOff ? fallbackCab : cSpec;

        // 2. Header Title + Pill Switcher Shell [ CAJAS | MICRÓFONOS ]
        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (10.5f, 13.0f * s), juce::Font::bold)));
        const juce::String ttl = bass ? juce::String::fromUTF8 (u8"CAJAS \u00b7 BAJO")
                                      : juce::String::fromUTF8 (u8"CAJAS \u00b7 GUITARRA");
        g.drawFittedText (ttl, juce::roundToInt (16.0f * s), juce::roundToInt (12.0f * s),
                          juce::roundToInt (152.0f * s), juce::roundToInt (24.0f * s),
                          juce::Justification::centredLeft, 1);

        const auto tabsPill = juce::Rectangle<float> (174.0f * s, 11.0f * s, 200.0f * s, 26.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 250, 236, 36));
        g.fillRoundedRectangle (tabsPill, 13.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 244, 214, 78));
        g.drawRoundedRectangle (tabsPill, 13.0f * s, 1.0f);

        // 3. Top Hero Box (NO < > arrows; clean light glass sub-panel)
        const auto heroR = computeHeroRect (b, s, sy);
        miamp_theme::paintGlassSubPanel (g, heroR, 15.0f * s);

        const auto heroImgBox = computeHeroStageRect (heroR, s, sy);
        if (cSpec.isOff)
        {
            paintDirectDiIcon (g, heroImgBox.reduced (26.0f * s, 18.0f * sy));
        }
        else if (cSpec.isCustomIr)
        {
            paintWavIrIcon (g, heroImgBox.reduced (26.0f * s, 18.0f * sy));
        }
        else if (visualCab.imageData != nullptr && visualCab.imageSize > 0)
        {
            auto im = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
            if (im.isValid())
            {
                const auto cabR = computeHeroCabRect (heroImgBox, visualCab);
                g.setOpacity (1.0f);
                g.drawImage (im, cabR, juce::RectanglePlacement::stretchToFit);

                if (cabMicZoom)
                    paintHeroMicPins (g, s, cabR, visualCab);
            }
        }

        // Hero metadata footer
        const juce::String heroName = cSpec.isOff
            ? juce::String ("SIN CAJA (DI)")
            : ((cSpec.isCustomIr && processor.getCustomCabIRName (bass).isNotEmpty())
                ? ("IR: " + processor.getCustomCabIRName (bass))
                : juce::String (cSpec.displayName));

        const float metaY = heroR.getBottom() - 48.0f * sy;
        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (11.0f, 13.5f * s), juce::Font::bold)));
        g.drawFittedText (heroName,
                          juce::roundToInt (heroR.getX() + 14.0f * s), juce::roundToInt (metaY),
                          juce::roundToInt (heroR.getWidth() - 166.0f * s), juce::roundToInt (18.0f * sy),
                          juce::Justification::centredLeft, 1);

        juce::String subText = cSpec.subtitle;
        if (cabMicZoom)
        {
            const int x1 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load());
            const int y1 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load());
            const int x2 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load());
            const int y2 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load());
            subText = "MI-57: " + juce::String (x1) + "%, " + juce::String (y1) + "%  \u00b7  MI-87: " + juce::String (x2) + "%, " + juce::String (y2) + "%";
        }
        g.setColour (cabMicZoom ? juce::Colour::fromRGB (166, 223, 201) : juce::Colour::fromRGBA (250, 243, 227, 200));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, 10.2f * s), cabMicZoom ? juce::Font::bold : juce::Font::plain)));
        g.drawFittedText (subText,
                          juce::roundToInt (heroR.getX() + 14.0f * s), juce::roundToInt (metaY + 19.0f * sy),
                          juce::roundToInt (heroR.getWidth() - 166.0f * s), juce::roundToInt (15.0f * sy),
                          juce::Justification::centredLeft, 1);

        // Counter pill ("CAJA X/10")
        const auto cntPill = juce::Rectangle<float> (heroR.getRight() - 146.0f * s, metaY + 7.0f * sy, 70.0f * s, 23.0f * sy);
        g.setColour (juce::Colour::fromRGBA (166, 223, 201, 46));
        g.fillRoundedRectangle (cntPill, cntPill.getHeight() * 0.5f);
        g.setColour (juce::Colour::fromRGBA (166, 223, 201, 145));
        g.drawRoundedRectangle (cntPill, cntPill.getHeight() * 0.5f, 1.0f);
        g.setColour (juce::Colour::fromRGB (220, 255, 240));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.5f, 9.5f * s), juce::Font::bold)));
        g.drawText ("CAJA " + juce::String (cabIdx + 1) + "/" + juce::String (static_cast<int> (cabs.size())),
                    cntPill, juce::Justification::centred);

        // 4. Lower Section: 5x2 Grid (CAJAS mode) OR Mic Controls Card (MICRÓFONOS mode)
        if (! cabMicZoom)
        {
            const auto cells = computeGridCells (b, s, sy);
            for (size_t i = 0; i < cells.size() && i < cabs.size(); ++i)
            {
                const auto& r = cells[i];
                const auto& item = cabs[i];
                const bool sel = (static_cast<int> (i) == cabIdx);
                const bool hov = (static_cast<int> (i) == hoveredCell);

                miamp_theme::paintGlassCard (g, r, 11.0f * s, sel, hov);

                const auto imgR = r.reduced (5.0f * s, 4.0f * sy).withTrimmedBottom (23.0f * sy);
                if (item.isOff)
                {
                    paintDirectDiIcon (g, imgR);
                }
                else if (item.isCustomIr)
                {
                    paintWavIrIcon (g, imgR);
                }
                else if (item.imageData != nullptr && item.imageSize > 0)
                {
                    auto im = juce::ImageCache::getFromMemory (item.imageData, item.imageSize);
                    if (im.isValid())
                    {
                        g.setOpacity (1.0f);
                        g.drawImage (im, imgR, juce::RectanglePlacement::centred);
                    }
                }

                g.setColour (sel ? juce::Colours::white : juce::Colour::fromRGB (250, 243, 227));
                g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.4f, 7.8f * s), juce::Font::bold)));
                g.drawFittedText (item.isOff ? "SIN CAJA (DI)" : item.displayName,
                                  juce::roundToInt (r.getX() + 3.0f * s),
                                  juce::roundToInt (r.getBottom() - 22.0f * sy),
                                  juce::roundToInt (r.getWidth() - 6.0f * s),
                                  juce::roundToInt (19.0f * sy),
                                  juce::Justification::centred, 2);
            }
        }
        else
        {
            const auto micsR = computeMicsCardRect (b, s, sy);
            miamp_theme::paintGlassSubPanel (g, micsR, 15.0f * s);

            g.setColour (juce::Colour::fromRGB (250, 243, 227));
            g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, 10.5f * s), juce::Font::bold)));
            g.drawText (juce::String::fromUTF8 (u8"POSICI\u00d3N Y MEZCLA DE MICR\u00d3FONOS"),
                        micsR.getX() + 13.0f * s, micsR.getY() + 10.0f * sy, 230.0f * s, 20.0f * sy,
                        juce::Justification::centredLeft);

            // Row containers & numbered badges (57 / 87)
            for (int row = 0; row < 2; ++row)
            {
                const auto rowR = computeMicRowRect (micsR, row, s, sy);
                miamp_theme::paintGlassCard (g, rowR, 10.0f * s, false, false);

                const float badgeSz = 22.0f * juce::jmin (s, sy);
                const auto badgeR = juce::Rectangle<float> (rowR.getX() + 8.0f * s,
                                                          rowR.getCentreY() - badgeSz * 0.5f,
                                                          badgeSz, badgeSz);
                const auto fill = (row == 0) ? juce::Colour::fromRGB (18, 34, 28) : juce::Colour::fromRGB (36, 28, 14);
                const auto rim  = (row == 0) ? juce::Colour::fromRGB (158, 246, 212) : juce::Colour::fromRGB (255, 208, 123);
                const auto txt  = (row == 0) ? juce::Colour::fromRGB (220, 255, 241) : juce::Colour::fromRGB (255, 242, 212);
                g.setColour (fill);
                g.fillEllipse (badgeR);
                g.setColour (rim);
                g.drawEllipse (badgeR, 1.8f);
                g.setColour (txt);
                g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.5f, 9.5f * s), juce::Font::bold)));
                g.drawText (row == 0 ? "57" : "87", badgeR, juce::Justification::centred);
            }
        }
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        const float s  = b.getWidth() / 420.0f;
        const float sy = b.getHeight() / 716.0f;

        btnClose.setBounds (juce::roundToInt (b.getWidth() - 38.0f * s), juce::roundToInt (11.0f * s),
                            juce::roundToInt (24.0f * s), juce::roundToInt (24.0f * s));

        btnTabCabs.setBounds (juce::roundToInt (177.0f * s), juce::roundToInt (13.0f * s),
                              juce::roundToInt (84.0f * s),  juce::roundToInt (22.0f * s));
        btnTabMics.setBounds (juce::roundToInt (264.0f * s), juce::roundToInt (13.0f * s),
                              juce::roundToInt (107.0f * s), juce::roundToInt (22.0f * s));

        const auto heroR = computeHeroRect (b, s, sy);
        const float metaY = heroR.getBottom() - 48.0f * sy;
        btnHeroWav.setBounds (juce::roundToInt (heroR.getRight() - 70.0f * s),
                              juce::roundToInt (metaY + 7.0f * sy),
                              juce::roundToInt (58.0f * s),
                              juce::roundToInt (23.0f * sy));

        const auto micsR = computeMicsCardRect (b, s, sy);
        btnBackCabs.setBounds (juce::roundToInt (micsR.getRight() - 134.0f * s),
                               juce::roundToInt (micsR.getY() + 9.0f * sy),
                               juce::roundToInt (122.0f * s),
                               juce::roundToInt (22.0f * sy));

        const auto r1 = computeMicRowRect (micsR, 0, s, sy);
        const auto r2 = computeMicRowRect (micsR, 1, s, sy);
        auto placeRowBtns = [&] (const juce::Rectangle<float>& rr, juce::TextButton& onB, juce::TextButton& distB, juce::TextButton& phB)
        {
            const float x0 = rr.getX() + 38.0f * s;
            const float avail = rr.getRight() - 8.0f * s - x0;
            const float gap = 6.0f * s;
            const float phW = 34.0f * s;
            const float rem = avail - phW - gap * 2.0f;
            const float onW = rem * 0.54f;
            const float dW  = rem * 0.46f;
            const float bh  = juce::jmin (24.0f * sy, rr.getHeight() - 8.0f * sy);
            const float by  = rr.getCentreY() - bh * 0.5f;
            onB.setBounds   (juce::roundToInt (x0), juce::roundToInt (by), juce::roundToInt (onW), juce::roundToInt (bh));
            distB.setBounds (juce::roundToInt (x0 + onW + gap), juce::roundToInt (by), juce::roundToInt (dW), juce::roundToInt (bh));
            phB.setBounds   (juce::roundToInt (x0 + onW + gap + dW + gap), juce::roundToInt (by), juce::roundToInt (phW), juce::roundToInt (bh));
        };
        placeRowBtns (r1, btnMic1On, btnMic1Dist, btnMic1Phase);
        placeRowBtns (r2, btnMic2On, btnMic2Dist, btnMic2Phase);

        const float knobY = r2.getBottom() + 6.0f * sy;
        const float knobH = juce::jmax (54.0f, micsR.getBottom() - knobY - 6.0f * sy);
        const float knobW = 78.0f * s;
        if (micMixKnob)
            micMixKnob->setBounds (juce::roundToInt (micsR.getCentreX() - knobW - 18.0f * s), juce::roundToInt (knobY),
                                   juce::roundToInt (knobW), juce::roundToInt (knobH));
        if (roomKnob)
            roomKnob->setBounds   (juce::roundToInt (micsR.getCentreX() + 18.0f * s), juce::roundToInt (knobY),
                                   juce::roundToInt (knobW), juce::roundToInt (knobH));
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        if (cabMicZoom)
        {
            if (hoveredCell != -1) { hoveredCell = -1; repaint(); }
            return;
        }
        const auto b = getLocalBounds().toFloat();
        const auto cells = computeGridCells (b, b.getWidth() / 420.0f, b.getHeight() / 716.0f);
        int hit = -1;
        for (size_t i = 0; i < cells.size(); ++i)
            if (cells[i].contains (e.position)) { hit = static_cast<int> (i); break; }
        if (hit != hoveredCell)
        {
            hoveredCell = hit;
            setMouseCursor (hit >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint();
        }
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoveredCell != -1) { hoveredCell = -1; repaint(); }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto b = getLocalBounds().toFloat();
        const float s  = b.getWidth() / 420.0f;
        const float sy = b.getHeight() / 716.0f;
        const bool bass = processor.isBassMode();

        if (cabMicZoom)
        {
            const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
            const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
                static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
            const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
            if (cSpec.isOff || cSpec.isCustomIr) return;

            const auto heroR = computeHeroRect (b, s, sy);
            const auto heroImgBox = computeHeroStageRect (heroR, s, sy);
            const auto cabR = computeHeroCabRect (heroImgBox, cSpec);
            const auto gr = computeHeroGrilleRect (cabR, cSpec);
            if (! gr.isEmpty() && heroImgBox.contains (e.position))
            {
                const auto p1 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f) * gr.getWidth(),
                                                    gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f) * gr.getHeight());
                const auto p2 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f) * gr.getWidth(),
                                                    gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f) * gr.getHeight());
                dragMic = (e.position.getDistanceFrom (p1) <= e.position.getDistanceFrom (p2)) ? 1 : 2;
                updateHeroMicFromPt (e.position, gr);
            }
            return;
        }

        const auto cells = computeGridCells (b, s, sy);
        const char* paramId = bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab;
        for (size_t i = 0; i < cells.size(); ++i)
        {
            if (cells[i].contains (e.position))
            {
                setParamPlain (processor.apvts, paramId, static_cast<float> (i));
                if (changedCb) changedCb();
                refresh();
                return;
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! cabMicZoom || dragMic == 0) return;
        const auto b = getLocalBounds().toFloat();
        const float s  = b.getWidth() / 420.0f;
        const float sy = b.getHeight() / 716.0f;
        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        if (cSpec.isOff || cSpec.isCustomIr) return;

        const auto heroR = computeHeroRect (b, s, sy);
        const auto heroImgBox = computeHeroStageRect (heroR, s, sy);
        const auto cabR = computeHeroCabRect (heroImgBox, cSpec);
        const auto gr = computeHeroGrilleRect (cabR, cSpec);
        if (! gr.isEmpty())
            updateHeroMicFromPt (e.position, gr);
    }

    void mouseUp (const juce::MouseEvent&) override { dragMic = 0; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! cabMicZoom) return;
        const auto b = getLocalBounds().toFloat();
        const float s  = b.getWidth() / 420.0f;
        const float sy = b.getHeight() / 716.0f;
        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        if (cSpec.isOff || cSpec.isCustomIr) return;

        const auto heroR = computeHeroRect (b, s, sy);
        const auto heroImgBox = computeHeroStageRect (heroR, s, sy);
        const auto cabR = computeHeroCabRect (heroImgBox, cSpec);
        const auto gr = computeHeroGrilleRect (cabR, cSpec);
        if (gr.isEmpty() || ! heroImgBox.contains (e.position)) return;

        const auto p1 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f) * gr.getWidth(),
                                            gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f) * gr.getHeight());
        const auto p2 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f) * gr.getWidth(),
                                            gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f) * gr.getHeight());
        const int which = (e.position.getDistanceFrom (p1) <= e.position.getDistanceFrom (p2)) ? 1 : 2;
        setParamPlain (processor.apvts, which == 1 ? MiAmpParams::mic1PosX : MiAmpParams::mic2PosX, which == 1 ? 36.0f : 66.0f);
        setParamPlain (processor.apvts, which == 1 ? MiAmpParams::mic1PosY : MiAmpParams::mic2PosY, which == 1 ? 52.0f : 48.0f);
        if (changedCb) changedCb();
        repaint();
    }

private:
    void setModeAndNotify (bool micMode)
    {
        if (cabMicZoom == micMode) return;
        cabMicZoom = micMode;
        syncVisibility();
        resized();
        repaint();
        if (modeCb) modeCb (cabMicZoom);
    }

    void syncVisibility()
    {
        btnTabCabs.setToggleState (! cabMicZoom, juce::dontSendNotification);
        btnTabMics.setToggleState (cabMicZoom, juce::dontSendNotification);

        btnBackCabs.setVisible (cabMicZoom);
        btnMic1On.setVisible (cabMicZoom);
        btnMic1Dist.setVisible (cabMicZoom);
        btnMic1Phase.setVisible (cabMicZoom);
        btnMic2On.setVisible (cabMicZoom);
        btnMic2Dist.setVisible (cabMicZoom);
        btnMic2Phase.setVisible (cabMicZoom);
        if (micMixKnob) micMixKnob->setVisible (cabMicZoom);
        if (roomKnob)   roomKnob->setVisible (cabMicZoom);
    }

    juce::Rectangle<float> computeHeroRect (juce::Rectangle<float> b, float s, float sy) const
    {
        return { 16.0f * s, 46.0f * sy, b.getWidth() - 32.0f * s, 434.0f * sy };
    }

    juce::Rectangle<float> computeHeroStageRect (juce::Rectangle<float> heroR, float s, float sy) const
    {
        return { heroR.getX() + 18.0f * s, heroR.getY() + 12.0f * sy,
                 heroR.getWidth() - 36.0f * s, heroR.getHeight() - 68.0f * sy };
    }

    juce::Rectangle<float> computeHeroCabRect (juce::Rectangle<float> stageR, const CabSpec& visualCab) const
    {
        if (visualCab.imageData == nullptr || visualCab.imageSize <= 0) return {};
        auto im = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
        if (! im.isValid()) return {};
        const float fit = juce::jmin (stageR.getWidth() / static_cast<float> (im.getWidth()),
                                      stageR.getHeight() / static_cast<float> (im.getHeight()));
        const float w = im.getWidth() * fit;
        const float h = im.getHeight() * fit;
        return { stageR.getCentreX() - w * 0.5f, stageR.getCentreY() - h * 0.5f, w, h };
    }

    juce::Rectangle<float> computeHeroGrilleRect (juce::Rectangle<float> cabR, const CabSpec& visualCab) const
    {
        if (cabR.isEmpty()) return {};
        return { cabR.getX() + visualCab.grilleX * cabR.getWidth(),
                 cabR.getY() + visualCab.grilleY * cabR.getHeight(),
                 visualCab.grilleW * cabR.getWidth(),
                 visualCab.grilleH * cabR.getHeight() };
    }

    std::vector<juce::Rectangle<float>> computeGridCells (juce::Rectangle<float> b, float s, float sy) const
    {
        const bool bass = processor.isBassMode();
        const int count = bass ? 9 : 10;
        const int cols = 5;
        const int rows = 2;
        const auto heroR = computeHeroRect (b, s, sy);
        const float topY = heroR.getBottom() + 12.0f * sy;
        const float botY = b.getHeight() - 16.0f * sy;
        const float gapX = 6.0f * s;
        const float gapY = 7.0f * sy;
        const float gx = 16.0f * s;
        const float gw = b.getWidth() - 32.0f * s;
        const float gh = botY - topY;
        const float cw = (gw - gapX * (cols - 1)) / cols;
        const float ch = (gh - gapY * (rows - 1)) / rows;
        std::vector<juce::Rectangle<float>> out;
        out.reserve (static_cast<size_t> (count));
        for (int i = 0; i < count; ++i)
        {
            const int r = i / cols, c = i % cols;
            out.emplace_back (gx + c * (cw + gapX), topY + r * (ch + gapY), cw, ch);
        }
        return out;
    }

    juce::Rectangle<float> computeMicsCardRect (juce::Rectangle<float> b, float s, float sy) const
    {
        const auto heroR = computeHeroRect (b, s, sy);
        const float topY = heroR.getBottom() + 12.0f * sy;
        return { 16.0f * s, topY, b.getWidth() - 32.0f * s, b.getHeight() - 16.0f * sy - topY };
    }

    juce::Rectangle<float> computeMicRowRect (juce::Rectangle<float> micsR, int row, float s, float sy) const
    {
        const float y0 = micsR.getY() + 36.0f * sy + row * (40.0f * sy);
        return { micsR.getX() + 12.0f * s, y0, micsR.getWidth() - 24.0f * s, 34.0f * sy };
    }

    void updateHeroMicFromPt (juce::Point<float> pt, juce::Rectangle<float> gr)
    {
        const float nx = juce::jlimit (0.0f, 100.0f, ((pt.x - gr.getX()) / juce::jmax (1.0f, gr.getWidth())) * 100.0f);
        const float ny = juce::jlimit (0.0f, 100.0f, ((pt.y - gr.getY()) / juce::jmax (1.0f, gr.getHeight())) * 100.0f);
        setParamPlain (processor.apvts, dragMic == 1 ? MiAmpParams::mic1PosX : MiAmpParams::mic2PosX, nx);
        setParamPlain (processor.apvts, dragMic == 1 ? MiAmpParams::mic1PosY : MiAmpParams::mic2PosY, ny);
        if (changedCb) changedCb();
        repaint();
    }

    void paintHeroMicPins (juce::Graphics& g, float s, juce::Rectangle<float> cabR, const CabSpec& visualCab) const
    {
        const auto gr = computeHeroGrilleRect (cabR, visualCab);
        if (gr.isEmpty()) return;

        // Subtle 4x4 guide grid over cabinet grille
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 34));
        for (int k = 1; k < 4; ++k)
        {
            const float f = static_cast<float> (k) * 0.25f;
            g.drawVerticalLine (juce::roundToInt (gr.getX() + f * gr.getWidth()), gr.getY() + 2.0f, gr.getBottom() - 2.0f);
            g.drawHorizontalLine (juce::roundToInt (gr.getY() + f * gr.getHeight()), gr.getX() + 2.0f, gr.getRight() - 2.0f);
        }

        const bool m1On = processor.apvts.getRawParameterValue (MiAmpParams::mic1On)->load() >= 0.5f;
        const bool m2On = processor.apvts.getRawParameterValue (MiAmpParams::mic2On)->load() >= 0.5f;
        const float px1 = gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f) * gr.getWidth();
        const float py1 = gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f) * gr.getHeight();
        const float px2 = gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f) * gr.getWidth();
        const float py2 = gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f) * gr.getHeight();

        auto drawPin = [&] (float x, float y, const char* lbl, juce::Colour fill, juce::Colour rim, juce::Colour txt, bool on)
        {
            const float r = 14.5f * s;
            g.setOpacity (on ? 1.0f : 0.36f);
            g.setColour (juce::Colour::fromRGBA (0, 0, 0, 145));
            g.fillEllipse (x - r, y - r + 2.5f * s, r * 2.0f, r * 2.0f);
            g.setColour (fill);
            g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
            g.setColour (rim);
            g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 2.2f);
            g.setColour (txt);
            g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, 10.5f * s), juce::Font::bold)));
            g.drawText (lbl, x - r, y - r, r * 2.0f, r * 2.0f, juce::Justification::centred);
            g.setOpacity (1.0f);
        };

        drawPin (px1, py1, "57", juce::Colour::fromRGB (18, 34, 28), juce::Colour::fromRGB (158, 246, 212), juce::Colour::fromRGB (220, 255, 241), m1On);
        drawPin (px2, py2, "87", juce::Colour::fromRGB (36, 28, 14), juce::Colour::fromRGB (255, 208, 123), juce::Colour::fromRGB (255, 242, 212), m2On);
    }

    MiAmpAudioProcessor& processor;
    std::function<void()> changedCb, closeCb, wavCb;
    std::function<void(bool)> modeCb;
    bool cabMicZoom = false;
    int dragMic = 0;
    int hoveredCell = -1;

    juce::TextButton btnClose    { juce::String::fromUTF8 (u8"\u00d7") };
    juce::TextButton btnTabCabs  { "CAJAS" };
    juce::TextButton btnTabMics  { juce::String::fromUTF8 (u8"MICR\u00d3FONOS") };
    juce::TextButton btnHeroWav  { ".WAV IR" };
    juce::TextButton btnBackCabs { juce::String::fromUTF8 (u8"\u2190 VOLVER A CAJAS") };

    juce::TextButton btnMic1On { "MI-57 DYN" }, btnMic1Dist { "CLOSE" }, btnMic1Phase { juce::String::fromUTF8 (u8"\u00d8") };
    juce::TextButton btnMic2On { "MI-87 COND" }, btnMic2Dist { "MED" },   btnMic2Phase { juce::String::fromUTF8 (u8"\u00d8") };
    std::unique_ptr<StudioMiniKnob> micMixKnob, roomKnob;
};
} // namespace miamp_rig
