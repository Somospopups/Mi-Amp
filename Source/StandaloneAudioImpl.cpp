#include "StandaloneAudioBridge.h"
#include "Localization.h"
#include <atomic>
#include <cmath>
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
    std::atomic<bool> gInputAllowed        { false };
    std::atomic<bool> gBlockedNoInterface  { false };
    std::atomic<bool> gFeedbackTripped     { false };
    juce::String      gSessionManualDevice;

    bool isExplicitLineIn (const juce::String& name)
    {
        const auto low = name.toLowerCase();
        return low.contains ("line in") || low.contains ("line-in")
            || low.contains ("linea de entrada")
            || low.contains (juce::String::fromUTF8 ("l\xc3\xadnea de entrada"))
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
            "microphone", "microfono", "micr\xc3\xb3" "fono", "varios micr", "mic array", "array de mic",
            "internal mic", "built-in mic", "built-in input", "macbook", "imac",
            "iphone", "ipad", "continuity", "airpods", "webcam", "camera", "c\xc3\xa1mara", "camara",
            "stereo mix", "mezcla est", "what u hear", "loopback",
            "hands-free", "headset", "auricular", "bluetooth", "voicemeeter", "cable output",
            "blackhole", "soundflower",
            "realtek", "conexant", "idt high definition", "high definition audio",
            "smart sound", "intel(r) smart", "sonido intel",
            "primary sound", "controlador primario", "asignador de sonido", "sound mapper",
            "nahimic", "waves maxx", "amd streaming", "nvidia broadcast"
        };
        for (const char* kw : kBlocked)
            if (low.contains (juce::String::fromUTF8 (kw)))
                return true;
        return false;
    }

    bool isKnownExternalInterface (const juce::String& name, const juce::String& deviceTypeName)
    {
        if (isBuiltInMicOrLoopback (name))
            return false;

        if (deviceTypeName.toLowerCase().contains ("asio"))
            return true;

        if (isExplicitLineIn (name))
            return true;

        const auto low = name.toLowerCase();
        static constexpr const char* kProKeywords[] = {
            "focusrite", "scarlett", "clarett", "saffire",
            "behringer", "umc", "uphoria",
            "m-vave", "mvave", "tank-g", "blackbox",
            "steinberg", "ur22", "ur12", "ur44", "ixo12", "ixo22",
            "audient", "id4", "id14", "id24", "evo ",
            "presonus", "audiobox", "studio 2", "quantum",
            "motu", "m2", "m4", "ultralite",
            "roland", "rubix", "boss", "katana", "gt-1", "gt-1000",
            "line 6", "helix", "hx stomp", "pod go",
            "fender", "mustang", "tone master",
            "yamaha", "thr", "ag03", "ag06",
            "irig", "ik multimedia", "axe i/o",
            "nux", "mooer", "valeton", "hotone", "ampero",
            "tascam", "m-audio", "m-track", "air 192",
            "ssl", "solid state logic", "apollo", "volt ", "universal audio",
            "antelope", "rme", "babyface", "apogee", "duet", "zoom",
            "quad cortex", "neural dsp", "kemper", "fractal", "axe-fx",
            "rocksmith", "guitar", "instrument", "usb audio", "usb pnp",
            "usb guitar", "audio interface", "asio"
        };
        for (const char* kw : kProKeywords)
            if (low.contains (juce::String (kw).toLowerCase()))
                return true;
        return false;
    }

    int scoreExternalInterface (const juce::String& name, const juce::String& deviceTypeName)
    {
        if (! isKnownExternalInterface (name, deviceTypeName))
            return -100;

        const auto low = name.toLowerCase();
        int score = 30;
        if (deviceTypeName.toLowerCase().contains ("asio"))
            score += 30;
        if (low.contains ("usb") || low.contains ("guitar") || low.contains ("instrument") || isExplicitLineIn (name))
            score += 20;
        return score;
    }

    bool isSuspiciousOrUnverifiedInput (const juce::String& name, const juce::String& deviceTypeName)
    {
        if (isBuiltInMicOrLoopback (name))
            return true;
        return ! isKnownExternalInterface (name, deviceTypeName);
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
    return true;
}

bool isStandaloneInputAllowed() noexcept
{
    return gInputAllowed.load (std::memory_order_relaxed) && ! gFeedbackTripped.load (std::memory_order_relaxed);
}

bool isInputBlockedNoInterface() noexcept
{
    return gBlockedNoInterface.load (std::memory_order_relaxed);
}

bool wasInputMutedForFeedback() noexcept
{
    return gFeedbackTripped.load (std::memory_order_relaxed);
}

void muteInputForSafety (bool feedbackDetected) noexcept
{
    gInputAllowed.store (false, std::memory_order_relaxed);
    if (feedbackDetected)
        gFeedbackTripped.store (true, std::memory_order_relaxed);
    else
        gBlockedNoInterface.store (true, std::memory_order_relaxed);

    juce::MessageManager::callAsync ([]
    {
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            holder->getMuteInputValue().setValue (true);
            holder->saveAudioDeviceState();
        }
    });
}

