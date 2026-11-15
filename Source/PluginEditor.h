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
    enum class ViewMode { Head, Pedals };
    enum class ModalMode { None, Presets, Drop, Tuner };

    struct BassImages
    {
        juce::Image bassBg, bassKnobCap, toggleUp, toggleDown;
        juce::Image floorBg, boardSurface;
        juce::Image xvActive, xvBypass, xvKnobPtr, xvCabBass, xvCabPunk, xvCabClean;
        juce::Image compActive, compOff, chActive, chOff, phActive, phOff, rvActive, rvOff;
        juce::Image compKnob[3], chKnob[3], phKnob[3];
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
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void setImage (juce::Image img) { image = std::move (img); repaint(); }
        void setParamId (const juce::String& newId);
        void refreshFromParam();

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

    // Modal overlay helper components
    class ModalOverlay;

    void loadImages();
    void timerCallback() override;
    void showHud (const juce::String& text);
    void setViewMode (ViewMode m);
    void setModalMode (ModalMode m);
    void syncPedalEditorControls();
    void layoutHeadControls();
    void layoutPedalControls();
    void paintHeadBackground (juce::Graphics& g, juce::Rectangle<float> area);
    void paintPedalboardView (juce::Graphics& g, juce::Rectangle<float> area);
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    static juce::String pedalName (int type);
    const juce::Image& pedalImage (int type, bool active) const;
    static const char* pedalBypassParamId (int type);
    std::vector<juce::Rectangle<float>> computeSlotRects (juce::Rectangle<float> boardRect, int count) const;
    juce::Rectangle<float> computeEditorPedalRect (juce::Rectangle<float> area) const;

    MiAmpAudioProcessor& audioProcessor;
    BassImages imgs;

    ViewMode viewMode = ViewMode::Head;
    ModalMode modalMode = ModalMode::None;
    int selectedPedalType = 1; // 1 = XV Bass by default

    // Top bar controls
    juce::TextButton btnHead { "CABEZAL" }, btnPedals { "PEDALES" };
    juce::TextButton btnPresets { "PRESETS" }, btnDrop { "DROP" }, btnTuner { "AFINADOR" }, btnCab { "CAB 8x10" };

    // Bass Head controls
    std::unique_ptr<ImageRotaryKnob> headKnobs[5];
    std::unique_ptr<ImageToggleSwitch> headSwitches[3];

    // Pedal Editor controls (up to 7 knobs for XV, 3 for other pedals)
    std::unique_ptr<ImageRotaryKnob> pedalKnobs[7];
    juce::TextButton btnMoveLeft { "<" }, btnMoveRight { ">" }, btnRemovePedal { "Quitar" };
    juce::TextButton addButtons[5];

    // Overlay modal
    std::unique_ptr<ModalOverlay> modalOverlay;

    // Live HUD & Tuner snapshot
    juce::String hudText;
    int hudTicks = 0;
    xvdsp::TunerReading tunerReading;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiAmpAudioProcessorEditor)
};
