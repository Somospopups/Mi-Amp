#pragma once
#include <JuceHeader.h>

namespace miamp_standalone
{
    bool isRunningStandalone() noexcept;
    void ensureAudioInputActive();
    void openAudioSettingsDialog();
    juce::String getCurrentAudioStatusSummary();
}
