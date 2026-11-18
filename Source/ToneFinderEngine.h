#pragma once
#include "PluginProcessor.h"
#include "Localization.h"
#include <vector>
#include <array>
#include <utility>
#include <algorithm>
#include <cmath>
#include <cstring>

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
    juce::String sourceBadge;   // "Rig Verificado", "Aproximacion por Genero (Web)", "Gemini IA + Google Search"
    juce::String rigSummary;    // e.g. "MI AMP TWIN · 2x12 BLACKFACE" or "SOLO PEDALES"
    juce::String explanation;
    bool bassMode = false;
    bool headBypass = false;
    int  dropSemi = 0;
    int  suggestedDropSemi = 0;

    // Amp head & cabinet models
    // Guitar head: 0=800, 1=Plexi, 2=Recto, 3=HG-120(6505), 4=AC-30, 5=Rocker, 6=BE-100, 7=Twin, 8=Classic Mint
    // Guitar cab:  0=Greenback, 1=BritV30, 2=Oversized, 3=Hi-Gain, 4=Class-A 2x12, 5=Tangerine, 6=Blackface 2x12, 7=MI Classic
    int  gtrHeadModel = 8;
    int  gtrChannel = 2;
    int  gtrCab = 1;
    bool gtrBright = true;
    float gtrGain = 62.0f, gtrBass = 54.0f, gtrMid = 60.0f, gtrTreble = 62.0f, gtrPresence = 58.0f, gtrMaster = 60.0f;
    float gtrGateThresh = 0.0f; // Head built-in gate (0..100)

    // Bass head: 0=CL-300, 1=PRO-450, 2=Bassman, 3=RB-800, 4=M-900, 5=B-15, 6=OR-200, 7=DB-750, 8=Classic Mint
    // Bass cab:  0=1x15 Vintage, 1=4x10 Touring, 2=4x10 Hi-Punch, 3=6x10 Heavy, 4=8x10 Classic, 5=8x10 Tangerine, 6=8x10 MI Classic
    int   bassHeadModel = 8;
    int   bassCabModel = 4;
    float bassGain = 54.0f, bassLow = 54.0f, bassMid = 54.0f, bassFreq = 50.0f, bassTreble = 54.0f;
    bool  bassUltraHi = false;
    bool  bassCabOn = true;

    // Dual Mic + Room calibration
    float micMix = 36.0f;   // 0 = MI-57 focus, 50 = balanced 57+87, 75 = warm MI-87
    float cabRoom = 14.0f;  // 0..100

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
            { u8"\u00e0", "a" }, { u8"\u00e8", "e" }, { u8"\u00ec", "i" }, { u8"\u00f2", "o" }, { u8"\u00f9", "u" },
            { u8"\u00f1", "n" }, { u8"\u00fc", "u" }, { u8"\u00e7", "c" },
            { u8"\u2019", "" },  { "'", "" }, { "&", " " }, { "+", " " },
            { "-", " " }, { "/", " " }, { ",", " " }, { ".", " " }, { "(", " " }, { ")", " " }, { "\"", " " }
        };
        for (const auto& r : kAccents)
            s = s.replace (juce::String::fromUTF8 (r.a), r.b);

        // Collapse multiple spaces into single space
        juce::StringArray toks;
        toks.addTokens (s, " \t\r\n", "");
        toks.removeEmptyStrings();
        return toks.joinIntoString (" ").trim();
    }

    static bool queryImpliesBass (const juce::String& qNorm)
    {
        static constexpr const char* kBassHints[] = {
            "bajo", "bass", "bajista", "bassist", "slap", "flea", "kelen capener", "mark hoppus",
            "justin chancellor", "hysteria", "chris wolstenholme", "mike dirnt", "matt freeman",
            "cliff burton", "jason newsted", "robert trujillo", "geddy lee", "steve harris",
            "jaco", "pastorius", "marcus miller", "pbass", "jazz bass", "zeta bosio",
            "diego arnedo", "duff mckagan", "james jamerson", "paul mccartney", "roger waters",
            "john paul jones", "royal blood", "xv original", "xv directo"
        };
        const juce::String padded = " " + qNorm + " ";
        for (const char* w : kBassHints)
            if (padded.contains (" " + juce::String (w) + " "))
                return true;
        return false;
    }

    static bool queryRequestsDropTuning (const juce::String& qNorm, int& outExplicitDrop)
    {
        outExplicitDrop = 0;
        const juce::String p = " " + qNorm + " ";
        if (p.contains (" drop a ") || p.contains (" drop b "))   { outExplicitDrop = -5; return true; }
        if (p.contains (" drop c "))                              { outExplicitDrop = -4; return true; }
        if (p.contains (" drop c# ") || p.contains (" drop db ")) { outExplicitDrop = -3; return true; }
        if (p.contains (" drop d "))                              { outExplicitDrop = -2; return true; }
        if (p.contains (" half step ") || p.contains (" medio tono ") || p.contains (" eb ")) { outExplicitDrop = -1; return true; }
        if (p.contains (" drop ") || p.contains (" afinacion ") || p.contains (" tuning ") || p.contains (" downtune "))
            return true;
        return false;
    }

    static juce::String gtrHeadName (int idx)
    {
        static constexpr const char* kNames[9] = {
            "MI AMP 800", "MI AMP PLEXI", "MI AMP RECTO", "MI AMP HG-120",
            "MI AMP AC-30", "MI AMP ROCKER", "MI AMP BE-100", "MI AMP TWIN", "MI AMP CLASSIC"
        };
        return kNames[std::clamp (idx, 0, 8)];
    }

    static juce::String gtrCabName (int idx)
    {
        static constexpr const char* kNames[10] = {
            "4x12 GREENBACK", "4x12 BRIT V30", "4x12 OVERSIZED", "4x12 HI-GAIN",
            "2x12 CLASS-A", "4x12 TANGERINE", "2x12 BLACKFACE", "4x12 MI CLASSIC",
            "CUSTOM IR", "CAB OFF"
        };
        return kNames[std::clamp (idx, 0, 9)];
    }

    static juce::String bassHeadName (int idx)
    {
        static constexpr const char* kNames[9] = {
            "MI AMP CL-300", "MI AMP PRO-450", "MI AMP BASSMAN", "MI AMP RB-800",
            "MI AMP M-900", "MI AMP B-15", "MI AMP OR-200", "MI AMP DB-750", "MI AMP CLASSIC"
        };
        return kNames[std::clamp (idx, 0, 8)];
    }

    static juce::String bassCabName (int idx)
    {
        static constexpr const char* kNames[9] = {
            "1x15 VINTAGE", "4x10 TOURING", "4x10 HI-PUNCH", "6x10 HEAVY",
            "8x10 CLASSIC", "8x10 TANGERINE", "8x10 MI CLASSIC", "CUSTOM IR", "CAB OFF"
        };
        return kNames[std::clamp (idx, 0, 8)];
    }

    static ToneMatchResult buildRig (const juce::String& title,
                                     const juce::String& badge,
                                     const juce::String& explanation,
                                     bool bassMode,
                                     bool headBypass,
                                     int suggestedDropSemi,
                                     int headModel,
                                     int ch,
                                     int cab,
                                     bool bright,
                                     float g, float b, float m, float t, float p, float mast,
                                     float micMix, float cabRoom,
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
        r.suggestedDropSemi = suggestedDropSemi;
        r.dropSemi = 0; // Never silently detune the player's guitar unless requested
        r.micMix = micMix;
        r.cabRoom = cabRoom;

        if (bassMode)
        {
            r.bassHeadModel = std::clamp (headModel, 0, 8);
            r.bassCabModel  = std::clamp (cab, 0, 8);
            r.bassGain = g; r.bassLow = b; r.bassMid = m; r.bassFreq = p; r.bassTreble = t;
            r.bassUltraHi = bright;
            r.bassCabOn = (! headBypass && r.bassCabModel < 8);
            r.rigSummary = headBypass ? "SOLO PEDALES (DI)"
                                      : (bassHeadName (r.bassHeadModel) + " \u00b7 " + bassCabName (r.bassCabModel));
        }
        else
        {
            r.gtrHeadModel = std::clamp (headModel, 0, 8);
            r.gtrChannel = std::clamp (ch, 0, 2);
            r.gtrCab = std::clamp (cab, 0, 9);
            r.gtrBright = bright;
            r.gtrGain = g; r.gtrBass = b; r.gtrMid = m; r.gtrTreble = t; r.gtrPresence = p; r.gtrMaster = mast;
            r.rigSummary = headBypass ? "SOLO PEDALES"
                                      : (gtrHeadName (r.gtrHeadModel) + " \u00b7 " + gtrCabName (r.gtrCab));
        }

        r.chainSlots = std::move (slots);
        r.overrides = std::move (overrides);
        return r;
    }

    struct CatalogEntry
    {
        juce::String exactAliases; // Pipe-separated exact artist/song phrases ("pink floyd|david gilmour|comfortably numb")
        juce::String styleTags;    // Space-separated genre/era/vibe tokens for fallback style matching
        ToneMatchResult result;
    };

    static const std::vector<CatalogEntry>& getCatalog()
    {
        static const std::vector<CatalogEntry> cat = []()
        {
            std::vector<CatalogEntry> v;
            auto addG = [&](const char* aliases, const char* tags, const char* title, const char* exp,
                            bool bypass, int sugDrop, int head, int ch, int cab, bool br,
                            float g, float b, float m, float t, float p, float mast,
                            float micMix, float room,
                            std::vector<int> slots, std::vector<ParamOverride> ov)
            {
                v.push_back ({
                    juce::String::fromUTF8 (aliases),
                    juce::String::fromUTF8 (tags),
                    buildRig (juce::String::fromUTF8 (title), "Rig Verificado", juce::String::fromUTF8 (exp),
                              false, bypass, sugDrop, head, ch, cab, br, g, b, m, t, p, mast, micMix, room,
                              std::move (slots), std::move (ov))
                });
            };

            auto addB = [&](const char* aliases, const char* tags, const char* title, const char* exp,
                            bool bypass, int sugDrop, int head, int cab, bool ultraHi,
                            float g, float b, float m, float freq, float t,
                            float micMix, float room,
                            std::vector<int> slots, std::vector<ParamOverride> ov)
            {
                v.push_back ({
                    juce::String::fromUTF8 (aliases),
                    juce::String::fromUTF8 (tags),
                    buildRig (juce::String::fromUTF8 (title), "Rig Verificado", juce::String::fromUTF8 (exp),
                              true, bypass, sugDrop, head, 0, cab, ultraHi, g, b, m, t, freq, 58.f, micMix, room,
                              std::move (slots), std::move (ov))
                });
            };

            // =========================================================================
            // GUITAR RIGS — ROCK LATINO / ARGENTINO / EN ESPAÑOL
            // =========================================================================
            addG ("soda stereo|gustavo cerati|cerati|persiana americana|profugos|cuando pase el temblor|signos|nada personal|te para tres|ciudad de la furia",
                  "rock argentino 80s 90s stereo chorus delay clean pop rock new wave",
                  "Cerati / Soda Stereo \u2022 Clean Chorus & Delay",
                  u8"MI AMP TWIN + 2x12 Blackface con Compresor, Sky Chorus est\u00e9reo y Pearl Delay r\u00edtmico (el sello ochentoso/noventoso de Cerati).",
                  false, 0, 7, 1, 6, true, 36.f, 48.f, 58.f, 66.f, 60.f, 64.f, 48.f, 22.f,
                  { 0, 3, 5, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 56.f }, { MiAmpParams::gtrCompAtt, 34.f }, { MiAmpParams::gtrCompLvl, 56.f }, { MiAmpParams::gtrCompTone, 60.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 36.f }, { MiAmpParams::gtrChorusDepth, 58.f }, { MiAmpParams::gtrChorusMix, 44.f }, { MiAmpParams::gtrChorusMode, 1.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 44.f }, { MiAmpParams::gtrDelayFb, 36.f }, { MiAmpParams::gtrDelayMix, 30.f }, { MiAmpParams::gtrDelaySubdiv, 1.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 54.f }, { MiAmpParams::gtrReverbTone, 58.f }, { MiAmpParams::gtrReverbMix, 26.f }
                  });

            addG ("de musica ligera|en la ciudad de la furia|bocanada|ahí vamos|ahi vamos|crimen|lago en el cielo|cerati crunch|cerati rock",
                  "rock argentino crunch drive overdrive stadium alt rock",
                  "Cerati \u2022 Stadium Drive & Mod",
                  u8"MI AMP AC-30 Top Boost + 2x12 Class-A empujado con Green Drive, Chorus sutil y Delay espacioso.",
                  false, 0, 4, 1, 4, true, 54.f, 52.f, 62.f, 62.f, 58.f, 62.f, 42.f, 20.f,
                  { 0, 1, 3, 5, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 44.f }, { MiAmpParams::gtrCompLvl, 54.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 42.f }, { MiAmpParams::gtrDriveTone, 56.f }, { MiAmpParams::gtrDriveLvl, 56.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 34.f }, { MiAmpParams::gtrChorusDepth, 42.f }, { MiAmpParams::gtrChorusMix, 24.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 46.f }, { MiAmpParams::gtrDelayFb, 34.f }, { MiAmpParams::gtrDelayMix, 24.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 50.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            addG ("divididos|ricardo mollo|mollo|ala delta|spaghetti del rock|que ves|paisano de hurlingham|el 38",
                  "rock nacional hard rock blues rock fender marshall plexi power trio",
                  "Divididos / Mollo \u2022 Plexi Power Trio",
                  u8"MI AMP PLEXI rugiendo en 4x12 Greenback con Green Drive empujando medios y arm\u00f3nicos.",
                  false, 0, 1, 1, 0, true, 68.f, 56.f, 68.f, 60.f, 58.f, 64.f, 38.f, 16.f,
                  { 1, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 34.f }, { MiAmpParams::gtrDriveTone, 56.f }, { MiAmpParams::gtrDriveLvl, 60.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 38.f }, { MiAmpParams::gtrReverbMix, 16.f }
                  });

            addG ("los redondos|patricio rey|skay|skay beilinson|jijiji|ji ji ji|la bestia pop|preso en mi ciudad",
                  "rock nacional 80s british crunch delay",
                  "Los Redondos / Skay \u2022 Brit Crunch & Delay",
                  u8"MI AMP 800 + 4x12 Brit V30 con ataque afilado, Green Drive y Pearl Delay r\u00edtmico.",
                  false, 0, 0, 2, 1, true, 60.f, 50.f, 66.f, 64.f, 62.f, 60.f, 36.f, 16.f,
                  { 1, 5, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 30.f }, { MiAmpParams::gtrDriveTone, 58.f }, { MiAmpParams::gtrDriveLvl, 56.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 38.f }, { MiAmpParams::gtrDelayFb, 28.f }, { MiAmpParams::gtrDelayMix, 22.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 40.f }, { MiAmpParams::gtrReverbMix, 18.f }
                  });

            addG ("rata blanca|walter giardino|mujer amante|la leyenda del hada|hada y el mago",
                  "heavy metal neoclasico shred lead hard rock",
                  "Rata Blanca / Giardino \u2022 Neoclassical Lead",
                  u8"MI AMP BE-100 + 4x12 Brit V30 con Green Drive boost para solos cantados, Delay y Reverb.",
                  false, 0, 6, 2, 1, true, 70.f, 52.f, 64.f, 66.f, 64.f, 60.f, 34.f, 18.f,
                  { 10, 1, 5, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 28.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 32.f }, { MiAmpParams::gtrDriveTone, 60.f }, { MiAmpParams::gtrDriveLvl, 62.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 46.f }, { MiAmpParams::gtrDelayFb, 34.f }, { MiAmpParams::gtrDelayMix, 24.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 52.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            addG ("heroes del silencio|juan valdivia|entre dos tierras|maldito duende|la chispa adecuada",
                  "rock en espanol 90s arpegio chorus delay",
                  u8"H\u00e9roes del Silencio \u2022 Arpegio & Crunch",
                  u8"MI AMP TWIN brillante con Compresor, Sky Chorus y Delay dotted 1/8 ideal para arpegios cristalinos.",
                  false, 0, 7, 1, 6, true, 42.f, 48.f, 56.f, 68.f, 62.f, 62.f, 46.f, 22.f,
                  { 0, 3, 5, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 54.f }, { MiAmpParams::gtrCompLvl, 55.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 36.f }, { MiAmpParams::gtrChorusDepth, 52.f }, { MiAmpParams::gtrChorusMix, 38.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 42.f }, { MiAmpParams::gtrDelayFb, 38.f }, { MiAmpParams::gtrDelayMix, 30.f }, { MiAmpParams::gtrDelaySubdiv, 1.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 54.f }, { MiAmpParams::gtrReverbMix, 26.f }
                  });

            addG ("charly garcia|spinetta|luis alberto spinetta|almendra|invisible|seru giran|sui generis|fito paez|calamaro",
                  "rock nacional vintage clean jazz rock warm",
                  "Rock Argentino Cl\u00e1sico \u2022 Vintage Clean",
                  u8"MI AMP TWIN + 2x12 Blackface c\u00e1lido y org\u00e1nico con Compresor suave, Chorus sutil y Studio Reverb.",
                  false, 0, 7, 0, 6, false, 32.f, 52.f, 58.f, 60.f, 54.f, 62.f, 52.f, 22.f,
                  { 0, 3, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 48.f }, { MiAmpParams::gtrCompAtt, 42.f }, { MiAmpParams::gtrCompLvl, 54.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 28.f }, { MiAmpParams::gtrChorusDepth, 36.f }, { MiAmpParams::gtrChorusMix, 24.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 48.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            // =========================================================================
            // GUITAR RIGS — CLASSIC ROCK / BLUES / VIRTUOSOS
            // =========================================================================
            addG ("pink floyd|david gilmour|gilmour|comfortably numb|wish you were here|another brick|time|shine on",
                  "prog rock classic rock solo sustain fuzz delay reverb",
                  "Pink Floyd / Gilmour \u2022 Solo & Atmosphere",
                  u8"MI AMP TWIN (headroom Hiwatt) + 2x12 Blackface con Compresor, Sustain Fuzz cremoso, Delay y Reverb espacial.",
                  false, 0, 7, 0, 6, true, 38.f, 52.f, 60.f, 62.f, 58.f, 64.f, 48.f, 24.f,
                  { 0, 2, 4, 5, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 58.f }, { MiAmpParams::gtrCompAtt, 38.f }, { MiAmpParams::gtrCompLvl, 56.f },
                      { MiAmpParams::gtrFuzzOn, 1.f }, { MiAmpParams::gtrFuzzFuzz, 58.f }, { MiAmpParams::gtrFuzzTone, 52.f }, { MiAmpParams::gtrFuzzLvl, 56.f },
                      { MiAmpParams::gtrPhaserOn, 0.f }, { MiAmpParams::gtrPhaserRate, 24.f }, { MiAmpParams::gtrPhaserDepth, 48.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 48.f }, { MiAmpParams::gtrDelayFb, 40.f }, { MiAmpParams::gtrDelayMix, 32.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 62.f }, { MiAmpParams::gtrReverbMix, 30.f }
                  });

            addG ("john mayer|slow dancing|gravity|neon|continuum|vultures|heartbreak warfare|edge of breakup",
                  "blues pop soul stratocaster clean overdrive",
                  "John Mayer \u2022 Glassy Edge-of-Breakup",
                  u8"MI AMP TWIN + 2x12 Blackface en el punto dulce justo antes de romper, empujado por Green Drive TS suave y Reverb.",
                  false, 0, 7, 1, 6, true, 38.f, 52.f, 62.f, 60.f, 56.f, 62.f, 48.f, 22.f,
                  { 0, 1, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 42.f }, { MiAmpParams::gtrCompAtt, 44.f }, { MiAmpParams::gtrCompLvl, 54.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 28.f }, { MiAmpParams::gtrDriveTone, 54.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 46.f }, { MiAmpParams::gtrReverbTone, 54.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            addG ("stevie ray vaughan|srv|pride and joy|texas flood|crossfire|cold shot",
                  "texas blues strat overdrive tubescreamer",
                  "Stevie Ray Vaughan \u2022 Texas Blues Drive",
                  u8"MI AMP TWIN fuerte + Green Drive (estilo TS808) con medios empujados y din\u00e1mica de p\u00faa.",
                  false, -1, 7, 1, 6, true, 48.f, 54.f, 66.f, 62.f, 58.f, 64.f, 44.f, 18.f,
                  { 1, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 44.f }, { MiAmpParams::gtrDriveTone, 56.f }, { MiAmpParams::gtrDriveLvl, 64.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 38.f }, { MiAmpParams::gtrReverbMix, 18.f }
                  });

            addG ("jimi hendrix|hendrix|voodoo child|purple haze|little wing|all along the watchtower|hey joe",
                  "psychedelic rock 60s fuzz phaser plexi classic rock",
                  "Jimi Hendrix \u2022 Plexi + Fuzz + Vibe",
                  u8"MI AMP PLEXI + 4x12 Greenback con MI Sustain Fuzz expresivo y Phaser lento estilo Uni-Vibe.",
                  false, -1, 1, 1, 0, true, 62.f, 56.f, 62.f, 60.f, 56.f, 62.f, 44.f, 20.f,
                  { 2, 4, 6 },
                  {
                      { MiAmpParams::gtrFuzzOn, 1.f }, { MiAmpParams::gtrFuzzFuzz, 64.f }, { MiAmpParams::gtrFuzzTone, 54.f }, { MiAmpParams::gtrFuzzLvl, 56.f },
                      { MiAmpParams::gtrPhaserOn, 1.f }, { MiAmpParams::gtrPhaserRate, 32.f }, { MiAmpParams::gtrPhaserDepth, 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 44.f }, { MiAmpParams::gtrReverbMix, 20.f }
                  });

            addG ("ac dc|acdc|angus young|malcolm young|back in black|highway to hell|thunderstruck|you shook me",
                  "hard rock 70s classic rock crunch marshall plexi riff",
                  "AC/DC \u2022 Pure Plexi Crunch (Sin Pedales)",
                  u8"MI AMP PLEXI directo a 4x12 Greenback sin pedales de saturaci\u00f3n: puro crunch brit\u00e1nico de v\u00e1lvulas.",
                  false, 0, 1, 1, 0, true, 66.f, 52.f, 66.f, 62.f, 60.f, 64.f, 36.f, 16.f,
                  { 6 },
                  {
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 28.f }, { MiAmpParams::gtrReverbMix, 12.f }
                  });

            addG ("led zeppelin|jimmy page|whole lotta love|stairway to heaven|black dog|kashmir|rock and roll",
                  "70s classic rock hard rock blues rock vintage",
                  "Led Zeppelin / Page \u2022 Vintage Plexi",
                  u8"MI AMP PLEXI + 4x12 Greenback con medios c\u00e1lidos y Reverb de sala vintage.",
                  false, 0, 1, 1, 0, true, 64.f, 54.f, 66.f, 60.f, 58.f, 62.f, 44.f, 22.f,
                  { 1, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 0.f }, { MiAmpParams::gtrDriveGain, 26.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 46.f }, { MiAmpParams::gtrReverbMix, 22.f }
                  });

            addG ("queen|brian may|bohemian rhapsody|killer queen|we will rock you|dont stop me now",
                  "classic rock 70s ac30 treble boost harmony lead",
                  "Queen / Brian May \u2022 AC-30 Top Boost",
                  u8"MI AMP AC-30 al rojo en 2x12 Class-A empujado por Green Drive brillante para ese canto dulce caracter\u00edstico.",
                  false, 0, 4, 1, 4, true, 72.f, 48.f, 68.f, 64.f, 58.f, 64.f, 40.f, 20.f,
                  { 1, 5, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 32.f }, { MiAmpParams::gtrDriveTone, 70.f }, { MiAmpParams::gtrDriveLvl, 62.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 42.f }, { MiAmpParams::gtrDelayMix, 20.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 42.f }, { MiAmpParams::gtrReverbMix, 18.f }
                  });

            addG ("van halen|eddie van halen|evh|panama|eruption|aint talkin bout love|runnin with the devil|jump",
                  "80s hard rock brown sound shred phaser",
                  "Van Halen \u2022 Brown Sound",
                  u8"MI AMP BE-100 + 4x12 Greenback con Phaser sutil pre-ampli y slapback Delay de estudio.",
                  false, -1, 6, 2, 0, true, 72.f, 54.f, 64.f, 64.f, 62.f, 62.f, 36.f, 18.f,
                  { 10, 4, 5, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 26.f },
                      { MiAmpParams::gtrPhaserOn, 1.f }, { MiAmpParams::gtrPhaserRate, 28.f }, { MiAmpParams::gtrPhaserDepth, 42.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 30.f }, { MiAmpParams::gtrDelayFb, 18.f }, { MiAmpParams::gtrDelayMix, 18.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 44.f }, { MiAmpParams::gtrReverbMix, 20.f }
                  });

            addG ("guns n roses|gnr|slash|sweet child o mine|welcome to the jungle|november rain|paradise city|nightrain",
                  "hard rock 80s 90s les paul jcm800 solo",
                  "Guns N' Roses / Slash \u2022 MI-800 Rock Lead",
                  u8"MI AMP 800 + 4x12 Brit V30 empujado con Green Drive suave para mordida de humbucker y solos fluidos.",
                  false, -1, 0, 2, 1, true, 68.f, 54.f, 66.f, 62.f, 60.f, 62.f, 36.f, 18.f,
                  { 10, 1, 5, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 24.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 28.f }, { MiAmpParams::gtrDriveTone, 56.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 42.f }, { MiAmpParams::gtrDelayMix, 20.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 42.f }, { MiAmpParams::gtrReverbMix, 18.f }
                  });

            addG ("dire straits|mark kn希pfler|mark knopfler|knopfler|sultans of swing|money for nothing|brothers in arms",
                  "classic rock clean strat fingerpicking compressor",
                  "Dire Straits / Knopfler \u2022 Clean Fingerpick",
                  u8"MI AMP TWIN + 2x12 Blackface con Compresor brillante para el chasquido limpio en posiciones intermedias.",
                  false, 0, 7, 0, 6, true, 30.f, 48.f, 58.f, 68.f, 62.f, 64.f, 46.f, 20.f,
                  { 0, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 58.f }, { MiAmpParams::gtrCompAtt, 32.f }, { MiAmpParams::gtrCompLvl, 58.f }, { MiAmpParams::gtrCompTone, 64.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 44.f }, { MiAmpParams::gtrReverbMix, 22.f }
                  });

            addG ("carlos santana|santana|black magic woman|europa|smooth|oya como va",
                  "latin rock blues lead sustain smooth",
                  "Santana \u2022 Creamy Singing Sustain",
                  u8"MI AMP BE-100 medios ricos + 4x12 Brit V30 y Green Drive c\u00e1lido para sustain infinito sin asperezas.",
                  false, 0, 6, 2, 1, false, 68.f, 54.f, 72.f, 54.f, 50.f, 60.f, 48.f, 20.f,
                  { 1, 5, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 38.f }, { MiAmpParams::gtrDriveTone, 46.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 44.f }, { MiAmpParams::gtrDelayMix, 20.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 50.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            addG ("bb king|b b king|the thrill is gone|albert king|gary moore|blues|blues rock|slow blues",
                  "blues clean warm crunch expressive",
                  "Classic Blues \u2022 Warm Tube Bite",
                  u8"MI AMP TWIN con breakup dulce + Green Drive suave y Studio Reverb de sala.",
                  false, 0, 7, 1, 6, false, 44.f, 52.f, 64.f, 58.f, 54.f, 62.f, 50.f, 22.f,
                  { 1, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 32.f }, { MiAmpParams::gtrDriveTone, 50.f }, { MiAmpParams::gtrDriveLvl, 56.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 44.f }, { MiAmpParams::gtrReverbMix, 22.f }
                  });

            // =========================================================================
            // GUITAR RIGS — 90s / 2000s ROCK, GRUNGE, INDIE, BRITPOP, POP-PUNK
            // =========================================================================
            addG ("red hot chili peppers|rhcp|john frusciante|frusciante|californication|cant stop|under the bridge|scar tissue|snow|dani california",
                  "funk rock 90s alt rock strat plexi clean breakup chorus",
                  "RHCP / Frusciante \u2022 Plexi Edge & Chorus",
                  u8"MI AMP PLEXI apenas rompiendo + 4x12 Greenback con Ice Comp y Sky Chorus est\u00e9reo.",
                  false, 0, 1, 1, 0, true, 46.f, 52.f, 62.f, 64.f, 60.f, 64.f, 44.f, 18.f,
                  { 0, 3, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 50.f }, { MiAmpParams::gtrCompAtt, 36.f }, { MiAmpParams::gtrCompLvl, 56.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 34.f }, { MiAmpParams::gtrChorusDepth, 48.f }, { MiAmpParams::gtrChorusMix, 32.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 38.f }, { MiAmpParams::gtrReverbMix, 18.f }
                  });

            addG ("nirvana|kurt cobain|cobain|smells like teen spirit|come as you are|in bloom|lithium|heart shaped box",
                  "90s grunge distortion chorus alt rock",
                  "Nirvana / Cobain \u2022 Twin + Orange Dist",
                  u8"MI AMP TWIN limpio + 2x12 Blackface golpeado por MI Orange Distortion y Sky Chorus estilo Small Clone.",
                  false, 0, 7, 0, 6, true, 36.f, 54.f, 58.f, 64.f, 58.f, 62.f, 38.f, 16.f,
                  { 7, 3 },
                  {
                      { MiAmpParams::gtrDistOn, 1.f }, { MiAmpParams::gtrDistDrive, 68.f }, { MiAmpParams::gtrDistTone, 58.f }, { MiAmpParams::gtrDistLevel, 60.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 44.f }, { MiAmpParams::gtrChorusDepth, 60.f }, { MiAmpParams::gtrChorusMix, 36.f }
                  });

            addG ("pearl jam|mike mccready|stone gossard|alive|even flow|black|jeremy|yellow ledbetter",
                  "90s grunge rock seattle marshall overdrive",
                  "Pearl Jam \u2022 90s Seattle Crunch",
                  u8"MI AMP 800 + 4x12 Greenback con Green Drive TS para solos con alma noventosa.",
                  false, 0, 0, 2, 0, true, 60.f, 54.f, 64.f, 60.f, 58.f, 62.f, 40.f, 20.f,
                  { 1, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 36.f }, { MiAmpParams::gtrDriveTone, 56.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 46.f }, { MiAmpParams::gtrReverbMix, 22.f }
                  });

            addG ("foo fighters|dave grohl|everlong|the pretender|best of you|my hero|times like these|learn to fly",
                  "alt rock post grunge stadium rock drive",
                  "Foo Fighters \u2022 Stadium Recto & AC Crunch",
                  u8"MI AMP RECTO CH2 + 4x12 Brit V30 con Green Drive limpio al frente para acordes enormes y definidos.",
                  false, 0, 2, 1, 1, true, 64.f, 56.f, 64.f, 62.f, 60.f, 60.f, 34.f, 16.f,
                  { 10, 1, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 24.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 22.f }, { MiAmpParams::gtrDriveTone, 58.f }, { MiAmpParams::gtrDriveLvl, 56.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 36.f }, { MiAmpParams::gtrReverbMix, 16.f }
                  });

            addG ("radiohead|jonny greenwood|ed obrien|creep|paranoid android|just|karma police|no surprises",
                  "alt rock 90s brit rock indie distortion delay",
                  "Radiohead \u2022 AC-30 + Orange Bite",
                  u8"MI AMP AC-30 + 2x12 Class-A con MI Orange Distortion para los estallidos de Creep y Pearl Delay.",
                  false, 0, 4, 1, 4, true, 48.f, 50.f, 62.f, 64.f, 60.f, 62.f, 42.f, 22.f,
                  { 7, 5, 6 },
                  {
                      { MiAmpParams::gtrDistOn, 1.f }, { MiAmpParams::gtrDistDrive, 64.f }, { MiAmpParams::gtrDistTone, 56.f }, { MiAmpParams::gtrDistLevel, 58.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 46.f }, { MiAmpParams::gtrDelayMix, 26.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 52.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            addG ("arctic monkeys|alex turner|do i wanna know|r u mine|505|brianstorm|crying lightning|i bet you look good",
                  "indie rock garage rock british fuzz octave",
                  "Arctic Monkeys \u2022 Fuzzy Brit Riff",
                  u8"MI AMP ROCKER + 4x12 Tangerine con Sub Octave sutil y Fuzz grueso para riffs pesados de estudio.",
                  false, 0, 5, 2, 5, false, 58.f, 58.f, 64.f, 56.f, 54.f, 62.f, 42.f, 20.f,
                  { 11, 2, 6 },
                  {
                      { MiAmpParams::gtrOctOn, 1.f }, { MiAmpParams::gtrOctDirect, 80.f }, { MiAmpParams::gtrOctSub1, 36.f }, { MiAmpParams::gtrOctUp, 0.f },
                      { MiAmpParams::gtrFuzzOn, 1.f }, { MiAmpParams::gtrFuzzFuzz, 56.f }, { MiAmpParams::gtrFuzzTone, 50.f }, { MiAmpParams::gtrFuzzLvl, 56.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 46.f }, { MiAmpParams::gtrReverbMix, 22.f }
                  });

            addG ("the strokes|julian casablancas|nick valensi|albert hammond|last nite|reptilia|someday|hard to explain",
                  "indie rock garage 2000s crunch clean drive",
                  "The Strokes \u2022 Tight Garage Crunch",
                  u8"MI AMP TWIN apretado + 2x12 Blackface con Green Drive tipo Jekyll & Hyde seco y cortante.",
                  false, 0, 7, 1, 6, true, 42.f, 48.f, 66.f, 62.f, 58.f, 62.f, 36.f, 14.f,
                  { 0, 1, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 46.f }, { MiAmpParams::gtrCompLvl, 54.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 52.f }, { MiAmpParams::gtrDriveTone, 56.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 30.f }, { MiAmpParams::gtrReverbMix, 14.f }
                  });

            addG ("muse|matt bellamy|plug in baby|knights of cydonia|supermassive black hole|uprising|starlight",
                  "alt rock prog space rock fuzz phaser",
                  "Muse / Bellamy \u2022 Fuzz & Arena Lead",
                  u8"MI AMP BE-100 + 4x12 Brit V30 con Sustain Fuzz explosivo y Phaser espacial.",
                  false, 0, 6, 2, 1, true, 68.f, 54.f, 62.f, 64.f, 62.f, 60.f, 34.f, 18.f,
                  { 10, 2, 4, 5, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 32.f },
                      { MiAmpParams::gtrFuzzOn, 1.f }, { MiAmpParams::gtrFuzzFuzz, 68.f }, { MiAmpParams::gtrFuzzTone, 56.f }, { MiAmpParams::gtrFuzzLvl, 54.f },
                      { MiAmpParams::gtrPhaserOn, 1.f }, { MiAmpParams::gtrPhaserRate, 32.f }, { MiAmpParams::gtrPhaserDepth, 46.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 44.f }, { MiAmpParams::gtrDelayMix, 22.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 48.f }, { MiAmpParams::gtrReverbMix, 20.f }
                  });

            addG ("oasis|noel gallagher|wonderwall|dont look back in anger|supersonic|champagne supernova|live forever",
                  "britpop 90s british rock crunch",
                  "Oasis \u2022 90s Britpop Wall",
                  u8"MI AMP 800 + 4x12 Greenback con medios llenos y Studio Reverb de sala grande.",
                  false, 0, 0, 2, 0, true, 58.f, 54.f, 68.f, 60.f, 58.f, 62.f, 40.f, 22.f,
                  { 1, 5, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 26.f }, { MiAmpParams::gtrDriveTone, 54.f }, { MiAmpParams::gtrDriveLvl, 56.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 42.f }, { MiAmpParams::gtrDelayMix, 18.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 52.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            addG ("u2|the edge|where the streets have no name|with or without you|sunday bloody sunday|pride|beautiful day",
                  "80s rock dotted delay shimmer clean ac30",
                  "U2 / The Edge \u2022 AC-30 Dotted 1/8",
                  u8"MI AMP AC-30 + 2x12 Class-A brillante con Ice Comp y Pearl Delay en corchea con puntillo.",
                  false, 0, 4, 1, 4, true, 44.f, 46.f, 58.f, 68.f, 64.f, 62.f, 44.f, 22.f,
                  { 0, 1, 5, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 56.f }, { MiAmpParams::gtrCompLvl, 56.f },
                      { MiAmpParams::gtrDriveOn, 0.f }, { MiAmpParams::gtrDriveGain, 24.f }, { MiAmpParams::gtrDriveLvl, 54.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 44.f }, { MiAmpParams::gtrDelayFb, 52.f }, { MiAmpParams::gtrDelayMix, 42.f }, { MiAmpParams::gtrDelaySubdiv, 1.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 56.f }, { MiAmpParams::gtrReverbMix, 26.f }
                  });

            addG ("story so far|the story so far|will levy|kevin geyer|proper dose|what you dont see|quicksand|nerve|empty space|out of it|heavy gloom",
                  "pop punk easycore modern punk",
                  "The Story So Far \u2022 Recto Pop-Punk",
                  u8"MI AMP RECTO CH2 + 4x12 Brit V30 apretado con Gate, Green Drive boost suave y Reverb corta.",
                  false, -2, 2, 1, 1, true, 66.f, 54.f, 64.f, 64.f, 64.f, 60.f, 34.f, 14.f,
                  { 10, 1, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 32.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 18.f }, { MiAmpParams::gtrDriveTone, 58.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 32.f }, { MiAmpParams::gtrReverbMix, 14.f }
                  });

            addG ("clairvoyant|placeholder|navy blue|keep this up|take me as you please|story so far clean",
                  "indie clean acoustic arpeggio chorus reverb",
                  "The Story So Far \u2022 Clairvoyant Clean",
                  u8"MI AMP TWIN + 2x12 Blackface con Compresor, Sky Chorus est\u00e9reo y Studio Reverb.",
                  false, 0, 7, 0, 6, true, 30.f, 48.f, 56.f, 64.f, 58.f, 62.f, 48.f, 24.f,
                  { 0, 3, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 50.f }, { MiAmpParams::gtrCompAtt, 38.f }, { MiAmpParams::gtrCompLvl, 54.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusRate, 34.f }, { MiAmpParams::gtrChorusDepth, 50.f }, { MiAmpParams::gtrChorusMix, 36.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 54.f }, { MiAmpParams::gtrReverbTone, 60.f }, { MiAmpParams::gtrReverbMix, 28.f }
                  });

            addG ("blink 182|blink|tom delonge|enema|dammit|whats my age again|all the small things|anthem|feeling this|90s cali punk",
                  "pop punk cali punk 90s 00s mesa",
                  "Blink-182 \u2022 90s/00s Cali Pop-Punk",
                  u8"MI AMP RECTO CH2 + 4x12 Brit V30 con Gate limpio, pegada abierta en acordes y Chorus sutil listo para puentes.",
                  false, 0, 2, 1, 1, true, 64.f, 54.f, 62.f, 64.f, 64.f, 60.f, 34.f, 14.f,
                  { 10, 1, 3 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 26.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 16.f }, { MiAmpParams::gtrDriveTone, 58.f }, { MiAmpParams::gtrDriveLvl, 56.f },
                      { MiAmpParams::gtrChorusOn, 0.f }, { MiAmpParams::gtrChorusMix, 24.f }
                  });

            addG ("green day|billie joe|billie joe armstrong|dookie|basket case|american idiot|boulevard of broken dreams|when i come around",
                  "punk rock 90s british crunch marshall",
                  "Green Day \u2022 Modded Brit Punk",
                  u8"MI AMP 800 + 4x12 Greenback con medios brit\u00e1nicos bien presentes y cero barro.",
                  false, -1, 0, 2, 0, true, 64.f, 50.f, 70.f, 62.f, 60.f, 62.f, 36.f, 14.f,
                  { 1, 12 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 24.f }, { MiAmpParams::gtrDriveTone, 56.f }, { MiAmpParams::gtrDriveLvl, 56.f },
                      { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[2], 56.f }, { MiAmpParams::gtrEqBand[3], 60.f }
                  });

            addG ("turnstile|glow on|blackout|mystery|pat mccrory|brady ebert",
                  "hardcore punk groove alt rock chorus",
                  "Turnstile \u2022 Hardcore Groove",
                  u8"MI AMP ROCKER + 4x12 Brit V30 con Gate r\u00e1pido, pegada gorda y Reverb corta.",
                  false, -2, 5, 2, 1, true, 70.f, 56.f, 64.f, 64.f, 62.f, 60.f, 34.f, 16.f,
                  { 10, 3, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 36.f },
                      { MiAmpParams::gtrChorusOn, 0.f }, { MiAmpParams::gtrChorusRate, 40.f }, { MiAmpParams::gtrChorusDepth, 50.f }, { MiAmpParams::gtrChorusMix, 28.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 36.f }, { MiAmpParams::gtrReverbMix, 16.f }
                  });

            addG ("neck deep|december|in bloom|gold steps|paramore|misery business|crushcrushcrush|sum 41|fat lip|in too deep|nofx|offspring",
                  "pop punk skate punk 2000s tight crunch",
                  "Pop-Punk / Skate Punk \u2022 Tight Bite",
                  u8"MI AMP RECTO CH2 + 4x12 Brit V30 con Gate, Green Drive tight y medios altos cortantes.",
                  false, -2, 2, 1, 1, true, 66.f, 52.f, 64.f, 66.f, 66.f, 60.f, 34.f, 14.f,
                  { 10, 1, 12, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 32.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 18.f }, { MiAmpParams::gtrDriveTone, 58.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[0], 46.f }, { MiAmpParams::gtrEqBand[4], 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 30.f }, { MiAmpParams::gtrReverbMix, 12.f }
                  });

            addG ("midwest emo|american football|math rock|polyphia|tim henson|covet|ichika|chon|twinkle",
                  "midwest emo math rock clean sparkle tapping compressor",
                  "Math Rock / Midwest Emo \u2022 Sparkle Clean",
                  u8"MI AMP TWIN + 2x12 Blackface ultra limpio con Compresor sustain alto para tapping y arpegios definidos.",
                  false, 0, 7, 0, 6, true, 28.f, 44.f, 58.f, 68.f, 64.f, 64.f, 46.f, 20.f,
                  { 0, 5, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 72.f }, { MiAmpParams::gtrCompAtt, 28.f }, { MiAmpParams::gtrCompLvl, 60.f }, { MiAmpParams::gtrCompTone, 64.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 34.f }, { MiAmpParams::gtrDelayMix, 22.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 50.f }, { MiAmpParams::gtrReverbMix, 26.f }
                  });

            addG ("title fight|floral green|shed|shoegaze|my bloody valentine|slowdive|deafheaven|post hardcore",
                  "shoegaze grunge chorus delay reverb wall",
                  "Shoegaze / Post-Hardcore \u2022 Chorus Wall",
                  u8"MI AMP ROCKER + 4x12 Greenback con Dual Crunch, Sky Chorus ancho y Reverb larga.",
                  false, -1, 5, 2, 0, true, 58.f, 56.f, 60.f, 60.f, 56.f, 60.f, 44.f, 24.f,
                  { 8, 3, 5, 6 },
                  {
                      { MiAmpParams::gtrDualOn, 1.f }, { MiAmpParams::gtrDualGain, 42.f }, { MiAmpParams::gtrDualDrive, 46.f }, { MiAmpParams::gtrDualBlend, 56.f }, { MiAmpParams::gtrDualOutput, 52.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusDepth, 66.f }, { MiAmpParams::gtrChorusMix, 44.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 48.f }, { MiAmpParams::gtrDelayMix, 32.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 70.f }, { MiAmpParams::gtrReverbMix, 38.f }
                  });

            // =========================================================================
            // GUITAR RIGS — METAL / HARD ROCK / MODERN HI-GAIN
            // =========================================================================
            addG ("metallica|james hetfield|kirk hammett|master of puppets|enter sandman|one|ride the lightning|sad but true|fade to black",
                  "thrash metal 80s 90s scoop heavy metal riff",
                  "Metallica \u2022 80s/90s Thrash Scoop",
                  u8"MI AMP RECTO CH3 + 4x12 Oversized con Gate r\u00e1pido y Graphic EQ en V para palm-mutes demoledores.",
                  false, 0, 2, 2, 2, true, 74.f, 62.f, 42.f, 66.f, 64.f, 60.f, 32.f, 12.f,
                  { 10, 1, 12 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 40.f }, { MiAmpParams::gtrGateDecay, 20.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 14.f }, { MiAmpParams::gtrDriveTone, 58.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[0], 58.f }, { MiAmpParams::gtrEqBand[2], 42.f }, { MiAmpParams::gtrEqBand[3], 38.f }, { MiAmpParams::gtrEqBand[5], 60.f }
                  });

            addG ("megadeth|dave mustaine|marty friedman|holy wars|tornado of souls|symphony of destruction",
                  "thrash metal shred tight mids solo",
                  "Megadeth \u2022 Tight Thrash & Lead",
                  u8"MI AMP BE-100 + 4x12 Brit V30 con Gate y Green Drive apretando graves para riffs r\u00e1pidos.",
                  false, 0, 6, 2, 1, true, 72.f, 50.f, 66.f, 66.f, 64.f, 60.f, 32.f, 12.f,
                  { 10, 1, 5 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 38.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 18.f }, { MiAmpParams::gtrDriveTone, 60.f }, { MiAmpParams::gtrDriveLvl, 60.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 40.f }, { MiAmpParams::gtrDelayMix, 16.f }
                  });

            addG ("iron maiden|dave murray|adrian smith|the trooper|fear of the dark|run to the hills|hallowed be thy name",
                  "nwobhm heavy metal 80s british twin guitar",
                  "Iron Maiden \u2022 80s Brit Heavy Metal",
                  u8"MI AMP 800 + 4x12 Greenback con Green Drive boost y Delay discreto para melod\u00edas dobladas.",
                  false, 0, 0, 2, 0, true, 68.f, 52.f, 68.f, 62.f, 60.f, 62.f, 36.f, 16.f,
                  { 10, 1, 5, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 26.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 26.f }, { MiAmpParams::gtrDriveTone, 58.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 40.f }, { MiAmpParams::gtrDelayMix, 18.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 36.f }, { MiAmpParams::gtrReverbMix, 14.f }
                  });

            addG ("black sabbath|tony iommi|paranoid|iron man|war pigs|n i b|children of the grave",
                  "heavy metal 70s doom metal dark crunch fuzz",
                  "Black Sabbath / Iommi \u2022 Heavy Doom",
                  u8"MI AMP ROCKER + 4x12 Tangerine con medios gruesos y Fuzz/Drive vintage oscuro.",
                  false, -3, 5, 2, 5, false, 68.f, 62.f, 68.f, 54.f, 50.f, 62.f, 40.f, 18.f,
                  { 1, 6 },
                  {
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 44.f }, { MiAmpParams::gtrDriveTone, 48.f }, { MiAmpParams::gtrDriveLvl, 58.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 38.f }, { MiAmpParams::gtrReverbMix, 16.f }
                  });

            addG ("pantera|dimebag|dimebag darrell|cowboys from hell|walk|cemetery gates|domination",
                  "groove metal 90s scoop tight aggressive",
                  "Pantera / Dimebag \u2022 Razor Scoop",
                  u8"MI AMP HG-120 Lead + 4x12 Hi-Gain con Gate cortante y Graphic EQ scoop agresivo.",
                  false, -2, 3, 2, 3, true, 76.f, 58.f, 38.f, 70.f, 68.f, 60.f, 30.f, 10.f,
                  { 10, 12 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 48.f }, { MiAmpParams::gtrGateDecay, 16.f },
                      { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[0], 56.f }, { MiAmpParams::gtrEqBand[2], 38.f }, { MiAmpParams::gtrEqBand[3], 34.f }, { MiAmpParams::gtrEqBand[5], 64.f }
                  });

            addG ("slipknot|mick thomson|jim root|duality|psychosocial|before i forget|wait and bleed|avenged sevenfold|synyster gates|system of a down|chop suey|toxicity",
                  "modern metal nu metal alt metal tight high gain",
                  "Modern Metal (HG-120) \u2022 Tight Chug",
                  u8"MI AMP HG-120 Lead + 4x12 Oversized con Gate r\u00e1pido y Green Drive como booster limpio (Gain 10, Level 62).",
                  false, -3, 3, 2, 2, true, 70.f, 54.f, 60.f, 66.f, 66.f, 58.f, 30.f, 10.f,
                  { 10, 1, 12 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 46.f }, { MiAmpParams::gtrGateDecay, 16.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 10.f }, { MiAmpParams::gtrDriveTone, 60.f }, { MiAmpParams::gtrDriveLvl, 62.f },
                      { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[0], 46.f }, { MiAmpParams::gtrEqBand[4], 58.f }
                  });

            addG ("deftones|stephen carpenter|my own summer|around the fur|white pony|be quiet and drive|change|digital bath",
                  "nu metal alt metal shoegaze heavy wall drop",
                  "Deftones \u2022 Recto Heavy Wall & Shimmer",
                  u8"MI AMP RECTO CH3 + 4x12 Oversized con Gate, Chorus y Reverb atmosf\u00e9rica.",
                  false, -3, 2, 2, 2, true, 76.f, 62.f, 56.f, 64.f, 62.f, 60.f, 34.f, 18.f,
                  { 10, 3, 5, 6 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 42.f },
                      { MiAmpParams::gtrChorusOn, 0.f }, { MiAmpParams::gtrChorusMix, 26.f },
                      { MiAmpParams::gtrDelayOn, 0.f }, { MiAmpParams::gtrDelayTime, 46.f }, { MiAmpParams::gtrDelayMix, 22.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 46.f }, { MiAmpParams::gtrReverbMix, 20.f }
                  });

            addG ("knocked loose|gojira|architects|spiritbox|periphery|meshuggah|metalcore|djent|beatdown|progressive metal|modern metal",
                  "metalcore djent beatdown modern high gain tight gate",
                  "Modern Metalcore / Djent \u2022 HG-120 Tight",
                  u8"MI AMP HG-120 Lead + 4x12 Hi-Gain con Gate ultrarr\u00e1pido, Green Drive tight y EQ cortando barro en 200 Hz.",
                  false, -4, 3, 2, 3, true, 68.f, 48.f, 64.f, 68.f, 70.f, 58.f, 28.f, 10.f,
                  { 10, 1, 12 },
                  {
                      { MiAmpParams::gtrGateOn, 1.f }, { MiAmpParams::gtrGateThresh, 54.f }, { MiAmpParams::gtrGateDecay, 14.f },
                      { MiAmpParams::gtrDriveOn, 1.f }, { MiAmpParams::gtrDriveGain, 10.f }, { MiAmpParams::gtrDriveTone, 62.f }, { MiAmpParams::gtrDriveLvl, 64.f },
                      { MiAmpParams::gtrEqOn, 1.f }, { MiAmpParams::gtrEqBand[1], 38.f }, { MiAmpParams::gtrEqBand[4], 62.f }
                  });

            addG ("queens of the stone age|qotsa|josh homme|kyuss|no one knows|little sister|stoner|doom|desert rock",
                  "stoner rock desert rock fuzz phaser mid heavy",
                  "QOTSA / Stoner \u2022 Rocker + Tangerine Fuzz",
                  u8"MI AMP ROCKER + 4x12 Tangerine con medios empujados, Sustain Fuzz grueso y Phaser lento.",
                  false, -4, 5, 2, 5, false, 64.f, 64.f, 70.f, 52.f, 50.f, 62.f, 40.f, 18.f,
                  { 2, 4, 6 },
                  {
                      { MiAmpParams::gtrFuzzOn, 1.f }, { MiAmpParams::gtrFuzzFuzz, 70.f }, { MiAmpParams::gtrFuzzTone, 48.f }, { MiAmpParams::gtrFuzzLvl, 58.f },
                      { MiAmpParams::gtrPhaserOn, 1.f }, { MiAmpParams::gtrPhaserRate, 26.f }, { MiAmpParams::gtrPhaserDepth, 52.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 38.f }, { MiAmpParams::gtrReverbMix, 16.f }
                  });

            // =========================================================================
            // GUITAR RIGS — CLEAN / FUNK / JAZZ / AMBIENT / DIRECTO
            // =========================================================================
            addG ("nile rodgers|chic|funk|le freak|get lucky|daft punk|funk guitar|clean funk",
                  "funk clean disco crisp strat",
                  "Funk Clean \u2022 Twin Crisp Compressor",
                  u8"MI AMP TWIN + 2x12 Blackface ultra limpio y percusivo con Compresor r\u00e1pido y Reverb corta.",
                  false, 0, 7, 0, 6, true, 26.f, 44.f, 60.f, 68.f, 62.f, 64.f, 40.f, 14.f,
                  { 0, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 54.f }, { MiAmpParams::gtrCompAtt, 24.f }, { MiAmpParams::gtrCompLvl, 58.f }, { MiAmpParams::gtrCompTone, 62.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 28.f }, { MiAmpParams::gtrReverbMix, 12.f }
                  });

            addG ("jazz|wes montgomery|pat metheny|george benson|neo soul|mateus asato|tom misch|warm clean",
                  "jazz neo soul warm clean hollowbody",
                  "Jazz / Neo-Soul \u2022 Warm Clean Twin",
                  u8"MI AMP TWIN sin Bright + 2x12 Blackface con medios redondos, Compresor suave y Studio Reverb tibia.",
                  false, 0, 7, 0, 6, false, 28.f, 54.f, 64.f, 44.f, 46.f, 64.f, 62.f, 22.f,
                  { 0, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 46.f }, { MiAmpParams::gtrCompAtt, 46.f }, { MiAmpParams::gtrCompLvl, 55.f }, { MiAmpParams::gtrCompTone, 42.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 48.f }, { MiAmpParams::gtrReverbTone, 44.f }, { MiAmpParams::gtrReverbMix, 24.f }
                  });

            addG ("ambient|post rock|worship|hillsong|explosions in the sky|sigur ros|atmospheric|shimmer",
                  "ambient post rock worship clean delay reverb atmospheric",
                  "Ambient / Post-Rock \u2022 Pad & Dotted Delay",
                  u8"MI AMP TWIN + 2x12 Class-A con Compresor, Sky Chorus, Pearl Delay dotted 1/8 y Studio Reverb profunda.",
                  false, 0, 7, 0, 4, true, 26.f, 50.f, 56.f, 62.f, 58.f, 64.f, 52.f, 28.f,
                  { 0, 3, 5, 6 },
                  {
                      { MiAmpParams::gtrCompOn, 1.f }, { MiAmpParams::gtrCompSus, 60.f }, { MiAmpParams::gtrCompLvl, 56.f },
                      { MiAmpParams::gtrChorusOn, 1.f }, { MiAmpParams::gtrChorusMix, 34.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 46.f }, { MiAmpParams::gtrDelayFb, 56.f }, { MiAmpParams::gtrDelayMix, 46.f }, { MiAmpParams::gtrDelaySubdiv, 1.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 80.f }, { MiAmpParams::gtrReverbMix, 50.f }
                  });

            addG ("dual directo|solo pedales|preamp directo",
                  "direct pedalboard preamp guitar",
                  "Dual Crunch \u2022 Solo Pedales (Directo)",
                  u8"Modo SOLO PEDALES con MI Dual Crunch equilibrado + Pearl Delay + Studio Reverb.",
                  true, 0, 8, 2, 7, true, 60.f, 54.f, 60.f, 60.f, 60.f, 60.f, 36.f, 16.f,
                  { 8, 5, 6 },
                  {
                      { MiAmpParams::gtrDualOn, 1.f }, { MiAmpParams::gtrDualGain, 44.f }, { MiAmpParams::gtrDualDrive, 44.f }, { MiAmpParams::gtrDualBlend, 54.f }, { MiAmpParams::gtrDualTone, 55.f }, { MiAmpParams::gtrDualOutput, 54.f },
                      { MiAmpParams::gtrDelayOn, 1.f }, { MiAmpParams::gtrDelayTime, 40.f }, { MiAmpParams::gtrDelayFb, 30.f }, { MiAmpParams::gtrDelayMix, 22.f },
                      { MiAmpParams::gtrReverbOn, 1.f }, { MiAmpParams::gtrReverbDec, 42.f }, { MiAmpParams::gtrReverbMix, 22.f }
                  });

            // =========================================================================
            // BASS RIGS — ROCK / FUNK / METAL / POP-PUNK / LATAM / STUDIO
            // =========================================================================
            addB ("flea|red hot chili peppers|rhcp|around the world|cant stop|higher ground|dark necessities|slap|funk bass",
                  "funk slap bass scooped punchy 4x10",
                  "Flea / RHCP \u2022 RB-800 Funk & Slap",
                  u8"MI AMP RB-800 + 4x10 Hi-Punch con MI-87 ataque r\u00e1pido y Bass EQ en sonrisa definida.",
                  false, 0, 3, 2, true, 56.f, 64.f, 44.f, 52.f, 66.f, 34.f, 14.f,
                  { 0, 13 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 70.f }, { MiAmpParams::compAttack, 22.f }, { MiAmpParams::compLevel, 60.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 64.f }, { MiAmpParams::bassEqBand[2], 38.f }, { MiAmpParams::bassEqBand[5], 70.f }
                  });

            addB ("tool|justin chancellor|schism|forty six and 2|the pot|lateralus|pneuma",
                  "prog metal bass pick growl flanger delay",
                  "Tool / Justin Chancellor \u2022 Prog Growl",
                  u8"MI AMP RB-800 + 4x10 Hi-Punch con MI-87, Bass Driver con mordida de p\u00faa y Flanger/Delay listos.",
                  false, -2, 3, 2, true, 60.f, 54.f, 68.f, 56.f, 64.f, 32.f, 14.f,
                  { 0, 5, 9, 10 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 52.f },
                      { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 36.f }, { MiAmpParams::bddiBlend, 58.f }, { MiAmpParams::bddiPresence, 68.f }, { MiAmpParams::bddiLevel, 54.f },
                      { MiAmpParams::bassFlangeOn, 0.f }, { MiAmpParams::bassFlangeDepth, 45.f }, { MiAmpParams::bassFlangeRate, 24.f },
                      { MiAmpParams::bassDelayOn, 0.f }, { MiAmpParams::bassDelayMix, 24.f }
                  });

            addB ("muse|hysteria|chris wolstenholme|uprising|time is running out|supermassive black hole",
                  "fuzz synth octave bass rock",
                  "Muse / Hysteria \u2022 Fuzz + Poly Octave",
                  u8"MI AMP OR-200 + 8x10 Classic con Poly Octave, Olive Fuzz y XV en paralelo para no perder graves.",
                  false, 0, 6, 4, false, 58.f, 62.f, 56.f, 50.f, 56.f, 34.f, 14.f,
                  { 0, 12, 8, 1 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f },
                      { MiAmpParams::bassOctOn, 1.f }, { MiAmpParams::bassOctDry, 78.f }, { MiAmpParams::bassOctSub, 60.f }, { MiAmpParams::bassOctUp, 18.f },
                      { MiAmpParams::oliveOn, 1.f }, { MiAmpParams::oliveSustain, 56.f }, { MiAmpParams::oliveTone, 54.f }, { MiAmpParams::oliveMode, 1.f },
                      { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvDrive, 26.f }, { MiAmpParams::xvBlend, 52.f }, { MiAmpParams::xvLevel, 52.f }, { MiAmpParams::xvCab, 1.f }
                  });

            addB ("soda stereo|zeta bosio|cerati|persiana americana|de musica ligera|ciudad de la furia",
                  "rock argentino 80s 90s chorus bass",
                  "Soda Stereo / Zeta Bosio \u2022 PRO-450 + Chorus",
                  u8"MI AMP PRO-450 + 4x10 Touring con MI-87 Compresor y MI Deep Chorus ochentoso.",
                  false, 0, 1, 1, true, 54.f, 58.f, 60.f, 54.f, 60.f, 38.f, 18.f,
                  { 0, 2, 4 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f }, { MiAmpParams::compLevel, 55.f },
                      { MiAmpParams::chorusOn, 1.f }, { MiAmpParams::chorusRate, 34.f }, { MiAmpParams::chorusDepth, 46.f }, { MiAmpParams::chorusMix, 32.f },
                      { MiAmpParams::reverbOn, 0.f }, { MiAmpParams::reverbMix, 16.f }
                  });

            addB ("divididos|diego arnedo|arnedo|ala delta|spaghetti del rock|el 38|los redondos|semilla bucciarelli",
                  "rock nacional hard rock cl300 growl bass",
                  "Divididos / Arnedo \u2022 CL-300 Rock Growl",
                  u8"MI AMP CL-300 + 8x10 Classic con MI-87 y Bass Driver con mordida valvulada bien gorda.",
                  false, 0, 0, 4, false, 64.f, 64.f, 62.f, 52.f, 56.f, 36.f, 14.f,
                  { 0, 5 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f }, { MiAmpParams::compLevel, 55.f },
                      { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 34.f }, { MiAmpParams::bddiBlend, 56.f }, { MiAmpParams::bddiPresence, 60.f }, { MiAmpParams::bddiLevel, 54.f }
                  });

            addB ("metallica|cliff burton|jason newsted|robert trujillo|master of puppets|enter sandman|for whom the bell tolls|orion",
                  "thrash metal bass pick fuzz growl",
                  "Metallica \u2022 CL-300 Thrash Pick & Growl",
                  u8"MI AMP CL-300 + 8x10 Classic con Bass Food y Bass EQ en sonrisa cortante.",
                  false, 0, 0, 4, true, 62.f, 62.f, 46.f, 52.f, 66.f, 32.f, 12.f,
                  { 0, 7, 13 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f },
                      { MiAmpParams::bfOn, 1.f }, { MiAmpParams::bfDrive, 38.f }, { MiAmpParams::bfBlend, 58.f }, { MiAmpParams::bfVol, 54.f }, { MiAmpParams::bfPad, 1.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 58.f }, { MiAmpParams::bassEqBand[3], 40.f }, { MiAmpParams::bassEqBand[5], 64.f }
                  });

            addB ("iron maiden|steve harris|the trooper|run to the hills|fear of the dark",
                  "heavy metal fingerstyle clank bright bass",
                  "Iron Maiden / Steve Harris \u2022 Finger Clank",
                  u8"MI AMP RB-800 + 4x10 Hi-Punch con medios-agudos agresivos para galope de dedos.",
                  false, 0, 3, 2, true, 56.f, 54.f, 66.f, 64.f, 70.f, 32.f, 12.f,
                  { 0, 13 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 58.f }, { MiAmpParams::compAttack, 26.f }, { MiAmpParams::compLevel, 58.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[2], 56.f }, { MiAmpParams::bassEqBand[4], 64.f }, { MiAmpParams::bassEqBand[5], 68.f }
                  });

            addB ("rush|geddy lee|tom sawyer|yyz|limelight|spirit of radio",
                  "prog rock jazz bass sansamp growl",
                  "Rush / Geddy Lee \u2022 PRO-450 SansAmp Grind",
                  u8"MI AMP PRO-450 + 8x10 Classic con Bass Driver saturando medios agudos estilo SansAmp.",
                  false, 0, 1, 4, true, 60.f, 54.f, 64.f, 58.f, 68.f, 34.f, 14.f,
                  { 0, 5 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f },
                      { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 44.f }, { MiAmpParams::bddiBlend, 62.f }, { MiAmpParams::bddiTreble, 62.f }, { MiAmpParams::bddiPresence, 72.f }, { MiAmpParams::bddiLevel, 54.f }
                  });

            addB ("guns n roses|duff mckagan|sweet child o mine|paradise city|welcome to the jungle",
                  "hard rock 80s gk chorus pick bass",
                  "GNR / Duff McKagan \u2022 RB-800 + Chorus",
                  u8"MI AMP RB-800 + 4x10 Hi-Punch con toque de p\u00faa y Deep Chorus espacial.",
                  false, -1, 3, 2, true, 56.f, 56.f, 62.f, 56.f, 66.f, 36.f, 16.f,
                  { 0, 2 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 52.f },
                      { MiAmpParams::chorusOn, 1.f }, { MiAmpParams::chorusRate, 32.f }, { MiAmpParams::chorusDepth, 42.f }, { MiAmpParams::chorusMix, 28.f }
                  });

            addB ("story so far|kelen capener|proper dose|quicksand|nerve|empty space",
                  "pop punk pick bass cl300",
                  "The Story So Far \u2022 CL-300 Pop-Punk",
                  u8"MI AMP CL-300 + 8x10 Classic con MI-87 y Bass Driver definido para ataque de p\u00faa.",
                  false, -2, 0, 4, true, 58.f, 58.f, 60.f, 54.f, 62.f, 34.f, 12.f,
                  { 0, 5 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f }, { MiAmpParams::compAttack, 32.f },
                      { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 30.f }, { MiAmpParams::bddiBlend, 56.f }, { MiAmpParams::bddiPresence, 66.f }, { MiAmpParams::bddiLevel, 54.f }
                  });

            addB ("blink 182|mark hoppus|enema|dammit|whats my age again|all the small things|90s cali punk",
                  "pop punk cali punk svt pick bass",
                  "Blink-182 / Hoppus \u2022 CL-300 Cali Punk",
                  u8"MI AMP CL-300 + 8x10 Classic con MI-87 y XV Preamp limpio en paralelo para golpe de p\u00faa.",
                  false, 0, 0, 4, true, 56.f, 62.f, 54.f, 50.f, 64.f, 34.f, 12.f,
                  { 0, 1 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 52.f }, { MiAmpParams::compAttack, 30.f },
                      { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvLevel, 54.f }, { MiAmpParams::xvDrive, 26.f }, { MiAmpParams::xvLow, 62.f }, { MiAmpParams::xvBlend, 54.f }, { MiAmpParams::xvMid, 34.f }, { MiAmpParams::xvPresence, 72.f }, { MiAmpParams::xvCab, 1.f }
                  });

            addB ("green day|mike dirnt|dookie|longview|basket case|american idiot",
                  "punk rock 90s mid punch pick bass",
                  "Green Day / Mike Dirnt \u2022 Mid Punch",
                  u8"MI AMP CL-300 + 8x10 Classic con Bass Food y Bass EQ realzando medios de Gibson G-3/Precision.",
                  false, -1, 0, 4, true, 60.f, 56.f, 68.f, 56.f, 62.f, 34.f, 12.f,
                  { 0, 7, 13 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 50.f },
                      { MiAmpParams::bfOn, 1.f }, { MiAmpParams::bfDrive, 34.f }, { MiAmpParams::bfBlend, 54.f }, { MiAmpParams::bfVol, 54.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[2], 58.f }, { MiAmpParams::bassEqBand[4], 60.f }
                  });

            addB ("turnstile|franz lyons|glow on|blackout|mystery|hardcore",
                  "hardcore punk bass groove",
                  "Turnstile \u2022 CL-300 Hardcore Groove",
                  u8"MI AMP CL-300 + 8x10 Classic con Gate, MI-87 Compresor y Bass Driver con pegada.",
                  false, -2, 0, 4, true, 60.f, 62.f, 62.f, 54.f, 60.f, 34.f, 12.f,
                  { 11, 0, 5 },
                  {
                      { MiAmpParams::bassGateOn, 1.f }, { MiAmpParams::bassGateThresh, 32.f },
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f },
                      { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 34.f }, { MiAmpParams::bddiBlend, 58.f }, { MiAmpParams::bddiPresence, 66.f }, { MiAmpParams::bddiLevel, 54.f }
                  });

            addB ("neck deep|paramore|sum 41|fat lip|pop punk",
                  "pop punk modern pick bite",
                  "Modern Pop-Punk \u2022 M-900 Pick & Bite",
                  u8"MI AMP M-900 + 4x10 Touring con MI-87 y Bass Driver definiendo cada nota de p\u00faa.",
                  false, -2, 4, 1, true, 58.f, 58.f, 58.f, 54.f, 66.f, 32.f, 12.f,
                  { 0, 5, 13 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f },
                      { MiAmpParams::bddiOn, 1.f }, { MiAmpParams::bddiDrive, 34.f }, { MiAmpParams::bddiBlend, 58.f }, { MiAmpParams::bddiPresence, 68.f }, { MiAmpParams::bddiLevel, 54.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 54.f }, { MiAmpParams::bassEqBand[5], 62.f }
                  });

            addB ("deftones|chi cheng|sergio vega|korn|fieldy|nu metal",
                  "nu metal drop wall heavy bass",
                  "Deftones / Nu-Metal \u2022 M-900 Heavy Wall",
                  u8"MI AMP M-900 + 6x10 Heavy con Gate, MI-87 y Gold Drive controlado.",
                  false, -3, 4, 3, false, 64.f, 66.f, 56.f, 50.f, 58.f, 32.f, 12.f,
                  { 11, 0, 6, 2 },
                  {
                      { MiAmpParams::bassGateOn, 1.f }, { MiAmpParams::bassGateThresh, 36.f },
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 56.f },
                      { MiAmpParams::goldOn, 1.f }, { MiAmpParams::goldGain, 38.f }, { MiAmpParams::goldBlend, 56.f }, { MiAmpParams::goldEq, 54.f }, { MiAmpParams::goldLevel, 54.f },
                      { MiAmpParams::chorusOn, 0.f }, { MiAmpParams::chorusMix, 24.f }
                  });

            addB ("knocked loose|darkglass|djent|metalcore|periphery|architects|spiritbox|modern metal",
                  "metalcore djent clank darkglass modern",
                  "Modern Metal / Djent \u2022 M-900 Clank",
                  u8"MI AMP M-900 + 6x10 Heavy con Gate, MI-87 y XV Preamp para clank cristalino sobre graves firmes.",
                  false, -4, 4, 3, true, 62.f, 58.f, 52.f, 56.f, 68.f, 30.f, 10.f,
                  { 11, 0, 1, 13 },
                  {
                      { MiAmpParams::bassGateOn, 1.f }, { MiAmpParams::bassGateThresh, 42.f },
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 58.f },
                      { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvDrive, 40.f }, { MiAmpParams::xvBlend, 58.f }, { MiAmpParams::xvLevel, 52.f }, { MiAmpParams::xvMid, 34.f }, { MiAmpParams::xvPresence, 76.f }, { MiAmpParams::xvCab, 1.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 56.f }, { MiAmpParams::bassEqBand[5], 62.f }
                  });

            addB ("nirvana|krist novoselic|foo fighters|nate mendel|pearl jam|grunge",
                  "90s grunge alt rock bass",
                  "90s Grunge \u2022 CL-300 Warm Crunch",
                  u8"MI AMP CL-300 + 8x10 Classic con MI-87 y Bass Food overdrive c\u00e1lido.",
                  false, 0, 0, 4, false, 58.f, 62.f, 62.f, 52.f, 56.f, 36.f, 14.f,
                  { 0, 7 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 52.f },
                      { MiAmpParams::bfOn, 1.f }, { MiAmpParams::bfDrive, 34.f }, { MiAmpParams::bfBlend, 54.f }, { MiAmpParams::bfVol, 54.f }
                  });

            addB ("motown|james jamerson|paul mccartney|beatles|soul|vintage warm|b15",
                  "soul motown vintage warm round bass",
                  "Motown / Vintage Soul \u2022 B-15 + 1x15",
                  u8"MI AMP B-15 Flip-Top + 1x15 Vintage con Compresor redondo sin estridencias.",
                  false, 0, 5, 0, false, 52.f, 66.f, 58.f, 44.f, 34.f, 56.f, 20.f,
                  { 0, 13 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 60.f }, { MiAmpParams::compAttack, 50.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[0], 62.f }, { MiAmpParams::bassEqBand[5], 32.f }
                  });

            addB ("jaco|jaco pastorius|marcus miller|jazz|fusion|victor wooten|weather report",
                  "jazz fusion slap articulated touring bass",
                  "Jaco / Fusion \u2022 DB-750 + Chorus",
                  u8"MI AMP DB-750 + 4x10 Touring con MI-87 definido y Deep Chorus sutil para puente fretless/jazz.",
                  false, 0, 7, 1, true, 50.f, 56.f, 64.f, 56.f, 60.f, 44.f, 18.f,
                  { 0, 2, 13 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 56.f }, { MiAmpParams::compAttack, 34.f },
                      { MiAmpParams::chorusOn, 1.f }, { MiAmpParams::chorusRate, 30.f }, { MiAmpParams::chorusDepth, 36.f }, { MiAmpParams::chorusMix, 24.f },
                      { MiAmpParams::bassEqOn, 1.f }, { MiAmpParams::bassEqBand[2], 58.f }, { MiAmpParams::bassEqBand[4], 60.f }
                  });

            addB ("stoner|queens of the stone age|qotsa|kyuss|royal blood|doom",
                  "stoner doom fuzz octave bass",
                  "Stoner / Royal Blood \u2022 OR-200 + Fuzz",
                  u8"MI AMP OR-200 + 8x10 Tangerine con Poly Octave y MI Olive Fuzz masivo.",
                  false, -4, 6, 5, false, 64.f, 68.f, 60.f, 50.f, 50.f, 36.f, 16.f,
                  { 0, 12, 8 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 52.f },
                      { MiAmpParams::bassOctOn, 1.f }, { MiAmpParams::bassOctDry, 76.f }, { MiAmpParams::bassOctSub, 54.f }, { MiAmpParams::bassOctUp, 24.f },
                      { MiAmpParams::oliveOn, 1.f }, { MiAmpParams::oliveSustain, 56.f }, { MiAmpParams::oliveTone, 48.f }, { MiAmpParams::oliveVol, 54.f }, { MiAmpParams::oliveMode, 1.f }
                  });

            addB ("pink floyd|roger waters|indie|shoegaze|post rock|ambient|title fight",
                  "indie classic rock round pick bass chorus reverb",
                  "Classic / Indie \u2022 Bassman + Deep Chorus",
                  u8"MI AMP BASSMAN + 1x15 Vintage con MI-87, Deep Chorus sutil y Reverb limpia.",
                  false, 0, 2, 0, false, 52.f, 60.f, 60.f, 50.f, 56.f, 44.f, 20.f,
                  { 0, 2, 4 },
                  {
                      { MiAmpParams::compOn, 1.f }, { MiAmpParams::compSustain, 54.f },
                      { MiAmpParams::chorusOn, 1.f }, { MiAmpParams::chorusRate, 32.f }, { MiAmpParams::chorusDepth, 44.f }, { MiAmpParams::chorusMix, 28.f },
                      { MiAmpParams::reverbOn, 1.f }, { MiAmpParams::reverbDecay, 40.f }, { MiAmpParams::reverbMix, 18.f }
                  });

            addB ("xv original|xv directo|solo pedales|preamp directo",
                  "direct preamp bass xv",
                  "XV Original \u2022 Solo Pedales (Puro)",
                  u8"Modo SOLO PEDALES con XV Preamp puro (Cab PUNK) sin cabezal.",
                  true, 0, 8, 4, false, 50.f, 50.f, 50.f, 50.f, 50.f, 35.f, 12.f,
                  { 1 },
                  {
                      { MiAmpParams::xvOn, 1.f }, { MiAmpParams::xvLevel, 56.f }, { MiAmpParams::xvDrive, 40.8f },
                      { MiAmpParams::xvLow, 65.4f }, { MiAmpParams::xvHigh, 53.5f }, { MiAmpParams::xvBlend, 57.6f },
                      { MiAmpParams::xvMid, 25.8f }, { MiAmpParams::xvPresence, 80.4f }, { MiAmpParams::xvCab, 1.f }
                  });

            return v;
        }();
        return cat;
    }

    static bool containsWordPhrase (const juce::String& haystackNorm, const juce::String& phraseNorm)
    {
        if (phraseNorm.isEmpty())
            return false;
        const juce::String h = " " + haystackNorm + " ";
        const juce::String p = " " + phraseNorm + " ";
        return h.contains (p);
    }

    static ToneMatchResult searchLocalCatalog (const juce::String& queryRaw, bool forBass, int& outBestScore, bool allowStyleOnly = false)
    {
        const juce::String q = normalizeQuery (queryRaw);
        outBestScore = 0;
        if (q.isEmpty())
            return {};

        juce::StringArray tokens;
        tokens.addTokens (q, " ", "");
        tokens.removeEmptyStrings();

        int explicitDrop = 0;
        const bool wantsDrop = queryRequestsDropTuning (q, explicitDrop);

        const auto& cat = getCatalog();
        int bestIdx = -1;
        int bestScore = 0;

        for (size_t i = 0; i < cat.size(); ++i)
        {
            if (cat[i].result.bassMode != forBass)
                continue;

            int score = 0;

            // 1. Exact artist/song alias match (highest priority, word-boundary safe)
            juce::StringArray aliasList;
            aliasList.addTokens (cat[i].exactAliases, "|", "");
            int aliasWordHits = 0;
            for (const auto& alRaw : aliasList)
            {
                const juce::String al = normalizeQuery (alRaw);
                if (al.length() < 3) continue;
                if (containsWordPhrase (q, al) || (q.length() >= 4 && containsWordPhrase (al, q)))
                {
                    const int wordCount = juce::StringArray::fromTokens (al, " ", "").size();
                    score = std::max (score, 180 + wordCount * 35);
                    ++aliasWordHits;
                }
            }

            // 2. Individual exact-word hits against aliases + title (never partial-substring junk)
            const juce::String aliasHay = normalizeQuery (cat[i].exactAliases.replace ("|", " ") + " " + cat[i].result.title);
            const juce::String styleHay = normalizeQuery (cat[i].styleTags);

            int nonFillerAliasTokens = 0;
            int styleTokens = 0;
            for (const auto& tok : tokens)
            {
                if (tok.length() < 2) continue;
                if (tok == "quiero" || tok == "sonar" || tok == "como" || tok == "esta"
                    || tok == "cancion" || tok == "tema" || tok == "para" || tok == "banda"
                    || tok == "sound" || tok == "like" || tok == "tone" || tok == "song" || tok == "the"
                    || tok == "parecerme" || tok == "estilo" || tok == "tono" || tok == "de"
                    || tok == "el" || tok == "la" || tok == "los" || tok == "las" || tok == "en"
                    || tok == "con" || tok == "sin" || tok == "guitar" || tok == "guitarra"
                    || tok == "bass" || tok == "bajo" || tok == "del" || tok == "and" || tok == "drop")
                    continue;

                if (containsWordPhrase (aliasHay, tok))
                {
                    score += (tok.length() >= 5 ? 55 : 36);
                    ++nonFillerAliasTokens;
                }
                else if (allowStyleOnly && containsWordPhrase (styleHay, tok))
                {
                    score += (tok.length() >= 5 ? 24 : 14);
                    ++styleTokens;
                }
            }

            if (aliasWordHits == 0 && nonFillerAliasTokens == 0 && (! allowStyleOnly || styleTokens == 0))
                continue;

            if (score > bestScore)
            {
                bestScore = score;
                bestIdx = static_cast<int> (i);
            }
        }

        outBestScore = bestScore;
        const int minThreshold = allowStyleOnly ? 28 : 50;
        if (bestIdx >= 0 && bestScore >= minThreshold)
        {
            ToneMatchResult res = cat[static_cast<size_t> (bestIdx)].result;
            if (wantsDrop)
                res.dropSemi = (explicitDrop != 0) ? explicitDrop : res.suggestedDropSemi;
            else
                res.dropSemi = 0;
            return res;
        }

        return {};
    }

    // Maps Wikipedia / MusicBrainz genre text to an honest genre rig without impersonating another band
    static ToneMatchResult mapGenreKeywordsToTone (const juce::String& webText, const juce::String& query, bool forBass, const juce::String& badge)
    {
        const juce::String norm = normalizeQuery (webText);
        int score = 0;
        auto styleRig = searchLocalCatalog (norm, forBass, score, /*allowStyleOnly=*/ true);
        if (styleRig.found && score >= 28)
        {
            const juce::String cleanQ = query.trim().substring (0, 26);
            const juce::String styleLabel = styleRig.title.fromFirstOccurrenceOf (juce::String::fromUTF8 (u8"\u2022 "), false, false);
            styleRig.sourceBadge = badge;
            styleRig.title = cleanQ + (styleLabel.isNotEmpty() ? (" \u2022 " + styleLabel) : "");
            styleRig.explanation = juce::String::fromUTF8 (u8"Aproximaci\u00f3n por estilo web (")
                                 + styleRig.rigSummary + "): " + styleRig.explanation;
            return styleRig;
        }
        return {};
    }

    static juce::String buildAiPrompt (const juce::String& userQuery, bool forBass)
    {
        const auto lang = miamp_i18n::currentLang();
        if (lang != miamp_i18n::Lang::ES)
        {
            const juce::String replyLang = miamp_i18n::langAiPromptName (lang);
            if (forBass)
            {
                return "I have the MI AMP plugin in BASS mode and want to recreate the bass tone of: \""
                    + userQuery.trim()
                    + "\".\n\nReply in " + replyLang + " using ONLY MI AMP BASS gear (values 0-100, EQ -12..+12 dB):\n"
                    "1) HEAD: 0=CL-300, 1=PRO-450, 2=BASSMAN, 3=RB-800, 4=M-900, 5=B-15, 6=OR-200, 7=DB-750, 8=CLASSIC (or PEDALS ONLY)\n"
                    "2) CABINET: 1x15 VINTAGE, 4x10 TOURING, 4x10 HI-PUNCH, 6x10 HEAVY, 8x10 CLASSIC, 8x10 TANGERINE, 8x10 MI CLASSIC\n"
                    "3) HEAD KNOBS: Gain, Bass, Mid, Freq, Treble, Ultra-Hi (On/Off)\n"
                    "4) PEDALS (up to 10): MI-87 COMP, XV PREAMP, MI DEEP CHORUS, MI LIME PHASE, MI STUDIO REVERB, MI BASS DRIVER, MI GOLD DRIVE, BASS FOOD, MI OLIVE FUZZ, MI VIOLET FLANGER, MI PEARL DELAY, MI SILENT GATE, MI POLY OCTAVE, MI BASS EQ (7-Band).";
            }

            return "I have the MI AMP plugin in GUITAR mode and want to recreate the guitar tone of: \""
                + userQuery.trim()
                + "\".\n\nReply in " + replyLang + " using ONLY MI AMP GUITAR gear (values 0-100, EQ -12..+12 dB):\n"
                "1) HEAD: 0=MI-800, 1=PLEXI, 2=RECTO, 3=HG-120, 4=AC-30, 5=ROCKER, 6=BE-100, 7=TWIN, 8=CLASSIC (Channel + Bright)\n"
                "2) CABINET: 4x12 GREENBACK, 4x12 BRIT V30, 4x12 OVERSIZED, 4x12 HI-GAIN, 2x12 CLASS-A, 4x12 TANGERINE, 2x12 BLACKFACE, 4x12 MI CLASSIC\n"
                "3) HEAD KNOBS: Gain, Bass, Mid, Treble, Presence, Master\n"
                "4) PEDALS (up to 10): MI ICE COMP, MI GREEN DRIVE, MI SUSTAIN FUZZ, MI SKY CHORUS, MI PHASER, MI PEARL DELAY, MI STUDIO REVERB, MI ORANGE DIST, MI DUAL CRUNCH, MI VIOLET FLANGER, MI SILENT GATE, MI SUB OCTAVE, MI GRAPHIC EQ.";
        }

        if (forBass)
        {
            return juce::String::fromUTF8 (
                u8"Tengo MI AMP en modo BAJO y quiero lograr el tono de: \"")
                + userQuery.trim()
                + juce::String::fromUTF8 (
                u8"\".\n\nUs\u00e1 EXCLUSIVAMENTE equipamiento de BAJO en MI AMP (valores 0-100):\n"
                u8"1) CABEZAL: CL-300 / PRO-450 / BASSMAN / RB-800 / M-900 / B-15 / OR-200 / DB-750 / CLASSIC (o SOLO PEDALES)\n"
                u8"2) CABINA: 1x15 VINTAGE / 4x10 TOURING / 4x10 HI-PUNCH / 6x10 HEAVY / 8x10 CLASSIC / 8x10 TANGERINE / 8x10 MI CLASSIC\n"
                u8"3) PERILLAS: Gain, Bass, Mid, Freq, Treble, Ultra-Hi\n"
                u8"4) PEDALES (en orden): MI-87 COMP, XV PREAMP, MI DEEP CHORUS, MI LIME PHASE, MI STUDIO REVERB, MI BASS DRIVER, MI GOLD DRIVE, BASS FOOD, MI OLIVE FUZZ, MI VIOLET FLANGER, MI PEARL DELAY, MI SILENT GATE, MI POLY OCTAVE, MI BASS EQ.");
        }

        return juce::String::fromUTF8 (
            u8"Tengo MI AMP en modo GUITARRA y quiero lograr el tono de: \"")
            + userQuery.trim()
            + juce::String::fromUTF8 (
            u8"\".\n\nUs\u00e1 EXCLUSIVAMENTE equipamiento de GUITARRA en MI AMP (valores 0-100):\n"
            u8"1) CABEZAL: MI-800 / PLEXI / RECTO / HG-120 / AC-30 / ROCKER / BE-100 / TWIN / CLASSIC (Canal y Bright)\n"
            u8"2) CABINA: 4x12 GREENBACK / 4x12 BRIT V30 / 4x12 OVERSIZED / 4x12 HI-GAIN / 2x12 CLASS-A / 4x12 TANGERINE / 2x12 BLACKFACE / 4x12 MI CLASSIC\n"
            u8"3) PERILLAS: Gain, Bass, Mid, Treble, Presence, Master\n"
            u8"4) PEDALES (en orden): MI ICE COMP, MI GREEN DRIVE, MI SUSTAIN FUZZ, MI SKY CHORUS, MI PHASER, MI PEARL DELAY, MI STUDIO REVERB, MI ORANGE DIST, MI DUAL CRUNCH, MI VIOLET FLANGER, MI SILENT GATE, MI SUB OCTAVE, MI GRAPHIC EQ.");
    }

    // Free public web lookup (Wikipedia + MusicBrainz, ZERO API keys required)
    static ToneMatchResult searchFreeWeb (const juce::String& query, bool forBass)
    {
        juce::String collectedText;

        try
        {
            const juce::String enc = juce::URL::addEscapeChars (query + " band music guitarist", false);
            juce::URL wikiUrl ("https://en.wikipedia.org/w/api.php?action=query&list=search&srlimit=3&utf8=1&format=json&srsearch=" + enc);
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (5000)
                            .withExtraHeaders ("User-Agent: MiAmpToneFinder/2.0\r\n");
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

        try
        {
            const juce::String encMb = juce::URL::addEscapeChars (query, false);
            juce::URL mbUrl ("https://musicbrainz.org/ws/2/artist/?query=" + encMb + "&limit=2&fmt=json");
            auto opts = juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                            .withConnectionTimeoutMs (5000)
                            .withExtraHeaders ("User-Agent: MiAmpToneFinder/2.0 (contact@popupsaudio.com)\r\n");
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

        return mapGenreKeywordsToTone (collectedText, query, forBass, "Aproximaci\u00f3n por G\u00e9nero (Web)");
    }

    // Optional Google Gemini + Google Search Grounding (full amp head, cab, channel, pedals & knob calibration)
    static ToneMatchResult searchWithGemini (const juce::String& query, bool forBass, const juce::String& apiKey)
    {
        if (apiKey.trim().isEmpty())
            return {};

        try
        {
            const juce::String instStr = forBass ? "BASS" : "GUITAR";
            const juce::String descLang = miamp_i18n::langAiPromptName (miamp_i18n::currentLang());
            const juce::String prompt =
                "You are a pro guitar/bass rig builder. Mode: " + instStr
                + ". Search the web for the real rig of \"" + query.replace ("\"", "'")
                + "\" and map it faithfully to MI AMP JSON (NO markdown fences). Keys:\n"
                  "\"title\" (string), \"explanation\" (short " + descLang + " summary mentioning amp+cab+pedals),\n"
                  "\"headBypass\" (bool, false when using an amp head; true ONLY for direct DI/preamp),\n"
                  "\"headModel\" (int 0..8: "
                + juce::String (forBass
                    ? "0=CL-300 SVT, 1=PRO-450, 2=Bassman, 3=RB-800 GK, 4=M-900 Darkglass, 5=B-15 Motown, 6=OR-200 Orange, 7=DB-750 Aguilar, 8=Classic Mint),\n"
                      "\"cab\" (int 0..6: 0=1x15 Vintage, 1=4x10 Touring, 2=4x10 Hi-Punch, 3=6x10 Heavy, 4=8x10 Classic, 5=8x10 Tangerine, 6=8x10 Classic),\n"
                      "\"ultraHi\" (bool), \"gain\" (0..100), \"bass\" (0..100), \"mid\" (0..100), \"freq\" (0..100), \"treble\" (0..100),\n"
                      "\"slots\" (array of 0..13: 0=MI87Comp,1=XV,2=Chorus,3=Phaser,4=Reverb,5=BassDriver,6=GoldDrive,7=BassFood,8=OliveFuzz,9=Flanger,10=Delay,11=Gate,12=Octaver,13=BassEQ),\n"
                    : "0=MI-800 JCM800, 1=Plexi, 2=Recto Mesa, 3=HG-120 6505, 4=AC-30 Vox, 5=Rocker Orange, 6=BE-100 Friedman, 7=Twin Reverb Fender, 8=Classic Mint),\n"
                      "\"channel\" (int: 0=clean, 1=crunch/top-boost/plexi, 2=lead/800/be100/rocker-dirty), \"bright\" (bool),\n"
                      "\"cab\" (int 0..7: 0=4x12 Greenback, 1=4x12 BritV30, 2=4x12 Oversized, 3=4x12 HiGain, 4=2x12 Class-A, 5=4x12 Tangerine, 6=2x12 Blackface, 7=MI Classic),\n"
                      "\"gain\" (0..100), \"bass\" (0..100), \"mid\" (0..100), \"treble\" (0..100), \"presence\" (0..100), \"master\" (0..100),\n"
                      "\"slots\" (array of 0..12: 0=Comp,1=GreenDrive,2=Fuzz,3=Chorus,4=Phaser,5=Delay,6=Reverb,7=OrangeDist,8=DualCrunch,9=Flanger,10=Gate,11=Octaver,12=GraphicEQ),\n")
                + "\"suggestedDropSemi\" (int -12..0), \"driveGain\" (0..100), \"driveTone\" (0..100), \"chorusMix\" (0..100), \"delayMix\" (0..100), \"reverbMix\" (0..100), \"gateThresh\" (0..100).";

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
                                    r.sourceBadge = miamp_i18n::tr (miamp_i18n::Msg::TfGeminiBadge);
                                    r.title = parsed.getProperty ("title", query).toString();
                                    r.explanation = parsed.getProperty ("explanation", miamp_i18n::tr (miamp_i18n::Msg::TfGeminiFallbackDesc)).toString();
                                    r.bassMode = forBass;
                                    r.headBypass = static_cast<bool> (parsed.getProperty ("headBypass", false));
                                    r.suggestedDropSemi = std::clamp (static_cast<int> (parsed.getProperty ("suggestedDropSemi", 0)), -12, 0);

                                    int explicitDrop = 0;
                                    if (queryRequestsDropTuning (normalizeQuery (query), explicitDrop))
                                        r.dropSemi = (explicitDrop != 0) ? explicitDrop : r.suggestedDropSemi;
                                    else
                                        r.dropSemi = 0;

                                    const int hm = std::clamp (static_cast<int> (parsed.getProperty ("headModel", forBass ? 0 : 2)), 0, 8);
                                    const int cb = std::clamp (static_cast<int> (parsed.getProperty ("cab", forBass ? 4 : 1)), 0, forBass ? 6 : 7);

                                    if (forBass)
                                    {
                                        r.bassHeadModel = hm;
                                        r.bassCabModel  = cb;
                                        r.bassCabOn     = ! r.headBypass;
                                        r.bassUltraHi   = static_cast<bool> (parsed.getProperty ("ultraHi", false));
                                        r.bassGain      = std::clamp (static_cast<float> (parsed.getProperty ("gain", 56.0)), 0.f, 100.f);
                                        r.bassLow       = std::clamp (static_cast<float> (parsed.getProperty ("bass", 58.0)), 0.f, 100.f);
                                        r.bassMid       = std::clamp (static_cast<float> (parsed.getProperty ("mid", 56.0)), 0.f, 100.f);
                                        r.bassFreq      = std::clamp (static_cast<float> (parsed.getProperty ("freq", 52.0)), 0.f, 100.f);
                                        r.bassTreble    = std::clamp (static_cast<float> (parsed.getProperty ("treble", 58.0)), 0.f, 100.f);
                                        r.rigSummary    = r.headBypass ? "SOLO PEDALES" : (bassHeadName (hm) + " \u00b7 " + bassCabName (cb));
                                    }
                                    else
                                    {
                                        r.gtrHeadModel = hm;
                                        r.gtrCab       = cb;
                                        r.gtrChannel   = std::clamp (static_cast<int> (parsed.getProperty ("channel", 1)), 0, 2);
                                        // Respect UI channel bindings for single-channel/specific heads
                                        if (hm == 0 || hm == 6) r.gtrChannel = 2;
                                        else if (hm == 1)       r.gtrChannel = 1;
                                        r.gtrBright    = static_cast<bool> (parsed.getProperty ("bright", true));
                                        r.gtrGain      = std::clamp (static_cast<float> (parsed.getProperty ("gain", 58.0)), 0.f, 100.f);
                                        r.gtrBass      = std::clamp (static_cast<float> (parsed.getProperty ("bass", 54.0)), 0.f, 100.f);
                                        r.gtrMid       = std::clamp (static_cast<float> (parsed.getProperty ("mid", 60.0)), 0.f, 100.f);
                                        r.gtrTreble    = std::clamp (static_cast<float> (parsed.getProperty ("treble", 62.0)), 0.f, 100.f);
                                        r.gtrPresence  = std::clamp (static_cast<float> (parsed.getProperty ("presence", 58.0)), 0.f, 100.f);
                                        r.gtrMaster    = std::clamp (static_cast<float> (parsed.getProperty ("master", 60.0)), 0.f, 100.f);
                                        r.rigSummary   = r.headBypass ? "SOLO PEDALES" : (gtrHeadName (hm) + " \u00b7 " + gtrCabName (cb));
                                    }

                                    if (auto* slArr = parsed.getProperty ("slots", {}).getArray())
                                    {
                                        const int maxT = forBass ? 14 : 13;
                                        for (const auto& sv : *slArr)
                                        {
                                            const int idx = static_cast<int> (sv);
                                            if (idx >= 0 && idx < maxT && r.chainSlots.size() < 10)
                                                r.chainSlots.push_back (idx);
                                        }
                                    }
                                    if (r.chainSlots.empty())
                                        r.chainSlots.push_back (forBass ? 0 : 6);

                                    // Apply pedal knob hints from Gemini JSON so pedals sound calibrated instead of raw defaults
                                    const float dGain = std::clamp (static_cast<float> (parsed.getProperty ("driveGain", 30.0)), 0.f, 100.f);
                                    const float dTone = std::clamp (static_cast<float> (parsed.getProperty ("driveTone", 56.0)), 0.f, 100.f);
                                    const float cMix  = std::clamp (static_cast<float> (parsed.getProperty ("chorusMix", 32.0)), 0.f, 100.f);
                                    const float dlMix = std::clamp (static_cast<float> (parsed.getProperty ("delayMix", 24.0)), 0.f, 100.f);
                                    const float rvMix = std::clamp (static_cast<float> (parsed.getProperty ("reverbMix", 20.0)), 0.f, 100.f);
                                    const float gtThr = std::clamp (static_cast<float> (parsed.getProperty ("gateThresh", 32.0)), 0.f, 100.f);

                                    if (! forBass)
                                    {
                                        r.overrides.push_back ({ MiAmpParams::gtrDriveGain, dGain });
                                        r.overrides.push_back ({ MiAmpParams::gtrDriveTone, dTone });
                                        r.overrides.push_back ({ MiAmpParams::gtrDriveLvl, 58.f });
                                        r.overrides.push_back ({ MiAmpParams::gtrChorusMix, cMix });
                                        r.overrides.push_back ({ MiAmpParams::gtrDelayMix, dlMix });
                                        r.overrides.push_back ({ MiAmpParams::gtrReverbMix, rvMix });
                                        r.overrides.push_back ({ MiAmpParams::gtrGateThresh, gtThr });
                                    }
                                    else
                                    {
                                        r.overrides.push_back ({ MiAmpParams::bddiDrive, dGain });
                                        r.overrides.push_back ({ MiAmpParams::chorusMix, cMix });
                                        r.overrides.push_back ({ MiAmpParams::bassDelayMix, dlMix });
                                        r.overrides.push_back ({ MiAmpParams::reverbMix, rvMix });
                                        r.overrides.push_back ({ MiAmpParams::bassGateThresh, gtThr });
                                    }

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

        // Drop tuner (0 unless user explicitly searched a drop tuning)
        proc.setParamNormalized (MiAmpParams::pitchOn, r.dropSemi != 0 ? 1.0f : 0.0f);
        proc.setParamNormalized (MiAmpParams::pitchSemi, static_cast<float> (r.dropSemi));
        proc.setParamNormalized (MiAmpParams::pitchCents, 0.0f);
        proc.setParamNormalized (MiAmpParams::pitchMix, 100.0f);

        // Calibrate Dual Mics (MI-57 + MI-87) & Room cleanly so no stale mic inversion/mute ruins the tone
        proc.setParamNormalized (MiAmpParams::mic1On,    1.0f);
        proc.setParamNormalized (MiAmpParams::mic1Level, 84.0f);
        proc.setParamNormalized (MiAmpParams::mic1PosX,  50.0f);
        proc.setParamNormalized (MiAmpParams::mic1PosY,  50.0f);
        proc.setParamNormalized (MiAmpParams::mic1Dist,  0.0f);
        proc.setParamNormalized (MiAmpParams::mic1Phase, 0.0f);
        proc.setParamNormalized (MiAmpParams::mic2On,    1.0f);
        proc.setParamNormalized (MiAmpParams::mic2Level, 78.0f);
        proc.setParamNormalized (MiAmpParams::mic2PosX,  48.0f);
        proc.setParamNormalized (MiAmpParams::mic2PosY,  52.0f);
        proc.setParamNormalized (MiAmpParams::mic2Dist,  0.0f);
        proc.setParamNormalized (MiAmpParams::mic2Phase, 0.0f);
        proc.setParamNormalized (MiAmpParams::cabMicMix, std::clamp (r.micMix, 0.0f, 100.0f));
        proc.setParamNormalized (MiAmpParams::cabRoom,   std::clamp (r.cabRoom, 0.0f, 100.0f));

        // Reset post-amp Studio FX so old heavy EQ/comp settings don't distort new search
        proc.setParamNormalized (MiAmpParams::sfxCompOn, 0.0f);
        proc.setParamNormalized (MiAmpParams::sfxEqOn,   0.0f);

        if (r.bassMode)
        {
            proc.setParamNormalized (MiAmpParams::ampPower,      1.0f);
            proc.setParamNormalized (MiAmpParams::ampStandby,    0.0f);
            proc.setParamNormalized (MiAmpParams::bassHeadModel, static_cast<float> (std::clamp (r.bassHeadModel, 0, 8)));
            proc.setParamNormalized (MiAmpParams::bassCabModel,  static_cast<float> (std::clamp (r.bassCabModel, 0, 8)));
            proc.setParamNormalized (MiAmpParams::ampCabOn,      r.bassCabOn ? 1.0f : 0.0f);
            proc.setParamNormalized (MiAmpParams::ampUltraHi,    r.bassUltraHi ? 1.0f : 0.0f);
            proc.setParamNormalized (MiAmpParams::ampGain,       r.bassGain);
            proc.setParamNormalized (MiAmpParams::ampBass,       r.bassLow);
            proc.setParamNormalized (MiAmpParams::ampMid,        r.bassMid);
            proc.setParamNormalized (MiAmpParams::ampFreq,       r.bassFreq);
            proc.setParamNormalized (MiAmpParams::ampTreble,     r.bassTreble);
            proc.setParamNormalized (MiAmpParams::bassMaster,    58.0f);
            proc.setParamNormalized (MiAmpParams::bassPad,       0.0f);
            proc.setParamNormalized (MiAmpParams::bassMuteSw,    0.0f);
            proc.setParamNormalized (MiAmpParams::bassUltraLo,   0.0f);
            proc.setParamNormalized (MiAmpParams::bassBrightSw,  0.0f);
            proc.setParamNormalized (MiAmpParams::bassDeepSw,    0.0f);
            proc.setParamNormalized (MiAmpParams::bassHeadEqOn,  0.0f);
            proc.setParamNormalized (MiAmpParams::bassCompCtrl,  35.f);
            proc.setParamNormalized (MiAmpParams::bassDriveCtrl, 42.f);
            proc.setParamNormalized (MiAmpParams::bassToneCtrl,  55.f);
            proc.setParamNormalized (MiAmpParams::bassBlendCtrl, 50.f);
            proc.setParamNormalized (MiAmpParams::bassTubeGain,  52.f);
        }
        else
        {
            const int hm = std::clamp (r.gtrHeadModel, 0, 8);
            const int ch = std::clamp (r.gtrChannel, 0, 2);
            proc.setParamNormalized (MiAmpParams::gtrPower,      1.0f);
            proc.setParamNormalized (MiAmpParams::gtrStandby,    0.0f);
            proc.setParamNormalized (MiAmpParams::gtrHeadModel,  static_cast<float> (hm));
            proc.setParamNormalized (MiAmpParams::gtrChannel,    static_cast<float> (ch));
            proc.setParamNormalized (MiAmpParams::gtrCab,        static_cast<float> (std::clamp (r.gtrCab, 0, 9)));
            proc.setParamNormalized (MiAmpParams::gtrModeBright, r.gtrBright ? 1.0f : 0.0f);
            proc.setParamNormalized (MiAmpParams::gtrOs4x,       1.0f);
            proc.setParamNormalized (MiAmpParams::gtrInput,      50.0f);
            proc.setParamNormalized (MiAmpParams::gtrGate,       r.gtrGateThresh);
            proc.setParamNormalized (MiAmpParams::gtrOutput,     60.0f);
            proc.setParamNormalized (MiAmpParams::gtrResonance,  50.0f);
            proc.setParamNormalized (MiAmpParams::gtrAmpRev,     0.0f);
            proc.setParamNormalized (MiAmpParams::gtrTremDep,    0.0f);
            proc.setParamNormalized (MiAmpParams::gtrTremSpd,    45.0f);
            proc.setParamNormalized (MiAmpParams::gtrHalfPwr,    0.0f);
            proc.setParamNormalized (MiAmpParams::gtrCrunchSw,   0.0f);
            proc.setParamNormalized (MiAmpParams::gtrBright2,    0.0f);
            proc.setParamNormalized (MiAmpParams::gtrAtten,      50.0f);
            proc.setParamNormalized (MiAmpParams::gtrVoiceSw,    1.0f);
            proc.setParamNormalized (MiAmpParams::gtrCleanBass,  r.gtrBass);
            proc.setParamNormalized (MiAmpParams::gtrCleanTreb,  r.gtrTreble);

            // Set knobs on active channel AND mirror clean/other channels safely so UI matches DSP
            proc.setParamNormalized (MiAmpParams::gtrChGain[ch],     r.gtrGain);
            proc.setParamNormalized (MiAmpParams::gtrChBass[ch],     r.gtrBass);
            proc.setParamNormalized (MiAmpParams::gtrChMid[ch],      r.gtrMid);
            proc.setParamNormalized (MiAmpParams::gtrChTreble[ch],   r.gtrTreble);
            proc.setParamNormalized (MiAmpParams::gtrChPresence[ch], r.gtrPresence);
            proc.setParamNormalized (MiAmpParams::gtrChMaster[ch],   r.gtrMaster);
        }

        proc.setActiveChain (r.bassMode, r.chainSlots);

        // Turn OFF every pedal of the active instrument before applying the ToneFinder overrides
        if (r.bassMode)
        {
            for (const char* pid : { MiAmpParams::compOn, MiAmpParams::xvOn, MiAmpParams::chorusOn,
                                     MiAmpParams::phaserOn, MiAmpParams::reverbOn, MiAmpParams::bddiOn,
                                     MiAmpParams::goldOn, MiAmpParams::bfOn, MiAmpParams::oliveOn,
                                     MiAmpParams::bassFlangeOn, MiAmpParams::bassDelayOn, MiAmpParams::bassGateOn,
                                     MiAmpParams::bassOctOn, MiAmpParams::bassEqOn })
                proc.setParamNormalized (pid, 0.0f);

            // Reset Bass EQ sliders to flat 50% before applying overrides
            for (int b = 0; b < 7; ++b)
                proc.setParamNormalized (MiAmpParams::bassEqBand[b], 50.0f);
        }
        else
        {
            for (const char* pid : { MiAmpParams::gtrCompOn, MiAmpParams::gtrDriveOn, MiAmpParams::gtrFuzzOn,
                                     MiAmpParams::gtrChorusOn, MiAmpParams::gtrPhaserOn, MiAmpParams::gtrDelayOn,
                                     MiAmpParams::gtrReverbOn, MiAmpParams::gtrDistOn, MiAmpParams::gtrDualOn,
                                     MiAmpParams::gtrFlangeOn, MiAmpParams::gtrGateOn, MiAmpParams::gtrOctOn,
                                     MiAmpParams::gtrEqOn })
                proc.setParamNormalized (pid, 0.0f);

            // Reset Guitar EQ sliders to flat 50% before applying overrides
            for (int b = 0; b < 7; ++b)
                proc.setParamNormalized (MiAmpParams::gtrEqBand[b], 50.0f);
        }

        for (const auto& ov : r.overrides)
            if (ov.paramId != nullptr)
                proc.setParamNormalized (ov.paramId, ov.value);

        // If a slot has no explicit *On override (e.g. from Gemini JSON), enable it cleanly
        for (int pt : r.chainSlots)
        {
            const char* onId = nullptr;
            if (r.bassMode)
            {
                static constexpr const char* kBOn[14] = {
                    MiAmpParams::compOn, MiAmpParams::xvOn, MiAmpParams::chorusOn, MiAmpParams::phaserOn,
                    MiAmpParams::reverbOn, MiAmpParams::bddiOn, MiAmpParams::goldOn, MiAmpParams::bfOn,
                    MiAmpParams::oliveOn, MiAmpParams::bassFlangeOn, MiAmpParams::bassDelayOn,
                    MiAmpParams::bassGateOn, MiAmpParams::bassOctOn, MiAmpParams::bassEqOn
                };
                if (pt >= 0 && pt < 14) onId = kBOn[pt];
            }
            else
            {
                static constexpr const char* kGOn[13] = {
                    MiAmpParams::gtrCompOn, MiAmpParams::gtrDriveOn, MiAmpParams::gtrFuzzOn, MiAmpParams::gtrChorusOn,
                    MiAmpParams::gtrPhaserOn, MiAmpParams::gtrDelayOn, MiAmpParams::gtrReverbOn, MiAmpParams::gtrDistOn,
                    MiAmpParams::gtrDualOn, MiAmpParams::gtrFlangeOn, MiAmpParams::gtrGateOn, MiAmpParams::gtrOctOn,
                    MiAmpParams::gtrEqOn
                };
                if (pt >= 0 && pt < 13) onId = kGOn[pt];
            }
            if (onId != nullptr)
            {
                const bool hasExplicit = std::any_of (r.overrides.begin(), r.overrides.end(),
                    [onId](const ParamOverride& o) { return o.paramId != nullptr && std::strcmp (o.paramId, onId) == 0; });
                if (! hasExplicit)
                    proc.setParamNormalized (onId, 1.0f);
            }
        }

        proc.setActivePresetName (r.bassMode, r.title);
    }
};

} // namespace mipedals
