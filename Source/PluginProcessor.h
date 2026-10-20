#pragma once

#include "dsp/MinilogueXd.h"
#include "dsp/SynthEngine.h"

#include <juce_audio_processors/juce_audio_processors.h>

class QuantumSynthProcessor : public juce::AudioProcessor
{
public:
    QuantumSynthProcessor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Filter settings from the current parameters, before envelope and key tracking.
    qs::FilterSettings getBaseFilterSettings() const;
    // Where the envelope has moved the barrier to right now (for the display).
    double getLiveBarrierHz() const { return liveBarrierHz.load(); }
    // CC number of the last minilogue xd control that moved a parameter, or -1.
    int getLastXdControl() const { return lastXdControl.load(); }

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    qs::SynthSettings readSettings() const;
    void handleController (int cc, int value);

    qs::SynthEngine engine;
    qs::xd::Decoder xdDecoder;
    std::atomic<int> lastXdControl { -1 };
    std::vector<float> mono;
    std::atomic<double> liveBarrierHz { -1.0 };  // negative until audio has run

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthProcessor)
};
