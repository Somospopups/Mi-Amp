#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

MiAmpAudioProcessor::MiAmpAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MI_AMP_STATE", createParameterLayout()),
      factoryPresets (getFactoryPresets()),
      guitarFactoryPresets (getGuitarFactoryPresets())
{
    loadUserPresetsFromDisk();
}

MiAmpAudioProcessor::~MiAmpAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MiAmpAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto addBool = [&p](const char* id, const juce::String& name, bool def)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def));
    };
    auto addPct = [&p](const char* id, const juce::String& name, float def, float minV = 0.0f, float maxV = 100.0f, float step = 0.1f)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (minV, maxV, step), def));
    };
    auto addInt = [&p](const char* id, const juce::String& name, int minV, int maxV, int def)
    {
        p.push_back (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { id, 1 }, name, minV, maxV, def));
    };

    // Instrument selector (0 = Guitarra, 1 = Bajo XV)
    addInt (MiAmpParams::instrument, "Instrument", 0, 1, 0);

    // Bass Head
    addBool (MiAmpParams::ampPower,       "Bass Power",       true);
    addBool (MiAmpParams::ampStandby,     "Bass Standby",     false);
    addBool (MiAmpParams::ampUltraHi,     "Bass Ultra-Hi",    false);
    addBool (MiAmpParams::ampCabOn,       "Bass Cab 8x10",    true);
    addPct  (MiAmpParams::ampGain,        "Bass Gain",        50.0f);
    addPct  (MiAmpParams::ampBass,        "Bass Low",         50.0f);
    addPct  (MiAmpParams::ampMid,         "Bass Mid",         50.0f);
    addPct  (MiAmpParams::ampFreq,        "Bass Mid Freq",    50.0f);
    addPct  (MiAmpParams::ampTreble,      "Bass Treble",      50.0f);
    addBool (MiAmpParams::bassHeadBypass, "Bass Head Bypass", false);

    // Shared Pitch Shifter & Tuner
    addBool (MiAmpParams::pitchOn,    "Drop Tune On",  false);
    addInt  (MiAmpParams::pitchSemi,  "Semitones",    -12, 12, 0);
    addPct  (MiAmpParams::pitchCents, "Cents",         0.0f, -50.0f, 50.0f, 1.0f);
    addPct  (MiAmpParams::pitchMix,   "Pitch Mix",     100.0f);
    addBool (MiAmpParams::tunerMuted, "Tuner Mute",    false);

    // Bass Pedalboard Slots (10 slots, Default: XV Preamp at index 1)
    addInt (MiAmpParams::slot[0], "Bass Slot 1", -1, 13, 1);
    for (int s = 1; s < 10; ++s)
        addInt (MiAmpParams::slot[s], "Bass Slot " + juce::String (s + 1), -1, 13, -1);

    // Bass Pedals
    addBool (MiAmpParams::compOn,      "Comp On",       true);
    addPct  (MiAmpParams::compSustain, "Comp Ratio",    50.0f);
    addPct  (MiAmpParams::compAttack,  "Comp Attack",   50.0f);
    addPct  (MiAmpParams::compLevel,   "Comp Output",   50.0f);
    addPct  (MiAmpParams::compRelease, "Comp Release",  45.0f);
    addPct  (MiAmpParams::compInput,   "Comp Input",    55.0f);

    addBool (MiAmpParams::xvOn,        "XV Bass On",    true);
    addPct  (MiAmpParams::xvLevel,     "XV Level",      56.0f);
    addPct  (MiAmpParams::xvDrive,     "XV Drive",      40.8f);
    addPct  (MiAmpParams::xvLow,       "XV Low",        65.4f);
    addPct  (MiAmpParams::xvHigh,      "XV High",       53.5f);
    addPct  (MiAmpParams::xvBlend,     "XV Blend",      57.6f);
    addPct  (MiAmpParams::xvMid,       "XV Mid",        25.8f);
    addPct  (MiAmpParams::xvPresence,  "XV Presence",   80.4f);
    addInt  (MiAmpParams::xvCab,       "XV Cab",        0, 2, 1);

    addBool (MiAmpParams::chorusOn,    "Chorus On",     true);
    addPct  (MiAmpParams::chorusRate,  "Chorus Rate",   50.0f);
    addPct  (MiAmpParams::chorusDepth, "Chorus Depth",  50.0f);
    addPct  (MiAmpParams::chorusMix,   "Chorus Mix",    50.0f);
    addPct  (MiAmpParams::chorusLoCut, "Chorus Lo-Cut", 40.0f);
    addBool (MiAmpParams::chorusXOver, "Chorus X-Over", false);

    addBool (MiAmpParams::phaserOn,       "Phaser On",       true);
    addPct  (MiAmpParams::phaserRate,     "Phaser Rate",     50.0f);
    addPct  (MiAmpParams::phaserDepth,    "Phaser Depth",    50.0f);
    addPct  (MiAmpParams::phaserFeedback, "Phaser Feedback", 50.0f);
    addPct  (MiAmpParams::phaserStage,    "Phaser Stage",    45.0f);

    addBool (MiAmpParams::reverbOn,    "Reverb On",     true);
    addPct  (MiAmpParams::reverbDecay, "Reverb Decay",  50.0f);
    addPct  (MiAmpParams::reverbTone,  "Reverb Tone",   50.0f);
    addPct  (MiAmpParams::reverbMix,   "Reverb Mix",    50.0f);
    addPct  (MiAmpParams::reverbMode,  "Reverb Mode",   35.0f);

    // New Bass Boutique Pedals
    addBool (MiAmpParams::goldOn,    "Gold Drive On",    true);
    addPct  (MiAmpParams::goldLevel, "Gold Drive Level", 56.0f);
    addPct  (MiAmpParams::goldEq,    "Gold Drive EQ",    54.0f);
    addPct  (MiAmpParams::goldBlend, "Gold Drive Blend", 58.0f);
    addPct  (MiAmpParams::goldGain,  "Gold Drive Gain",  52.0f);

    addBool (MiAmpParams::bfOn,     "Bass Food On",     true);
    addPct  (MiAmpParams::bfVol,    "Bass Food Vol",    58.0f);
    addPct  (MiAmpParams::bfTreble, "Bass Food Treble", 54.0f);
    addPct  (MiAmpParams::bfDrive,  "Bass Food Drive",  42.0f);
    addPct  (MiAmpParams::bfBlend,  "Bass Food Blend",  55.0f);
    addBool (MiAmpParams::bfPad,    "Bass Food Pad",    false);

    addBool (MiAmpParams::bddiOn,       "BDDI On",       true);
    addPct  (MiAmpParams::bddiLevel,    "BDDI Level",    58.0f);
    addPct  (MiAmpParams::bddiBlend,    "BDDI Blend",    62.0f);
    addPct  (MiAmpParams::bddiTreble,   "BDDI Treble",   56.0f);
    addPct  (MiAmpParams::bddiPresence, "BDDI Presence", 65.0f);
    addPct  (MiAmpParams::bddiDrive,    "BDDI Drive",    48.0f);
    addPct  (MiAmpParams::bddiBass,     "BDDI Bass",     60.0f);
    addBool (MiAmpParams::bddiXlrPad,   "BDDI XLR Pad",  false);
    addBool (MiAmpParams::bddiInBoost,  "BDDI In +10dB", false);
    addBool (MiAmpParams::bddiOutBoost, "BDDI Out +10dB",false);

    addBool (MiAmpParams::oliveOn,      "Olive Fuzz On",      true);
    addPct  (MiAmpParams::oliveVol,     "Olive Fuzz Vol",     55.0f);
    addPct  (MiAmpParams::oliveTone,    "Olive Fuzz Tone",    52.0f);
    addPct  (MiAmpParams::oliveSustain, "Olive Fuzz Sustain", 62.0f);
    addInt  (MiAmpParams::oliveMode,    "Olive Fuzz Mode",    0, 2, 1);

    addBool (MiAmpParams::bassFlangeOn,     "Bass Flange On",     true);
    addPct  (MiAmpParams::bassFlangeManual, "Bass Flange Manual", 48.0f);
    addPct  (MiAmpParams::bassFlangeRes,    "Bass Flange Res",    45.0f);
    addPct  (MiAmpParams::bassFlangeDepth,  "Bass Flange Depth",  55.0f);
    addPct  (MiAmpParams::bassFlangeRate,   "Bass Flange Rate",   36.0f);
    addInt  (MiAmpParams::bassFlangeMode,   "Bass Flange Mode",   0, 2, 0);

    addBool (MiAmpParams::bassDelayOn,     "Bass Delay On",     true);
    addPct  (MiAmpParams::bassDelayTime,   "Bass Delay Time",   42.0f);
    addPct  (MiAmpParams::bassDelayFb,     "Bass Delay Fb",     36.0f);
    addPct  (MiAmpParams::bassDelayMix,    "Bass Delay Mix",    35.0f);
    addPct  (MiAmpParams::bassDelayMode,   "Bass Delay Mode",   30.0f);
    addBool (MiAmpParams::bassDelaySubdiv, "Bass Delay Subdiv", false);

    addBool (MiAmpParams::bassGateOn,     "Bass Gate On",     true);
    addPct  (MiAmpParams::bassGateThresh, "Bass Gate Thresh", 32.0f);
    addPct  (MiAmpParams::bassGateDecay,  "Bass Gate Decay",  40.0f);
    addPct  (MiAmpParams::bassGateMode,   "Bass Gate Mode",   50.0f);

    addBool (MiAmpParams::bassOctOn,  "Bass Oct On",  true);
    addPct  (MiAmpParams::bassOctDry, "Bass Oct Dry", 85.0f);
    addPct  (MiAmpParams::bassOctSub, "Bass Oct Sub", 58.0f);
    addPct  (MiAmpParams::bassOctUp,  "Bass Oct Up",  28.0f);

    addBool (MiAmpParams::bassEqOn, "Bass EQ On", true);
    for (int b = 0; b < 7; ++b)
        addPct (MiAmpParams::bassEqBand[b], "Bass EQ Band " + juce::String (b + 1), 50.0f);

    // Guitar Head (MI Amp Multi-Head Edition)
    addPct  (MiAmpParams::gtrInput,      "Guitar Input",       56.0f);
    addPct  (MiAmpParams::gtrGate,       "Guitar Gate",        32.0f);
    addBool (MiAmpParams::gtrOs4x,       "Guitar 4x OS",       true);
    addInt  (MiAmpParams::gtrCab,        "Guitar Cab",         0, 8, 1);
    addBool (MiAmpParams::gtrPower,      "Guitar Power",       true);
    addBool (MiAmpParams::gtrStandby,    "Guitar Standby",     false);
    addBool (MiAmpParams::gtrModeBright, "Guitar Bright Cap",  true);
    addInt  (MiAmpParams::gtrChannel,    "Guitar Channel",     0, 2, 2);
    addPct  (MiAmpParams::gtrOutput,     "Guitar Output",      58.0f);
    addBool (MiAmpParams::gtrHeadBypass, "Guitar Head Bypass", false);

    const float defGain[3]   = { 28.0f, 58.0f, 68.0f };
    const float defBass[3]   = { 44.0f, 54.0f, 56.0f };
    const float defMid[3]    = { 54.0f, 60.0f, 60.0f };
    const float defTreb[3]   = { 68.0f, 58.0f, 64.0f };
    const float defPres[3]   = { 62.0f, 56.0f, 66.0f };
    const float defMast[3]   = { 64.0f, 58.0f, 60.0f };

    for (int c = 0; c < 3; ++c)
    {
        const juce::String pfx = "CH" + juce::String (c + 1) + " ";
        addPct (MiAmpParams::gtrChGain[c],     pfx + "Gain",     defGain[c]);
        addPct (MiAmpParams::gtrChBass[c],     pfx + "Bass",     defBass[c]);
        addPct (MiAmpParams::gtrChMid[c],      pfx + "Mid",      defMid[c]);
        addPct (MiAmpParams::gtrChTreble[c],   pfx + "Treble",   defTreb[c]);
        addPct (MiAmpParams::gtrChPresence[c], pfx + "Presence", defPres[c]);
        addPct (MiAmpParams::gtrChMaster[c],   pfx + "Master",   defMast[c]);
    }

    // Guitar Pedalboard Slots (10 slots, Default: Overdrive at index 1)
    addInt (MiAmpParams::gtrSlot[0], "Guitar Slot 1", -1, 12, 1);
    for (int s = 1; s < 10; ++s)
        addInt (MiAmpParams::gtrSlot[s], "Guitar Slot " + juce::String (s + 1), -1, 12, -1);

    // Guitar Pedals
    addBool (MiAmpParams::gtrCompOn,   "Gtr Comp On",   true);
    addPct  (MiAmpParams::gtrCompSus,  "Gtr Comp Sus",  50.0f);
    addPct  (MiAmpParams::gtrCompAtt,  "Gtr Comp Att",  45.0f);
    addPct  (MiAmpParams::gtrCompLvl,  "Gtr Comp Lvl",  52.0f);
    addPct  (MiAmpParams::gtrCompTone, "Gtr Comp Tone", 52.0f);

    addBool (MiAmpParams::gtrDriveOn,   "Gtr Drive On",   true);
    addPct  (MiAmpParams::gtrDriveGain, "Gtr Drive Gain", 18.0f);
    addPct  (MiAmpParams::gtrDriveTone, "Gtr Drive Tone", 66.0f);
    addPct  (MiAmpParams::gtrDriveLvl,  "Gtr Drive Lvl",  64.0f);
    addBool (MiAmpParams::gtrDriveClip, "Gtr Drive Clip", false);

    addBool (MiAmpParams::gtrFuzzOn,   "Gtr Fuzz On",   true);
    addPct  (MiAmpParams::gtrFuzzFuzz, "Gtr Fuzz Fuzz", 65.0f);
    addPct  (MiAmpParams::gtrFuzzTone, "Gtr Fuzz Tone", 52.0f);
    addPct  (MiAmpParams::gtrFuzzLvl,  "Gtr Fuzz Lvl",  55.0f);

    addBool (MiAmpParams::gtrChorusOn,    "Gtr Chorus On",    true);
    addPct  (MiAmpParams::gtrChorusRate,  "Gtr Chorus Rate",  42.0f);
    addPct  (MiAmpParams::gtrChorusDepth, "Gtr Chorus Depth", 48.0f);
    addPct  (MiAmpParams::gtrChorusMix,   "Gtr Chorus Mix",   44.0f);
    addPct  (MiAmpParams::gtrChorusEq,    "Gtr Chorus EQ",    55.0f);
    addInt  (MiAmpParams::gtrChorusMode,  "Gtr Chorus Mode",  0, 2, 0);

    addBool (MiAmpParams::gtrPhaserOn,    "Gtr Phaser On",    true);
    addPct  (MiAmpParams::gtrPhaserRate,  "Gtr Phaser Rate",  38.0f);
    addPct  (MiAmpParams::gtrPhaserDepth, "Gtr Phaser Depth", 55.0f);
    addPct  (MiAmpParams::gtrPhaserFb,    "Gtr Phaser Fb",    48.0f);

    addBool (MiAmpParams::gtrDelayOn,     "Gtr Delay On",     true);
    addPct  (MiAmpParams::gtrDelayTime,   "Gtr Delay Time",   42.0f);
    addPct  (MiAmpParams::gtrDelayFb,     "Gtr Delay Fb",     36.0f);
    addPct  (MiAmpParams::gtrDelayMix,    "Gtr Delay Mix",    32.0f);
    addPct  (MiAmpParams::gtrDelayMode,   "Gtr Delay Mode",   30.0f);
    addBool (MiAmpParams::gtrDelaySubdiv, "Gtr Delay Subdiv", false);

    addBool (MiAmpParams::gtrReverbOn,   "Gtr Reverb On",   true);
    addPct  (MiAmpParams::gtrReverbDec,  "Gtr Reverb Dec",  46.0f);
    addPct  (MiAmpParams::gtrReverbTone, "Gtr Reverb Tone", 56.0f);
    addPct  (MiAmpParams::gtrReverbMix,  "Gtr Reverb Mix",  28.0f);
    addPct  (MiAmpParams::gtrReverbMode, "Gtr Reverb Mode", 35.0f);

    addBool (MiAmpParams::gtrDistOn,    "Gtr Dist On",    true);
    addPct  (MiAmpParams::gtrDistTone,  "Gtr Dist Tone",  52.0f);
    addPct  (MiAmpParams::gtrDistLevel, "Gtr Dist Level", 60.0f);
    addPct  (MiAmpParams::gtrDistDrive, "Gtr Dist Drive", 62.0f);

    addBool (MiAmpParams::gtrDualOn,     "Gtr Dual On",     true);
    addPct  (MiAmpParams::gtrDualOutput, "Gtr Dual Output", 55.0f);
    addPct  (MiAmpParams::gtrDualGain,   "Gtr Dual Gain",   58.0f);
    addPct  (MiAmpParams::gtrDualBlend,  "Gtr Dual Blend",  50.0f);
    addPct  (MiAmpParams::gtrDualTone,   "Gtr Dual Tone",   56.0f);
    addPct  (MiAmpParams::gtrDualDrive,  "Gtr Dual Drive",  60.0f);
    addBool (MiAmpParams::gtrDualScoop,  "Gtr Dual Scoop",  false);

    addBool (MiAmpParams::gtrFlangeOn,     "Gtr Flange On",     true);
    addPct  (MiAmpParams::gtrFlangeManual, "Gtr Flange Manual", 48.0f);
    addPct  (MiAmpParams::gtrFlangeRes,    "Gtr Flange Res",    45.0f);
    addPct  (MiAmpParams::gtrFlangeDepth,  "Gtr Flange Depth",  55.0f);
    addPct  (MiAmpParams::gtrFlangeRate,   "Gtr Flange Rate",   36.0f);
    addInt  (MiAmpParams::gtrFlangeMode,   "Gtr Flange Mode",   0, 2, 0);

    addBool (MiAmpParams::gtrGateOn,     "Gtr Gate On",     true);
    addPct  (MiAmpParams::gtrGateThresh, "Gtr Gate Thresh", 32.0f);
    addPct  (MiAmpParams::gtrGateDecay,  "Gtr Gate Decay",  40.0f);
    addPct  (MiAmpParams::gtrGateMode,   "Gtr Gate Mode",   50.0f);

    addBool (MiAmpParams::gtrOctOn,     "Gtr Oct On",     true);
    addPct  (MiAmpParams::gtrOctDirect, "Gtr Oct Direct", 85.0f);
    addPct  (MiAmpParams::gtrOctUp,     "Gtr Oct Up",     25.0f);
    addPct  (MiAmpParams::gtrOctSub1,   "Gtr Oct -1",     55.0f);
    addPct  (MiAmpParams::gtrOctSub2,   "Gtr Oct -2",     20.0f);

    addBool (MiAmpParams::gtrEqOn, "Gtr EQ On", true);
    for (int b = 0; b < 7; ++b)
        addPct (MiAmpParams::gtrEqBand[b], "Gtr EQ Band " + juce::String (b + 1), 50.0f);

    // v25 Extended Guitar & Bass Head Parameters
    addInt  (MiAmpParams::gtrHeadModel,  "Guitar Head Model",  0, 8, 8); // 8 = MI AMP ZENBOX (Classic Mint)
    addPct  (MiAmpParams::gtrResonance,  "Guitar Resonance",   50.0f);
    addPct  (MiAmpParams::gtrVol2,       "Guitar Vol 2",       45.0f);
    addPct  (MiAmpParams::gtrCleanBass,  "Guitar Clean Bass",  52.0f);
    addPct  (MiAmpParams::gtrCleanTreb,  "Guitar Clean Treble",56.0f);
    addPct  (MiAmpParams::gtrAmpRev,     "Guitar Amp Reverb",  0.0f);
    addPct  (MiAmpParams::gtrTremSpd,    "Guitar Trem Speed",  45.0f);
    addPct  (MiAmpParams::gtrTremDep,    "Guitar Trem Depth",  0.0f);
    addPct  (MiAmpParams::gtrAtten,      "Guitar Attenuator",  85.0f);
    addBool (MiAmpParams::gtrBright2,    "Guitar Bright 2",    false);
    addBool (MiAmpParams::gtrCrunchSw,   "Guitar Crunch Sw",   false);
    addBool (MiAmpParams::gtrHalfPwr,    "Guitar Half Power",  false);
    addInt  (MiAmpParams::gtrVoiceSw,    "Guitar Voice Sw",    0, 2, 1);

    addInt  (MiAmpParams::bassHeadModel, "Bass Head Model",    0, 8, 8); // 8 = MI AMP CLASSIC (Mint)
    addInt  (MiAmpParams::bassCabModel,  "Bass Cab Model",     0, 7, 4); // 4 = 8x10 CLASSIC
    addPct  (MiAmpParams::bassMaster,    "Bass Master",        58.0f);
    addPct  (MiAmpParams::bassTubeGain,  "Bass Tube Gain",     50.0f);
    addPct  (MiAmpParams::bassCompCtrl,  "Bass Head Comp",     35.0f);
    addPct  (MiAmpParams::bassDriveCtrl, "Bass Head Drive",    45.0f);
    addPct  (MiAmpParams::bassToneCtrl,  "Bass Head Tone",     55.0f);
    addPct  (MiAmpParams::bassBlendCtrl, "Bass Head Blend",    50.0f);
    addPct  (MiAmpParams::bassLoMid,     "Bass Lo-Mid",        50.0f);
    addPct  (MiAmpParams::bassBiAmpLo,   "Bass Bi-Amp Lo",     55.0f);
    addBool (MiAmpParams::bassPad,       "Bass Pad",           false);
    addBool (MiAmpParams::bassMuteSw,    "Bass Mute",          false);
    addBool (MiAmpParams::bassUltraLo,   "Bass Ultra-Lo",      false);
    addBool (MiAmpParams::bassBrightSw,  "Bass Bright Sw",     false);
    addBool (MiAmpParams::bassDeepSw,    "Bass Deep Sw",       false);
    addBool (MiAmpParams::bassHeadEqOn,  "Bass Head GEQ On",   true);
    for (int b = 0; b < 10; ++b)
        addPct (MiAmpParams::bassHeadEq[b], "Bass Head GEQ " + juce::String (b + 1), 50.0f);

    // v25 Dual Microphones & Room
    addBool (MiAmpParams::mic1On,    "MI-57 On",      true);
    addPct  (MiAmpParams::mic1Level, "MI-57 Level",   85.0f);
    addPct  (MiAmpParams::mic1PosX,  "MI-57 Pos X",   42.0f);
    addPct  (MiAmpParams::mic1PosY,  "MI-57 Pos Y",   52.0f);
    addInt  (MiAmpParams::mic1Dist,  "MI-57 Dist",    0, 2, 0);
    addBool (MiAmpParams::mic1Phase, "MI-57 Phase",   false);

    addBool (MiAmpParams::mic2On,    "MI-87 On",      true);
    addPct  (MiAmpParams::mic2Level, "MI-87 Level",   75.0f);
    addPct  (MiAmpParams::mic2PosX,  "MI-87 Pos X",   60.0f);
    addPct  (MiAmpParams::mic2PosY,  "MI-87 Pos Y",   48.0f);
    addInt  (MiAmpParams::mic2Dist,  "MI-87 Dist",    0, 2, 1);
    addBool (MiAmpParams::mic2Phase, "MI-87 Phase",   false);

    addPct  (MiAmpParams::cabMicMix, "Mic Blend",     45.0f);
    addPct  (MiAmpParams::cabRoom,   "Room Ambience", 18.0f);

    // v25 Post-Amp Stereo STUDIO FX
    addBool (MiAmpParams::sfxCompOn,   "Multi-Comp On",     false);
    addPct  (MiAmpParams::sfxCompPar,  "Multi-Comp Mix",    100.0f);
    addPct  (MiAmpParams::sfxCompXLow, "Multi-Comp X-Low",  180.0f, 50.0f, 800.0f, 1.0f);
    addPct  (MiAmpParams::sfxCompXHigh,"Multi-Comp X-High", 2400.0f, 800.0f, 8000.0f, 1.0f);
    const float defThr[3] = { -18.0f, -16.0f, -15.0f };
    const float defRat[3] = { 3.5f, 3.0f, 2.5f };
    const float defAtt[3] = { 12.0f, 10.0f, 6.0f };
    const float defRel[3] = { 120.0f, 95.0f, 75.0f };
    for (int b = 0; b < 3; ++b)
    {
        addBool (MiAmpParams::sfxCompBOn[b],  "Comp Band " + juce::String (b + 1) + " On", true);
        addBool (MiAmpParams::sfxCompBSol[b], "Comp Band " + juce::String (b + 1) + " Solo", false);
        addPct  (MiAmpParams::sfxCompBThr[b], "Comp Band " + juce::String (b + 1) + " Thresh", defThr[b], -48.0f, 0.0f, 0.1f);
        addPct  (MiAmpParams::sfxCompBRat[b], "Comp Band " + juce::String (b + 1) + " Ratio",  defRat[b], 1.0f, 20.0f, 0.1f);
        addPct  (MiAmpParams::sfxCompBAtt[b], "Comp Band " + juce::String (b + 1) + " Attack", defAtt[b], 0.5f, 100.0f, 0.5f);
        addPct  (MiAmpParams::sfxCompBRel[b], "Comp Band " + juce::String (b + 1) + " Rel",    defRel[b], 10.0f, 800.0f, 1.0f);
        addPct  (MiAmpParams::sfxCompBGn[b],  "Comp Band " + juce::String (b + 1) + " Gain",   0.0f, -12.0f, 18.0f, 0.1f);
    }

    addBool (MiAmpParams::sfxEqOn,   "Studio EQ On",       false);
    addBool (MiAmpParams::sfxEqAnal, "Studio EQ Analyzer", true);
    addPct  (MiAmpParams::sfxEqHpf,  "Studio EQ HPF",      30.0f, 20.0f, 400.0f, 1.0f);
    addPct  (MiAmpParams::sfxEqLpf,  "Studio EQ LPF",      18000.0f, 2000.0f, 20000.0f, 10.0f);
    addPct  (MiAmpParams::sfxEqOut,  "Studio EQ Out",      0.0f, -12.0f, 12.0f, 0.1f);
    const float defEqF[5] = { 90.0f, 320.0f, 1000.0f, 3200.0f, 8000.0f };
    for (int b = 0; b < 5; ++b)
    {
        addPct (MiAmpParams::sfxEqFreq[b], "Studio EQ Band " + juce::String (b + 1) + " Freq", defEqF[b], 30.0f, 16000.0f, 1.0f);
        addPct (MiAmpParams::sfxEqGain[b], "Studio EQ Band " + juce::String (b + 1) + " Gain", 0.0f, -15.0f, 15.0f, 0.1f);
        addPct (MiAmpParams::sfxEqQ[b],    "Studio EQ Band " + juce::String (b + 1) + " Q",    1.0f, 0.3f, 8.0f, 0.05f);
    }

    addBool (MiAmpParams::sfxDlyOn,    "Studio Delay On",       false);
    addBool (MiAmpParams::sfxDlyPP,    "Studio Delay PingPong", true);
    addBool (MiAmpParams::sfxDlySync,  "Studio Delay Sync",     false);
    addPct  (MiAmpParams::sfxDlyTime,  "Studio Delay Time",     380.0f, 20.0f, 1500.0f, 1.0f);
    addInt  (MiAmpParams::sfxDlySub,   "Studio Delay Subdiv",   0, 4, 2);
    addPct  (MiAmpParams::sfxDlyFb,    "Studio Delay Fb",       36.0f);
    addPct  (MiAmpParams::sfxDlyLoCut, "Studio Delay Lo-Cut",   120.0f, 20.0f, 1200.0f, 1.0f);
    addPct  (MiAmpParams::sfxDlyHiCut, "Studio Delay Hi-Cut",   5500.0f, 1000.0f, 18000.0f, 10.0f);
    addPct  (MiAmpParams::sfxDlyMix,   "Studio Delay Mix",      28.0f);

    addBool (MiAmpParams::sfxRevOn,     "Studio Reverb On",     false);
    addInt  (MiAmpParams::sfxRevMode,   "Studio Reverb Mode",   0, 3, 1);
    addPct  (MiAmpParams::sfxRevPreDly, "Studio Reverb PreDly", 18.0f, 0.0f, 120.0f, 1.0f);
    addPct  (MiAmpParams::sfxRevDecay,  "Studio Reverb Decay",  52.0f);
    addPct  (MiAmpParams::sfxRevDamp,   "Studio Reverb Damp",   45.0f);
    addPct  (MiAmpParams::sfxRevWidth,  "Studio Reverb Width",  75.0f);
    addPct  (MiAmpParams::sfxRevMix,    "Studio Reverb Mix",    26.0f);

    return { p.begin(), p.end() };
}

void MiAmpAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    bassDsp.prepare (sampleRate, samplesPerBlock, std::max (2, getTotalNumOutputChannels()));
    guitarDsp.prepare (sampleRate, samplesPerBlock, std::max (2, getTotalNumOutputChannels()));
    cabMicDsp.prepare (sampleRate, samplesPerBlock);
    studioFxDsp.prepare (sampleRate, samplesPerBlock);
    tuner.prepare (sampleRate);
    monoScratch.resize (static_cast<size_t> (std::max (256, samplesPerBlock)), 0.0f);
}

void MiAmpAudioProcessor::releaseResources()
{
    bassDsp.reset();
    guitarDsp.reset();
    cabMicDsp.reset();
    studioFxDsp.reset();
    tuner.reset();
}

bool MiAmpAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return in == out || in == juce::AudioChannelSet::mono();
}

bool MiAmpAudioProcessor::isBassMode() const noexcept
{
    return apvts.getRawParameterValue (MiAmpParams::instrument)->load() >= 0.5f;
}

void MiAmpAudioProcessor::setInstrumentMode (bool bass)
{
    setParamNormalized (MiAmpParams::instrument, bass ? 1.0f : 0.0f);
}

miamp::BassControlSnapshot MiAmpAudioProcessor::readBassSnapshot() const noexcept
{
    miamp::BassControlSnapshot s;
    auto g = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    s.headModel  = static_cast<int> (std::lround (g (MiAmpParams::bassHeadModel)));
    s.useExternalCabEngine = true;
    s.ampPower   = g (MiAmpParams::ampPower)   >= 0.5f;
    s.ampStandby = g (MiAmpParams::ampStandby) >= 0.5f;
    s.ampUltraHi = g (MiAmpParams::ampUltraHi) >= 0.5f;
    s.ampCabOn   = g (MiAmpParams::ampCabOn)   >= 0.5f;
    s.headBypass = g (MiAmpParams::bassHeadBypass) >= 0.5f;
    s.ampGain    = g (MiAmpParams::ampGain);
    s.ampBass    = g (MiAmpParams::ampBass);
    s.ampMid     = g (MiAmpParams::ampMid);
    s.ampFreq    = g (MiAmpParams::ampFreq);
    s.ampTreble  = g (MiAmpParams::ampTreble);

    s.master     = g (MiAmpParams::bassMaster);
    s.tubeGain   = g (MiAmpParams::bassTubeGain);
    s.compCtrl   = g (MiAmpParams::bassCompCtrl);
    s.driveCtrl  = g (MiAmpParams::bassDriveCtrl);
    s.toneCtrl   = g (MiAmpParams::bassToneCtrl);
    s.blendCtrl  = g (MiAmpParams::bassBlendCtrl);
    s.loMid      = g (MiAmpParams::bassLoMid);
    s.biAmpLo    = g (MiAmpParams::bassBiAmpLo);
    s.pad        = g (MiAmpParams::bassPad)      >= 0.5f;
    s.muteSw     = g (MiAmpParams::bassMuteSw)   >= 0.5f;
    s.ultraLo    = g (MiAmpParams::bassUltraLo)  >= 0.5f;
    s.brightSw   = g (MiAmpParams::bassBrightSw) >= 0.5f;
    s.deepSw     = g (MiAmpParams::bassDeepSw)   >= 0.5f;
    s.headEqOn   = g (MiAmpParams::bassHeadEqOn) >= 0.5f;
    for (int b = 0; b < 10; ++b)
        s.headEq[static_cast<size_t> (b)] = g (MiAmpParams::bassHeadEq[b]);

    s.pitchEnabled   = g (MiAmpParams::pitchOn) >= 0.5f;
    s.pitchSemitones = static_cast<int> (std::lround (g (MiAmpParams::pitchSemi)));
    s.pitchCents     = g (MiAmpParams::pitchCents);
    s.pitchMix       = g (MiAmpParams::pitchMix);
    s.tunerMuted     = g (MiAmpParams::tunerMuted) >= 0.5f;

    for (int i = 0; i < 10; ++i)
        s.chainSlots[static_cast<size_t> (i)] = static_cast<int> (std::lround (g (MiAmpParams::slot[i])));

    s.compOn      = g (MiAmpParams::compOn) >= 0.5f;
    s.compSustain = g (MiAmpParams::compSustain);
    s.compAttack  = g (MiAmpParams::compAttack);
    s.compLevel   = g (MiAmpParams::compLevel);
    s.compRelease = g (MiAmpParams::compRelease);
    s.compInput   = g (MiAmpParams::compInput);

    s.xvOn       = g (MiAmpParams::xvOn) >= 0.5f;
    s.xvLevel    = g (MiAmpParams::xvLevel);
    s.xvDrive    = g (MiAmpParams::xvDrive);
    s.xvLow      = g (MiAmpParams::xvLow);
    s.xvHigh     = g (MiAmpParams::xvHigh);
    s.xvBlend    = g (MiAmpParams::xvBlend);
    s.xvMid      = g (MiAmpParams::xvMid);
    s.xvPresence = g (MiAmpParams::xvPresence);
    s.xvCab      = static_cast<int> (std::lround (g (MiAmpParams::xvCab)));

    s.chorusOn    = g (MiAmpParams::chorusOn) >= 0.5f;
    s.chorusRate  = g (MiAmpParams::chorusRate);
    s.chorusDepth = g (MiAmpParams::chorusDepth);
    s.chorusMix   = g (MiAmpParams::chorusMix);
    s.chorusLoCut = g (MiAmpParams::chorusLoCut);
    s.chorusXOver = g (MiAmpParams::chorusXOver) >= 0.5f;

    s.phaserOn       = g (MiAmpParams::phaserOn) >= 0.5f;
    s.phaserRate     = g (MiAmpParams::phaserRate);
    s.phaserDepth    = g (MiAmpParams::phaserDepth);
    s.phaserFeedback = g (MiAmpParams::phaserFeedback);
    s.phaserStage    = g (MiAmpParams::phaserStage);

    s.reverbOn    = g (MiAmpParams::reverbOn) >= 0.5f;
    s.reverbDecay = g (MiAmpParams::reverbDecay);
    s.reverbTone  = g (MiAmpParams::reverbTone);
    s.reverbMix   = g (MiAmpParams::reverbMix);
    s.reverbMode  = g (MiAmpParams::reverbMode);

    s.bddiOn       = g (MiAmpParams::bddiOn) >= 0.5f;
    s.bddiLevel    = g (MiAmpParams::bddiLevel);
    s.bddiBlend    = g (MiAmpParams::bddiBlend);
    s.bddiTreble   = g (MiAmpParams::bddiTreble);
    s.bddiPresence = g (MiAmpParams::bddiPresence);
    s.bddiDrive    = g (MiAmpParams::bddiDrive);
    s.bddiBass     = g (MiAmpParams::bddiBass);
    s.bddiXlrPad   = g (MiAmpParams::bddiXlrPad) >= 0.5f;
    s.bddiInBoost  = g (MiAmpParams::bddiInBoost) >= 0.5f;
    s.bddiOutBoost = g (MiAmpParams::bddiOutBoost) >= 0.5f;

    s.goldOn    = g (MiAmpParams::goldOn) >= 0.5f;
    s.goldLevel = g (MiAmpParams::goldLevel);
    s.goldEq    = g (MiAmpParams::goldEq);
    s.goldBlend = g (MiAmpParams::goldBlend);
    s.goldGain  = g (MiAmpParams::goldGain);

    s.foodOn     = g (MiAmpParams::bfOn) >= 0.5f;
    s.foodVol    = g (MiAmpParams::bfVol);
    s.foodTreble = g (MiAmpParams::bfTreble);
    s.foodDrive  = g (MiAmpParams::bfDrive);
    s.foodBlend  = g (MiAmpParams::bfBlend);
    s.foodPad    = g (MiAmpParams::bfPad) >= 0.5f;

    s.oliveOn      = g (MiAmpParams::oliveOn) >= 0.5f;
    s.oliveVol     = g (MiAmpParams::oliveVol);
    s.oliveTone    = g (MiAmpParams::oliveTone);
    s.oliveSustain = g (MiAmpParams::oliveSustain);
    s.oliveMode    = static_cast<int> (std::lround (g (MiAmpParams::oliveMode)));

    s.flangeOn     = g (MiAmpParams::bassFlangeOn) >= 0.5f;
    s.flangeManual = g (MiAmpParams::bassFlangeManual);
    s.flangeRes    = g (MiAmpParams::bassFlangeRes);
    s.flangeDepth  = g (MiAmpParams::bassFlangeDepth);
    s.flangeRate   = g (MiAmpParams::bassFlangeRate);
    s.flangeMode   = static_cast<int> (std::lround (g (MiAmpParams::bassFlangeMode)));

    s.delayOn     = g (MiAmpParams::bassDelayOn) >= 0.5f;
    s.delayTime   = g (MiAmpParams::bassDelayTime);
    s.delayFb     = g (MiAmpParams::bassDelayFb);
    s.delayMix    = g (MiAmpParams::bassDelayMix);
    s.delayMode   = g (MiAmpParams::bassDelayMode);
    s.delaySubdiv = g (MiAmpParams::bassDelaySubdiv) >= 0.5f;

    s.gateOn     = g (MiAmpParams::bassGateOn) >= 0.5f;
    s.gateThresh = g (MiAmpParams::bassGateThresh);
    s.gateDecay  = g (MiAmpParams::bassGateDecay);
    s.gateMode   = g (MiAmpParams::bassGateMode);

    s.octOn  = g (MiAmpParams::bassOctOn) >= 0.5f;
    s.octDry = g (MiAmpParams::bassOctDry);
    s.octSub = g (MiAmpParams::bassOctSub);
    s.octUp  = g (MiAmpParams::bassOctUp);

    s.eqOn = g (MiAmpParams::bassEqOn) >= 0.5f;
    for (int b = 0; b < 7; ++b)
        s.eqBands[static_cast<size_t> (b)] = g (MiAmpParams::bassEqBand[b]);
    return s;
}

