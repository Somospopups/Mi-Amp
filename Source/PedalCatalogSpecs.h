#pragma once
#include "PluginProcessor.h"
#include <array>

namespace mipedalui
{

struct KnobSpec
{
    float cx = 0.5f, cy = 0.2f, diamW = 0.2f;
    const char* paramId = nullptr;
    const char* label = "";
};

struct ToggleSpec
{
    float cx = 0.5f, cy = 0.3f;
    const char* paramId = nullptr;
    const char* label = "";
    bool threeWay = false;
    const char* opt0 = "OFF";
    const char* opt1 = "ON";
    const char* opt2 = "";
};

struct EqSpec
{
    bool hasEq = false;
    float yTop = 0.1575f, yBot = 0.425f;
    std::array<float, 7> xs {};
    std::array<const char*, 7> paramIds {};
    std::array<const char*, 7> bandLabels {};
};

struct PedalVisualSpec
{
    int artKey = 0;       // 0..21 into Images::pedBodies/pedKnobs, -1 = XV Preamp
    int pedalType = 0;    // zenbox::GuitarPedalType or miamp::PedalType
    const char* name = "";
    const char* shortName = "";
    const char* bypassParamId = nullptr;
    int natW = 480, natH = 800;
    bool isWide = false;

    int numKnobs = 0;
    std::array<KnobSpec, 7> knobs {};

    int numToggles = 0;
    std::array<ToggleSpec, 3> toggles {};

    EqSpec eq {};

    // Bypass LED position & bezel diameter normalized to chassis width
    float ledCx = 0.5f, ledCy = 0.5f, ledDiamW = 0.08f;

    // Footswitch hit area in editor
    float swX = 0.20f, swY = 0.58f, swW = 0.60f, swH = 0.36f;

