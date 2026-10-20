#pragma once

#include "PluginProcessor.h"

#include <juce_audio_utils/juce_audio_utils.h>

// Draws the barrier potential and the filter response it produces.
class BarrierDisplay : public juce::Component, private juce::Timer
{
public:
    explicit BarrierDisplay (QuantumSynthProcessor& p);
    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;
    void paintPotential (juce::Graphics& g, juce::Rectangle<float> area);
    void paintResponse (juce::Graphics& g, juce::Rectangle<float> area);

    QuantumSynthProcessor& processor;
    qs::FilterSettings settings;
    double liveBarrierHz = 800.0;
};

class QuantumSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QuantumSynthEditor (QuantumSynthProcessor&);
    ~QuantumSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // A labelled control: a rotary knob or a drop-down attached to one parameter.
    struct Control : public juce::Component
    {
        juce::Label label;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAttachment;
        int preferredWidth = 74;

        void resized() override;
    };

    struct Section
    {
        juce::String title;
        std::vector<Control*> controls;
        juce::Rectangle<int> bounds;
    };

    Control* addKnob (const juce::String& paramId, const juce::String& name);
    Control* addChoice (const juce::String& paramId, const juce::String& name);
    void timerCallback() override;
    void enableMinilogueMidiInput();

    QuantumSynthProcessor& processor;
    BarrierDisplay display;
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Section> sections;

    juce::Label lastControl;
    int shownControl = -1;
    int midiCheckCounter = 19;  // check for a minilogue xd straight away
    juce::StringArray autoEnabledMidiInputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthEditor)
};
