#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AmpCatalogSpecs.h"
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
        const float sc = juce::jmin (b.getWidth() / 44.0f, b.getHeight() / 82.0f);
        const float rad = 14.0f * sc;

        auto drawR = b.reduced (1.0f);
        if (down)
            drawR = drawR.reduced (1.5f);

        juce::ColourGradient grad (
            juce::Colour::fromRGBA (255, 248, 232, highlighted ? 88 : 62), drawR.getX(), drawR.getY(),
            juce::Colour::fromRGBA (16, 20, 18, highlighted ? 176 : 148), drawR.getX(), drawR.getBottom(), false);
        grad.addColour (0.48, juce::Colour::fromRGBA (166, 223, 201, highlighted ? 62 : 26));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (drawR, rad);

        // Top specular edge
        g.setColour (juce::Colour::fromRGBA (255, 252, 240, highlighted ? 150 : 105));
        g.drawHorizontalLine (juce::roundToInt (drawR.getY() + 1.0f), drawR.getX() + rad * 0.45f, drawR.getRight() - rad * 0.45f);

        g.setColour (highlighted ? juce::Colour::fromRGB (166, 223, 201)
                                 : juce::Colour::fromRGBA (255, 243, 210, 132));
        g.drawRoundedRectangle (drawR, rad, 1.3f);

        // Inner vertical glass accent line
        const float accentX = isRight ? (drawR.getX() + 3.5f * sc) : (drawR.getRight() - 3.5f * sc);
        g.setColour (juce::Colour::fromRGBA (255, 248, 232, highlighted ? 120 : 75));
        g.drawVerticalLine (juce::roundToInt (accentX), drawR.getY() + 7.0f * sc, drawR.getBottom() - 7.0f * sc);

        // Double chevron SVG (main + echo), mirrored on right
        const float vw = drawR.getWidth() * 0.58f;
        const float vh = drawR.getHeight() * 0.58f;
        const float vx = drawR.getCentreX() - vw * 0.5f;
        const float vy = drawR.getCentreY() - vh * 0.5f;

        auto mapX = [&] (float svgX)
        {
            const float nx = isRight ? (28.0f - svgX) / 28.0f : (svgX / 28.0f);
            return vx + nx * vw;
        };
        auto mapY = [&] (float svgY) { return vy + (svgY / 48.0f) * vh; };

        juce::Path mainP;
        mainP.startNewSubPath (mapX (18.5f), mapY (8.5f));
        mainP.lineTo        (mapX (7.5f),  mapY (24.0f));
        mainP.lineTo        (mapX (18.5f), mapY (39.5f));
        g.setColour (highlighted ? juce::Colour::fromRGB (225, 255, 242) : juce::Colour::fromRGB (250, 243, 227));
        g.strokePath (mainP, juce::PathStrokeType (juce::jmax (1.8f, 3.1f * sc), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path echoP;
        echoP.startNewSubPath (mapX (23.5f), mapY (14.5f));
        echoP.lineTo        (mapX (16.0f), mapY (24.0f));
        echoP.lineTo        (mapX (23.5f), mapY (33.5f));
        g.setColour (highlighted ? juce::Colour::fromRGBA (191, 252, 225, 215) : juce::Colour::fromRGBA (166, 223, 201, 122));
        g.strokePath (echoP, juce::PathStrokeType (juce::jmax (1.3f, 2.1f * sc), juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

private:
    bool isRight = false;
    juce::String tip;
};

//==============================================================================
// Helper: draw red circle-slash "OFF / DI" sign matching preview SVG
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

//==============================================================================
// Left Frosted Glass Cabinet Picker Panel (+ Zoomed Full-Size Mic Stage)
class CabPickerPanel final : public juce::Component
{
public:
    CabPickerPanel (MiAmpAudioProcessor& proc,
                    std::function<void()> onChanged,
                    std::function<void()> onClose,
                    std::function<void()> onBackFromMicZoom,
                    std::function<void()> onOpenWav)
        : processor (proc),
          changedCb (std::move (onChanged)),
          closeCb (std::move (onClose)),
          backCabsCb (std::move (onBackFromMicZoom)),
          wavCb (std::move (onOpenWav))
    {
        btnClose.setComponentID ("popoverClose");
        btnClose.onClick = [this] { if (closeCb) closeCb(); };
        addAndMakeVisible (btnClose);

        btnBackCabs.onClick = [this] { if (backCabsCb) backCabsCb(); };
        addChildComponent (btnBackCabs);

        btnPrev.onClick = [this] { cycleCab (-1); };
        btnNext.onClick = [this] { cycleCab (+1); };
        addAndMakeVisible (btnPrev);
        addAndMakeVisible (btnNext);

        btnHeroWav.onClick = [this] { if (wavCb) wavCb(); };
        addChildComponent (btnHeroWav);

        btnFooterWav.onClick = [this] { if (wavCb) wavCb(); };
        addAndMakeVisible (btnFooterWav);
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
        syncVisibility();
        resized();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;

        // Frosted glass panel background
        juce::ColourGradient grad (juce::Colour::fromRGBA (239, 222, 183, 74), b.getX(), b.getY(),
                                   juce::Colour::fromRGBA (53, 54, 45, 184), b.getRight(), b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 20.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 158));
        g.drawRoundedRectangle (b, 20.0f * s, 1.5f);

        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& fallbackCab = cabs[static_cast<size_t> (bass ? 6 : 7)];
        const auto& visualCab = cSpec.isOff ? fallbackCab : cSpec;

        // Header title
        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (11.0f, 13.5f * s), juce::Font::bold)));
        if (cabMicZoom)
        {
            const juce::String ttl = juce::String ("MICS \u00b7 ") + (cSpec.isOff ? "DI (SIN CAJA)" : cSpec.displayName);
            g.drawText (ttl, 98.0f * s, 12.0f * s, 272.0f * s, 26.0f * s, juce::Justification::centredLeft, true);
        }
        else
        {
            const juce::String ttl = bass ? juce::String::fromUTF8 (u8"CAJAS \u00b7 BAJO")
                                          : juce::String::fromUTF8 (u8"CAJAS \u00b7 GUITARRA");
            g.drawText (ttl, 16.0f * s, 12.0f * s, 350.0f * s, 26.0f * s, juce::Justification::centredLeft, true);
        }

        if (cabMicZoom)
        {
            paintMicZoomStage (g, b, s, cSpec, visualCab);
            return;
        }

        // Hero Box
        const auto heroR = computeHeroRect (b, s);
        g.setColour (juce::Colour::fromRGBA (16, 18, 17, 108));
        g.fillRoundedRectangle (heroR, 14.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 66));
        g.drawRoundedRectangle (heroR, 14.0f * s, 1.0f);

        const auto heroImgBox = juce::Rectangle<float> (heroR.getX() + 50.0f * s, heroR.getY() + 8.0f * s,
                                                        heroR.getWidth() - 100.0f * s, 94.0f * s);
        const auto& heroSpec = cSpec.isOff ? fallbackCab : cSpec;
        if (heroSpec.imageData != nullptr && heroSpec.imageSize > 0)
        {
            auto im = juce::ImageCache::getFromMemory (heroSpec.imageData, heroSpec.imageSize);
            if (im.isValid())
            {
                if (cSpec.isOff)
                {
                    auto dim = makeDimmedCabImage (im, 0.44f);
                    g.setOpacity (0.94f);
                    g.drawImage (dim, heroImgBox, juce::RectanglePlacement::centred, false);
                    g.setOpacity (1.0f);
                    paintCabOffSign (g, heroImgBox.getCentre(), 54.0f * s);
                }
                else
                {
                    g.setOpacity (1.0f);
                    g.drawImage (im, heroImgBox, juce::RectanglePlacement::centred);
                }
            }
        }

        const juce::String heroName = (cSpec.isCustomIr && processor.getCustomCabIRName (bass).isNotEmpty())
            ? ("IR: " + processor.getCustomCabIRName (bass))
            : juce::String (cSpec.displayName);

        g.setColour (juce::Colour::fromRGB (250, 243, 227));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (10.5f, 13.0f * s), juce::Font::bold)));
        g.drawText (heroName, heroR.getX() + 12.0f * s, heroImgBox.getBottom() + 4.0f * s, heroR.getWidth() - 24.0f * s, 18.0f * s,
                    juce::Justification::centred, true);

        g.setColour (juce::Colour::fromRGBA (250, 243, 227, 194));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, 10.5f * s), juce::Font::plain)));
        g.drawText (cSpec.subtitle, heroR.getX() + 12.0f * s, heroImgBox.getBottom() + 21.0f * s, heroR.getWidth() - 24.0f * s, 14.0f * s,
                    juce::Justification::centred, true);

        // 3x3 Cabinet Grid
        const auto cells = computeGridCells (b, s);
        for (size_t i = 0; i < cells.size() && i < cabs.size(); ++i)
        {
            const auto& r = cells[i];
            const auto& item = cabs[i];
            const bool sel = (static_cast<int> (i) == cabIdx);

            g.setColour (sel ? juce::Colour::fromRGBA (166, 223, 201, 42) : juce::Colour::fromRGBA (20, 22, 20, 134));
            g.fillRoundedRectangle (r, 11.0f * s);
            g.setColour (sel ? juce::Colour::fromRGB (166, 223, 201) : juce::Colour::fromRGBA (255, 243, 210, 62));
            g.drawRoundedRectangle (r, 11.0f * s, sel ? 1.5f : 1.1f);

            const auto imgR = r.reduced (6.0f * s, 5.0f * s).withTrimmedBottom (20.0f * s);
            const auto& thumbSpec = item.isOff ? fallbackCab : item;
            if (thumbSpec.imageData != nullptr && thumbSpec.imageSize > 0)
            {
                auto im = juce::ImageCache::getFromMemory (thumbSpec.imageData, thumbSpec.imageSize);
                if (im.isValid())
                {
                    if (item.isOff)
                    {
                        auto dim = makeDimmedCabImage (im, 0.44f);
                        g.setOpacity (0.92f);
                        g.drawImage (dim, imgR, juce::RectanglePlacement::centred, false);
                        g.setOpacity (1.0f);
                        paintCabOffSign (g, imgR.getCentre(), 32.0f * s);
                    }
                    else
                    {
                        g.setOpacity (1.0f);
                        g.drawImage (im, imgR, juce::RectanglePlacement::centred);
                    }
                }
            }

            g.setColour (juce::Colour::fromRGB (250, 243, 227));
            g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.0f, 9.0f * s), juce::Font::bold)));
            g.drawText (item.isOff ? "SIN CAJA (DI)" : item.displayName,
                        r.getX() + 4.0f * s, r.getBottom() - 20.0f * s, r.getWidth() - 8.0f * s, 16.0f * s,
                        juce::Justification::centred, true);
        }

        // Footer index
        g.setColour (juce::Colour::fromRGBA (250, 243, 227, 224));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.5f, 11.0f * s), juce::Font::bold)));
        g.drawText ("CAJA " + juce::String (cabIdx + 1) + "/" + juce::String (static_cast<int> (cabs.size())),
                    16.0f * s, b.getHeight() - 34.0f * s, 140.0f * s, 24.0f * s, juce::Justification::centredLeft);
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;

        btnClose.setBounds (juce::roundToInt (b.getWidth() - 40.0f * s), juce::roundToInt (11.0f * s),
                            juce::roundToInt (26.0f * s), juce::roundToInt (24.0f * s));
        btnBackCabs.setBounds (juce::roundToInt (14.0f * s), juce::roundToInt (12.0f * s),
                               juce::roundToInt (78.0f * s), juce::roundToInt (24.0f * s));

        const auto heroR = computeHeroRect (b, s);
        btnPrev.setBounds (juce::roundToInt (heroR.getX() + 10.0f * s),
                           juce::roundToInt (heroR.getY() + 26.0f * s),
                           juce::roundToInt (32.0f * s), juce::roundToInt (56.0f * s));
        btnNext.setBounds (juce::roundToInt (heroR.getRight() - 42.0f * s),
                           juce::roundToInt (heroR.getY() + 26.0f * s),
                           juce::roundToInt (32.0f * s), juce::roundToInt (56.0f * s));
        btnHeroWav.setBounds (juce::roundToInt (heroR.getCentreX() - 72.0f * s),
                              juce::roundToInt (heroR.getBottom() - 28.0f * s),
                              juce::roundToInt (144.0f * s), juce::roundToInt (22.0f * s));

        btnFooterWav.setBounds (juce::roundToInt (b.getWidth() - 96.0f * s),
                                juce::roundToInt (b.getHeight() - 36.0f * s),
                                juce::roundToInt (80.0f * s), juce::roundToInt (26.0f * s));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;
        const bool bass = processor.isBassMode();

        if (cabMicZoom)
        {
            const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
            const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
                static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
            const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
            const auto& visualCab = cSpec.isOff ? cabs[static_cast<size_t> (bass ? 6 : 7)] : cSpec;
            const auto gr = computeZoomGrilleRect (b, s, visualCab);
            if (! gr.isEmpty())
            {
                const auto p1 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f) * gr.getWidth(),
                                                    gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f) * gr.getHeight());
                const auto p2 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f) * gr.getWidth(),
                                                    gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f) * gr.getHeight());
                const float d1 = e.position.getDistanceFrom (p1);
                const float d2 = e.position.getDistanceFrom (p2);
                dragMic = (d1 <= d2) ? 1 : 2;
                updateZoomMicFromPt (e.position, gr);
            }
            return;
        }

        const auto cells = computeGridCells (b, s);
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
        const float s = b.getWidth() / 420.0f;
        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& visualCab = cSpec.isOff ? cabs[static_cast<size_t> (bass ? 6 : 7)] : cSpec;
        const auto gr = computeZoomGrilleRect (b, s, visualCab);
        if (! gr.isEmpty())
            updateZoomMicFromPt (e.position, gr);
    }

    void mouseUp (const juce::MouseEvent&) override { dragMic = 0; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (! cabMicZoom) return;
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;
        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& visualCab = cSpec.isOff ? cabs[static_cast<size_t> (bass ? 6 : 7)] : cSpec;
        const auto gr = computeZoomGrilleRect (b, s, visualCab);
        if (gr.isEmpty()) return;
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
    void syncVisibility()
    {
        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const bool isCustom = cabs[static_cast<size_t> (cabIdx)].isCustomIr;

        btnBackCabs.setVisible (cabMicZoom);
        btnPrev.setVisible (! cabMicZoom);
        btnNext.setVisible (! cabMicZoom);
        btnHeroWav.setVisible (! cabMicZoom && isCustom);
        btnFooterWav.setVisible (! cabMicZoom);
    }

    void cycleCab (int delta)
    {
        const bool bass = processor.isBassMode();
        const int total = bass ? 9 : 10;
        const char* id = bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab;
        const int cur = static_cast<int> (std::lround (processor.apvts.getRawParameterValue (id)->load()));
        const int nxt = (cur + delta + total) % total;
        setParamPlain (processor.apvts, id, static_cast<float> (nxt));
        if (changedCb) changedCb();
        refresh();
    }

    juce::Rectangle<float> computeHeroRect (juce::Rectangle<float> b, float s) const
    {
        return { 16.0f * s, 44.0f * s, b.getWidth() - 32.0f * s, 162.0f * s };
    }

    std::vector<juce::Rectangle<float>> computeGridCells (juce::Rectangle<float> b, float s) const
    {
        const bool bass = processor.isBassMode();
        const int count = bass ? 9 : 10;
        const int cols = 3;
        const int rows = (count + cols - 1) / cols;
        const float topY = 214.0f * s;
        const float botY = b.getHeight() - 42.0f * s;
        const float gap = 8.0f * s;
        const float gx = 16.0f * s;
        const float gw = b.getWidth() - 32.0f * s;
        const float gh = botY - topY;
        const float cw = (gw - gap * (cols - 1)) / cols;
        const float ch = (gh - gap * (rows - 1)) / rows;
        std::vector<juce::Rectangle<float>> out;
        for (int i = 0; i < count; ++i)
        {
            const int r = i / cols, c = i % cols;
            out.emplace_back (gx + c * (cw + gap), topY + r * (ch + gap), cw, ch);
        }
        return out;
    }

    juce::Rectangle<float> computeZoomCabRect (juce::Rectangle<float> b, float s, const CabSpec& visualCab) const
    {
        if (visualCab.imageData == nullptr || visualCab.imageSize <= 0) return {};
        auto im = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
        if (! im.isValid()) return {};
        const float maxW = b.getWidth() - 44.0f * s;
        const float maxH = b.getHeight() - 104.0f * s;
        const float fit = juce::jmin (maxW / static_cast<float> (im.getWidth()),
                                      maxH / static_cast<float> (im.getHeight()));
        const float w = im.getWidth() * fit;
        const float h = im.getHeight() * fit;
        const float areaTop = 48.0f * s;
        return { b.getCentreX() - w * 0.5f, areaTop + (maxH - h) * 0.5f, w, h };
    }

    juce::Rectangle<float> computeZoomGrilleRect (juce::Rectangle<float> b, float s, const CabSpec& visualCab) const
    {
        const auto cr = computeZoomCabRect (b, s, visualCab);
        if (cr.isEmpty()) return {};
        return { cr.getX() + visualCab.grilleX * cr.getWidth(),
                 cr.getY() + visualCab.grilleY * cr.getHeight(),
                 visualCab.grilleW * cr.getWidth(),
                 visualCab.grilleH * cr.getHeight() };
    }

    void updateZoomMicFromPt (juce::Point<float> pt, juce::Rectangle<float> gr)
    {
        const float nx = juce::jlimit (0.0f, 100.0f, ((pt.x - gr.getX()) / juce::jmax (1.0f, gr.getWidth())) * 100.0f);
        const float ny = juce::jlimit (0.0f, 100.0f, ((pt.y - gr.getY()) / juce::jmax (1.0f, gr.getHeight())) * 100.0f);
        setParamPlain (processor.apvts, dragMic == 1 ? MiAmpParams::mic1PosX : MiAmpParams::mic2PosX, nx);
        setParamPlain (processor.apvts, dragMic == 1 ? MiAmpParams::mic1PosY : MiAmpParams::mic2PosY, ny);
        if (changedCb) changedCb();
        repaint();
    }

    void paintMicZoomStage (juce::Graphics& g, juce::Rectangle<float> b, float s, const CabSpec& cSpec, const CabSpec& visualCab) const
    {
        const auto cabR = computeZoomCabRect (b, s, visualCab);
        const auto grR  = computeZoomGrilleRect (b, s, visualCab);
        if (! cabR.isEmpty() && visualCab.imageData != nullptr)
        {
            auto im = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
            if (im.isValid())
            {
                if (cSpec.isOff)
                {
                    auto dim = makeDimmedCabImage (im, 0.44f);
                    g.setOpacity (0.94f);
                    g.drawImage (dim, cabR, juce::RectanglePlacement::stretchToFit, false);
                    g.setOpacity (1.0f);
                }
                else
                {
                    g.setOpacity (1.0f);
                    g.drawImage (im, cabR, juce::RectanglePlacement::stretchToFit);
                }
            }
        }

        if (! grR.isEmpty())
        {
            const float dashLen[2] = { 4.0f * s, 4.0f * s };
            juce::Path gp;
            gp.addRoundedRectangle (grR, 6.0f * s);
            juce::PathStrokeType pst (1.0f);
            juce::Path dashed;
            pst.createDashedStroke (dashed, gp, dashLen, 2);
            g.setColour (juce::Colour::fromRGBA (166, 223, 201, 64));
            g.fillPath (dashed);

            static constexpr float kDistScale[3] = { 1.0f, 0.86f, 0.72f };
            const bool m1On = processor.apvts.getRawParameterValue (MiAmpParams::mic1On)->load() >= 0.5f;
            const bool m2On = processor.apvts.getRawParameterValue (MiAmpParams::mic2On)->load() >= 0.5f;
            const int  m1D  = juce::jlimit (0, 2, static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::mic1Dist)->load())));
            const int  m2D  = juce::jlimit (0, 2, static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::mic2Dist)->load())));

            const auto img57 = juce::ImageCache::getFromMemory (BinaryData::mic_mi57_clean_png, BinaryData::mic_mi57_clean_pngSize);
            const auto img87 = juce::ImageCache::getFromMemory (BinaryData::mic_mi87_clean_png, BinaryData::mic_mi87_clean_pngSize);

            const float x1p = processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load();
            const float y1p = processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load();
            const float x2p = processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load();
            const float y2p = processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load();

            if (img57.isValid())
            {
                const float mw1 = cabR.getWidth() * 0.25f * kDistScale[m1D];
                const float mh1 = mw1 * (static_cast<float> (img57.getHeight()) / juce::jmax (1, img57.getWidth()));
                const float cx1 = grR.getX() + (x1p * 0.01f) * grR.getWidth();
                const float cy1 = grR.getY() + (y1p * 0.01f) * grR.getHeight();
                const auto mr1 = juce::Rectangle<float> (cx1 - mw1 * 0.5f, cy1 - mh1 * 0.5f, mw1, mh1);
                g.setOpacity (m1On ? 1.0f : 0.34f);
                g.drawImage (img57, mr1, juce::RectanglePlacement::centred);
                g.setOpacity (1.0f);
                const auto tag1 = juce::Rectangle<float> (cx1 - 20.0f * s, mr1.getBottom() - 2.0f * s, 40.0f * s, 14.0f * s);
                g.setColour (juce::Colour::fromRGBA (16, 18, 17, 215));
                g.fillRoundedRectangle (tag1, 4.0f * s);
                g.setColour (juce::Colour::fromRGB (158, 246, 212));
                g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.0f, 9.0f * s), juce::Font::bold)));
                g.drawText ("MI-57", tag1, juce::Justification::centred);
            }

            if (img87.isValid())
            {
                const float mh2 = cabR.getWidth() * 0.26f * kDistScale[m2D];
                const float mw2 = mh2 * (static_cast<float> (img87.getWidth()) / juce::jmax (1, img87.getHeight()));
                const float cx2 = grR.getX() + (x2p * 0.01f) * grR.getWidth();
                const float cy2 = grR.getY() + (y2p * 0.01f) * grR.getHeight();
                const auto mr2 = juce::Rectangle<float> (cx2 - mw2 * 0.5f, cy2 - mh2 * 0.5f, mw2, mh2);
                g.setOpacity (m2On ? 1.0f : 0.34f);
                g.drawImage (img87, mr2, juce::RectanglePlacement::centred);
                g.setOpacity (1.0f);
                const auto tag2 = juce::Rectangle<float> (cx2 - 20.0f * s, mr2.getBottom() - 2.0f * s, 40.0f * s, 14.0f * s);
                g.setColour (juce::Colour::fromRGBA (16, 18, 17, 215));
                g.fillRoundedRectangle (tag2, 4.0f * s);
                g.setColour (juce::Colour::fromRGB (255, 208, 123));
                g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.0f, 9.0f * s), juce::Font::bold)));
                g.drawText ("MI-87", tag2, juce::Justification::centred);
            }
        }

        if (cSpec.isOff && ! cabR.isEmpty())
        {
            const auto offPill = juce::Rectangle<float> (cabR.getCentreX() - 62.0f * s, cabR.getCentreY() - 11.0f * s, 124.0f * s, 22.0f * s);
            g.setColour (juce::Colour::fromRGBA (16, 18, 17, 225));
            g.fillRoundedRectangle (offPill, 6.0f * s);
            g.setColour (juce::Colour::fromRGBA (239, 68, 68, 190));
            g.drawRoundedRectangle (offPill, 6.0f * s, 1.0f);
            g.setColour (juce::Colour::fromRGB (252, 165, 165));
            g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.5f, 9.5f * s), juce::Font::bold)));
            g.drawText ("CABINET OFF (DI)", offPill, juce::Justification::centred);
        }

        // Footer coordinates bar
        const float fy = b.getHeight() - 34.0f * s;
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 36));
        g.drawHorizontalLine (juce::roundToInt (fy - 2.0f * s), 16.0f * s, b.getWidth() - 16.0f * s);

        const int x1 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load());
        const int y1 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load());
        const int x2 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load());
        const int y2 = juce::roundToInt (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load());

        g.setColour (juce::Colour::fromRGB (166, 223, 201));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (9.0f, 10.5f * s), juce::Font::bold)));
        g.drawText ("MI-57: " + juce::String (x1) + "%, " + juce::String (y1) + "%  \u00b7  MI-87: " + juce::String (x2) + "%, " + juce::String (y2) + "%",
                    16.0f * s, fy, 260.0f * s, 24.0f * s, juce::Justification::centredLeft);

        g.setColour (juce::Colour::fromRGBA (250, 243, 227, 175));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.5f, 10.0f * s), juce::Font::plain)));
        g.drawText (juce::String::fromUTF8 (u8"Arrastr\u00e1 los mics"),
                    b.getWidth() - 140.0f * s, fy, 124.0f * s, 24.0f * s, juce::Justification::centredRight);
    }

    MiAmpAudioProcessor& processor;
    std::function<void()> changedCb, closeCb, backCabsCb, wavCb;
    bool cabMicZoom = false;
    int dragMic = 0;

    juce::TextButton btnClose     { juce::String::fromUTF8 (u8"\u00d7") };
    juce::TextButton btnBackCabs  { juce::String::fromUTF8 (u8"\u2190 CAJAS") };
    juce::TextButton btnPrev      { "<" };
    juce::TextButton btnNext      { ">" };
    juce::TextButton btnHeroWav   { "+ CARGAR .WAV IR" };
    juce::TextButton btnFooterWav { ".WAV IR" };
};

