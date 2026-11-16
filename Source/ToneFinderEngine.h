#pragma once
#include "PluginProcessor.h"
#include <vector>
#include <array>
#include <utility>
#include <algorithm>
#include <cmath>

namespace mipedals
{

struct ParamOverride
{
    const char* paramId = nullptr;
    float value = 0.0f;
};

struct ToneMatchResult
{
    bool found = false;
    juce::String title;
    juce::String sourceBadge;   // "Rig Verificado", "Web (MusicBrainz/Wiki)", "IA Gemini + Search"
    juce::String explanation;
    bool bassMode = false;
    bool headBypass = false;
    int  dropSemi = 0;

    // Optional Guitar head snapshot (used when !bassMode && !headBypass)
    int  gtrChannel = 2;
    int  gtrCab = 1;
    bool gtrBright = true;
    float gtrGain = 66.0f, gtrBass = 54.0f, gtrMid = 60.0f, gtrTreble = 62.0f, gtrPresence = 60.0f, gtrMaster = 58.0f;

    // Optional Bass head snapshot (used when bassMode && !headBypass)
    float bassGain = 52.0f, bassLow = 54.0f, bassMid = 52.0f, bassFreq = 50.0f, bassTreble = 54.0f;
    bool  bassUltraHi = false;
    bool  bassCabOn = true;

    std::vector<int> chainSlots;
    std::vector<ParamOverride> overrides;
};

class ToneFinderEngine
{
public:
    static juce::String normalizeQuery (const juce::String& raw)
    {
        juce::String s = raw.toLowerCase();
        struct Repl { const char* a; const char* b; };
        static constexpr Repl kAccents[] = {
            { u8"\u00e1", "a" }, { u8"\u00e9", "e" }, { u8"\u00ed", "i" }, { u8"\u00f3", "o" }, { u8"\u00fa", "u" },
            { u8"\u00f1", "n" }, { u8"\u00fc", "u" }, { u8"\u2019", "" }, { "'", "" }, { "-", " " }, { "/", " " }, { ",", " " }, { ".", " " }
        };
        for (const auto& r : kAccents)
            s = s.replace (juce::String::fromUTF8 (r.a), r.b);
        return s.trim();
    }

    static bool queryImpliesBass (const juce::String& qNorm)
    {
        static constexpr const char* kBassHints[] = {
            "bajo", "bass", "bajista", "bassist", "slap", "flea", "kelen capener", "mark hoppus",
            "justin chancellor", "hysteria", "chris wolstenholme", "mike dirnt", "matt freeman",
            "cliff burton", "geddy lee", "steve harris", "jaco", "marcus miller", "pbass", "jazz bass"
        };
        for (const char* w : kBassHints)
            if (qNorm.contains (w))
                return true;
        return false;
    }

    static ToneMatchResult buildArchetype (const juce::String& title,
                                           const juce::String& keywords,
                                           const juce::String& badge,
                                           const juce::String& explanation,
                                           bool bassMode,
                                           bool headBypass,
                                           int dropSemi,
                                           int ch, int cab, bool bright,
                                           float g, float b, float m, float t, float p, float mast,
                                           std::vector<int> slots,
                                           std::vector<ParamOverride> overrides)
    {
        ToneMatchResult r;
        r.found = true;
        r.title = title;
        r.sourceBadge = badge;
        r.explanation = explanation;
        r.bassMode = bassMode;
        r.headBypass = headBypass;
        r.dropSemi = dropSemi;
        if (bassMode)
        {
            r.bassGain = g; r.bassLow = b; r.bassMid = m; r.bassFreq = p; r.bassTreble = t;
            r.bassUltraHi = bright; r.bassCabOn = (cab == 0);
        }
        else
        {
            r.gtrChannel = ch; r.gtrCab = cab; r.gtrBright = bright;
            r.gtrGain = g; r.gtrBass = b; r.gtrMid = m; r.gtrTreble = t; r.gtrPresence = p; r.gtrMaster = mast;
        }
        r.chainSlots = std::move (slots);
        r.overrides = std::move (overrides);
        juce::ignoreUnused (keywords);
        return r;
    }

    struct CatalogEntry
    {
        juce::String keywords;
        ToneMatchResult result;
    };

    static const std::vector<CatalogEntry>& getCatalog()
    {
        static const std::vector<CatalogEntry> cat = []()
        {
            std::vector<CatalogEntry> v;
            auto add = [&](const char* kw, const char* title, const char* exp,
                           bool bass, bool bypass, int drop,
                           int ch, int cab, bool br,
                           float g, float b, float m, float t, float p, float mast,
                           std::vector<int> slots, std::vector<ParamOverride> ov)
            {
                v.push_back ({
                    juce::String::fromUTF8 (kw),
                    buildArchetype (juce::String::fromUTF8 (title), juce::String::fromUTF8 (kw),
                                    "Base Rig Real", juce::String::fromUTF8 (exp),
                                    bass, bypass, drop, ch, cab, br, g, b, m, t, p, mast,
                                    std::move (slots), std::move (ov))
                });
            };

            // === THE STORY SO FAR (Guitar & Bass, real rigs verified from Equipboard) ===
            add ("story so far will levy kevin geyer proper dose what you dont see quicksand nerve empty space out of it heavy gloom pop punk",
                 "The Story So Far \u2022 Pop-Punk High Gain",
                 "Cabezal Dual CH3 + Cab Modern 4x12, empujado con Green Drive (TS), Benson/Dual Crunch sutil, Gate y Chorus/Reverb para estribillos.",
                 false, false, -2, 2, 1, true, 72.f, 54.f, 64.f, 64.f, 66.f, 58.f,
                 { 10, 1, 3, 6 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 34.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 22.f }, { MiAmpParams::gtrDriveTone, 64.f }, { MiAmpParams::gtrDriveLvl, 68.f },
                     { MiAmpParams::gtrChorusOn, 0.f }, { MiAmpParams::gtrChorusRate, 32.f }, { MiAmpParams::gtrChorusDepth, 42.f }, { MiAmpParams::gtrChorusMix, 24.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 42.f }, { MiAmpParams::gtrReverbTone, 56.f }, { MiAmpParams::gtrReverbMix, 18.f }
                 });

            add ("story so far clairvoyant placeholder navy blue keep this up take me as you please clean acoustic indie",
                 "The Story So Far \u2022 Clairvoyant Clean",
                 u8"Canal Clean CH1 en Open 2x12 + Compresor + Chorus estereo + Studio Reverb para arpegios limpios.",
                 false, false, 0, 0, 2, true, 30.f, 48.f, 56.f, 64.f, 58.f, 62.f,
                 { 0, 3, 6 },
                 {
                     { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 54.f }, { MiAmpParams::gtrCompAtt, 38.f }, { MiAmpParams::gtrCompLvl, 58.f },
                     { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 34.f }, { MiAmpParams::gtrChorusDepth, 52.f }, { MiAmpParams::gtrChorusMix, 38.f }, { MiAmpParams::gtrChorusMode, 0.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 58.f }, { MiAmpParams::gtrReverbTone, 60.f }, { MiAmpParams::gtrReverbMix, 34.f }
                 });

            add ("story so far bass kelen capener bajo pop punk empty space quicksand proper dose nerve heavy gloom clairvoyant will levy",
                 "The Story So Far \u2022 Bajo (8x10 + BDDI)",
                 u8"MI-87 Compresor siempre activo + MI Bass Driver con presencia de p\u00faa sobre cabezal 8x10 cl\u00e1sico.",
                 true, false, -2, 0, 0, false, 56.f, 56.f, 58.f, 60.f, 52.f, 55.f,
                 { 0, 5 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 58.f }, { MiAmpParams::compAttack, 34.f }, { MiAmpParams::compLevel, 58.f }, { MiAmpParams::compInput, 62.f },
                     { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 44.f }, { MiAmpParams::bddiBlend, 64.f }, { MiAmpParams::bddiTreble, 62.f }, { MiAmpParams::bddiPresence, 72.f }, { MiAmpParams::bddiBass, 58.f }, { MiAmpParams::bddiLevel, 58.f }
                 });

