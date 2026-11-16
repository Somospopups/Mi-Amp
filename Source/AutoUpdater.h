#pragma once
#include <JuceHeader.h>
#include "Localization.h"
#include "StandaloneAudioBridge.h"
#include <atomic>
#include <functional>

#ifndef MIAMP_BUILD_NUMBER
 #define MIAMP_BUILD_NUMBER 0
#endif

#ifndef MIAMP_APP_VERSION
 #define MIAMP_APP_VERSION "1.1.0"
#endif

namespace miamp_updater
{
struct UpdateInfo
{
    bool available = false;
    bool networkOk = false;
    int remoteBuild = 0;
    juce::String remoteVersion;
    juce::String downloadUrl;
};

inline juce::String versionJsonUrl()
{
    return "https://github.com/Somospopups/Mi-Amp/releases/download/v1.0.0-win/version.json";
}

inline int currentBuildNumber() noexcept
{
    return MIAMP_BUILD_NUMBER;
}

inline juce::String currentVersionString()
{
    return juce::String (MIAMP_APP_VERSION);
}

inline std::atomic<bool>& startupCheckDoneFlag() noexcept
{
    static std::atomic<bool> flag { false };
    return flag;
}

inline UpdateInfo fetchLatestSync (bool allowFromLocalBuild = false)
{
    UpdateInfo info;
    try
    {
        juce::URL url (versionJsonUrl());
        auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                        .withConnectionTimeoutMs (6500)
                        .withNumRedirectsToFollow (6)
                        .withExtraHeaders ("User-Agent: MiAmp-Updater\r\nCache-Control: no-cache\r\n");

        if (auto stream = url.createInputStream (opts))
        {
            const juce::String body = stream->readEntireStreamAsString();
            const auto parsed = juce::JSON::parse (body);
            if (parsed.isObject())
            {
                info.networkOk = true;
                info.remoteBuild = static_cast<int> (parsed.getProperty ("build", 0));
                info.remoteVersion = parsed.getProperty ("version", "").toString().trim();
                #if JUCE_MAC
                 info.downloadUrl = parsed.getProperty ("macos_url", "").toString().trim();
                #else
                 info.downloadUrl = parsed.getProperty ("windows_url", "").toString().trim();
                #endif

                if (info.remoteBuild > 0 && info.downloadUrl.isNotEmpty())
                {
                    if (currentBuildNumber() > 0)
                        info.available = (info.remoteBuild > currentBuildNumber());
                    else
                        info.available = allowFromLocalBuild;
                }
            }
        }
    }
    catch (...) {}
    return info;
}

inline bool validateDownloadedInstaller (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;
    if (file.getSize() < static_cast<juce::int64> (5 * 1024 * 1024))
        return false;

    juce::FileInputStream fis (file);
    if (! fis.openedOk())
        return false;

    uint8_t hdr[4] = { 0, 0, 0, 0 };
    if (fis.read (hdr, 4) < 4)
        return false;

    #if JUCE_MAC
     // Apple flat .pkg uses xar! magic bytes
     return hdr[0] == 'x' && hdr[1] == 'a' && hdr[2] == 'r' && hdr[3] == '!';
    #else
     // Windows PE executable MZ magic bytes
     return hdr[0] == 'M' && hdr[1] == 'Z';
    #endif
}

inline bool launchStandaloneSilentUpdateAndQuit (const juce::File& installerFile)
{
    #if JUCE_WINDOWS
     const juce::String curExe = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getFullPathName();
     const juce::File cmdFile = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                    .getChildFile ("miamp_self_update.cmd");

     juce::String script;
     script << "@echo off\r\n"
            << "timeout /t 2 /nobreak >nul\r\n"
            << "taskkill /F /IM \"MI AMP.exe\" >nul 2>nul\r\n"
            << "timeout /t 1 /nobreak >nul\r\n"
            << "\"" << installerFile.getFullPathName() << "\" /S\r\n"
            << "if exist \"" << curExe << "\" (\r\n"
            << "  start \"\" \"" << curExe << "\"\r\n"
            << ") else if exist \"%ProgramFiles%\\Popups Audio\\MI AMP\\MI AMP.exe\" (\r\n"
            << "  start \"\" \"%ProgramFiles%\\Popups Audio\\MI AMP\\MI AMP.exe\"\r\n"
            << ")\r\n"
            << "del /f /q \"" << installerFile.getFullPathName() << "\" >nul 2>nul\r\n"
            << "del /f /q \"%~f0\" >nul 2>nul\r\n";

     if (! cmdFile.replaceWithText (script))
         return false;

     juce::ChildProcess proc;
     const bool started = proc.start ("cmd.exe /c start \"MI AMP Updater\" /min \"" + cmdFile.getFullPathName() + "\"");
     if (! started && ! cmdFile.startAsProcess())
         return false;

     if (auto* app = juce::JUCEApplicationBase::getInstance())
         app->systemRequestedQuit();
     return true;

    #elif JUCE_MAC
     const juce::String curApp = juce::File::getSpecialLocation (juce::File::currentApplicationFile).getFullPathName();
     const juce::File shFile ("/tmp/miamp_mac_update.sh");
     const juce::String pkgEsc = installerFile.getFullPathName().replace ("\"", "\\\"");
     const juce::String appEsc = curApp.replace ("\"", "\\\"");

     juce::String script;
     script << "#!/bin/sh\n"
            << "sleep 2\n"
            << "/usr/bin/osascript -e 'do shell script \"installer -pkg \\\"" << pkgEsc << "\\\" -target /\" with administrator privileges'\n"
            << "if [ -d \"" << appEsc << "\" ]; then\n"
            << "  open -a \"" << appEsc << "\"\n"
            << "elif [ -d \"/Applications/MI AMP.app\" ]; then\n"
            << "  open -a \"/Applications/MI AMP.app\"\n"
            << "fi\n"
            << "rm -f \"" << pkgEsc << "\" \"$0\"\n";

     if (! shFile.replaceWithText (script))
         return false;

     shFile.setExecutePermission (true);
     juce::ChildProcess proc;
     if (! proc.start ("/bin/sh -c \"nohup /tmp/miamp_mac_update.sh >/dev/null 2>&1 &\""))
         return false;

     if (auto* app = juce::JUCEApplicationBase::getInstance())
         app->systemRequestedQuit();
     return true;
    #else
     juce::ignoreUnused (installerFile);
     return false;
    #endif
}

class CheckThread final : public juce::Thread
{
public:
    CheckThread (std::function<void (UpdateInfo)> cb, bool allowLocalOverride = false)
        : juce::Thread ("MiAmpUpdateCheck"),
          callback (std::move (cb)),
          allowLocal (allowLocalOverride)
    {}

