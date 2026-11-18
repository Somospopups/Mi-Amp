#pragma once
#include <JuceHeader.h>
#include "ToneFinderEngine.h"
#include "Localization.h"
#include "AutoUpdater.h"
#include "AmpCatalogSpecs.h"
#include "MiAmpGlassTheme.h"
#include "RigStageComponents.h"
#include "StudioFxModal.h"
#include "CabPanels.h"
#include "PracticeStudioView.h"

class MiAmpAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit MiAmpAudioProcessorEditor (MiAmpAudioProcessor&);
    ~MiAmpAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    static juce::String formatNoteName (const char* noteName, int octave, bool useSolfege);
    static juce::String formatNoteNameOnly (const char* noteName, bool useSolfege);

private:
    enum class PopoverKind { None, Presets, Tuner, Drop, Config, ToneFinder };
    enum class KnobKind    { GuitarHead, BassHead, Pedal };
    enum class ArtKey
    {
        MiOrange = 0, MiGreenDrive, MiDualCrunch, MiSustain, MiIceComp, MiPhaser,
        MiSkyChorus, MiViolet, MiPearl, MiSilent, MiSubOctave, MiGraphicEq, MiStudio,
        MiGold, BassFood, MiBassDriver, MiOlive, Mi87, MiLimePhase, MiDeepChorus,
        MiPolyOctave, MiBassEq, Count
    };

    struct Images
    {
        juce::Image introSky, introLogo, popupsCredit, guitarKnobRing;
        juce::Image skateparkTall, mic57, mic87;
        juce::Image guitarBg, bassBg, bassKnobCap, toggleUp, toggleDown;
        juce::Image floorBg, boardSurface;
        juce::Image xvActive, xvBypass, xvKnobPtr, xvCabBass, xvCabPunk, xvCabClean;
        juce::Image pedBodies[static_cast<int> (ArtKey::Count)];
        juce::Image pedKnobs[static_cast<int> (ArtKey::Count)];
        juce::Image pedSliderCap;
    };

    class GlassLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        GlassLookAndFeel();
        void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg,
                                   bool highlighted, bool down) override;
        void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool highlighted, bool down) override;
        void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                               float sliderPos, float minSliderPos, float maxSliderPos,
                               const juce::Slider::SliderStyle, juce::Slider&) override;
        void drawScrollbar (juce::Graphics& g, juce::ScrollBar& scrollbar, int x, int y, int width, int height,
                            bool isScrollbarVertical, int thumbStartPosition, int thumbSize,
                            bool isMouseOver, bool isMouseDown) override;
        void fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& te) override;
        void drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& te) override;
    };

    class ImageRotaryKnob final : public juce::Component
    {
    public:
        ImageRotaryKnob (juce::AudioProcessorValueTreeState& state,
                         const juce::String& paramId,
                         const juce::String& labelText,
                         juce::Image knobImg,
                         juce::Image ringImg,
                         KnobKind kind,
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
        juce::Image image, ringImage;
        KnobKind knobKind = KnobKind::Pedal;
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
                           bool invertDownState,
                           std::function<void(const juce::String&)> onHud);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::AudioProcessorValueTreeState& apvts;
        juce::String pid;
        juce::String label;
        juce::Image imgUp, imgDown;
        bool invertDown = false;
        std::function<void(const juce::String&)> hudCallback;
    };

    class TrashIconButton final : public juce::Button
    {
    public:
        TrashIconButton();
        void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
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
                      std::function<void(juce::Point<float>)> onDragEnd,
                      std::function<void(juce::Graphics&, juce::Rectangle<float>)> onPaintArt = nullptr);

    private:
        juce::String name;
        juce::Image art;
        bool alreadyInChain = false;
        bool dragged = false;
        juce::Point<int> downPos;
        std::function<void()> clickFn;
        std::function<void(juce::Point<float>)> dragMoveFn, dragEndFn;
        std::function<void(juce::Graphics&, juce::Rectangle<float>)> paintArtFn;
    };

    class PedalToggleOverlay final : public juce::Component
    {
    public:
        PedalToggleOverlay (juce::AudioProcessorValueTreeState& state,
                            juce::Image upImg, juce::Image downImg,
                            std::function<void(const juce::String&)> onHud);
        void setConfig (const juce::String& paramId, const juce::String& labelText, bool isThreeWay,
                        const juce::String& opt0, const juce::String& opt1, const juce::String& opt2);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        juce::AudioProcessorValueTreeState& apvts;
        juce::Image imgUp, imgDown;
        juce::String pid, label;
        juce::String opts[3];
        bool threeWay = false;
        std::function<void(const juce::String&)> hudCallback;
    };

    class EqFader final : public juce::Component
    {
    public:
        EqFader (juce::AudioProcessorValueTreeState& state,
                 juce::Image capImg,
                 std::function<void(const juce::String&)> onHud);
        void setParam (const juce::String& paramId, const juce::String& bandLabel);
        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    private:
        juce::AudioProcessorValueTreeState& apvts;
        juce::Image capImage;
        juce::String pid, label;
        std::function<void(const juce::String&)> hudCallback;
        bool dragging = false;
        float dragStartVal = 50.0f;
        int dragStartY = 0;
        void setFromY (int yPx);
        juce::String formatDb (float val) const;
    };

    class GlassPopover;

    void loadImages();
    void timerCallback() override;
    void showHud (const juce::String& text);
    void finishIntro();
    void setFloorMode (bool floor);
    void switchInstrument (bool toBass);
    void togglePopover (PopoverKind kind);
    void closeFloatingOverlays (const char* except = nullptr);
    void cycleActiveHead (int delta);
    void applyLanguageTexts();
    void startAutoUpdateCheck (bool manualFromConfig = false);
    void beginAutoUpdateDownload (const miamp_updater::UpdateInfo& info);
    void dismissUpdateModal();
    void syncHeadControls();
    void rebuildRigHeadControls();
    void computeRigStageLayout (juce::Rectangle<float>& cabRectOut, juce::Rectangle<float>& headRectOut,
                                juce::Rectangle<float>& grilleRectOut) const;
    void setPracticeMode (bool practice);
    void syncPedalEditorControls();
    void syncPalettePositions();
    void layoutAll();

    float stageScale() const noexcept { return static_cast<float> (getWidth()) / 1550.0f; }
    juce::Rectangle<float> toScreen (float x, float y, float w, float h) const noexcept;
    juce::Rectangle<int> toScreenInt (float x, float y, float w, float h) const noexcept;
    juce::Point<float> editorPtToStage (juce::Point<float> editorPt) const noexcept;
    juce::Point<float> applyBoardTransform (juce::Point<float> stagePt) const noexcept;
    juce::Rectangle<float> applyBoardTransform (juce::Rectangle<float> stageRect) const noexcept;

    void paintHeadScene (juce::Graphics& g, float yOffsetStage = 0.0f);
    void paintRigOnly (juce::Graphics& g, float yOffsetStage = 0.0f, float opacity = 1.0f);
    void paintFloorScene (juce::Graphics& g);
    void paintGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float radius = 22.0f, float alpha = 1.0f) const;
    void paintMiniPedalKnobs (juce::Graphics& g, bool bassMode, int type, juce::Rectangle<float> pedalRect, bool dimmed = false) const;
    void paintPedalStateOverlays (juce::Graphics& g, bool bassMode, int type, juce::Rectangle<float> pedalRect, bool active) const;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    int computeDropInsertIndex (bool bassMode, juce::Point<float> stagePt, int excludePedal = -1) const;
    bool isOutsideBoardViewport (juce::Point<float> stagePt) const noexcept;
    static juce::String pedalName (bool bassMode, int type);
    const juce::Image& pedalImage (bool bassMode, int type, bool active) const;
    static const char* pedalBypassParamId (bool bassMode, int type);
    std::vector<juce::Rectangle<float>> computeBoardSlotRects (bool bassMode, const std::vector<int>& chain) const;
    juce::Rectangle<float> computeEditorPedalStageRect() const;

    MiAmpAudioProcessor& audioProcessor;
    GlassLookAndFeel glassLaf;
    Images imgs;

    bool isFloorMode = false;
    bool isPracticeMode = false;
    bool closeUpView = true; // true = Primer Plano (default, close to head), false = Rig & Mics (full stack + draggable mics)
    bool consoleOpen = false; // bottom console strip hidden by default
    bool cabDrawerOpen = false; // left cabinet picker & dual-mics drawer
    bool cabMicZoom = false; // full-size cab inside cabPickerPanel when MOVER EN CAB active
    float cabDrawerAnim = 0.0f; // smooth 0..1 pan animation for stage & skatepark
    bool editorOpen = true;
    bool floorTunerOpen = false;
    bool useSolfege = false; // false = C D E F G A B, true = Do Re Mi Fa Sol La Si
    PopoverKind activePopover = PopoverKind::None;
    float popoverAnim = 0.0f;
    bool popoverClosing = false;
    float rightMenuAnim = 1.0f;
    int selectedBassPedal = 1;
    int selectedGuitarPedal = 1;

    // #4 Elevación de Estudio Suave (0.62s Guitar <-> Bass transition, zero background zoom/flicker)
    bool instSwitching = false;
    float instSwitchAnim = 1.0f;
    juce::Image instOutSnapshot;
    bool standaloneStartupNoticeShown = false;
    bool standaloneFeedbackNoticeShown = false;

    // Intro animation (2.9s matching index.html)
    bool  introRunning = true;
    float introElapsed = 0.0f;
    float headSceneYOffsetStage = 0.0f;
    juce::TextButton btnSkipIntro { "SALTAR" };

    // Brand credit easter egg
    bool creditOpen = false;
    juce::TextButton btnCreditClose { "Cerrar" };

    // Smooth board transform animation (.board-group)
    float boardTx = 0.0f, boardTy = 0.0f, boardScale = 1.0f;

    // Smooth per-pedal slot & pickup animations on floor board
    std::array<juce::Rectangle<float>, 14> animPedalRects {};
    std::array<bool, 14> animPedalHasPos {};

    // Drag & Drop state on floor pedalboard
    bool  draggingPedalActive = false;
    bool  draggingFromPalette = false;
    bool  draggingOutsideBoard = false;
    int   draggingPedalType   = -1;
    int   dragInsertIndex     = -1;
    juce::Point<int>   dragStartPx;
    juce::Point<float> dragStagePos;
    juce::Point<float> dragRenderPos;
    float dragLiftScale = 1.0f;

    // Head right menu (.menu) - unified right glass panel
    juce::TextButton btnInstGuitar { "GUITARRA" }, btnInstBass { "BAJO" };
    juce::TextButton btnMainHeads { "CABEZALES" }, btnMainCabs { "CAJAS" };
    juce::TextButton btnMainPresets { "PRESETS" }, btnMainTuner { "AFINADOR" }, btnMainToneFinder { "BUSCAR TONO" },
                     btnMainPedals { "PEDALES" }, btnMainStudioFx { "STUDIO FX" }, btnMainPractice { "ENSAYO" }, btnMainConsole { "CONSOLA" };
    juce::TextButton btnDockDrop { "DROP" }, btnDockConfig { "CONFIG" };
    juce::TextButton btnViewCloseUp { "PRIMER PLANO" }, btnViewRigMics { "RIG & MICS" };

    // Modular v25 Rig Controls + Console Strip + Dual Mics + Overlays
    std::vector<std::unique_ptr<miamp_rig::RigHeadKnob>> rigKnobs;
    std::vector<std::unique_ptr<miamp_rig::RigHeadSwitch>> rigSwitches;
    std::vector<std::unique_ptr<miamp_rig::RigHeadSlider>> rigSliders;
    int lastBuiltRigModel = -1;
    bool lastBuiltRigBass = false;

    std::unique_ptr<miamp_rig::RigMicBody> mic1Body, mic2Body;
    std::unique_ptr<miamp_rig::HeadSideArrowButton> btnHeadSidePrev, btnHeadSideNext;
    std::unique_ptr<miamp_rig::CabPickerPanel> cabPickerPanel;
    std::unique_ptr<miamp_rig::RigBrowserOverlay> rigBrowser;
    std::unique_ptr<miamp_rig::StudioFxModal> studioFxModal;

    // Bottom Analog Console Controls
    juce::TextButton btnAmpPrev { "<" }, btnAmpNext { ">" }, btnAmpBrowse { "AMP" };
    juce::TextButton btnCabPrev { "<" }, btnCabNext { ">" }, btnCabBrowse { "CABINET" }, btnCabLoadWav { ".WAV" };
    juce::TextButton btnMic1On { "MI-57" }, btnMic1Dist { "CLS" }, btnMic1Phase { "Ø" };
    juce::TextButton btnMic2On { "MI-87" }, btnMic2Dist { "MID" }, btnMic2Phase { "Ø" };
    juce::TextButton btnConsoleStudioFx { "STUDIO FX" }, btnConsolePower { "POWER" };
    std::unique_ptr<miamp_rig::StudioMiniKnob> conKnobs[5]; // Gain, Bass, Mid, Treble, Master
    std::unique_ptr<miamp_rig::StudioMiniKnob> conMicMixKnob, conRoomKnob;

    // Legacy Mint Head physical knobs & switches (used when MI AMP Classic Mint selected)
    std::unique_ptr<ImageRotaryKnob> guitarHeadKnobs[19];
    std::unique_ptr<ImageToggleSwitch> guitarHeadSwitches[3];
    std::unique_ptr<ImageRotaryKnob> bassHeadKnobs[5];
    std::unique_ptr<ImageToggleSwitch> bassHeadSwitches[3];

    // Floor Editor controls (.edit)
    std::unique_ptr<ImageRotaryKnob> editorKnobs[7];
    std::unique_ptr<EqFader> eqFaders[7];
    std::unique_ptr<PedalToggleOverlay> pedalToggles[3];
    juce::TextButton btnEditClose { juce::String::fromUTF8 (u8"\u00d7") };
    juce::TextButton btnMoveLeft { "<" }, btnMoveRight { ">" };
    TrashIconButton btnTrashPedal;

    // Floor top-right Amp vs Pedals-Only switch
    juce::TextButton btnHeadModeAmp { "CON AMPLI" };
    juce::TextButton btnHeadModePedals { "SOLO PEDALES" };

    // Floor Palette (.palette) - smooth scrollable horizontal carousel
    juce::Component paletteStrip;
    juce::TextButton btnPaletteLeft { "<" }, btnPaletteRight { ">" };
    float paletteScrollTarget = 0.0f;
    float paletteScrollCur    = 0.0f;
    std::unique_ptr<PalettePedalButton> paletteItems[14];
    juce::TextButton btnFloorPresets { "PRESETS" }, btnFloorTuner { "AFINADOR" };
    juce::TextButton btnFloorToneFinder { "BUSCAR TONO" }, btnFloorDrop { "DROP" }, btnFloorConfig { "CONFIG" }, btnBackToAmp { "VOLVER AL AMPLI" }, btnFloorPractice { "ENSAYO" };
    juce::TextButton btnFloorTunerMute { "MUTE" }, btnFloorTunerNotation { "DO RE MI" };

    std::unique_ptr<miamp_practice_ui::PracticeStudioView> practiceView;

    // Floating glass popover (.popover / .floor-popover)
    std::unique_ptr<GlassPopover> popover;

    class UpdateOverlayComponent;

    // Auto-Updater (background check + centered glass modal; never a banner above amp head)
    std::unique_ptr<miamp_updater::CheckThread> updateCheckThread;
    std::unique_ptr<miamp_updater::DownloadThread> updateDownloadThread;
    std::unique_ptr<UpdateOverlayComponent> updateOverlay;
    bool updateModalVisible = false;
    bool updateDownloading = false;
    bool updateInstalling = false;
    bool updateFailed = false;
    float updateProgress = 0.0f;
    miamp_updater::UpdateInfo pendingUpdate;
    juce::TextButton btnUpdateNow { "ACTUALIZAR AHORA" }, btnUpdateLater { "MAS TARDE" };

    juce::String hudText;
    int hudTicks = 0;
    xvdsp::TunerReading tunerReading;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiAmpAudioProcessorEditor)
};