//==============================================================================
// Bottom-Left Glass Dual Mics & Room Popup (below CabPickerPanel)
class CabMicsPanel final : public juce::Component
{
public:
    CabMicsPanel (MiAmpAudioProcessor& proc,
                  std::function<void()> onChanged,
                  std::function<void()> onToggleMicZoom)
        : processor (proc), changedCb (std::move (onChanged)), toggleZoomCb (std::move (onToggleMicZoom))
    {
        auto bindTog = [this] (juce::TextButton& b, const char* id)
        {
            b.setClickingTogglesState (true);
            b.onClick = [this, &b, id]
            {
                setParamPlain (processor.apvts, id, b.getToggleState() ? 1.0f : 0.0f);
                if (changedCb) changedCb();
                refresh();
            };
            addAndMakeVisible (b);
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
            addAndMakeVisible (b);
        };
        bindDist (btnMic1Dist, MiAmpParams::mic1Dist);
        bindDist (btnMic2Dist, MiAmpParams::mic2Dist);

        btnMoveMics.onClick = [this] { if (toggleZoomCb) toggleZoomCb(); };
        addAndMakeVisible (btnMoveMics);

        micMixKnob = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::cabMicMix, "MIC MIX", "%");
        roomKnob   = std::make_unique<StudioMiniKnob> (processor.apvts, MiAmpParams::cabRoom,   "ROOM", "%");
        addAndMakeVisible (*micMixKnob);
        addAndMakeVisible (*roomKnob);
    }

    void setMicZoomActive (bool zoom)
    {
        micZoomActive = zoom;
        btnMoveMics.setToggleState (zoom, juce::dontSendNotification);
        btnMoveMics.setButtonText (zoom ? juce::String::fromUTF8 (u8"\u2190 VER CAJAS") : "MOVER EN CAB");
    }

    void refresh()
    {
        static constexpr const char* kDistTags[3] = { "CLOSE", "MED", "FAR" };
        btnMic1On.setToggleState    (processor.apvts.getRawParameterValue (MiAmpParams::mic1On)->load() >= 0.5f, juce::dontSendNotification);
        btnMic1Phase.setToggleState (processor.apvts.getRawParameterValue (MiAmpParams::mic1Phase)->load() >= 0.5f, juce::dontSendNotification);
        btnMic1Dist.setButtonText   (kDistTags[juce::jlimit (0, 2, static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::mic1Dist)->load())))]);

        btnMic2On.setToggleState    (processor.apvts.getRawParameterValue (MiAmpParams::mic2On)->load() >= 0.5f, juce::dontSendNotification);
        btnMic2Phase.setToggleState (processor.apvts.getRawParameterValue (MiAmpParams::mic2Phase)->load() >= 0.5f, juce::dontSendNotification);
        btnMic2Dist.setButtonText   (kDistTags[juce::jlimit (0, 2, static_cast<int> (std::lround (processor.apvts.getRawParameterValue (MiAmpParams::mic2Dist)->load())))]);

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;

        juce::ColourGradient grad (juce::Colour::fromRGBA (239, 222, 183, 74), b.getX(), b.getY(),
                                   juce::Colour::fromRGBA (53, 54, 45, 184), b.getRight(), b.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (b, 20.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 158));
        g.drawRoundedRectangle (b, 20.0f * s, 1.5f);

        g.setColour (juce::Colour::fromRGBA (191, 252, 225, 218));
        g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (8.5f, 10.0f * s), juce::Font::bold)));
        g.drawText ("DUAL MICS", 14.0f * s, 12.0f * s, 132.0f * s, 14.0f * s, juce::Justification::centredLeft);

        // Mini XY Pad (full uncropped cab + aligned 57 / 87 pins)
        const auto padR = computeXyPadRect (b, s);
        g.setColour (juce::Colour::fromRGBA (12, 14, 13, 198));
        g.fillRoundedRectangle (padR, 12.0f * s);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 96));
        g.drawRoundedRectangle (padR, 12.0f * s, 1.3f);

        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& visualCab = cSpec.isOff ? cabs[static_cast<size_t> (bass ? 6 : 7)] : cSpec;

        const auto innerR = padR.reduced (4.0f * s);
        if (visualCab.imageData != nullptr && visualCab.imageSize > 0)
        {
            auto im = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
            if (im.isValid())
            {
                if (cSpec.isOff)
                {
                    auto dim = makeDimmedCabImage (im, 0.36f);
                    g.setOpacity (0.86f);
                    g.drawImage (dim, innerR, juce::RectanglePlacement::centred, false);
                    g.setOpacity (1.0f);
                }
                else
                {
                    g.setOpacity (0.86f);
                    g.drawImage (im, innerR, juce::RectanglePlacement::centred);
                    g.setOpacity (1.0f);
                }
            }
        }

        // Subtle 4x4 grid inside XY pad
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 20));
        for (int k = 1; k < 4; ++k)
        {
            const float f = static_cast<float> (k) * 0.25f;
            g.drawVerticalLine (juce::roundToInt (padR.getX() + f * padR.getWidth()), padR.getY() + 2.0f, padR.getBottom() - 2.0f);
            g.drawHorizontalLine (juce::roundToInt (padR.getY() + f * padR.getHeight()), padR.getX() + 2.0f, padR.getRight() - 2.0f);
        }

        const auto gr = computeMiniGrilleRect (padR, s, visualCab);
        if (! gr.isEmpty())
        {
            const bool m1On = processor.apvts.getRawParameterValue (MiAmpParams::mic1On)->load() >= 0.5f;
            const bool m2On = processor.apvts.getRawParameterValue (MiAmpParams::mic2On)->load() >= 0.5f;
            const float px1 = gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f) * gr.getWidth();
            const float py1 = gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f) * gr.getHeight();
            const float px2 = gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f) * gr.getWidth();
            const float py2 = gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f) * gr.getHeight();

            auto drawPin = [&] (float x, float y, const char* lbl, juce::Colour fill, juce::Colour rim, juce::Colour txt, bool on)
            {
                const float r = 10.5f * s;
                g.setOpacity (on ? 1.0f : 0.35f);
                g.setColour (fill);
                g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
                g.setColour (rim);
                g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.8f);
                g.setColour (txt);
                g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.5f, 8.5f * s), juce::Font::bold)));
                g.drawText (lbl, x - r, y - r, r * 2.0f, r * 2.0f, juce::Justification::centred);
                g.setOpacity (1.0f);
            };
            drawPin (px1, py1, "57", juce::Colour::fromRGB (18, 34, 28), juce::Colour::fromRGB (158, 246, 212), juce::Colour::fromRGB (220, 255, 241), m1On);
            drawPin (px2, py2, "87", juce::Colour::fromRGB (36, 28, 14), juce::Colour::fromRGB (255, 208, 123), juce::Colour::fromRGB (255, 242, 212), m2On);
        }

        if (cSpec.isOff)
        {
            const auto offPill = juce::Rectangle<float> (padR.getCentreX() - 28.0f * s, padR.getCentreY() - 9.0f * s, 56.0f * s, 18.0f * s);
            g.setColour (juce::Colour::fromRGBA (16, 18, 17, 226));
            g.fillRoundedRectangle (offPill, 5.0f * s);
            g.setColour (juce::Colour::fromRGBA (239, 68, 68, 190));
            g.drawRoundedRectangle (offPill, 5.0f * s, 1.0f);
            g.setColour (juce::Colour::fromRGB (252, 165, 165));
            g.setFont (juce::Font (juce::FontOptions ("Arial", juce::jmax (7.5f, 8.5f * s), juce::Font::bold)));
            g.drawText ("DI (OFF)", offPill, juce::Justification::centred);
        }
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;

        btnMic1On.setBounds    (juce::roundToInt (14.0f * s), juce::roundToInt (32.0f * s), juce::roundToInt (46.0f * s), juce::roundToInt (24.0f * s));
        btnMic1Dist.setBounds  (juce::roundToInt (63.0f * s), juce::roundToInt (32.0f * s), juce::roundToInt (54.0f * s), juce::roundToInt (24.0f * s));
        btnMic1Phase.setBounds (juce::roundToInt (120.0f * s),juce::roundToInt (32.0f * s), juce::roundToInt (26.0f * s), juce::roundToInt (24.0f * s));

        btnMic2On.setBounds    (juce::roundToInt (14.0f * s), juce::roundToInt (62.0f * s), juce::roundToInt (46.0f * s), juce::roundToInt (24.0f * s));
        btnMic2Dist.setBounds  (juce::roundToInt (63.0f * s), juce::roundToInt (62.0f * s), juce::roundToInt (54.0f * s), juce::roundToInt (24.0f * s));
        btnMic2Phase.setBounds (juce::roundToInt (120.0f * s),juce::roundToInt (62.0f * s), juce::roundToInt (26.0f * s), juce::roundToInt (24.0f * s));

        btnMoveMics.setBounds  (juce::roundToInt (14.0f * s), juce::roundToInt (96.0f * s), juce::roundToInt (132.0f * s), juce::roundToInt (26.0f * s));

        if (micMixKnob) micMixKnob->setBounds (juce::roundToInt (278.0f * s), juce::roundToInt (24.0f * s), juce::roundToInt (64.0f * s), juce::roundToInt (98.0f * s));
        if (roomKnob)   roomKnob->setBounds   (juce::roundToInt (344.0f * s), juce::roundToInt (24.0f * s), juce::roundToInt (64.0f * s), juce::roundToInt (98.0f * s));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;
        const auto padR = computeXyPadRect (b, s);
        if (! padR.contains (e.position)) return;

        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& visualCab = cSpec.isOff ? cabs[static_cast<size_t> (bass ? 6 : 7)] : cSpec;
        const auto gr = computeMiniGrilleRect (padR, s, visualCab);
        if (gr.isEmpty()) return;

        const auto p1 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f) * gr.getWidth(),
                                            gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f) * gr.getHeight());
        const auto p2 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f) * gr.getWidth(),
                                            gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f) * gr.getHeight());
        dragMic = (e.position.getDistanceFrom (p1) <= e.position.getDistanceFrom (p2)) ? 1 : 2;
        updateFromPt (e.position, gr);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragMic == 0) return;
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;
        const auto padR = computeXyPadRect (b, s);
        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& visualCab = cSpec.isOff ? cabs[static_cast<size_t> (bass ? 6 : 7)] : cSpec;
        const auto gr = computeMiniGrilleRect (padR, s, visualCab);
        if (! gr.isEmpty())
            updateFromPt (e.position, gr);
    }

    void mouseUp (const juce::MouseEvent&) override { dragMic = 0; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto b = getLocalBounds().toFloat();
        const float s = b.getWidth() / 420.0f;
        const auto padR = computeXyPadRect (b, s);
        if (! padR.contains (e.position)) return;
        const bool bass = processor.isBassMode();
        const auto& cabs = bass ? getBassCabSpecs() : getGuitarCabSpecs();
        const int cabIdx = juce::jlimit (0, static_cast<int> (cabs.size()) - 1,
            static_cast<int> (std::lround (processor.apvts.getRawParameterValue (bass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)->load())));
        const auto& cSpec = cabs[static_cast<size_t> (cabIdx)];
        const auto& visualCab = cSpec.isOff ? cabs[static_cast<size_t> (bass ? 6 : 7)] : cSpec;
        const auto gr = computeMiniGrilleRect (padR, s, visualCab);
        if (gr.isEmpty()) return;
        const auto p1 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosX)->load() * 0.01f) * gr.getWidth(),
                                            gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic1PosY)->load() * 0.01f) * gr.getHeight());
        const auto p2 = juce::Point<float> (gr.getX() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosX)->load() * 0.01f) * gr.getWidth(),
                                            gr.getY() + (processor.apvts.getRawParameterValue (MiAmpParams::mic2PosY)->load() * 0.01f) * gr.getHeight());
        const int which = (e.position.getDistanceFrom (p1) <= e.position.getDistanceFrom (p2)) ? 1 : 2;
        setParamPlain (processor.apvts, which == 1 ? MiAmpParams::mic1PosX : MiAmpParams::mic2PosX, which == 1 ? 36.0f : 66.0f);
        setParamPlain (processor.apvts, which == 1 ? MiAmpParams::mic1PosY : MiAmpParams::mic2PosY, which == 1 ? 52.0f : 48.0f);
        if (changedCb) changedCb();
        refresh();
    }