void ensureAudioInputActive()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
    {
        gInputAllowed.store (false, std::memory_order_relaxed);
        return;
    }

    // Never honor stale persistent bypass of built-in mic protection across launches
    if (holder->settings != nullptr && holder->settings->containsKey ("v21ManualInputConfirmed"))
    {
        holder->settings->removeValue ("v21ManualInputConfirmed");
        holder->settings->saveIfNeeded();
    }

    auto& dm = holder->deviceManager;
    auto setup = dm.getAudioDeviceSetup();
    auto* dev = dm.getCurrentAudioDevice();
    auto* devType = dm.getCurrentDeviceTypeObject();
    const juce::String devTypeName = devType != nullptr ? devType->getTypeName() : juce::String();

    const juce::String curInName = setup.inputDeviceName.isNotEmpty()
                                       ? setup.inputDeviceName
                                       : (dev != nullptr ? dev->getName() : juce::String());

    // 1. Scan current device type for a verified dedicated USB / ASIO / external guitar/audio interface
    juce::String bestExternalInput;
    int bestScore = 0;
    if (devType != nullptr)
    {
        devType->scanForDevices();
        for (const auto& name : devType->getDeviceNames (true))
        {
            const int sc = scoreExternalInterface (name, devTypeName);
            if (sc > bestScore)
            {
                bestScore = sc;
                bestExternalInput = name;
            }
        }
    }

    if (gFeedbackTripped.load (std::memory_order_relaxed))
    {
        gInputAllowed.store (false, std::memory_order_relaxed);
        holder->getMuteInputValue().setValue (true);
        holder->saveAudioDeviceState();
        return;
    }

    // 2. If current input is a built-in mic, OS default driver, or unverified generic input:
    //    switch to a detected verified external interface OR keep input muted safely!
    if (isSuspiciousOrUnverifiedInput (curInName, devTypeName))
    {
        if (bestExternalInput.isNotEmpty())
        {
            setup.inputDeviceName = bestExternalInput;
            setup.useDefaultInputChannels = true;
            if (setup.inputChannels.countNumberOfSetBits() == 0)
                setup.inputChannels.setRange (0, 2, true);
            dm.setAudioDeviceSetup (setup, true);
            holder->getMuteInputValue().setValue (false);
            gBlockedNoInterface.store (false, std::memory_order_relaxed);
            gInputAllowed.store (true, std::memory_order_relaxed);
        }
        else if (gSessionManualDevice.isNotEmpty() && gSessionManualDevice == curInName
                 && ! static_cast<bool> (holder->getMuteInputValue().getValue()))
        {
            gBlockedNoInterface.store (false, std::memory_order_relaxed);
            gInputAllowed.store (true, std::memory_order_relaxed);
        }
        else
        {
            gInputAllowed.store (false, std::memory_order_relaxed);
            gBlockedNoInterface.store (true, std::memory_order_relaxed);
            holder->getMuteInputValue().setValue (true);
        }
        holder->saveAudioDeviceState();
        return;
    }

    // 3. Current input is a verified external interface / ASIO / Line-In: ensure channels are open & unmuted!
    const bool hasActiveIns = (setup.inputChannels.countNumberOfSetBits() > 0)
                           && (dev != nullptr && dev->getActiveInputChannels().countNumberOfSetBits() > 0);
    if (! hasActiveIns)
    {
        setup.useDefaultInputChannels = true;
        setup.inputChannels.setRange (0, 2, true);
        dm.setAudioDeviceSetup (setup, true);
    }

    const bool userMuted = static_cast<bool> (holder->getMuteInputValue().getValue());
    if (userMuted && gBlockedNoInterface.load (std::memory_order_relaxed))
    {
        holder->getMuteInputValue().setValue (false);
    }

    const bool finalMuted = static_cast<bool> (holder->getMuteInputValue().getValue());
    gBlockedNoInterface.store (false, std::memory_order_relaxed);
    gInputAllowed.store (! finalMuted, std::memory_order_relaxed);
    holder->saveAudioDeviceState();
}

void openAudioSettingsDialog()
{
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        gFeedbackTripped.store (false, std::memory_order_relaxed);
        holder->showAudioSettingsDialog();

        auto& dm = holder->deviceManager;
        const auto setup = dm.getAudioDeviceSetup();
        auto* dev = dm.getCurrentAudioDevice();
        const juce::String curIn = setup.inputDeviceName.isNotEmpty()
                                       ? setup.inputDeviceName
                                       : (dev != nullptr ? dev->getName() : juce::String());
        const bool muted = static_cast<bool> (holder->getMuteInputValue().getValue());
        const int ins = dev != nullptr ? dev->getActiveInputChannels().countNumberOfSetBits() : 0;

        if (! muted && ins > 0 && curIn.isNotEmpty())
        {
            gSessionManualDevice = curIn;
            gBlockedNoInterface.store (false, std::memory_order_relaxed);
            gInputAllowed.store (true, std::memory_order_relaxed);
        }
        else
        {
            gSessionManualDevice.clear();
            gInputAllowed.store (false, std::memory_order_relaxed);
        }
    }
}

juce::String getCurrentAudioStatusSummary()
{
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return {};

    if (gFeedbackTripped.load (std::memory_order_relaxed))
        return miamp_i18n::tr (miamp_i18n::Msg::StandaloneFeedbackMuted);

    if (static_cast<bool> (holder->getMuteInputValue().getValue()) || ! gInputAllowed.load (std::memory_order_relaxed))
        return miamp_i18n::tr (miamp_i18n::Msg::CfgAudioMuted);

    auto* dev = holder->deviceManager.getCurrentAudioDevice();
    if (dev == nullptr)
        return miamp_i18n::tr (miamp_i18n::Msg::CfgAudioNoDevice);

    const int ins = dev->getActiveInputChannels().countNumberOfSetBits();
    if (ins <= 0)
        return "Audio: " + dev->getName() + " (" + miamp_i18n::tr (miamp_i18n::Msg::CfgAudioNoInput) + ")";

    const int srKhz = static_cast<int> (std::lround (dev->getCurrentSampleRate() / 1000.0));
    return dev->getName() + " (" + juce::String (ins) + " in / " + juce::String (srKhz) + "k)";
}

} // namespace miamp_standalone
