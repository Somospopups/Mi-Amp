#include "StandaloneAudioBridge.h"
#include "Localization.h"
#include <atomic>
#include <cmath>
#include <juce_audio_devices/juce_audio_devices.h>
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
    bool              gReconfiguring       = false;
    juce::String      gSessionManualDevice;
    juce::String      gSessionManualOutput;
    juce::String      gLastActiveExternalInput;
    juce::String      gLastActiveExternalOutput;
    juce::String      gPendingAutoNotice;

    struct ScopedReconfigGuard
    {
        bool& ref;
        bool prev = false;
        explicit ScopedReconfigGuard (bool& flag) noexcept : ref (flag), prev (flag) { ref = true; }
        ~ScopedReconfigGuard() noexcept { ref = prev; }
    };

    bool isBlackboxOrMvave (const juce::String& name)
    {
        const auto low = name.toLowerCase();
        return low.contains ("blackbox")
            || low.contains ("annblackbox")
            || low.contains ("m-vave")
            || low.contains ("mvave")
            || low.contains ("tank-g")
            || low.contains ("tank g")
            || low.contains ("cuvave")
            || low.contains ("sinco");
    }

    bool isExplicitLineIn (const juce::String& name)
    {
        const auto low = name.toLowerCase();
        return low.contains ("line in") || low.contains ("line-in")
            || low.contains ("linea de entrada")
            || low.contains (juce::String::fromUTF8 ("l\xc3\xadnea de entrada"))
            || low.contains ("instrument") || low.contains ("hi-z") || low.contains ("guitar");
    }

    bool isHardBlockedDevice (const juce::String& name)
    {
        if (name.trim().isEmpty())
            return true;

        const auto low = name.toLowerCase();
        static constexpr const char* kHardBlocked[] = {
            "varios micr", "mic array", "array de mic",
            "internal mic", "built-in mic", "built-in input", "macbook", "imac",
            "iphone", "ipad", "continuity", "airpods", "webcam", "camera", "c\xc3\xa1mara", "camara",
            "stereo mix", "mezcla est", "what u hear", "loopback",
            "hands-free", "headset", "auricular", "bluetooth", "voicemeeter", "cable output",
            "blackhole", "soundflower",
            "realtek", "conexant", "idt high definition", "high definition audio",
            "smart sound", "intel(r) smart", "sonido intel",
            "primary sound", "controlador primario", "asignador de sonido", "sound mapper",
            "nahimic", "waves maxx", "amd streaming", "nvidia broadcast",
            "communications", "comunicaciones"
        };
        for (const char* kw : kHardBlocked)
            if (low.contains (juce::String::fromUTF8 (kw)))
                return true;
        return false;
    }

    bool hasKnownExternalKeyword (const juce::String& name)
    {
        if (isBlackboxOrMvave (name) || isExplicitLineIn (name))
            return true;

        const auto low = name.toLowerCase();
        static constexpr const char* kProKeywords[] = {
            "focusrite", "scarlett", "clarett", "saffire",
            "behringer", "umc", "uphoria",
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
            "rocksmith", "guitar", "instrument",
            "usb audio", "dispositivo de audio usb", "usb pnp",
            "usb guitar", "digital audio interface", "audio interface", "asio"
        };
        for (const char* kw : kProKeywords)
            if (low.contains (juce::String::fromUTF8 (kw).toLowerCase()))
                return true;
        return false;
    }

    bool isBuiltInMicOrLoopback (const juce::String& name)
    {
        if (isHardBlockedDevice (name))
            return true;

        // Windows WASAPI labels USB audio interfaces as "Microphone (BLACKBOX)" or "Micrófono (USB Audio)".
        // Allow them if they explicitly carry a known external interface / USB Audio / Blackbox tag.
        if (hasKnownExternalKeyword (name))
            return false;

        const auto low = name.toLowerCase();
        static constexpr const char* kMicOnly[] = {
            "microphone", "microfono", "micr\xc3\xb3" "fono", "mic "
        };
        for (const char* kw : kMicOnly)
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

        return hasKnownExternalKeyword (name);
    }

    bool isExclusiveAudioType (const juce::String& typeName)
    {
        return typeName.toLowerCase().contains ("exclusive");
    }

    int deviceTypePriorityBonus (const juce::String& typeName)
    {
        const auto low = typeName.toLowerCase();
        if (low.contains ("coreaudio"))
            return 25;
        if (low.contains ("asio"))
            return 22;
        if (low.contains ("windows audio") && ! low.contains ("exclusive"))
            return 20;
        if (low.contains ("directsound"))
            return 5;
        return 0;
    }

    int scoreExternalInterface (const juce::String& name, const juce::String& deviceTypeName)
    {
        if (! isKnownExternalInterface (name, deviceTypeName))
            return -100;

        const auto low = name.toLowerCase();
        int score = 30 + deviceTypePriorityBonus (deviceTypeName);

        if (isBlackboxOrMvave (name))
            score += 140;
        else if (low.contains ("usb audio") || low.contains ("dispositivo de audio usb") || low.contains ("usb pnp"))
            score += 55;
        else if (low.contains ("usb") || low.contains ("guitar") || low.contains ("instrument") || isExplicitLineIn (name))
            score += 35;

        return score;
    }

    bool isSuspiciousOrUnverifiedInput (const juce::String& name, const juce::String& deviceTypeName)
    {
        if (isBuiltInMicOrLoopback (name))
            return true;
        return ! isKnownExternalInterface (name, deviceTypeName);
    }

    // Extract hardware tag from Windows endpoint names like "Microphone (2- BLACKBOX)" -> "blackbox"
    juce::String extractHardwareTag (const juce::String& endpointName)
    {
        const int openP  = endpointName.indexOfChar ('(');
        const int closeP = endpointName.lastIndexOfChar (')');
        juce::String tag = (openP >= 0 && closeP > openP)
                               ? endpointName.substring (openP + 1, closeP)
                               : endpointName;
        tag = tag.trim().toLowerCase();
        if (tag.length() > 3 && juce::CharacterFunctions::isDigit (tag[0]) && tag[1] == '-' && tag[2] == ' ')
            tag = tag.substring (3).trim();
        return tag;
    }

    juce::String findMatchingOutputDevice (juce::AudioIODeviceType& devType, const juce::String& inputName)
    {
        const auto outputs = devType.getDeviceNames (false);
        const juce::String inLow = inputName.toLowerCase();
        const juce::String inTag = extractHardwareTag (inputName);

        juce::String bestOut;
        int bestScore = 0;

        for (const auto& out : outputs)
        {
            if (isHardBlockedDevice (out))
                continue;

            const auto outLow = out.toLowerCase();
            const auto outTag = extractHardwareTag (out);
            int sc = 0;

            if (isBlackboxOrMvave (inputName) && isBlackboxOrMvave (out))
                sc += 150;
            if (inTag.isNotEmpty() && outTag == inTag)
                sc += 100;
            else if (inTag.isNotEmpty() && outLow.contains (inTag))
                sc += 70;
            if (outLow == inLow)
                sc += 40;

            if (sc > bestScore)
            {
                bestScore = sc;
                bestOut = out;
            }
        }
        return bestScore >= 70 ? bestOut : juce::String();
    }

    double pickPreferredSampleRate (const juce::Array<double>& rates, bool prefer44100)
    {
        if (rates.isEmpty())
            return prefer44100 ? 44100.0 : 0.0;

        if (prefer44100)
        {
            for (double r : rates)
                if (std::abs (r - 44100.0) < 1.0)
                    return r;
        }
        for (double r : rates)
            if (std::abs (r - 48000.0) < 1.0)
                return r;
        for (double r : rates)
            if (std::abs (r - 44100.0) < 1.0)
                return r;
        return rates[0];
    }

    int pickPreferredBufferSize (const juce::Array<int>& sizes)
    {
        if (sizes.isEmpty())
            return 0;

        static constexpr int kTargets[] = { 128, 192, 256, 160, 144, 96, 64, 320, 384, 512 };
        for (int target : kTargets)
            if (sizes.contains (target))
                return target;

        int best = sizes[0];
        int bestDiff = std::abs (best - 192);
        for (int s : sizes)
        {
            const int d = std::abs (s - 192);
            if (d < bestDiff)
            {
                bestDiff = d;
                best = s;
            }
        }
        return best;
    }

    void pushAutoNotice (const juce::String& text)
    {
        if (text.isNotEmpty())
            gPendingAutoNotice = text;
    }

    bool deviceExistsInType (juce::AudioIODeviceType& devType, const juce::String& name, bool isInput)
    {
        if (name.isEmpty())
            return false;
        return devType.getDeviceNames (isInput).contains (name);
    }

    struct HotplugMonitor final : public juce::ChangeListener,
                                  public juce::Timer,
                                  public juce::DeletedAtShutdown
    {
        explicit HotplugMonitor (juce::AudioDeviceManager& dm) : deviceManager (&dm)
        {
            deviceManager->addChangeListener (this);
            startTimer (1500);
        }

        ~HotplugMonitor() override
        {
            stopTimer();
            if (auto* holder = juce::StandalonePluginHolder::getInstance())
            {
                if (&holder->deviceManager == deviceManager)
                    deviceManager->removeChangeListener (this);
            }
            instancePtr = nullptr;
        }

        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            if (gReconfiguring)
                return;
            forceFullRescan = true;
            juce::MessageManager::callAsync ([] { ensureAudioInputActive(); });
        }

        void timerCallback() override
        {
            if (gReconfiguring)
                return;
            ++tickCounter;
            if ((tickCounter % 3) == 0)
                forceFullRescan = true;
            ensureAudioInputActive();
        }

        static void ensureAttached (juce::AudioDeviceManager& dm)
        {
            if (instancePtr != nullptr || ! juce::MessageManager::getInstance()->isThisTheMessageThread())
                return;
            instancePtr = new HotplugMonitor (dm);
        }

        juce::AudioDeviceManager* deviceManager = nullptr;
        int tickCounter = 0;
        static inline bool forceFullRescan = true;
        static inline HotplugMonitor* instancePtr = nullptr;
    };

    struct DeviceCandidate
    {
        juce::AudioIODeviceType* type = nullptr;
        juce::String typeName;
        juce::String inputName;
        int score = -100;
    };

    DeviceCandidate findBestExternalCandidate (juce::AudioDeviceManager& dm, bool scanCurrentTypeOnly)
    {
        DeviceCandidate best;

        auto scanType = [&] (juce::AudioIODeviceType* dt)
        {
            if (dt == nullptr)
                return;
            const auto tName = dt->getTypeName();
            if (isExclusiveAudioType (tName))
                return;

            dt->scanForDevices();
            for (const auto& name : dt->getDeviceNames (true))
            {
                const int sc = scoreExternalInterface (name, tName);
                if (sc > best.score)
                {
                    best.score = sc;
                    best.type = dt;
                    best.typeName = tName;
                    best.inputName = name;
                }
            }
        };

        if (scanCurrentTypeOnly)
        {
            scanType (dm.getCurrentDeviceTypeObject());
            return best;
        }

        for (auto* dt : dm.getAvailableDeviceTypes())
            scanType (dt);
        return best;
    }

    bool configureAndActivateInterface (juce::StandalonePluginHolder& holder,
                                        const DeviceCandidate& cand,
                                        bool announceChange)
    {
        if (cand.type == nullptr || cand.inputName.isEmpty())
            return false;

        ScopedReconfigGuard guard (gReconfiguring);
        auto& dm = holder.deviceManager;

        if (dm.getCurrentAudioDeviceType() != cand.typeName)
            dm.setCurrentAudioType (cand.typeName);

        auto* activeType = dm.getCurrentDeviceTypeObject();
        if (activeType == nullptr)
            return false;

        activeType->scanForDevices();
        auto setup = dm.getAudioDeviceSetup();
        setup.inputDeviceName = cand.inputName;
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        setup.inputChannels.setRange (0, 2, true);

        const juce::String matchedOut = findMatchingOutputDevice (*activeType, cand.inputName);
        if (matchedOut.isNotEmpty() && gSessionManualOutput.isEmpty())
        {
            setup.outputDeviceName = matchedOut;
            setup.useDefaultOutputChannels = false;
            setup.outputChannels.clear();
            setup.outputChannels.setRange (0, 2, true);
        }

        const bool isBlackbox = isBlackboxOrMvave (cand.inputName);
        if (isBlackbox)
            setup.sampleRate = 44100.0;

        juce::String err = dm.setAudioDeviceSetup (setup, true);
        if (err.isNotEmpty() && matchedOut.isNotEmpty())
        {
            // Fallback: keep OS default output if paired USB output failed to open
            auto fallback = dm.getAudioDeviceSetup();
            fallback.inputDeviceName = cand.inputName;
            fallback.useDefaultInputChannels = false;
            fallback.inputChannels.clear();
            fallback.inputChannels.setRange (0, 2, true);
            fallback.outputDeviceName.clear();
            fallback.useDefaultOutputChannels = true;
            err = dm.setAudioDeviceSetup (fallback, true);
        }

        // Second pass: lock exact native sample rate (44.1 kHz for Blackbox) and low-latency buffer size
        if (auto* opened = dm.getCurrentAudioDevice())
        {
            auto tuned = dm.getAudioDeviceSetup();
            bool needRetune = false;

            const double desiredRate = pickPreferredSampleRate (opened->getAvailableSampleRates(), isBlackbox);
            if (desiredRate > 0.0 && std::abs (opened->getCurrentSampleRate() - desiredRate) > 1.0)
            {
                tuned.sampleRate = desiredRate;
                needRetune = true;
            }

            const int curBuf = opened->getCurrentBufferSizeSamples();
            const int desiredBuf = pickPreferredBufferSize (opened->getAvailableBufferSizes());
            if (desiredBuf > 0 && (curBuf <= 0 || curBuf > 256 || (isBlackbox && curBuf != desiredBuf && curBuf > desiredBuf)))
            {
                tuned.bufferSize = desiredBuf;
                needRetune = true;
            }

            if (opened->getActiveInputChannels().countNumberOfSetBits() == 0)
            {
                tuned.useDefaultInputChannels = false;
                tuned.inputChannels.clear();
                tuned.inputChannels.setRange (0, 2, true);
                needRetune = true;
            }

            if (needRetune)
                dm.setAudioDeviceSetup (tuned, true);
        }

        gFeedbackTripped.store (false, std::memory_order_relaxed);
        gBlockedNoInterface.store (false, std::memory_order_relaxed);
        gInputAllowed.store (true, std::memory_order_relaxed);
        holder.getMuteInputValue().setValue (false);
        holder.saveAudioDeviceState();

        const auto finalSetup = dm.getAudioDeviceSetup();
        gLastActiveExternalInput  = cand.inputName;
        gLastActiveExternalOutput = finalSetup.outputDeviceName;

        if (announceChange)
        {
            if (isBlackbox)
            {
                pushAutoNotice (miamp_i18n::tr (miamp_i18n::Msg::StandaloneBlackboxConnected));
            }
            else
            {
                auto* opened = dm.getCurrentAudioDevice();
                const int srKhz = opened != nullptr
                                      ? static_cast<int> (std::lround (opened->getCurrentSampleRate() / 1000.0))
                                      : 44;
                pushAutoNotice (miamp_i18n::tr (miamp_i18n::Msg::StandaloneInterfaceConnected)
                                + cand.inputName + " (" + juce::String (srKhz) + " kHz)");
            }
        }
        return err.isEmpty();
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
            ScopedReconfigGuard guard (gReconfiguring);
            holder->getMuteInputValue().setValue (true);
            holder->saveAudioDeviceState();
        }
    });
}

