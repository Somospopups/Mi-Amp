#include "StandaloneAudioBridge.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

extern juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter();

#include <juce_audio_plugin_client/juce_audio_plugin_client.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace miamp_standalone
{
namespace
{
    bool isExplicitLineIn (const juce::String& name)
    {
        const auto low = name.toLowerCase();
        return low.contains ("line in") || low.contains ("line-in")
            || low.contains ("linea de entrada") || low.contains ("l\xc3\xadnea de entrada")
            || low.contains ("instrument") || low.contains ("hi-z") || low.contains ("guitar");
    }

    bool isBuiltInMicOrLoopback (const juce::String& name)
    {
        if (name.trim().isEmpty())
            return true;
        if (isExplicitLineIn (name))
            return false;

        const auto low = name.toLowerCase();
        static constexpr const char* kBlocked[] = {
            "microphone", "microfono", "micr\xc3\xb3" "fono", "mic array", "array de mic",
            "internal mic", "built-in mic", "webcam", "camera",
            "stereo mix", "mezcla est", "what u hear", "loopback",
            "hands-free", "headset", "bluetooth", "voicemeeter", "cable output",
            "realtek", "conexant", "idt high definition"
        };
        for (const char* kw : kBlocked)
            if (low.contains (juce::String::fromUTF8 (kw)))
                return true;
        return false;
    }

    int scoreExternalInterface (const juce::String& name)
    {
        if (isBuiltInMicOrLoopback (name))
            return -100;

        const auto low = name.toLowerCase();
        int score = 10;
        static constexpr const char* kProKeywords[] = {
            "focusrite", "scarlett", "behringer", "umc", "m-vave", "mvave",
            "steinberg", "audient", "presonus", "motu", "roland", "boss",
            "line 6", "helix", "fender", "yamaha", "irig", "nux", "mooer",
            "valeton", "hotone", "tascam", "m-audio", "ssl", "apollo",
            "universal audio", "rocksmith", "guitar", "instrument", "usb", "asio"
        };
        for (const char* kw : kProKeywords)
            if (low.contains (kw))
                score += 20;
        return score;
    }
} // namespace

bool isRunningStandalone() noexcept
{
    return true;
}

bool isAudioInputMuted() noexcept
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
        return static_cast<bool> (holder->getMuteInputValue().getValue());
    return false;
}

void ensureAudioInputActive()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    auto& dm = holder->deviceManager;
    auto setup = dm.getAudioDeviceSetup();
    auto* dev = dm.getCurrentAudioDevice();

    const juce::String curInName = setup.inputDeviceName.isNotEmpty()
                                       ? setup.inputDeviceName
                                       : (dev != nullptr ? dev->getName() : juce::String());

    const bool manualConfirmed = (holder->settings != nullptr)
                               && holder->settings->getBoolValue ("v21ManualInputConfirmed", false);

    // 1. Scan current device type for a dedicated USB / external guitar/audio interface
    juce::String bestExternalInput;
    int bestScore = 0;
    if (auto* devType = dm.getCurrentDeviceTypeObject())
    {
        devType->scanForDevices();
        for (const auto& name : devType->getDeviceNames (true))
        {
            const int sc = scoreExternalInterface (name);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestExternalInput = name;
            }
        }
    }

    // 2. If current input is a laptop mic / Stereo Mix (from v20 or OS default),
    //    either switch to a detected external USB interface OR mute safely to prevent horrible speaker feedback!
    if (isBuiltInMicOrLoopback (curInName))
    {
        if (bestExternalInput.isNotEmpty())
        {
            setup.inputDeviceName = bestExternalInput;
            setup.useDefaultInputChannels = true;
            if (setup.inputChannels.countNumberOfSetBits() == 0)
                setup.inputChannels.setRange (0, 2, true);
            dm.setAudioDeviceSetup (setup, true);
            holder->getMuteInputValue().setValue (false);
        }
        else if (! manualConfirmed)
        {
            // Keep input muted so laptop mic + speakers never howl/feedback when nothing is plugged in!
            holder->getMuteInputValue().setValue (true);
        }
        holder->saveAudioDeviceState();
        return;
    }

    // 3. Current input is a real external interface / line-in: ensure channels are open & unmuted!
    const bool hasActiveIns = (setup.inputChannels.countNumberOfSetBits() > 0)
                           && (dev != nullptr && dev->getActiveInputChannels().countNumberOfSetBits() > 0);
    if (! hasActiveIns)
    {
        setup.useDefaultInputChannels = true;
        setup.inputChannels.setRange (0, 2, true);
        dm.setAudioDeviceSetup (setup, true);
    }

    holder->getMuteInputValue().setValue (false);
    holder->saveAudioDeviceState();
}

void openAudioSettingsDialog()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        if (holder->settings != nullptr)
            holder->settings->setValue ("v21ManualInputConfirmed", true);
        holder->showAudioSettingsDialog();
    }
}

juce::String getCurrentAudioStatusSummary()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return {};

    if (static_cast<bool> (holder->getMuteInputValue().getValue()))
        return "Entrada silenciada (elegi tu interfaz)";

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
