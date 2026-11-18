#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "MiAmpGlassTheme.h"
#include "Localization.h"
#include "PedalCatalogSpecs.h"

namespace miamp_practice_ui
{

inline juce::Colour trackAccentColour (int trackIdx) noexcept
{
    static const juce::Colour kCols[miamp_practice::kNumTracks] = {
        juce::Colour::fromRGB (255, 134, 152), // 0: VOZ (Coral)
        juce::Colour::fromRGB (255, 186, 98),  // 1: GUITARRA (Amber)
        juce::Colour::fromRGB (126, 232, 189), // 2: BAJO (Mint)
        juce::Colour::fromRGB (112, 199, 255), // 3: BATERIA (Sky)
        juce::Colour::fromRGB (193, 156, 255), // 4: OTROS / FX (Lavender)
        juce::Colour::fromRGB (255, 224, 118), // 5: MI GUITARRA / BAJO (Gold)
        juce::Colour::fromRGB (138, 235, 221)  // 6: METRONOMO (Teal)
    };
    return kCols[juce::jlimit (0, miamp_practice::kNumTracks - 1, trackIdx)];
}

inline juce::String formatTimeMmSs (double sec)
{
    const int total = juce::jmax (0, static_cast<int> (std::floor (sec)));
    const int m = total / 60;
    const int s = total % 60;
    return juce::String (m).paddedLeft ('0', 2) + ":" + juce::String (s).paddedLeft ('0', 2);
}

class PracticeStudioView final : public juce::Component
{
public:
    struct Callbacks
    {
        std::function<void()> onBackToAmp;
        std::function<void(bool)> onSwitchInstrument;
        std::function<void()> onOpenTuner;
        std::function<void()> onOpenAmpCab;
        std::function<void()> onOpenFloorPedals;
        std::function<void()> onOpenPresets;
        std::function<void()> onOpenToneFinder;
        std::function<void(const juce::String&)> onHud;
    };