    // Optional 10-LED GR meter (MI-87 COMP)
    bool hasGrMeter = false;
    std::array<std::array<float, 2>, 10> grLeds {};
};

// Palette order matching the 12-effect catalog + Reverb (and BDDI + XV)
inline constexpr int kGuitarPaletteOrder[13] = { 7, 1, 8, 2, 0, 4, 3, 9, 5, 10, 11, 12, 6 };
inline constexpr int kBassPaletteOrder[14]   = { 6, 7, 5, 1, 8, 0, 3, 2, 9, 10, 11, 12, 13, 4 };

inline const PedalVisualSpec& getGuitarPedalSpec (int type) noexcept
{
    static const std::array<PedalVisualSpec, 13> specs = []()
    {
        std::array<PedalVisualSpec, 13> a {};

        // 0: Compresor -> MI ICE COMP (artKey=4, 470x800)
        {
            auto& s = a[0];
            s.artKey = 4; s.pedalType = 0; s.name = "MI Ice Comp"; s.shortName = "ICE COMP";
            s.bypassParamId = MiAmpParams::gtrCompOn; s.natW = 470; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.28723f, 0.11125f, 0.20426f, MiAmpParams::gtrCompLvl,  "LEVEL" };
            s.knobs[1] = { 0.71915f, 0.10875f, 0.20426f, MiAmpParams::gtrCompTone, "TONE" };
            s.knobs[2] = { 0.27872f, 0.32375f, 0.20426f, MiAmpParams::gtrCompAtt,  "ATTACK" };
            s.knobs[3] = { 0.71915f, 0.31625f, 0.20426f, MiAmpParams::gtrCompSus,  "SUSTAIN" };
            s.ledCx = 0.50000f; s.ledCy = 0.33750f; s.ledDiamW = 0.07660f;
        }

        // 1: Drive -> MI GREEN DRIVE (artKey=1, 454x800)
        {
            auto& s = a[1];
            s.artKey = 1; s.pedalType = 1; s.name = "MI Green Drive"; s.shortName = "GREEN DRV";
            s.bypassParamId = MiAmpParams::gtrDriveOn; s.natW = 454; s.natH = 800;
            s.numKnobs = 3;
            s.knobs[0] = { 0.49119f, 0.13625f, 0.22026f, MiAmpParams::gtrDriveTone, "TONE" };
            s.knobs[1] = { 0.24890f, 0.27375f, 0.22026f, MiAmpParams::gtrDriveGain, "DRIVE" };
            s.knobs[2] = { 0.74229f, 0.27375f, 0.22026f, MiAmpParams::gtrDriveLvl,  "LEVEL" };
            s.numToggles = 1;
            s.toggles[0] = { 0.48436f, 0.31850f, MiAmpParams::gtrDriveClip, "CLIPPING", false, "SYM", "ASYM", "" };
            s.ledCx = 0.49559f; s.ledCy = 0.64750f; s.ledDiamW = 0.11013f;
        }

        // 2: Fuzz -> MI SUSTAIN (artKey=3, 800x555, wide)
        {
            auto& s = a[2];
            s.artKey = 3; s.pedalType = 2; s.name = "MI Sustain"; s.shortName = "SUSTAIN";
            s.bypassParamId = MiAmpParams::gtrFuzzOn; s.natW = 800; s.natH = 555; s.isWide = true;
            s.numKnobs = 3;
            s.knobs[0] = { 0.20875f, 0.17658f, 0.13000f, MiAmpParams::gtrFuzzLvl,  "VOLUME" };
            s.knobs[1] = { 0.50125f, 0.17838f, 0.13000f, MiAmpParams::gtrFuzzTone, "TONE" };
            s.knobs[2] = { 0.79000f, 0.16937f, 0.13000f, MiAmpParams::gtrFuzzFuzz, "SUSTAIN" };
            s.ledCx = 0.35375f; s.ledCy = 0.81441f; s.ledDiamW = 0.05500f;
            s.swX = 0.38f; s.swY = 0.64f; s.swW = 0.36f; s.swH = 0.32f;
        }

        // 3: Chorus -> MI SKY CHORUS (artKey=6, 513x800)
        {
            auto& s = a[3];
            s.artKey = 6; s.pedalType = 3; s.name = "MI Sky Chorus"; s.shortName = "SKY CHOR";
            s.bypassParamId = MiAmpParams::gtrChorusOn; s.natW = 513; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.25146f, 0.13625f, 0.19493f, MiAmpParams::gtrChorusMix,   "LEVEL" };
            s.knobs[1] = { 0.73489f, 0.13625f, 0.19493f, MiAmpParams::gtrChorusEq,    "EQ" };
            s.knobs[2] = { 0.25341f, 0.35625f, 0.19493f, MiAmpParams::gtrChorusRate,  "RATE" };
            s.knobs[3] = { 0.73099f, 0.35750f, 0.19493f, MiAmpParams::gtrChorusDepth, "DEPTH" };
            s.numToggles = 1;
            s.toggles[0] = { 0.49556f, 0.41500f, MiAmpParams::gtrChorusMode, "MODE", true, "STEREO", "DUAL", "VIBRATO" };
            s.ledCx = 0.49903f; s.ledCy = 0.20000f; s.ledDiamW = 0.07797f;
        }

        // 4: Phaser -> MI PHASER (artKey=5, 324x800)
        {
            auto& s = a[4];
            s.artKey = 5; s.pedalType = 4; s.name = "MI Phaser"; s.shortName = "PHASER";
            s.bypassParamId = MiAmpParams::gtrPhaserOn; s.natW = 324; s.natH = 800;
            s.numKnobs = 1;
            s.knobs[0] = { 0.53086f, 0.16875f, 0.46914f, MiAmpParams::gtrPhaserRate, "SPEED" };
            s.ledCx = 0.50617f; s.ledCy = 0.44875f; s.ledDiamW = 0.15432f;
        }

