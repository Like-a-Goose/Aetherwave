#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
const juce::StringArray waveNames { "Saw", "Square", "Triangle", "Sine" };
const juce::StringArray modeNames { "Tunnel", "Reflect", "Resonant Tunnel", "Resonant Reflect" };

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
    s.outputGainDb = value ("gain");
    return s;
}

void QuantumSynthProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate);
    engine.setSettings (readSettings());
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
    }

    if (position < numSamples)
        engine.render (mono.data() + position, numSamples - position);

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.copyFrom (ch, 0, mono.data(), numSamples);

    liveBarrierHz.store (engine.getCurrentFilterSettings().barrierHz);
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
