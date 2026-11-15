#include "PluginProcessor.h"
#include "PluginEditor.h"

MiAmpAudioProcessor::MiAmpAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "MI_AMP_BASS_STATE", createParameterLayout())
{
    factoryPresets = getFactoryPresets();
    loadUserPresetsFromDisk();
}

MiAmpAudioProcessor::~MiAmpAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout MiAmpAudioProcessor::createParameterLayout()
{
    using namespace MiAmpParams;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto pctRange = [] { return juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f); };

    auto pid = [] (const char* id) { return juce::ParameterID { id, 1 }; };

    // Bass Head
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (ampPower),   "Power",      true));
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (ampStandby), "Standby",    false));
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (ampUltraHi), "Ultra Mode", false));
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (ampCabOn),   "Caja 8x10",  true));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (ampGain),    "Gain",   pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (ampBass),    "Bass",   pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (ampMid),     "Mid",    pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (ampFreq),    "Freq",   pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (ampTreble),  "Treble", pctRange(), 50.0f));

    // Drop Pitch Shifter & Tuner
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (pitchOn),    "Drop Pitch", false));
    p.push_back (std::make_unique<juce::AudioParameterInt>   (pid (pitchSemi),  "Semitonos",  -12, 12, 0));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (pitchCents), "Cents", juce::NormalisableRange<float> (-50.0f, 50.0f, 1.0f), 0.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (pitchMix),   "Drop Mix", pctRange(), 100.0f));
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (tunerMuted), "Tuner Mute", false));

    // Pedalboard Slots (-1 = Empty, 0 = Compresor, 1 = XV, 2 = Chorus, 3 = Phaser, 4 = Reverb)
    p.push_back (std::make_unique<juce::AudioParameterInt> (pid (slot0), "Slot 1", -1, 4, 1));
    p.push_back (std::make_unique<juce::AudioParameterInt> (pid (slot1), "Slot 2", -1, 4, -1));
    p.push_back (std::make_unique<juce::AudioParameterInt> (pid (slot2), "Slot 3", -1, 4, -1));
    p.push_back (std::make_unique<juce::AudioParameterInt> (pid (slot3), "Slot 4", -1, 4, -1));
    p.push_back (std::make_unique<juce::AudioParameterInt> (pid (slot4), "Slot 5", -1, 4, -1));

    // Compresor
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (compOn),      "Compresor", true));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (compSustain), "Comp Sustain", pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (compAttack),  "Comp Attack",  pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (compLevel),   "Comp Level",   pctRange(), 50.0f));

    // XV Bass Pedal
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (xvOn),       "XV Bass",     true));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (xvLevel),    "XV Level",    pctRange(), 56.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (xvDrive),    "XV Drive",    pctRange(), 40.8f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (xvLow),      "XV Low",      pctRange(), 65.4f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (xvHigh),     "XV High",     pctRange(), 53.5f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (xvBlend),    "XV Blend",    pctRange(), 57.6f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (xvMid),      "XV Mid",      pctRange(), 25.8f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (xvPresence), "XV Presence", pctRange(), 80.4f));
    p.push_back (std::make_unique<juce::AudioParameterInt>   (pid (xvCab),      "XV Cab",      0, 2, 1));

    // Chorus
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (chorusOn),    "Chorus", true));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (chorusRate),  "Chorus Rate",  pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (chorusDepth), "Chorus Depth", pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (chorusMix),   "Chorus Mix",   pctRange(), 50.0f));

    // Phaser
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (phaserOn),       "Phaser", true));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (phaserRate),     "Phaser Rate",     pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (phaserDepth),    "Phaser Depth",    pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (phaserFeedback), "Phaser Feedback", pctRange(), 50.0f));

    // Reverb
    p.push_back (std::make_unique<juce::AudioParameterBool>  (pid (reverbOn),    "Reverb", true));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (reverbDecay), "Reverb Decay", pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (reverbTone),  "Reverb Tone",  pctRange(), 50.0f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (pid (reverbMix),   "Reverb Mix",   pctRange(), 50.0f));

    return { p.begin(), p.end() };
}

void MiAmpAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    bassDsp.prepare (sampleRate, samplesPerBlock, 2);
    tuner.prepare (sampleRate);
    monoScratch.assign (static_cast<size_t> (std::max (256, samplesPerBlock)), 0.0f);
}

void MiAmpAudioProcessor::releaseResources()
{
    bassDsp.reset();
    tuner.reset();
}

bool MiAmpAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    if (in != juce::AudioChannelSet::mono() && in != juce::AudioChannelSet::stereo() && ! in.isDisabled())
        return false;
    return true;
}

miamp::BassControlSnapshot MiAmpAudioProcessor::readSnapshot() const noexcept
{
    using namespace MiAmpParams;
    auto val = [this] (const char* id) -> float { return apvts.getRawParameterValue (id)->load(); };

    miamp::BassControlSnapshot s;
    s.ampPower   = val (ampPower)   > 0.5f;
    s.ampStandby = val (ampStandby) > 0.5f;
    s.ampUltraHi = val (ampUltraHi) > 0.5f;
    s.ampCabOn   = val (ampCabOn)   > 0.5f;
    s.ampGain    = val (ampGain);
    s.ampBass    = val (ampBass);
    s.ampMid     = val (ampMid);
    s.ampFreq    = val (ampFreq);
    s.ampTreble  = val (ampTreble);

    s.pitchEnabled   = val (pitchOn) > 0.5f;
    s.pitchSemitones = static_cast<int> (std::lround (val (pitchSemi)));
    s.pitchCents     = val (pitchCents);
    s.pitchMix       = val (pitchMix);
    s.tunerMuted     = val (tunerMuted) > 0.5f;

    s.chainSlots[0] = static_cast<int> (std::lround (val (slot0)));
    s.chainSlots[1] = static_cast<int> (std::lround (val (slot1)));
    s.chainSlots[2] = static_cast<int> (std::lround (val (slot2)));
    s.chainSlots[3] = static_cast<int> (std::lround (val (slot3)));
    s.chainSlots[4] = static_cast<int> (std::lround (val (slot4)));

    s.compOn      = val (compOn) > 0.5f;
    s.compSustain = val (compSustain);
    s.compAttack  = val (compAttack);
    s.compLevel   = val (compLevel);

    s.xvOn       = val (xvOn) > 0.5f;
    s.xvLevel    = val (xvLevel);
    s.xvDrive    = val (xvDrive);
    s.xvLow      = val (xvLow);
    s.xvHigh     = val (xvHigh);
    s.xvBlend    = val (xvBlend);
    s.xvMid      = val (xvMid);
    s.xvPresence = val (xvPresence);
    s.xvCab      = static_cast<int> (std::lround (val (xvCab)));

    s.chorusOn    = val (chorusOn) > 0.5f;
    s.chorusRate  = val (chorusRate);
    s.chorusDepth = val (chorusDepth);
    s.chorusMix   = val (chorusMix);

    s.phaserOn       = val (phaserOn) > 0.5f;
    s.phaserRate     = val (phaserRate);
    s.phaserDepth    = val (phaserDepth);
    s.phaserFeedback = val (phaserFeedback);

    s.reverbOn    = val (reverbOn) > 0.5f;
    s.reverbDecay = val (reverbDecay);
    s.reverbTone  = val (reverbTone);
    s.reverbMix   = val (reverbMix);

    return s;
}

void MiAmpAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples <= 0 || totalOut <= 0)
        return;

    const auto snap = readSnapshot();
    const float* inL = (totalIn > 0) ? buffer.getReadPointer (0) : nullptr;
    const float* inR = (totalIn > 1) ? buffer.getReadPointer (1) : inL;

    double sumInSq = 0.0;
    if (inL != nullptr)
    {
        if (static_cast<int> (monoScratch.size()) < numSamples)
            monoScratch.resize (static_cast<size_t> (numSamples), 0.0f);

        for (int i = 0; i < numSamples; ++i)
        {
            const float m = (inR != nullptr && inR != inL) ? 0.5f * (inL[i] + inR[i]) : inL[i];
            monoScratch[static_cast<size_t> (i)] = m;
            sumInSq += static_cast<double> (m) * m;
        }
        tuner.pushSamples (monoScratch.data(), numSamples);
    }

    bassDsp.process (buffer, snap);

    double sumOutSq = 0.0;
    bool clipped = false;
    const float* outL = buffer.getReadPointer (0);
    const float* outR = (totalOut > 1) ? buffer.getReadPointer (1) : outL;
    for (int i = 0; i < numSamples; ++i)
    {
        const float l = outL[i];
        const float r = outR[i];
        if (std::fabs (l) >= 0.99f || std::fabs (r) >= 0.99f)
            clipped = true;
        sumOutSq += 0.5 * (static_cast<double> (l) * l + static_cast<double> (r) * r);
    }

    const float rmsIn  = static_cast<float> (std::sqrt (sumInSq / numSamples));
    const float rmsOut = static_cast<float> (std::sqrt (sumOutSq / numSamples));
    inputRms.store  (0.75f * inputRms.load()  + 0.25f * rmsIn,  std::memory_order_relaxed);
    outputRms.store (0.75f * outputRms.load() + 0.25f * rmsOut, std::memory_order_relaxed);
    if (clipped) isClipping.store (true, std::memory_order_relaxed);
}

std::vector<int> MiAmpAudioProcessor::getActiveChain() const
{
    using namespace MiAmpParams;
    const char* ids[5] = { slot0, slot1, slot2, slot3, slot4 };
    std::vector<int> out;
    bool seen[5] = { false, false, false, false, false };
    for (int i = 0; i < 5; ++i)
    {
        int t = static_cast<int> (std::lround (apvts.getRawParameterValue (ids[i])->load()));
        if (t >= 0 && t <= 4 && ! seen[t])
        {
            seen[t] = true;
            out.push_back (t);
        }
    }
    return out;
}

void MiAmpAudioProcessor::setActiveChain (const std::vector<int>& order)
{
    using namespace MiAmpParams;
    const char* ids[5] = { slot0, slot1, slot2, slot3, slot4 };
    for (int i = 0; i < 5; ++i)
    {
        const int v = (i < static_cast<int> (order.size())) ? order[static_cast<size_t> (i)] : -1;
        setParamNormalized (ids[i], static_cast<float> (v));
    }
}

bool MiAmpAudioProcessor::addPedalToChain (int pedalType)
{
    if (pedalType < 0 || pedalType > 4) return false;
    auto chain = getActiveChain();
    if (std::find (chain.begin(), chain.end(), pedalType) != chain.end())
        return false;
    if (chain.size() >= 5) return false;
    chain.push_back (pedalType);
    setActiveChain (chain);
    return true;
}

void MiAmpAudioProcessor::removePedalFromChain (int pedalType)
{
    auto chain = getActiveChain();
    chain.erase (std::remove (chain.begin(), chain.end(), pedalType), chain.end());
    setActiveChain (chain);
}

void MiAmpAudioProcessor::movePedalInChain (int pedalType, int delta)
{
    auto chain = getActiveChain();
    auto it = std::find (chain.begin(), chain.end(), pedalType);
    if (it == chain.end()) return;
    int idx = static_cast<int> (std::distance (chain.begin(), it));
    int nxt = idx + delta;
    if (nxt < 0 || nxt >= static_cast<int> (chain.size())) return;
    std::swap (chain[static_cast<size_t> (idx)], chain[static_cast<size_t> (nxt)]);
    setActiveChain (chain);
}

void MiAmpAudioProcessor::setParamNormalized (const char* id, float plainVal)
{
    if (auto* param = apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (plainVal));
}