        // 5: Delay -> MI PEARL (artKey=8, 488x800)
        {
            auto& s = a[5];
            s.artKey = 8; s.pedalType = 5; s.name = "MI Pearl"; s.shortName = "PEARL DLY";
            s.bypassParamId = MiAmpParams::gtrDelayOn; s.natW = 488; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.23156f, 0.10750f, 0.19262f, MiAmpParams::gtrDelayMix,  "LEVEL" };
            s.knobs[1] = { 0.76434f, 0.10875f, 0.19262f, MiAmpParams::gtrDelayFb,   "FEEDBACK" };
            s.knobs[2] = { 0.22541f, 0.35750f, 0.19262f, MiAmpParams::gtrDelayTime, "TIME" };
            s.knobs[3] = { 0.75820f, 0.37250f, 0.19262f, MiAmpParams::gtrDelayMode, "MODE" };
            s.numToggles = 1;
            s.toggles[0] = { 0.49600f, 0.38000f, MiAmpParams::gtrDelaySubdiv, "SUBDIV", false, "1/4", "DOT 1/8", "" };
            s.ledCx = 0.49180f; s.ledCy = 0.70000f; s.ledDiamW = 0.08607f;
        }

        // 6: Reverb -> MI STUDIO (artKey=12, 471x800)
        {
            auto& s = a[6];
            s.artKey = 12; s.pedalType = 6; s.name = "MI Studio"; s.shortName = "STUDIO RVB";
            s.bypassParamId = MiAmpParams::gtrReverbOn; s.natW = 471; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.27813f, 0.10375f, 0.20382f, MiAmpParams::gtrReverbMix,  "LEVEL" };
            s.knobs[1] = { 0.72187f, 0.10375f, 0.20382f, MiAmpParams::gtrReverbTone, "TONE" };
            s.knobs[2] = { 0.27389f, 0.32000f, 0.20382f, MiAmpParams::gtrReverbDec,  "TIME" };
            s.knobs[3] = { 0.71762f, 0.32750f, 0.20382f, MiAmpParams::gtrReverbMode, "MODE" };
            s.ledCx = 0.49894f; s.ledCy = 0.50375f; s.ledDiamW = 0.09342f;
        }

        // 7: Distortion -> MI ORANGE (artKey=0, 498x800)
        {
            auto& s = a[7];
            s.artKey = 0; s.pedalType = 7; s.name = "MI Orange"; s.shortName = "ORANGE";
            s.bypassParamId = MiAmpParams::gtrDistOn; s.natW = 498; s.natH = 800;
            s.numKnobs = 3;
            s.knobs[0] = { 0.22691f, 0.11000f, 0.22490f, MiAmpParams::gtrDistTone,  "TONE" };
            s.knobs[1] = { 0.75703f, 0.12250f, 0.22490f, MiAmpParams::gtrDistLevel, "LEVEL" };
            s.knobs[2] = { 0.50201f, 0.31125f, 0.22490f, MiAmpParams::gtrDistDrive, "DIST" };
            s.ledCx = 0.49598f; s.ledCy = 0.10750f; s.ledDiamW = 0.10843f;
        }

        // 8: DualCrunch -> MI DUAL CRUNCH (artKey=2, 507x800)
        {
            auto& s = a[8];
            s.artKey = 2; s.pedalType = 8; s.name = "MI Dual Crunch"; s.shortName = "DUAL CR";
            s.bypassParamId = MiAmpParams::gtrDualOn; s.natW = 507; s.natH = 800;
            s.numKnobs = 5;
            s.knobs[0] = { 0.19724f, 0.10125f, 0.17751f, MiAmpParams::gtrDualOutput, "OUTPUT" };
            s.knobs[1] = { 0.49507f, 0.10125f, 0.17751f, MiAmpParams::gtrDualGain,   "GAIN" };
            s.knobs[2] = { 0.78304f, 0.10125f, 0.17751f, MiAmpParams::gtrDualBlend,  "BLEND" };
            s.knobs[3] = { 0.27416f, 0.29250f, 0.17751f, MiAmpParams::gtrDualTone,   "TONE" };
            s.knobs[4] = { 0.70217f, 0.29250f, 0.17751f, MiAmpParams::gtrDualDrive,  "DRIVE" };
            s.numToggles = 1;
            s.toggles[0] = { 0.49700f, 0.30200f, MiAmpParams::gtrDualScoop, "SCOOP", false, "FLAT", "SCOOP", "" };
            s.ledCx = 0.49704f; s.ledCy = 0.59125f; s.ledDiamW = 0.09862f;
        }

        // 9: Flanger -> MI VIOLET (artKey=7, 495x800)
        {
            auto& s = a[9];
            s.artKey = 7; s.pedalType = 9; s.name = "MI Violet"; s.shortName = "VIOLET";
            s.bypassParamId = MiAmpParams::gtrFlangeOn; s.natW = 495; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.27475f, 0.12250f, 0.20606f, MiAmpParams::gtrFlangeManual, "MANUAL" };
            s.knobs[1] = { 0.72121f, 0.11750f, 0.20606f, MiAmpParams::gtrFlangeRes,    "RES" };
            s.knobs[2] = { 0.27475f, 0.33500f, 0.20606f, MiAmpParams::gtrFlangeDepth,  "DEPTH" };
            s.knobs[3] = { 0.72121f, 0.33500f, 0.20606f, MiAmpParams::gtrFlangeRate,   "RATE" };
            s.numToggles = 1;
            s.toggles[0] = { 0.50192f, 0.38200f, MiAmpParams::gtrFlangeMode, "MODE", true, "CLASSIC", "JET", "SLOW" };
            s.ledCx = 0.50101f; s.ledCy = 0.49500f; s.ledDiamW = 0.08081f;
        }

        // 10: NoiseGate -> MI SILENT (artKey=9, 467x800)
        {
            auto& s = a[10];
            s.artKey = 9; s.pedalType = 10; s.name = "MI Silent"; s.shortName = "SILENT";
            s.bypassParamId = MiAmpParams::gtrGateOn; s.natW = 467; s.natH = 800;
            s.numKnobs = 3;
            s.knobs[0] = { 0.25054f, 0.14750f, 0.22698f, MiAmpParams::gtrGateThresh, "THRESH" };
            s.knobs[1] = { 0.75161f, 0.14750f, 0.22698f, MiAmpParams::gtrGateDecay,  "DECAY" };
            s.knobs[2] = { 0.49893f, 0.34125f, 0.22698f, MiAmpParams::gtrGateMode,   "MODE" };
            s.ledCx = 0.50107f; s.ledCy = 0.17125f; s.ledDiamW = 0.10707f;
        }

        // 11: Octaver -> MI SUB OCTAVE (artKey=10, 479x800)
        {
            auto& s = a[11];
            s.artKey = 10; s.pedalType = 11; s.name = "MI Sub Octave"; s.shortName = "SUB OCT";
            s.bypassParamId = MiAmpParams::gtrOctOn; s.natW = 479; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.27766f, 0.13125f, 0.24217f, MiAmpParams::gtrOctDirect, "DIRECT" };
            s.knobs[1] = { 0.72025f, 0.13625f, 0.24217f, MiAmpParams::gtrOctUp,     "+1 OCT" };
            s.knobs[2] = { 0.27766f, 0.37250f, 0.24217f, MiAmpParams::gtrOctSub1,   "-1 OCT" };
            s.knobs[3] = { 0.71816f, 0.37000f, 0.24217f, MiAmpParams::gtrOctSub2,   "-2 OCT" };
            s.ledCx = 0.50104f; s.ledCy = 0.66250f; s.ledDiamW = 0.07516f;
        }

        // 12: GraphicEq -> MI GRAPHIC EQ (artKey=11, 477x800)
        {
            auto& s = a[12];
            s.artKey = 11; s.pedalType = 12; s.name = "MI Graphic EQ"; s.shortName = "GTR EQ";
            s.bypassParamId = MiAmpParams::gtrEqOn; s.natW = 477; s.natH = 800;
            s.eq.hasEq = true; s.eq.yTop = 0.1450f; s.eq.yBot = 0.4350f;
            s.eq.xs = { 0.1460f, 0.2650f, 0.3840f, 0.5010f, 0.6240f, 0.7440f, 0.8630f };
            for (int b = 0; b < 7; ++b) s.eq.paramIds[b] = MiAmpParams::gtrEqBand[b];
            s.eq.bandLabels = { "100Hz", "200Hz", "400Hz", "800Hz", "1.6kHz", "3.2kHz", "LEVEL" };
            s.ledCx = 0.50105f; s.ledCy = 0.72375f; s.ledDiamW = 0.09224f;
        }

        return a;
    }();
    return specs[static_cast<size_t> (std::clamp (type, 0, 12))];
}