zenbox::GuitarControlSnapshot MiAmpAudioProcessor::readGuitarSnapshot() const noexcept
{
    zenbox::GuitarControlSnapshot s;
    auto g = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    s.headModel     = static_cast<int> (std::lround (g (MiAmpParams::gtrHeadModel)));
    s.useExternalCabEngine = true;
    s.inputGain     = g (MiAmpParams::gtrInput);
    s.gateThresh    = g (MiAmpParams::gtrGate);
    s.os4x          = g (MiAmpParams::gtrOs4x) >= 0.5f;
    s.cabIndex      = static_cast<int> (std::lround (g (MiAmpParams::gtrCab)));
    s.ampPower      = g (MiAmpParams::gtrPower) >= 0.5f;
    s.ampStandby    = g (MiAmpParams::gtrStandby) >= 0.5f;
    s.ampModeBright = g (MiAmpParams::gtrModeBright) >= 0.5f;
    s.activeChannel = static_cast<int> (std::lround (g (MiAmpParams::gtrChannel)));
    s.outputKnob    = g (MiAmpParams::gtrOutput);
    s.headBypass    = g (MiAmpParams::gtrHeadBypass) >= 0.5f;
    s.resonance     = g (MiAmpParams::gtrResonance);
    s.vol2          = g (MiAmpParams::gtrVol2);
    s.cleanBass     = g (MiAmpParams::gtrCleanBass);
    s.cleanTreb     = g (MiAmpParams::gtrCleanTreb);
    s.ampReverb     = g (MiAmpParams::gtrAmpRev);
    s.tremSpeed     = g (MiAmpParams::gtrTremSpd);
    s.tremDepth     = g (MiAmpParams::gtrTremDep);
    s.attenuator    = g (MiAmpParams::gtrAtten);
    s.bright2       = g (MiAmpParams::gtrBright2) >= 0.5f;
    s.crunchSw      = g (MiAmpParams::gtrCrunchSw) >= 0.5f;
    s.halfPower     = g (MiAmpParams::gtrHalfPwr) >= 0.5f;
    s.voiceSw       = static_cast<int> (std::lround (g (MiAmpParams::gtrVoiceSw)));

    for (size_t c = 0; c < 3; ++c)
    {
        s.chGain[c]     = g (MiAmpParams::gtrChGain[c]);
        s.chBass[c]     = g (MiAmpParams::gtrChBass[c]);
        s.chMid[c]      = g (MiAmpParams::gtrChMid[c]);
        s.chTreble[c]   = g (MiAmpParams::gtrChTreble[c]);
        s.chPresence[c] = g (MiAmpParams::gtrChPresence[c]);
        s.chMaster[c]   = g (MiAmpParams::gtrChMaster[c]);
    }

    s.pitchEnabled   = g (MiAmpParams::pitchOn) >= 0.5f;
    s.pitchSemitones = static_cast<int> (std::lround (g (MiAmpParams::pitchSemi)));
    s.pitchCents     = g (MiAmpParams::pitchCents);
    s.pitchMix       = g (MiAmpParams::pitchMix);
    s.tunerMuted     = g (MiAmpParams::tunerMuted) >= 0.5f;

    for (int i = 0; i < 10; ++i)
        s.chainSlots[static_cast<size_t> (i)] = static_cast<int> (std::lround (g (MiAmpParams::gtrSlot[i])));

    s.compOn      = g (MiAmpParams::gtrCompOn) >= 0.5f;
    s.compSustain = g (MiAmpParams::gtrCompSus);
    s.compAttack  = g (MiAmpParams::gtrCompAtt);
    s.compLevel   = g (MiAmpParams::gtrCompLvl);
    s.compTone    = g (MiAmpParams::gtrCompTone);

    s.driveOn    = g (MiAmpParams::gtrDriveOn) >= 0.5f;
    s.driveGain  = g (MiAmpParams::gtrDriveGain);
    s.driveTone  = g (MiAmpParams::gtrDriveTone);
    s.driveLevel = g (MiAmpParams::gtrDriveLvl);
    s.driveClip  = g (MiAmpParams::gtrDriveClip) >= 0.5f;

    s.fuzzOn    = g (MiAmpParams::gtrFuzzOn) >= 0.5f;
    s.fuzzFuzz  = g (MiAmpParams::gtrFuzzFuzz);
    s.fuzzTone  = g (MiAmpParams::gtrFuzzTone);
    s.fuzzLevel = g (MiAmpParams::gtrFuzzLvl);

    s.chorusOn    = g (MiAmpParams::gtrChorusOn) >= 0.5f;
    s.chorusRate  = g (MiAmpParams::gtrChorusRate);
    s.chorusDepth = g (MiAmpParams::gtrChorusDepth);
    s.chorusMix   = g (MiAmpParams::gtrChorusMix);
    s.chorusEq    = g (MiAmpParams::gtrChorusEq);
    s.chorusMode  = static_cast<int> (std::lround (g (MiAmpParams::gtrChorusMode)));

    s.phaserOn       = g (MiAmpParams::gtrPhaserOn) >= 0.5f;
    s.phaserRate     = g (MiAmpParams::gtrPhaserRate);
    s.phaserDepth    = g (MiAmpParams::gtrPhaserDepth);
    s.phaserFeedback = g (MiAmpParams::gtrPhaserFb);

    s.delayOn       = g (MiAmpParams::gtrDelayOn) >= 0.5f;
    s.delayTime     = g (MiAmpParams::gtrDelayTime);
    s.delayFeedback = g (MiAmpParams::gtrDelayFb);
    s.delayMix      = g (MiAmpParams::gtrDelayMix);
    s.delayMode     = g (MiAmpParams::gtrDelayMode);
    s.delaySubdiv   = g (MiAmpParams::gtrDelaySubdiv) >= 0.5f;

    s.reverbOn    = g (MiAmpParams::gtrReverbOn) >= 0.5f;
    s.reverbDecay = g (MiAmpParams::gtrReverbDec);
    s.reverbTone  = g (MiAmpParams::gtrReverbTone);
    s.reverbMix   = g (MiAmpParams::gtrReverbMix);
    s.reverbMode  = g (MiAmpParams::gtrReverbMode);

    s.distOn    = g (MiAmpParams::gtrDistOn) >= 0.5f;
    s.distTone  = g (MiAmpParams::gtrDistTone);
    s.distLevel = g (MiAmpParams::gtrDistLevel);
    s.distDrive = g (MiAmpParams::gtrDistDrive);

    s.dualOn     = g (MiAmpParams::gtrDualOn) >= 0.5f;
    s.dualOutput = g (MiAmpParams::gtrDualOutput);
    s.dualGain   = g (MiAmpParams::gtrDualGain);
    s.dualBlend  = g (MiAmpParams::gtrDualBlend);
    s.dualTone   = g (MiAmpParams::gtrDualTone);
    s.dualDrive  = g (MiAmpParams::gtrDualDrive);
    s.dualScoop  = g (MiAmpParams::gtrDualScoop) >= 0.5f;

    s.flangeOn     = g (MiAmpParams::gtrFlangeOn) >= 0.5f;
    s.flangeManual = g (MiAmpParams::gtrFlangeManual);
    s.flangeRes    = g (MiAmpParams::gtrFlangeRes);
    s.flangeDepth  = g (MiAmpParams::gtrFlangeDepth);
    s.flangeRate   = g (MiAmpParams::gtrFlangeRate);
    s.flangeMode   = static_cast<int> (std::lround (g (MiAmpParams::gtrFlangeMode)));

    s.gatePedalOn     = g (MiAmpParams::gtrGateOn) >= 0.5f;
    s.gatePedalThresh = g (MiAmpParams::gtrGateThresh);
    s.gatePedalDecay  = g (MiAmpParams::gtrGateDecay);
    s.gatePedalMode   = g (MiAmpParams::gtrGateMode);

    s.octOn     = g (MiAmpParams::gtrOctOn) >= 0.5f;
    s.octDirect = g (MiAmpParams::gtrOctDirect);
    s.octUp     = g (MiAmpParams::gtrOctUp);
    s.octSub1   = g (MiAmpParams::gtrOctSub1);
    s.octSub2   = g (MiAmpParams::gtrOctSub2);

    s.eqOn = g (MiAmpParams::gtrEqOn) >= 0.5f;
    for (int b = 0; b < 7; ++b)
        s.eqBands[static_cast<size_t> (b)] = g (MiAmpParams::gtrEqBand[b]);
    return s;
}

