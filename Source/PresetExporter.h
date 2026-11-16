#pragma once
#include "PluginProcessor.h"
#include "DSP/ZenboxCabIRs.h"
#include "DSP/XVBassDSP.h"
#include "DSP/MiAmpBoutiquePedalsDSP.h"
#include "DSP/MiAmpGuitarChainDSP.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

namespace mipedals
{

enum class ExportFormat
{
    IrWav24_44k = 1,   // Universal .wav IR (M-Vave Blackbox, Tank-G, Mooer, NUX, Valeton, Ampero, Helix)
    HelixHlx    = 2,   // Line 6 Helix / HX Stomp / POD Go (.hlx JSON)
    HeadrushRig = 3,   // Headrush (.rig JSON)
    BlackboxTxt = 4,   // Guia de mapeo M-Vave Blackbox & universal (.txt)
    MiAmpXml    = 5    // Preset portable completo MI AMP (.miamp)
};

class PresetExporter
{
public:
    static juce::String sanitizeFileName (const juce::String& raw)
    {
        juce::String out;
        const std::string s (raw.toRawUTF8());
        for (unsigned char c : s)
        {
            if (std::isalnum (c) || c == ' ' || c == '-' || c == '_')
            {
                const char ch[2] = { static_cast<char> (c), '\0' };
                out += juce::String (ch);
            }
            else if (c == '.')
                out += "_";
        }
        out = out.trim();
        return out.isEmpty() ? juce::String ("MI_AMP_Preset") : out;
    }

    static bool writeTextFile (const juce::File& f, const juce::String& content)
    {
        const std::string utf8 (content.toRawUTF8());
        std::ofstream os (f.getFullPathName().toRawUTF8(), std::ios::binary | std::ios::trunc);
        if (! os)
            return false;
        os.write (utf8.data(), static_cast<std::streamsize> (utf8.size()));
        return os.good();
    }

