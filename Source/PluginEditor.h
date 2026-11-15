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

    struct Images
    {
        juce::Image guitarBg, bassBg, bassKnobCap, toggleUp, toggleDown;
        juce::Image floorBg, boardSurface;
        juce::Image xvActive, xvBypass, xvKnobPtr, xvCabBass, xvCabPunk, xvCabClean;
        juce::Image compActive, compOff, driveActive, driveOff, fuzzActive, fuzzOff;
        juce::Image chActive, chOff, phActive, phOff, delayActive, delayOff, rvActive, rvOff;
        juce::Image compKnob[3], driveKnob[3], chKnob[3], phKnob[3];
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
                         bool isHeadStyle,
                         std::function<void(const juce::String&)> onHud,
                         std::function<void()> onInteract = nullptr);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void setParam (const juce::String& newId, const juce::String& newLabel, juce::Image newImg);

    private:
        juce::AudioProcessorValueTreeState& apvts;
        juce::String pid;
        juce::String label;
        juce::Image image;
        bool headStyle = false;
        std::function<void(const juce::String&)> hudCallback;
        std::function<void()> interactCallback;
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
        void setConfig (const juce::String& newId, const juce::String& newLabel);

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
        PalettePedalButton (const juce::String& title, juce::Image img,
                            std::function<void()> onClick,
                            std::function<void(juce::Point<float>)> onDragMove = nullptr,
                            std::function<void(juce::Point<float>)> onDragEnd  = nullptr);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void setInChain (bool inChain) { alreadyInChain = inChain; repaint(); }
        void setData (const juce::String& title, juce::Image img,
                      std::function<void()> onClick,
                      std::function<void(juce::Point<float>)> onDragMove,
                      std::function<void(juce::Point<float>)> onDragEnd);

    private:
        juce::String name;
        juce::Image art;
        bool alreadyInChain = false;
        bool dragged = false;
        juce::Point<int> downPos;
        std::function<void()> clickFn;
        std::function<void(juce::Point<float>)> dragMoveFn, dragEndFn;
    };

    class GlassPopover;

    void loadImages();
    void timerCallback() override;
    void showHud (const juce::String& text);
    void setFloorMode (bool floor);
    void switchInstrument (bool toBass);
    void togglePopover (PopoverKind kind);
    void syncHeadControls();
    void syncPedalEditorControls();
    void layoutAll();

    float stageScale() const noexcept { return static_cast<float> (getWidth()) / 1550.0f; }
    juce::Rectangle<float> toScreen (float x, float y, float w, float h) const noexcept;
    juce::Rectangle<int> toScreenInt (float x, float y, float w, float h) const noexcept;
    juce::Point<float> editorPtToStage (juce::Point<float> editorPt) const noexcept;

    void paintHeadScene (juce::Graphics& g);
    void paintFloorScene (juce::Graphics& g);
    void paintGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 22.0f) const;
    void paintMiniPedalKnobs (juce::Graphics& g, bool bassMode, int type, juce::Rectangle<float> pedalRect) const;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    int computeDropInsertIndex (bool bassMode, juce::Point<float> stagePt, int excludePedal = -1) const;
    static juce::String pedalName (bool bassMode, int type);
    const juce::Image& pedalImage (bool bassMode, int type, bool active) const;
    static const char* pedalBypassParamId (bool bassMode, int type);
    std::vector<juce::Rectangle<float>> computeBoardSlotRects (bool bassMode, const std::vector<int>& chain) const;
    juce::Rectangle<float> computeEditorPedalStageRect() const;

    MiAmpAudioProcessor& audioProcessor;
    GlassLookAndFeel glassLaf;
    Images imgs;

    bool isFloorMode = false;
    bool editorOpen = true;
    bool floorTunerOpen = false;
    PopoverKind activePopover = PopoverKind::None;
    int selectedBassPedal = 1;
    int selectedGuitarPedal = 1;

    // Drag & Drop state on floor pedalboard
    bool  draggingPedalActive = false;
    bool  draggingFromPalette = false;
    int   draggingPedalType   = -1;
    int   dragInsertIndex     = -1;
    juce::Point<int>   dragStartPx;
    juce::Point<float> dragStagePos;

    // Head right menu (.menu)
    juce::TextButton btnInstGuitar { "Guitar" }, btnInstBass { "Bass" };
    juce::TextButton btnMainPresets { "PRESETS" }, btnMainTuner { "AFINADOR" }, btnMainPedals { "PEDALES" };

    // Head top-left dock (.audio-dock)
    juce::TextButton btnDockCab { "CAB" }, btnDockDrop { "DROP" };
    juce::TextButton btnDockCh  { "CH2" }, btnDockOs { "4x OS" }, btnDockGate { "GATE" };

    // Head physical knobs & switches (up to 19 knobs in Guitar mode, 5 in Bass mode)
    std::unique_ptr<ImageRotaryKnob> guitarHeadKnobs[19];
    std::unique_ptr<ImageToggleSwitch> guitarHeadSwitches[3];
    std::unique_ptr<ImageRotaryKnob> bassHeadKnobs[5];
    std::unique_ptr<ImageToggleSwitch> bassHeadSwitches[3];

    // Floor Editor controls (.edit)
    std::unique_ptr<ImageRotaryKnob> editorKnobs[7];
    juce::TextButton btnEditClose { "x" };
    juce::TextButton btnMoveLeft { "<" }, btnMoveRight { ">" }, btnRemovePedal { "Quitar" };

    // Floor Palette (.palette) - up to 7 pedals in Guitar mode, 5 in Bass mode
    std::unique_ptr<PalettePedalButton> paletteItems[7];
    juce::TextButton btnFloorPresets { "PRESETS" }, btnFloorTuner { "AFINADOR" };
    juce::TextButton btnFloorCab { "CAB" }, btnFloorDrop { "DROP" }, btnBackToAmp { "VOLVER AL AMPLI" };
    juce::TextButton btnFloorTunerMute { "MUTE" };

    // Floating glass popover (.popover)
    std::unique_ptr<GlassPopover> popover;

    juce::String hudText;
    int hudTicks = 0;
    xvdsp::TunerReading tunerReading;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiAmpAudioProcessorEditor)
};
