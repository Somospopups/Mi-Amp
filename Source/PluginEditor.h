#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MiAmpAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit MiAmpAudioProcessorEditor (MiAmpAudioProcessor&);
    ~MiAmpAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    enum class PopoverKind { None, Presets, Tuner, Drop };

    struct BassImages
    {
        juce::Image bassBg, bassKnobCap, toggleUp, toggleDown;
        juce::Image floorBg, boardSurface;
        juce::Image xvActive, xvBypass, xvKnobPtr, xvCabBass, xvCabPunk, xvCabClean;
        juce::Image compActive, compOff, chActive, chOff, phActive, phOff, rvActive, rvOff;
        juce::Image compKnob[3], chKnob[3], phKnob[3];
    };

    class GlassLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                   bool highlighted, bool down) override;
        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool down) override;
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               const juce::Slider::SliderStyle, juce::Slider&) override;
    };

    class ImageRotaryKnob final : public juce::Component
    {
    public:
        ImageRotaryKnob (juce::AudioProcessorValueTreeState& state,
                         const juce::String& paramId,
                         const juce::String& labelText,
                         juce::Image knobImg,
                         bool isBassHeadStyle,
                         std::function<void(const juce::String&)> onHud);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void setParamId (const juce::String& newId);

    private:
        juce::AudioProcessorValueTreeState& apvts;
        juce::String pid;
        juce::String label;
        juce::Image image;
        bool headStyle = false;
        std::function<void(const juce::String&)> hudCallback;
        float dragStartVal = 50.0f;
        int dragStartY = 0;
        bool dragging = false;
    };

    class ImageToggleSwitch final : public juce::Component
    {
    public:
        ImageToggleSwitch (juce::AudioProcessorValueTreeState& state,
                           const juce::String& paramId,
                           const juce::String& labelText,
                           juce::Image upImg,
                           juce::Image downImg,
                           std::function<void(const juce::String&)> onHud);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::AudioProcessorValueTreeState& apvts;
        juce::String pid;
        juce::String label;
        juce::Image imgUp, imgDown;
        std::function<void(const juce::String&)> hudCallback;
    };

    class PalettePedalButton final : public juce::Component
    {
    public:
        PalettePedalButton (int pedalType, const juce::String& title, juce::Image img,
                            std::function<void()> onClick);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override { if (clickFn) clickFn(); }
        void setInChain (bool inChain) { alreadyInChain = inChain; repaint(); }

    private:
        int type = 0;
        juce::String name;
        juce::Image art;
        bool alreadyInChain = false;
        std::function<void()> clickFn;
    };

    class GlassPopover;

    void loadImages();
    void timerCallback() override;
    void showHud (const juce::String& text);
    void setFloorMode (bool floor);
    void togglePopover (PopoverKind kind);
    void syncPedalEditorControls();
    void layoutAll();

    float stageScale() const noexcept { return static_cast<float> (getWidth()) / 1550.0f; }
    juce::Rectangle<float> toScreen (float x, float y, float w, float h) const noexcept;
    juce::Rectangle<int> toScreenInt (float x, float y, float w, float h) const noexcept;

    void paintHeadScene (juce::Graphics& g);
    void paintFloorScene (juce::Graphics& g);
    void paintGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 22.0f) const;
    void paintMiniPedalKnobs (juce::Graphics& g, int type, juce::Rectangle<float> pedalRect) const;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    static juce::String pedalName (int type);
    const juce::Image& pedalImage (int type, bool active) const;
    static const char* pedalBypassParamId (int type);
    std::vector<juce::Rectangle<float>> computeBoardSlotRects (int count) const;
    juce::Rectangle<float> computeEditorPedalStageRect() const;

    MiAmpAudioProcessor& audioProcessor;
    GlassLookAndFeel glassLaf;
    BassImages imgs;

    bool isFloorMode = false;
    bool editorOpen = true;
    bool floorTunerOpen = false;
    PopoverKind activePopover = PopoverKind::None;
    int selectedPedalType = 1; // XV Bass

    // Head right menu (.menu)
    juce::TextButton btnInstGuitar { "Guitar" }, btnInstBass { "Bass" };
    juce::TextButton btnMainPresets { "PRESETS" }, btnMainTuner { "AFINADOR" }, btnMainPedals { "PEDALES" };

    // Head top-left dock (.audio-dock)
    juce::TextButton btnDockCab { "CAB 8x10" }, btnDockDrop { "DROP" };

    // Head physical knobs & switches
    std::unique_ptr<ImageRotaryKnob> headKnobs[5];
    std::unique_ptr<ImageToggleSwitch> headSwitches[3];

    // Floor Editor controls (.edit)
    std::unique_ptr<ImageRotaryKnob> editorKnobs[7];
    juce::TextButton btnEditClose { "x" };
    juce::TextButton btnMoveLeft { "<" }, btnMoveRight { ">" }, btnRemovePedal { "Quitar" };

    // Floor Palette (.palette)
    std::unique_ptr<PalettePedalButton> paletteItems[5];
    juce::TextButton btnFloorPresets { "PRESETS" }, btnFloorTuner { "AFINADOR" };
    juce::TextButton btnFloorCab { "CAB 8x10" }, btnFloorDrop { "DROP" }, btnBackToAmp { "VOLVER AL AMPLI" };
    juce::TextButton btnFloorTunerMute { "MUTE" };

    // Floating glass popover (.popover)
    std::unique_ptr<GlassPopover> popover;

    juce::String hudText;
    int hudTicks = 0;
    xvdsp::TunerReading tunerReading;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiAmpAudioProcessorEditor)
};