    // 1) Universal 44.1 kHz / 24-bit mono Impulse Response (.wav) — ready for M-Vave Blackbox IR CAB slots!
    static bool exportIrWav44k24Bit (const MiAmpAudioProcessor& proc, const juce::File& outFile)
    {
        constexpr int kSr  = 44100;
        constexpr int kLen = 1024;
        std::vector<float> buf (kLen, 0.0f);
        buf[0] = 1.0f; // Unit impulse

        const bool bass = proc.isBassMode();
        if (bass)
        {
            const auto snap = proc.getBassSnapshot();
            const bool hasEq = snap.eqOn && std::find (snap.chainSlots.begin(), snap.chainSlots.end(), 13) != snap.chainSlots.end();
            const bool hasXv = snap.xvOn && std::find (snap.chainSlots.begin(), snap.chainSlots.end(), 1)  != snap.chainSlots.end();

            Boutique7BandEQFX eq7;
            eq7.prepare (kSr, true);

            xvdsp::Biquad flLo, flMid, flHi, eqBass, eqMid, eqTreble;
            if (snap.ampUltraHi)
            {
                flLo.setLowShelf (kSr, 65.0, 0.0);
                flMid.setPeaking (kSr, 450.0, 0.0, 0.9);
                flHi.setHighShelf (kSr, 4200.0, 4.8);
            }
            else
            {
                flLo.setLowShelf (kSr, 65.0, 4.0);
                flMid.setPeaking (kSr, 450.0, -2.5, 0.9);
                flHi.setHighShelf (kSr, 4200.0, 0.0);
            }
            eqBass.setLowShelf (kSr, 80.0, (snap.ampBass * 0.01 - 0.5) * 18.0);
            const double midHz = 220.0 * std::pow (2800.0 / 220.0, snap.ampFreq * 0.01);
            eqMid.setPeaking (kSr, midHz, (snap.ampMid * 0.01 - 0.5) * 18.0, 1.2);
            eqTreble.setHighShelf (kSr, 3200.0, (snap.ampTreble * 0.01 - 0.5) * 18.0);

            xvdsp::CabIrConvolver cab;
            cab.prepare (kSr);
            const int cabMode = hasXv ? std::clamp (snap.xvCab, 0, 2) : 0; // 0=8x10 Bass, 1=Punk, 2=Clean

            for (int i = 0; i < kLen; ++i)
            {
                float s = buf[static_cast<size_t> (i)];
                if (hasEq)
                    s = eq7.process (s, true, snap.eqBands);
                if (! snap.headBypass)
                {
                    s = flHi.processSample (flMid.processSample (flLo.processSample (s)));
                    s = eqTreble.processSample (eqMid.processSample (eqBass.processSample (s)));
                }
                if (cabMode < 2)
                    s = cab.processSample (s, cabMode);
                buf[static_cast<size_t> (i)] = s;
            }
        }
        else
        {
            const auto snap = proc.getGuitarSnapshot();
            const bool hasEq = snap.eqOn && std::find (snap.chainSlots.begin(), snap.chainSlots.end(), 12) != snap.chainSlots.end();
            const int ch = std::clamp (snap.activeChannel, 0, 2);

            Boutique7BandEQFX eq7;
            eq7.prepare (kSr, false);

            xvdsp::Biquad eqBass, eqMid, eqTreble, eqPresence;
            eqBass.setLowShelf     (kSr, 95.0,   (snap.chBass[static_cast<size_t> (ch)] * 0.01 - 0.5) * 16.0);
            eqMid.setPeaking       (kSr, 650.0,  (snap.chMid[static_cast<size_t> (ch)] * 0.01 - 0.5) * 16.0, 1.0);
            eqTreble.setHighShelf  (kSr, 2500.0, (snap.chTreble[static_cast<size_t> (ch)] * 0.01 - 0.5) * 16.0);
            eqPresence.setHighShelf(kSr, 3800.0, (snap.chPresence[static_cast<size_t> (ch)] * 0.01 - 0.5) * 12.0);

            zenbox::ZenboxCabConvolver cab;
            cab.prepare (kSr);
            const int cabIdx = (snap.cabIndex >= 0 && snap.cabIndex < 4) ? snap.cabIndex : 1; // fallback Modern 4x12 when exporting IR

            for (int i = 0; i < kLen; ++i)
            {
                float s = buf[static_cast<size_t> (i)];
                if (hasEq)
                    s = eq7.process (s, true, snap.eqBands);
                if (! snap.headBypass)
                    s = eqPresence.processSample (eqTreble.processSample (eqMid.processSample (eqBass.processSample (s))));
                s = cab.processSample (s, cabIdx);
                buf[static_cast<size_t> (i)] = s;
            }
        }

        // Normalize to -1.0 dBFS peak (0.891) so hardware IR slots never clip
        float peak = 1e-6f;
        for (float v : buf)
            peak = std::max (peak, std::fabs (v));
        const float norm = 0.891f / peak;

        std::ofstream os (outFile.getFullPathName().toRawUTF8(), std::ios::binary | std::ios::trunc);
        if (! os)
            return false;

        auto writeU32 = [&os](uint32_t v)
        {
            const uint8_t b[4] = {
                static_cast<uint8_t> (v & 0xFFu),
                static_cast<uint8_t> ((v >> 8) & 0xFFu),
                static_cast<uint8_t> ((v >> 16) & 0xFFu),
                static_cast<uint8_t> ((v >> 24) & 0xFFu)
            };
            os.write (reinterpret_cast<const char*> (b), 4);
        };
        auto writeU16 = [&os](uint16_t v)
        {
            const uint8_t b[2] = {
                static_cast<uint8_t> (v & 0xFFu),
                static_cast<uint8_t> ((v >> 8) & 0xFFu)
            };
            os.write (reinterpret_cast<const char*> (b), 2);
        };

        const uint32_t dataBytes = static_cast<uint32_t> (kLen * 3);
        const uint32_t riffSize  = 36u + dataBytes;

        os.write ("RIFF", 4);
        writeU32 (riffSize);
        os.write ("WAVE", 4);
        os.write ("fmt ", 4);
        writeU32 (16u);                 // PCM chunk size
        writeU16 (1u);                  // PCM format
        writeU16 (1u);                  // Mono
        writeU32 (static_cast<uint32_t> (kSr));
        writeU32 (static_cast<uint32_t> (kSr * 3)); // Byte rate
        writeU16 (3u);                  // Block align (24-bit mono)
        writeU16 (24u);                 // Bits per sample
        os.write ("data", 4);
        writeU32 (dataBytes);

        for (float v : buf)
        {
            const double scaled = std::clamp (static_cast<double> (v * norm), -1.0, 1.0);
            int32_t s24 = static_cast<int32_t> (std::llround (scaled * 8388607.0));
            const uint8_t b[3] = {
                static_cast<uint8_t> (s24 & 0xFF),
                static_cast<uint8_t> ((s24 >> 8) & 0xFF),
                static_cast<uint8_t> ((s24 >> 16) & 0xFF)
            };
            os.write (reinterpret_cast<const char*> (b), 3);
        }
        return os.good();
    }

