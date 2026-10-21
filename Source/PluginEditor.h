#pragma once

#include "PluginProcessor.h"
#include "QuantumLookAndFeel.h"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <functional>

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

// Browses the preset library: previous / next, a menu of every preset, and saving your own.
class PresetBar : public juce::Component, private juce::Timer
{
public:
    explicit PresetBar (PresetManager& manager);
    void resized() override;

private:
    void timerCallback() override;
    void showMenu();
    void step (int delta);
    void beginSave();
    void finishSave (bool commit);
    void refresh();

    PresetManager& presets;
    juce::TextButton previous, next, browse, save { "Save" };
    juce::TextEditor nameEditor;
    juce::String shownText;
};

// Plays notes from the computer keyboard like a piano: A S D F ... are the white keys of one
// octave and W E T Y U the black ones; Z and X shift the octave.
class LaptopKeys
{
public:
    explicit LaptopKeys (juce::MidiKeyboardState& state) : keyboardState (state) {}

    // Returns true if the key is one of ours (so it isn't passed on to the host).
    bool keyPressed (const juce::KeyPress& key);
    // Starts and stops notes to match which mapped keys are down.
    bool keyStateChanged();
    void releaseAll();

    int getBaseNote() const { return baseNote; }
    void setBaseNote (int note);
    static constexpr int numKeys = 18;  // C to the F an octave and a half above

    std::function<void()> onOctaveChange;

private:
    juce::MidiKeyboardState& keyboardState;
    int baseNote = 60;
    std::array<int, numKeys> playing;  // note held by each key, or -1
    bool initialised = false;
};

class QuantumSynthEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit QuantumSynthEditor (QuantumSynthProcessor&);
    ~QuantumSynthEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void focusLost (FocusChangeType) override;
    void visibilityChanged() override;

private:
    // A labelled control: a rotary knob or a drop-down attached to one parameter.
    struct Control : public juce::Component
    {
        static constexpr int labelHeight = 20, valueHeight = 22, boxHeight = 30;

        static constexpr int hintHeight = 15;

        juce::Label label;
        juce::Label hint;  // what the control does, under minilogue xd panel names
        juce::String name, xdName, xdHint;
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
        const juce::Colour* accent;  // a theme colour, so it follows theme changes
        std::vector<Control*> controls;
        juce::Rectangle<int> bounds;
        juce::StringArray rowTitles;  // minilogue xd layout: names printed beside each row
        juce::String note;

        int naturalWidth() const;
        void layOutControls (juce::Rectangle<int> area) const;
    };

    Control* addKnob (const juce::String& paramId, const juce::String& name, const juce::String& xdName = {}, const juce::String& xdHint = {});
    Control* addChoice (const juce::String& paramId, const juce::String& name, const juce::String& xdName = {}, const juce::String& xdHint = {});
    void addSection (const juce::String& title, const juce::Colour& accent, std::vector<Control*> sectionControls);
    void layOutRow (juce::Rectangle<int> row, std::initializer_list<size_t> sectionIndices);
    void layOutStandard (juce::Rectangle<int> area);
    void layOutMinilogue (juce::Rectangle<int> area);
    void paintSection (juce::Graphics& g, const Section& section) const;
    void paintLaptopKeys (juce::Graphics& g) const;
    void showThemeMenu();
    void setTheme (int index);
    void applyThemeColours();
    void keepKeyboardFocus();
    void timerCallback() override;
    void enableMinilogueMidiInput();

    // Everything is laid out at this size and scaled to the window, so enlarging the
    // window enlarges text and knobs too.
    static constexpr int designWidth = 1236, designHeight = 716;
    static constexpr int titleWidth = 200, subtitleWidth = 168;
    static constexpr int rowTitleWidth = 64;  // row names in the minilogue xd layout
    float scale = 1.0f;

    QuantumLookAndFeel lookAndFeel;  // declared first so it outlives every component using it
    QuantumSynthProcessor& synth;
    BarrierDisplay display;
    PresetBar presetBar;
    juce::TextButton themeButton { "Theme" };
    juce::MidiKeyboardComponent keyboard;
    LaptopKeys laptopKeys;
    juce::PropertiesFile settings;

    std::vector<std::unique_ptr<Control>> controls;
    std::vector<Section> sections;     // the standard layout
    std::vector<Section> xdSections;   // the minilogue xd panel layout
    bool minilogueLayout = false;
    int themeIndex = 0;

    Control* gapControl = nullptr;
    juce::Rectangle<int> header, statusPill;
    juce::Label lastControl;
    int shownControl = -1;
    int midiCheckCounter = 19;  // check for a minilogue xd straight away
    juce::StringArray autoEnabledMidiInputs;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuantumSynthEditor)
};