std::vector<MiAmpBassPreset> MiAmpAudioProcessor::getFactoryPresets()
{
    std::vector<MiAmpBassPreset> list;

    auto mkHead = [] (juce::String n, float g, float b, float m, float f, float t, bool ultra,
                      std::array<int, 5> slots, bool xvOn, int xvCab, juce::String hint)
    {
        MiAmpBassPreset p;
        p.name = std::move (n);
        p.ampGain = g; p.ampBass = b; p.ampMid = m; p.ampFreq = f; p.ampTreble = t;
        p.ampUltraHi = ultra;
        p.slots = slots;
        p.xvOn = xvOn; p.xvCab = xvCab;
        p.hint = std::move (hint);
        return p;
    };

    list.push_back (mkHead ("Inicial (XV Pop-Punk)", 50, 50, 50, 50, 50, false, { 1, -1, -1, -1, -1 }, true, 1, "Cabezal neutro + XV Bass Punk"));
    list.push_back (mkHead ("Limpio (8x10 Valvular)", 32, 56, 52, 46, 52, false, { 0, -1, -1, -1, -1 }, false, 2, "Compresor suave + Cabezal 8x10 puro"));
    list.push_back (mkHead ("Crunch Clásico", 64, 58, 48, 42, 62, true, { 0, 1, -1, -1, -1 }, true, 0, "Comp + XV Bass Mode + Ultra-Hi"));
    list.push_back (mkHead ("Punk Completo", 68, 52, 60, 62, 66, true, { 0, 1, 2, -1, -1 }, true, 1, "Comp + XV Punk + Chorus sutil"));

    auto mkXv = [] (juce::String n, float lvl, float drv, float lowDb, float hiDb, float blend, float midDb, float pres, int cab, juce::String hint)
    {
        MiAmpBassPreset p;
        p.name = std::move (n);
        p.isXvFactory = true;
        p.slots = { 1, -1, -1, -1, -1 };
        p.xvOn = true;
        p.xvLvl = lvl; p.xvDrv = drv;
        p.xvLow = dbToXvKnob (lowDb);
        p.xvHigh = dbToXvKnob (hiDb);
        p.xvBlend = blend;
        p.xvMid = dbToXvKnob (midDb);
        p.xvPres = pres;
        p.xvCab = cab;
        p.hint = std::move (hint);
        return p;
    };

    list.push_back (mkXv ("XV • Pop-Punk (Default)", 56.0f, 40.8f, +3.7f, +0.85f, 57.6f, -5.8f, 80.4f, 1, "Ataque P-Bass definido con caja 4x10 Punk"));
    list.push_back (mkXv ("XV • Mark Hoppus Bi-Amp", 68.0f, 34.0f, +4.0f, +2.0f, 58.0f, +1.0f, 68.0f, 1, "Sub limpio + SVT Punk con medios mordidos"));
    list.push_back (mkXv ("XV • SVT Classic Crunch", 64.0f, 42.0f, +2.5f, +1.0f, 70.0f, -2.0f, 55.0f, 0, "Calidez de válvulas 8x10 vintage"));
    list.push_back (mkXv ("XV • Modern Djent / Metal", 72.0f, 65.0f, +3.0f, +5.5f, 68.0f, -6.5f, 82.0f, 1, "Bajos comprimidos + clank metálico"));
    list.push_back (mkXv ("XV • Warm Vintage Motown", 62.0f, 12.0f, +4.5f, -4.5f, 30.0f, +3.5f, 22.0f, 0, "Redondo, dulce y sin estridencias"));
    list.push_back (mkXv ("XV • Fat Clean DI (8x10)", 70.0f, 15.0f, +3.0f, +1.5f, 35.0f, 0.0f, 48.0f, 2, "Salida limpia DI con cabezal 8x10 activo"));
    list.push_back (mkXv ("XV • Slap Funk", 66.0f, 22.0f, +5.0f, +6.0f, 45.0f, -5.5f, 74.0f, 0, "Scoop de medios con graves y brillo"));
    list.push_back (mkXv ("XV • Stoner / Fuzz Growl", 68.0f, 82.0f, +5.5f, -1.0f, 85.0f, +4.0f, 44.0f, 1, "Saturación gruesa y rugido analógico"));
    list.push_back (mkXv ("XV • Pick Rock Drive", 65.0f, 50.0f, +2.0f, +3.5f, 72.0f, +2.5f, 66.0f, 1, "Púa presente ideal para mezcla de rock"));
    list.push_back (mkXv ("XV • Sub Dub Deep", 72.0f, 8.0f, +7.0f, -7.5f, 20.0f, +1.0f, 15.0f, 0, "Subgrave profundo y oscuro"));
    list.push_back (mkXv ("XV • Studio Bypass / Flat", 50.0f, 0.0f, 0.0f, 0.0f, 50.0f, 0.0f, 50.0f, 2, "Señal neutra de referencia"));

    return list;
}