    // 2) Line 6 Helix / HX Stomp / POD Go (.hlx JSON, schema L6Preset v6)
    static bool exportHelixHlx (const MiAmpAudioProcessor& proc, const juce::String& presetName, const juce::File& outFile)
    {
        const bool bass = proc.isBassMode();
        std::ostringstream ss;
        auto esc = [](const std::string& s)
        {
            std::string r;
            for (char c : s)
            {
                if (c == '"' || c == '\\') r += '\\';
                r += c;
            }
            return r;
        };

        ss << "{\n"
           << "  \"schema\": \"L6Preset\",\n"
           << "  \"version\": 6,\n"
           << "  \"data\": {\n"
           << "    \"device\": 2162689,\n"
           << "    \"device_version\": 56623104,\n"
           << "    \"meta\": { \"name\": \"" << esc (presetName.toRawUTF8()) << "\", \"application\": \"HX Edit\", \"appversion\": 56623104 },\n"
           << "    \"tone\": {\n"
           << "      \"dsp0\": {\n"
           << "        \"inputA\": { \"@input\": 1, \"@model\": \"HD2_AppDSPFlow1Input\", \"noiseGate\": true, \"threshold\": -48.0, \"decay\": 0.5 },\n"
           << "        \"outputA\": { \"@model\": \"HD2_AppDSPFlowOutput\", \"@output\": 1, \"gain\": 0.0, \"pan\": 0.5 }";

        int blkIdx = 0;
        auto addBlock = [&](const std::string& model, int type, bool enabled, const std::string& extraParams)
        {
            ss << ",\n        \"block" << blkIdx << "\": {\n"
               << "          \"@model\": \"" << model << "\",\n"
               << "          \"@path\": 0,\n"
               << "          \"@position\": " << blkIdx << ",\n"
               << "          \"@type\": " << type << ",\n"
               << "          \"@enabled\": " << (enabled ? "true" : "false") << ",\n"
               << "          \"@stereo\": false"
               << (extraParams.empty() ? "" : (",\n          " + extraParams))
               << "\n        }";
            ++blkIdx;
        };

        if (bass)
        {
            const auto s = proc.getBassSnapshot();
            for (int pt : s.chainSlots)
            {
                if (pt < 0 || blkIdx >= 7) continue;
                switch (pt)
                {
                    case 0:  addBlock ("HD2_CompressorDeluxeComp", 0, s.compOn, "\"Threshold\": -24.0, \"Ratio\": " + std::to_string (1.0 + s.compSustain * 0.09) + ", \"Level\": " + std::to_string (s.compLevel * 0.01)); break;
                    case 1:  addBlock ("HD2_DistObsidian7000", 0, s.xvOn, "\"Drive\": " + std::to_string (s.xvDrive * 0.01) + ", \"Blend\": " + std::to_string (s.xvBlend * 0.01) + ", \"Level\": " + std::to_string (s.xvLevel * 0.01)); break;
                    case 2:  addBlock ("HD2_Chorus", 0, s.chorusOn, "\"Speed\": " + std::to_string (s.chorusRate * 0.01) + ", \"Depth\": " + std::to_string (s.chorusDepth * 0.01) + ", \"Mix\": " + std::to_string (s.chorusMix * 0.01)); break;
                    case 3:  addBlock ("HD2_PhaserScriptModPhase", 0, s.phaserOn, "\"Rate\": " + std::to_string (s.phaserRate * 0.01)); break;
                    case 4:  addBlock ("HD2_ReverbPlate", 7, s.reverbOn, "\"Decay\": " + std::to_string (s.reverbDecay * 0.01) + ", \"Mix\": " + std::to_string (s.reverbMix * 0.01)); break;
                    case 5:  addBlock ("HD2_DistZeroAmpBassDI", 0, s.bddiOn, "\"Drive\": " + std::to_string (s.bddiDrive * 0.01) + ", \"Blend\": " + std::to_string (s.bddiBlend * 0.01) + ", \"Level\": " + std::to_string (s.bddiLevel * 0.01)); break;
                    case 6:  addBlock ("HD2_DistMinotaur", 0, s.goldOn, "\"Gain\": " + std::to_string (s.goldGain * 0.01) + ", \"Tone\": " + std::to_string (s.goldEq * 0.01) + ", \"Level\": " + std::to_string (s.goldLevel * 0.01)); break;
                    case 7:  addBlock ("HD2_DistClawthornDrive", 0, s.foodOn, "\"Drive\": " + std::to_string (s.foodDrive * 0.01) + ", \"Level\": " + std::to_string (s.foodVol * 0.01)); break;
                    case 8:  addBlock ("HD2_DistArbitratorFuzz", 0, s.oliveOn, "\"Fuzz\": " + std::to_string (s.oliveSustain * 0.01) + ", \"Level\": " + std::to_string (s.oliveVol * 0.01)); break;
                    case 9:  addBlock ("HD2_FlangerDeluxeFlanger", 0, s.flangeOn, "\"Speed\": " + std::to_string (s.flangeRate * 0.01) + ", \"Depth\": " + std::to_string (s.flangeDepth * 0.01)); break;
                    case 10: addBlock ("HD2_DelaySimpleDelay", 7, s.delayOn, "\"Time\": " + std::to_string (0.1 + s.delayTime * 0.006) + ", \"Feedback\": " + std::to_string (s.delayFb * 0.01) + ", \"Mix\": " + std::to_string (s.delayMix * 0.01)); break;
                    case 11: addBlock ("HD2_GateNoiseGate", 0, s.gateOn, "\"Threshold\": " + std::to_string (-75.0 + s.gateThresh * 0.5)); break;
                    case 12: addBlock ("HD2_PitchSimplePitch", 0, s.octOn, "\"Interval\": -12, \"Mix\": " + std::to_string (s.octSub * 0.01)); break;
                    case 13: addBlock ("HD2_EQGraphic10Band", 0, s.eqOn, "\"Level\": 0.0"); break;
                    default: break;
                }
            }
            if (! s.headBypass)
            {
                addBlock ("HD2_AmpSVBeastNrm", 1, true, "\"Drive\": " + std::to_string (s.ampGain * 0.01) + ", \"Bass\": " + std::to_string (s.ampBass * 0.01) + ", \"Mid\": " + std::to_string (s.ampMid * 0.01) + ", \"Treble\": " + std::to_string (s.ampTreble * 0.01));
                if (s.ampCabOn)
                    addBlock ("HD2_Cab8x10SVBeast", 2, true, "\"Mic\": 0");
            }
        }
        else
        {
            const auto s = proc.getGuitarSnapshot();
            for (int pt : s.chainSlots)
            {
                if (pt < 0 || blkIdx >= 7) continue;
                switch (pt)
                {
                    case 0:  addBlock ("HD2_CompressorDeluxeComp", 0, s.compOn, "\"Threshold\": -24.0, \"Level\": " + std::to_string (s.compLevel * 0.01)); break;
                    case 1:  addBlock ("HD2_DistScream808", 0, s.driveOn, "\"Gain\": " + std::to_string (s.driveGain * 0.01) + ", \"Tone\": " + std::to_string (s.driveTone * 0.01) + ", \"Level\": " + std::to_string (s.driveLevel * 0.01)); break;
                    case 2:  addBlock ("HD2_DistArbitratorFuzz", 0, s.fuzzOn, "\"Fuzz\": " + std::to_string (s.fuzzFuzz * 0.01) + ", \"Level\": " + std::to_string (s.fuzzLevel * 0.01)); break;
                    case 3:  addBlock ("HD2_Chorus", 0, s.chorusOn, "\"Speed\": " + std::to_string (s.chorusRate * 0.01) + ", \"Depth\": " + std::to_string (s.chorusDepth * 0.01) + ", \"Mix\": " + std::to_string (s.chorusMix * 0.01)); break;
                    case 4:  addBlock ("HD2_PhaserScriptModPhase", 0, s.phaserOn, "\"Rate\": " + std::to_string (s.phaserRate * 0.01)); break;
                    case 5:  addBlock ("HD2_DelaySimpleDelay", 7, s.delayOn, "\"Time\": " + std::to_string (0.1 + s.delayTime * 0.006) + ", \"Feedback\": " + std::to_string (s.delayFeedback * 0.01) + ", \"Mix\": " + std::to_string (s.delayMix * 0.01)); break;
                    case 6:  addBlock ("HD2_ReverbPlate", 7, s.reverbOn, "\"Decay\": " + std::to_string (s.reverbDecay * 0.01) + ", \"Mix\": " + std::to_string (s.reverbMix * 0.01)); break;
                    case 7:  addBlock ("HD2_DistRatatouilleDist", 0, s.distOn, "\"Distortion\": " + std::to_string (s.distDrive * 0.01) + ", \"Filter\": " + std::to_string (s.distTone * 0.01) + ", \"Level\": " + std::to_string (s.distLevel * 0.01)); break;
                    case 8:  addBlock ("HD2_DistTeemah", 0, s.dualOn, "\"Gain\": " + std::to_string (s.dualDrive * 0.01) + ", \"Treble\": " + std::to_string (s.dualTone * 0.01) + ", \"Level\": " + std::to_string (s.dualOutput * 0.01)); break;
                    case 9:  addBlock ("HD2_FlangerDeluxeFlanger", 0, s.flangeOn, "\"Speed\": " + std::to_string (s.flangeRate * 0.01) + ", \"Depth\": " + std::to_string (s.flangeDepth * 0.01)); break;
                    case 10: addBlock ("HD2_GateNoiseGate", 0, s.gatePedalOn, "\"Threshold\": " + std::to_string (-75.0 + s.gatePedalThresh * 0.5)); break;
                    case 11: addBlock ("HD2_PitchSimplePitch", 0, s.octOn, "\"Interval\": -12, \"Mix\": " + std::to_string (s.octSub1 * 0.01)); break;
                    case 12: addBlock ("HD2_EQGraphic10Band", 0, s.eqOn, "\"Level\": 0.0"); break;
                    default: break;
                }
            }
            if (! s.headBypass)
            {
                const int ch = std::clamp (s.activeChannel, 0, 2);
                const char* ampModel = (ch == 0) ? "HD2_AmpUSDoubleNrm"
                                     : (ch == 1) ? "HD2_AmpBrit2204"
                                                 : "HD2_AmpCaliRectifire";
                addBlock (ampModel, 1, true, "\"Drive\": " + std::to_string (s.chGain[static_cast<size_t> (ch)] * 0.01) + ", \"Bass\": " + std::to_string (s.chBass[static_cast<size_t> (ch)] * 0.01) + ", \"Mid\": " + std::to_string (s.chMid[static_cast<size_t> (ch)] * 0.01) + ", \"Treble\": " + std::to_string (s.chTreble[static_cast<size_t> (ch)] * 0.01) + ", \"Presence\": " + std::to_string (s.chPresence[static_cast<size_t> (ch)] * 0.01));
                if (s.cabIndex < 4)
                    addBlock ("HD2_Cab4x12CaliV30", 2, true, "\"Mic\": 0");
            }
        }

        ss << "\n      },\n      \"dsp1\": {}\n    }\n  }\n}\n";
        return writeTextFile (outFile, juce::String (ss.str()));
    }