inline const PedalVisualSpec& getBassPedalSpec (int type) noexcept
{
    static const std::array<PedalVisualSpec, 14> specs = []()
    {
        std::array<PedalVisualSpec, 14> a {};

        // 0: Compresor -> MI-87 COMP (artKey=17, 474x800, 10 GR LEDs)
        {
            auto& s = a[0];
            s.artKey = 17; s.pedalType = 0; s.name = "MI-87 Comp"; s.shortName = "MI-87";
            s.bypassParamId = MiAmpParams::compOn; s.natW = 474; s.natH = 800;
            s.numKnobs = 5;
            s.knobs[0] = { 0.22574f, 0.11250f, 0.15612f, MiAmpParams::compRelease, "RELEASE" };
            s.knobs[1] = { 0.50000f, 0.11250f, 0.15612f, MiAmpParams::compSustain, "RATIO" };
            s.knobs[2] = { 0.77848f, 0.11250f, 0.15612f, MiAmpParams::compAttack,  "ATTACK" };
            s.knobs[3] = { 0.30169f, 0.38375f, 0.15612f, MiAmpParams::compLevel,   "OUTPUT" };
            s.knobs[4] = { 0.68987f, 0.38375f, 0.15612f, MiAmpParams::compInput,   "INPUT" };
            s.ledCx = 0.49578f; s.ledCy = 0.73125f; s.ledDiamW = 0.08439f;
            s.hasGrMeter = true;
            s.grLeds = {{
                {{0.16101f, 0.26491f}}, {{0.23683f, 0.26478f}}, {{0.31306f, 0.26470f}},
                {{0.38780f, 0.26499f}}, {{0.46282f, 0.26475f}}, {{0.53823f, 0.26476f}},
                {{0.61315f, 0.26484f}}, {{0.68836f, 0.26468f}}, {{0.76365f, 0.26485f}},
                {{0.83896f, 0.26486f}}
            }};
        }

        // 1: XV Preamp (artKey=-1, 1391x1131, wide)
        {
            auto& s = a[1];
            s.artKey = -1; s.pedalType = 1; s.name = "XV Preamp"; s.shortName = "XV PREAMP";
            s.bypassParamId = MiAmpParams::xvOn; s.natW = 1391; s.natH = 1131; s.isWide = true;
            s.numKnobs = 7;
            s.knobs[0] = { 0.19482f, 0.196f, 0.116f, MiAmpParams::xvLevel,    "LEVEL" };
            s.knobs[1] = { 0.35226f, 0.196f, 0.116f, MiAmpParams::xvDrive,    "DRIVE" };
            s.knobs[2] = { 0.50683f, 0.196f, 0.116f, MiAmpParams::xvLow,      "LOW" };
            s.knobs[3] = { 0.66139f, 0.196f, 0.116f, MiAmpParams::xvHigh,     "HIGH" };
            s.knobs[4] = { 0.81668f, 0.196f, 0.116f, MiAmpParams::xvBlend,    "BLEND" };
            s.knobs[5] = { 0.43422f, 0.407f, 0.116f, MiAmpParams::xvMid,      "MID" };
            s.knobs[6] = { 0.59526f, 0.407f, 0.116f, MiAmpParams::xvPresence, "PRESENCE" };
            s.swX = 0.10f; s.swY = 0.62f; s.swW = 0.20f; s.swH = 0.24f;
        }

        // 2: Chorus -> MI DEEP CHORUS (artKey=19, 520x800)
        {
            auto& s = a[2];
            s.artKey = 19; s.pedalType = 2; s.name = "MI Deep Chorus"; s.shortName = "DEEP CH";
            s.bypassParamId = MiAmpParams::chorusOn; s.natW = 520; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.26538f, 0.11750f, 0.18462f, MiAmpParams::chorusMix,   "LEVEL" };
            s.knobs[1] = { 0.72308f, 0.11750f, 0.18462f, MiAmpParams::chorusLoCut, "LO-CUT" };
            s.knobs[2] = { 0.26538f, 0.35375f, 0.18462f, MiAmpParams::chorusRate,  "RATE" };
            s.knobs[3] = { 0.72308f, 0.35375f, 0.18462f, MiAmpParams::chorusDepth, "DEPTH" };
            s.numToggles = 1;
            s.toggles[0] = { 0.49852f, 0.34975f, MiAmpParams::chorusXOver, "X-OVER", false, "FULL", "X-OVER", "" };
            s.ledCx = 0.49808f; s.ledCy = 0.72500f; s.ledDiamW = 0.08462f;
        }

        // 3: Phaser -> MI LIME PHASE (artKey=18, 484x800)
        {
            auto& s = a[3];
            s.artKey = 18; s.pedalType = 3; s.name = "MI Lime Phase"; s.shortName = "LIME PH";
            s.bypassParamId = MiAmpParams::phaserOn; s.natW = 484; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.28926f, 0.12625f, 0.19008f, MiAmpParams::phaserRate,     "RATE" };
            s.knobs[1] = { 0.72521f, 0.12125f, 0.19008f, MiAmpParams::phaserDepth,    "DEPTH" };
            s.knobs[2] = { 0.26860f, 0.35125f, 0.19008f, MiAmpParams::phaserFeedback, "RES" };
            s.knobs[3] = { 0.72521f, 0.35125f, 0.19008f, MiAmpParams::phaserStage,    "STAGE" };
            s.ledCx = 0.50413f; s.ledCy = 0.31250f; s.ledDiamW = 0.09917f;
        }

        // 4: Reverb -> MI STUDIO (artKey=12, 471x800)
        {
            auto& s = a[4];
            s.artKey = 12; s.pedalType = 4; s.name = "MI Studio"; s.shortName = "STUDIO RVB";
            s.bypassParamId = MiAmpParams::reverbOn; s.natW = 471; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.27813f, 0.10375f, 0.20382f, MiAmpParams::reverbMix,   "LEVEL" };
            s.knobs[1] = { 0.72187f, 0.10375f, 0.20382f, MiAmpParams::reverbTone,  "TONE" };
            s.knobs[2] = { 0.27389f, 0.32000f, 0.20382f, MiAmpParams::reverbDecay, "TIME" };
            s.knobs[3] = { 0.71762f, 0.32750f, 0.20382f, MiAmpParams::reverbMode,  "MODE" };
            s.ledCx = 0.49894f; s.ledCy = 0.50375f; s.ledDiamW = 0.09342f;
        }

        // 5: BassDriver -> MI BASS DRIVER (artKey=15, 800x771, wide, 6 knobs + 3 toggles)
        {
            auto& s = a[5];
            s.artKey = 15; s.pedalType = 5; s.name = "MI Bass Driver"; s.shortName = "BASS DRV";
            s.bypassParamId = MiAmpParams::bddiOn; s.natW = 800; s.natH = 771; s.isWide = true;
            s.numKnobs = 6;
            s.knobs[0] = { 0.22125f, 0.12192f, 0.10500f, MiAmpParams::bddiLevel,    "LEVEL" };
            s.knobs[1] = { 0.50750f, 0.12192f, 0.10500f, MiAmpParams::bddiBlend,    "BLEND" };
            s.knobs[2] = { 0.78250f, 0.12192f, 0.10500f, MiAmpParams::bddiTreble,   "TREBLE" };
            s.knobs[3] = { 0.21875f, 0.34371f, 0.10500f, MiAmpParams::bddiPresence, "PRESENCE" };
            s.knobs[4] = { 0.51250f, 0.34371f, 0.10500f, MiAmpParams::bddiDrive,    "DRIVE" };
            s.knobs[5] = { 0.78250f, 0.34371f, 0.10500f, MiAmpParams::bddiBass,     "BASS" };
            s.numToggles = 3;
            s.toggles[0] = { 0.22800f, 0.56200f, MiAmpParams::bddiXlrPad,   "XLR -20dB",   false, "0dB", "-20dB", "" };
            s.toggles[1] = { 0.50000f, 0.56200f, MiAmpParams::bddiInBoost,  "INPUT +10dB", false, "0dB", "+10dB", "" };
            s.toggles[2] = { 0.77200f, 0.56200f, MiAmpParams::bddiOutBoost, "OUTPUT +10dB",false, "0dB", "+10dB", "" };
            s.ledCx = 0.51125f; s.ledCy = 0.74449f; s.ledDiamW = 0.06000f;
            s.swX = 0.35f; s.swY = 0.70f; s.swW = 0.30f; s.swH = 0.26f;
        }

        // 6: GoldDrive -> MI GOLD (artKey=13, 475x800)
        {
            auto& s = a[6];
            s.artKey = 13; s.pedalType = 6; s.name = "MI Gold"; s.shortName = "MI GOLD";
            s.bypassParamId = MiAmpParams::goldOn; s.natW = 475; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.25474f, 0.10625f, 0.19368f, MiAmpParams::goldLevel, "LEVEL" };
            s.knobs[1] = { 0.74947f, 0.10625f, 0.19368f, MiAmpParams::goldEq,    "EQ" };
            s.knobs[2] = { 0.25474f, 0.30625f, 0.19368f, MiAmpParams::goldBlend, "BLEND" };
            s.knobs[3] = { 0.74105f, 0.30875f, 0.19368f, MiAmpParams::goldGain,  "GAIN" };
            s.ledCx = 0.50316f; s.ledCy = 0.15500f; s.ledDiamW = 0.10105f;
        }

        // 7: BassFood -> BASS FOOD (artKey=14, 378x800)
        {
            auto& s = a[7];
            s.artKey = 14; s.pedalType = 7; s.name = "Bass Food"; s.shortName = "BASS FOOD";
            s.bypassParamId = MiAmpParams::bfOn; s.natW = 378; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.23545f, 0.11125f, 0.24339f, MiAmpParams::bfVol,    "VOL" };
            s.knobs[1] = { 0.75397f, 0.11500f, 0.24339f, MiAmpParams::bfTreble, "TREBLE" };
            s.knobs[2] = { 0.20899f, 0.29125f, 0.24339f, MiAmpParams::bfDrive,  "DRIVE" };
            s.knobs[3] = { 0.75661f, 0.29125f, 0.24339f, MiAmpParams::bfBlend,  "BLEND" };
            s.numToggles = 1;
            s.toggles[0] = { 0.50096f, 0.19500f, MiAmpParams::bfPad, "PAD", false, "0dB", "-10dB", "" };
            s.ledCx = 0.50000f; s.ledCy = 0.69375f; s.ledDiamW = 0.09524f;
        }

        // 8: OliveFuzz -> MI OLIVE (artKey=16, 529x800)
        {
            auto& s = a[8];
            s.artKey = 16; s.pedalType = 8; s.name = "MI Olive"; s.shortName = "MI OLIVE";
            s.bypassParamId = MiAmpParams::oliveOn; s.natW = 529; s.natH = 800;
            s.numKnobs = 3;
            s.knobs[0] = { 0.18904f, 0.11000f, 0.17769f, MiAmpParams::oliveVol,     "VOLUME" };
            s.knobs[1] = { 0.49716f, 0.11500f, 0.17769f, MiAmpParams::oliveTone,    "TONE" };
            s.knobs[2] = { 0.79962f, 0.11375f, 0.17769f, MiAmpParams::oliveSustain, "SUSTAIN" };
            s.numToggles = 1;
            s.toggles[0] = { 0.49705f, 0.30013f, MiAmpParams::oliveMode, "MODE", true, "DRY", "NORM", "BOOST" };
            s.ledCx = 0.50095f; s.ledCy = 0.69250f; s.ledDiamW = 0.06805f;
        }

        // 9: Flanger -> MI VIOLET (artKey=7, 495x800)
        {
            auto& s = a[9];
            s.artKey = 7; s.pedalType = 9; s.name = "MI Violet"; s.shortName = "VIOLET";
            s.bypassParamId = MiAmpParams::bassFlangeOn; s.natW = 495; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.27475f, 0.12250f, 0.20606f, MiAmpParams::bassFlangeManual, "MANUAL" };
            s.knobs[1] = { 0.72121f, 0.11750f, 0.20606f, MiAmpParams::bassFlangeRes,    "RES" };
            s.knobs[2] = { 0.27475f, 0.33500f, 0.20606f, MiAmpParams::bassFlangeDepth,  "DEPTH" };
            s.knobs[3] = { 0.72121f, 0.33500f, 0.20606f, MiAmpParams::bassFlangeRate,   "RATE" };
            s.numToggles = 1;
            s.toggles[0] = { 0.50192f, 0.38200f, MiAmpParams::bassFlangeMode, "MODE", true, "CLASSIC", "JET", "SLOW" };
            s.ledCx = 0.50101f; s.ledCy = 0.49500f; s.ledDiamW = 0.08081f;
        }

        // 10: Delay -> MI PEARL (artKey=8, 488x800)
        {
            auto& s = a[10];
            s.artKey = 8; s.pedalType = 10; s.name = "MI Pearl"; s.shortName = "PEARL DLY";
            s.bypassParamId = MiAmpParams::bassDelayOn; s.natW = 488; s.natH = 800;
            s.numKnobs = 4;
            s.knobs[0] = { 0.23156f, 0.10750f, 0.19262f, MiAmpParams::bassDelayMix,  "LEVEL" };
            s.knobs[1] = { 0.76434f, 0.10875f, 0.19262f, MiAmpParams::bassDelayFb,   "FEEDBACK" };
            s.knobs[2] = { 0.22541f, 0.35750f, 0.19262f, MiAmpParams::bassDelayTime, "TIME" };
            s.knobs[3] = { 0.75820f, 0.37250f, 0.19262f, MiAmpParams::bassDelayMode, "MODE" };
            s.numToggles = 1;
            s.toggles[0] = { 0.49600f, 0.38000f, MiAmpParams::bassDelaySubdiv, "SUBDIV", false, "1/4", "DOT 1/8", "" };
            s.ledCx = 0.49180f; s.ledCy = 0.70000f; s.ledDiamW = 0.08607f;
        }

        // 11: NoiseGate -> MI SILENT (artKey=9, 467x800)
        {
            auto& s = a[11];
            s.artKey = 9; s.pedalType = 11; s.name = "MI Silent"; s.shortName = "SILENT";
            s.bypassParamId = MiAmpParams::bassGateOn; s.natW = 467; s.natH = 800;
            s.numKnobs = 3;
            s.knobs[0] = { 0.25054f, 0.14750f, 0.22698f, MiAmpParams::bassGateThresh, "THRESH" };
            s.knobs[1] = { 0.75161f, 0.14750f, 0.22698f, MiAmpParams::bassGateDecay,  "DECAY" };
            s.knobs[2] = { 0.49893f, 0.34125f, 0.22698f, MiAmpParams::bassGateMode,   "MODE" };
            s.ledCx = 0.50107f; s.ledCy = 0.17125f; s.ledDiamW = 0.10707f;
        }

        // 12: Octaver -> MI POLY OCTAVE (artKey=20, 442x800)
        {
            auto& s = a[12];
            s.artKey = 20; s.pedalType = 12; s.name = "MI Poly Octave"; s.shortName = "POLY OCT";
            s.bypassParamId = MiAmpParams::bassOctOn; s.natW = 442; s.natH = 800;
            s.numKnobs = 3;
            s.knobs[0] = { 0.19231f, 0.17125f, 0.22172f, MiAmpParams::bassOctDry, "DRY" };
            s.knobs[1] = { 0.49548f, 0.17000f, 0.22172f, MiAmpParams::bassOctSub, "SUB" };
            s.knobs[2] = { 0.79638f, 0.16875f, 0.22172f, MiAmpParams::bassOctUp,  "OCT UP" };
            s.ledCx = 0.50000f; s.ledCy = 0.34750f; s.ledDiamW = 0.09050f;
        }

        // 13: BassEq -> MI BASS EQ (artKey=21, 527x800)
        {
            auto& s = a[13];
            s.artKey = 21; s.pedalType = 13; s.name = "MI Bass EQ"; s.shortName = "BASS EQ";
            s.bypassParamId = MiAmpParams::bassEqOn; s.natW = 527; s.natH = 800;
            s.eq.hasEq = true; s.eq.yTop = 0.1450f; s.eq.yBot = 0.4400f;
            s.eq.xs = { 0.1560f, 0.2700f, 0.3860f, 0.5020f, 0.6180f, 0.7340f, 0.8500f };
            for (int b = 0; b < 7; ++b) s.eq.paramIds[b] = MiAmpParams::bassEqBand[b];
            s.eq.bandLabels = { "50Hz", "120Hz", "400Hz", "500Hz", "800Hz", "4.5kHz", "LEVEL" };
            s.ledCx = 0.50474f; s.ledCy = 0.72500f; s.ledDiamW = 0.08349f;
        }

        return a;
    }();
    return specs[static_cast<size_t> (std::clamp (type, 0, 13))];
}

inline const PedalVisualSpec& getPedalSpec (bool bassMode, int type) noexcept
{
    return bassMode ? getBassPedalSpec (type) : getGuitarPedalSpec (type);
}

} // namespace mipedalui
