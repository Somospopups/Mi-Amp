#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <vector>

namespace miamp_rig
{
enum class AmpSwitchKind
{
    ToggleSilver,
    RockerRed,
    RockerBlack,
    PushRound,
    PushSquare,
    SlideBlack
};

enum class AmpSwitchAction
{
    BoolParam,
    ChannelCycle3, // cycles 0 -> 1 -> 2 (Recto)
    ChannelToggle01, // toggles 0 <-> 2 or 0 <-> 1
    VoiceCycle3 // cycles 0 -> 1 -> 2 (BE-100)
};

struct AmpKnobSpec
{
    float cx = 0.5f;    // normalized 0..1 of head image width
    float cy = 0.5f;    // normalized 0..1 of head image height
    float diamW = 0.045f; // diameter relative to head image width
    const char* paramId = nullptr;
    const char* label   = "";
};

struct AmpSwitchSpec
{
    float cx = 0.5f;
    float cy = 0.5f;
    float w  = 0.026f;  // hit width relative to head image width
    AmpSwitchKind kind = AmpSwitchKind::ToggleSilver;
    AmpSwitchAction action = AmpSwitchAction::BoolParam;
    const char* paramId = nullptr;
    const char* label   = "";
    bool invertLogic = false;
};

struct AmpSliderSpec
{
    float cx = 0.5f;
    float yTop = 0.44f;
    float yBot = 0.65f;
    const char* paramId = nullptr;
    const char* label   = "";
};

struct AmpHeadSpec
{
    int index = 0;
    bool forBass = false;
    const char* idName = "";
    const char* displayName = "";
    const char* subtitle = "";
    const void* chassisData = nullptr;
    int chassisSize = 0;
    const void* knobSpriteData = nullptr;
    int knobSpriteSize = 0;
    bool useClassicOverlay = false; // true for legacy Zenbox Mint (8) & Classic Mint (8)
    std::vector<AmpKnobSpec> knobs;
    std::vector<AmpSwitchSpec> switches;
    std::vector<AmpSliderSpec> sliders;
};

struct CabSpec
{
    int index = 0;
    bool forBass = false;
    bool isCustomIr = false;
    bool isOff = false;
    const char* displayName = "";
    const char* subtitle = "";
    const void* imageData = nullptr;
    int imageSize = 0;
    // Relative visual scale inside the stage cab slot (4x12/8x10 = 1.0)
    float widthScale  = 1.0f;
    float heightScale = 1.0f;
    // Normalized speaker grille rectangle within the cab image (where mics move)
    float grilleX = 0.12f, grilleY = 0.14f, grilleW = 0.76f, grilleH = 0.72f;
};

inline const std::vector<AmpHeadSpec>& getGuitarHeadSpecs()
{
    static const std::vector<AmpHeadSpec> specs = {
        {
            0, false, "mi800", "MI AMP 800", "British Crunch Icon (100W EL34)",
            BinaryData::head_mi800_png, BinaryData::head_mi800_pngSize,
            BinaryData::knob_marshall_gold_png, BinaryData::knob_marshall_gold_pngSize, false,
            {
                { 0.3039f, 0.7470f, 0.0480f, MiAmpParams::gtrChPresence[2], "PRESENCE" },
                { 0.3902f, 0.7454f, 0.0480f, MiAmpParams::gtrChTreble[2], "TREBLE" },
                { 0.4724f, 0.7468f, 0.0480f, MiAmpParams::gtrChMid[2], "MIDDLE" },
                { 0.5571f, 0.7472f, 0.0480f, MiAmpParams::gtrChBass[2], "BASS" },
                { 0.6418f, 0.7472f, 0.0480f, MiAmpParams::gtrChMaster[2], "MASTER" },
                { 0.7263f, 0.7476f, 0.0480f, MiAmpParams::gtrChGain[2], "PRE-AMP" }
            },
            {
                { 0.1175f, 0.7448f, 0.0420f, AmpSwitchKind::RockerRed, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1802f, 0.7446f, 0.0420f, AmpSwitchKind::RockerRed, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false }
            }, {}
        },
        {
            1, false, "miplexi", "MI AMP PLEXI", "1968 Vintage Non-Master Super Lead",
            BinaryData::head_miplexi_png, BinaryData::head_miplexi_pngSize,
            BinaryData::knob_marshall_gold_png, BinaryData::knob_marshall_gold_pngSize, false,
            {
                { 0.2920f, 0.7227f, 0.0550f, MiAmpParams::gtrChPresence[1], "PRESENCE" },
                { 0.3710f, 0.7227f, 0.0550f, MiAmpParams::gtrChBass[1], "BASS" },
                { 0.4490f, 0.7227f, 0.0550f, MiAmpParams::gtrChMid[1], "MIDDLE" },
                { 0.5250f, 0.7227f, 0.0550f, MiAmpParams::gtrChTreble[1], "TREBLE" },
                { 0.6010f, 0.7227f, 0.0550f, MiAmpParams::gtrChGain[1], "VOLUME I" },
                { 0.6740f, 0.7227f, 0.0550f, MiAmpParams::gtrChGain[0], "VOLUME II" }
            },
            {
                { 0.1450f, 0.7130f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.2140f, 0.7130f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false }
            }, {}
        },
        {
            2, false, "mirecto", "MI AMP RECTO", "Dual Channel Modern High-Gain",
            BinaryData::head_mirecto_png, BinaryData::head_mirecto_pngSize,
            BinaryData::knob_mesa_chrome_png, BinaryData::knob_mesa_chrome_pngSize, false,
            {
                { 0.2200f, 0.6586f, 0.0440f, MiAmpParams::gtrChPresence[1], "CH2 PRES" },
                { 0.3020f, 0.6586f, 0.0440f, MiAmpParams::gtrChBass[1], "CH2 BASS" },
                { 0.3830f, 0.6586f, 0.0440f, MiAmpParams::gtrChMid[1], "CH2 MID" },
                { 0.4640f, 0.6586f, 0.0440f, MiAmpParams::gtrChTreble[1], "CH2 TREBLE" },
                { 0.5450f, 0.6586f, 0.0440f, MiAmpParams::gtrChGain[1], "CH2 GAIN" },
                { 0.6250f, 0.6586f, 0.0440f, MiAmpParams::gtrChMaster[1], "CH2 MASTER" },
                { 0.7060f, 0.6586f, 0.0440f, MiAmpParams::gtrOutput, "OUTPUT" },
                { 0.2200f, 0.7837f, 0.0440f, MiAmpParams::gtrChPresence[2], "CH3 PRES" },
                { 0.3020f, 0.7837f, 0.0440f, MiAmpParams::gtrChBass[2], "CH3 BASS" },
                { 0.3830f, 0.7837f, 0.0440f, MiAmpParams::gtrChMid[2], "CH3 MID" },
                { 0.4640f, 0.7837f, 0.0440f, MiAmpParams::gtrChTreble[2], "CH3 TREBLE" },
                { 0.5450f, 0.7837f, 0.0440f, MiAmpParams::gtrChGain[2], "CH3 GAIN" },
                { 0.6250f, 0.7837f, 0.0440f, MiAmpParams::gtrChMaster[2], "CH3 MASTER" },
                { 0.7060f, 0.7837f, 0.0440f, MiAmpParams::gtrOutput, "SOLO" }
            },
            {
                { 0.1560f, 0.7212f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::ChannelCycle3, MiAmpParams::gtrChannel, "CHANNEL", false },
                { 0.7680f, 0.7212f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.8280f, 0.7212f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true }
            }, {}
        },
        {
            3, false, "mi6505", "MI AMP HG-120", "120W Tight Metal Lead & Crunch",
            BinaryData::head_mi6505_png, BinaryData::head_mi6505_pngSize,
            BinaryData::knob_peavey_black_png, BinaryData::knob_peavey_black_pngSize, false,
            {
                { 0.1233f, 0.7013f, 0.0360f, MiAmpParams::gtrChGain[0], "RHY PRE" },
                { 0.2258f, 0.7013f, 0.0360f, MiAmpParams::gtrChBass[0], "RHY LOW" },
                { 0.2775f, 0.7013f, 0.0360f, MiAmpParams::gtrChMid[0], "RHY MID" },
                { 0.3283f, 0.7013f, 0.0360f, MiAmpParams::gtrChTreble[0], "RHY HIGH" },
                { 0.3792f, 0.7013f, 0.0360f, MiAmpParams::gtrChMaster[0], "RHY POST" },
                { 0.4892f, 0.7013f, 0.0360f, MiAmpParams::gtrChGain[2], "LEAD PRE" },
                { 0.5425f, 0.7013f, 0.0360f, MiAmpParams::gtrChBass[2], "LEAD LOW" },
                { 0.5958f, 0.7013f, 0.0360f, MiAmpParams::gtrChMid[2], "LEAD MID" },
                { 0.6492f, 0.7013f, 0.0360f, MiAmpParams::gtrChTreble[2], "LEAD HIGH" },
                { 0.7017f, 0.7013f, 0.0360f, MiAmpParams::gtrChMaster[2], "LEAD POST" },
                { 0.7642f, 0.7013f, 0.0360f, MiAmpParams::gtrResonance, "RESONANCE" },
                { 0.8167f, 0.7013f, 0.0360f, MiAmpParams::gtrChPresence[2], "PRESENCE" }
            },
            {
                { 0.1630f, 0.7013f, 0.0220f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::gtrBright2, "BRIGHT", false },
                { 0.1840f, 0.7013f, 0.0220f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::gtrCrunchSw, "CRUNCH", false },
                { 0.4310f, 0.7013f, 0.0240f, AmpSwitchKind::PushSquare, AmpSwitchAction::ChannelToggle01, MiAmpParams::gtrChannel, "CHANNEL", false },
                { 0.8750f, 0.7013f, 0.0280f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.9220f, 0.7013f, 0.0280f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true }
            }, {}
        },
        {
            4, false, "miac30", "MI AMP AC-30", "30W Class-A Chime & Tremolo",
            BinaryData::head_miac30_png, BinaryData::head_miac30_pngSize,
            BinaryData::knob_chicken_cream_png, BinaryData::knob_chicken_cream_pngSize, false,
            {
                { 0.2050f, 0.2857f, 0.0430f, MiAmpParams::gtrChGain[0], "NORMAL VOL" },
                { 0.2600f, 0.2857f, 0.0430f, MiAmpParams::gtrChGain[1], "TOP BOOST" },
                { 0.3140f, 0.2857f, 0.0430f, MiAmpParams::gtrChTreble[1], "TREBLE" },
                { 0.3710f, 0.2857f, 0.0430f, MiAmpParams::gtrChBass[1], "BASS" },
                { 0.4260f, 0.2857f, 0.0430f, MiAmpParams::gtrAmpRev, "REVERB TONE" },
                { 0.4840f, 0.2857f, 0.0430f, MiAmpParams::gtrChPresence[1], "REVERB LVL" },
                { 0.5400f, 0.2857f, 0.0430f, MiAmpParams::gtrTremSpd, "TREM SPEED" },
                { 0.5960f, 0.2857f, 0.0430f, MiAmpParams::gtrTremDep, "TREM DEPTH" },
                { 0.6540f, 0.2857f, 0.0430f, MiAmpParams::gtrChPresence[0], "TONE CUT" },
                { 0.7120f, 0.2857f, 0.0430f, MiAmpParams::gtrOutput, "MASTER VOL" }
            },
            {
                { 0.7610f, 0.2857f, 0.0240f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrModeBright, "BRILLIANCE", false },
                { 0.7930f, 0.2857f, 0.0240f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrBright2, "EQ MODE", false },
                { 0.8440f, 0.2857f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.8880f, 0.2857f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true }
            }, {}
        },
        {
            5, false, "mirocker", "MI AMP ROCKER", "Thick British Crunch & Spring Reverb",
            BinaryData::head_mirocker_png, BinaryData::head_mirocker_pngSize,
            BinaryData::knob_orange_black_png, BinaryData::knob_orange_black_pngSize, false,
            {
                { 0.2543f, 0.6699f, 0.0460f, MiAmpParams::gtrAtten, "ATTENUATOR" },
                { 0.3212f, 0.6699f, 0.0460f, MiAmpParams::gtrAmpRev, "REVERB" },
                { 0.3851f, 0.6699f, 0.0460f, MiAmpParams::gtrChMaster[2], "DIRTY VOL" },
                { 0.4498f, 0.6699f, 0.0460f, MiAmpParams::gtrChTreble[2], "TREBLE" },
                { 0.5127f, 0.6699f, 0.0460f, MiAmpParams::gtrChMid[2], "MID" },
                { 0.5776f, 0.6699f, 0.0460f, MiAmpParams::gtrChBass[2], "BASS" },
                { 0.6420f, 0.6699f, 0.0460f, MiAmpParams::gtrChGain[2], "GAIN" },
                { 0.7062f, 0.6699f, 0.0460f, MiAmpParams::gtrCleanTreb, "CLEAN TREB" },
                { 0.7694f, 0.6699f, 0.0460f, MiAmpParams::gtrCleanBass, "CLEAN BASS" },
                { 0.8317f, 0.6699f, 0.0460f, MiAmpParams::gtrChGain[0], "CLEAN VOL" }
            },
            {
                { 0.1070f, 0.6699f, 0.0280f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1510f, 0.6699f, 0.0280f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrHalfPwr, "FULL/HALF", false },
                { 0.1950f, 0.6699f, 0.0280f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::ChannelToggle01, MiAmpParams::gtrChannel, "CLEAN/DIRTY", false }
            }, {}
        },
        {
            6, false, "mibe100", "MI AMP BE-100", "Modded Boutique British Hot-Rod",
            BinaryData::head_mibe100_png, BinaryData::head_mibe100_pngSize,
            BinaryData::knob_be_black_png, BinaryData::knob_be_black_pngSize, false,
            {
                { 0.2770f, 0.6947f, 0.0480f, MiAmpParams::gtrChPresence[2], "PRESENCE" },
                { 0.3540f, 0.6947f, 0.0480f, MiAmpParams::gtrChBass[2], "BASS" },
                { 0.4310f, 0.6947f, 0.0480f, MiAmpParams::gtrChMid[2], "MIDDLE" },
                { 0.5090f, 0.6947f, 0.0480f, MiAmpParams::gtrChTreble[2], "TREBLE" },
                { 0.5860f, 0.6947f, 0.0480f, MiAmpParams::gtrChMaster[2], "MASTER" },
                { 0.6630f, 0.6947f, 0.0480f, MiAmpParams::gtrChGain[2], "GAIN" },
                { 0.8400f, 0.6947f, 0.0500f, MiAmpParams::gtrChGain[0], "CLEAN VOL" }
            },
            {
                { 0.0720f, 0.6870f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1350f, 0.6870f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.7250f, 0.7062f, 0.0240f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::VoiceCycle3, MiAmpParams::gtrVoiceSw, "VOICE", false },
                { 0.7750f, 0.7062f, 0.0240f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrModeBright, "BRIGHT", false }
            }, {}
        },
        {
            7, false, "mitwin", "MI AMP TWIN", "65 Blackface Clean Headroom & Vibrato",
            BinaryData::head_mitwin_png, BinaryData::head_mitwin_pngSize,
            BinaryData::knob_fender_skirt_png, BinaryData::knob_fender_skirt_pngSize, false,
            {
                { 0.1780f, 0.7478f, 0.0370f, MiAmpParams::gtrChGain[0], "NORM VOL" },
                { 0.2310f, 0.7478f, 0.0370f, MiAmpParams::gtrChTreble[0], "NORM TREB" },
                { 0.2860f, 0.7478f, 0.0370f, MiAmpParams::gtrChMid[0], "NORM MID" },
                { 0.3400f, 0.7478f, 0.0370f, MiAmpParams::gtrChBass[0], "NORM BASS" },
                { 0.5100f, 0.7478f, 0.0370f, MiAmpParams::gtrChGain[1], "VIB VOL" },
                { 0.5610f, 0.7478f, 0.0370f, MiAmpParams::gtrChTreble[1], "VIB TREB" },
                { 0.6110f, 0.7478f, 0.0370f, MiAmpParams::gtrChMid[1], "VIB MID" },
                { 0.6610f, 0.7478f, 0.0370f, MiAmpParams::gtrChBass[1], "VIB BASS" },
                { 0.7110f, 0.7478f, 0.0370f, MiAmpParams::gtrAmpRev, "REVERB" },
                { 0.7620f, 0.7478f, 0.0370f, MiAmpParams::gtrTremSpd, "SPEED" },
                { 0.8130f, 0.7478f, 0.0370f, MiAmpParams::gtrTremDep, "INTENSITY" }
            },
            {
                { 0.1350f, 0.7478f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrModeBright, "BRIGHT N", false },
                { 0.4680f, 0.7478f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrBright2, "BRIGHT V", false }
            }, {}
        },
        {
            8, false, "zenbox", "MI AMP ZENBOX", "Signature 3-Channel California Mint",
            BinaryData::head_zenbox_mint_png, BinaryData::head_zenbox_mint_pngSize,
            BinaryData::knob_zenbox_mint_png, BinaryData::knob_zenbox_mint_pngSize, false,
            {
                { 0.2460f, 0.6680f, 0.0440f, MiAmpParams::gtrChGain[0], "CH1 GAIN" },
                { 0.2990f, 0.6680f, 0.0440f, MiAmpParams::gtrChTreble[0], "CH1 TREBLE" },
                { 0.3540f, 0.6680f, 0.0440f, MiAmpParams::gtrChMid[0], "CH1 MID" },
                { 0.2460f, 0.7740f, 0.0440f, MiAmpParams::gtrChGain[1], "CH2 GAIN" },
                { 0.2990f, 0.7740f, 0.0440f, MiAmpParams::gtrChTreble[1], "CH2 TREBLE" },
                { 0.3540f, 0.7740f, 0.0440f, MiAmpParams::gtrChMid[1], "CH2 MID" },
                { 0.4330f, 0.6680f, 0.0440f, MiAmpParams::gtrChGain[2], "CH3 GAIN" },
                { 0.4860f, 0.6680f, 0.0440f, MiAmpParams::gtrChTreble[2], "CH3 TREBLE" },
                { 0.5400f, 0.6680f, 0.0440f, MiAmpParams::gtrChMid[2], "CH3 MID" },
                { 0.4330f, 0.7740f, 0.0440f, MiAmpParams::gtrChBass[0], "CH1 BASS" },
                { 0.4860f, 0.7740f, 0.0440f, MiAmpParams::gtrChPresence[0], "CH1 PRESENCE" },
                { 0.5400f, 0.7740f, 0.0440f, MiAmpParams::gtrChMaster[0], "CH1 MASTER" },
                { 0.6180f, 0.6680f, 0.0440f, MiAmpParams::gtrChBass[1], "CH2 BASS" },
                { 0.6710f, 0.6680f, 0.0440f, MiAmpParams::gtrChPresence[1], "CH2 PRESENCE" },
                { 0.7240f, 0.6680f, 0.0440f, MiAmpParams::gtrChMaster[1], "CH2 MASTER" },
                { 0.6180f, 0.7740f, 0.0440f, MiAmpParams::gtrChBass[2], "CH3 BASS" },
                { 0.6710f, 0.7740f, 0.0440f, MiAmpParams::gtrChPresence[2], "CH3 PRESENCE" },
                { 0.7240f, 0.7740f, 0.0440f, MiAmpParams::gtrChMaster[2], "CH3 MASTER" },
                { 0.7990f, 0.7310f, 0.0640f, MiAmpParams::gtrOutput, "OUTPUT" }
            },
            {
                { 0.1110f, 0.7240f, 0.0320f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1800f, 0.7240f, 0.0250f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.4020f, 0.7260f, 0.0170f, AmpSwitchKind::SlideBlack, AmpSwitchAction::BoolParam, MiAmpParams::gtrModeBright, "MODE / BRIGHT", false }
            }, {}
        }
    };
    return specs;
}

inline const std::vector<AmpHeadSpec>& getBassHeadSpecs()
{
    static const std::vector<AmpHeadSpec> specs = {
        {
            0, true, "cl300", "MI AMP CL-300", "300W All-Tube 6550 Stadium Head",
            BinaryData::bass_head_svt_png, BinaryData::bass_head_svt_pngSize,
            BinaryData::knob_svt_fluted_png, BinaryData::knob_svt_fluted_pngSize, false,
            {
                { 0.1860f, 0.6688f, 0.0560f, MiAmpParams::ampGain, "GAIN" },
                { 0.3910f, 0.6688f, 0.0560f, MiAmpParams::ampBass, "BASS" },
                { 0.4830f, 0.6688f, 0.0560f, MiAmpParams::ampMid, "MIDRANGE" },
                { 0.5760f, 0.6688f, 0.0560f, MiAmpParams::ampFreq, "FREQUENCY" },
                { 0.6690f, 0.6688f, 0.0560f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.7580f, 0.6688f, 0.0560f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.2650f, 0.6688f, 0.0320f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "ULTRA HI", false },
                { 0.3120f, 0.6688f, 0.0320f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::bassUltraLo, "ULTRA LO", false },
                { 0.8290f, 0.6688f, 0.0320f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STANDBY", false },
                { 0.8800f, 0.6688f, 0.0320f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            1, true, "pro450", "MI AMP PRO-450", "Touring Rack Hybrid + 9-Band GEQ",
            BinaryData::bass_head_svt3pro_png, BinaryData::bass_head_svt3pro_pngSize,
            BinaryData::knob_svt_fluted_png, BinaryData::knob_svt_fluted_pngSize, false,
            {
                { 0.2350f, 0.5617f, 0.0380f, MiAmpParams::ampGain, "GAIN" },
                { 0.3080f, 0.5617f, 0.0380f, MiAmpParams::ampBass, "BASS" },
                { 0.3510f, 0.5617f, 0.0380f, MiAmpParams::ampMid, "MID" },
                { 0.3940f, 0.5617f, 0.0380f, MiAmpParams::ampFreq, "FREQ" },
                { 0.4390f, 0.5617f, 0.0380f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4870f, 0.5617f, 0.0380f, MiAmpParams::bassTubeGain, "TUBE GAIN" },
                { 0.5340f, 0.5617f, 0.0380f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.1630f, 0.5617f, 0.0170f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassMuteSw, "MUTE", false },
                { 0.1970f, 0.5617f, 0.0170f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "-15dB PAD", false },
                { 0.2730f, 0.5554f, 0.0170f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "ULTRA HI/LO", false },
                { 0.5690f, 0.5107f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BRIGHT", false },
                { 0.5690f, 0.5585f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassHeadEqOn, "EQ ON", false },
                { 0.8670f, 0.5511f, 0.0340f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {
                { 0.6170f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[0], "31Hz" },
                { 0.6380f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[1], "63Hz" },
                { 0.6600f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[2], "125Hz" },
                { 0.6810f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[3], "250Hz" },
                { 0.7030f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[4], "500Hz" },
                { 0.7250f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[5], "1KHz" },
                { 0.7460f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[6], "2KHz" },
                { 0.7680f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[7], "4KHz" },
                { 0.7890f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[8], "8KHz" },
                { 0.8180f, 0.4681f, 0.6224f, MiAmpParams::bassHeadEq[9], "LEVEL" }
            }
        },
        {
            2, true, "bassman", "MI AMP BASSMAN", "Silverface Vintage Dual-Channel Tube",
            BinaryData::bass_head_bassman_png, BinaryData::bass_head_bassman_pngSize,
            BinaryData::knob_fender_skirt_png, BinaryData::knob_fender_skirt_pngSize, false,
            {
                { 0.2590f, 0.6930f, 0.0480f, MiAmpParams::ampGain, "BASS VOL" },
                { 0.3410f, 0.6930f, 0.0480f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4190f, 0.6930f, 0.0480f, MiAmpParams::ampBass, "BASS" },
                { 0.6710f, 0.6930f, 0.0480f, MiAmpParams::bassMaster, "NORM VOL" },
                { 0.7480f, 0.6930f, 0.0480f, MiAmpParams::ampMid, "NORM TREB" },
                { 0.8270f, 0.6930f, 0.0480f, MiAmpParams::ampFreq, "NORM BASS" }
            },
            {
                { 0.1870f, 0.7056f, 0.0240f, AmpSwitchKind::SlideBlack, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "DEEP", false },
                { 0.5940f, 0.7056f, 0.0240f, AmpSwitchKind::SlideBlack, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BRIGHT", false }
            }, {}
        },
        {
            3, true, "rb800", "MI AMP RB-800", "80s Bi-Amp Punch Rack Head",
            BinaryData::bass_head_gk800_png, BinaryData::bass_head_gk800_pngSize,
            BinaryData::knob_gk_black_png, BinaryData::knob_gk_black_pngSize, false,
            {
                { 0.2140f, 0.5923f, 0.0390f, MiAmpParams::ampGain, "VOLUME" },
                { 0.3910f, 0.5923f, 0.0390f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4440f, 0.5923f, 0.0390f, MiAmpParams::ampFreq, "HI-MID" },
                { 0.4950f, 0.5923f, 0.0390f, MiAmpParams::ampMid, "LO-MID" },
                { 0.5480f, 0.5923f, 0.0390f, MiAmpParams::ampBass, "BASS" },
                { 0.6510f, 0.5923f, 0.0390f, MiAmpParams::bassDriveCtrl, "BOOST" },
                { 0.7060f, 0.5923f, 0.0390f, MiAmpParams::ampFreq, "X-OVER" },
                { 0.7670f, 0.5923f, 0.0390f, MiAmpParams::bassBiAmpLo, "BI-AMP" },
                { 0.8190f, 0.5923f, 0.0390f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.1620f, 0.5923f, 0.0240f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "-10dB PAD", false },
                { 0.2670f, 0.5923f, 0.0240f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassUltraLo, "LO CUT", false },
                { 0.3010f, 0.5923f, 0.0240f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "MID CONTOUR", false },
                { 0.3350f, 0.5923f, 0.0240f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "HI BOOST", false },
                { 0.6020f, 0.5923f, 0.0240f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BOOST SW", false },
                { 0.8750f, 0.5923f, 0.0340f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            4, true, "m900", "MI AMP M-900", "Modern Dual-Engine Microtubes + 6-Band EQ",
            BinaryData::bass_head_dg900_png, BinaryData::bass_head_dg900_pngSize,
            BinaryData::knob_dg_alum_png, BinaryData::knob_dg_alum_pngSize, false,
            {
                { 0.2460f, 0.4964f, 0.0580f, MiAmpParams::bassCompCtrl, "COMP" },
                { 0.3400f, 0.4964f, 0.0500f, MiAmpParams::bassDriveCtrl, "DRIVE" },
                { 0.4040f, 0.4964f, 0.0500f, MiAmpParams::ampTreble, "TONE" },
                { 0.4780f, 0.4964f, 0.0500f, MiAmpParams::ampBass, "LEVEL" },
                { 0.5510f, 0.4964f, 0.0500f, MiAmpParams::bassBlendCtrl, "BLEND" },
                { 0.6250f, 0.4964f, 0.0500f, MiAmpParams::ampGain, "GAIN" },
                { 0.9130f, 0.4964f, 0.0600f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.1380f, 0.4122f, 0.0170f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "PASSIVE", false },
                { 0.1840f, 0.4538f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassMuteSw, "MUTE", false },
                { 0.3650f, 0.6002f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "ENGINE", false },
                { 0.5250f, 0.6002f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "VOICING", false }
            }, {
                { 0.7067f, 0.3797f, 0.6020f, MiAmpParams::bassHeadEq[0], "LOW" },
                { 0.7358f, 0.3797f, 0.6020f, MiAmpParams::bassHeadEq[2], "250Hz" },
                { 0.7642f, 0.3797f, 0.6020f, MiAmpParams::bassHeadEq[4], "500Hz" },
                { 0.7933f, 0.3797f, 0.6020f, MiAmpParams::bassHeadEq[5], "1.5kHz" },
                { 0.8217f, 0.3797f, 0.6020f, MiAmpParams::bassHeadEq[6], "3kHz" },
                { 0.8508f, 0.3797f, 0.6020f, MiAmpParams::bassHeadEq[8], "HIGH" }
            }
        },
        {
            5, true, "b15", "MI AMP B-15", "Motown Warm Flip-Top Studio Tube",
            BinaryData::bass_head_b15_png, BinaryData::bass_head_b15_pngSize,
            BinaryData::knob_svt_fluted_png, BinaryData::knob_svt_fluted_pngSize, false,
            {
                { 0.2060f, 0.7024f, 0.0540f, MiAmpParams::ampGain, "CH1 VOL" },
                { 0.2860f, 0.7024f, 0.0540f, MiAmpParams::ampTreble, "CH1 TREB" },
                { 0.3650f, 0.7024f, 0.0540f, MiAmpParams::ampBass, "CH1 BASS" },
                { 0.5140f, 0.7024f, 0.0540f, MiAmpParams::bassMaster, "CH2 VOL" },
                { 0.5880f, 0.7024f, 0.0540f, MiAmpParams::ampMid, "CH2 TREB" },
                { 0.6600f, 0.7024f, 0.0540f, MiAmpParams::ampFreq, "CH2 BASS" }
            },
            {
                { 0.7450f, 0.7092f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STANDBY", false },
                { 0.8020f, 0.7092f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassUltraLo, "GROUND", false },
                { 0.8580f, 0.7092f, 0.0320f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            6, true, "or200", "MI AMP OR-200", "200W Stoner / Rock All-Tube Growl",
            BinaryData::bass_head_orange200_png, BinaryData::bass_head_orange200_pngSize,
            BinaryData::knob_orange_black_png, BinaryData::knob_orange_black_pngSize, false,
            {
                { 0.3760f, 0.6943f, 0.0660f, MiAmpParams::bassMaster, "MASTER" },
                { 0.4600f, 0.6943f, 0.0660f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.5450f, 0.6943f, 0.0660f, MiAmpParams::ampMid, "MIDDLE" },
                { 0.6290f, 0.6943f, 0.0660f, MiAmpParams::ampBass, "BASS" },
                { 0.7140f, 0.6943f, 0.0660f, MiAmpParams::ampGain, "GAIN" }
            },
            {
                { 0.1160f, 0.7109f, 0.0340f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true },
                { 0.2000f, 0.7080f, 0.0270f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STBY", false }
            }, {}
        },
        {
            7, true, "db750", "MI AMP DB-750", "750W Hybrid Touring Rack Punch",
            BinaryData::bass_head_aguilar_png, BinaryData::bass_head_aguilar_pngSize,
            BinaryData::knob_aguilar_silver_png, BinaryData::knob_aguilar_silver_pngSize, false,
            {
                { 0.2260f, 0.5657f, 0.0480f, MiAmpParams::ampGain, "GAIN" },
                { 0.3650f, 0.5657f, 0.0480f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4385f, 0.5657f, 0.0480f, MiAmpParams::ampMid, "MIDRANGE" },
                { 0.5120f, 0.5657f, 0.0480f, MiAmpParams::ampBass, "BASS" },
                { 0.6350f, 0.5657f, 0.0480f, MiAmpParams::bassMaster, "MASTER" },
                { 0.7045f, 0.5657f, 0.0480f, MiAmpParams::bassDriveCtrl, "FX SEND" },
                { 0.7740f, 0.5657f, 0.0480f, MiAmpParams::ampFreq, "FX RETURN" }
            },
            {
                { 0.1750f, 0.5657f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "-6dB PAD", false },
                { 0.2980f, 0.5657f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BRIGHT", false },
                { 0.5760f, 0.5657f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "DEEP", false },
                { 0.8320f, 0.5657f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassMuteSw, "MUTE", false },
                { 0.8760f, 0.5589f, 0.0340f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            8, true, "classic", "MI AMP CLASSIC", "Signature California Mint Tube Bass",
            BinaryData::bass_head_mint_png, BinaryData::bass_head_mint_pngSize,
            BinaryData::knob_bass_mint_png, BinaryData::knob_bass_mint_pngSize, false,
            {
                { 0.2841f, 0.6759f, 0.0700f, MiAmpParams::ampGain, "GAIN" },
                { 0.3836f, 0.6759f, 0.0700f, MiAmpParams::ampBass, "BASS" },
                { 0.4810f, 0.6759f, 0.0700f, MiAmpParams::ampMid, "MID" },
                { 0.5796f, 0.6759f, 0.0700f, MiAmpParams::ampFreq, "FREQ" },
                { 0.6784f, 0.6759f, 0.0700f, MiAmpParams::ampTreble, "TREBLE" }
            },
            {
                { 0.1082f, 0.6852f, 0.0360f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true },
                { 0.1857f, 0.6878f, 0.0360f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "ULTRA HI/LO", false },
                { 0.7710f, 0.6928f, 0.0360f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STANDBY", false }
            }, {}
        }
    };
    return specs;
}

inline const std::vector<CabSpec>& getGuitarCabSpecs()
{
    static const std::vector<CabSpec> specs = {
        { 0, false, false, false, "4x12 GREENBACK", "British 25W Warm Punch",      BinaryData::cab_4x12_green_png,     BinaryData::cab_4x12_green_pngSize,     1.00f, 1.00f, 0.12f, 0.1711f, 0.76f, 0.6940f },
        { 1, false, false, false, "4x12 BRIT V30",  "Vintage 30 Aggressive Bite",  BinaryData::cab_4x12_britv30_png,   BinaryData::cab_4x12_britv30_pngSize,   1.00f, 1.00f, 0.12f, 0.1732f, 0.76f, 0.6922f },
        { 2, false, false, false, "4x12 OVERSIZED", "Modern Tight Sub-Low End",    BinaryData::cab_4x12_recto_png,     BinaryData::cab_4x12_recto_pngSize,     1.00f, 1.04f, 0.12f, 0.1400f, 0.76f, 0.7200f },
        { 3, false, false, false, "4x12 HI-GAIN",   "Focused Metal Articulation",  BinaryData::cab_4x12_evh_png,       BinaryData::cab_4x12_evh_pngSize,       1.00f, 1.00f, 0.12f, 0.1766f, 0.76f, 0.6894f },
        { 4, false, false, false, "2x12 CLASS-A",   "Alnico Blue Open-Back Chime", BinaryData::cab_2x12_classa_png,    BinaryData::cab_2x12_classa_pngSize,    0.96f, 0.62f, 0.12f, 0.2339f, 0.76f, 0.6166f },
        { 5, false, false, false, "4x12 TANGERINE", "Thick Mid-Rich Sludge",       BinaryData::cab_4x12_tangerine_png, BinaryData::cab_4x12_tangerine_pngSize, 1.00f, 0.96f, 0.12f, 0.1827f, 0.76f, 0.6843f },
        { 6, false, false, false, "2x12 BLACKFACE", "Jensen Clean Sparkle",        BinaryData::cab_2x12_twin_png,      BinaryData::cab_2x12_twin_pngSize,      0.96f, 0.60f, 0.12f, 0.2321f, 0.76f, 0.6181f },
        { 7, false, true,  false, "CUSTOM WAV IR",  "Load External .WAV Impulse",  BinaryData::cab_4x12_britv30_png,   BinaryData::cab_4x12_britv30_pngSize,   1.00f, 1.00f, 0.12f, 0.1732f, 0.76f, 0.6922f },
        { 8, false, false, true,  "CAB OFF",        "Direct Amp Output (No Cab)",  nullptr, 0, 1.00f, 1.00f, 0.12f, 0.14f, 0.76f, 0.72f }
    };
    return specs;
}

inline const std::vector<CabSpec>& getBassCabSpecs()
{
    static const std::vector<CabSpec> specs = {
        { 0, true, false, false, "1x15 VINTAGE",    "Flip-Top Warm Sub",         BinaryData::bass_cab_1x15_b15_png,       BinaryData::bass_cab_1x15_b15_pngSize,       0.84f, 0.76f, 0.14f, 0.1835f, 0.72f, 0.6571f },
        { 1, true, false, false, "4x10 TOURING",    "Tight Neodymium Punch",     BinaryData::bass_cab_4x10_aguilar_png,   BinaryData::bass_cab_4x10_aguilar_pngSize,   0.88f, 0.80f, 0.12f, 0.1864f, 0.76f, 0.6586f },
        { 2, true, false, false, "4x10 HI-PUNCH",   "Crisp Slap & Articulation", BinaryData::bass_cab_4x10_gk_png,        BinaryData::bass_cab_4x10_gk_pngSize,        0.88f, 0.80f, 0.12f, 0.1729f, 0.76f, 0.6696f },
        { 3, true, false, false, "6x10 HEAVY",      "Aggressive Rock Midrange",  BinaryData::bass_cab_6x10_heavy_png,     BinaryData::bass_cab_6x10_heavy_pngSize,     0.92f, 0.94f, 0.14f, 0.1457f, 0.72f, 0.7378f },
        { 4, true, false, false, "8x10 CLASSIC",    "Iconic Sealed Fridge Stack",BinaryData::bass_cab_8x10_classic_png,   BinaryData::bass_cab_8x10_classic_pngSize,   0.94f, 1.08f, 0.14f, 0.1216f, 0.72f, 0.7808f },
        { 5, true, false, false, "8x10 TANGERINE",  "Warm Stoner Low-Mid Growl", BinaryData::bass_cab_8x10_tangerine_png, BinaryData::bass_cab_8x10_tangerine_pngSize, 0.94f, 1.08f, 0.14f, 0.1036f, 0.72f, 0.8291f },
        { 6, true, true,  false, "CUSTOM WAV IR",   "Load External .WAV Impulse",BinaryData::bass_cab_8x10_classic_png,   BinaryData::bass_cab_8x10_classic_pngSize,   0.94f, 1.08f, 0.14f, 0.1216f, 0.72f, 0.7808f },
        { 7, true, false, true,  "CAB OFF",         "Direct Bass Preamp / DI",   nullptr, 0, 0.94f, 1.08f, 0.14f, 0.10f, 0.72f, 0.80f }
    };
    return specs;
}
} // namespace miamp_rig
