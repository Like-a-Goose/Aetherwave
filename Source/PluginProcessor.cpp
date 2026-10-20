#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray waveNames { "Saw", "Square", "Triangle", "Sine" };
// New modes are appended so saved projects keep their mode. The minilogue xd reaches them in
// banks of four (MULTI TYPE picks the bank, VCO1 OCTAVE the mode), so keep groups of four.
const juce::StringArray modeNames { "Tunnel", "Reflect", "Resonant Tunnel", "Resonant Reflect",
                                    "Soft Tunnel", "Step", "Quantum Well", "Crystal",
                                    "Band Gap", "Particle in a Box", "Harmonic Oscillator", "Hydrogen" };
const juce::StringArray lfoWaveNames { "Square", "Triangle", "Saw" };
const juce::StringArray lfoTargetNames { "Barrier", "Mix", "Pitch" };
const juce::StringArray ccMapNames { "Off", "minilogue xd" };

juce::AudioParameterFloatAttributes shown (int decimals, const juce::String& unit = {})
{
    return juce::AudioParameterFloatAttributes()
        .withLabel (unit)
        .withStringFromValueFunction ([decimals, unit] (float v, int) { return juce::String (v, decimals) + (unit.isEmpty() ? "" : " " + unit); });
}

juce::NormalisableRange<float> timeRange (float maxSeconds)
{
    juce::NormalisableRange<float> range (0.001f, maxSeconds);
    range.setSkewForCentre (0.25f);
    return range;
}
} // namespace

QuantumSynthProcessor::QuantumSynthProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "QuantumSynth", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout QuantumSynthProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "osc1Wave", 1 }, "Osc 1 Wave", waveNames, 0));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "osc2Wave", 1 }, "Osc 2 Wave", waveNames, 1));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "osc2Semi", 1 }, "Osc 2 Semitones", -24, 24, 0));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "osc2Fine", 1 }, "Osc 2 Fine",
                                                        NormalisableRange<float> (-50.0f, 50.0f), 7.0f,
                                                        shown (1, "ct")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "oscMix", 1 }, "Osc Mix",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.5f, shown (2)));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "filterMode", 1 }, "Filter Mode", modeNames, 1));

    NormalisableRange<float> barrierRange (20.0f, 18000.0f);
    barrierRange.setSkewForCentre (800.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "barrier", 1 }, "Barrier Height", barrierRange, 600.0f,
                                                        shown (0, "Hz")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "width", 1 }, "Barrier Width",
                                                        NormalisableRange<float> (0.3f, 8.0f), 3.0f, shown (2)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gap", 1 }, "Barrier Gap",
                                                        NormalisableRange<float> (0.5f, 20.0f), 6.0f, shown (2)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "envAmount", 1 }, "Filter Env Amount",
                                                        NormalisableRange<float> (-6.0f, 6.0f), 3.0f,
                                                        shown (2, "oct")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "keyTrack", 1 }, "Key Tracking",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.5f, shown (2)));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampA", 1 }, "Amp Attack", timeRange (5.0f), 0.005f, shown (3, "s")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampD", 1 }, "Amp Decay", timeRange (5.0f), 0.3f, shown (3, "s")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampS", 1 }, "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.8f, shown (2)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "ampR", 1 }, "Amp Release", timeRange (5.0f), 0.4f, shown (3, "s")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "filtA", 1 }, "Filter Attack", timeRange (5.0f), 0.01f, shown (3, "s")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "filtD", 1 }, "Filter Decay", timeRange (5.0f), 0.5f, shown (3, "s")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "filtS", 1 }, "Filter Sustain", NormalisableRange<float> (0.0f, 1.0f), 0.2f, shown (2)));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "filtR", 1 }, "Filter Release", timeRange (5.0f), 0.5f, shown (3, "s")));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "lfoWave", 1 }, "LFO Wave", lfoWaveNames, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "lfoTarget", 1 }, "LFO Target", lfoTargetNames, 0));
    NormalisableRange<float> lfoRateRange (0.05f, 20.0f);
    lfoRateRange.setSkewForCentre (2.0f);
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lfoRate", 1 }, "LFO Rate", lfoRateRange, 4.0f, shown (2, "Hz")));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "lfoAmount", 1 }, "LFO Amount",
                                                        NormalisableRange<float> (0.0f, 1.0f), 0.0f, shown (2)));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "ccMap", 1 }, "CC Map", ccMapNames, 1));
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { "bendRange", 1 }, "Bend Range", 0, 12, 2));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "gain", 1 }, "Output Gain",
                                                        NormalisableRange<float> (-36.0f, 6.0f), -6.0f,
                                                        shown (1, "dB")));

    return { p.begin(), p.end() };
}

