#include "StandaloneAudioBridge.h"

namespace miamp_standalone
{
    bool isRunningStandalone() noexcept       { return false; }
    bool isAudioInputMuted() noexcept         { return false; }
    bool isStandaloneInputAllowed() noexcept  { return true; }
    bool isInputBlockedNoInterface() noexcept { return false; }
    bool wasInputMutedForFeedback() noexcept  { return false; }
    void muteInputForSafety (bool) noexcept   {}
    void ensureAudioInputActive() {}
    void openAudioSettingsDialog() {}
    juce::String getCurrentAudioStatusSummary() { return {}; }
    juce::String popAudioAutoSwitchNotice()     { return {}; }
}