void MiAmpAudioProcessor::applyPreset (const MiAmpBassPreset& p)
{
    using namespace MiAmpParams;
    activePresetName = p.name;

    setParamNormalized (ampGain, p.ampGain);
    setParamNormalized (ampBass, p.ampBass);
    setParamNormalized (ampMid, p.ampMid);
    setParamNormalized (ampFreq, p.ampFreq);
    setParamNormalized (ampTreble, p.ampTreble);
    setParamNormalized (ampUltraHi, p.ampUltraHi ? 1.0f : 0.0f);

    const char* sIds[5] = { slot0, slot1, slot2, slot3, slot4 };
    for (int i = 0; i < 5; ++i)
        setParamNormalized (sIds[i], static_cast<float> (p.slots[static_cast<size_t> (i)]));

    setParamNormalized (compOn, p.compOn ? 1.0f : 0.0f);
    setParamNormalized (compSustain, p.compSus);
    setParamNormalized (compAttack, p.compAtt);
    setParamNormalized (compLevel, p.compLvl);

    setParamNormalized (xvOn, p.xvOn ? 1.0f : 0.0f);
    setParamNormalized (xvLevel, p.xvLvl);
    setParamNormalized (xvDrive, p.xvDrv);
    setParamNormalized (xvLow, p.xvLow);
    setParamNormalized (xvHigh, p.xvHigh);
    setParamNormalized (xvBlend, p.xvBlend);
    setParamNormalized (xvMid, p.xvMid);
    setParamNormalized (xvPresence, p.xvPres);
    setParamNormalized (xvCab, static_cast<float> (p.xvCab));

    setParamNormalized (chorusOn, p.chorusOn ? 1.0f : 0.0f);
    setParamNormalized (chorusRate, p.chRate);
    setParamNormalized (chorusDepth, p.chDepth);
    setParamNormalized (chorusMix, p.chMix);

    setParamNormalized (phaserOn, p.phaserOn ? 1.0f : 0.0f);
    setParamNormalized (phaserRate, p.phRate);
    setParamNormalized (phaserDepth, p.phDepth);
    setParamNormalized (phaserFeedback, p.phFb);

    setParamNormalized (reverbOn, p.reverbOn ? 1.0f : 0.0f);
    setParamNormalized (reverbDecay, p.rvDec);
    setParamNormalized (reverbTone, p.rvTone);
    setParamNormalized (reverbMix, p.rvMix);
}

int MiAmpAudioProcessor::getNumPrograms()
{
    return static_cast<int> (factoryPresets.size());
}

void MiAmpAudioProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < static_cast<int> (factoryPresets.size()))
    {
        currentProgramIndex = index;
        applyPreset (factoryPresets[static_cast<size_t> (index)]);
    }
}

const juce::String MiAmpAudioProcessor::getProgramName (int index)
{
    if (index >= 0 && index < static_cast<int> (factoryPresets.size()))
        return factoryPresets[static_cast<size_t> (index)].name;
    return {};
}

juce::File MiAmpAudioProcessor::getUserPresetsFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Popups Audio")
                   .getChildFile ("MI AMP");
    dir.createDirectory();
    return dir.getChildFile ("user_presets.xml");
}

