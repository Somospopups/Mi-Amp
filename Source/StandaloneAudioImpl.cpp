#include "StandaloneAudioBridge.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace miamp_standalone
{

bool isRunningStandalone() noexcept
{
    return true;
}

void ensureAudioInputActive()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    // JUCE Standalone mutes audio input by default to guard against laptop mic feedback.
    // For a guitar/bass amp standalone app, unmute input automatically so the instrument is heard!
    if (static_cast<bool> (holder->getMuteInputValue().getValue()))
    {
        holder->getMuteInputValue().setValue (false);
    }

    auto& dm = holder->deviceManager;
    auto setup = dm.getAudioDeviceSetup();
    auto* dev = dm.getCurrentAudioDevice();

    const bool hasInputDev  = (dev != nullptr && dev->getInputChannelNames().size() > 0);
    const bool hasActiveIns = (setup.inputChannels.countNumberOfSetBits() > 0);

    if (! hasInputDev || ! hasActiveIns)
    {
        // Try enabling default input channels on the current device first
        setup.useDefaultInputChannels = true;
        if (setup.inputChannels.countNumberOfSetBits() == 0)
            setup.inputChannels.setRange (0, 2, true);

        const auto err = dm.setAudioDeviceSetup (setup, true);
        if (err.isNotEmpty() || dm.getCurrentAudioDevice() == nullptr
            || dm.getAudioDeviceSetup().inputChannels.countNumberOfSetBits() == 0)
        {
            // Fallback: re-initialise with 2 inputs / 2 outputs using OS default devices
            dm.initialise (2, 2, nullptr, true);
        }
    }

    holder->saveAudioDeviceState();
}

void openAudioSettingsDialog()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->getMuteInputValue().setValue (false);
        holder->showAudioSettingsDialog();
    }
}

juce::String getCurrentAudioStatusSummary()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return {};

    auto* dev = holder->deviceManager.getCurrentAudioDevice();
    if (dev == nullptr)
        return "Audio: Sin dispositivo activo";

    const int ins = dev->getActiveInputChannels().countNumberOfSetBits();
    if (ins <= 0)
        return "Audio: " + dev->getName() + " (Sin entrada)";

    const int srKhz = static_cast<int> (std::lround (dev->getCurrentSampleRate() / 1000.0));
    return dev->getName() + " (" + juce::String (ins) + " in / " + juce::String (srKhz) + "k)";
}

} // namespace miamp_standalone