private:
    juce::Rectangle<float> computeXyPadRect (juce::Rectangle<float> b, float s) const
    {
        return { 154.0f * s, 17.0f * s, 116.0f * s, 116.0f * s };
    }

    juce::Rectangle<float> computeMiniGrilleRect (juce::Rectangle<float> padR, float s, const CabSpec& visualCab) const
    {
        if (visualCab.imageData == nullptr || visualCab.imageSize <= 0) return {};
        auto im = juce::ImageCache::getFromMemory (visualCab.imageData, visualCab.imageSize);
        if (! im.isValid()) return {};
        const float box = padR.getWidth() - 8.0f * s;
        const float fit = juce::jmin (box / static_cast<float> (im.getWidth()),
                                      box / static_cast<float> (im.getHeight()));
        const float cw = im.getWidth() * fit;
        const float ch = im.getHeight() * fit;
        const float ox = padR.getX() + 4.0f * s + (box - cw) * 0.5f;
        const float oy = padR.getY() + 4.0f * s + (box - ch) * 0.5f;
        return { ox + visualCab.grilleX * cw, oy + visualCab.grilleY * ch,
                 visualCab.grilleW * cw, visualCab.grilleH * ch };
    }

    void updateFromPt (juce::Point<float> pt, juce::Rectangle<float> gr)
    {
        const float nx = juce::jlimit (0.0f, 100.0f, ((pt.x - gr.getX()) / juce::jmax (1.0f, gr.getWidth())) * 100.0f);
        const float ny = juce::jlimit (0.0f, 100.0f, ((pt.y - gr.getY()) / juce::jmax (1.0f, gr.getHeight())) * 100.0f);
        setParamPlain (processor.apvts, dragMic == 1 ? MiAmpParams::mic1PosX : MiAmpParams::mic2PosX, nx);
        setParamPlain (processor.apvts, dragMic == 1 ? MiAmpParams::mic1PosY : MiAmpParams::mic2PosY, ny);
        if (changedCb) changedCb();
        refresh();
    }

    MiAmpAudioProcessor& processor;
    std::function<void()> changedCb, toggleZoomCb;
    bool micZoomActive = false;
    int dragMic = 0;

    juce::TextButton btnMic1On { "MI-57" }, btnMic1Dist { "CLOSE" }, btnMic1Phase { juce::String::fromUTF8 (u8"\u00d8") };
    juce::TextButton btnMic2On { "MI-87" }, btnMic2Dist { "MED" },   btnMic2Phase { juce::String::fromUTF8 (u8"\u00d8") };
    juce::TextButton btnMoveMics { "MOVER EN CAB" };
    std::unique_ptr<StudioMiniKnob> micMixKnob, roomKnob;
};
} // namespace miamp_rig