    PracticeStudioView (MiAmpAudioProcessor& proc, Callbacks cbs)
        : processor (proc), engine (proc.getPracticeEngine()), callbacks (std::move (cbs))
    {
        setInterceptsMouseClicks (true, true);

        auto addGlassBtn = [this] (juce::TextButton& b, const juce::String& compId = {})
        {
            if (compId.isNotEmpty()) b.setComponentID (compId);
            addAndMakeVisible (b);
        };

        // Top Navigation Bar
        addGlassBtn (btnBackToAmp, "mintPill");
        btnBackToAmp.onClick = [this] { if (callbacks.onBackToAmp) callbacks.onBackToAmp(); };

        addGlassBtn (btnInstGuitar);
        addGlassBtn (btnInstBass);
        btnInstGuitar.onClick = [this] { if (callbacks.onSwitchInstrument) callbacks.onSwitchInstrument (false); syncTextsAndButtons(); };
        btnInstBass.onClick   = [this] { if (callbacks.onSwitchInstrument) callbacks.onSwitchInstrument (true);  syncTextsAndButtons(); };

        addGlassBtn (btnQuickTuner);
        btnQuickTuner.onClick = [this] { if (callbacks.onOpenTuner) callbacks.onOpenTuner(); };

        addGlassBtn (btnQuickAmpCab);
        btnQuickAmpCab.onClick = [this] { if (callbacks.onOpenAmpCab) callbacks.onOpenAmpCab(); };

        addGlassBtn (btnQuickPedals);
        btnQuickPedals.onClick = [this]
        {
            pedalModalOpen = ! pedalModalOpen;
            rebuildPedalPopup();
            resized();
            repaint();
        };

        addGlassBtn (btnQuickPresets);
        btnQuickPresets.onClick = [this] { if (callbacks.onOpenPresets) callbacks.onOpenPresets(); };

        addGlassBtn (btnQuickToneFinder);
        btnQuickToneFinder.onClick = [this] { if (callbacks.onOpenToneFinder) callbacks.onOpenToneFinder(); };

        // Left Playlist Sidebar
        addGlassBtn (btnAddSong, "mintPill");
        btnAddSong.onClick = [this] { openAddSongChooser(); };

        addGlassBtn (btnAddFreeTake);
        btnAddFreeTake.onClick = [this]
        {
            engine.addFreeTakeSession();
            rebuildPlaylistRows();
            repaint();
        };

        addGlassBtn (btnModeContinuous);
        addGlassBtn (btnModePauseChoose);
        btnModeContinuous.onClick = [this]
        {
            engine.setEndMode (miamp_practice::PlaylistEndMode::Continuous);
            syncTextsAndButtons();
        };
        btnModePauseChoose.onClick = [this]
        {
            engine.setEndMode (miamp_practice::PlaylistEndMode::PauseAndChoose);
            syncTextsAndButtons();
        };

        playlistViewport.setViewedComponent (&playlistHolder, false);
        playlistViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (playlistViewport);

        // AI Action Button
        addGlassBtn (btnAiAction, "mintPill");
        btnAiAction.onClick = [this]
        {
            if (engine.isAiBusy()) return;
            if (! miamp_practice::isDemucsModelReady())
                engine.startModelDownload();
            else if (engine.getActiveSongIndex() >= 0)
                engine.startAiSeparationForSong (engine.getActiveSongIndex());
            syncTextsAndButtons();
        };

        // Transport Controls
        addGlassBtn (btnRewind);
        btnRewind.onClick = [this] { engine.seekToSeconds (0.0); repaint(); };

        addGlassBtn (btnPlayPause, "mintPill");
        btnPlayPause.onClick = [this]
        {
            if (engine.isPlaying()) engine.pause();
            else                    engine.play();
            syncTextsAndButtons();
        };

        addGlassBtn (btnStop);
        btnStop.onClick = [this] { engine.stop(); syncTextsAndButtons(); };

        addGlassBtn (btnRec, "recPill");
        btnRec.onClick = [this] { engine.toggleRecord(); syncTextsAndButtons(); };

        addGlassBtn (btnClearTake);
        btnClearTake.onClick = [this] { engine.clearUserTake(); repaint(); };

        // Loop A-B
        addGlassBtn (btnSetA);
        btnSetA.onClick = [this] { engine.setLoopAHere(); repaint(); };
        addGlassBtn (btnSetB);
        btnSetB.onClick = [this] { engine.setLoopBHere(); repaint(); };
        addGlassBtn (btnLoopToggle);
        btnLoopToggle.onClick = [this]
        {
            engine.setLoopEnabled (! engine.isLoopEnabled());
            syncTextsAndButtons();
        };

        // Speed & Pitch
        addGlassBtn (btnSpeedMinus);
        addGlassBtn (btnSpeedPlus);
        btnSpeedMinus.onClick = [this] { engine.setPlaybackSpeed (engine.getPlaybackSpeed() - 0.05f); syncTextsAndButtons(); };
        btnSpeedPlus.onClick  = [this] { engine.setPlaybackSpeed (engine.getPlaybackSpeed() + 0.05f); syncTextsAndButtons(); };

        addGlassBtn (btnPitchMinus);
        addGlassBtn (btnPitchPlus);
        btnPitchMinus.onClick = [this] { engine.setPitchSemitones (engine.getPitchSemitones() - 1); syncTextsAndButtons(); };
        btnPitchPlus.onClick  = [this] { engine.setPitchSemitones (engine.getPitchSemitones() + 1); syncTextsAndButtons(); };

        // Metronome & Pre-Count
        addGlassBtn (btnMetToggle);
        btnMetToggle.onClick = [this] { engine.setMetronomeEnabled (! engine.isMetronomeEnabled()); syncTextsAndButtons(); };
        addGlassBtn (btnBpmMinus);
        addGlassBtn (btnBpmPlus);
        btnBpmMinus.onClick = [this] { engine.setBpm (engine.getBpm() - 2.0f); syncTextsAndButtons(); };
        btnBpmPlus.onClick  = [this] { engine.setBpm (engine.getBpm() + 2.0f); syncTextsAndButtons(); };
        addGlassBtn (btnTapTempo);
        btnTapTempo.onClick = [this] { engine.tapTempo(); syncTextsAndButtons(); };
        addGlassBtn (btnPreCount);
        btnPreCount.onClick = [this] { engine.setPreCountEnabled (! engine.isPreCountEnabled()); syncTextsAndButtons(); };

        // Per-Track Controls (M, S, EXP, Vol)
        for (int t = 0; t < miamp_practice::kNumTracks; ++t)
        {
            auto& row = trackCtrls[static_cast<size_t> (t)];
            addAndMakeVisible (row.btnMute);
            addAndMakeVisible (row.btnSolo);
            addAndMakeVisible (row.btnExp);
            addAndMakeVisible (row.volSlider);

            row.btnMute.setButtonText ("M");
            row.btnSolo.setButtonText ("S");
            row.btnExp.setButtonText ("EXP");
            row.btnMute.setClickingTogglesState (true);
            row.btnSolo.setClickingTogglesState (true);
            row.btnExp.setClickingTogglesState (true);
            row.btnExp.setToggleState (engine.isTrackExportChecked (t), juce::dontSendNotification);

            row.btnMute.onClick = [this, t]
            {
                engine.setTrackMuted (t, trackCtrls[static_cast<size_t> (t)].btnMute.getToggleState());
                repaint();
            };
            row.btnSolo.onClick = [this, t]
            {
                engine.setTrackSolo (t, trackCtrls[static_cast<size_t> (t)].btnSolo.getToggleState());
                repaint();
            };
            row.btnExp.onClick = [this, t]
            {
                engine.setTrackExportChecked (t, trackCtrls[static_cast<size_t> (t)].btnExp.getToggleState());
            };

            row.volSlider.setSliderStyle (juce::Slider::LinearHorizontal);
            row.volSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
            row.volSlider.setRange (0.0, 1.5, 0.01);
            row.volSlider.setValue (engine.getTrackVolume (t), juce::dontSendNotification);
            row.volSlider.onValueChange = [this, t]
            {
                engine.setTrackVolume (t, static_cast<float> (trackCtrls[static_cast<size_t> (t)].volSlider.getValue()));
            };
        }

        // Bottom Selective Export Bar
        addGlassBtn (btnExpSelAll);
        addGlassBtn (btnExpSelStems);
        addGlassBtn (btnExportMix, "mintPill");
        addGlassBtn (btnExportStems);
        addGlassBtn (btnExportTake);

        btnExpSelAll.onClick = [this]
        {
            for (int t = 0; t < miamp_practice::kNumTracks; ++t)
            {
                const bool chk = (t != miamp_practice::kTrackMet);
                engine.setTrackExportChecked (t, chk);
                trackCtrls[static_cast<size_t> (t)].btnExp.setToggleState (chk, juce::dontSendNotification);
            }
        };
        btnExpSelStems.onClick = [this]
        {
            for (int t = 0; t < miamp_practice::kNumTracks; ++t)
            {
                const bool chk = (t < miamp_practice::kNumStems);
                engine.setTrackExportChecked (t, chk);
                trackCtrls[static_cast<size_t> (t)].btnExp.setToggleState (chk, juce::dontSendNotification);
            }
        };

        btnExportMix.onClick   = [this] { exportMixFlow(); };
        btnExportStems.onClick = [this] { exportStemsFlow(); };
        btnExportTake.onClick  = [this] { exportTakeFlow(); };

        // End-of-song Choice Modal ("Pausar y Elegir")
        addChildComponent (btnEndRepeat);
        addChildComponent (btnEndNext);
        addChildComponent (btnEndClose);
        btnEndRepeat.setComponentID ("mintPill");
        btnEndClose.setComponentID ("popoverClose");
        btnEndRepeat.onClick = [this] { engine.dismissEndChoiceAndRepeat(); syncTextsAndButtons(); };
        btnEndNext.onClick   = [this]
        {
            engine.dismissEndChoiceAndNext();
            autoLoadLinkedPresetIfAny();
            rebuildPlaylistRows();
            syncTextsAndButtons();
        };
        btnEndClose.onClick  = [this] { engine.dismissEndChoiceOnly(); syncTextsAndButtons(); };

        // Quick Pedals Popup inside Ensayo
        addChildComponent (pedalModalClose);
        pedalModalClose.setComponentID ("popoverClose");
        pedalModalClose.onClick = [this] { pedalModalOpen = false; resized(); repaint(); };

        addChildComponent (btnOpenFullFloor);
        btnOpenFullFloor.setComponentID ("mintPill");
        btnOpenFullFloor.onClick = [this]
        {
            pedalModalOpen = false;
            if (callbacks.onOpenFloorPedals) callbacks.onOpenFloorPedals();
        };

        pedalViewport.setViewedComponent (&pedalHolder, false);
        pedalViewport.setScrollBarsShown (true, false);
        addChildComponent (pedalViewport);

        juce::Component::SafePointer<PracticeStudioView> safeSelf (this);
        engine.onStateChanged = [safeSelf]
        {
            if (safeSelf == nullptr) return;
            safeSelf->rebuildPlaylistRows();
            safeSelf->syncTextsAndButtons();
            const auto& st = safeSelf->engine.getAiStatusText();
            if (! safeSelf->engine.isAiBusy() && st.isNotEmpty() && safeSelf->callbacks.onHud)
                safeSelf->callbacks.onHud (st);
            safeSelf->repaint();
        };
        engine.onSongAutoAdvanced = [safeSelf] (int)
        {
            if (safeSelf == nullptr) return;
            safeSelf->autoLoadLinkedPresetIfAny();
            safeSelf->rebuildPlaylistRows();
            safeSelf->syncTextsAndButtons();
            safeSelf->repaint();
        };

        // Load initial song if playlist persisted from previous session
        if (engine.getActiveSongIndex() >= 0 && engine.getLoadedStemsSnapshot() == nullptr)
            engine.loadSongAtIndex (engine.getActiveSongIndex(), false);

        rebuildPlaylistRows();
        syncTextsAndButtons();
    }

    ~PracticeStudioView() override
    {
        engine.onStateChanged = nullptr;
        engine.onSongAutoAdvanced = nullptr;
    }

    void updateFromTimer (const xvdsp::TunerReading& tr, bool solfege)
    {
        tunerReading = tr;
        useSolfege = solfege;

        engine.updateLiveRecordingPeaksIfNeeded();
        if (engine.pollEndOfSongEvent())
        {
            rebuildPlaylistRows();
            syncTextsAndButtons();
        }
        if (isVisible())
        {
            syncTransportDynamicStates();
            repaint();
        }
    }