qs::FilterSettings QuantumSynthProcessor::getBaseFilterSettings() const
{
    auto value = [this] (const char* id) { return (double) parameters.getRawParameterValue (id)->load(); };

    qs::FilterSettings f;
    f.mode = (qs::FilterMode) (int) value ("filterMode");
    f.barrierHz = value ("barrier");
    f.width = value ("width");
    f.gap = value ("gap");
    return f;
}

qs::SynthSettings QuantumSynthProcessor::readSettings() const
{
    auto value = [this] (const char* id) { return parameters.getRawParameterValue (id)->load(); };

    qs::SynthSettings s;
    s.osc1Wave = (qs::Waveform) (int) value ("osc1Wave");
    s.osc2Wave = (qs::Waveform) (int) value ("osc2Wave");
    s.osc2Semitones = value ("osc2Semi");
    s.osc2Cents = value ("osc2Fine");
    s.oscMix = value ("oscMix");
    s.filter = getBaseFilterSettings();
    s.envAmountOctaves = value ("envAmount");
    s.keyTrack = value ("keyTrack");
    s.ampAttack = value ("ampA");
    s.ampDecay = value ("ampD");
    s.ampSustain = value ("ampS");
    s.ampRelease = value ("ampR");
    s.filterAttack = value ("filtA");
    s.filterDecay = value ("filtD");
    s.filterSustain = value ("filtS");
    s.filterRelease = value ("filtR");
    s.lfoWave = (qs::LfoWave) (int) value ("lfoWave");
    s.lfoTarget = (qs::LfoTarget) (int) value ("lfoTarget");
    s.lfoRate = value ("lfoRate");
    s.lfoAmount = value ("lfoAmount");
    s.bendRange = value ("bendRange");
    s.outputGainDb = value ("gain");
    return s;
}

void QuantumSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setSettings (readSettings());
    engine.resetControllers();
    xdDecoder.reset();
    mono.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    keyboardState.reset();
}

bool QuantumSynthProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void QuantumSynthProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if ((int) mono.size() < numSamples)
        mono.resize ((size_t) numSamples);  // hosts should not exceed the prepared size, but be safe

    // Merge notes played on the on-screen keyboard into the incoming MIDI.
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    engine.setSettings (readSettings());

    int position = 0;
    for (const auto metadata : midi)
    {
        const int eventPos = juce::jlimit (0, numSamples, metadata.samplePosition);
        if (eventPos > position)
        {
            engine.render (mono.data() + position, eventPos - position);
            position = eventPos;
        }

        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            engine.noteOn (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            engine.noteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
            engine.allNotesOff();
        else if (msg.isPitchWheel())
            engine.setPitchBend ((float) (msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isResetAllControllers())
            engine.resetControllers();
        else if (msg.isController())
            handleController (msg.getControllerNumber(), msg.getControllerValue());
    }

    if (position < numSamples)
        engine.render (mono.data() + position, numSamples - position);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono.data(), numSamples);

    liveBarrierHz.store (engine.getCurrentFilterSettings().barrierHz);
}

void QuantumSynthProcessor::handleController (int cc, int value)
{
    switch (cc)
    {
        case qs::xd::joystickUpController:   engine.setModWheel ((float) value / 127.0f); return;
        case qs::xd::joystickDownController: engine.setBarrierDrop ((float) value / 127.0f); return;
        case qs::xd::damperController:       engine.setSustainPedal (value >= 64); return;
        default: break;
    }

    if ((int) parameters.getRawParameterValue ("ccMap")->load() == 0)
        return;

    const auto change = xdDecoder.handle (cc, value);
    if (! change)
        return;

    if (auto* param = parameters.getParameter (change->control->paramId))
    {
        const float normalised = change->normalised ? change->value : param->convertTo0to1 (change->value);
        param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));

        // Apply it from this point in the block rather than from the next block.
        engine.setSettings (readSettings());
        lastXdControl.store (change->control->cc);
    }
}

juce::AudioProcessorEditor* QuantumSynthProcessor::createEditor()
{
    return new QuantumSynthEditor (*this);
}

void QuantumSynthProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void QuantumSynthProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (parameters.state.getType()))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new QuantumSynthProcessor();
}