void MiAmpAudioProcessor::loadUserPresetsFromDisk()
{
    userPresets.clear();
    auto file = getUserPresetsFile();
    if (! file.existsAsFile()) return;
    if (auto xml = juce::XmlDocument::parse (file))
    {
        for (auto* child : xml->getChildIterator())
        {
            if (! child->hasTagName ("PRESET")) continue;
            MiAmpBassPreset p;
            p.name = child->getStringAttribute ("name", "Custom");
            p.ampGain = (float) child->getDoubleAttribute ("ampGain", 50.0);
            p.ampBass = (float) child->getDoubleAttribute ("ampBass", 50.0);
            p.ampMid  = (float) child->getDoubleAttribute ("ampMid", 50.0);
            p.ampFreq = (float) child->getDoubleAttribute ("ampFreq", 50.0);
            p.ampTreble = (float) child->getDoubleAttribute ("ampTreble", 50.0);
            p.ampUltraHi = child->getBoolAttribute ("ampUltraHi", false);
            for (int i = 0; i < 5; ++i)
                p.slots[static_cast<size_t> (i)] = child->getIntAttribute ("slot" + juce::String (i), (i == 0 ? 1 : -1));
            p.compOn  = child->getBoolAttribute ("compOn", true);
            p.compSus = (float) child->getDoubleAttribute ("compSus", 50.0);
            p.compAtt = (float) child->getDoubleAttribute ("compAtt", 50.0);
            p.compLvl = (float) child->getDoubleAttribute ("compLvl", 50.0);
            p.xvOn    = child->getBoolAttribute ("xvOn", true);
            p.xvLvl   = (float) child->getDoubleAttribute ("xvLvl", 56.0);
            p.xvDrv   = (float) child->getDoubleAttribute ("xvDrv", 40.8);
            p.xvLow   = (float) child->getDoubleAttribute ("xvLow", 65.4);
            p.xvHigh  = (float) child->getDoubleAttribute ("xvHigh", 53.5);
            p.xvBlend = (float) child->getDoubleAttribute ("xvBlend", 57.6);
            p.xvMid   = (float) child->getDoubleAttribute ("xvMid", 25.8);
            p.xvPres  = (float) child->getDoubleAttribute ("xvPres", 80.4);
            p.xvCab   = child->getIntAttribute ("xvCab", 1);
            p.chorusOn = child->getBoolAttribute ("chorusOn", true);
            p.chRate  = (float) child->getDoubleAttribute ("chRate", 50.0);
            p.chDepth = (float) child->getDoubleAttribute ("chDepth", 50.0);
            p.chMix   = (float) child->getDoubleAttribute ("chMix", 50.0);
            p.phaserOn = child->getBoolAttribute ("phaserOn", true);
            p.phRate  = (float) child->getDoubleAttribute ("phRate", 50.0);
            p.phDepth = (float) child->getDoubleAttribute ("phDepth", 50.0);
            p.phFb    = (float) child->getDoubleAttribute ("phFb", 50.0);
            p.reverbOn = child->getBoolAttribute ("reverbOn", true);
            p.rvDec   = (float) child->getDoubleAttribute ("rvDec", 50.0);
            p.rvTone  = (float) child->getDoubleAttribute ("rvTone", 50.0);
            p.rvMix   = (float) child->getDoubleAttribute ("rvMix", 50.0);
            userPresets.push_back (p);
        }
    }
}

