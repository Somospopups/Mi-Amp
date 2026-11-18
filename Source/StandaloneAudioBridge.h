#pragma once
#include <JuceHeader.h>

namespace miamp_standalone
{
    bool isRunningStandalone() noexcept;
    bool isAudioInputMuted() noexcept;
    bool isStandaloneInputAllowed() noexcept;
    bool isInputBlockedNoInterface() noexcept;
    bool wasInputMutedForFeedback() noexcept;
    void muteInputForSafety (bool feedbackDetected) noexcept;
    void ensureAudioInputActive();
    void openAudioSettingsDialog();
    juce::String getCurrentAudioStatusSummary();
    juce::String popAudioAutoSwitchNotice();
}
