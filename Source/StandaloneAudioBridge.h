#pragma once
#include <JuceHeader.h>

namespace miamp_standalone
{
    bool isRunningStandalone() noexcept;
    bool isAudioInputMuted() noexcept;
    void ensureAudioInputActive();
    void openAudioSettingsDialog();
    juce::String getCurrentAudioStatusSummary();
}