void ensureAudioInputActive()
{
    if (gReconfiguring)
        return;

    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
    {
        gInputAllowed.store (false, std::memory_order_relaxed);
        return;
    }

    HotplugMonitor::ensureAttached (holder->deviceManager);

    // Never honor stale persistent bypass of built-in mic protection across launches
    if (holder->settings != nullptr && holder->settings->containsKey ("v21ManualInputConfirmed"))
    {
        holder->settings->removeValue ("v21ManualInputConfirmed");
        if (auto* propsFile = dynamic_cast<juce::PropertiesFile*> (holder->settings.get()))
            propsFile->saveIfNeeded();
    }

    auto& dm = holder->deviceManager;
    auto setup = dm.getAudioDeviceSetup();
    auto* dev = dm.getCurrentAudioDevice();
    auto* devType = dm.getCurrentDeviceTypeObject();
    const juce::String devTypeName = devType != nullptr ? devType->getTypeName() : juce::String();

    if (devType != nullptr)
        devType->scanForDevices();

    const juce::String curInName = setup.inputDeviceName.isNotEmpty()
                                       ? setup.inputDeviceName
                                       : (dev != nullptr ? dev->getName() : juce::String());

    const bool curInStillPresent = (devType != nullptr) && deviceExistsInType (*devType, curInName, true);
    const bool curWasExternal    = curInName.isNotEmpty() && ! isSuspiciousOrUnverifiedInput (curInName, devTypeName);

    // Detect unplug of previously active external interface (e.g. Blackbox unplugged)
    const bool activeExternalUnplugged = gLastActiveExternalInput.isNotEmpty()
                                      && (devType == nullptr || ! deviceExistsInType (*devType, gLastActiveExternalInput, true));

    if (activeExternalUnplugged)
    {
        const juce::String unpluggedIn  = gLastActiveExternalInput;
        const juce::String unpluggedOut = gLastActiveExternalOutput;
        gLastActiveExternalInput.clear();
        gLastActiveExternalOutput.clear();
        if (gSessionManualDevice == unpluggedIn)
            gSessionManualDevice.clear();
        if (gSessionManualOutput == unpluggedOut)
            gSessionManualOutput.clear();

        // Check if another external interface is connected across any driver
        const auto replacement = findBestExternalCandidate (dm, false);
        if (replacement.score > 0 && replacement.inputName.isNotEmpty())
        {
            configureAndActivateInterface (*holder, replacement, true);
            return;
        }

        ScopedReconfigGuard guard (gReconfiguring);
        if (unpluggedOut.isNotEmpty() && setup.outputDeviceName == unpluggedOut)
        {
            setup.outputDeviceName.clear();
            setup.useDefaultOutputChannels = true;
            dm.setAudioDeviceSetup (setup, true);
        }
        gInputAllowed.store (false, std::memory_order_relaxed);
        gBlockedNoInterface.store (true, std::memory_order_relaxed);
        holder->getMuteInputValue().setValue (true);
        holder->saveAudioDeviceState();
        pushAutoNotice (miamp_i18n::tr (miamp_i18n::Msg::StandaloneInterfaceDisconnected));
        return;
    }

    // Fast path: if already running cleanly on a verified Blackbox that is still plugged in, do not disturb audio!
    if (curInStillPresent && curWasExternal && isBlackboxOrMvave (curInName)
        && dev != nullptr && dev->getActiveInputChannels().countNumberOfSetBits() > 0)
    {
        HotplugMonitor::forceFullRescan = false;
        gLastActiveExternalInput = curInName;
        gLastActiveExternalOutput = setup.outputDeviceName;
        const bool userMuted = static_cast<bool> (holder->getMuteInputValue().getValue());
        if (userMuted && gBlockedNoInterface.load (std::memory_order_relaxed))
        {
            ScopedReconfigGuard guard (gReconfiguring);
            holder->getMuteInputValue().setValue (false);
        }
        const bool finalMuted = static_cast<bool> (holder->getMuteInputValue().getValue());
        gBlockedNoInterface.store (false, std::memory_order_relaxed);
        gInputAllowed.store (! finalMuted && ! gFeedbackTripped.load (std::memory_order_relaxed),
                             std::memory_order_relaxed);
        return;
    }

    // Only scan across all device types when no verified interface is active, on ChangeListener events, or periodic check
    const bool needFullScan = HotplugMonitor::forceFullRescan
                           || ! curInStillPresent
                           || ! curWasExternal
                           || gBlockedNoInterface.load (std::memory_order_relaxed);
    HotplugMonitor::forceFullRescan = false;

    const auto best = needFullScan ? findBestExternalCandidate (dm, false)
                                   : findBestExternalCandidate (dm, true);

    // If a dedicated M-VAVE Blackbox just plugged in while no manual override is active, auto-switch immediately
    // (even if feedback had previously tripped on a laptop mic or another device was idle!)
    if (best.score > 0 && isBlackboxOrMvave (best.inputName)
        && (gSessionManualDevice.isEmpty() || gSessionManualDevice == best.inputName))
    {
        const bool switchingToNew = (curInName != best.inputName || devTypeName != best.typeName
                                     || ! curInStillPresent || gBlockedNoInterface.load (std::memory_order_relaxed)
                                     || dev == nullptr || dev->getActiveInputChannels().countNumberOfSetBits() == 0);
        if (switchingToNew)
        {
            configureAndActivateInterface (*holder, best, true);
            return;
        }
    }

    if (gFeedbackTripped.load (std::memory_order_relaxed))
    {
        ScopedReconfigGuard guard (gReconfiguring);
        gInputAllowed.store (false, std::memory_order_relaxed);
        holder->getMuteInputValue().setValue (true);
        holder->saveAudioDeviceState();
        return;
    }

    // If current input is missing, built-in mic, OS default driver, or unverified generic input:
    // switch to detected external interface OR keep input muted safely!
    if (! curInStillPresent || isSuspiciousOrUnverifiedInput (curInName, devTypeName))
    {
        if (best.score > 0 && best.inputName.isNotEmpty())
        {
            configureAndActivateInterface (*holder, best, gLastActiveExternalInput != best.inputName);
            return;
        }

        if (curInStillPresent && gSessionManualDevice.isNotEmpty() && gSessionManualDevice == curInName
            && ! static_cast<bool> (holder->getMuteInputValue().getValue()))
        {
            gBlockedNoInterface.store (false, std::memory_order_relaxed);
            gInputAllowed.store (true, std::memory_order_relaxed);
            return;
        }

        ScopedReconfigGuard guard (gReconfiguring);
        gInputAllowed.store (false, std::memory_order_relaxed);
        gBlockedNoInterface.store (true, std::memory_order_relaxed);
        holder->getMuteInputValue().setValue (true);
        holder->saveAudioDeviceState();
        return;
    }

    // Current input is a verified external interface / ASIO / Line-In: ensure channels & sample rate are ready
    const bool hasActiveIns = (setup.inputChannels.countNumberOfSetBits() > 0)
                           && (dev != nullptr && dev->getActiveInputChannels().countNumberOfSetBits() > 0);
    if (! hasActiveIns)
    {
        configureAndActivateInterface (*holder,
                                       DeviceCandidate { devType, devTypeName, curInName, 50 },
                                       false);
        return;
    }

    gLastActiveExternalInput  = curInName;
    gLastActiveExternalOutput = setup.outputDeviceName;

    const bool userMuted = static_cast<bool> (holder->getMuteInputValue().getValue());
    if (userMuted && gBlockedNoInterface.load (std::memory_order_relaxed))
    {
        ScopedReconfigGuard guard (gReconfiguring);
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
        {
            ScopedReconfigGuard guard (gReconfiguring);
            holder->showAudioSettingsDialog();
        }

        auto& dm = holder->deviceManager;
        const auto setup = dm.getAudioDeviceSetup();
        auto* dev = dm.getCurrentAudioDevice();
        const juce::String curIn = setup.inputDeviceName.isNotEmpty()
                                       ? setup.inputDeviceName
                                       : (dev != nullptr ? dev->getName() : juce::String());
        const bool muted = static_cast<bool> (holder->getMuteInputValue().getValue());
        const int ins = dev != nullptr ? dev->getActiveInputChannels().countNumberOfSetBits() : 0;

        gSessionManualOutput = setup.outputDeviceName;

        if (! muted && ins > 0 && curIn.isNotEmpty())
        {
            gSessionManualDevice = curIn;
            gLastActiveExternalInput = curIn;
            gLastActiveExternalOutput = setup.outputDeviceName;
            gBlockedNoInterface.store (false, std::memory_order_relaxed);
            gInputAllowed.store (true, std::memory_order_relaxed);
        }
        else
        {
            gSessionManualDevice.clear();
            gLastActiveExternalInput.clear();
            gInputAllowed.store (false, std::memory_order_relaxed);
        }
        holder->saveAudioDeviceState();
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

    const double srKhz = dev->getCurrentSampleRate() / 1000.0;
    const juce::String srText = (std::abs (srKhz - 44.1) < 0.2)
                                    ? juce::String ("44.1k")
                                    : (juce::String (static_cast<int> (std::lround (srKhz))) + "k");
    return dev->getName() + " (" + juce::String (ins) + " in / " + srText + ")";
}

juce::String popAudioAutoSwitchNotice()
{
    juce::String out;
    out.swapWith (gPendingAutoNotice);
    return out;
}

} // namespace miamp_standalone