miamp::CabMicSnapshot MiAmpAudioProcessor::readCabMicSnapshot() const noexcept
{
    miamp::CabMicSnapshot s;
    auto g = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };
    s.isBass   = isBassMode();
    s.cabIndex = static_cast<int> (std::lround (g (s.isBass ? MiAmpParams::bassCabModel : MiAmpParams::gtrCab)));

    s.mic1On    = g (MiAmpParams::mic1On) >= 0.5f;
    s.mic1Level = g (MiAmpParams::mic1Level);
    s.mic1PosX  = g (MiAmpParams::mic1PosX);
    s.mic1PosY  = g (MiAmpParams::mic1PosY);
    s.mic1Dist  = static_cast<int> (std::lround (g (MiAmpParams::mic1Dist)));
    s.mic1Phase = g (MiAmpParams::mic1Phase) >= 0.5f;

    s.mic2On    = g (MiAmpParams::mic2On) >= 0.5f;
    s.mic2Level = g (MiAmpParams::mic2Level);
    s.mic2PosX  = g (MiAmpParams::mic2PosX);
    s.mic2PosY  = g (MiAmpParams::mic2PosY);
    s.mic2Dist  = static_cast<int> (std::lround (g (MiAmpParams::mic2Dist)));
    s.mic2Phase = g (MiAmpParams::mic2Phase) >= 0.5f;

    s.micMix  = g (MiAmpParams::cabMicMix);
    s.roomAmt = g (MiAmpParams::cabRoom);
    return s;
}

miamp::StudioFxSnapshot MiAmpAudioProcessor::readStudioFxSnapshot() const noexcept
{
    miamp::StudioFxSnapshot s;
    auto g = [this](const char* id) { return apvts.getRawParameterValue (id)->load(); };

    s.compOn       = g (MiAmpParams::sfxCompOn) >= 0.5f;
    s.compParallel = g (MiAmpParams::sfxCompPar);
    s.compXLowHz   = g (MiAmpParams::sfxCompXLow);
    s.compXHighHz  = g (MiAmpParams::sfxCompXHigh);
    for (size_t b = 0; b < 3; ++b)
    {
        s.compBands[b].on       = g (MiAmpParams::sfxCompBOn[b]) >= 0.5f;
        s.compBands[b].solo     = g (MiAmpParams::sfxCompBSol[b]) >= 0.5f;
        s.compBands[b].threshDb = g (MiAmpParams::sfxCompBThr[b]);
        s.compBands[b].ratio    = g (MiAmpParams::sfxCompBRat[b]);
        s.compBands[b].attackMs = g (MiAmpParams::sfxCompBAtt[b]);
        s.compBands[b].relMs    = g (MiAmpParams::sfxCompBRel[b]);
        s.compBands[b].gainDb   = g (MiAmpParams::sfxCompBGn[b]);
    }

    s.eqOn       = g (MiAmpParams::sfxEqOn) >= 0.5f;
    s.eqAnalyzer = g (MiAmpParams::sfxEqAnal) >= 0.5f;
    s.eqHpfHz    = g (MiAmpParams::sfxEqHpf);
    s.eqLpfHz    = g (MiAmpParams::sfxEqLpf);
    s.eqOutDb    = g (MiAmpParams::sfxEqOut);
    for (size_t b = 0; b < 5; ++b)
    {
        s.eqBands[b].freqHz = g (MiAmpParams::sfxEqFreq[b]);
        s.eqBands[b].gainDb = g (MiAmpParams::sfxEqGain[b]);
        s.eqBands[b].q      = g (MiAmpParams::sfxEqQ[b]);
    }

    s.dlyOn       = g (MiAmpParams::sfxDlyOn) >= 0.5f;
    s.dlyPingPong = g (MiAmpParams::sfxDlyPP) >= 0.5f;
    s.dlySync     = g (MiAmpParams::sfxDlySync) >= 0.5f;
    s.dlyTimeMs   = g (MiAmpParams::sfxDlyTime);
    s.dlySubdiv   = static_cast<int> (std::lround (g (MiAmpParams::sfxDlySub)));
    s.dlyFeedback = g (MiAmpParams::sfxDlyFb);
    s.dlyLowCutHz = g (MiAmpParams::sfxDlyLoCut);
    s.dlyHiCutHz  = g (MiAmpParams::sfxDlyHiCut);
    s.dlyMix      = g (MiAmpParams::sfxDlyMix);

    s.revOn       = g (MiAmpParams::sfxRevOn) >= 0.5f;
    s.revMode     = static_cast<int> (std::lround (g (MiAmpParams::sfxRevMode)));
    s.revPreDlyMs = g (MiAmpParams::sfxRevPreDly);
    s.revDecaySec = 0.3f + (g (MiAmpParams::sfxRevDecay) * 0.01f) * 7.2f;
    s.revDampHz   = 1000.0f + (1.0f - g (MiAmpParams::sfxRevDamp) * 0.01f) * 14000.0f;
    s.revWidthPct = g (MiAmpParams::sfxRevWidth);
    s.revMixPct   = g (MiAmpParams::sfxRevMix);
    return s;
}

bool MiAmpAudioProcessor::loadCustomCabIR (bool forBass, const juce::File& file)
{
    if (! cabMicDsp.loadCustomIr (forBass, file))
        return false;
    if (forBass)
    {
        customBassIrPath = file.getFullPathName();
        customBassIrName = file.getFileNameWithoutExtension();
    }
    else
    {
        customGtrIrPath = file.getFullPathName();
        customGtrIrName = file.getFileNameWithoutExtension();
    }
    return true;
}

void MiAmpAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() == 0)
        return;

    // Duplicate mono input to right channel if stereo output
    if (totalIn == 1 && totalOut >= 2)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // In Standalone mode, many 2-channel interfaces have the instrument plugged into
    // only Input 1 (L) or Input 2 (R). Automatically mirror the active channel to L+R
    // so it plays centered at full volume without needing DAW input routing.
    if (wrapperType == wrapperType_Standalone && totalIn >= 2 && totalOut >= 2)
    {
        float* ch0 = buffer.getWritePointer (0);
        float* ch1 = buffer.getWritePointer (1);
        double sq0 = 0.0, sq1 = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            sq0 += static_cast<double> (ch0[i]) * static_cast<double> (ch0[i]);
            sq1 += static_cast<double> (ch1[i]) * static_cast<double> (ch1[i]);
        }
        const double rms0 = std::sqrt (sq0 / static_cast<double> (numSamples));
        const double rms1 = std::sqrt (sq1 / static_cast<double> (numSamples));
        if (rms0 > rms1 * 2.5 || rms1 > rms0 * 2.5)
        {
            const bool use1 = (rms1 > rms0);
            for (int i = 0; i < numSamples; ++i)
            {
                const float s = use1 ? ch1[i] : ch0[i];
                ch0[i] = s;
                ch1[i] = s;
            }
        }
    }

    // Measure input RMS + feed shared tuner
    const float* in0 = buffer.getReadPointer (0);
    const float* in1 = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : in0;
    double inSq = 0.0;
    if (static_cast<int> (monoScratch.size()) < numSamples)
        monoScratch.resize (static_cast<size_t> (numSamples), 0.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = 0.5f * (in0[i] + in1[i]);
        monoScratch[static_cast<size_t> (i)] = m;
        inSq += static_cast<double> (m) * static_cast<double> (m);
    }
    inputRms.store (static_cast<float> (std::sqrt (inSq / static_cast<double> (numSamples))));
    tuner.pushSamples (monoScratch.data(), numSamples);

    const bool bass = isBassMode();
    if (bass)
    {
        bassDsp.process (buffer, readBassSnapshot());
        bassCompGrDb.store (bassDsp.getLastCompGrDb());
    }
    else
    {
        guitarDsp.process (buffer, readGuitarSnapshot());
        bassCompGrDb.store (0.0f);
    }

    if (! isHeadBypassed (bass))
        cabMicDsp.processStereo (buffer, readCabMicSnapshot());

    double dawBpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpmOpt = pos->getBpm())
                dawBpm = std::clamp (*bpmOpt, 40.0, 260.0);

    studioFxDsp.processStereo (buffer, readStudioFxSnapshot(), dawBpm);

    double outSq = 0.0;
    float peak = 0.0f;
    for (int c = 0; c < buffer.getNumChannels(); ++c)
    {
        const float* p = buffer.getReadPointer (c);
        for (int i = 0; i < numSamples; ++i)
        {
            const float v = p[i];
            outSq += static_cast<double> (v) * static_cast<double> (v);
            peak = std::max (peak, std::fabs (v));
        }
    }
    outputRms.store (static_cast<float> (std::sqrt (outSq / static_cast<double> (numSamples * buffer.getNumChannels()))));
    isClipping.store (peak >= 0.985f);
}

bool MiAmpAudioProcessor::isHeadBypassed (bool forBass) const noexcept
{
    const char* id = forBass ? MiAmpParams::bassHeadBypass : MiAmpParams::gtrHeadBypass;
    return apvts.getRawParameterValue (id)->load() >= 0.5f;
}

void MiAmpAudioProcessor::setHeadBypassed (bool forBass, bool bypass)
{
    setParamNormalized (forBass ? MiAmpParams::bassHeadBypass : MiAmpParams::gtrHeadBypass, bypass ? 1.0f : 0.0f);
}

juce::File MiAmpAudioProcessor::getSettingsFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PopupsAudio").getChildFile ("MiAmp");
    dir.createDirectory();
    return dir.getChildFile ("settings.xml");
}

juce::String MiAmpAudioProcessor::getGeminiApiKey()
{
    auto file = getSettingsFile();
    if (! file.existsAsFile()) return {};
    if (auto xml = juce::XmlDocument::parse (file))
        return xml->getStringAttribute ("geminiApiKey", "").trim();
    return {};
}

void MiAmpAudioProcessor::setGeminiApiKey (const juce::String& key)
{
    juce::XmlElement root ("MiAmpSettings");
    root.setAttribute ("geminiApiKey", key.trim());
    root.writeTo (getSettingsFile());
}

std::vector<int> MiAmpAudioProcessor::getActiveChain (bool forBass) const
{
    std::vector<int> out;
    const int maxTypes = forBass ? static_cast<int> (miamp::PedalType::Count)
                                 : static_cast<int> (zenbox::GuitarPedalType::Count);
    for (int i = 0; i < 10; ++i)
    {
        const char* id = forBass ? MiAmpParams::slot[i] : MiAmpParams::gtrSlot[i];
        const int v = static_cast<int> (std::lround (apvts.getRawParameterValue (id)->load()));
        if (v >= 0 && v < maxTypes && std::find (out.begin(), out.end(), v) == out.end())
            out.push_back (v);
    }
    return out;
}

void MiAmpAudioProcessor::setActiveChain (bool forBass, const std::vector<int>& order)
{
    for (int i = 0; i < 10; ++i)
    {
        const char* id = forBass ? MiAmpParams::slot[i] : MiAmpParams::gtrSlot[i];
        setParamNormalized (id, i < static_cast<int> (order.size()) ? static_cast<float> (order[static_cast<size_t> (i)]) : -1.0f);
    }
}

bool MiAmpAudioProcessor::addPedalToChain (bool forBass, int pedalType, int insertAt)
{
    auto chain = getActiveChain (forBass);
    const int maxTypes = forBass ? static_cast<int> (miamp::PedalType::Count)
                                 : static_cast<int> (zenbox::GuitarPedalType::Count);
    if (pedalType < 0 || pedalType >= maxTypes) return false;

    auto it = std::find (chain.begin(), chain.end(), pedalType);
    if (it != chain.end())
    {
        if (insertAt >= 0)
        {
            reorderPedalToIndex (forBass, pedalType, insertAt);
            return true;
        }
        return false;
    }

    if (static_cast<int> (chain.size()) >= 10) return false;
    if (insertAt < 0 || insertAt >= static_cast<int> (chain.size()))
        chain.push_back (pedalType);
    else
        chain.insert (chain.begin() + insertAt, pedalType);

    setActiveChain (forBass, chain);
    return true;
}

void MiAmpAudioProcessor::removePedalFromChain (bool forBass, int pedalType)
{
    auto chain = getActiveChain (forBass);
    chain.erase (std::remove (chain.begin(), chain.end(), pedalType), chain.end());
    setActiveChain (forBass, chain);
}

void MiAmpAudioProcessor::movePedalInChain (bool forBass, int pedalType, int delta)
{
    auto chain = getActiveChain (forBass);
    auto it = std::find (chain.begin(), chain.end(), pedalType);
    if (it == chain.end()) return;
    const int idx = static_cast<int> (std::distance (chain.begin(), it));
    reorderPedalToIndex (forBass, pedalType, idx + delta);
}

void MiAmpAudioProcessor::reorderPedalToIndex (bool forBass, int pedalType, int targetIndex)
{
    auto chain = getActiveChain (forBass);
    auto it = std::find (chain.begin(), chain.end(), pedalType);
    if (it == chain.end()) return;
    chain.erase (it);
    const int clamped = std::clamp (targetIndex, 0, static_cast<int> (chain.size()));
    chain.insert (chain.begin() + clamped, pedalType);
    setActiveChain (forBass, chain);
}

void MiAmpAudioProcessor::setParamNormalized (const char* id, float plainVal)
{
    if (auto* param = apvts.getParameter (id))
    {
        const float norm = param->convertTo0to1 (plainVal);
        param->beginChangeGesture();
        param->setValueNotifyingHost (norm);
        param->endChangeGesture();
    }
}

