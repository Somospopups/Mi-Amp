#pragma once
#include <JuceHeader.h>
#include "DSP/MiAmpBassChainDSP.h"
#include "DSP/MiAmpGuitarChainDSP.h"
#include "DSP/XVBassTuner.h"

namespace MiAmpParams
{
    inline constexpr const char* instrument  = "instrument_mode"; // 0 = Guitarra, 1 = Bajo

    // Bass Head
    inline constexpr const char* ampPower    = "amp_power";
    inline constexpr const char* ampStandby  = "amp_standby";
    inline constexpr const char* ampUltraHi  = "amp_ultra_hi";
    inline constexpr const char* ampCabOn    = "amp_cab_on";
    inline constexpr const char* ampGain     = "amp_gain";
    inline constexpr const char* ampBass     = "amp_bass";
    inline constexpr const char* ampMid      = "amp_mid";
    inline constexpr const char* ampFreq     = "amp_freq";
    inline constexpr const char* ampTreble   = "amp_treble";

    // Shared Drop Pitch Shifter & Tuner
    inline constexpr const char* pitchOn     = "pitch_on";
    inline constexpr const char* pitchSemi   = "pitch_semi";
    inline constexpr const char* pitchCents  = "pitch_cents";
    inline constexpr const char* pitchMix    = "pitch_mix";
    inline constexpr const char* tunerMuted  = "tuner_muted";

    // Bass Pedalboard Slots (-1 = Empty, 0 = Compresor, 1 = XV, 2 = Chorus, 3 = Phaser, 4 = Reverb)
    inline constexpr const char* slot0       = "slot_0";
    inline constexpr const char* slot1       = "slot_1";
    inline constexpr const char* slot2       = "slot_2";
    inline constexpr const char* slot3       = "slot_3";
    inline constexpr const char* slot4       = "slot_4";

    // Bass Pedals
    inline constexpr const char* compOn      = "comp_on";
    inline constexpr const char* compSustain = "comp_sustain";
    inline constexpr const char* compAttack  = "comp_attack";
    inline constexpr const char* compLevel   = "comp_level";

    inline constexpr const char* xvOn        = "xv_on";
    inline constexpr const char* xvLevel     = "xv_level";
    inline constexpr const char* xvDrive     = "xv_drive";
    inline constexpr const char* xvLow       = "xv_low";
    inline constexpr const char* xvHigh      = "xv_high";
    inline constexpr const char* xvBlend     = "xv_blend";
    inline constexpr const char* xvMid       = "xv_mid";
    inline constexpr const char* xvPresence  = "xv_presence";
    inline constexpr const char* xvCab       = "xv_cab";

    inline constexpr const char* chorusOn    = "chorus_on";
    inline constexpr const char* chorusRate  = "chorus_rate";
    inline constexpr const char* chorusDepth = "chorus_depth";
    inline constexpr const char* chorusMix   = "chorus_mix";

    inline constexpr const char* phaserOn       = "phaser_on";
    inline constexpr const char* phaserRate     = "phaser_rate";
    inline constexpr const char* phaserDepth    = "phaser_depth";
    inline constexpr const char* phaserFeedback = "phaser_feedback";

    inline constexpr const char* reverbOn    = "reverb_on";
    inline constexpr const char* reverbDecay = "reverb_decay";
    inline constexpr const char* reverbTone  = "reverb_tone";
    inline constexpr const char* reverbMix   = "reverb_mix";

    // Guitar Head (Zenbox Edition)
    inline constexpr const char* gtrInput      = "gtr_input";
    inline constexpr const char* gtrGate       = "gtr_gate";
    inline constexpr const char* gtrOs4x       = "gtr_os4x";
    inline constexpr const char* gtrCab        = "gtr_cab"; // 0..3 Cabs, 4 = OFF
    inline constexpr const char* gtrPower      = "gtr_power";
    inline constexpr const char* gtrStandby    = "gtr_standby";
    inline constexpr const char* gtrModeBright = "gtr_mode_bright";
    inline constexpr const char* gtrChannel    = "gtr_channel"; // 0=CH1, 1=CH2, 2=CH3
    inline constexpr const char* gtrOutput     = "gtr_output";

