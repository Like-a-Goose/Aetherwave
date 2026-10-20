#pragma once

#include "PluginProcessor.h"
#include "QuantumLookAndFeel.h"

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
    void paintScattering (juce::Graphics& g, juce::Rectangle<float> area);
    void paintLevels (juce::Graphics& g, juce::Rectangle<float> area);
    void paintResponse (juce::Graphics& g, juce::Rectangle<float> area);
    static void paintHeading (juce::Graphics& g, juce::Rectangle<float>& area, const juce::String& text);

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
        static constexpr int labelHeight = 20, valueHeight = 22, boxHeight = 30;

        juce::Label label;
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::TextBoxBelow };
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> boxAttachment;
        int preferredWidth = 84;

        void resized() override;
    };

    struct Section
    {
        juce::String title;
        juce::Colour accent;
        std::vector<Control*> controls;
        juce::Rectangle<int> bounds;

        int naturalWidth() const;
        void layOutControls (juce::Rectangle<int> area) const;
    };

    Control* addKnob (const juce::String& paramId, const juce::String& name);
    Control* addChoice (const juce::String& paramId, const juce::String& name);
    void addSection (const juce::String& title, juce::Colour accent, std::vector<Control*> sectionControls);
    void layOutRow (juce::Rectangle<int> row, std::initializer_list<size_t> sectionIndices);
    void timerCallback() override;
    void enableMinilogueMidiInput();

    // Everything is laid out at this size and scaled to the window, so enlarging the
    // window enlarges text and knobs too.
    static constexpr int designWidth = 1236, designHeight = 716;
    float scale = 1.0f;

    QuantumLookAndFeel lookAndFeel;  // declared first so it outlives every component using it
    QuantumSynthProcessor& synth;
    BarrierDisplay display;
    juce::MidiKeyboardComponent keyboard;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Section> sections;

    Control* gapControl = nullptr;
    juce::Rectangle<int> header, statusPill;
    juce::Label lastControl;
    int shownControl = -1;
    int midiCheckCounter = 19;  // check for a minilogue xd straight away
    juce::StringArray autoEnabledMidiInputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthEditor)
};