    void syncTextsAndButtons()
    {
        const bool bass = processor.isBassMode();
        btnBackToAmp.setButtonText       (miamp_i18n::tr (miamp_i18n::Msg::PracticeBackToAmp));
        btnInstGuitar.setButtonText      (miamp_i18n::tr (miamp_i18n::Msg::BtnGuitar));
        btnInstBass.setButtonText        (miamp_i18n::tr (miamp_i18n::Msg::BtnBass));
        btnInstGuitar.setToggleState     (! bass, juce::dontSendNotification);
        btnInstBass.setToggleState       (bass,   juce::dontSendNotification);

        btnQuickTuner.setButtonText      (miamp_i18n::tr (miamp_i18n::Msg::BtnTuner));
        btnQuickAmpCab.setButtonText     (miamp_i18n::tr (miamp_i18n::Msg::PracticeAmpCab));
        btnQuickPedals.setButtonText     (miamp_i18n::tr (miamp_i18n::Msg::BtnPedals));
        btnQuickPresets.setButtonText    (miamp_i18n::tr (miamp_i18n::Msg::BtnPresets));
        btnQuickToneFinder.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnToneFinder));

        btnAddSong.setButtonText         (miamp_i18n::tr (miamp_i18n::Msg::PracticeAddSong));
        btnAddFreeTake.setButtonText     (miamp_i18n::tr (miamp_i18n::Msg::PracticeFreeTake));
        btnModeContinuous.setButtonText  (miamp_i18n::tr (miamp_i18n::Msg::PracticeModeContinuous));
        btnModePauseChoose.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::PracticeModePauseChoose));
        btnModeContinuous.setToggleState  (engine.getEndMode() == miamp_practice::PlaylistEndMode::Continuous, juce::dontSendNotification);
        btnModePauseChoose.setToggleState (engine.getEndMode() == miamp_practice::PlaylistEndMode::PauseAndChoose, juce::dontSendNotification);

        btnRewind.setButtonText ("|<");
        btnStop.setButtonText   ("STOP");
        btnSetA.setButtonText   ("SET A");
        btnSetB.setButtonText   ("SET B");
        btnLoopToggle.setButtonText ("LOOP A-B");
        btnSpeedMinus.setButtonText ("-");
        btnSpeedPlus.setButtonText  ("+");
        btnPitchMinus.setButtonText ("-");
        btnPitchPlus.setButtonText  ("+");
        btnBpmMinus.setButtonText   ("-");
        btnBpmPlus.setButtonText    ("+");
        btnTapTempo.setButtonText   ("TAP");
        btnPreCount.setButtonText   ("1-2-3-4");
        btnClearTake.setButtonText  ("CLR");

        btnExpSelAll.setButtonText   ("ALL");
        btnExpSelStems.setButtonText ("STEMS");
        btnExportMix.setButtonText   (miamp_i18n::tr (miamp_i18n::Msg::PracticeExportMix));
        btnExportStems.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::PracticeExportStems));
        btnExportTake.setButtonText  (miamp_i18n::tr (miamp_i18n::Msg::PracticeExportMyTake));

        btnEndRepeat.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::PracticeRepeatBtn));
        btnEndNext.setButtonText   (miamp_i18n::tr (miamp_i18n::Msg::PracticeNextBtn));
        btnEndClose.setButtonText  (juce::String::fromUTF8 (u8"\u00d7"));
        pedalModalClose.setButtonText (juce::String::fromUTF8 (u8"\u00d7"));
        btnOpenFullFloor.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::BtnPedals) + " ->");

        syncTransportDynamicStates();
    }

    void syncTransportDynamicStates()
    {
        btnPlayPause.setButtonText (engine.isPlaying() ? "PAUSA" : "PLAY");
        btnPlayPause.setToggleState (engine.isPlaying(), juce::dontSendNotification);
        btnRec.setButtonText (engine.isRecording() ? "REC ON" : "REC");
        btnRec.setToggleState (engine.isRecording(), juce::dontSendNotification);
        btnLoopToggle.setToggleState (engine.isLoopEnabled(), juce::dontSendNotification);
        btnMetToggle.setButtonText (engine.isMetronomeEnabled() ? "MET ON" : "MET OFF");
        btnMetToggle.setToggleState (engine.isMetronomeEnabled(), juce::dontSendNotification);
        btnPreCount.setToggleState (engine.isPreCountEnabled(), juce::dontSendNotification);
        btnClearTake.setVisible (engine.hasRecordedTake());

        if (engine.isAiBusy())
        {
            const int pct = juce::roundToInt (engine.getAiProgress() * 100.0f);
            btnAiAction.setButtonText ("AI " + juce::String (pct) + "%");
            btnAiAction.setEnabled (false);
        }
        else if (! miamp_practice::isDemucsModelReady())
        {
            btnAiAction.setButtonText (miamp_i18n::tr (miamp_i18n::Msg::PracticeDownloadAi));
            btnAiAction.setEnabled (true);
        }
        else
        {
            const auto& pl = engine.getPlaylist();
            const int idx = engine.getActiveSongIndex();
            const bool aiDone = (idx >= 0 && idx < static_cast<int> (pl.size())
                                 && pl[static_cast<size_t> (idx)].stemQuality == miamp_practice::StemQuality::AiReady);
            btnAiAction.setButtonText (aiDone ? "AI HTDEMUCS OK" : miamp_i18n::tr (miamp_i18n::Msg::PracticeSeparateAi));
            btnAiAction.setEnabled (! aiDone && idx >= 0 && ! pl[static_cast<size_t> (idx)].isFreeTake);
        }

        const bool endOpen = engine.isEndChoicePending();
        btnEndRepeat.setVisible (endOpen);
        btnEndNext.setVisible   (endOpen);
        btnEndClose.setVisible  (endOpen);

        pedalModalClose.setVisible  (pedalModalOpen);
        btnOpenFullFloor.setVisible (pedalModalOpen);
        pedalViewport.setVisible    (pedalModalOpen);
    }

    void resized() override
    {
        const float s = static_cast<float> (getWidth()) / 1550.0f;
        auto rInt = [s] (float x, float y, float w, float h) -> juce::Rectangle<int>
        {
            return { juce::roundToInt (x * s), juce::roundToInt (y * s),
                     juce::jmax (1, juce::roundToInt (w * s)), juce::jmax (1, juce::roundToInt (h * s)) };
        };

        // Top Bar (y = 16..56)
        btnBackToAmp.setBounds       (rInt (22,   20, 178, 34));
        btnInstGuitar.setBounds      (rInt (212,  20, 96,  34));
        btnInstBass.setBounds        (rInt (312,  20, 86,  34));

        // Center-Right Top Bar buttons (leaving x=416..670 for persistent live Tuner Strip!)
        btnQuickTuner.setBounds      (rInt (684,  20, 108, 34));
        btnQuickAmpCab.setBounds     (rInt (800,  20, 134, 34));
        btnQuickPedals.setBounds     (rInt (942,  20, 112, 34));
        btnQuickPresets.setBounds    (rInt (1250, 20, 118, 34));
        btnQuickToneFinder.setBounds (rInt (1376, 20, 150, 34));

        // Left Playlist Sidebar (x = 20..324, y = 72..748)
        btnAddSong.setBounds         (rInt (34,  112, 160, 32));
        btnAddFreeTake.setBounds     (rInt (200, 112, 110, 32));
        btnModeContinuous.setBounds  (rInt (34,  152, 134, 28));
        btnModePauseChoose.setBounds (rInt (174, 152, 136, 28));
        playlistViewport.setBounds   (rInt (30,  190, 282, 544));
        layoutPlaylistRows();

        // Transport Strip (x = 348..1520, y = 76..150)
        btnAiAction.setBounds   (rInt (1316, 82, 198, 28));

        btnRewind.setBounds     (rInt (352,  116, 42, 30));
        btnPlayPause.setBounds  (rInt (398,  116, 84, 30));
        btnStop.setBounds       (rInt (486,  116, 56, 30));
        btnRec.setBounds        (rInt (546,  116, 72, 30));

        // Loop A-B
        btnSetA.setBounds       (rInt (752,  116, 58, 30));
        btnSetB.setBounds       (rInt (814,  116, 58, 30));
        btnLoopToggle.setBounds (rInt (876,  116, 84, 30));

        // Speed & Pitch
        btnSpeedMinus.setBounds (rInt (972,  116, 26, 30));
        btnSpeedPlus.setBounds  (rInt (1068, 116, 26, 30));
        btnPitchMinus.setBounds (rInt (1106, 116, 26, 30));
        btnPitchPlus.setBounds  (rInt (1194, 116, 26, 30));

        // Metronome
        btnMetToggle.setBounds  (rInt (1232, 116, 74, 30));
        btnBpmMinus.setBounds   (rInt (1310, 116, 26, 30));
        btnBpmPlus.setBounds    (rInt (1392, 116, 26, 30));
        btnTapTempo.setBounds   (rInt (1422, 116, 44, 30));
        btnPreCount.setBounds   (rInt (1470, 116, 54, 30));

        // 7 Track Lanes (y = 160..682 -> ~74px per lane)
        for (int t = 0; t < miamp_practice::kNumTracks; ++t)
        {
            const float ly = 162.0f + static_cast<float> (t) * 74.0f;
            auto& row = trackCtrls[static_cast<size_t> (t)];
            row.btnMute.setBounds   (rInt (354, ly + 36.0f, 30, 24));
            row.btnSolo.setBounds   (rInt (388, ly + 36.0f, 30, 24));
            row.btnExp.setBounds    (rInt (422, ly + 36.0f, 44, 24));
            row.volSlider.setBounds (rInt (470, ly + 36.0f, 108, 24));
        }
        btnClearTake.setBounds (rInt (528, 162.0f + 5.0f * 74.0f + 8.0f, 46, 22));

        // Bottom Export Bar (y = 692..740)
        btnExpSelAll.setBounds   (rInt (480,  696, 62,  32));
        btnExpSelStems.setBounds (rInt (548,  696, 76,  32));
        btnExportMix.setBounds   (rInt (860,  696, 214, 32));
        btnExportStems.setBounds (rInt (1084, 696, 228, 32));
        btnExportTake.setBounds  (rInt (1322, 696, 192, 32));

        // End-of-song Choice Popup centered over DAW
        btnEndClose.setBounds  (rInt (1120, 286, 28,  28));
        btnEndRepeat.setBounds (rInt (752,  394, 182, 42));
        btnEndNext.setBounds   (rInt (950,  394, 182, 42));

        // Quick Pedals Popup Modal
        pedalModalClose.setBounds  (rInt (1178, 134, 30, 30));
        btnOpenFullFloor.setBounds (rInt (1002, 134, 168, 30));
        pedalViewport.setBounds    (rInt (374,  178, 834, 444));
        layoutPedalPopupRows();
    }

    void paint (juce::Graphics& g) override
    {
        const float s = static_cast<float> (getWidth()) / 1550.0f;
        auto R = [s] (float x, float y, float w, float h) -> juce::Rectangle<float>
        {
            return { x * s, y * s, w * s, h * s };
        };

        // Soft atmospheric overlay over skatepark floor background (never pure black)
        g.setColour (juce::Colour::fromRGBA (26, 36, 38, 92));
        g.fillRect (getLocalBounds());

        // Top Bar Glass Panel
        miamp_theme::paintFrostedGlassPanel (g, R (14, 12, 1522, 50), 14.0f * s);

        // Always-Visible Live Mini Tuner Pill (x = 412..674, y = 18..56)
        const auto tunerBox = R (412, 18, 260, 38);
        miamp_theme::paintGlassSubPanel (g, tunerBox, 10.0f * s);
        paintMiniTuner (g, tunerBox, s);

        // Left Playlist Sidebar Glass Panel
        miamp_theme::paintFrostedGlassPanel (g, R (18, 70, 306, 680), 18.0f * s);
        g.setColour (juce::Colour::fromRGB (247, 238, 219));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (11.0f, 13.5f * s), juce::Font::bold)));
        g.drawText (miamp_i18n::tr (miamp_i18n::Msg::PracticePlaylistTitle), R (34, 80, 274, 24), juce::Justification::centredLeft);

        if (engine.getPlaylist().empty())
        {
            g.setColour (juce::Colour::fromRGBA (242, 232, 212, 175));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.5f, 12.5f * s), juce::Font::plain)));
            g.drawFittedText (miamp_i18n::tr (miamp_i18n::Msg::PracticeEmptyPlaylist),
                               R (42, 280, 258, 90).toNearestInt(), juce::Justification::centred, 3);
        }

        // Main Mini-DAW Glass Panel
        miamp_theme::paintFrostedGlassPanel (g, R (336, 70, 1196, 680), 18.0f * s);

        // Song Title & AI Status Badge
        juce::String songTitle = miamp_i18n::tr (miamp_i18n::Msg::TitlePracticeStudio);
        juce::String qualityBadge = "DSP";
        const auto& pl = engine.getPlaylist();
        const int activeIdx = engine.getActiveSongIndex();
        if (activeIdx >= 0 && activeIdx < static_cast<int> (pl.size()))
        {
            const auto& item = pl[static_cast<size_t> (activeIdx)];
            songTitle = item.title;
            if (item.isFreeTake)                                     qualityBadge = "LIBRE";
            else if (item.stemQuality == miamp_practice::StemQuality::AiReady)     qualityBadge = "AI 6-STEMS";
            else if (item.stemQuality == miamp_practice::StemQuality::AiSeparating) qualityBadge = "AI...";
            else                                                      qualityBadge = "DSP RAPIDO";
            if (item.linkedPresetName.isNotEmpty())
                songTitle += "  [" + item.linkedPresetName + "]";
        }

        g.setColour (juce::Colour::fromRGB (247, 238, 219));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (12.0f, 15.5f * s), juce::Font::bold)));
        g.drawText (songTitle, R (354, 80, 830, 28), juce::Justification::centredLeft, true);

        const auto badgeR = R (1196, 83, 108, 24);
        miamp_theme::paintGlassCard (g, badgeR, 7.0f * s, qualityBadge == "AI 6-STEMS", false);
        g.setColour (juce::Colour::fromRGB (235, 248, 242));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.0f, 10.5f * s), juce::Font::bold)));
        g.drawText (qualityBadge, badgeR, juce::Justification::centred);

        // AI Progress Bar if running
        if (engine.isAiBusy())
        {
            const auto progTrack = R (354, 106, 950, 4);
            g.setColour (juce::Colour::fromRGBA (255, 243, 210, 48));
            g.fillRoundedRectangle (progTrack, 2.0f * s);
            g.setColour (juce::Colour::fromRGB (166, 223, 201));
            g.fillRoundedRectangle (progTrack.withWidth (progTrack.getWidth() * engine.getAiProgress()), 2.0f * s);
        }

        // Timecode + Pre-Count Display
        const double curSec = engine.getPlayheadSeconds();
        const double totSec = engine.getDurationSeconds();
        const auto timeBox = R (626, 116, 118, 30);
        miamp_theme::paintGlassSubPanel (g, timeBox, 8.0f * s);
        g.setColour (engine.isInPreCount() ? juce::Colour::fromRGB (255, 212, 115) : juce::Colour::fromRGB (247, 238, 219));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.5f, 12.5f * s), juce::Font::bold)));
        const juce::String timeTxt = engine.isInPreCount()
            ? ("PRE: " + juce::String (engine.getPreCountBeat()))
            : (formatTimeMmSs (curSec) + " / " + formatTimeMmSs (totSec));
        g.drawText (timeTxt, timeBox, juce::Justification::centred);

        // Speed / Pitch / BPM readouts between -/+ buttons
        auto drawReadout = [&] (float x, const juce::String& lbl, const juce::String& val)
        {
            const auto r = R (x, 116, 68, 30);
            miamp_theme::paintGlassSubPanel (g, r, 7.0f * s);
            g.setColour (juce::Colour::fromRGBA (242, 232, 212, 165));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (7.5f, 8.5f * s), juce::Font::bold)));
            g.drawText (lbl, r.withHeight (11.0f * s), juce::Justification::centred);
            g.setColour (juce::Colour::fromRGB (247, 238, 219));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.5f, 11.2f * s), juce::Font::bold)));
            g.drawText (val, r.withTrimmedTop (10.0f * s), juce::Justification::centred);
        };
        drawReadout (1000, "SPEED", juce::String (engine.getPlaybackSpeed(), 2) + "x");
        const int st = engine.getPitchSemitones();
        drawReadout (1134, "PITCH", (st > 0 ? "+" : "") + juce::String (st) + " st");
        drawReadout (1338, "BPM",   juce::String (juce::roundToInt (engine.getBpm())));

        // 7 Track Lanes
        paintTrackLanes (g, s);

        // Bottom Export Bar Frame
        miamp_theme::paintGlassSubPanel (g, R (348, 690, 1172, 44), 10.0f * s);
        g.setColour (juce::Colour::fromRGB (247, 238, 219));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.5f, 11.5f * s), juce::Font::bold)));
        g.drawText ("EXPORTAR (.WAV):", R (360, 696, 116, 32), juce::Justification::centredLeft);

        // End-of-song Popup Modal ("Pausar y Elegir")
        if (engine.isEndChoicePending())
        {
            g.setColour (juce::Colour::fromRGBA (26, 34, 34, 155));
            g.fillRect (getLocalBounds());

            const auto popR = R (726, 272, 434, 188);
            miamp_theme::paintFrostedGlassPanel (g, popR, 20.0f * s, 1.0f, 1.8f);
            g.setColour (juce::Colour::fromRGB (166, 223, 201));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (12.0f, 15.0f * s), juce::Font::bold)));
            g.drawText (miamp_i18n::tr (miamp_i18n::Msg::PracticeEndTitle), R (748, 290, 360, 26), juce::Justification::centredLeft);

            g.setColour (juce::Colour::fromRGB (247, 238, 219));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (11.5f, 14.0f * s), juce::Font::plain)));
            g.drawText (songTitle, R (748, 332, 390, 36), juce::Justification::centred, true);
        }

        // Quick Pedals Popup Overlay
        if (pedalModalOpen)
        {
            g.setColour (juce::Colour::fromRGBA (26, 34, 34, 145));
            g.fillRect (getLocalBounds());

            const auto popR = R (356, 118, 868, 522);
            miamp_theme::paintFrostedGlassPanel (g, popR, 22.0f * s, 1.0f, 1.8f);
            g.setColour (juce::Colour::fromRGB (247, 238, 219));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (13.0f, 16.0f * s), juce::Font::bold)));
            const juce::String pTitle = processor.isBassMode() ? "PEDALES DE BAJO (ACTIVOS)" : "PEDALES DE GUITARRA (ACTIVOS)";
            g.drawText (pTitle, R (378, 132, 560, 32), juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (pedalModalOpen || engine.isEndChoicePending())
            return;
        handleTimelineScrub (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pedalModalOpen || engine.isEndChoicePending())
            return;
        handleTimelineScrub (e.position);
    }

