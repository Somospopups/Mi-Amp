#pragma once
#include <JuceHeader.h>
#include "DSP/MiAmpBassChainDSP.h"
#include "DSP/XVBassTuner.h"

namespace MiAmpParams
{
    inline constexpr const char* ampPower    = "amp_power";
    inline constexpr const char* ampStandby  = "amp_standby";
    inline constexpr const char* ampUltraHi  = "amp_ultra_hi";
    inline constexpr const char* ampCabOn    = "amp_cab_on";
    inline constexpr const char* ampGain     = "amp_gain";
    inline constexpr const char* ampBass     = "amp_bass";
    inline constexpr const char* ampMid      = "amp_mid";
    inline constexpr const char* ampFreq     = "amp_freq";
    inline constexpr const char* ampTreble   = "amp_treble";

    inline constexpr const char* pitchOn     = "pitch_on";
    inline constexpr const char* pitchSemi   = "pitch_semi";
    inline constexpr const char* pitchCents  = "pitch_cents";
    inline constexpr const char* pitchMix    = "pitch_mix";
    inline constexpr const char* tunerMuted  = "tuner_muted";

    inline constexpr const char* slot0       = "slot_0";
    inline constexpr const char* slot1       = "slot_1";
    inline constexpr const char* slot2       = "slot_2";
    inline constexpr const char* slot3       = "slot_3";
    inline constexpr const char* slot4       = "slot_4";

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

    // Chain management helpers for Editor
    std::vector<int> getActiveChain() const;
    void setActiveChain (const std::vector<int>& order);
    bool addPedalToChain (int pedalType);
    void removePedalFromChain (int pedalType);
    void movePedalInChain (int pedalType, int delta);

    // Presets
    static std::vector<MiAmpBassPreset> getFactoryPresets();
    void applyPreset (const MiAmpBassPreset& p);
    juce::String getActivePresetName() const { return activePresetName; }

    const std::vector<MiAmpBassPreset>& getUserPresets() const { return userPresets; }
    void saveUserPreset (const juce::String& name);
    void deleteUserPreset (int index);
    void loadUserPreset (int index);

    // Live metering & tuner
    xvdsp::XVBassTuner tuner;
    std::atomic<float> inputRms  { 0.0f };
    std::atomic<float> outputRms { 0.0f };
    std::atomic<bool>  isClipping { false };

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    miamp::BassControlSnapshot readSnapshot() const noexcept;
    static float dbToXvKnob (float db) noexcept { return std::clamp (((db + 12.0f) / 24.0f) * 100.0f, 0.0f, 100.0f); }
    static juce::File getUserPresetsFile();
    void loadUserPresetsFromDisk();
    void saveUserPresetsToDisk() const;
    void setParamNormalized (const char* id, float plainVal);

    miamp::MiAmpBassChainDSP bassDsp;
    std::vector<float> monoScratch;
    std::vector<MiAmpBassPreset> factoryPresets;
    std::vector<MiAmpBassPreset> userPresets;
    juce::String activePresetName { "XV" };
    int currentProgramIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MiAmpAudioProcessor)
};