std::vector<MiAmpBassPreset> MiAmpAudioProcessor::getFactoryPresets()
{
    std::vector<MiAmpBassPreset> list;

    auto mkXv = [](const char* nameUtf8, float lvl, float drv, float lowDb, float highDb, float blendPct, float midDb, float presDb, int cab, const char* hintUtf8)
    {
        MiAmpBassPreset p;
        p.name = juce::String::fromUTF8 (nameUtf8); p.isXvFactory = true; p.hint = juce::String::fromUTF8 (hintUtf8);
        p.ampGain = 50.0f; p.ampBass = 50.0f; p.ampMid = 50.0f; p.ampFreq = 50.0f; p.ampTreble = 50.0f; p.ampUltraHi = false;
        p.slots = { 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.xvOn = true; p.xvLvl = lvl * 10.0f; p.xvDrv = drv * 10.0f;
        p.xvLow = dbToXvKnob (lowDb); p.xvHigh = dbToXvKnob (highDb);
        p.xvBlend = blendPct; p.xvMid = dbToXvKnob (midDb); p.xvPres = dbToXvKnob (presDb);
        p.xvCab = cab;
        return p;
    };

    list.push_back (mkXv (u8"Inicial (XV Pop-Punk)",       5.6f, 4.08f, +3.7f, +0.85f, 57.6f, -5.8f, +7.3f, 1, u8"Cabezal neutro + XV Pop-Punk (Cab PUNK)"));
    list.push_back (mkXv (u8"XV \u2022 Punk Cali Pick",    6.2f, 5.2f,  +4.5f, +2.0f,  72.0f, -4.5f, +8.5f, 1, u8"Bite agresivo con pua en PUNK"));
    list.push_back (mkXv (u8"XV \u2022 90s Punk Crunch",   5.8f, 4.4f,  +3.5f, +1.5f,  60.0f, -5.5f, +7.5f, 1, u8"Crunch limpio de finales de los 90"));
    list.push_back (mkXv (u8"XV \u2022 Pop Punk Moderno", 6.0f, 6.2f,  +5.0f, +2.5f,  78.0f, -6.5f, +9.0f, 1, u8"Scoop moderno con presencia alta"));
    list.push_back (mkXv (u8"XV \u2022 Clack de Pua",     5.5f, 3.8f,  +2.0f, +3.5f,  65.0f, -3.0f, +10.0f,1, u8"Ataque afilado de pua"));
    list.push_back (mkXv (u8"XV \u2022 Calido Vintage",   6.5f, 2.2f,  +5.5f, -3.5f,  35.0f, +2.5f, +1.5f, 0, u8"Cuerpo gordo clasico (Cab BASS)"));
    list.push_back (mkXv (u8"XV \u2022 Classic Soul",     7.0f, 1.5f,  +6.0f, -6.0f,  25.0f, +3.5f, -2.0f, 0, u8"Graves redondos sin brillo"));
    list.push_back (mkXv (u8"XV \u2022 Rock Clasico",     6.0f, 4.8f,  +3.5f, +0.5f,  58.0f, +1.5f, +4.5f, 0, u8"Rugido rockero con medios presentes"));
    list.push_back (mkXv (u8"XV \u2022 Sub / Slap Clean", 6.4f, 1.2f,  +6.5f, +3.0f,  30.0f, -7.0f, +6.5f, 2, u8"Clean + sub profundo"));
    list.push_back (mkXv (u8"XV \u2022 Drive Brutal",     5.4f, 8.8f,  +4.0f, +2.0f,  88.0f, -2.0f, +9.5f, 1, u8"Saturacion extrema"));
    list.push_back (mkXv (u8"XV \u2022 DI Estudio",       6.2f, 2.5f,  +2.5f, +1.0f,  45.0f, -1.5f, +3.5f, 2, u8"Directo cristalino sin cabina"));

    {
        auto p = mkXv (u8"XV Directo \u2022 Original PUNK", 5.6f, 4.08f, +3.7f, +0.85f, 57.6f, -5.8f, +7.3f, 1, u8"SOLO PEDALES: XV puro sin cabezal (identico al original)");
        p.headBypass = true;
        list.push_back (p);
    }
    {
        auto p = mkXv (u8"XV Directo \u2022 Original BASS", 6.2f, 3.2f, +4.8f, -1.5f, 48.0f, +1.5f, +3.2f, 0, u8"SOLO PEDALES: XV puro con Cab BASS sin cabezal");
        p.headBypass = true;
        list.push_back (p);
    }

    {
        MiAmpBassPreset p;
        p.name = "8x10 Classic Tube";
        p.hint = "Cabezal 8x10 caliente + Compresor suave, sin XV";
        p.ampGain = 62.0f; p.ampBass = 58.0f; p.ampMid = 54.0f; p.ampFreq = 46.0f; p.ampTreble = 56.0f; p.ampUltraHi = false;
        p.slots = { 0, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.compOn = true; p.compSus = 48.0f; p.compAtt = 42.0f; p.compLvl = 56.0f;
        list.push_back (p);
    }
    {
        MiAmpBassPreset p;
        p.name = "Dream Chorus 80s";
        p.hint = "Compresor + Chorus ancho + Reverb de estudio";
        p.ampGain = 48.0f; p.ampBass = 52.0f; p.ampMid = 48.0f; p.ampFreq = 58.0f; p.ampTreble = 60.0f; p.ampUltraHi = true;
        p.slots = { 0, 2, 4, -1, -1, -1, -1, -1, -1, -1 };
        p.compOn = true; p.compSus = 54.0f; p.compAtt = 38.0f; p.compLvl = 55.0f;
        p.chorusOn = true; p.chRate = 38.0f; p.chDepth = 62.0f; p.chMix = 52.0f;
        p.reverbOn = true; p.rvDec = 48.0f; p.rvTone = 62.0f; p.rvMix = 32.0f;
        list.push_back (p);
    }
    {
        MiAmpBassPreset p;
        p.name = "Funk Phaser Groove";
        p.hint = "Compresor tight + Phaser profundo";
        p.ampGain = 52.0f; p.ampBass = 56.0f; p.ampMid = 44.0f; p.ampFreq = 64.0f; p.ampTreble = 62.0f; p.ampUltraHi = true;
        p.slots = { 0, 3, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.compOn = true; p.compSus = 64.0f; p.compAtt = 28.0f; p.compLvl = 58.0f;
        p.phaserOn = true; p.phRate = 44.0f; p.phDepth = 68.0f; p.phFb = 58.0f;
        list.push_back (p);
    }
    {
        MiAmpBassPreset p;
        p.name = "Full Rig Arena";
        p.hint = "Comp + XV Punk + Chorus + Reverb";
        p.ampGain = 54.0f; p.ampBass = 52.0f; p.ampMid = 50.0f; p.ampFreq = 52.0f; p.ampTreble = 55.0f;
        p.slots = { 0, 1, 2, 4, -1, -1, -1, -1, -1, -1 };
        p.compOn = true; p.compSus = 50.0f; p.compAtt = 40.0f; p.compLvl = 54.0f;
        p.xvOn = true; p.xvLvl = 56.0f; p.xvDrv = 48.0f; p.xvLow = 64.0f; p.xvHigh = 56.0f; p.xvBlend = 62.0f; p.xvMid = 30.0f; p.xvPres = 80.0f; p.xvCab = 1;
        p.chorusOn = true; p.chRate = 34.0f; p.chDepth = 45.0f; p.chMix = 30.0f;
        p.reverbOn = true; p.rvDec = 42.0f; p.rvTone = 55.0f; p.rvMix = 24.0f;
        list.push_back (p);
    }
    return list;
}

std::vector<MiAmpGuitarPreset> MiAmpAudioProcessor::getGuitarFactoryPresets()
{
    std::vector<MiAmpGuitarPreset> list;

    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 90s Cali Punk");
        p.channel = 2; p.cab = 1; p.bright = true; p.os4x = true;
        p.input = 56.0f; p.gate = 28.0f; p.output = 58.0f;
        p.gain = 68.0f; p.bass = 56.0f; p.mid = 60.0f; p.treble = 64.0f; p.presence = 66.0f; p.master = 60.0f;
        p.slots = { 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.driveOn = true; p.driveGain = 18.0f; p.driveTone = 66.0f; p.driveLvl = 64.0f;
        p.hint = "Dual High-Gain + Crunch Brit 90s + Cab 4x12 Modern";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 90s Cali Clean");
        p.channel = 0; p.cab = 2; p.bright = true; p.os4x = true;
        p.input = 52.0f; p.gate = 10.0f; p.output = 60.0f;
        p.gain = 28.0f; p.bass = 44.0f; p.mid = 54.0f; p.treble = 68.0f; p.presence = 62.0f; p.master = 64.0f;
        p.slots = { 0, 6, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.compOn = true; p.compSus = 52.0f; p.compAtt = 35.0f; p.compLvl = 56.0f;
        p.reverbOn = true; p.rvDec = 38.0f; p.rvTone = 64.0f; p.rvMix = 20.0f;
        p.hint = "Clean brillante con punch + Comp + Room";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 00s Punk Wide");
        p.channel = 2; p.cab = 1; p.bright = true; p.os4x = true;
        p.input = 56.0f; p.gate = 26.0f; p.output = 58.0f;
        p.gain = 74.0f; p.bass = 58.0f; p.mid = 56.0f; p.treble = 64.0f; p.presence = 66.0f; p.master = 58.0f;
        p.slots = { 1, 3, 5, 6, -1, -1, -1, -1, -1, -1 };
        p.driveOn = true; p.driveGain = 26.0f; p.driveTone = 64.0f; p.driveLvl = 64.0f;
        p.chorusOn = true; p.chRate = 34.0f; p.chDepth = 48.0f; p.chMix = 28.0f;
        p.delayOn = false; p.dlTime = 38.0f; p.dlFb = 30.0f; p.dlMix = 22.0f;
        p.reverbOn = true; p.rvDec = 46.0f; p.rvTone = 60.0f; p.rvMix = 22.0f;
        p.hint = "High-Gain + Overdrive + Chorus sutil";
        list.push_back (p);
    }

    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 Rock Crunch");
        p.channel = 1; p.cab = 1; p.bright = true; p.os4x = true;
        p.input = 52.0f; p.gate = 18.0f; p.output = 58.0f;
        p.gain = 58.0f; p.bass = 54.0f; p.mid = 60.0f; p.treble = 58.0f; p.presence = 56.0f; p.master = 58.0f;
        p.slots = { 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.driveOn = false; p.driveGain = 42.0f; p.driveTone = 58.0f; p.driveLvl = 62.0f;
        p.hint = "CH2 Crunch 70s clasico + Cab Modern 4x12";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 Pop-Punk Drive");
        p.channel = 2; p.cab = 1; p.bright = true; p.os4x = true;
        p.input = 55.0f; p.gate = 28.0f; p.output = 56.0f;
        p.gain = 72.0f; p.bass = 56.0f; p.mid = 62.0f; p.treble = 64.0f; p.presence = 62.0f; p.master = 56.0f;
        p.slots = { 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.driveOn = true; p.driveGain = 36.0f; p.driveTone = 64.0f; p.driveLvl = 64.0f;
        p.hint = "Drive empujando CH3 para acordes rapidos con definicion";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 70s Breakup");
        p.channel = 0; p.cab = 0; p.bright = true; p.os4x = true;
        p.input = 50.0f; p.gate = 12.0f; p.output = 60.0f;
        p.gain = 46.0f; p.bass = 52.0f; p.mid = 56.0f; p.treble = 58.0f; p.presence = 54.0f; p.master = 62.0f;
        p.slots = { 6, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.reverbOn = true; p.rvDec = 42.0f; p.rvTone = 58.0f; p.rvMix = 24.0f;
        p.hint = "Edge of breakup calido con Vintage 4x12 y Bright Cap";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 Glass Clean Chorus");
        p.channel = 0; p.cab = 2; p.bright = true; p.os4x = true;
        p.input = 48.0f; p.gate = 10.0f; p.output = 62.0f;
        p.gain = 26.0f; p.bass = 50.0f; p.mid = 52.0f; p.treble = 62.0f; p.presence = 58.0f; p.master = 64.0f;
        p.slots = { 0, 3, 6, -1, -1, -1, -1, -1, -1, -1 };
        p.compOn = true; p.compSus = 48.0f; p.compAtt = 42.0f; p.compLvl = 54.0f;
        p.chorusOn = true; p.chRate = 38.0f; p.chDepth = 56.0f; p.chMix = 46.0f;
        p.reverbOn = true; p.rvDec = 52.0f; p.rvTone = 64.0f; p.rvMix = 32.0f;
        p.hint = "Clean cristalino Open 2x12 + Comp + Chorus + Reverb";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 Hard Rock Lead");
        p.channel = 2; p.cab = 0; p.bright = true; p.os4x = true;
        p.input = 56.0f; p.gate = 32.0f; p.output = 56.0f;
        p.gain = 78.0f; p.bass = 54.0f; p.mid = 68.0f; p.treble = 60.0f; p.presence = 60.0f; p.master = 58.0f;
        p.slots = { 1, 5, 6, -1, -1, -1, -1, -1, -1, -1 };
        p.driveOn = true; p.driveGain = 48.0f; p.driveTone = 60.0f; p.driveLvl = 66.0f;
        p.delayOn = true; p.dlTime = 44.0f; p.dlFb = 34.0f; p.dlMix = 28.0f;
        p.reverbOn = true; p.rvDec = 46.0f; p.rvTone = 55.0f; p.rvMix = 24.0f;
        p.hint = "Solo con sustain largo, Delay y Vintage 4x12";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 Fuzz Stoner Wall");
        p.channel = 1; p.cab = 0; p.bright = false; p.os4x = true;
        p.input = 54.0f; p.gate = 28.0f; p.output = 55.0f;
        p.gain = 64.0f; p.bass = 66.0f; p.mid = 58.0f; p.treble = 50.0f; p.presence = 48.0f; p.master = 60.0f;
        p.slots = { 2, 4, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.fuzzOn = true; p.fuzzFuzz = 74.0f; p.fuzzTone = 48.0f; p.fuzzLvl = 58.0f;
        p.phaserOn = true; p.phRate = 28.0f; p.phDepth = 62.0f; p.phFb = 54.0f;
        p.hint = "Fuzz gordo + Phaser lento sobre CH2";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 Studio 1x12 Indie");
        p.channel = 1; p.cab = 3; p.bright = true; p.os4x = true;
        p.input = 50.0f; p.gate = 16.0f; p.output = 58.0f;
        p.gain = 48.0f; p.bass = 50.0f; p.mid = 62.0f; p.treble = 60.0f; p.presence = 58.0f; p.master = 58.0f;
        p.slots = { 4, 5, 6, -1, -1, -1, -1, -1, -1, -1 };
        p.phaserOn = true; p.phRate = 36.0f; p.phDepth = 45.0f; p.phFb = 40.0f;
        p.delayOn = true; p.dlTime = 38.0f; p.dlFb = 38.0f; p.dlMix = 34.0f;
        p.reverbOn = true; p.rvDec = 54.0f; p.rvTone = 60.0f; p.rvMix = 32.0f;
        p.hint = "Studio 1x12 enfocado en medios + modulacion y espacio";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"MI \u2022 Cab Off (IR Externo)");
        p.channel = 1; p.cab = 4; p.bright = true; p.os4x = true;
        p.input = 50.0f; p.gate = 20.0f; p.output = 56.0f;
        p.gain = 58.0f; p.bass = 54.0f; p.mid = 58.0f; p.treble = 58.0f; p.presence = 56.0f; p.master = 58.0f;
        p.slots = { 1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
        p.driveOn = false;
        p.hint = "Salida directa del cabezal sin cabina para usar tu IR propio";
        list.push_back (p);
    }
    {
        MiAmpGuitarPreset p;
        p.name = juce::String::fromUTF8 (u8"Directo \u2022 Dual Crunch Preamp");
        p.headBypass = true;
        p.slots = { 8, 5, 6, -1, -1, -1, -1, -1, -1, -1 };
        p.delayOn = true; p.dlTime = 40.0f; p.dlFb = 30.0f; p.dlMix = 22.0f;
        p.reverbOn = true; p.rvDec = 42.0f; p.rvTone = 58.0f; p.rvMix = 22.0f;
        p.hint = "SOLO PEDALES: Dual Crunch + Delay + Reverb sin cabezal";
        list.push_back (p);
    }

    return list;
}

void MiAmpAudioProcessor::applyPreset (const MiAmpBassPreset& p)
{
    setParamNormalized (MiAmpParams::ampPower,       1.0f);
    setParamNormalized (MiAmpParams::ampStandby,     0.0f);
    setParamNormalized (MiAmpParams::ampCabOn,       1.0f);
    setParamNormalized (MiAmpParams::bassHeadBypass, p.headBypass ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::ampGain,    p.ampGain);
    setParamNormalized (MiAmpParams::ampBass,    p.ampBass);
    setParamNormalized (MiAmpParams::ampMid,     p.ampMid);
    setParamNormalized (MiAmpParams::ampFreq,    p.ampFreq);
    setParamNormalized (MiAmpParams::ampTreble,  p.ampTreble);
    setParamNormalized (MiAmpParams::ampUltraHi, p.ampUltraHi ? 1.0f : 0.0f);

    for (int i = 0; i < 10; ++i)
        setParamNormalized (MiAmpParams::slot[i], static_cast<float> (p.slots[static_cast<size_t> (i)]));

    setParamNormalized (MiAmpParams::compOn,      p.compOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::compSustain, p.compSus);
    setParamNormalized (MiAmpParams::compAttack,  p.compAtt);
    setParamNormalized (MiAmpParams::compLevel,   p.compLvl);

    setParamNormalized (MiAmpParams::xvOn,       p.xvOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::xvLevel,    p.xvLvl);
    setParamNormalized (MiAmpParams::xvDrive,    p.xvDrv);
    setParamNormalized (MiAmpParams::xvLow,      p.xvLow);
    setParamNormalized (MiAmpParams::xvHigh,     p.xvHigh);
    setParamNormalized (MiAmpParams::xvBlend,    p.xvBlend);
    setParamNormalized (MiAmpParams::xvMid,      p.xvMid);
    setParamNormalized (MiAmpParams::xvPresence, p.xvPres);
    setParamNormalized (MiAmpParams::xvCab,      static_cast<float> (p.xvCab));

    setParamNormalized (MiAmpParams::chorusOn,    p.chorusOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::chorusRate,  p.chRate);
    setParamNormalized (MiAmpParams::chorusDepth, p.chDepth);
    setParamNormalized (MiAmpParams::chorusMix,   p.chMix);

    setParamNormalized (MiAmpParams::phaserOn,       p.phaserOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::phaserRate,     p.phRate);
    setParamNormalized (MiAmpParams::phaserDepth,    p.phDepth);
    setParamNormalized (MiAmpParams::phaserFeedback, p.phFb);

    setParamNormalized (MiAmpParams::reverbOn,    p.reverbOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::reverbDecay, p.rvDec);
    setParamNormalized (MiAmpParams::reverbTone,  p.rvTone);
    setParamNormalized (MiAmpParams::reverbMix,   p.rvMix);

    auto hasSlot = [&](int id) { return std::find (p.slots.begin(), p.slots.end(), id) != p.slots.end(); };
    setParamNormalized (MiAmpParams::bddiOn,       hasSlot (5)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::goldOn,       hasSlot (6)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::bfOn,         hasSlot (7)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::oliveOn,      hasSlot (8)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::bassFlangeOn, hasSlot (9)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::bassDelayOn,  hasSlot (10) ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::bassGateOn,   hasSlot (11) ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::bassOctOn,    hasSlot (12) ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::bassEqOn,     hasSlot (13) ? 1.0f : 0.0f);

    activeBassPresetName = p.name;
}

void MiAmpAudioProcessor::applyGuitarPreset (const MiAmpGuitarPreset& p)
{
    const int ch = std::clamp (p.channel, 0, 2);
    setParamNormalized (MiAmpParams::gtrPower,      1.0f);
    setParamNormalized (MiAmpParams::gtrStandby,    0.0f);
    setParamNormalized (MiAmpParams::gtrHeadBypass, p.headBypass ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrChannel,    static_cast<float> (ch));
    setParamNormalized (MiAmpParams::gtrCab,        static_cast<float> (std::clamp (p.cab, 0, 4)));
    setParamNormalized (MiAmpParams::gtrModeBright, p.bright ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrOs4x,       p.os4x ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrInput,      p.input);
    setParamNormalized (MiAmpParams::gtrGate,       p.gate);
    setParamNormalized (MiAmpParams::gtrOutput,     p.output);

    setParamNormalized (MiAmpParams::gtrChGain[ch],     p.gain);
    setParamNormalized (MiAmpParams::gtrChBass[ch],     p.bass);
    setParamNormalized (MiAmpParams::gtrChMid[ch],      p.mid);
    setParamNormalized (MiAmpParams::gtrChTreble[ch],   p.treble);
    setParamNormalized (MiAmpParams::gtrChPresence[ch], p.presence);
    setParamNormalized (MiAmpParams::gtrChMaster[ch],   p.master);

    for (int i = 0; i < 10; ++i)
        setParamNormalized (MiAmpParams::gtrSlot[i], static_cast<float> (p.slots[static_cast<size_t> (i)]));

    setParamNormalized (MiAmpParams::gtrCompOn,  p.compOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrCompSus, p.compSus);
    setParamNormalized (MiAmpParams::gtrCompAtt, p.compAtt);
    setParamNormalized (MiAmpParams::gtrCompLvl, p.compLvl);

    setParamNormalized (MiAmpParams::gtrDriveOn,   p.driveOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrDriveGain, p.driveGain);
    setParamNormalized (MiAmpParams::gtrDriveTone, p.driveTone);
    setParamNormalized (MiAmpParams::gtrDriveLvl,  p.driveLvl);

    setParamNormalized (MiAmpParams::gtrFuzzOn,   p.fuzzOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrFuzzFuzz, p.fuzzFuzz);
    setParamNormalized (MiAmpParams::gtrFuzzTone, p.fuzzTone);
    setParamNormalized (MiAmpParams::gtrFuzzLvl,  p.fuzzLvl);

    setParamNormalized (MiAmpParams::gtrChorusOn,    p.chorusOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrChorusRate,  p.chRate);
    setParamNormalized (MiAmpParams::gtrChorusDepth, p.chDepth);
    setParamNormalized (MiAmpParams::gtrChorusMix,   p.chMix);

    setParamNormalized (MiAmpParams::gtrPhaserOn,    p.phaserOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrPhaserRate,  p.phRate);
    setParamNormalized (MiAmpParams::gtrPhaserDepth, p.phDepth);
    setParamNormalized (MiAmpParams::gtrPhaserFb,    p.phFb);

    setParamNormalized (MiAmpParams::gtrDelayOn,   p.delayOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrDelayTime, p.dlTime);
    setParamNormalized (MiAmpParams::gtrDelayFb,   p.dlFb);
    setParamNormalized (MiAmpParams::gtrDelayMix,  p.dlMix);

    setParamNormalized (MiAmpParams::gtrReverbOn,   p.reverbOn ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrReverbDec,  p.rvDec);
    setParamNormalized (MiAmpParams::gtrReverbTone, p.rvTone);
    setParamNormalized (MiAmpParams::gtrReverbMix,  p.rvMix);

    auto hasSlot = [&](int id) { return std::find (p.slots.begin(), p.slots.end(), id) != p.slots.end(); };
    setParamNormalized (MiAmpParams::gtrDistOn,   hasSlot (7)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrDualOn,   hasSlot (8)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrFlangeOn, hasSlot (9)  ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrGateOn,   hasSlot (10) ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrOctOn,    hasSlot (11) ? 1.0f : 0.0f);
    setParamNormalized (MiAmpParams::gtrEqOn,     hasSlot (12) ? 1.0f : 0.0f);

    activeGuitarPresetName = p.name;
}

juce::File MiAmpAudioProcessor::getUserPresetsFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("PopupsAudio").getChildFile ("MiAmp");
    dir.createDirectory();
    return dir.getChildFile ("user_presets.xml");
}

void MiAmpAudioProcessor::loadUserPresetsFromDisk()
{
    userPresets.clear();
    guitarUserPresets.clear();
    auto file = getUserPresetsFile();
    if (! file.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (file))
    {
        for (auto* child : xml->getChildIterator())
        {
            if (child->hasTagName ("BassPreset"))
            {
                MiAmpBassPreset p;
                p.name       = child->getStringAttribute ("name", "Usuario");
                p.headBypass = child->getBoolAttribute ("headBypass", false);
                p.ampGain    = static_cast<float> (child->getDoubleAttribute ("ampGain", 50.0));
                p.ampBass    = static_cast<float> (child->getDoubleAttribute ("ampBass", 50.0));
                p.ampMid     = static_cast<float> (child->getDoubleAttribute ("ampMid", 50.0));
                p.ampFreq    = static_cast<float> (child->getDoubleAttribute ("ampFreq", 50.0));
                p.ampTreble  = static_cast<float> (child->getDoubleAttribute ("ampTreble", 50.0));
                p.ampUltraHi = child->getBoolAttribute ("ampUltraHi", false);
                for (int i = 0; i < 10; ++i)
                    p.slots[static_cast<size_t> (i)] = child->getIntAttribute ("slot" + juce::String (i), -1);
                p.compOn   = child->getBoolAttribute ("compOn", true);
                p.compSus  = static_cast<float> (child->getDoubleAttribute ("compSus", 50.0));
                p.compAtt  = static_cast<float> (child->getDoubleAttribute ("compAtt", 50.0));
                p.compLvl  = static_cast<float> (child->getDoubleAttribute ("compLvl", 50.0));
                p.xvOn     = child->getBoolAttribute ("xvOn", true);
                p.xvLvl    = static_cast<float> (child->getDoubleAttribute ("xvLvl", 56.0));
                p.xvDrv    = static_cast<float> (child->getDoubleAttribute ("xvDrv", 40.8));
                p.xvLow    = static_cast<float> (child->getDoubleAttribute ("xvLow", 65.4));
                p.xvHigh   = static_cast<float> (child->getDoubleAttribute ("xvHigh", 53.5));
                p.xvBlend  = static_cast<float> (child->getDoubleAttribute ("xvBlend", 57.6));
                p.xvMid    = static_cast<float> (child->getDoubleAttribute ("xvMid", 25.8));
                p.xvPres   = static_cast<float> (child->getDoubleAttribute ("xvPres", 80.4));
                p.xvCab    = child->getIntAttribute ("xvCab", 1);
                p.chorusOn = child->getBoolAttribute ("chorusOn", true);
                p.chRate   = static_cast<float> (child->getDoubleAttribute ("chRate", 50.0));
                p.chDepth  = static_cast<float> (child->getDoubleAttribute ("chDepth", 50.0));
                p.chMix    = static_cast<float> (child->getDoubleAttribute ("chMix", 50.0));
                p.phaserOn = child->getBoolAttribute ("phaserOn", true);
                p.phRate   = static_cast<float> (child->getDoubleAttribute ("phRate", 50.0));
                p.phDepth  = static_cast<float> (child->getDoubleAttribute ("phDepth", 50.0));
                p.phFb     = static_cast<float> (child->getDoubleAttribute ("phFb", 50.0));
                p.reverbOn = child->getBoolAttribute ("reverbOn", true);
                p.rvDec    = static_cast<float> (child->getDoubleAttribute ("rvDec", 50.0));
                p.rvTone   = static_cast<float> (child->getDoubleAttribute ("rvTone", 50.0));
                p.rvMix    = static_cast<float> (child->getDoubleAttribute ("rvMix", 50.0));
                userPresets.push_back (p);
            }
            else if (child->hasTagName ("GuitarPreset"))
            {
                MiAmpGuitarPreset p;
                p.name       = child->getStringAttribute ("name", "Usuario");
                p.headBypass = child->getBoolAttribute ("headBypass", false);
                p.channel    = child->getIntAttribute ("channel", 1);
                p.cab      = child->getIntAttribute ("cab", 1);
                p.bright   = child->getBoolAttribute ("bright", true);
                p.os4x     = child->getBoolAttribute ("os4x", true);
                p.input    = static_cast<float> (child->getDoubleAttribute ("input", 50.0));
                p.gate     = static_cast<float> (child->getDoubleAttribute ("gate", 18.0));
                p.output   = static_cast<float> (child->getDoubleAttribute ("output", 58.0));
                p.gain     = static_cast<float> (child->getDoubleAttribute ("gain", 58.0));
                p.bass     = static_cast<float> (child->getDoubleAttribute ("bass", 54.0));
                p.mid      = static_cast<float> (child->getDoubleAttribute ("mid", 58.0));
                p.treble   = static_cast<float> (child->getDoubleAttribute ("treble", 58.0));
                p.presence = static_cast<float> (child->getDoubleAttribute ("presence", 56.0));
                p.master   = static_cast<float> (child->getDoubleAttribute ("master", 58.0));
                for (int i = 0; i < 10; ++i)
                    p.slots[static_cast<size_t> (i)] = child->getIntAttribute ("slot" + juce::String (i), -1);
                p.compOn   = child->getBoolAttribute ("compOn", false);
                p.compSus  = static_cast<float> (child->getDoubleAttribute ("compSus", 50.0));
                p.compAtt  = static_cast<float> (child->getDoubleAttribute ("compAtt", 45.0));
                p.compLvl  = static_cast<float> (child->getDoubleAttribute ("compLvl", 52.0));
                p.driveOn  = child->getBoolAttribute ("driveOn", false);
                p.driveGain= static_cast<float> (child->getDoubleAttribute ("driveGain", 42.0));
                p.driveTone= static_cast<float> (child->getDoubleAttribute ("driveTone", 58.0));
                p.driveLvl = static_cast<float> (child->getDoubleAttribute ("driveLvl", 62.0));
                p.fuzzOn   = child->getBoolAttribute ("fuzzOn", false);
                p.fuzzFuzz = static_cast<float> (child->getDoubleAttribute ("fuzzFuzz", 65.0));
                p.fuzzTone = static_cast<float> (child->getDoubleAttribute ("fuzzTone", 52.0));
                p.fuzzLvl  = static_cast<float> (child->getDoubleAttribute ("fuzzLvl", 55.0));
                p.chorusOn = child->getBoolAttribute ("chorusOn", false);
                p.chRate   = static_cast<float> (child->getDoubleAttribute ("chRate", 42.0));
                p.chDepth  = static_cast<float> (child->getDoubleAttribute ("chDepth", 48.0));
                p.chMix    = static_cast<float> (child->getDoubleAttribute ("chMix", 44.0));
                p.phaserOn = child->getBoolAttribute ("phaserOn", false);
                p.phRate   = static_cast<float> (child->getDoubleAttribute ("phRate", 38.0));
                p.phDepth  = static_cast<float> (child->getDoubleAttribute ("phDepth", 55.0));
                p.phFb     = static_cast<float> (child->getDoubleAttribute ("phFb", 48.0));
                p.delayOn  = child->getBoolAttribute ("delayOn", false);
                p.dlTime   = static_cast<float> (child->getDoubleAttribute ("dlTime", 42.0));
                p.dlFb     = static_cast<float> (child->getDoubleAttribute ("dlFb", 36.0));
                p.dlMix    = static_cast<float> (child->getDoubleAttribute ("dlMix", 32.0));
                p.reverbOn = child->getBoolAttribute ("reverbOn", false);
                p.rvDec    = static_cast<float> (child->getDoubleAttribute ("rvDec", 46.0));
                p.rvTone   = static_cast<float> (child->getDoubleAttribute ("rvTone", 56.0));
                p.rvMix    = static_cast<float> (child->getDoubleAttribute ("rvMix", 28.0));
                guitarUserPresets.push_back (p);
            }
        }
    }
}

void MiAmpAudioProcessor::saveUserPresetsToDisk() const
{
    juce::XmlElement root ("MiAmpUserPresets");
    for (const auto& p : userPresets)
    {
        auto* e = root.createNewChildElement ("BassPreset");
        e->setAttribute ("name", p.name);
        e->setAttribute ("headBypass", p.headBypass);
        e->setAttribute ("ampGain", p.ampGain); e->setAttribute ("ampBass", p.ampBass);
        e->setAttribute ("ampMid", p.ampMid);   e->setAttribute ("ampFreq", p.ampFreq);
        e->setAttribute ("ampTreble", p.ampTreble); e->setAttribute ("ampUltraHi", p.ampUltraHi);
        for (int i = 0; i < 10; ++i)
            e->setAttribute ("slot" + juce::String (i), p.slots[static_cast<size_t> (i)]);
        e->setAttribute ("compOn", p.compOn); e->setAttribute ("compSus", p.compSus);
        e->setAttribute ("compAtt", p.compAtt); e->setAttribute ("compLvl", p.compLvl);
        e->setAttribute ("xvOn", p.xvOn); e->setAttribute ("xvLvl", p.xvLvl);
        e->setAttribute ("xvDrv", p.xvDrv); e->setAttribute ("xvLow", p.xvLow);
        e->setAttribute ("xvHigh", p.xvHigh); e->setAttribute ("xvBlend", p.xvBlend);
        e->setAttribute ("xvMid", p.xvMid); e->setAttribute ("xvPres", p.xvPres);
        e->setAttribute ("xvCab", p.xvCab);
        e->setAttribute ("chorusOn", p.chorusOn); e->setAttribute ("chRate", p.chRate);
        e->setAttribute ("chDepth", p.chDepth); e->setAttribute ("chMix", p.chMix);
        e->setAttribute ("phaserOn", p.phaserOn); e->setAttribute ("phRate", p.phRate);
        e->setAttribute ("phDepth", p.phDepth); e->setAttribute ("phFb", p.phFb);
        e->setAttribute ("reverbOn", p.reverbOn); e->setAttribute ("rvDec", p.rvDec);
        e->setAttribute ("rvTone", p.rvTone); e->setAttribute ("rvMix", p.rvMix);
    }
    for (const auto& p : guitarUserPresets)
    {
        auto* e = root.createNewChildElement ("GuitarPreset");
        e->setAttribute ("name", p.name);
        e->setAttribute ("headBypass", p.headBypass);
        e->setAttribute ("channel", p.channel); e->setAttribute ("cab", p.cab);
        e->setAttribute ("bright", p.bright);   e->setAttribute ("os4x", p.os4x);
        e->setAttribute ("input", p.input);     e->setAttribute ("gate", p.gate);
        e->setAttribute ("output", p.output);
        e->setAttribute ("gain", p.gain);       e->setAttribute ("bass", p.bass);
        e->setAttribute ("mid", p.mid);         e->setAttribute ("treble", p.treble);
        e->setAttribute ("presence", p.presence); e->setAttribute ("master", p.master);
        for (int i = 0; i < 10; ++i)
            e->setAttribute ("slot" + juce::String (i), p.slots[static_cast<size_t> (i)]);
        e->setAttribute ("compOn", p.compOn); e->setAttribute ("compSus", p.compSus);
        e->setAttribute ("compAtt", p.compAtt); e->setAttribute ("compLvl", p.compLvl);
        e->setAttribute ("driveOn", p.driveOn); e->setAttribute ("driveGain", p.driveGain);
        e->setAttribute ("driveTone", p.driveTone); e->setAttribute ("driveLvl", p.driveLvl);
        e->setAttribute ("fuzzOn", p.fuzzOn); e->setAttribute ("fuzzFuzz", p.fuzzFuzz);
        e->setAttribute ("fuzzTone", p.fuzzTone); e->setAttribute ("fuzzLvl", p.fuzzLvl);
        e->setAttribute ("chorusOn", p.chorusOn); e->setAttribute ("chRate", p.chRate);
        e->setAttribute ("chDepth", p.chDepth); e->setAttribute ("chMix", p.chMix);
        e->setAttribute ("phaserOn", p.phaserOn); e->setAttribute ("phRate", p.phRate);
        e->setAttribute ("phDepth", p.phDepth); e->setAttribute ("phFb", p.phFb);
        e->setAttribute ("delayOn", p.delayOn); e->setAttribute ("dlTime", p.dlTime);
        e->setAttribute ("dlFb", p.dlFb); e->setAttribute ("dlMix", p.dlMix);
        e->setAttribute ("reverbOn", p.reverbOn); e->setAttribute ("rvDec", p.rvDec);
        e->setAttribute ("rvTone", p.rvTone); e->setAttribute ("rvMix", p.rvMix);
    }
    root.writeTo (getUserPresetsFile());
}

void MiAmpAudioProcessor::saveUserPreset (const juce::String& name)
{
    if (isBassMode())
    {
        auto s = readBassSnapshot();
        MiAmpBassPreset p;
        p.name = name.trim().isEmpty() ? ("Bajo " + juce::String (userPresets.size() + 1)) : name.trim();
        p.headBypass = s.headBypass;
        p.ampGain = s.ampGain; p.ampBass = s.ampBass; p.ampMid = s.ampMid; p.ampFreq = s.ampFreq; p.ampTreble = s.ampTreble;
        p.ampUltraHi = s.ampUltraHi; p.slots = s.chainSlots;
        p.compOn = s.compOn; p.compSus = s.compSustain; p.compAtt = s.compAttack; p.compLvl = s.compLevel;
        p.xvOn = s.xvOn; p.xvLvl = s.xvLevel; p.xvDrv = s.xvDrive; p.xvLow = s.xvLow; p.xvHigh = s.xvHigh;
        p.xvBlend = s.xvBlend; p.xvMid = s.xvMid; p.xvPres = s.xvPresence; p.xvCab = s.xvCab;
        p.chorusOn = s.chorusOn; p.chRate = s.chorusRate; p.chDepth = s.chorusDepth; p.chMix = s.chorusMix;
        p.phaserOn = s.phaserOn; p.phRate = s.phaserRate; p.phDepth = s.phaserDepth; p.phFb = s.phaserFeedback;
        p.reverbOn = s.reverbOn; p.rvDec = s.reverbDecay; p.rvTone = s.reverbTone; p.rvMix = s.reverbMix;
        userPresets.push_back (p);
        activeBassPresetName = p.name;
    }
    else
    {
        auto s = readGuitarSnapshot();
        const size_t ch = static_cast<size_t> (std::clamp (s.activeChannel, 0, 2));
        MiAmpGuitarPreset p;
        p.name = name.trim().isEmpty() ? ("Guitarra " + juce::String (guitarUserPresets.size() + 1)) : name.trim();
        p.headBypass = s.headBypass;
        p.channel = static_cast<int> (ch); p.cab = s.cabIndex; p.bright = s.ampModeBright; p.os4x = s.os4x;
        p.input = s.inputGain; p.gate = s.gateThresh; p.output = s.outputKnob;
        p.gain = s.chGain[ch]; p.bass = s.chBass[ch]; p.mid = s.chMid[ch];
        p.treble = s.chTreble[ch]; p.presence = s.chPresence[ch]; p.master = s.chMaster[ch];
        p.slots = s.chainSlots;
        p.compOn = s.compOn; p.compSus = s.compSustain; p.compAtt = s.compAttack; p.compLvl = s.compLevel;
        p.driveOn = s.driveOn; p.driveGain = s.driveGain; p.driveTone = s.driveTone; p.driveLvl = s.driveLevel;
        p.fuzzOn = s.fuzzOn; p.fuzzFuzz = s.fuzzFuzz; p.fuzzTone = s.fuzzTone; p.fuzzLvl = s.fuzzLevel;
        p.chorusOn = s.chorusOn; p.chRate = s.chorusRate; p.chDepth = s.chorusDepth; p.chMix = s.chorusMix;
        p.phaserOn = s.phaserOn; p.phRate = s.phaserRate; p.phDepth = s.phaserDepth; p.phFb = s.phaserFeedback;
        p.delayOn = s.delayOn; p.dlTime = s.delayTime; p.dlFb = s.delayFeedback; p.dlMix = s.delayMix;
        p.reverbOn = s.reverbOn; p.rvDec = s.reverbDecay; p.rvTone = s.reverbTone; p.rvMix = s.reverbMix;
        guitarUserPresets.push_back (p);
        activeGuitarPresetName = p.name;
    }
    saveUserPresetsToDisk();
}

void MiAmpAudioProcessor::deleteUserPreset (int index)
{
    if (isBassMode())
    {
        if (index >= 0 && index < static_cast<int> (userPresets.size()))
            userPresets.erase (userPresets.begin() + index);
    }
    else
    {
        if (index >= 0 && index < static_cast<int> (guitarUserPresets.size()))
            guitarUserPresets.erase (guitarUserPresets.begin() + index);
    }
    saveUserPresetsToDisk();
}

void MiAmpAudioProcessor::loadUserPreset (int index)
{
    if (isBassMode())
    {
        if (index >= 0 && index < static_cast<int> (userPresets.size()))
            applyPreset (userPresets[static_cast<size_t> (index)]);
    }
    else
    {
        if (index >= 0 && index < static_cast<int> (guitarUserPresets.size()))
            applyGuitarPreset (guitarUserPresets[static_cast<size_t> (index)]);
    }
}

int MiAmpAudioProcessor::getNumPrograms()
{
    return static_cast<int> (guitarFactoryPresets.size() + factoryPresets.size());
}

void MiAmpAudioProcessor::setCurrentProgram (int index)
{
    const int nG = static_cast<int> (guitarFactoryPresets.size());
    if (index >= 0 && index < nG)
    {
        currentProgramIndex = index;
        setInstrumentMode (false);
        applyGuitarPreset (guitarFactoryPresets[static_cast<size_t> (index)]);
    }
    else if (index >= nG && index < nG + static_cast<int> (factoryPresets.size()))
    {
        currentProgramIndex = index;
        setInstrumentMode (true);
        applyPreset (factoryPresets[static_cast<size_t> (index - nG)]);
    }
}

const juce::String MiAmpAudioProcessor::getProgramName (int index)
{
    const int nG = static_cast<int> (guitarFactoryPresets.size());
    if (index >= 0 && index < nG)
        return guitarFactoryPresets[static_cast<size_t> (index)].name;
    if (index >= nG && index < nG + static_cast<int> (factoryPresets.size()))
        return factoryPresets[static_cast<size_t> (index - nG)].name;
    return {};
}

void MiAmpAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("activeBassPresetName", activeBassPresetName);
    xml->setAttribute ("activeGuitarPresetName", activeGuitarPresetName);
    xml->setAttribute ("customGtrIrPath", customGtrIrPath);
    xml->setAttribute ("customBassIrPath", customBassIrPath);
    copyXmlToBinary (*xml, destData);
}

void MiAmpAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml && xml->hasTagName (apvts.state.getType()))
    {
        activeBassPresetName   = xml->getStringAttribute ("activeBassPresetName", activeBassPresetName);
        activeGuitarPresetName = xml->getStringAttribute ("activeGuitarPresetName", activeGuitarPresetName);
        const juce::String gPath = xml->getStringAttribute ("customGtrIrPath", "");
        const juce::String bPath = xml->getStringAttribute ("customBassIrPath", "");
        if (gPath.isNotEmpty() && juce::File (gPath).existsAsFile())
            loadCustomCabIR (false, juce::File (gPath));
        if (bPath.isNotEmpty() && juce::File (bPath).existsAsFile())
            loadCustomCabIR (true, juce::File (bPath));
        apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }
}

juce::String MiAmpAudioProcessor::exportStateAsXmlString (const juce::String& presetName)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (! xml)
        return {};
    xml->setAttribute ("exportedPresetName", presetName);
    xml->setAttribute ("activeBassPresetName", isBassMode() ? presetName : activeBassPresetName);
    xml->setAttribute ("activeGuitarPresetName", isBassMode() ? activeGuitarPresetName : presetName);
    return xml->toString();
}

bool MiAmpAudioProcessor::importStateFromXmlString (const juce::String& xmlText)
{
    std::unique_ptr<juce::XmlElement> xml (juce::XmlDocument::parse (xmlText));
    if (! xml || ! xml->hasTagName (apvts.state.getType()))
        return false;
    activeBassPresetName   = xml->getStringAttribute ("activeBassPresetName", activeBassPresetName);
    activeGuitarPresetName = xml->getStringAttribute ("activeGuitarPresetName", activeGuitarPresetName);
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    const juce::String impName = xml->getStringAttribute ("exportedPresetName", "").trim();
    if (impName.isNotEmpty())
        saveUserPreset (impName);
    return true;
}

juce::AudioProcessorEditor* MiAmpAudioProcessor::createEditor()
{
    return new MiAmpAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MiAmpAudioProcessor();
}
