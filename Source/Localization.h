#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace miamp_i18n
{
enum class Lang : int
{
    ES = 0,
    EN = 1,
    PT = 2,
    FR = 3,
    DE = 4,
    Count = 5
};

enum class Msg : int
{
    // Main Head Menu & Navigation
    BtnGuitar = 0,
    BtnBass,
    BtnPresets,
    BtnTuner,
    BtnToneFinder,
    BtnPedals,
    BtnConfig,
    BtnSkip,
    BtnClose,
    BtnBackToAmp,
    BtnWithAmp,
    BtnPedalsOnly,

    // HUD & Instrument Notices
    HudGuitar,
    HudBass,
    HudWithAmp,
    HudPedalsOnly,
    HudRemoved,
    HudMoved,

    // Pedalboard Floor & Editor
    BoardEmptyHint,
    ReleaseToRemove,
    EditPrefix,
    ChainPrefix,
    DragKnobHint,
    StateActive,
    StateBypass,
    PaletteTitle,
    PaletteSubtitle,
    TunerLabel,
    WaitingSignal,

    // Presets Popover
    TitlePresetsBass,
    TitlePresetsGuitar,
    TitleExportPreset,
    TitleDropTuner,
    TitleConfig,
    TitleToneFinderBass,
    TitleToneFinderGuitar,
    TitleTunerDrop,
    PresetNamePlaceholder,
    BtnSave,
    BtnImportMiAmp,
    SavePresetHeader,
    ImportChooserTitle,
    TooltipExportPreset,
    TooltipDeletePreset,
    BtnBackArrow,
    ExportChooseFormat,
    TagBassParens,
    TagGuitarParens,
    ExportDialogPrefix,
    ExportedOkPrefix,
    ExportSaveError,
    ExportProprietaryNote,

    // Export Format Titles & Subtitles (11 formats)
    ExpNamA2Title,      ExpNamA2Sub,
    ExpNamA1Title,      ExpNamA1Sub,
    ExpNamLstmTitle,    ExpNamLstmSub,
    ExpAidaXTitle,      ExpAidaXSub,
    ExpProteusTitle,    ExpProteusSub,
    ExpNeuralPiTitle,   ExpNeuralPiSub,
    ExpIrWavTitle,      ExpIrWavSub,
    ExpBlackboxTitle,   ExpBlackboxSub,
    ExpHelixTitle,      ExpHelixSub,
    ExpHeadrushTitle,   ExpHeadrushSub,
    ExpMiAmpTitle,      ExpMiAmpSub,

    // Drop & Tuner
    DropActive,
    DropEnable,
    LabelSemitones,
    LabelCents,
    LabelMixPct,
    LabelDropTune,
    TunerMuteOn,
    TunerMuteOff,
    TunerMuteShort,
    TunerNotesSolfege,
    TunerNotesLetters,

    // Config
    CfgRouteMode,
    CfgBassCab,
    CfgGuitarCab,
    CfgCabOff,
    CfgCab8x10On,
    CfgCab8x10Off,
    CfgHeadChannel,
    CfgNoiseGate,
    CfgInputTrim,
    CfgOs4xOn,
    CfgOs4xOff,
    CfgLanguage,
    CfgGeminiKey,
    CfgGeminiPlaceholder,
    CfgSavedOk,
    CfgCleared,
    CfgAudioHeader,
    CfgAudioBtn,
    CfgAudioMuted,
    CfgAudioNoDevice,
    CfgAudioNoInput,
    CfgUpdatesHeader,
    CfgCheckUpdatesBtn,
    CfgCheckingUpdates,
    CfgUpToDate,
    CfgUpdateCheckErr,
    CfgVersionPrefix,

    // Tone Finder
    TfPromptLabel,
    TfPlaceholder,
    TfSearchBtn,
    TfSuggestions,
    TfSearching,
    TfApplyBtn,
    TfSavePresetBtn,
    TfSavedBtn,
    TfSavedNotice,
    TfAskSaveNotice,
    TfNotFound,
    TfCopyPromptBtn,
    TfPromptCopiedBtn,
    TfEmptyIntro,
    TfPedalsWord,
    TfLocalBadge,
    TfGeminiBadge,
    TfGeminiFallbackDesc,

    // Credit
    DevelopedBy,

    // Auto-Updater Modal
    UpdModalTitle,
    UpdAvailableTitle,
    UpdDownloading,
    UpdInstalling,
    UpdRestarting,
    UpdDawWarning,
    UpdBtnNow,
    UpdBtnLater,
    UpdBtnCancel,
    UpdFailed,

    Count
};

inline Lang detectSystemLang() noexcept
{
    const auto code = juce::SystemStats::getUserLanguage().trim().toLowerCase();
    if (code.startsWith ("es")) return Lang::ES;
    if (code.startsWith ("pt")) return Lang::PT;
    if (code.startsWith ("fr")) return Lang::FR;
    if (code.startsWith ("de")) return Lang::DE;
    return Lang::EN;
}

inline std::unique_ptr<juce::PropertiesFile> openSettingsFile()
{
    juce::PropertiesFile::Options opts;
    opts.applicationName = "MI AMP";
    opts.filenameSuffix = ".settings";
    opts.folderName = "Popups Audio";
    opts.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (opts);
}

inline std::atomic<int>& cachedLangAtomic() noexcept
{
    static std::atomic<int> v { -1 };
    return v;
}

inline Lang currentLang() noexcept
{
    auto& atom = cachedLangAtomic();
    int cur = atom.load (std::memory_order_relaxed);
    if (cur < 0 || cur >= static_cast<int> (Lang::Count))
    {
        Lang detected = detectSystemLang();
        if (auto pf = openSettingsFile())
        {
            const juce::String saved = pf->getValue ("uiLang", "").trim().toLowerCase();
            if      (saved == "es") detected = Lang::ES;
            else if (saved == "en") detected = Lang::EN;
            else if (saved == "pt") detected = Lang::PT;
            else if (saved == "fr") detected = Lang::FR;
            else if (saved == "de") detected = Lang::DE;
        }
        cur = static_cast<int> (detected);
        atom.store (cur, std::memory_order_relaxed);
    }
    return static_cast<Lang> (cur);
}

inline void setLang (Lang l) noexcept
{
    const int idx = juce::jlimit (0, static_cast<int> (Lang::Count) - 1, static_cast<int> (l));
    cachedLangAtomic().store (idx, std::memory_order_relaxed);
    static constexpr const char* kCodes[5] = { "es", "en", "pt", "fr", "de" };
    if (auto pf = openSettingsFile())
    {
        pf->setValue ("uiLang", juce::String (kCodes[idx]));
        pf->saveIfNeeded();
    }
}

inline const char* langShortCode (Lang l) noexcept
{
    static constexpr const char* kCodes[5] = { "ES", "EN", "PT", "FR", "DE" };
    return kCodes[juce::jlimit (0, 4, static_cast<int> (l))];
}

inline juce::String langAiPromptName (Lang l)
{
    switch (l)
    {
        case Lang::ES: return "Spanish";
        case Lang::PT: return "Portuguese";
        case Lang::FR: return "French";
        case Lang::DE: return "German";
        case Lang::EN:
        default:       return "English";
    }
}

inline juce::String tr (Msg id)
{
    static constexpr std::array<std::array<const char*, 5>, static_cast<size_t> (Msg::Count)> kTable = {{
        // BtnGuitar
        { u8"GUITARRA", u8"GUITAR", u8"GUITARRA", u8"GUITARE", u8"GITARRE" },
        // BtnBass
        { u8"BAJO", u8"BASS", u8"BAIXO", u8"BASSE", u8"BASS" },
        // BtnPresets
        { u8"PRESETS", u8"PRESETS", u8"PRESETS", u8"PRESETS", u8"PRESETS" },
        // BtnTuner
        { u8"AFINADOR", u8"TUNER", u8"AFINADOR", u8"ACCORDEUR", u8"STIMMER" },
        // BtnToneFinder
        { u8"BUSCAR TONO", u8"TONE FINDER", u8"BUSCAR TIMBRE", u8"CHERCHER SON", u8"TONE FINDER" },
        // BtnPedals
        { u8"PEDALES", u8"PEDALS", u8"PEDAIS", u8"P\u00c9DALES", u8"PEDALE" },
        // BtnConfig
        { u8"CONFIG", u8"CONFIG", u8"CONFIG", u8"CONFIG", u8"CONFIG" },
        // BtnSkip
        { u8"SALTAR", u8"SKIP", u8"PULAR", u8"PASSER", u8"WEITER" },
        // BtnClose
        { u8"Cerrar", u8"Close", u8"Fechar", u8"Fermer", u8"Schlie\u00dfen" },
        // BtnBackToAmp
        { u8"VOLVER AL AMPLI", u8"BACK TO AMP", u8"VOLTAR AO AMP", u8"RETOUR AMPLI", u8"ZUM AMP" },
        // BtnWithAmp
        { u8"CON AMPLI", u8"WITH AMP", u8"COM AMP", u8"AVEC AMPLI", u8"MIT AMP" },
        // BtnPedalsOnly
        { u8"SOLO PEDALES", u8"PEDALS ONLY", u8"S\u00d3 PEDAIS", u8"P\u00c9DALES SEULES", u8"NUR PEDALE" },

        // HudGuitar
        { u8"GUITARRA \u00b7 MI AMP", u8"GUITAR \u00b7 MI AMP", u8"GUITARRA \u00b7 MI AMP", u8"GUITARE \u00b7 MI AMP", u8"GITARRE \u00b7 MI AMP" },
        // HudBass
        { u8"BAJO \u00b7 XV EDITION", u8"BASS \u00b7 XV EDITION", u8"BAIXO \u00b7 XV EDITION", u8"BASSE \u00b7 XV EDITION", u8"BASS \u00b7 XV EDITION" },
        // HudWithAmp
        { u8"CON AMPLI (CABEZAL ACTIVO)", u8"WITH AMP (HEAD ACTIVE)", u8"COM AMP (CABE\u00c7OTE ATIVO)", u8"AVEC AMPLI (T\u00caTE ACTIVE)", u8"MIT AMP (TOPTEIL AKTIV)" },
        // HudPedalsOnly
        { u8"SOLO PEDALES (CABEZAL EN BYPASS)", u8"PEDALS ONLY (HEAD BYPASS)", u8"S\u00d3 PEDAIS (CABE\u00c7OTE BYPASS)", u8"P\u00c9DALES SEULES (T\u00caTE BYPASS)", u8"NUR PEDALE (AMP BYPASS)" },
        // HudRemoved
        { u8" QUITADO", u8" REMOVED", u8" REMOVIDO", u8" RETIR\u00c9", u8" ENTFERNT" },
        // HudMoved
        { u8" MOVIDO", u8" MOVED", u8" MOVIDO", u8" D\u00c9PLAC\u00c9", u8" VERSCHOBEN" },

        // BoardEmptyHint
        { u8"TU PEDALBOARD EST\u00c1 VAC\u00cdA \u2014 Arrastr\u00e1 un pedal desde abajo",
          u8"YOUR PEDALBOARD IS EMPTY \u2014 Drag a pedal from below",
          u8"SUA PEDALBOARD EST\u00c1 VAZIA \u2014 Arraste um pedal abaixo",
          u8"VOTRE PEDALBOARD EST VIDE \u2014 Glissez une p\u00e9dale ci-dessous",
          u8"DEIN PEDALBOARD IST LEER \u2014 Ziehe ein Pedal von unten" },
        // ReleaseToRemove
        { u8"SOLT\u00c1 PARA QUITAR", u8"DROP TO REMOVE", u8"SOLTE PARA REMOVER", u8"REL\u00c2CHER POUR RETIRER", u8"LOSLASSEN ZUM ENTFERNEN" },
        // EditPrefix
        { u8"EDITAR ", u8"EDIT ", u8"EDITAR ", u8"\u00c9DITER ", u8"BEARBEITEN " },
        // ChainPrefix
        { u8"CADENA ", u8"CHAIN ", u8"CADEIA ", u8"CHA\u00ceNE ", u8"KETTE " },
        // DragKnobHint
        { u8"Arrastr\u00e1 una perilla \u00b7 ", u8"Drag a knob \u00b7 ", u8"Arraste um knob \u00b7 ", u8"Glissez un bouton \u00b7 ", u8"Regler ziehen \u00b7 " },
        // StateActive
        { u8"ACTIVO", u8"ACTIVE", u8"ATIVO", u8"ACTIF", u8"AKTIV" },
        // StateBypass
        { u8"BYPASS", u8"BYPASS", u8"BYPASS", u8"BYPASS", u8"BYPASS" },
        // PaletteTitle
        { u8"ARRASTR\u00c1 LOS PEDALES", u8"DRAG THE PEDALS", u8"ARRASTE OS PEDAIS", u8"GLISSEZ LES P\u00c9DALES", u8"PEDALE ZIEHEN" },
        // PaletteSubtitle
        { u8"o hac\u00e9 clic para agregar", u8"or click to add", u8"ou clique para adicionar", u8"ou cliquez pour ajouter", u8"oder klicken zum Hinzuf\u00fcgen" },
        // TunerLabel
        { u8"AFINADOR", u8"TUNER", u8"AFINADOR", u8"ACCORDEUR", u8"STIMMER" },
        // WaitingSignal
        { u8"Esperando se\u00f1al", u8"Waiting for signal", u8"Aguardando sinal", u8"En attente de signal", u8"Warte auf Signal" },

        // TitlePresetsBass
        { u8"PRESETS \u00b7 BAJO", u8"PRESETS \u00b7 BASS", u8"PRESETS \u00b7 BAIXO", u8"PRESETS \u00b7 BASSE", u8"PRESETS \u00b7 BASS" },
        // TitlePresetsGuitar
        { u8"PRESETS \u00b7 GUITARRA", u8"PRESETS \u00b7 GUITAR", u8"PRESETS \u00b7 GUITARRA", u8"PRESETS \u00b7 GUITARE", u8"PRESETS \u00b7 GITARRE" },
        // TitleExportPreset
        { u8"EXPORTAR PRESET", u8"EXPORT PRESET", u8"EXPORTAR PRESET", u8"EXPORTER PRESET", u8"PRESET EXPORTIEREN" },
        // TitleDropTuner
        { u8"DROP TUNER", u8"DROP TUNER", u8"DROP TUNER", u8"DROP TUNER", u8"DROP TUNER" },
        // TitleConfig
        { u8"CONFIGURACI\u00d3N", u8"SETTINGS", u8"CONFIGURA\u00c7\u00d5ES", u8"PARAM\u00c8TRES", u8"EINSTELLUNGEN" },
        // TitleToneFinderBass
        { u8"BUSCAR TONO \u00b7 BAJO", u8"TONE FINDER \u00b7 BASS", u8"BUSCAR TIMBRE \u00b7 BAIXO", u8"CHERCHER SON \u00b7 BASSE", u8"TONE FINDER \u00b7 BASS" },
        // TitleToneFinderGuitar
        { u8"BUSCAR TONO \u00b7 GUITARRA", u8"TONE FINDER \u00b7 GUITAR", u8"BUSCAR TIMBRE \u00b7 GUITARRA", u8"CHERCHER SON \u00b7 GUITARE", u8"TONE FINDER \u00b7 GITARRE" },
        // TitleTunerDrop
        { u8"AFINADOR + DROP", u8"TUNER + DROP", u8"AFINADOR + DROP", u8"ACCORDEUR + DROP", u8"STIMMER + DROP" },
        // PresetNamePlaceholder
        { u8"Nombre del preset", u8"Preset name", u8"Nome do preset", u8"Nom du preset", u8"Preset-Name" },
        // BtnSave
        { u8"GUARDAR", u8"SAVE", u8"SALVAR", u8"SAUVER", u8"SPEICHERN" },
        // BtnImportMiAmp
        { u8"IMPORTAR .MIAMP", u8"IMPORT .MIAMP", u8"IMPORTAR .MIAMP", u8"IMPORTER .MIAMP", u8".MIAMP IMPORTIEREN" },
        // SavePresetHeader
        { u8"GUARDAR PRESET", u8"SAVE PRESET", u8"SALVAR PRESET", u8"ENREGISTRER PRESET", u8"PRESET SPEICHERN" },
        // ImportChooserTitle
        { u8"Importar preset de MI AMP (.miamp)", u8"Import MI AMP preset (.miamp)", u8"Importar preset do MI AMP (.miamp)", u8"Importer preset MI AMP (.miamp)", u8"MI AMP Preset importieren (.miamp)" },
        // TooltipExportPreset
        { u8"Descargar preset para pedalera digital", u8"Export preset for hardware/DAW", u8"Exportar preset para pedaleira/DAW", u8"Exporter le preset pour p\u00e9dalier/DAW", u8"Preset f\u00fcr Modeler/DAW exportieren" },
        // TooltipDeletePreset
        { u8"Eliminar preset guardado", u8"Delete saved preset", u8"Excluir preset salvo", u8"Supprimer le preset", u8"Gespeichertes Preset l\u00f6schen" },
        // BtnBackArrow
        { u8"\u2190 VOLVER", u8"\u2190 BACK", u8"\u2190 VOLTAR", u8"\u2190 RETOUR", u8"\u2190 ZUR\u00dcCK" },
        // ExportChooseFormat
        { u8"Eleg\u00ed el formato para tu pedalera o DAW:", u8"Choose format for your pedalboard or DAW:", u8"Escolha o formato para sua pedaleira ou DAW:", u8"Choisissez le format pour votre p\u00e9dalier ou DAW :", u8"Format f\u00fcr dein Pedalboard oder DAW w\u00e4hlen:" },
        // TagBassParens
        { u8" (BAJO)", u8" (BASS)", u8" (BAIXO)", u8" (BASSE)", u8" (BASS)" },
        // TagGuitarParens
        { u8" (GUITARRA)", u8" (GUITAR)", u8" (GUITARRA)", u8" (GUITARE)", u8" (GITARRE)" },
        // ExportDialogPrefix
        { u8"Exportar ", u8"Export ", u8"Exportar ", u8"Exporter ", u8"Exportieren " },
        // ExportedOkPrefix
        { u8"\u00a1Exportado! ", u8"Exported! ", u8"Exportado! ", u8"Export\u00e9 ! ", u8"Exportiert! " },
        // ExportSaveError
        { u8"Error al guardar el archivo.", u8"Error saving file.", u8"Erro ao salvar o arquivo.", u8"Erreur lors de l'enregistrement.", u8"Fehler beim Speichern der Datei." },
        // ExportProprietaryNote
        { u8"Nota: Kemper (.kipr), Neural Capture, IK TONEX y Fractal son formatos cerrados propietarios; us\u00e1 .nam o .aidax como puente.",
          u8"Note: Kemper (.kipr), Neural Capture, IK TONEX and Fractal are closed formats; use .nam or .aidax as a bridge.",
          u8"Nota: Kemper (.kipr), Neural Capture, IK TONEX e Fractal s\u00e3o formatos fechados; use .nam ou .aidax como ponte.",
          u8"Note : Kemper (.kipr), Neural Capture, IK TONEX et Fractal sont ferm\u00e9s ; utilisez .nam ou .aidax comme passerelle.",
          u8"Hinweis: Kemper (.kipr), Neural Capture, IK TONEX und Fractal sind geschlossene Formate; nutze .nam oder .aidax." },

        // Export formats (11)
        { u8"NAM A2 \u00b7 M-Vave / TONE3000", u8"NAM A2 \u00b7 M-Vave / TONE3000", u8"NAM A2 \u00b7 M-Vave / TONE3000", u8"NAM A2 \u00b7 M-Vave / TONE3000", u8"NAM A2 \u00b7 M-Vave / TONE3000" },
        { u8"Lite 3ch + Full 8ch \u00b7 JuiceBox & ANNlab", u8"Lite 3ch + Full 8ch \u00b7 JuiceBox & ANNlab", u8"Lite 3ch + Full 8ch \u00b7 JuiceBox & ANNlab", u8"Lite 3ch + Full 8ch \u00b7 JuiceBox & ANNlab", u8"Lite 3ch + Full 8ch \u00b7 JuiceBox & ANNlab" },

        { u8"NAM A1 \u00b7 WaveNet Est\u00e1ndar", u8"NAM A1 \u00b7 Standard WaveNet", u8"NAM A1 \u00b7 WaveNet Padr\u00e3o", u8"NAM A1 \u00b7 WaveNet Standard", u8"NAM A1 \u00b7 Standard WaveNet" },
        { u8"Neural Amp Modeler cl\u00e1sico (48 kHz)", u8"Classic Neural Amp Modeler (48 kHz)", u8"Neural Amp Modeler cl\u00e1ssico (48 kHz)", u8"Neural Amp Modeler classique (48 kHz)", u8"Klassischer Neural Amp Modeler (48 kHz)" },

        { u8"NAM \u00b7 LSTM Ligero", u8"NAM \u00b7 Lightweight LSTM", u8"NAM \u00b7 LSTM Leve", u8"NAM \u00b7 LSTM L\u00e9ger", u8"NAM \u00b7 Leichtes LSTM" },
        { u8"Baja latencia / consumo m\u00ednimo CPU", u8"Low latency / minimal CPU usage", u8"Baixa lat\u00eancia / uso m\u00ednimo de CPU", u8"Faible latence / CPU minimale", u8"Geringe Latenz / minimale CPU-Last" },

        { u8"AIDA-X \u00b7 MOD Dwarf / Genome", u8"AIDA-X \u00b7 MOD Dwarf / Genome", u8"AIDA-X \u00b7 MOD Dwarf / Genome", u8"AIDA-X \u00b7 MOD Dwarf / Genome", u8"AIDA-X \u00b7 MOD Dwarf / Genome" },
        { u8"RTNeural Keras LSTM-12", u8"RTNeural Keras LSTM-12", u8"RTNeural Keras LSTM-12", u8"RTNeural Keras LSTM-12", u8"RTNeural Keras LSTM-12" },

        { u8"GuitarML Proteus", u8"GuitarML Proteus", u8"GuitarML Proteus", u8"GuitarML Proteus", u8"GuitarML Proteus" },
        { u8"Proteus Capture Player (LSTM-40)", u8"Proteus Capture Player (LSTM-40)", u8"Proteus Capture Player (LSTM-40)", u8"Proteus Capture Player (LSTM-40)", u8"Proteus Capture Player (LSTM-40)" },

        { u8"GuitarML NeuralPi", u8"GuitarML NeuralPi", u8"GuitarML NeuralPi", u8"GuitarML NeuralPi", u8"GuitarML NeuralPi" },
        { u8"NeuralPi / Raspberry Pi (LSTM-20)", u8"NeuralPi / Raspberry Pi (LSTM-20)", u8"NeuralPi / Raspberry Pi (LSTM-20)", u8"NeuralPi / Raspberry Pi (LSTM-20)", u8"NeuralPi / Raspberry Pi (LSTM-20)" },

        { u8"M-Vave Blackbox \u00b7 IR Cabina", u8"M-Vave Blackbox \u00b7 Cab IR", u8"M-Vave Blackbox \u00b7 IR Gabinete", u8"M-Vave Blackbox \u00b7 IR Baffle", u8"M-Vave Blackbox \u00b7 Cab IR" },
        { u8"44.1 kHz / 24-bit mono (IR CAB + EQ)", u8"44.1 kHz / 24-bit mono (IR CAB + EQ)", u8"44.1 kHz / 24-bit mono (IR CAB + EQ)", u8"44.1 kHz / 24-bit mono (IR CAB + EQ)", u8"44.1 kHz / 24-bit mono (IR CAB + EQ)" },

        { u8"M-Vave Blackbox \u00b7 Gu\u00eda", u8"M-Vave Blackbox \u00b7 Guide", u8"M-Vave Blackbox \u00b7 Guia", u8"M-Vave Blackbox \u00b7 Guide", u8"M-Vave Blackbox \u00b7 Anleitung" },
        { u8"Mapeo de m\u00f3dulos y perillas", u8"Module and knob mapping", u8"Mapeamento de m\u00f3dulos e knobs", u8"Mappage des modules et r\u00e9glages", u8"Modul- und Regler-Mapping" },

        { u8"Line 6 Helix / HX Stomp", u8"Line 6 Helix / HX Stomp", u8"Line 6 Helix / HX Stomp", u8"Line 6 Helix / HX Stomp", u8"Line 6 Helix / HX Stomp" },
        { u8"Preset JSON v6 compatible POD Go", u8"Preset JSON v6 compatible with POD Go", u8"Preset JSON v6 compat\u00edvel POD Go", u8"Preset JSON v6 compatible POD Go", u8"Preset JSON v6 POD Go-kompatibel" },

        { u8"HeadRush Pedalboard", u8"HeadRush Pedalboard", u8"HeadRush Pedalboard", u8"HeadRush Pedalboard", u8"HeadRush Pedalboard" },
        { u8"Rig JSON (Amp + Cab + FX)", u8"Rig JSON (Amp + Cab + FX)", u8"Rig JSON (Amp + Cab + FX)", u8"Rig JSON (Amp + Cab + FX)", u8"Rig JSON (Amp + Cab + FX)" },

        { u8"MI AMP \u00b7 Backup Portable", u8"MI AMP \u00b7 Portable Backup", u8"MI AMP \u00b7 Backup Port\u00e1til", u8"MI AMP \u00b7 Sauvegarde Portable", u8"MI AMP \u00b7 Portables Backup" },
        { u8"Preset completo importable en MI AMP", u8"Full preset importable in MI AMP", u8"Preset completo import\u00e1vel no MI AMP", u8"Preset complet importable dans MI AMP", u8"Vollst\u00e4ndiges Preset f\u00fcr MI AMP" },

        // Drop & Tuner
        { u8"DROP ACTIVADO", u8"DROP ACTIVE", u8"DROP ATIVO", u8"DROP ACTIF", u8"DROP AKTIV" },
        { u8"ACTIVAR DROP", u8"ENABLE DROP", u8"ATIVAR DROP", u8"ACTIVER DROP", u8"DROP AKTIVIEREN" },
        { u8"Semitonos", u8"Semitones", u8"Semitons", u8"Demi-tons", u8"Halbt\u00f6ne" },
        { u8"Cents", u8"Cents", u8"Cents", u8"Cents", u8"Cents" },
        { u8"Mix %", u8"Mix %", u8"Mix %", u8"Mix %", u8"Mix %" },
        { u8"DROP TUNE", u8"DROP TUNE", u8"DROP TUNE", u8"DROP TUNE", u8"DROP TUNE" },
        { u8"MUTE: ON", u8"MUTE: ON", u8"MUTE: ON", u8"MUTE: ON", u8"MUTE: AN" },
        { u8"MUTE: OFF", u8"MUTE: OFF", u8"MUTE: OFF", u8"MUTE: OFF", u8"MUTE: AUS" },
        { u8"MUTE", u8"MUTE", u8"MUTE", u8"MUTE", u8"MUTE" },
        { u8"DO RE MI", u8"DO RE MI", u8"DO RE MI", u8"DO RE MI", u8"DO RE MI" },
        { u8"C D E F G", u8"C D E F G", u8"C D E F G", u8"C D E F G", u8"C D E F G" },

        // Config
        { u8"MODO DE RUTA", u8"SIGNAL ROUTING", u8"MODO DE ROTA", u8"ROUTAGE DU SIGNAL", u8"SIGNALWEG" },
        { u8"CABINA 8x10", u8"8x10 CABINET", u8"GABINETE 8x10", u8"BAFFLE 8x10", u8"8x10 BOX" },
        { u8"CABINA DE GUITARRA", u8"GUITAR CABINET", u8"GABINETE DE GUITARRA", u8"BAFFLE GUITARE", u8"GITARREN-BOX" },
        { u8"CAB OFF", u8"CAB OFF", u8"CAB OFF", u8"CAB OFF", u8"BOX AUS" },
        { u8"CAB 8x10 ON", u8"CAB 8x10 ON", u8"CAB 8x10 ON", u8"CAB 8x10 ON", u8"8x10 AN" },
        { u8"CAB 8x10 OFF", u8"CAB 8x10 OFF", u8"CAB 8x10 OFF", u8"CAB 8x10 OFF", u8"8x10 AUS" },
        { u8"CANAL DEL CABEZAL", u8"AMP CHANNEL", u8"CANAL DO CABE\u00c7OTE", u8"CANAL D'AMPLI", u8"AMP-KANAL" },
        { u8"NOISE GATE", u8"NOISE GATE", u8"NOISE GATE", u8"NOISE GATE", u8"NOISE GATE" },
        { u8"INPUT TRIM", u8"INPUT TRIM", u8"INPUT TRIM", u8"GAIN D'ENTR\u00c9E", u8"EINGANGSPEGEL" },
        { u8"4x OVERSAMPLING: ON", u8"4x OVERSAMPLING: ON", u8"4x OVERSAMPLING: ON", u8"SUR\u00c9CHANTILLONNAGE 4x : ON", u8"4x OVERSAMPLING: AN" },
        { u8"4x OVERSAMPLING: OFF", u8"4x OVERSAMPLING: OFF", u8"4x OVERSAMPLING: OFF", u8"SUR\u00c9CHANTILLONNAGE 4x : OFF", u8"4x OVERSAMPLING: AUS" },
        { u8"IDIOMA / LANGUAGE", u8"LANGUAGE", u8"IDIOMA", u8"LANGUE", u8"SPRACHE" },
        { u8"GEMINI AI KEY (OPCIONAL)", u8"GEMINI AI KEY (OPTIONAL)", u8"CHAVE GEMINI AI (OPCIONAL)", u8"CL\u00c9 GEMINI AI (OPTIONNEL)", u8"GEMINI AI KEY (OPTIONAL)" },
        { u8"API Key Gemini (opcional)", u8"Gemini API Key (optional)", u8"Chave API Gemini (opcional)", u8"Cl\u00e9 API Gemini (optionnel)", u8"Gemini API Key (optional)" },
        { u8"LISTO!", u8"SAVED!", u8"SALVO!", u8"OK !", u8"GESPEICHERT!" },
        { u8"BORRADA", u8"CLEARED", u8"APAGADA", u8"EFFAC\u00c9E", u8"GEL\u00d6SCHT" },
        { u8"AUDIO STANDALONE (SIN DAW)", u8"STANDALONE AUDIO", u8"\u00c1UDIO STANDALONE", u8"AUDIO STANDALONE", u8"STANDALONE-AUDIO" },
        { u8"CONFIGURAR PLACA / AUDIO", u8"AUDIO / INTERFACE SETTINGS", u8"CONFIGURAR INTERFACE / \u00c1UDIO", u8"R\u00c9GLER INTERFACE / AUDIO", u8"AUDIO-INTERFACE W\u00c4HLEN" },
        { u8"Entrada silenciada (eleg\u00ed tu interfaz)", u8"Input muted (select your interface)", u8"Entrada silenciada (escolha a interface)", u8"Entr\u00e9e coup\u00e9e (choisir l'interface)", u8"Eingang stumm (Interface w\u00e4hlen)" },
        { u8"Audio: Sin dispositivo activo", u8"Audio: No active device", u8"\u00c1udio: Sem dispositivo ativo", u8"Audio : Aucun p\u00e9riph\u00e9rique actif", u8"Audio: Kein aktives Ger\u00e4t" },
        { u8"Sin entrada", u8"No input", u8"Sem entrada", u8"Aucune entr\u00e9e", u8"Kein Eingang" },
        { u8"ACTUALIZACIONES", u8"UPDATES", u8"ATUALIZA\u00c7\u00d5ES", u8"MISES \u00c0 JOUR", u8"UPDATES" },
        { u8"BUSCAR ACTUALIZACIONES", u8"CHECK FOR UPDATES", u8"BUSCAR ATUALIZA\u00c7\u00d5ES", u8"V\u00c9RIFIER MISES \u00c0 JOUR", u8"NACH UPDATES SUCHEN" },
        { u8"Buscando...", u8"Checking...", u8"Verificando...", u8"V\u00e9rification...", u8"Suche..." },
        { u8"Actualizado (ultima version)", u8"Up to date (latest)", u8"Atualizado (mais recente)", u8"\u00c0 jour (derni\u00e8re version)", u8"Aktuell (neueste Version)" },
        { u8"Sin conexi\u00f3n a GitHub", u8"Could not reach GitHub", u8"Sem conex\u00e3o com GitHub", u8"Impossible de joindre GitHub", u8"Keine Verbindung zu GitHub" },
        { u8"Versi\u00f3n ", u8"Version ", u8"Vers\u00e3o ", u8"Version ", u8"Version " },

        // Tone Finder
        { u8"Escrib\u00ed banda, canci\u00f3n o estilo:", u8"Enter band, song or style:", u8"Digite banda, m\u00fasica ou estilo:", u8"Entrez groupe, morceau ou style :", u8"Band, Song oder Stil eingeben:" },
        { u8"Ej: The Story So Far, Blink, Deftones...", u8"Ex: The Story So Far, Blink, Deftones...", u8"Ex: The Story So Far, Blink, Deftones...", u8"Ex : The Story So Far, Blink, Deftones...", u8"Z.B. The Story So Far, Blink, Deftones..." },
        { u8"BUSCAR", u8"SEARCH", u8"BUSCAR", u8"CHERCHER", u8"SUCHEN" },
        { u8"SUGERENCIAS", u8"SUGGESTIONS", u8"SUGEST\u00d5ES", u8"SUGGESTIONS", u8"VORSCHL\u00c4GE" },
        { u8"Buscando rig real...", u8"Searching real rig...", u8"Buscando rig real...", u8"Recherche du rig...", u8"Suche echtes Rig..." },
        { u8"APLICAR", u8"APPLY", u8"APLICAR", u8"APPLIQUER", u8"ANWENDEN" },
        { u8"GUARDAR PRESET", u8"SAVE PRESET", u8"SALVAR PRESET", u8"SAUVER PRESET", u8"PRESET SPEICHERN" },
        { u8"GUARDADO!", u8"SAVED!", u8"SALVO!", u8"SAUV\u00c9 !", u8"GESPEICHERT!" },
        { u8"\u00a1Tono guardado en Presets!", u8"Tone saved to Presets!", u8"Timbre salvo em Presets!", u8"Son enregistr\u00e9 dans Presets !", u8"Sound in Presets gespeichert!" },
        { u8"\u00bfQuer\u00e9s guardar este tono en Presets?", u8"Save this tone to Presets?", u8"Quer salvar este timbre nos Presets?", u8"Enregistrer ce son dans Presets ?", u8"Diesen Sound in Presets speichern?" },
        { u8"No est\u00e1 en la base. Copi\u00e1 este prompt para tu IA:", u8"Not in local DB. Copy this prompt for your AI:", u8"N\u00e3o est\u00e1 na base. Copie este prompt para sua IA:", u8"Absent de la base. Copiez ce prompt pour votre IA :", u8"Nicht in der Datenbank. Kopiere diesen KI-Prompt:" },
        { u8"COPIAR PROMPT PARA IA", u8"COPY AI PROMPT", u8"COPIAR PROMPT PARA IA", u8"COPIER PROMPT IA", u8"KI-PROMPT KOPIEREN" },
        { u8"\u00a1PROMPT COPIADO!", u8"PROMPT COPIED!", u8"PROMPT COPIADO!", u8"PROMPT COPI\u00c9 !", u8"PROMPT KOPIERT!" },
        { u8"Gratis y sin configuraci\u00f3n: arma cabezal, cabina, afinaci\u00f3n Drop y cadena de pedales al instante.",
          u8"Free and zero setup: builds amp head, cabinet, Drop tuning and pedal chain instantly.",
          u8"Gr\u00e1tis e sem configura\u00e7\u00e3o: monta cabe\u00e7ote, gabinete, afina\u00e7\u00e3o Drop e cadeia de pedais na hora.",
          u8"Gratuit et sans r\u00e9glage : configure ampli, baffle, Drop tuning et cha\u00eene de p\u00e9dales instantan\u00e9ment.",
          u8"Kostenlos ohne Setup: erstellt Amp, Box, Drop-Tuning und Pedalkette sofort." },
        { u8"pedales", u8"pedals", u8"pedais", u8"p\u00e9dales", u8"Pedale" },
        { u8"Cat\u00e1logo MI AMP", u8"MI AMP Catalog", u8"Cat\u00e1logo MI AMP", u8"Catalogue MI AMP", u8"MI AMP Katalog" },
        { u8"Gemini IA + Google Search", u8"Gemini AI + Google Search", u8"Gemini IA + Google Search", u8"Gemini IA + Google Search", u8"Gemini KI + Google Search" },
        { u8"Cadena armada con b\u00fasqueda web en vivo.", u8"Signal chain built from live web search.", u8"Cadeia montada com busca web ao vivo.", u8"Cha\u00eene construite par recherche web en direct.", u8"Signalkette per Websuche erstellt." },

        // Credit
        { u8"DESARROLLADO POR", u8"DEVELOPED BY", u8"DESENVOLVIDO POR", u8"D\u00c9VELOPP\u00c9 PAR", u8"ENTWICKELT VON" },

        // AutoUpdater
        { u8"ACTUALIZACI\u00d3N AUTOM\u00c1TICA", u8"AUTOMATIC UPDATE", u8"ATUALIZA\u00c7\u00c3O AUTOM\u00c1TICA", u8"MISE \u00c0 JOUR AUTOMATIQUE", u8"AUTOMATISCHES UPDATE" },
        { u8"Nueva versi\u00f3n disponible: v", u8"New version available: v", u8"Nova vers\u00e3o dispon\u00edvel: v", u8"Nouvelle version disponible : v", u8"Neue Version verf\u00fcgbar: v" },
        { u8"Descargando actualizaci\u00f3n...", u8"Downloading update...", u8"Baixando atualiza\u00e7\u00e3o...", u8"T\u00e9l\u00e9chargement de la mise \u00e0 jour...", u8"Update wird heruntergeladen..." },
        { u8"Instalando y reiniciando MI AMP...", u8"Installing and restarting MI AMP...", u8"Instalando e reiniciando MI AMP...", u8"Installation et red\u00e9marrage de MI AMP...", u8"MI AMP wird installiert und neu gestartet..." },
        { u8"Reiniciando...", u8"Restarting...", u8"Reiniciando...", u8"Red\u00e9marrage...", u8"Neustart..." },
        { u8"Para actualizar el plugin, cerr\u00e1 tu DAW y complet\u00e1 el instalador que se va a abrir.",
          u8"To update the plugin, close your DAW and run the installer that will open.",
          u8"Para atualizar o plugin, feche sua DAW e execute o instalador que ser\u00e1 aberto.",
          u8"Pour mettre \u00e0 jour le plugin, fermez votre DAW et lancez l'installateur.",
          u8"Um das Plugin zu aktualisieren, schlie\u00dfe deine DAW und starte den Installer." },
        { u8"ACTUALIZAR AHORA", u8"UPDATE NOW", u8"ATUALIZAR AGORA", u8"METTRE \u00c0 JOUR", u8"JETZT AKTUALISIEREN" },
        { u8"M\u00c1S TARDE", u8"LATER", u8"MAIS TARDE", u8"PLUS TARD", u8"SP\u00c4TER" },
        { u8"CONTINUAR SIN ACTUALIZAR", u8"CONTINUE WITHOUT UPDATING", u8"CONTINUAR SEM ATUALIZAR", u8"CONTINUER SANS METTRE \u00c0 JOUR", u8"OHNE UPDATE FORTFAHREN" },
        { u8"No se pudo descargar. Reintentar luego.", u8"Download failed. Try again later.", u8"Falha no download. Tente depois.", u8"\u00c9chec du t\u00e9l\u00e9chargement. R\u00e9essayez plus tard.", u8"Download fehlgeschlagen. Sp\u00e4ter erneut versuchen." }
    }};

    const int li = juce::jlimit (0, 4, static_cast<int> (currentLang()));
    const int mi = juce::jlimit (0, static_cast<int> (Msg::Count) - 1, static_cast<int> (id));
    return juce::String::fromUTF8 (kTable[static_cast<size_t> (mi)][static_cast<size_t> (li)]);
}

} // namespace miamp_i18n