    void run() override
    {
        auto info = fetchLatestSync (allowLocal);
        if (threadShouldExit())
            return;
        auto fn = callback;
        juce::MessageManager::callAsync ([fn, info]
        {
            if (fn) fn (info);
        });
    }

private:
    std::function<void (UpdateInfo)> callback;
    bool allowLocal = false;
};

class DownloadThread final : public juce::Thread
{
public:
    DownloadThread (juce::String urlStr, int buildNum,
                    std::function<void (float)> onProg,
                    std::function<void (bool, juce::File)> onFinish)
        : juce::Thread ("MiAmpUpdateDownload"),
          downloadUrl (std::move (urlStr)),
          remoteBuild (buildNum),
          progressCb (std::move (onProg)),
          finishCb (std::move (onFinish))
    {}

    void run() override
    {
        #if JUCE_MAC
         const juce::String fname = "Instalar_MI_AMP_macOS_Universal_" + juce::String (remoteBuild) + ".pkg";
        #else
         const juce::String fname = "Instalar_MI_AMP_Windows_x64_" + juce::String (remoteBuild) + ".exe";
        #endif

        const juce::File targetDir = miamp_standalone::isRunningStandalone()
                                         ? juce::File::getSpecialLocation (juce::File::tempDirectory)
                                         : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
        const juce::File outFile = targetDir.getChildFile (fname);
        if (outFile.existsAsFile())
            outFile.deleteFile();

        bool ok = false;
        try
        {
            juce::URL url (downloadUrl);
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (15000)
                            .withNumRedirectsToFollow (8)
                            .withExtraHeaders ("User-Agent: MiAmp-Updater\r\n");

            if (auto in = url.createInputStream (opts))
            {
                const juce::int64 totalLen = in->getTotalLength();
                juce::FileOutputStream out (outFile);
                if (out.openedOk())
                {
                    char buf[65536];
                    juce::int64 downloaded = 0;
                    int lastPct = -1;
                    while (! threadShouldExit())
                    {
                        const int n = in->read (buf, static_cast<int> (sizeof (buf)));
                        if (n <= 0)
                            break;
                        if (! out.write (buf, static_cast<size_t> (n)))
                            break;
                        downloaded += n;
                        const float prog = (totalLen > 0)
                                               ? juce::jlimit (0.02f, 0.99f, static_cast<float> (downloaded) / static_cast<float> (totalLen))
                                               : 0.5f;
                        const int pct = static_cast<int> (prog * 100.0f);
                        if (pct != lastPct)
                        {
                            lastPct = pct;
                            auto pCb = progressCb;
                            juce::MessageManager::callAsync ([pCb, prog] { if (pCb) pCb (prog); });
                        }
                    }
                    out.flush();
                    ok = (! threadShouldExit()) && validateDownloadedInstaller (outFile);
                }
            }
        }
        catch (...)
        {
            ok = false;
        }

        if (! ok && outFile.existsAsFile())
            outFile.deleteFile();

        auto fCb = finishCb;
        juce::MessageManager::callAsync ([fCb, ok, outFile]
        {
            if (fCb) fCb (ok, outFile);
        });
    }

private:
    juce::String downloadUrl;
    int remoteBuild = 0;
    std::function<void (float)> progressCb;
    std::function<void (bool, juce::File)> finishCb;
};

} // namespace miamp_updater