    // 3) Headrush (.rig JSON)
    static bool exportHeadrushRig (const MiAmpAudioProcessor& proc, const juce::String& presetName, const juce::File& outFile)
    {
        const bool bass = proc.isBassMode();
        std::ostringstream ss;
        ss << "{\n  \"name\": \"" << presetName.replace ("\"", "'").toRawUTF8() << "\",\n"
           << "  \"instrument\": \"" << (bass ? "Bass" : "Guitar") << "\",\n"
           << "  \"route\": \"" << (proc.isHeadBypassed (bass) ? "PedalsOnly" : "AmpAndCab") << "\",\n"
           << "  \"slots\": [\n";
        const auto chain = proc.getActiveChain (bass);
        for (size_t i = 0; i < chain.size(); ++i)
        {
            ss << "    { \"index\": " << i << ", \"miAmpPedalId\": " << chain[i] << " }"
               << (i + 1 < chain.size() ? "," : "") << "\n";
        }
        ss << "  ]\n}\n";
        return writeTextFile (outFile, juce::String (ss.str()));
    }

    // 4) Guia de Mapeo M-Vave Blackbox & Universal (.txt)
    static bool exportBlackboxGuideTxt (const MiAmpAudioProcessor& proc, const juce::String& presetName, const juce::File& outFile)
    {
        const bool bass = proc.isBassMode();
        std::ostringstream ss;
        ss << "============================================================\n"
           << "  MI AMP -> GUIA PARA M-VAVE BLACKBOX & PEDALERAS DIGITALES\n"
           << "  Preset: " << presetName.toRawUTF8() << "\n"
           << "  Instrumento: " << (bass ? "BAJO" : "GUITARRA") << "\n"
           << "============================================================\n\n"
           << "NOTA SOBRE M-VAVE BLACKBOX:\n"
           << "El formato interno de preset de M-EFCS (.apreset) es binario cerrado.\n"
           << "Para llevar este tono a tu M-Vave Blackbox:\n"
           << "  1) Exporta tambien el formato 'IR Cabina + EQ (.wav)' desde MI AMP\n"
           << "     y cargalo en un slot IR CAB de la Blackbox via M-EFCS.\n"
           << "  2) Copia estos ajustes en los modulos de tu Blackbox:\n\n";

        if (bass)
        {
            const auto s = proc.getBassSnapshot();
            ss << "[MOD. NR / GATE]   " << (s.gateOn ? "ON" : "OFF") << " | Threshold: " << static_cast<int> (s.gateThresh) << "%\n";
            ss << "[MOD. FX1 / COMP]  " << (s.compOn ? "ON" : "OFF") << " | Ratio/Sustain: " << static_cast<int> (s.compSustain) << "% | Output: " << static_cast<int> (s.compLevel) << "%\n";
            ss << "[MOD. AMP/PREAMP]  " << (s.headBypass ? "PREAMP PEDAL (XV/BDDI)" : "8x10 BASS HEAD") << "\n";
            if (! s.headBypass)
                ss << "   -> Gain: " << static_cast<int> (s.ampGain) << "% | Bass: " << static_cast<int> (s.ampBass) << "% | Mid: " << static_cast<int> (s.ampMid) << "% | Treble: " << static_cast<int> (s.ampTreble) << "%\n";
            if (s.xvOn)
                ss << "   -> XV Preamp: Drive " << static_cast<int> (s.xvDrive) << "% | Blend " << static_cast<int> (s.xvBlend) << "% | Low " << static_cast<int> (s.xvLow) << "% | Mid " << static_cast<int> (s.xvMid) << "% | High " << static_cast<int> (s.xvHigh) << "%\n";
            if (s.bddiOn)
                ss << "   -> Bass Driver: Drive " << static_cast<int> (s.bddiDrive) << "% | Blend " << static_cast<int> (s.bddiBlend) << "% | Bass " << static_cast<int> (s.bddiBass) << "% | Treble " << static_cast<int> (s.bddiTreble) << "%\n";
            ss << "[MOD. MOD]         " << (s.chorusOn ? "Chorus ON" : (s.phaserOn ? "Phaser ON" : (s.flangeOn ? "Flanger ON" : "OFF"))) << "\n";
            ss << "[MOD. DLY]         " << (s.delayOn ? "ON" : "OFF") << " | Time: " << static_cast<int> (s.delayTime) << "% | Fb: " << static_cast<int> (s.delayFb) << "% | Mix: " << static_cast<int> (s.delayMix) << "%\n";
            ss << "[MOD. RVB]         " << (s.reverbOn ? "ON" : "OFF") << " | Decay: " << static_cast<int> (s.reverbDecay) << "% | Mix: " << static_cast<int> (s.reverbMix) << "%\n";
            ss << "[MOD. IR CAB]      " << (s.ampCabOn ? "ON (usar .wav exportado desde MI AMP)" : "OFF") << "\n";
        }
        else
        {
            const auto s = proc.getGuitarSnapshot();
            ss << "[MOD. NR / GATE]   " << ((s.gatePedalOn || s.gateThresh > 5.f) ? "ON" : "OFF") << " | Threshold: " << static_cast<int> (std::max (s.gateThresh, s.gatePedalThresh)) << "%\n";
            ss << "[MOD. FX1 / COMP]  " << (s.compOn ? "ON" : "OFF") << " | Sustain: " << static_cast<int> (s.compSustain) << "% | Level: " << static_cast<int> (s.compLevel) << "%\n";
            ss << "[MOD. FX1 / DRIVE] " << (s.driveOn ? "Green OD ON" : (s.dualOn ? "Dual Crunch ON" : (s.distOn ? "Dist ON" : (s.fuzzOn ? "Fuzz ON" : "OFF")))) << "\n";
            if (s.dualOn)
                ss << "   -> Dual Crunch: Gain " << static_cast<int> (s.dualGain) << "% | Drive " << static_cast<int> (s.dualDrive) << "% | Tone " << static_cast<int> (s.dualTone) << "% | Output " << static_cast<int> (s.dualOutput) << "%\n";
            if (s.driveOn)
                ss << "   -> Green Drive: Gain " << static_cast<int> (s.driveGain) << "% | Tone " << static_cast<int> (s.driveTone) << "% | Level " << static_cast<int> (s.driveLevel) << "%\n";
            const int ch = std::clamp (s.activeChannel, 0, 2);
            ss << "[MOD. AMP]         " << (s.headBypass ? "SOLO PEDALES (usar preamp/drive)" : (ch == 0 ? "CH1 Clean" : (ch == 1 ? "CH2 Crunch" : "CH3 High-Gain"))) << "\n";
            if (! s.headBypass)
                ss << "   -> Gain: " << static_cast<int> (s.chGain[static_cast<size_t> (ch)]) << "% | Bass: " << static_cast<int> (s.chBass[static_cast<size_t> (ch)]) << "% | Mid: " << static_cast<int> (s.chMid[static_cast<size_t> (ch)]) << "% | Treble: " << static_cast<int> (s.chTreble[static_cast<size_t> (ch)]) << "% | Pres: " << static_cast<int> (s.chPresence[static_cast<size_t> (ch)]) << "%\n";
            ss << "[MOD. MOD]         " << (s.chorusOn ? "Chorus ON" : (s.phaserOn ? "Phaser ON" : (s.flangeOn ? "Flanger ON" : "OFF"))) << "\n";
            ss << "[MOD. DLY]         " << (s.delayOn ? "ON" : "OFF") << " | Time: " << static_cast<int> (s.delayTime) << "% | Fb: " << static_cast<int> (s.delayFeedback) << "% | Mix: " << static_cast<int> (s.delayMix) << "%\n";
            ss << "[MOD. RVB]         " << (s.reverbOn ? "ON" : "OFF") << " | Decay: " << static_cast<int> (s.reverbDecay) << "% | Mix: " << static_cast<int> (s.reverbMix) << "%\n";
            ss << "[MOD. IR CAB]      " << (s.cabIndex < 4 ? "ON (usar .wav exportado desde MI AMP)" : "OFF") << "\n";
        }
        return writeTextFile (outFile, juce::String (ss.str()));
    }

    // 5) Portable MI AMP XML (.miamp)
    static bool exportMiAmpXml (MiAmpAudioProcessor& proc, const juce::String& presetName, const juce::File& outFile)
    {
        return writeTextFile (outFile, proc.exportStateAsXmlString (presetName));
    }
};

} // namespace mipedals
