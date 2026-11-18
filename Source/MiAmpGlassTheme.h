#pragma once
#include <JuceHeader.h>

namespace miamp_theme
{
    //==============================================================================
    // REFERENCIA OFICIAL UNICA DE VIDRIO CLARO ("Vidrio Claro") — MI AMP
    // Todos los paneles, popovers, modales, tarjetas y sub-paneles de vidrio
    // del plugin DEBEN usar exclusivamente estas funciones/constantes para mantener
    // exactamente el mismo tono, gradiente y luminosidad en toda la interfaz.
    //==============================================================================

    inline juce::Colour glassPanelGradTopLeft (float alpha = 1.0f) noexcept
    {
        return juce::Colour::fromRGBA (239, 222, 183, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 74.0f * alpha)));
    }

    inline juce::Colour glassPanelGradBottomRight (float alpha = 1.0f) noexcept
    {
        return juce::Colour::fromRGBA (53, 54, 45, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 178.0f * alpha)));
    }

    inline juce::Colour glassPanelBorder (float alpha = 1.0f) noexcept
    {
        return juce::Colour::fromRGBA (255, 243, 210, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 158.0f * alpha)));
    }

    inline void paintFrostedGlassPanel (juce::Graphics& g,
                                        juce::Rectangle<float> r,
                                        float cornerRadius,
                                        float alpha = 1.0f,
                                        float borderThickness = 1.5f)
    {
        juce::ColourGradient grad (glassPanelGradTopLeft (alpha), r.getX(), r.getY(),
                                   glassPanelGradBottomRight (alpha), r.getRight(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, cornerRadius);

        g.setColour (glassPanelBorder (alpha));
        g.drawRoundedRectangle (r, cornerRadius, borderThickness);
    }

    // Sub-panel interior claro (Hero box, controles Dual Mics, sub-paneles Studio FX, notas)
    inline void paintGlassSubPanel (juce::Graphics& g,
                                    juce::Rectangle<float> r,
                                    float cornerRadius,
                                    float alpha = 1.0f,
                                    float borderThickness = 1.2f)
    {
        juce::ColourGradient grad (
            juce::Colour::fromRGBA (255, 250, 238, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 41.0f * alpha))),
            r.getX(), r.getY(),
            juce::Colour::fromRGBA (230, 218, 190, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 23.0f * alpha))),
            r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, cornerRadius);

        g.setColour (juce::Colour::fromRGBA (255, 246, 222, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 107.0f * alpha))));
        g.drawRoundedRectangle (r, cornerRadius, borderThickness);
    }

    // Tarjeta / celda interior clara (Grilla 5x2 de cajas, RigSelector, filas de presets/export)
    inline void paintGlassCard (juce::Graphics& g,
                                juce::Rectangle<float> r,
                                float cornerRadius,
                                bool selected,
                                bool hovered = false,
                                float alpha = 1.0f)
    {
        if (selected)
        {
            g.setColour (juce::Colour::fromRGBA (166, 223, 201, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 71.0f * alpha))));
            g.fillRoundedRectangle (r, cornerRadius);
            g.setColour (juce::Colour::fromRGBA (166, 223, 201, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, 255.0f * alpha))));
            g.drawRoundedRectangle (r, cornerRadius, 1.8f);
            return;
        }

        const float topA = hovered ? 87.0f : 56.0f;
        const float botA = hovered ? 56.0f : 31.0f;
        const float brdA = hovered ? 198.0f : 122.0f;

        juce::ColourGradient grad (
            juce::Colour::fromRGBA (255, 250, 238, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, topA * alpha))),
            r.getX(), r.getY(),
            juce::Colour::fromRGBA (236, 224, 198, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, botA * alpha))),
            r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, cornerRadius);

        g.setColour (juce::Colour::fromRGBA (255, 246, 222, static_cast<juce::uint8> (juce::jlimit (0.0f, 255.0f, brdA * alpha))));
        g.drawRoundedRectangle (r, cornerRadius, hovered ? 1.4f : 1.2f);
    }
} // namespace miamp_theme