private:
    struct TrackControls
    {
        juce::TextButton btnMute, btnSolo, btnExp;
        juce::Slider volSlider;
    };

    class PlaylistRowComponent final : public juce::Component
    {
    public:
        PlaylistRowComponent (const miamp_practice::PracticeSongItem& item, bool active,
                              std::function<void()> onSelect,
                              std::function<void(int)> onMove,
                              std::function<void()> onDelete,
                              std::function<void()> onLinkPreset)
            : isActive (active), song (item)
        {
            addAndMakeVisible (btnSelect);
            addAndMakeVisible (btnUp);
            addAndMakeVisible (btnDown);
            addAndMakeVisible (btnDel);
            addAndMakeVisible (btnPreset);

            btnSelect.setButtonText (item.title);
            btnSelect.setComponentID (active ? "presetFactory" : "presetLoad");
            btnSelect.setToggleState (active, juce::dontSendNotification);
            btnSelect.onClick = std::move (onSelect);

            btnUp.setButtonText (juce::String::fromUTF8 (u8"\u2191"));
            btnDown.setButtonText (juce::String::fromUTF8 (u8"\u2193"));
            btnDel.setButtonText (juce::String::fromUTF8 (u8"\u00d7"));
            btnUp.onClick   = [onMove] { if (onMove) onMove (-1); };
            btnDown.onClick = [onMove] { if (onMove) onMove (+1); };
            btnDel.onClick  = std::move (onDelete);

            btnPreset.setButtonText (item.linkedPresetName.isNotEmpty()
                                         ? ("Preset: " + item.linkedPresetName)
                                         : miamp_i18n::tr (miamp_i18n::Msg::PracticeLinkPreset));
            btnPreset.setToggleState (item.linkedPresetName.isNotEmpty(), juce::dontSendNotification);
            btnPreset.onClick = std::move (onLinkPreset);
        }

        void paint (juce::Graphics& g) override
        {
            miamp_theme::paintGlassCard (g, getLocalBounds().toFloat().reduced (1.0f), 9.0f, isActive, false);
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (6, 5);
            auto top = b.removeFromTop (24);
            btnDel.setBounds  (top.removeFromRight (22));
            top.removeFromRight (2);
            btnDown.setBounds (top.removeFromRight (22));
            top.removeFromRight (2);
            btnUp.setBounds   (top.removeFromRight (22));
            top.removeFromRight (4);
            btnSelect.setBounds (top);

            b.removeFromTop (4);
            btnPreset.setBounds (b.removeFromTop (20));
        }

    private:
        bool isActive = false;
        miamp_practice::PracticeSongItem song;
        juce::TextButton btnSelect, btnUp, btnDown, btnDel, btnPreset;
    };

    class QuickPedalCard final : public juce::Component
    {
    public:
        QuickPedalCard (juce::AudioProcessorValueTreeState& state, bool forBass, int pedalType,
                        std::function<void()> onChanged)
            : apvts (state), bass (forBass), type (pedalType), changedCb (std::move (onChanged))
        {
            const auto& spec = mipedalui::getPedalSpec (bass, type);
            addAndMakeVisible (btnBypass);
            btnBypass.setClickingTogglesState (true);
            btnBypass.setButtonText (spec.name);
            const bool isOn = (spec.bypassParamId != nullptr) && (apvts.getRawParameterValue (spec.bypassParamId)->load() >= 0.5f);
            btnBypass.setToggleState (isOn, juce::dontSendNotification);
            btnBypass.onClick = [this, spec]
            {
                if (spec.bypassParamId != nullptr)
                    if (auto* p = apvts.getParameter (spec.bypassParamId))
                        p->setValueNotifyingHost (btnBypass.getToggleState() ? 1.0f : 0.0f);
                if (changedCb) changedCb();
                repaint();
            };

            const int nSliders = juce::jmin (3, spec.numKnobs);
            for (int i = 0; i < nSliders; ++i)
            {
                const auto& k = spec.knobs[static_cast<size_t> (i)];
                auto sl = std::make_unique<juce::Slider> (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                sl->setTextBoxStyle (juce::Slider::TextBoxRight, false, 40, 18);
                if (auto* param = apvts.getParameter (k.paramId))
                {
                    const auto r = param->getNormalisableRange();
                    sl->setRange (r.start, r.end, r.interval > 0.0f ? r.interval : 0.1);
                    sl->setValue (apvts.getRawParameterValue (k.paramId)->load(), juce::dontSendNotification);
                }
                const juce::String pid (k.paramId);
                sl->onValueChange = [this, pid, sPtr = sl.get()]
                {
                    if (auto* param = apvts.getParameter (pid))
                        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (sPtr->getValue())));
                };
                addAndMakeVisible (*sl);
                sliders.push_back (std::move (sl));
                sliderLabels.push_back (k.label);
            }
        }

        void paint (juce::Graphics& g) override
        {
            const bool on = btnBypass.getToggleState();
            miamp_theme::paintGlassCard (g, getLocalBounds().toFloat().reduced (1.0f), 10.0f, on, false);
            g.setColour (juce::Colour::fromRGBA (242, 232, 212, on ? 225 : 140));
            g.setFont (juce::Font (juce::FontOptions (11.0f, juce::Font::bold)));
            for (size_t i = 0; i < sliders.size(); ++i)
            {
                auto r = sliders[i]->getBounds();
                g.drawText (sliderLabels[i], r.getX() - 78, r.getY(), 74, r.getHeight(), juce::Justification::centredRight);
            }
        }

        void resized() override
        {
            auto b = getLocalBounds().reduced (10, 8);
            btnBypass.setBounds (b.removeFromLeft (150).withHeight (34).withCentre ({ 85, getHeight() / 2 }));
            b.removeFromLeft (84);
            const int rowH = sliders.empty() ? 24 : juce::jmax (22, b.getHeight() / static_cast<int> (sliders.size()));
            for (auto& sl : sliders)
                sl->setBounds (b.removeFromTop (rowH).reduced (0, 2));
        }

    private:
        juce::AudioProcessorValueTreeState& apvts;
        bool bass;
        int type;
        std::function<void()> changedCb;
        juce::TextButton btnBypass;
        std::vector<std::unique_ptr<juce::Slider>> sliders;
        std::vector<juce::String> sliderLabels;
    };

    void handleTimelineScrub (juce::Point<float> pos)
    {
        const float s = static_cast<float> (getWidth()) / 1550.0f;
        const float waveX = 590.0f * s;
        const float waveW = 924.0f * s;
        const float waveTop = 162.0f * s;
        const float waveBot = 162.0f * s + 7.0f * 74.0f * s;
        if (pos.x >= waveX && pos.x <= waveX + waveW && pos.y >= waveTop && pos.y <= waveBot)
        {
            const float frac = juce::jlimit (0.0f, 1.0f, (pos.x - waveX) / waveW);
            engine.seekToSeconds (static_cast<double> (frac) * engine.getDurationSeconds());
            repaint();
        }
    }

    void paintMiniTuner (juce::Graphics& g, juce::Rectangle<float> box, float s)
    {
        const bool active = tunerReading.hasSignal && tunerReading.freqHz > 20.0f && tunerReading.noteName != nullptr;
        const juce::String noteTxt = active ? juce::String (tunerReading.noteName) + juce::String (tunerReading.octave) : "--";
        const float cents = active ? juce::jlimit (-50.0f, 50.0f, tunerReading.cents) : 0.0f;
        const bool inTune = active && std::fabs (cents) < 6.0f;

        g.setColour (juce::Colour::fromRGBA (242, 232, 212, 165));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (8.0f, 9.0f * s), juce::Font::bold)));
        g.drawText ("AFINADOR", box.withTrimmedLeft (8.0f * s).withWidth (62.0f * s), juce::Justification::centredLeft);

        g.setColour (inTune ? juce::Colour::fromRGB (136, 236, 192) : juce::Colour::fromRGB (247, 238, 219));
        g.setFont (juce::Font (juce::FontOptions (juce::jmax (12.0f, 15.5f * s), juce::Font::bold)));
        g.drawText (noteTxt, box.withX (box.getX() + 68.0f * s).withWidth (46.0f * s), juce::Justification::centred);

        const auto bar = juce::Rectangle<float> (box.getX() + 120.0f * s, box.getCentreY() - 3.5f * s, 126.0f * s, 7.0f * s);
        g.setColour (juce::Colour::fromRGBA (36, 46, 44, 170));
        g.fillRoundedRectangle (bar, 3.5f * s);
        g.setColour (juce::Colour::fromRGBA (255, 243, 210, 95));
        g.drawVerticalLine (juce::roundToInt (bar.getCentreX()), bar.getY() - 2.0f * s, bar.getBottom() + 2.0f * s);
        if (active)
        {
            const float nx = bar.getCentreX() + (cents / 50.0f) * (bar.getWidth() * 0.46f);
            g.setColour (inTune ? juce::Colour::fromRGB (136, 236, 192) : juce::Colour::fromRGB (255, 192, 108));
            g.fillEllipse (nx - 4.5f * s, bar.getCentreY() - 4.5f * s, 9.0f * s, 9.0f * s);
        }
    }

    void paintTrackLanes (juce::Graphics& g, float s)
    {
        const auto* stems = engine.getLoadedStemsSnapshot();
        const auto takePeaks = engine.getUserTakePeaksSnapshot();
        const double dur = juce::jmax (0.1, engine.getDurationSeconds());
        const double cur = engine.getPlayheadSeconds();
        const float playFrac = juce::jlimit (0.0f, 1.0f, static_cast<float> (cur / dur));
        const bool loopOn = engine.isLoopEnabled();
        const float loopAFrac = juce::jlimit (0.0f, 1.0f, static_cast<float> (engine.getLoopStartSeconds() / dur));
        const float loopBFrac = juce::jlimit (loopAFrac, 1.0f, static_cast<float> (engine.getLoopEndSeconds() / dur));
        const bool bass = processor.isBassMode();

        const miamp_i18n::Msg kTrackLabels[miamp_practice::kNumTracks] = {
            miamp_i18n::Msg::PracticeTrackVocals,
            miamp_i18n::Msg::PracticeTrackGuitar,
            miamp_i18n::Msg::PracticeTrackBass,
            miamp_i18n::Msg::PracticeTrackDrums,
            miamp_i18n::Msg::PracticeTrackOther,
            bass ? miamp_i18n::Msg::PracticeTrackMyBass : miamp_i18n::Msg::PracticeTrackMyGuitar,
            miamp_i18n::Msg::PracticeTrackMetronome
        };

        const float waveX = 590.0f * s;
        const float waveW = 924.0f * s;

        for (int t = 0; t < miamp_practice::kNumTracks; ++t)
        {
            const float ly = (162.0f + static_cast<float> (t) * 74.0f) * s;
            const float lh = 68.0f * s;
            const auto laneR = juce::Rectangle<float> (348.0f * s, ly, 1172.0f * s, lh);
            const auto headR = juce::Rectangle<float> (348.0f * s, ly, 234.0f * s, lh);
            const auto waveR = juce::Rectangle<float> (waveX, ly, waveW, lh);
            const auto acc = trackAccentColour (t);
            const bool muted = engine.isTrackMuted (t);

            miamp_theme::paintGlassCard (g, laneR, 10.0f * s, false, false);
            g.setColour (acc.withAlpha (muted ? 0.32f : 0.92f));
            g.fillRoundedRectangle (headR.withWidth (5.5f * s), 3.0f * s);

            g.setColour (juce::Colour::fromRGB (247, 238, 219).withAlpha (muted ? 0.45f : 0.96f));
            g.setFont (juce::Font (juce::FontOptions (juce::jmax (10.0f, 12.0f * s), juce::Font::bold)));
            g.drawText (miamp_i18n::tr (kTrackLabels[t]),
                        juce::Rectangle<float> (360.0f * s, ly + 6.0f * s, 180.0f * s, 22.0f * s),
                        juce::Justification::centredLeft);

            // Waveform area subtle inner sub-panel
            miamp_theme::paintGlassSubPanel (g, waveR.reduced (2.0f * s, 3.0f * s), 7.0f * s, 0.85f);

            // Loop A-B shaded range
            if (loopOn && loopBFrac > loopAFrac)
            {
                const float ax = waveX + loopAFrac * waveW;
                const float bx = waveX + loopBFrac * waveW;
                g.setColour (juce::Colour::fromRGBA (166, 223, 201, 34));
                g.fillRect (ax, ly + 3.0f * s, bx - ax, lh - 6.0f * s);
            }

            if (t < miamp_practice::kNumStems)
            {
                if (stems != nullptr && ! stems->isFreeTake)
                    drawPeaks (g, waveR.reduced (6.0f * s, 8.0f * s), stems->peaks[static_cast<size_t> (t)], acc.withAlpha (muted ? 0.25f : 0.85f));
            }
            else if (t == miamp_practice::kTrackTake)
            {
                if (engine.hasRecordedTake())
                    drawPeaks (g, waveR.reduced (6.0f * s, 8.0f * s), takePeaks, acc.withAlpha (muted ? 0.25f : 0.92f));
                else
                {
                    g.setColour (juce::Colour::fromRGBA (242, 232, 212, 115));
                    g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.5f, 11.0f * s), juce::Font::plain)));
                    g.drawText (engine.isRecording() ? "GRABANDO TOMA EN VIVO..." : "Presiona REC para grabar tu instrumento con el tono de MI AMP",
                                waveR, juce::Justification::centred);
                }
            }
            else if (t == miamp_practice::kTrackMet)
            {
                // Beat Grid Ticks + Live Beat Indicator (1 2 3 4)
                const float curBpm = engine.getBpm();
                const double beatSec = 60.0 / juce::jmax (40.0f, curBpm);
                const int totalBeats = juce::jlimit (4, 600, static_cast<int> (dur / beatSec));
                const int step = juce::jmax (1, totalBeats / 160);
                for (int b = 0; b < totalBeats; b += step)
                {
                    const float bx = waveX + static_cast<float> ((b * beatSec) / dur) * waveW;
                    const bool downbeat = (b % 4) == 0;
                    const float bh = downbeat ? lh * 0.56f : lh * 0.30f;
                    g.setColour (acc.withAlpha ((downbeat ? 0.78f : 0.36f) * (muted ? 0.3f : 1.0f)));
                    g.drawVerticalLine (juce::roundToInt (bx), waveR.getCentreY() - bh * 0.5f, waveR.getCentreY() + bh * 0.5f);
                }

                // 4-Beat LED dots inside header right
                const int curBeat = engine.getCurrentMetronomeBeat();
                for (int d = 1; d <= 4; ++d)
                {
                    const bool lit = engine.isPlaying() && engine.isMetronomeEnabled() && (curBeat == d);
                    const float dx = (500.0f + (d - 1) * 18.0f) * s;
                    g.setColour (lit ? acc : acc.withAlpha (0.25f));
                    g.fillEllipse (dx, ly + 11.0f * s, 10.0f * s, 10.0f * s);
                }
            }
        }

        // Loop A / B vertical flags & Playhead across all 7 lanes
        const float topY = 162.0f * s;
        const float botY = (162.0f + 7.0f * 74.0f - 6.0f) * s;
        if (loopOn)
        {
            const float ax = waveX + loopAFrac * waveW;
            const float bx = waveX + loopBFrac * waveW;
            g.setColour (juce::Colour::fromRGB (166, 223, 201));
            g.drawVerticalLine (juce::roundToInt (ax), topY, botY);
            g.drawVerticalLine (juce::roundToInt (bx), topY, botY);
        }

        const float px = waveX + playFrac * waveW;
        g.setColour (juce::Colour::fromRGB (247, 238, 219));
        g.drawLine (px, topY, px, botY, juce::jmax (1.5f, 2.0f * s));
    }

    static void drawPeaks (juce::Graphics& g, juce::Rectangle<float> area, const std::vector<float>& peaks, juce::Colour col)
    {
        if (peaks.empty()) return;
        g.setColour (col);
        const int n = static_cast<int> (peaks.size());
        const float barW = area.getWidth() / static_cast<float> (n);
        const float cy = area.getCentreY();
        const float maxH = area.getHeight() * 0.48f;
        for (int i = 0; i < n; ++i)
        {
            const float v = juce::jmax (0.04f, peaks[static_cast<size_t> (i)]);
            const float h = v * maxH;
            g.fillRect (area.getX() + static_cast<float> (i) * barW, cy - h, juce::jmax (1.2f, barW * 0.72f), h * 2.0f);
        }
    }

    void rebuildPlaylistRows()
    {
        playlistRows.clear();
        const auto& pl = engine.getPlaylist();
        const int activeIdx = engine.getActiveSongIndex();
        for (size_t i = 0; i < pl.size(); ++i)
        {
            const int idx = static_cast<int> (i);
            juce::Component::SafePointer<PracticeStudioView> safeView (this);
            auto r = std::make_unique<PlaylistRowComponent> (
                pl[i], idx == activeIdx,
                [safeView, idx]
                {
                    juce::MessageManager::callAsync ([safeView, idx]
                    {
                        if (safeView == nullptr) return;
                        safeView->engine.loadSongAtIndex (idx, false);
                        safeView->autoLoadLinkedPresetIfAny();
                        safeView->rebuildPlaylistRows();
                        safeView->syncTextsAndButtons();
                        safeView->repaint();
                    });
                },
                [safeView, idx] (int d)
                {
                    juce::MessageManager::callAsync ([safeView, idx, d]
                    {
                        if (safeView == nullptr) return;
                        safeView->engine.moveSongInPlaylist (idx, d);
                        safeView->rebuildPlaylistRows();
                    });
                },
                [safeView, idx]
                {
                    juce::MessageManager::callAsync ([safeView, idx]
                    {
                        if (safeView == nullptr) return;
                        safeView->engine.removeSongAtIndex (idx);
                        safeView->rebuildPlaylistRows();
                        safeView->syncTextsAndButtons();
                        safeView->repaint();
                    });
                },
                [safeView, idx]
                {
                    juce::MessageManager::callAsync ([safeView, idx]
                    {
                        if (safeView == nullptr) return;
                        if (idx != safeView->engine.getActiveSongIndex())
                            safeView->engine.loadSongAtIndex (idx, false);
                        const auto& list = safeView->engine.getPlaylist();
                        if (idx >= 0 && idx < static_cast<int> (list.size()))
                        {
                            if (list[static_cast<size_t> (idx)].linkedPresetName.isNotEmpty())
                                safeView->engine.clearLinkedPresetOnActiveSong();
                            else
                                safeView->engine.linkPresetToActiveSong (safeView->processor.getActivePresetName(), safeView->processor.isBassMode());
                        }
                        safeView->rebuildPlaylistRows();
                    });
                });
            playlistHolder.addAndMakeVisible (*r);
            playlistRows.push_back (std::move (r));
        }
        layoutPlaylistRows();
    }

    void layoutPlaylistRows()
    {
        const int vw = juce::jmax (120, playlistViewport.getWidth() - 8);
        int y = 0;
        for (auto& r : playlistRows)
        {
            r->setBounds (0, y, vw, 56);
            y += 62;
        }
        playlistHolder.setSize (vw, juce::jmax (10, y));
    }

    void rebuildPedalPopup()
    {
        pedalCards.clear();
        if (! pedalModalOpen) return;
        const bool bass = processor.isBassMode();
        const auto chain = processor.getActiveChain (bass);
        for (int pt : chain)
        {
            auto c = std::make_unique<QuickPedalCard> (processor.apvts, bass, pt, [this] { repaint(); });
            pedalHolder.addAndMakeVisible (*c);
            pedalCards.push_back (std::move (c));
        }
        layoutPedalPopupRows();
    }

    void layoutPedalPopupRows()
    {
        const int vw = juce::jmax (200, pedalViewport.getWidth() - 10);
        int y = 0;
        for (auto& c : pedalCards)
        {
            c->setBounds (0, y, vw, 86);
            y += 94;
        }
        pedalHolder.setSize (vw, juce::jmax (10, y));
    }

    void autoLoadLinkedPresetIfAny()
    {
        const auto& pl = engine.getPlaylist();
        const int idx = engine.getActiveSongIndex();
        if (idx < 0 || idx >= static_cast<int> (pl.size())) return;
        const auto& item = pl[static_cast<size_t> (idx)];
        if (item.autoLoadPreset && item.linkedPresetName.isNotEmpty())
        {
            if (processor.loadPresetByName (item.linkedPresetName, item.linkedPresetIsBass))
                if (callbacks.onHud) callbacks.onHud ("Preset: " + item.linkedPresetName);
        }
    }

    void openAddSongChooser()
    {
        juce::Component::SafePointer<PracticeStudioView> safeThis (this);
        auto initDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
        if (! initDir.isDirectory())
            initDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory);
        activeChooser = std::make_unique<juce::FileChooser> (
            "Elegir canciones (MP3, WAV, FLAC, AIFF, OGG)",
            initDir,
            "*.mp3;*.wav;*.flac;*.aiff;*.aif;*.ogg;*.m4a");
        activeChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
            [safeThis] (const juce::FileChooser& fc)
            {
                if (safeThis == nullptr) return;
                const auto res = fc.getResults();
                if (! res.isEmpty())
                {
                    safeThis->engine.addAudioFilesToPlaylist (res);
                    safeThis->rebuildPlaylistRows();
                    safeThis->syncTextsAndButtons();
                    safeThis->repaint();
                }
            });
    }

    void exportMixFlow()
    {
        juce::Component::SafePointer<PracticeStudioView> safeThis (this);
        activeChooser = std::make_unique<juce::FileChooser> (
            "Guardar Mezcla (.WAV)",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("MiAmp_Ensayo_Mix.wav"),
            "*.wav");
        activeChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [safeThis] (const juce::FileChooser& fc)
            {
                if (safeThis == nullptr) return;
                auto f = fc.getResult();
                if (f != juce::File{})
                {
                    if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
                    const bool ok = safeThis->engine.exportMixWav (f);
                    if (safeThis->callbacks.onHud) safeThis->callbacks.onHud (ok ? ("Exportado: " + f.getFileName()) : "Nada para exportar");
                }
            });
    }

    void exportStemsFlow()
    {
        juce::Component::SafePointer<PracticeStudioView> safeThis (this);
        activeChooser = std::make_unique<juce::FileChooser> (
            "Elegir carpeta para guardar los Stems (.WAV)",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory));
        activeChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
            [safeThis] (const juce::FileChooser& fc)
            {
                if (safeThis == nullptr) return;
                const auto dir = fc.getResult();
                if (dir != juce::File{})
                {
                    const int count = safeThis->engine.exportCheckedStemsToFolder (dir);
                    if (safeThis->callbacks.onHud) safeThis->callbacks.onHud (count > 0 ? (juce::String (count) + " stems .WAV exportados") : "Nada marcado para exportar");
                }
            });
    }

    void exportTakeFlow()
    {
        if (! engine.hasRecordedTake())
        {
            if (callbacks.onHud) callbacks.onHud ("Primero grabá una toma con REC");
            return;
        }
        juce::Component::SafePointer<PracticeStudioView> safeThis (this);
        activeChooser = std::make_unique<juce::FileChooser> (
            "Guardar Mi Toma (.WAV)",
            juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("MiAmp_Mi_Toma.wav"),
            "*.wav");
        activeChooser->launchAsync (
            juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
            [safeThis] (const juce::FileChooser& fc)
            {
                if (safeThis == nullptr) return;
                auto f = fc.getResult();
                if (f != juce::File{})
                {
                    if (! f.hasFileExtension ("wav")) f = f.withFileExtension ("wav");
                    const bool ok = safeThis->engine.exportUserTakeWav (f);
                    if (safeThis->callbacks.onHud) safeThis->callbacks.onHud (ok ? ("Toma exportada: " + f.getFileName()) : "Error exportando toma");
                }
            });
    }

    MiAmpAudioProcessor& processor;
    miamp_practice::MiAmpPracticeEngine& engine;
    Callbacks callbacks;

    xvdsp::TunerReading tunerReading;
    bool useSolfege = false;
    bool pedalModalOpen = false;

    juce::TextButton btnBackToAmp, btnInstGuitar, btnInstBass;
    juce::TextButton btnQuickTuner, btnQuickAmpCab, btnQuickPedals, btnQuickPresets, btnQuickToneFinder;

    juce::TextButton btnAddSong, btnAddFreeTake, btnModeContinuous, btnModePauseChoose;
    juce::Viewport playlistViewport;
    juce::Component playlistHolder;
    std::vector<std::unique_ptr<PlaylistRowComponent>> playlistRows;

    juce::TextButton btnAiAction;
    juce::TextButton btnRewind, btnPlayPause, btnStop, btnRec, btnClearTake;
    juce::TextButton btnSetA, btnSetB, btnLoopToggle;
    juce::TextButton btnSpeedMinus, btnSpeedPlus, btnPitchMinus, btnPitchPlus;
    juce::TextButton btnMetToggle, btnBpmMinus, btnBpmPlus, btnTapTempo, btnPreCount;

    std::array<TrackControls, miamp_practice::kNumTracks> trackCtrls;

    juce::TextButton btnExpSelAll, btnExpSelStems, btnExportMix, btnExportStems, btnExportTake;
    juce::TextButton btnEndRepeat, btnEndNext, btnEndClose;

    juce::TextButton pedalModalClose, btnOpenFullFloor;
    juce::Viewport pedalViewport;
    juce::Component pedalHolder;
    std::vector<std::unique_ptr<QuickPedalCard>> pedalCards;

    std::unique_ptr<juce::FileChooser> activeChooser;
};

} // namespace miamp_practice_ui