    inline constexpr const char* gtrChGain[3]     = { "gtr_ch1_gain",     "gtr_ch2_gain",     "gtr_ch3_gain" };
    inline constexpr const char* gtrChBass[3]     = { "gtr_ch1_bass",     "gtr_ch2_bass",     "gtr_ch3_bass" };
    inline constexpr const char* gtrChMid[3]      = { "gtr_ch1_mid",      "gtr_ch2_mid",      "gtr_ch3_mid" };
    inline constexpr const char* gtrChTreble[3]   = { "gtr_ch1_treble",   "gtr_ch2_treble",   "gtr_ch3_treble" };
    inline constexpr const char* gtrChPresence[3] = { "gtr_ch1_presence", "gtr_ch2_presence", "gtr_ch3_presence" };
    inline constexpr const char* gtrChMaster[3]   = { "gtr_ch1_master",   "gtr_ch2_master",   "gtr_ch3_master" };

    // Guitar Pedalboard Slots (-1 = Empty, 0=Comp, 1=Drive, 2=Fuzz, 3=Chorus, 4=Phaser, 5=Delay, 6=Reverb)
    inline constexpr const char* gtrSlot[7] = {
        "gtr_slot_0", "gtr_slot_1", "gtr_slot_2", "gtr_slot_3", "gtr_slot_4", "gtr_slot_5", "gtr_slot_6"
    };

    inline constexpr const char* gtrCompOn = "gtr_comp_on", *gtrCompSus = "gtr_comp_sus", *gtrCompAtt = "gtr_comp_att", *gtrCompLvl = "gtr_comp_lvl";
    inline constexpr const char* gtrDriveOn = "gtr_drive_on", *gtrDriveGain = "gtr_drive_gain", *gtrDriveTone = "gtr_drive_tone", *gtrDriveLvl = "gtr_drive_lvl";
    inline constexpr const char* gtrFuzzOn = "gtr_fuzz_on", *gtrFuzzFuzz = "gtr_fuzz_fuzz", *gtrFuzzTone = "gtr_fuzz_tone", *gtrFuzzLvl = "gtr_fuzz_lvl";
    inline constexpr const char* gtrChorusOn = "gtr_chorus_on", *gtrChorusRate = "gtr_chorus_rate", *gtrChorusDepth = "gtr_chorus_depth", *gtrChorusMix = "gtr_chorus_mix";
    inline constexpr const char* gtrPhaserOn = "gtr_phaser_on", *gtrPhaserRate = "gtr_phaser_rate", *gtrPhaserDepth = "gtr_phaser_depth", *gtrPhaserFb = "gtr_phaser_fb";
    inline constexpr const char* gtrDelayOn = "gtr_delay_on", *gtrDelayTime = "gtr_delay_time", *gtrDelayFb = "gtr_delay_fb", *gtrDelayMix = "gtr_delay_mix";
    inline constexpr const char* gtrReverbOn = "gtr_reverb_on", *gtrReverbDec = "gtr_reverb_dec", *gtrReverbTone = "gtr_reverb_tone", *gtrReverbMix = "gtr_reverb_mix";
}

struct MiAmpBassPreset
{
    juce::String name;
    bool  isXvFactory = false;
    float ampGain = 50.0f, ampBass = 50.0f, ampMid = 50.0f, ampFreq = 50.0f, ampTreble = 50.0f;
    bool  ampUltraHi = false;
    std::array<int, 5> slots { 1, -1, -1, -1, -1 };

    bool  compOn = true; float compSus = 50.0f, compAtt = 50.0f, compLvl = 50.0f;
    bool  xvOn = true; float xvLvl = 56.0f, xvDrv = 40.8f, xvLow = 65.4f, xvHigh = 53.5f, xvBlend = 57.6f, xvMid = 25.8f, xvPres = 80.4f; int xvCab = 1;
    bool  chorusOn = true; float chRate = 50.0f, chDepth = 50.0f, chMix = 50.0f;
    bool  phaserOn = true; float phRate = 50.0f, phDepth = 50.0f, phFb = 50.0f;
    bool  reverbOn = true; float rvDec = 50.0f, rvTone = 50.0f, rvMix = 50.0f;

    juce::String hint;
};

