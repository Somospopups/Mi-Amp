#include "StandaloneAudioBridge.h"

namespace miamp_standalone
{
    bool isRunningStandalone() noexcept { return false; }
    void ensureAudioInputActive() {}
    void openAudioSettingsDialog() {}
    juce::String getCurrentAudioStatusSummary() { return {}; }
}
