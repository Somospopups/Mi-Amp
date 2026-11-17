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
    bool useClassicOverlay = false;
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
    float widthScale  = 1.0f;
    float heightScale = 1.0f;
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
                { 0.3025f, 0.7400f, 0.0470f, MiAmpParams::gtrChPresence[2], "PRESENCE" },
                { 0.3885f, 0.7400f, 0.0470f, MiAmpParams::gtrChTreble[2], "TREBLE" },
                { 0.4730f, 0.7400f, 0.0470f, MiAmpParams::gtrChMid[2], "MIDDLE" },
                { 0.5570f, 0.7400f, 0.0470f, MiAmpParams::gtrChBass[2], "BASS" },
                { 0.6415f, 0.7400f, 0.0470f, MiAmpParams::gtrChMaster[2], "MASTER" },
                { 0.7250f, 0.7400f, 0.0470f, MiAmpParams::gtrChGain[2], "PRE-AMP" }
            },
            {
                { 0.1180f, 0.7470f, 0.0360f, AmpSwitchKind::RockerRed, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1840f, 0.7470f, 0.0360f, AmpSwitchKind::RockerRed, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false }
            }, {}
        },
        {
            1, false, "miplexi", "MI AMP PLEXI", "1968 Vintage Non-Master Super Lead",
            BinaryData::head_miplexi_png, BinaryData::head_miplexi_pngSize,
            BinaryData::knob_marshall_gold_png, BinaryData::knob_marshall_gold_pngSize, false,
            {
                { 0.3020f, 0.7260f, 0.0470f, MiAmpParams::gtrChPresence[1], "PRESENCE" },
                { 0.3765f, 0.7260f, 0.0470f, MiAmpParams::gtrChBass[1], "BASS" },
                { 0.4510f, 0.7260f, 0.0470f, MiAmpParams::gtrChMid[1], "MIDDLE" },
                { 0.5255f, 0.7260f, 0.0470f, MiAmpParams::gtrChTreble[1], "TREBLE" },
                { 0.6005f, 0.7260f, 0.0470f, MiAmpParams::gtrChGain[1], "VOLUME I" },
                { 0.6750f, 0.7260f, 0.0470f, MiAmpParams::gtrChGain[0], "VOLUME II" }
            },
            {
                { 0.1470f, 0.7250f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.2150f, 0.7250f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false }
            }, {}
        },
        {
            2, false, "mirecto", "MI AMP RECTO", "Dual Channel Modern High-Gain",
            BinaryData::head_mirecto_png, BinaryData::head_mirecto_pngSize,
            BinaryData::knob_mesa_chrome_png, BinaryData::knob_mesa_chrome_pngSize, false,
            {
                { 0.2190f, 0.6780f, 0.0410f, MiAmpParams::gtrChPresence[1], "CH2 PRES" },
                { 0.3010f, 0.6780f, 0.0410f, MiAmpParams::gtrChBass[1], "CH2 BASS" },
                { 0.3820f, 0.6780f, 0.0410f, MiAmpParams::gtrChMid[1], "CH2 MID" },
                { 0.4640f, 0.6780f, 0.0410f, MiAmpParams::gtrChTreble[1], "CH2 TREBLE" },
                { 0.5450f, 0.6780f, 0.0410f, MiAmpParams::gtrChGain[1], "CH2 GAIN" },
                { 0.6250f, 0.6780f, 0.0410f, MiAmpParams::gtrChMaster[1], "CH2 MASTER" },
                { 0.7060f, 0.6780f, 0.0410f, MiAmpParams::gtrOutput, "OUTPUT" },
                { 0.2190f, 0.8180f, 0.0410f, MiAmpParams::gtrChPresence[2], "CH3 PRES" },
                { 0.3010f, 0.8180f, 0.0410f, MiAmpParams::gtrChBass[2], "CH3 BASS" },
                { 0.3820f, 0.8180f, 0.0410f, MiAmpParams::gtrChMid[2], "CH3 MID" },
                { 0.4640f, 0.8180f, 0.0410f, MiAmpParams::gtrChTreble[2], "CH3 TREBLE" },
                { 0.5450f, 0.8180f, 0.0410f, MiAmpParams::gtrChGain[2], "CH3 GAIN" },
                { 0.6250f, 0.8180f, 0.0410f, MiAmpParams::gtrChMaster[2], "CH3 MASTER" },
                { 0.7060f, 0.8180f, 0.0410f, MiAmpParams::gtrOutput, "SOLO" }
            },
            {
                { 0.1580f, 0.7350f, 0.0250f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::ChannelCycle3, MiAmpParams::gtrChannel, "CHANNEL", false },
                { 0.7680f, 0.7250f, 0.0250f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.8260f, 0.7250f, 0.0250f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true }
            }, {}
        },
        {
            3, false, "mi6505", "MI AMP HG-120", "120W Tight Metal Lead & Crunch",
            BinaryData::head_mi6505_png, BinaryData::head_mi6505_pngSize,
            BinaryData::knob_peavey_black_png, BinaryData::knob_peavey_black_pngSize, false,
            {
                { 0.1230f, 0.7100f, 0.0355f, MiAmpParams::gtrChGain[0], "RHY PRE" },
                { 0.2260f, 0.7100f, 0.0355f, MiAmpParams::gtrChBass[0], "RHY LOW" },
                { 0.2780f, 0.7100f, 0.0355f, MiAmpParams::gtrChMid[0], "RHY MID" },
                { 0.3290f, 0.7100f, 0.0355f, MiAmpParams::gtrChTreble[0], "RHY HIGH" },
                { 0.3800f, 0.7100f, 0.0355f, MiAmpParams::gtrChMaster[0], "RHY POST" },
                { 0.4890f, 0.7100f, 0.0355f, MiAmpParams::gtrChGain[2], "LEAD PRE" },
                { 0.5420f, 0.7100f, 0.0355f, MiAmpParams::gtrChBass[2], "LEAD LOW" },
                { 0.5960f, 0.7100f, 0.0355f, MiAmpParams::gtrChMid[2], "LEAD MID" },
                { 0.6490f, 0.7100f, 0.0355f, MiAmpParams::gtrChTreble[2], "LEAD HIGH" },
                { 0.7020f, 0.7100f, 0.0355f, MiAmpParams::gtrChMaster[2], "LEAD POST" },
                { 0.7640f, 0.7100f, 0.0355f, MiAmpParams::gtrResonance, "RESONANCE" },
                { 0.8170f, 0.7100f, 0.0355f, MiAmpParams::gtrChPresence[2], "PRESENCE" }
            },
            {
                { 0.1620f, 0.7150f, 0.0200f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::gtrBright2, "BRIGHT", false },
                { 0.1900f, 0.7150f, 0.0200f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::gtrCrunchSw, "CRUNCH", false },
                { 0.4320f, 0.7150f, 0.0220f, AmpSwitchKind::PushSquare, AmpSwitchAction::ChannelToggle01, MiAmpParams::gtrChannel, "CHANNEL", false },
                { 0.8750f, 0.7150f, 0.0260f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.9180f, 0.7150f, 0.0260f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true }
            }, {}
        },
        {
            4, false, "miac30", "MI AMP AC-30", "30W Class-A Chime & Tremolo",
            BinaryData::head_miac30_png, BinaryData::head_miac30_pngSize,
            BinaryData::knob_chicken_cream_png, BinaryData::knob_chicken_cream_pngSize, false,
            {
                { 0.2040f, 0.2960f, 0.0375f, MiAmpParams::gtrChGain[0], "NORMAL VOL" },
                { 0.2600f, 0.2960f, 0.0375f, MiAmpParams::gtrChGain[1], "TOP BOOST" },
                { 0.3160f, 0.2960f, 0.0375f, MiAmpParams::gtrChTreble[1], "TREBLE" },
                { 0.3710f, 0.2960f, 0.0375f, MiAmpParams::gtrChBass[1], "BASS" },
                { 0.4270f, 0.2960f, 0.0375f, MiAmpParams::gtrAmpRev, "REVERB TONE" },
                { 0.4830f, 0.2960f, 0.0375f, MiAmpParams::gtrChPresence[1], "REVERB LVL" },
                { 0.5390f, 0.2960f, 0.0375f, MiAmpParams::gtrTremSpd, "TREM SPEED" },
                { 0.5950f, 0.2960f, 0.0375f, MiAmpParams::gtrTremDep, "TREM DEPTH" },
                { 0.6510f, 0.2960f, 0.0375f, MiAmpParams::gtrChPresence[0], "TONE CUT" },
                { 0.7070f, 0.2960f, 0.0375f, MiAmpParams::gtrOutput, "MASTER VOL" }
            },
            {
                { 0.7610f, 0.3000f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrModeBright, "BRILLIANCE", false },
                { 0.7930f, 0.3000f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrBright2, "EQ MODE", false },
                { 0.8420f, 0.2980f, 0.0240f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.8880f, 0.2980f, 0.0240f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true }
            }, {}
        },
        {
            5, false, "mirocker", "MI AMP ROCKER", "Thick British Crunch & Spring Reverb",
            BinaryData::head_mirocker_png, BinaryData::head_mirocker_pngSize,
            BinaryData::knob_orange_black_png, BinaryData::knob_orange_black_pngSize, false,
            {
                { 0.2540f, 0.6780f, 0.0460f, MiAmpParams::gtrAtten, "ATTENUATOR" },
                { 0.3200f, 0.6780f, 0.0460f, MiAmpParams::gtrAmpRev, "REVERB" },
                { 0.3840f, 0.6780f, 0.0460f, MiAmpParams::gtrChMaster[2], "DIRTY VOL" },
                { 0.4490f, 0.6780f, 0.0460f, MiAmpParams::gtrChTreble[2], "TREBLE" },
                { 0.5130f, 0.6780f, 0.0460f, MiAmpParams::gtrChMid[2], "MID" },
                { 0.5770f, 0.6780f, 0.0460f, MiAmpParams::gtrChBass[2], "BASS" },
                { 0.6410f, 0.6780f, 0.0460f, MiAmpParams::gtrChGain[2], "GAIN" },
                { 0.7050f, 0.6780f, 0.0460f, MiAmpParams::gtrCleanTreb, "CLEAN TREB" },
                { 0.7690f, 0.6780f, 0.0460f, MiAmpParams::gtrCleanBass, "CLEAN BASS" },
                { 0.8310f, 0.6780f, 0.0460f, MiAmpParams::gtrChGain[0], "CLEAN VOL" }
            },
            {
                { 0.1140f, 0.6540f, 0.0210f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1570f, 0.6540f, 0.0210f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrHalfPwr, "FULL/HALF", false },
                { 0.1990f, 0.6540f, 0.0210f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::ChannelToggle01, MiAmpParams::gtrChannel, "CLEAN/DIRTY", false }
            }, {}
        },
        {
            6, false, "mibe100", "MI AMP BE-100", "Modded Boutique British Hot-Rod",
            BinaryData::head_mibe100_png, BinaryData::head_mibe100_pngSize,
            BinaryData::knob_be_black_png, BinaryData::knob_be_black_pngSize, false,
            {
                { 0.2800f, 0.7130f, 0.0450f, MiAmpParams::gtrChPresence[2], "PRESENCE" },
                { 0.3570f, 0.7130f, 0.0450f, MiAmpParams::gtrChBass[2], "BASS" },
                { 0.4340f, 0.7130f, 0.0450f, MiAmpParams::gtrChMid[2], "MIDDLE" },
                { 0.5110f, 0.7130f, 0.0450f, MiAmpParams::gtrChTreble[2], "TREBLE" },
                { 0.5880f, 0.7130f, 0.0450f, MiAmpParams::gtrChMaster[2], "MASTER" },
                { 0.6650f, 0.7130f, 0.0450f, MiAmpParams::gtrChGain[2], "GAIN" },
                { 0.8385f, 0.7130f, 0.0450f, MiAmpParams::gtrChGain[0], "CLEAN VOL" }
            },
            {
                { 0.0820f, 0.7050f, 0.0320f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1440f, 0.7050f, 0.0320f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false },
                { 0.7250f, 0.7200f, 0.0185f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::VoiceCycle3, MiAmpParams::gtrVoiceSw, "VOICE", false },
                { 0.7750f, 0.7200f, 0.0185f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrModeBright, "BRIGHT", false }
            }, {}
        },
        {
            7, false, "mitwin", "MI AMP TWIN", "65 Blackface Clean Headroom & Vibrato",
            BinaryData::head_mitwin_png, BinaryData::head_mitwin_pngSize,
            BinaryData::knob_fender_skirt_png, BinaryData::knob_fender_skirt_pngSize, false,
            {
                { 0.1750f, 0.7280f, 0.0365f, MiAmpParams::gtrChGain[0], "NORM VOL" },
                { 0.2280f, 0.7280f, 0.0365f, MiAmpParams::gtrChTreble[0], "NORM TREB" },
                { 0.2820f, 0.7280f, 0.0365f, MiAmpParams::gtrChMid[0], "NORM MID" },
                { 0.3350f, 0.7280f, 0.0365f, MiAmpParams::gtrChBass[0], "NORM BASS" },
                { 0.5060f, 0.7280f, 0.0365f, MiAmpParams::gtrChGain[1], "VIB VOL" },
                { 0.5560f, 0.7280f, 0.0365f, MiAmpParams::gtrChTreble[1], "VIB TREB" },
                { 0.6060f, 0.7280f, 0.0365f, MiAmpParams::gtrChMid[1], "VIB MID" },
                { 0.6560f, 0.7280f, 0.0365f, MiAmpParams::gtrChBass[1], "VIB BASS" },
                { 0.7060f, 0.7280f, 0.0365f, MiAmpParams::gtrAmpRev, "REVERB" },
                { 0.7560f, 0.7280f, 0.0365f, MiAmpParams::gtrTremSpd, "SPEED" },
                { 0.8065f, 0.7280f, 0.0365f, MiAmpParams::gtrTremDep, "INTENSITY" }
            },
            {
                { 0.1380f, 0.7220f, 0.0190f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrModeBright, "BRIGHT N", false },
                { 0.4660f, 0.7220f, 0.0190f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrBright2, "BRIGHT V", false }
            }, {}
        },
        {
            8, false, "zenbox", "MI AMP ZENBOX", "Signature 3-Channel California Mint",
            BinaryData::head_zenbox_mint_png, BinaryData::head_zenbox_mint_pngSize,
            BinaryData::knob_zenbox_mint_png, BinaryData::knob_zenbox_mint_pngSize, false,
            {
                { 0.2640f, 0.6640f, 0.0435f, MiAmpParams::gtrChGain[0], "CH1 GAIN" },
                { 0.3140f, 0.6640f, 0.0435f, MiAmpParams::gtrChTreble[0], "CH1 TREBLE" },
                { 0.3640f, 0.6640f, 0.0435f, MiAmpParams::gtrChMid[0], "CH1 MID" },
                { 0.2640f, 0.7920f, 0.0435f, MiAmpParams::gtrChBass[0], "CH1 BASS" },
                { 0.3140f, 0.7920f, 0.0435f, MiAmpParams::gtrChPresence[0], "CH1 PRESENCE" },
                { 0.3640f, 0.7920f, 0.0435f, MiAmpParams::gtrChMaster[0], "CH1 MASTER" },
                { 0.4490f, 0.6640f, 0.0435f, MiAmpParams::gtrChGain[1], "CH2 GAIN" },
                { 0.4990f, 0.6640f, 0.0435f, MiAmpParams::gtrChTreble[1], "CH2 TREBLE" },
                { 0.5490f, 0.6640f, 0.0435f, MiAmpParams::gtrChMid[1], "CH2 MID" },
                { 0.4490f, 0.7920f, 0.0435f, MiAmpParams::gtrChBass[1], "CH2 BASS" },
                { 0.4990f, 0.7920f, 0.0435f, MiAmpParams::gtrChPresence[1], "CH2 PRESENCE" },
                { 0.5490f, 0.7920f, 0.0435f, MiAmpParams::gtrChMaster[1], "CH2 MASTER" },
                { 0.6340f, 0.6640f, 0.0435f, MiAmpParams::gtrChGain[2], "CH3 GAIN" },
                { 0.6840f, 0.6640f, 0.0435f, MiAmpParams::gtrChTreble[2], "CH3 TREBLE" },
                { 0.7340f, 0.6640f, 0.0435f, MiAmpParams::gtrChMid[2], "CH3 MID" },
                { 0.6340f, 0.7920f, 0.0435f, MiAmpParams::gtrChBass[2], "CH3 BASS" },
                { 0.6840f, 0.7920f, 0.0435f, MiAmpParams::gtrChPresence[2], "CH3 PRESENCE" },
                { 0.7340f, 0.7920f, 0.0435f, MiAmpParams::gtrChMaster[2], "CH3 MASTER" },
                { 0.8060f, 0.7280f, 0.0590f, MiAmpParams::gtrOutput, "OUTPUT" }
            },
            {
                { 0.1110f, 0.7250f, 0.0340f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::gtrPower, "POWER", true },
                { 0.1940f, 0.7250f, 0.0240f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::gtrStandby, "STANDBY", false }
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
                { 0.2010f, 0.6880f, 0.0500f, MiAmpParams::ampGain, "GAIN" },
                { 0.3820f, 0.6880f, 0.0500f, MiAmpParams::ampBass, "BASS" },
                { 0.4740f, 0.6880f, 0.0500f, MiAmpParams::ampMid, "MIDRANGE" },
                { 0.5660f, 0.6880f, 0.0500f, MiAmpParams::ampFreq, "FREQUENCY" },
                { 0.6580f, 0.6880f, 0.0500f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.7500f, 0.6880f, 0.0500f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.2680f, 0.6900f, 0.0280f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "ULTRA HI", false },
                { 0.3175f, 0.6900f, 0.0280f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::bassUltraLo, "ULTRA LO", false },
                { 0.8180f, 0.6900f, 0.0300f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STANDBY", false },
                { 0.8690f, 0.6900f, 0.0300f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            1, true, "pro450", "MI AMP PRO-450", "Touring Rack Hybrid + 9-Band GEQ",
            BinaryData::bass_head_svt3pro_png, BinaryData::bass_head_svt3pro_pngSize,
            BinaryData::knob_svt_fluted_png, BinaryData::knob_svt_fluted_pngSize, false,
            {
                { 0.2440f, 0.5640f, 0.0345f, MiAmpParams::ampGain, "GAIN" },
                { 0.3170f, 0.5640f, 0.0345f, MiAmpParams::ampBass, "BASS" },
                { 0.3600f, 0.5640f, 0.0345f, MiAmpParams::ampMid, "MID" },
                { 0.4020f, 0.5640f, 0.0345f, MiAmpParams::ampFreq, "FREQ" },
                { 0.4450f, 0.5640f, 0.0345f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4880f, 0.5640f, 0.0345f, MiAmpParams::bassTubeGain, "TUBE GAIN" },
                { 0.5300f, 0.5640f, 0.0345f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.1630f, 0.5560f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassMuteSw, "MUTE", false },
                { 0.1920f, 0.5560f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "-15dB PAD", false },
                { 0.2800f, 0.5560f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "ULTRA HI/LO", false },
                { 0.5620f, 0.5120f, 0.0160f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BRIGHT", false },
                { 0.5620f, 0.5650f, 0.0190f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassHeadEqOn, "EQ ON", false },
                { 0.8590f, 0.5180f, 0.0280f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {
                { 0.5940f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[0], "31Hz" },
                { 0.6165f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[1], "63Hz" },
                { 0.6390f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[2], "125Hz" },
                { 0.6615f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[3], "250Hz" },
                { 0.6840f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[4], "500Hz" },
                { 0.7065f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[5], "1KHz" },
                { 0.7290f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[6], "2KHz" },
                { 0.7515f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[7], "4KHz" },
                { 0.7740f, 0.4720f, 0.6160f, MiAmpParams::bassHeadEq[8], "8KHz" },
                { 0.8100f, 0.4680f, 0.6280f, MiAmpParams::bassHeadEq[9], "LEVEL" }
            }
        },
        {
            2, true, "bassman", "MI AMP BASSMAN", "Silverface Vintage Dual-Channel Tube",
            BinaryData::bass_head_bassman_png, BinaryData::bass_head_bassman_pngSize,
            BinaryData::knob_fender_skirt_png, BinaryData::knob_fender_skirt_pngSize, false,
            {
                { 0.2920f, 0.7020f, 0.0440f, MiAmpParams::ampGain, "BASS VOL" },
                { 0.3540f, 0.7020f, 0.0440f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4150f, 0.7020f, 0.0440f, MiAmpParams::ampBass, "BASS" },
                { 0.6730f, 0.7020f, 0.0440f, MiAmpParams::bassMaster, "NORM VOL" },
                { 0.7350f, 0.7020f, 0.0440f, MiAmpParams::ampMid, "NORM TREB" },
                { 0.7970f, 0.7020f, 0.0440f, MiAmpParams::ampFreq, "NORM BASS" }
            },
            {
                { 0.1860f, 0.6960f, 0.0220f, AmpSwitchKind::SlideBlack, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "DEEP", false },
                { 0.6010f, 0.6960f, 0.0220f, AmpSwitchKind::SlideBlack, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BRIGHT", false }
            }, {}
        },
        {
            3, true, "rb800", "MI AMP RB-800", "80s Bi-Amp Punch Rack Head",
            BinaryData::bass_head_gk800_png, BinaryData::bass_head_gk800_pngSize,
            BinaryData::knob_gk_black_png, BinaryData::knob_gk_black_pngSize, false,
            {
                { 0.2115f, 0.6000f, 0.0380f, MiAmpParams::ampGain, "VOLUME" },
                { 0.3880f, 0.6000f, 0.0380f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4405f, 0.6000f, 0.0380f, MiAmpParams::ampFreq, "HI-MID" },
                { 0.4930f, 0.6000f, 0.0380f, MiAmpParams::ampMid, "LO-MID" },
                { 0.5455f, 0.6000f, 0.0380f, MiAmpParams::ampBass, "BASS" },
                { 0.6400f, 0.6000f, 0.0380f, MiAmpParams::bassDriveCtrl, "BOOST" },
                { 0.6980f, 0.6000f, 0.0380f, MiAmpParams::ampFreq, "X-OVER" },
                { 0.7600f, 0.6000f, 0.0380f, MiAmpParams::bassBiAmpLo, "BI-AMP" },
                { 0.8180f, 0.6000f, 0.0380f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.1600f, 0.6000f, 0.0200f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "-10dB PAD", false },
                { 0.2640f, 0.6000f, 0.0190f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassUltraLo, "LO CUT", false },
                { 0.2980f, 0.6000f, 0.0190f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "MID CONTOUR", false },
                { 0.3320f, 0.6000f, 0.0190f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "HI BOOST", false },
                { 0.5920f, 0.6000f, 0.0190f, AmpSwitchKind::PushSquare, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BOOST SW", false },
                { 0.8700f, 0.6000f, 0.0220f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            4, true, "m900", "MI AMP M-900", "Modern Dual-Engine Microtubes + 6-Band EQ",
            BinaryData::bass_head_dg900_png, BinaryData::bass_head_dg900_pngSize,
            BinaryData::knob_dg_alum_png, BinaryData::knob_dg_alum_pngSize, false,
            {
                { 0.2850f, 0.6080f, 0.0440f, MiAmpParams::bassCompCtrl, "COMP" },
                { 0.3450f, 0.6080f, 0.0440f, MiAmpParams::bassDriveCtrl, "DRIVE" },
                { 0.4050f, 0.6080f, 0.0440f, MiAmpParams::ampTreble, "TONE" },
                { 0.4640f, 0.6080f, 0.0440f, MiAmpParams::ampBass, "LEVEL" },
                { 0.5240f, 0.6080f, 0.0440f, MiAmpParams::bassBlendCtrl, "BLEND" },
                { 0.5840f, 0.6080f, 0.0440f, MiAmpParams::ampGain, "GAIN" },
                { 0.6440f, 0.6080f, 0.0440f, MiAmpParams::bassMaster, "MASTER" }
            },
            {
                { 0.1410f, 0.5780f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "PASSIVE", false },
                { 0.1840f, 0.5780f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassMuteSw, "MUTE", false },
                { 0.3570f, 0.7100f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "ENGINE", false },
                { 0.5030f, 0.7100f, 0.0200f, AmpSwitchKind::PushRound, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "VOICING", false }
            }, {
                { 0.7050f, 0.4950f, 0.7250f, MiAmpParams::bassHeadEq[0], "LOW" },
                { 0.7310f, 0.4950f, 0.7250f, MiAmpParams::bassHeadEq[2], "250Hz" },
                { 0.7570f, 0.4950f, 0.7250f, MiAmpParams::bassHeadEq[4], "500Hz" },
                { 0.7830f, 0.4950f, 0.7250f, MiAmpParams::bassHeadEq[5], "1.5kHz" },
                { 0.8080f, 0.4950f, 0.7250f, MiAmpParams::bassHeadEq[6], "3kHz" },
                { 0.8340f, 0.4950f, 0.7250f, MiAmpParams::bassHeadEq[8], "HIGH" }
            }
        },
        {
            5, true, "b15", "MI AMP B-15", "Motown Warm Flip-Top Studio Tube",
            BinaryData::bass_head_b15_png, BinaryData::bass_head_b15_pngSize,
            BinaryData::knob_svt_fluted_png, BinaryData::knob_svt_fluted_pngSize, false,
            {
                { 0.2035f, 0.7160f, 0.0490f, MiAmpParams::ampGain, "CH1 VOL" },
                { 0.2855f, 0.7160f, 0.0490f, MiAmpParams::ampTreble, "CH1 TREB" },
                { 0.3675f, 0.7160f, 0.0490f, MiAmpParams::ampBass, "CH1 BASS" },
                { 0.5155f, 0.7160f, 0.0490f, MiAmpParams::bassMaster, "CH2 VOL" },
                { 0.5975f, 0.7160f, 0.0490f, MiAmpParams::ampMid, "CH2 TREB" },
                { 0.6785f, 0.7160f, 0.0490f, MiAmpParams::ampFreq, "CH2 BASS" }
            },
            {
                { 0.7450f, 0.7150f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STANDBY", false },
                { 0.7930f, 0.7150f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassUltraLo, "GROUND", false },
                { 0.8400f, 0.7150f, 0.0300f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            6, true, "or200", "MI AMP OR-200", "200W Stoner / Rock All-Tube Growl",
            BinaryData::bass_head_orange200_png, BinaryData::bass_head_orange200_pngSize,
            BinaryData::knob_orange_black_png, BinaryData::knob_orange_black_pngSize, false,
            {
                { 0.3690f, 0.6650f, 0.0630f, MiAmpParams::bassMaster, "MASTER" },
                { 0.4550f, 0.6650f, 0.0630f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.5420f, 0.6650f, 0.0630f, MiAmpParams::ampMid, "MIDDLE" },
                { 0.6280f, 0.6650f, 0.0630f, MiAmpParams::ampBass, "BASS" },
                { 0.7140f, 0.6650f, 0.0630f, MiAmpParams::ampGain, "GAIN" }
            },
            {
                { 0.1300f, 0.6650f, 0.0340f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true },
                { 0.2140f, 0.6650f, 0.0340f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STBY", false }
            }, {}
        },
        {
            7, true, "db750", "MI AMP DB-750", "750W Hybrid Touring Rack Punch",
            BinaryData::bass_head_aguilar_png, BinaryData::bass_head_aguilar_pngSize,
            BinaryData::knob_aguilar_silver_png, BinaryData::knob_aguilar_silver_pngSize, false,
            {
                { 0.2280f, 0.6200f, 0.0520f, MiAmpParams::ampGain, "GAIN" },
                { 0.3560f, 0.6200f, 0.0520f, MiAmpParams::ampTreble, "TREBLE" },
                { 0.4310f, 0.6200f, 0.0520f, MiAmpParams::ampMid, "MIDRANGE" },
                { 0.5060f, 0.6200f, 0.0520f, MiAmpParams::ampBass, "BASS" },
                { 0.6220f, 0.6200f, 0.0520f, MiAmpParams::bassMaster, "MASTER" },
                { 0.6970f, 0.6200f, 0.0520f, MiAmpParams::bassDriveCtrl, "FX SEND" },
                { 0.7720f, 0.6200f, 0.0520f, MiAmpParams::ampFreq, "FX RETURN" }
            },
            {
                { 0.1720f, 0.6200f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassPad, "-6dB PAD", false },
                { 0.2920f, 0.6200f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassBrightSw, "BRIGHT", false },
                { 0.5640f, 0.6200f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassDeepSw, "DEEP", false },
                { 0.8310f, 0.6200f, 0.0220f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::bassMuteSw, "MUTE", false },
                { 0.8710f, 0.6200f, 0.0250f, AmpSwitchKind::RockerBlack, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true }
            }, {}
        },
        {
            8, true, "classic", "MI AMP CLASSIC", "Signature California Mint Tube Bass",
            BinaryData::bass_head_mint_png, BinaryData::bass_head_mint_pngSize,
            BinaryData::knob_bass_mint_png, BinaryData::knob_bass_mint_pngSize, false,
            {
                { 0.2750f, 0.6420f, 0.0490f, MiAmpParams::ampGain, "GAIN" },
                { 0.3700f, 0.6420f, 0.0490f, MiAmpParams::ampBass, "BASS" },
                { 0.4650f, 0.6420f, 0.0490f, MiAmpParams::ampMid, "MID" },
                { 0.5600f, 0.6420f, 0.0490f, MiAmpParams::ampFreq, "FREQ" },
                { 0.6550f, 0.6420f, 0.0490f, MiAmpParams::ampTreble, "TREBLE" }
            },
            {
                { 0.1140f, 0.6420f, 0.0280f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampPower, "POWER", true },
                { 0.1930f, 0.6420f, 0.0240f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampUltraHi, "ULTRA HI/LO", false },
                { 0.7680f, 0.6420f, 0.0280f, AmpSwitchKind::ToggleSilver, AmpSwitchAction::BoolParam, MiAmpParams::ampStandby, "STANDBY", false }
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
        { 7, false, false, false, "4x12 MI ZENBOX", "Mint Tolex Original Stack",   BinaryData::cab_4x12_zenbox_png,    BinaryData::cab_4x12_zenbox_pngSize,    1.00f, 1.00f, 0.09f, 0.1300f, 0.82f, 0.7200f },
        { 8, false, true,  false, "CUSTOM WAV IR",  "Load External .WAV Impulse",  BinaryData::cab_4x12_zenbox_png,    BinaryData::cab_4x12_zenbox_pngSize,    1.00f, 1.00f, 0.09f, 0.1300f, 0.82f, 0.7200f },
        { 9, false, false, true,  "CAB OFF",        "Direct Amp Output (No Cab)",  nullptr, 0, 1.00f, 1.00f, 0.09f, 0.13f, 0.82f, 0.72f }
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
        { 6, true, false, false, "8x10 MI CLASSIC", "Mint Tolex Original Fridge",BinaryData::bass_cab_8x10_mint_png,      BinaryData::bass_cab_8x10_mint_pngSize,      0.94f, 1.08f, 0.095f, 0.0900f, 0.81f, 0.8300f },
        { 7, true, true,  false, "CUSTOM WAV IR",   "Load External .WAV Impulse",BinaryData::bass_cab_8x10_mint_png,      BinaryData::bass_cab_8x10_mint_pngSize,      0.94f, 1.08f, 0.095f, 0.0900f, 0.81f, 0.8300f },
        { 8, true, false, true,  "CAB OFF",         "Direct Bass Preamp / DI",   nullptr, 0, 0.94f, 1.08f, 0.095f, 0.09f, 0.81f, 0.83f }
    };
    return specs;
}
} // namespace miamp_rig