            // === POP-PUNK / PUNK / HARDCORE ===
            add ("blink 182 enema dammit whats my age again all the small things anthem feeling this tom delonge pop punk",
                 "90s/00s Cali Pop-Punk \u2022 Dual Crunch",
                 "Cabezal Dual High-Gain + Brit Crunch en Cab Modern 4x12 con Green Drive adelante y corte firme.",
                 false, false, 0, 2, 1, true, 68.f, 56.f, 60.f, 64.f, 66.f, 60.f,
                 { 10, 1, 3 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 28.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 18.f }, { MiAmpParams::gtrDriveTone, 66.f }, { MiAmpParams::gtrDriveLvl, 64.f },
                     { MiAmpParams::gtrChorusOn, 0.f }, { MiAmpParams::gtrChorusMix, 24.f }
                 });

            add ("blink 182 enema dammit whats my age again all the small things anthem feeling this cali pop punk bass mark hoppus bajo",
                 "90s/00s Cali Pop-Punk \u2022 Bajo XV Directo",
                 u8"Modo SOLO PEDALES con XV Preamp puro (Cab PUNK) + Compresor para ataque cristalino de p\u00faa id\u00e9ntico al original.",
                 true, true, 0, 0, 0, false, 50.f, 50.f, 50.f, 50.f, 50.f, 50.f,
                 { 0, 1 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 52.f }, { MiAmpParams::compAttack, 30.f }, { MiAmpParams::compLevel, 56.f },
                     { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvLevel, 58.f }, { MiAmpParams::xvDrive, 46.f }, { MiAmpParams::xvLow, 66.f },
                     { MiAmpParams::xvHigh, 58.f }, { MiAmpParams::xvBlend, 66.f }, { MiAmpParams::xvMid, 28.f }, { MiAmpParams::xvPresence, 84.f }, { MiAmpParams::xvCab, 1.f }
                 });

            add ("turnstile glow on blackout mystery hardcore punk pat mccrory brady ebert",
                 "Turnstile \u2022 Hardcore Groove & Chorus",
                 u8"Gate r\u00e1pido + Green Drive + Cabezal CH3 saturado y Sky Chorus para partes mel\u00f3dicas.",
                 false, false, -2, 2, 1, true, 76.f, 58.f, 62.f, 64.f, 64.f, 60.f,
                 { 10, 1, 3, 6 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 42.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 28.f }, { MiAmpParams::gtrDriveTone, 64.f }, { MiAmpParams::gtrDriveLvl, 68.f },
                     { MiAmpParams::gtrChorusOn, 0.f }, { MiAmpParams::gtrChorusRate, 40.f }, { MiAmpParams::gtrChorusDepth, 55.f }, { MiAmpParams::gtrChorusMix, 34.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbMix, 20.f }
                 });

            add ("neck deep december in bloom gold steps life not out to get you pop punk",
                 "Neck Deep \u2022 Modern UK Pop-Punk",
                 u8"CH3 apretado con Green Drive + EQ gr\u00e1fico realzando medios altos para riffs r\u00e1pidos.",
                 false, false, -2, 2, 1, true, 70.f, 52.f, 64.f, 66.f, 66.f, 58.f,
                 { 10, 1, 12 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 34.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 24.f }, { MiAmpParams::gtrDriveTone, 66.f }, { MiAmpParams::gtrDriveLvl, 66.f },
                     { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[0], 45.f }, { MiAmpParams::gtrEqBand[2], 58.f }, { MiAmpParams::gtrEqBand[4], 62.f }, { MiAmpParams::gtrEqBand[5], 58.f }
                 });

            add ("green day dookie basket case american idiot billie joe armstrong crunch punk",
                 "90s Punk Crunch \u2022 Brit High-Mid",
                 "Canal CH2 Crunch con medios empujados en Vintage 4x12 y overdrive leve de ganancia.",
                 false, false, -1, 1, 0, true, 64.f, 52.f, 70.f, 62.f, 58.f, 62.f,
                 { 1, 12 },
                 {
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 20.f }, { MiAmpParams::gtrDriveTone, 60.f }, { MiAmpParams::gtrDriveLvl, 62.f },
                     { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[2], 60.f }, { MiAmpParams::gtrEqBand[3], 64.f }
                 });

            add ("green day mike dirnt dookie longview bass bajo punk",
                 "90s Punk Bass \u2022 Pick Mid-Punch",
                 "Bass Food + Cabezal 8x10 con medios presentes (400-800 Hz) para cortar mezcla.",
                 true, false, -1, 0, 0, false, 60.f, 54.f, 66.f, 62.f, 54.f, 55.f,
                 { 0, 7, 13 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 52.f }, { MiAmpParams::compLevel, 58.f },
                     { MiAmpParams::bfOn, 1.f }, { MiAmpParams::bfDrive, 48.f }, { MiAmpParams::bfBlend, 60.f }, { MiAmpParams::bfTreble, 60.f },
                     { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[2], 60.f }, { MiAmpParams::bassEqBand[4], 64.f }
                 });

            add ("paramore misery business crushcrushcrush thats what you get Hayley pop punk",
                 "2000s Alt Pop-Punk \u2022 Tight Dual",
                 "CH3 con gate preciso, Green Drive + Dual Crunch para pared de guitarras.",
                 false, false, -2, 2, 1, true, 72.f, 54.f, 62.f, 65.f, 64.f, 58.f,
                 { 10, 1, 6 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 36.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 26.f }, { MiAmpParams::gtrDriveTone, 64.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbMix, 16.f }
                 });

            add ("sum 41 fat lip in too deep nofx punk rock",
                 "Fast Skate Punk \u2022 Tight Bite",
                 "CH3 definido, graves controlados y medios presentes para palm-mutes veloces.",
                 false, false, 0, 2, 1, true, 70.f, 50.f, 62.f, 66.f, 68.f, 58.f,
                 { 10, 1 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 38.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 22.f }, { MiAmpParams::gtrDriveTone, 68.f }
                 });

            // === ALTERNATIVE / NU-METAL / METAL / HARDCORE ===
            add ("deftones my own summer around the fur white pony stephen carpenter nu metal drop",
                 "Deftones \u2022 Heavy Wall & Shimmer",
                 "Drop -3 semitonos + Gate + High Gain CH3 con graves gruesos, EQ y Chorus/Delay espacial.",
                 false, false, -3, 2, 1, true, 80.f, 64.f, 56.f, 64.f, 62.f, 60.f,
                 { 10, 1, 3, 5, 6 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 44.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 24.f }, { MiAmpParams::gtrDriveTone, 60.f },
                     { MiAmpParams::gtrChorusOn, 0.f }, { MiAmpParams::gtrChorusMix, 28.f },
                     { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 46.f }, { MiAmpParams::gtrDelayMix, 26.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbMix, 24.f }
                 });

            add ("metallica master of puppets enter sandman thrash metal scoop",
                 "80s Thrash Metal \u2022 Mid Scoop",
                 u8"CH3 ganancia alta + EQ 7-Bandas en V cl\u00e1sica (800 Hz recortados, 100 Hz y 3.2 kHz arriba).",
                 false, false, 0, 2, 1, true, 78.f, 64.f, 36.f, 68.f, 64.f, 60.f,
                 { 10, 1, 12 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 42.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 20.f }, { MiAmpParams::gtrDriveLvl, 68.f },
                     { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[0], 64.f }, { MiAmpParams::gtrEqBand[2], 38.f }, { MiAmpParams::gtrEqBand[3], 32.f }, { MiAmpParams::gtrEqBand[5], 68.f }
                 });

            add ("knocked loose metalcore hardcore beatdown modern metal djent drop",
                 "Modern Metalcore \u2022 Ultra Tight Gate",
                 "Drop -4 semitonos + Gate agresivo + Green Drive Boost + CH3 + EQ corte de barro en 200 Hz.",
                 false, false, -4, 2, 1, true, 82.f, 50.f, 62.f, 68.f, 70.f, 58.f,
                 { 10, 1, 12 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 58.f }, { MiAmpParams::gtrGateDecay, 18.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 12.f }, { MiAmpParams::gtrDriveTone, 70.f }, { MiAmpParams::gtrDriveLvl, 76.f },
                     { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[1], 36.f }, { MiAmpParams::gtrEqBand[4], 64.f }
                 });

            // === BASS ICONIC TONES ===
            add ("muse hysteria chris wolstenholme fuzz synth bass bajo",
                 "Muse Hysteria \u2022 Fuzz + Octaver Synth",
                 u8"MI Olive Fuzz (Boost) + Poly Octave + XV Preamp en SOLO PEDALES para bajo sint\u00e9tico grueso.",
                 true, true, 0, 0, 0, false, 50.f, 50.f, 50.f, 50.f, 50.f, 50.f,
                 { 0, 12, 8, 1 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 60.f },
                     { MiAmpParams::bassOctOn, 1.f }, { MiAmpParams::bassOctDry, 78.f }, { MiAmpParams::bassOctSub, 68.f }, { MiAmpParams::bassOctUp, 22.f },
                     { MiAmpParams::oliveOn, 1.f }, { MiAmpParams::oliveSustain, 78.f }, { MiAmpParams::oliveTone, 58.f }, { MiAmpParams::oliveMode, 2.f },
                     { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvDrive, 52.f }, { MiAmpParams::xvBlend, 68.f }, { MiAmpParams::xvCab, 1.f }
                 });

            add ("tool schism forty six and 2 justin chancellor bass bajo",
                 "Prog Bass \u2022 Pick Growl + Flanger",
                 "Drop -2 semitonos + Bass Driver + Violet Flanger sutil + cabezal 8x10 con medios rugientes.",
                 true, false, -2, 0, 0, false, 60.f, 54.f, 68.f, 64.f, 58.f, 55.f,
                 { 0, 5, 9, 10 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 56.f },
                     { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 54.f }, { MiAmpParams::bddiBlend, 66.f }, { MiAmpParams::bddiPresence, 74.f },
                     { MiAmpParams::bassFlangeOn, 0.f }, { MiAmpParams::bassFlangeDepth, 45.f }, { MiAmpParams::bassFlangeRate, 24.f },
                     { MiAmpParams::bassDelayOn, 0.f }, { MiAmpParams::bassDelayMix, 24.f }
                 });

            add ("flea red hot chili peppers slap funk bass bajo",
                 "Funk Slap Bass \u2022 Scooped & Punchy",
                 "MI-87 ataque rápido + Bass EQ en sonrisa (50 Hz y 4.5 kHz arriba) en modo SOLO PEDALES con XV Clean.",
                 true, true, 0, 0, 0, false, 50.f, 50.f, 50.f, 50.f, 50.f, 50.f,
                 { 0, 1, 13 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 72.f }, { MiAmpParams::compAttack, 24.f }, { MiAmpParams::compLevel, 62.f },
                     { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvDrive, 15.f }, { MiAmpParams::xvLow, 72.f }, { MiAmpParams::xvHigh, 74.f }, { MiAmpParams::xvMid, 24.f }, { MiAmpParams::xvCab, 2.f },
                     { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 66.f }, { MiAmpParams::bassEqBand[2], 36.f }, { MiAmpParams::bassEqBand[5], 72.f }
                 });

            add ("xv original solo directo pop punk bass bajo",
                 "XV Original \u2022 Solo Pedales (Puro)",
                 u8"Desactiva el cabezal (SOLO PEDALES) para que el XV Preamp suene 100% id\u00e9ntico al plugin original.",
                 true, true, 0, 0, 0, false, 50.f, 50.f, 50.f, 50.f, 50.f, 50.f,
                 { 1 },
                 {
                     { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvLevel, 56.f }, { MiAmpParams::xvDrive, 40.8f },
                     { MiAmpParams::xvLow, 65.4f }, { MiAmpParams::xvHigh, 53.5f }, { MiAmpParams::xvBlend, 57.6f },
                     { MiAmpParams::xvMid, 25.8f }, { MiAmpParams::xvPresence, 80.4f }, { MiAmpParams::xvCab, 1.f }
                 });

            // === INDIE / EMO / SHOEGAZE / CLASSIC ROCK / GRUNGE ===
            add ("midwest emo math rock american football clean tele twinkle",
                 "Midwest Emo \u2022 Sparkle Clean & Comp",
                 "MI Ice Comp sustain alto + Studio Reverb + CH1 Open 2x12 brillante para tapping y arpegios.",
                 false, false, 0, 0, 2, true, 32.f, 44.f, 58.f, 68.f, 64.f, 64.f,
                 { 0, 5, 6 },
                 {
                     { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 72.f }, { MiAmpParams::gtrCompAtt, 28.f }, { MiAmpParams::gtrCompLvl, 62.f }, { MiAmpParams::gtrCompTone, 64.f },
                     { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 34.f }, { MiAmpParams::gtrDelayMix, 22.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 54.f }, { MiAmpParams::gtrReverbMix, 30.f }
                 });

            add ("title fight floral green shed shoegaze grunge chorus",
                 "Shoegaze / Post-Hardcore \u2022 Wall & Chorus",
                 "Dual Crunch + Sky Chorus ancho + Pearl Delay + Reverb larga para acordes abiertos envolventes.",
                 false, false, -1, 1, 0, true, 64.f, 58.f, 60.f, 60.f, 58.f, 60.f,
                 { 8, 3, 5, 6 },
                 {
                     { MiAmpParams::gtrDualOn, 1.f }, { MiAmpParams::gtrDualDrive, 62.f }, { MiAmpParams::gtrDualBlend, 58.f },
                     { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusDepth, 68.f }, { MiAmpParams::gtrChorusMix, 46.f },
                     { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 48.f }, { MiAmpParams::gtrDelayMix, 34.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 72.f }, { MiAmpParams::gtrReverbMix, 42.f }
                 });

            add ("nirvana nevermind smells like teen spirit kurt cobain grunge distortion",
                 "90s Grunge \u2022 MI Orange + Chorus",
                 "MI Orange Distortion empujando CH1 limpio + Sky Chorus en estribillos.",
                 false, false, 0, 0, 0, true, 42.f, 56.f, 58.f, 62.f, 56.f, 62.f,
                 { 7, 3 },
                 {
                     { MiAmpParams::gtrDistOn, 1.f }, { MiAmpParams::gtrDistDrive, 72.f }, { MiAmpParams::gtrDistTone, 58.f }, { MiAmpParams::gtrDistLevel, 64.f },
                     { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 46.f }, { MiAmpParams::gtrChorusDepth, 64.f }, { MiAmpParams::gtrChorusMix, 40.f }
                 });

            add ("foo fighters everlong the pretender rock modern drive",
                 "Alt Rock Stadium \u2022 Big Crunch",
                 "CH2 + Green Drive sutil en Modern 4x12, pegada ancha y definición en acordes.",
                 false, false, -2, 1, 1, true, 66.f, 56.f, 62.f, 62.f, 60.f, 60.f,
                 { 10, 1, 6 },
                 {
                     { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 26.f },
                     { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 24.f }, { MiAmpParams::gtrDriveTone, 60.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbMix, 18.f }
                 });

            add ("stoner doom queens of the stone age kyuss fuzz bassy",
                 "Stoner / Desert Fuzz",
                 "MI Sustain Octave Fuzz + Phaser lento sobre CH2 en Vintage 4x12.",
                 false, false, -4, 1, 0, false, 64.f, 68.f, 64.f, 50.f, 48.f, 62.f,
                 { 2, 4 },
                 {
                     { MiAmpParams::gtrFuzzOn, 1.f }, { MiAmpParams::gtrFuzzFuzz, 76.f }, { MiAmpParams::gtrFuzzTone, 46.f }, { MiAmpParams::gtrFuzzLvl, 60.f },
                     { MiAmpParams::gtrPhaserOn, 1.f }, { MiAmpParams::gtrPhaserRate, 26.f }, { MiAmpParams::gtrPhaserDepth, 60.f }
                 });

            add ("ambient post rock delay reverb clean worship atmospheric",
                 "Ambient / Post-Rock Pad",
                 "CH1 limpio + Compresor + Pearl Delay dotted 1/8 + Studio Reverb profundo.",
                 false, false, 0, 0, 2, true, 26.f, 50.f, 54.f, 60.f, 58.f, 64.f,
                 { 0, 3, 5, 6 },
                 {
                     { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 62.f },
                     { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusMix, 34.f },
                     { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 46.f }, { MiAmpParams::gtrDelayFb, 58.f }, { MiAmpParams::gtrDelayMix, 48.f }, { MiAmpParams::gtrDelaySubdiv, 1.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 82.f }, { MiAmpParams::gtrReverbMix, 54.f }
                 });

            add ("soul motown vintage warm round bass bajo",
                 "Vintage Warm Soul Bass",
                 u8"Bass Food c\u00e1lido sin brillo + Cabezal 8x10 con graves llenos.",
                 true, false, 0, 0, 0, false, 52.f, 66.f, 58.f, 34.f, 42.f, 50.f,
                 { 0, 13 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 60.f }, { MiAmpParams::compAttack, 52.f },
                     { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 62.f }, { MiAmpParams::bassEqBand[5], 30.f }
                 });

            // === ADDITIONAL BASS RIGS (Full parity for every genre/band when in Bass mode) ===
            add ("turnstile glow on blackout mystery hardcore punk franz lyons bass bajo",
                 "Turnstile \u2022 Bajo Hardcore Groove",
                 u8"Drop -2 + MI-87 Comp + MI Bass Driver con grit y pegada en medios-bajos sobre 8x10.",
                 true, false, -2, 0, 0, false, 60.f, 62.f, 62.f, 62.f, 54.f, 55.f,
                 { 11, 0, 5 },
                 {
                     { MiAmpParams::bassGateOn, 1.f }, { MiAmpParams::bassGateThresh, 32.f },
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 60.f }, { MiAmpParams::compAttack, 30.f },
                     { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 52.f }, { MiAmpParams::bddiBlend, 68.f }, { MiAmpParams::bddiPresence, 70.f }
                 });

            add ("neck deep december in bloom gold steps paramore sum 41 fat lip skate pop punk bass bajo",
                 "Modern Pop-Punk \u2022 Bajo Pick & Bite",
                 u8"MI-87 Comp + MI Bass Driver + Bass EQ realzando ataque de p\u00faa en 4.5 kHz.",
                 true, false, -2, 0, 0, false, 58.f, 58.f, 58.f, 66.f, 58.f, 55.f,
                 { 0, 5, 13 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 58.f },
                     { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 48.f }, { MiAmpParams::bddiBlend, 64.f }, { MiAmpParams::bddiPresence, 76.f },
                     { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 56.f }, { MiAmpParams::bassEqBand[5], 66.f }
                 });

            add ("deftones my own summer around the fur white pony chi cheng sergio vega nu metal drop bass bajo",
                 "Deftones \u2022 Bajo Drop Wall",
                 u8"Drop -3 + Silent Gate + MI-87 Comp + MI Gold Drive grueso + 8x10 con sub pesado.",
                 true, false, -3, 0, 0, false, 64.f, 68.f, 56.f, 60.f, 50.f, 55.f,
                 { 11, 0, 6, 2 },
                 {
                     { MiAmpParams::bassGateOn, 1.f }, { MiAmpParams::bassGateThresh, 38.f },
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 62.f },
                     { MiAmpParams::goldOn, 1.f }, { MiAmpParams::goldGain, 58.f }, { MiAmpParams::goldBlend, 62.f }, { MiAmpParams::goldEq, 56.f },
                     { MiAmpParams::chorusOn, 0.f }, { MiAmpParams::chorusMix, 24.f }
                 });

            add ("metallica master of puppets enter sandman cliff burton jason newsted thrash metal bass bajo",
                 "80s Thrash \u2022 Bajo Pick & Growl",
                 u8"MI-87 Comp + Bass Food + Bass EQ en sonrisa para cortar entre guitarras.",
                 true, false, 0, 0, 0, false, 62.f, 62.f, 44.f, 68.f, 56.f, 55.f,
                 { 0, 7, 13 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 60.f },
                     { MiAmpParams::bfOn, 1.f }, { MiAmpParams::bfDrive, 56.f }, { MiAmpParams::bfBlend, 66.f }, { MiAmpParams::bfPad, 1.f },
                     { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 62.f }, { MiAmpParams::bassEqBand[3], 38.f }, { MiAmpParams::bassEqBand[5], 68.f }
                 });

            add ("knocked loose metalcore hardcore beatdown modern metal djent drop bass bajo",
                 "Metalcore / Djent \u2022 Bajo Clank Brutal",
                 u8"Drop -4 + Silent Gate + MI-87 Comp + XV Preamp en SOLO PEDALES con clank agresivo.",
                 true, true, -4, 0, 0, false, 50.f, 50.f, 50.f, 50.f, 50.f, 50.f,
                 { 11, 0, 1, 13 },
                 {
                     { MiAmpParams::bassGateOn, 1.f }, { MiAmpParams::bassGateThresh, 48.f },
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 68.f },
                     { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvDrive, 76.f }, { MiAmpParams::xvBlend, 74.f }, { MiAmpParams::xvMid, 32.f }, { MiAmpParams::xvPresence, 86.f }, { MiAmpParams::xvCab, 1.f },
                     { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 60.f }, { MiAmpParams::bassEqBand[5], 68.f }
                 });

            add ("nirvana nevermind smells like teen spirit grunge foo fighters everlong alt rock bass bajo",
                 "90s Grunge / Alt Rock \u2022 Bajo C\u00e1lido",
                 u8"MI-87 Comp + Bass Food crunch suave + Cabezal 8x10 con graves y medios redondos.",
                 true, false, 0, 0, 0, false, 58.f, 62.f, 62.f, 56.f, 50.f, 55.f,
                 { 0, 7 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f },
                     { MiAmpParams::bfOn, 1.f }, { MiAmpParams::bfDrive, 42.f }, { MiAmpParams::bfBlend, 58.f }
                 });

            add ("title fight shoegaze post hardcore midwest emo math rock ambient post rock clean bass bajo",
                 "Indie / Emo / Shoegaze \u2022 Bajo Mel\u00f3dico",
                 u8"MI-87 Comp + MI Deep Chorus sutil + MI Studio Reverb + Cabezal 8x10 limpio y definido.",
                 true, false, 0, 0, 0, false, 48.f, 58.f, 60.f, 58.f, 52.f, 55.f,
                 { 0, 2, 4 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 58.f },
                     { MiAmpParams::chorusOn, 1.f }, { MiAmpParams::chorusRate, 32.f }, { MiAmpParams::chorusDepth, 48.f }, { MiAmpParams::chorusMix, 34.f },
                     { MiAmpParams::reverbOn, 1.f }, { MiAmpParams::reverbDecay, 46.f }, { MiAmpParams::reverbMix, 22.f }
                 });

            add ("stoner doom queens of the stone age kyuss fuzz bass bajo",
                 "Stoner / Doom \u2022 Bajo Fuzz Masivo",
                 u8"Drop -4 + Poly Octave Sub + MI Olive Fuzz + 8x10 con graves gigantes.",
                 true, false, -4, 0, 0, false, 64.f, 70.f, 58.f, 48.f, 44.f, 55.f,
                 { 0, 12, 8 },
                 {
                     { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 56.f },
                     { MiAmpParams::bassOctOn, 1.f }, { MiAmpParams::bassOctDry, 78.f }, { MiAmpParams::bassOctSub, 62.f },
                     { MiAmpParams::oliveOn, 1.f }, { MiAmpParams::oliveSustain, 78.f }, { MiAmpParams::oliveTone, 48.f }, { MiAmpParams::oliveMode, 1.f }
                 });

            add ("dual directo solo pedales preamp guitar guitarra",
                 "Dual Crunch \u2022 Solo Pedales (Directo)",
                 u8"Modo SOLO PEDALES con MI Dual Crunch + Pearl Delay + Studio Reverb sin cabezal.",
                 false, true, 0, 2, 1, true, 60.f, 54.f, 60.f, 60.f, 60.f, 60.f,
                 { 8, 5, 6 },
                 {
                     { MiAmpParams::gtrDualOn, 1.f }, { MiAmpParams::gtrDualDrive, 62.f }, { MiAmpParams::gtrDualBlend, 55.f }, { MiAmpParams::gtrDualTone, 58.f }, { MiAmpParams::gtrDualOutput, 60.f },
                     { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 40.f }, { MiAmpParams::gtrDelayFb, 30.f }, { MiAmpParams::gtrDelayMix, 22.f },
                     { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 42.f }, { MiAmpParams::gtrReverbMix, 22.f }
                 });

            return v;
        }();
        return cat;
    }

    static ToneMatchResult searchLocalCatalog (const juce::String& queryRaw, bool forBass, int& outBestScore)
    {
        const juce::String q = normalizeQuery (queryRaw);
        outBestScore = 0;
        if (q.isEmpty())
            return {};

        juce::StringArray tokens;
        tokens.addTokens (q, " ", "");
        tokens.removeEmptyStrings();

        const auto& cat = getCatalog();

        int bestIdx = -1;
        int bestScore = 0;

        for (size_t i = 0; i < cat.size(); ++i)
        {
            // Strictly respect the currently selected instrument (Guitar vs Bass)!
            if (cat[i].result.bassMode != forBass)
                continue;

            const juce::String hay = normalizeQuery (cat[i].keywords + " " + cat[i].result.title);
            int score = 0;

            int tokenHits = 0;
            // Bonus for multi-word exact phrase match
            if (q.length() >= 4 && hay.contains (q))
            {
                score += 120;
                ++tokenHits;
            }

            for (const auto& tok : tokens)
            {
                if (tok.length() < 2) continue;
                // Ignore generic filler words
                if (tok == "quiero" || tok == "sonar" || tok == "como" || tok == "esta"
                    || tok == "cancion" || tok == "tema" || tok == "para" || tok == "banda"
                    || tok == "sound" || tok == "like" || tok == "tone" || tok == "song" || tok == "the"
                    || tok == "parecerme" || tok == "estilo" || tok == "tono")
                    continue;
                if (hay.contains (tok))
                {
                    score += (tok.length() >= 5 ? 30 : 18);
                    ++tokenHits;
                }
            }

            if (tokenHits == 0)
                continue;

            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = static_cast<int> (i);
            }
        }

        outBestScore = bestScore;
        if (bestIdx >= 0 && bestScore >= 28)
            return cat[static_cast<size_t> (bestIdx)].result;

        return {};
    }

    static ToneMatchResult mapGenreKeywordsToTone (const juce::String& text, const juce::String& query, bool forBass, const juce::String& badge)
    {
        const juce::String norm = normalizeQuery (text);
        int score = 0;
        auto local = searchLocalCatalog (norm, forBass, score);
        if (local.found && score >= 28)
        {
            local.sourceBadge = badge;
            local.title = query.trim().substring (0, 26) + " (" + local.title.upToFirstOccurrenceOf (juce::String::fromUTF8 (u8" \u2022"), false, false) + ")";
            return local;
        }
        return {};
    }

    static juce::String buildAiPrompt (const juce::String& userQuery, bool forBass)
    {
        if (forBass)
        {
            return juce::String::fromUTF8 (
                u8"Tengo el plugin MI AMP en modo BAJO y quiero lograr el tono de bajo de: \"")
                + userQuery.trim()
                + juce::String::fromUTF8 (
                u8"\".\n\n"
                u8"Us\u00e1 EXCLUSIVAMENTE este equipamiento de BAJO en MI AMP y decime paso a paso qu\u00e9 elegir con valores exactos (0 a 100, o -12 a +12 dB en EQ):\n\n"
                u8"1) RUTA Y AFINACI\u00d3N:\n"
                u8"- Modo de ruta: CON AMPLI (cabezal 8x10 + cabina) o SOLO PEDALES (bypass completo del cabezal, ideal para XV directo)\n"
                u8"- DROP TUNER: On/Off, Semitonos (-12 a +12, ej. -2 para D)\n\n"
                u8"2) CABEZAL DE BAJO (si usa CON AMPLI):\n"
                u8"- Cabezal 8x10 | Switch ULTRA-HI (On/Off) | Cabina 8x10 (ON/OFF)\n"
                u8"- Perillas (0-100): Gain, Bass, Mid, Freq, Treble\n\n"
                u8"3) PEDALES DE BAJO (hasta 10 en orden):\n"
                u8"- MI-87 COMP (Compresor): Sustain, Attack, Level, Input\n"
                u8"- XV PREAMP (Bi-Amp Preamp): Level, Drive, Low, High, Blend, Mid, Presence, Cab (BASS/PUNK/CLEAN)\n"
                u8"- MI DEEP CHORUS: Rate, Depth, Mix, Tone\n"
                u8"- MI LIME PHASE: Speed, Depth, Feedback\n"
                u8"- MI STUDIO (Reverb): Decay, Tone, Mix, Mode\n"
                u8"- MI BASS DRIVER (DI Preamp): Drive, Blend, Treble, Presence, Bass, Level, 3 switches (Grit/Mid/Cab)\n"
                u8"- MI GOLD DRIVE: Gain, Tone, Blend, Level\n"
                u8"- BASS FOOD (Overdrive): Drive, Blend, Treble, Bass, Level, Switch GRIT\n"
                u8"- MI OLIVE (Fuzz): Level, Tone, Sustain, Switch 3-pos (Normal/Boost/Cut)\n"
                u8"- MI VIOLET (Flanger): Manual, Res, Depth, Rate, Switch MODE\n"
                u8"- MI PEARL (Delay): Time, Feedback, Mix, Mode, Switch SUBDIV\n"
                u8"- MI SILENT (Noise Gate): Thresh, Decay, Mode\n"
                u8"- MI POLY OCTAVE: Dry, Sub, Oct Up\n"
                u8"- MI BASS EQ (7 Bandas -12 a +12 dB): 50Hz, 120Hz, 400Hz, 500Hz, 800Hz, 4.5kHz, 10kHz\n\n"
                u8"Respondeme con:\n"
                u8"1) Modo (CON AMPLI o SOLO PEDALES) y Drop Tuner\n"
                u8"2) Configuraci\u00f3n del Cabezal 8x10 y Cabina\n"
                u8"3) Orden exacto de pedales de BAJO y valor de cada perilla/switch.");
        }

        return juce::String::fromUTF8 (
            u8"Tengo el plugin MI AMP en modo GUITARRA y quiero lograr el tono de guitarra de: \"")
            + userQuery.trim()
            + juce::String::fromUTF8 (
            u8"\".\n\n"
            u8"Us\u00e1 EXCLUSIVAMENTE este equipamiento de GUITARRA en MI AMP y decime paso a paso qu\u00e9 elegir con valores exactos (0 a 100, o -12 a +12 dB en EQ):\n\n"
            u8"1) RUTA Y AFINACI\u00d3N:\n"
            u8"- Modo de ruta: CON AMPLI (cabezal + cabina activos) o SOLO PEDALES (bypass completo del cabezal)\n"
            u8"- DROP TUNER: On/Off, Semitonos (-12 a +12, ej. -2 para D)\n\n"
            u8"2) CABEZAL Y CABINA DE GUITARRA (si usa CON AMPLI):\n"
            u8"- Canales: CH1 CLEAN / CH2 CRUNCH / CH3 PUNK | Switch BRIGHT (On/Off)\n"
            u8"- Perillas (0-100): Gain, Bass, Mid, Treble, Presence, Master, Output | Noise Gate (0-100) | Input Trim (0-100)\n"
            u8"- Cabinas: VINT 4x12, MOD 4x12, OPEN 2x12, STUD 1x12, CAB OFF\n\n"
            u8"3) PEDALES DE GUITARRA (hasta 10 en orden):\n"
            u8"- MI ICE COMP (Compresor): Sustain, Attack, Level, Tone\n"
            u8"- MI GREEN DRIVE (Overdrive): Gain, Tone, Level, Switch CLIP\n"
            u8"- MI SUSTAIN (Octave Fuzz): Volume, Tone, Sustain\n"
            u8"- MI SKY CHORUS (Chorus): Rate, Depth, Mix, EQ, Switch MODE\n"
            u8"- MI PHASER (4-Stage Phaser): Speed\n"
            u8"- MI PEARL (Delay): Time, Feedback, Mix, Mode, Switch SUBDIV\n"
            u8"- MI STUDIO (Reverb): Decay, Tone, Mix, Mode\n"
            u8"- MI ORANGE (Distortion): Tone, Level, Dist\n"
            u8"- MI DUAL CRUNCH (High Gain Preamp): Output, Gain, Blend, Tone, Drive, Switch SCOOP\n"
            u8"- MI VIOLET (Flanger): Manual, Res, Depth, Rate, Switch MODE\n"
            u8"- MI SILENT (Noise Gate): Thresh, Decay, Mode\n"
            u8"- MI SUB OCTAVE (Octavador): Direct, Oct Up, Sub 1, Sub 2\n"
            u8"- MI GRAPHIC EQ (7 Bandas -12 a +12 dB): 100Hz, 200Hz, 400Hz, 800Hz, 1.6kHz, 3.2kHz, 6.4kHz\n\n"
            u8"Respondeme con:\n"
            u8"1) Modo (CON AMPLI o SOLO PEDALES) y Drop Tuner\n"
            u8"2) Configuraci\u00f3n del Cabezal y Cabina\n"
            u8"3) Orden exacto de pedales de GUITARRA y valor de cada perilla/switch.");
    }

    // Free public web lookup (Wikipedia + MusicBrainz, ZERO API keys required)
    static ToneMatchResult searchFreeWeb (const juce::String& query, bool forBass)
    {
        juce::String collectedText;

        // 1. Wikipedia Opensearch / Query API (fast, free, no key)
        try
        {
            const juce::String enc = juce::URL::addEscapeChars (query + " band music", false);
            juce::URL wikiUrl ("https://en.wikipedia.org/w/api.php?action=query&list=search&srlimit=3&utf8=1&format=json&srsearch=" + enc);
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (5000)
                            .withExtraHeaders ("User-Agent: MiAmpToneFinder/1.0\r\n");
            if (auto stream = wikiUrl.createInputStream (opts))
            {
                const juce::String body = stream->readEntireStreamAsString();
                auto json = juce::JSON::parse (body);
                if (auto* qObj = json.getProperty ("query", {}).getDynamicObject())
                {
                    if (auto* arr = qObj->getProperty ("search").getArray())
                    {
                        for (const auto& item : *arr)
                        {
                            collectedText += " " + item.getProperty ("title", "").toString();
                            collectedText += " " + item.getProperty ("snippet", "").toString();
                        }
                    }
                }
            }
        }
        catch (...) {}

        // 2. MusicBrainz Artist/Recording tags (free, no key)
        try
        {
            const juce::String encMb = juce::URL::addEscapeChars (query, false);
            juce::URL mbUrl ("https://musicbrainz.org/ws/2/artist/?query=" + encMb + "&limit=2&fmt=json");
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (5000)
                            .withExtraHeaders ("User-Agent: MiAmpToneFinder/1.0 (contact@popupsaudio.com)\r\n");
            if (auto stream = mbUrl.createInputStream (opts))
            {
                const juce::String body = stream->readEntireStreamAsString();
                auto json = juce::JSON::parse (body);
                if (auto* arr = json.getProperty ("artists", {}).getArray())
                {
                    for (const auto& a : *arr)
                    {
                        collectedText += " " + a.getProperty ("name", "").toString();
                        collectedText += " " + a.getProperty ("disambiguation", "").toString();
                        if (auto* tags = a.getProperty ("tags", {}).getArray())
                            for (const auto& t : *tags)
                                collectedText += " " + t.getProperty ("name", "").toString();
                    }
                }
            }
        }
        catch (...) {}

        if (collectedText.trim().isEmpty())
            return {};

        return mapGenreKeywordsToTone (collectedText, query, forBass, "Web (Wiki + MusicBrainz)");
    }

    // Optional Google Gemini + Google Search Grounding (if user entered their own free Gemini key in CONFIG)
    static ToneMatchResult searchWithGemini (const juce::String& query, bool forBass, const juce::String& apiKey)
    {
        if (apiKey.trim().isEmpty())
            return {};

        try
        {
            const juce::String instStr = forBass ? "BASS (use ONLY bass pedals 0..13 and bass head)" : "GUITAR (use ONLY guitar pedals 0..12 and guitar head)";
            const juce::String prompt =
                "You are a guitar/bass rig expert. The user is currently in " + instStr
                + ". Search the web for the real rig, tuning and pedal chain for: \""
                + query.replace ("\"", "'")
                + "\". Map it to MI AMP parameters for " + instStr + " ONLY and reply with valid JSON (no markdown fences) with keys: "
                  "\"title\" (string), \"explanation\" (short Spanish description), \"bassMode\" (must be " + juce::String (forBass ? "true" : "false") + "), \"headBypass\" (bool, true for direct preamp/XV without amp head), "
                  "\"dropSemi\" (int -12..12), \"channel\" (0 clean, 1 crunch, 2 high-gain), \"cab\" (0..3), "
                  "\"gain\" (0..100), \"bass\" (0..100), \"mid\" (0..100), \"treble\" (0..100), \"presence\" (0..100), "
                  "\"slots\" (array of pedal indices: "
                  + juce::String (forBass
                      ? "for bass 0=MI87Comp,1=XV,2=Chorus,3=Phaser,4=Reverb,5=BassDriver,6=GoldDrive,7=BassFood,8=OliveFuzz,9=Flanger,10=Delay,11=Gate,12=Octaver,13=BassEQ)."
                      : "for guitar 0=Comp,1=GreenDrive,2=Fuzz,3=Chorus,4=Phaser,5=Delay,6=Reverb,7=OrangeDist,8=DualCrunch,9=Flanger,10=Gate,11=Octaver,12=GraphicEQ).");

            auto* root = new juce::DynamicObject();
            juce::Array<juce::var> contents;
            auto* cObj = new juce::DynamicObject();
            juce::Array<juce::var> parts;
            auto* pObj = new juce::DynamicObject();
            pObj->setProperty ("text", prompt);
            parts.add (juce::var (pObj));
            cObj->setProperty ("parts", parts);
            contents.add (juce::var (cObj));
            root->setProperty ("contents", contents);

            juce::Array<juce::var> tools;
            auto* tObj = new juce::DynamicObject();
            tObj->setProperty ("google_search", juce::var (new juce::DynamicObject()));
            tools.add (juce::var (tObj));
            root->setProperty ("tools", tools);

            const juce::String payload = juce::JSON::toString (juce::var (root), true);
            juce::URL url ("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent?key="
                           + juce::URL::addEscapeChars (apiKey.trim(), false));
            url = url.withPOSTData (payload);

            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (12000)
                            .withHttpRequestCmd ("POST")
                            .withExtraHeaders ("Content-Type: application/json\r\n");

            if (auto stream = url.createInputStream (opts))
            {
                const juce::String body = stream->readEntireStreamAsString();
                auto json = juce::JSON::parse (body);
                if (auto* cands = json.getProperty ("candidates", {}).getArray())
                {
                    if (! cands->isEmpty())
                    {
                        auto content = (*cands)[0].getProperty ("content", {});
                        if (auto* pArr = content.getProperty ("parts", {}).getArray())
                        {
                            if (! pArr->isEmpty())
                            {
                                juce::String txt = (*pArr)[0].getProperty ("text", "").toString().trim();
                                const int s0 = txt.indexOfChar ('{');
                                const int s1 = txt.lastIndexOfChar ('}');
                                if (s0 >= 0 && s1 > s0)
                                    txt = txt.substring (s0, s1 + 1);
                                auto parsed = juce::JSON::parse (txt);
                                if (parsed.isObject())
                                {
                                    ToneMatchResult r;
                                    r.found = true;
                                    r.sourceBadge = "Gemini IA + Google Search";
                                    r.title = parsed.getProperty ("title", query).toString();
                                    r.explanation = parsed.getProperty ("explanation", "Cadena armada con busqueda web en vivo.").toString();
                                    r.bassMode = forBass;
                                    r.headBypass = static_cast<bool> (parsed.getProperty ("headBypass", false));
                                    r.dropSemi = std::clamp (static_cast<int> (parsed.getProperty ("dropSemi", 0)), -12, 12);
                                    r.gtrChannel = std::clamp (static_cast<int> (parsed.getProperty ("channel", 2)), 0, 2);
                                    r.gtrCab = std::clamp (static_cast<int> (parsed.getProperty ("cab", 1)), 0, 4);
                                    r.gtrGain = std::clamp (static_cast<float> (parsed.getProperty ("gain", 66.0)), 0.f, 100.f);
                                    r.gtrBass = std::clamp (static_cast<float> (parsed.getProperty ("bass", 54.0)), 0.f, 100.f);
                                    r.gtrMid = std::clamp (static_cast<float> (parsed.getProperty ("mid", 58.0)), 0.f, 100.f);
                                    r.gtrTreble = std::clamp (static_cast<float> (parsed.getProperty ("treble", 62.0)), 0.f, 100.f);
                                    r.gtrPresence = std::clamp (static_cast<float> (parsed.getProperty ("presence", 60.0)), 0.f, 100.f);
                                    r.bassGain = r.gtrGain; r.bassLow = r.gtrBass; r.bassMid = r.gtrMid; r.bassTreble = r.gtrTreble;
                                    if (auto* slArr = parsed.getProperty ("slots", {}).getArray())
                                    {
                                        const int maxT = r.bassMode ? 14 : 13;
                                        for (const auto& sv : *slArr)
                                        {
                                            const int idx = static_cast<int> (sv);
                                            if (idx >= 0 && idx < maxT && r.chainSlots.size() < 10)
                                                r.chainSlots.push_back (idx);
                                        }
                                    }
                                    if (r.chainSlots.empty())
                                        r.chainSlots.push_back (r.bassMode ? 1 : 1);
                                    return r;
                                }
                            }
                        }
                    }
                }
            }
        }
        catch (...) {}
        return {};
    }

    static void applyToProcessor (MiAmpAudioProcessor& proc, const ToneMatchResult& r)
    {
        if (! r.found)
            return;

        proc.setInstrumentMode (r.bassMode);
        proc.setHeadBypassed (r.bassMode, r.headBypass);

        // Drop tuner
        proc.setParamNormalized (MiAmpParams::pitchOn, r.dropSemi != 0 ? 1.0f : 0.0f);
        proc.setParamNormalized (MiAmpParams::pitchSemi, static_cast<float> (r.dropSemi));
        proc.setParamNormalized (MiAmpParams::pitchCents, 0.0f);
        proc.setParamNormalized (MiAmpParams::pitchMix, 100.0f);

        if (r.bassMode)
        {
            proc.setParamNormalized (MiAmpParams::ampPower, 1.0f);
            proc.setParamNormalized (MiAmpParams::ampStandby, 0.0f);
            proc.setParamNormalized (MiAmpParams::ampCabOn, r.bassCabOn ? 1.0f : 0.0f);
            proc.setParamNormalized (MiAmpParams::ampUltraHi, r.bassUltraHi ? 1.0f : 0.0f);
            proc.setParamNormalized (MiAmpParams::ampGain, r.bassGain);
            proc.setParamNormalized (MiAmpParams::ampBass, r.bassLow);
            proc.setParamNormalized (MiAmpParams::ampMid, r.bassMid);
            proc.setParamNormalized (MiAmpParams::ampFreq, r.bassFreq);
            proc.setParamNormalized (MiAmpParams::ampTreble, r.bassTreble);
        }
        else
        {
            const int ch = std::clamp (r.gtrChannel, 0, 2);
            proc.setParamNormalized (MiAmpParams::gtrPower, 1.0f);
            proc.setParamNormalized (MiAmpParams::gtrStandby, 0.0f);
            proc.setParamNormalized (MiAmpParams::gtrChannel, static_cast<float> (ch));
            proc.setParamNormalized (MiAmpParams::gtrCab, static_cast<float> (std::clamp (r.gtrCab, 0, 4)));
            proc.setParamNormalized (MiAmpParams::gtrModeBright, r.gtrBright ? 1.0f : 0.0f);
            proc.setParamNormalized (MiAmpParams::gtrChGain[ch], r.gtrGain);
            proc.setParamNormalized (MiAmpParams::gtrChBass[ch], r.gtrBass);
            proc.setParamNormalized (MiAmpParams::gtrChMid[ch], r.gtrMid);
            proc.setParamNormalized (MiAmpParams::gtrChTreble[ch], r.gtrTreble);
            proc.setParamNormalized (MiAmpParams::gtrChPresence[ch], r.gtrPresence);
            proc.setParamNormalized (MiAmpParams::gtrChMaster[ch], r.gtrMaster);
        }

        proc.setActiveChain (r.bassMode, r.chainSlots);

        for (const auto& ov : r.overrides)
            if (ov.paramId != nullptr)
                proc.setParamNormalized (ov.paramId, ov.value);

        proc.setActivePresetName (r.bassMode, r.title);
    }
};

} // namespace mipedals