void MiAmpAudioProcessor::saveUserPresetsToDisk() const
{
    juce::XmlElement root ("MI_AMP_USER_PRESETS");
    for (const auto& p : userPresets)
    {
        auto* e = root.createNewChildElement ("PRESET");
        e->setAttribute ("name", p.name);
        e->setAttribute ("ampGain", p.ampGain); e->setAttribute ("ampBass", p.ampBass);
        e->setAttribute ("ampMid", p.ampMid);   e->setAttribute ("ampFreq", p.ampFreq);
        e->setAttribute ("ampTreble", p.ampTreble); e->setAttribute ("ampUltraHi", p.ampUltraHi);
        for (int i = 0; i < 5; ++i) e->setAttribute ("slot" + juce::String (i), p.slots[static_cast<size_t> (i)]);
        e->setAttribute ("compOn", p.compOn); e->setAttribute ("compSus", p.compSus);
        e->setAttribute ("compAtt", p.compAtt); e->setAttribute ("compLvl", p.compLvl);
        e->setAttribute ("xvOn", p.xvOn); e->setAttribute ("xvLvl", p.xvLvl);
        e->setAttribute ("xvDrv", p.xvDrv); e->setAttribute ("xvLow", p.xvLow);
        e->setAttribute ("xvHigh", p.xvHigh); e->setAttribute ("xvBlend", p.xvBlend);
        e->setAttribute ("xvMid", p.xvMid); e->setAttribute ("xvPres", p.xvPres);
        e->setAttribute ("xvCab", p.xvCab);
        e->setAttribute ("chorusOn", p.chorusOn); e->setAttribute ("chRate", p.chRate);
        e->setAttribute ("chDepth", p.chDepth); e->setAttribute ("chMix", p.chMix);
        e->setAttribute ("phaserOn", p.phaserOn); e->setAttribute ("phRate", p.phRate);
        e->setAttribute ("phDepth", p.phDepth); e->setAttribute ("phFb", p.phFb);
        e->setAttribute ("reverbOn", p.reverbOn); e->setAttribute ("rvDec", p.rvDec);
        e->setAttribute ("rvTone", p.rvTone); e->setAttribute ("rvMix", p.rvMix);
    }
    root.writeTo (getUserPresetsFile());
}

void MiAmpAudioProcessor::saveUserPreset (const juce::String& name)
{
    const auto s = readSnapshot();
    MiAmpBassPreset p;
    p.name = name.trim().isNotEmpty() ? name.trim() : ("Mi Preset " + juce::String (userPresets.size() + 1));
    p.ampGain = s.ampGain; p.ampBass = s.ampBass; p.ampMid = s.ampMid; p.ampFreq = s.ampFreq; p.ampTreble = s.ampTreble;
    p.ampUltraHi = s.ampUltraHi;
    p.slots = s.chainSlots;
    p.compOn = s.compOn; p.compSus = s.compSustain; p.compAtt = s.compAttack; p.compLvl = s.compLevel;
    p.xvOn = s.xvOn; p.xvLvl = s.xvLevel; p.xvDrv = s.xvDrive; p.xvLow = s.xvLow;
    p.xvHigh = s.xvHigh; p.xvBlend = s.xvBlend; p.xvMid = s.xvMid; p.xvPres = s.xvPresence; p.xvCab = s.xvCab;
    p.chorusOn = s.chorusOn; p.chRate = s.chorusRate; p.chDepth = s.chorusDepth; p.chMix = s.chorusMix;
    p.phaserOn = s.phaserOn; p.phRate = s.phaserRate; p.phDepth = s.phaserDepth; p.phFb = s.phaserFeedback;
    p.reverbOn = s.reverbOn; p.rvDec = s.reverbDecay; p.rvTone = s.reverbTone; p.rvMix = s.reverbMix;

    userPresets.push_back (p);
    activePresetName = p.name;
    saveUserPresetsToDisk();
}

void MiAmpAudioProcessor::deleteUserPreset (int index)
{
    if (index >= 0 && index < static_cast<int> (userPresets.size()))
    {
        userPresets.erase (userPresets.begin() + index);
        saveUserPresetsToDisk();
    }
}

void MiAmpAudioProcessor::loadUserPreset (int index)
{
    if (index >= 0 && index < static_cast<int> (userPresets.size()))
        applyPreset (userPresets[static_cast<size_t> (index)]);
}

void MiAmpAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    xml->setAttribute ("activePresetName", activePresetName);
    copyXmlToBinary (*xml, destData);
}

void MiAmpAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));
    if (xmlState != nullptr && xmlState->hasTagName (apvts.state.getType()))
    {
        activePresetName = xmlState->getStringAttribute ("activePresetName", "XV");
        apvts.replaceState (juce::ValueTree::fromXml (*xmlState));
    }
}

juce::AudioProcessorEditor* MiAmpAudioProcessor::createEditor()
{
    return new MiAmpAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MiAmpAudioProcessor();
}