struct MiAmpGuitarPreset
{
    juce::String name;
    int   channel = 1; // 0..2
    int   cab = 1;     // 0..4
    bool  bright = true;
    bool  os4x = true;
    float input = 50.0f, gate = 18.0f, output = 58.0f;
    float gain = 56.0f, bass = 54.0f, mid = 58.0f, treble = 58.0f, presence = 56.0f, master = 58.0f;
    std::array<int, 7> slots { 1, -1, -1, -1, -1, -1, -1 };
    bool  compOn = false;  float compSus = 50.0f, compAtt = 45.0f, compLvl = 52.0f;
    bool  driveOn = false; float driveGain = 42.0f, driveTone = 58.0f, driveLvl = 62.0f;
    bool  fuzzOn = false;  float fuzzFuzz = 65.0f, fuzzTone = 52.0f, fuzzLvl = 55.0f;
    bool  chorusOn = false; float chRate = 42.0f, chDepth = 48.0f, chMix = 44.0f;
    bool  phaserOn = false; float phRate = 38.0f, phDepth = 55.0f, phFb = 48.0f;
    bool  delayOn = false;  float dlTime = 42.0f, dlFb = 36.0f, dlMix = 32.0f;
    bool  reverbOn = false; float rvDec = 46.0f, rvTone = 56.0f, rvMix = 28.0f;
    juce::String hint;
};

class MiAmpAudioProcessor final : public juce::AudioProcessor
{
public:
    MiAmpAudioProcessor();
    ~MiAmpAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgramIndex; }
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    bool isBassMode() const noexcept;
    void setInstrumentMode (bool bass);

    // Unified chain management (dispatches based on active instrument or explicit mode)
    std::vector<int> getActiveChain (bool forBass) const;
    void setActiveChain (bool forBass, const std::vector<int>& order);
    bool addPedalToChain (bool forBass, int pedalType, int insertAt = -1);
    void removePedalFromChain (bool forBass, int pedalType);
    void movePedalInChain (bool forBass, int pedalType, int delta);
    void reorderPedalToIndex (bool forBass, int pedalType, int targetIndex);

    // Legacy Bass chain wrappers
    std::vector<int> getActiveChain() const { return getActiveChain (true); }
    bool addPedalToChain (int pedalType)    { return addPedalToChain (true, pedalType, -1); }
    void removePedalFromChain (int pedalType) { removePedalFromChain (true, pedalType); }
    void movePedalInChain (int pedalType, int delta) { movePedalInChain (true, pedalType, delta); }

    // Presets
    static std::vector<MiAmpBassPreset> getFactoryPresets();
    static std::vector<MiAmpGuitarPreset> getGuitarFactoryPresets();
    void applyPreset (const MiAmpBassPreset& p);
    void applyGuitarPreset (const MiAmpGuitarPreset& p);
    juce::String getActivePresetName() const { return isBassMode() ? activeBassPresetName : activeGuitarPresetName; }

    const std::vector<MiAmpBassPreset>& getUserPresets() const { return userPresets; }
    const std::vector<MiAmpGuitarPreset>& getGuitarUserPresets() const { return guitarUserPresets; }
    void saveUserPreset (const juce::String& name);
    void deleteUserPreset (int index);
    void loadUserPreset (int index);

    xvdsp::XVBassTuner tuner;
    std::atomic<float> inputRms  { 0.0f };
    std::atomic<float> outputRms { 0.0f };
    std::atomic<bool>  isClipping { false };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    miamp::BassControlSnapshot readBassSnapshot() const noexcept;
    zenbox::GuitarControlSnapshot readGuitarSnapshot() const noexcept;
    static float dbToXvKnob (float db) noexcept { return std::clamp (((db + 12.0f) / 24.0f) * 100.0f, 0.0f, 100.0f); }
    static juce::File getUserPresetsFile();
    void loadUserPresetsFromDisk();
    void saveUserPresetsToDisk() const;
    void setParamNormalized (const char* id, float plainVal);

    miamp::MiAmpBassChainDSP bassDsp;
    zenbox::MiAmpGuitarChainDSP guitarDsp;
    std::vector<float> monoScratch;
    std::vector<MiAmpBassPreset> factoryPresets;
    std::vector<MiAmpGuitarPreset> guitarFactoryPresets;
    std::vector<MiAmpBassPreset> userPresets;
    std::vector<MiAmpGuitarPreset> guitarUserPresets;
    juce::String activeBassPresetName   { "Inicial (XV Pop-Punk)" };
    juce::String activeGuitarPresetName { juce::String::fromUTF8 (u8"Tom DeLonge \u2022 Enema Dirt") };
    int currentProgramIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiAmpAudioProcessor)
};
