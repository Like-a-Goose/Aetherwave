#pragma once

#include "dsp/FactoryPresets.h"

#include <juce_audio_processors/juce_audio_processors.h>

// The preset library: the factory sounds plus presets the user saves, which live as files in
// a folder of their own so they can be copied between machines.
class PresetManager
{
public:
    struct Entry
    {
        juce::String name, category;
        int factoryIndex = -1;  // index into qs::factoryPresets(), or -1 for a user preset
        juce::File file;        // user presets only

        bool isFactory() const { return factoryIndex >= 0; }
    };

    static constexpr const char* fileExtension = ".qspreset";

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    // Factory presets first (in category order), then user presets sorted by name.
    juce::Array<Entry> getEntries() const;
    int indexOfCurrent (const juce::Array<Entry>& entries) const;

    void load (const Entry& entry);
    void loadFactory (int index);
    // Saves the current sound as a user preset, replacing one with the same name.
    bool saveUserPreset (const juce::String& name);
    bool deleteUserPreset (const Entry& entry);

    juce::String getCurrentName() const;
    int getCurrentFactoryIndex() const;

    // True once a knob has moved since the preset was loaded or saved.
    bool isModified() const;
    // Takes the current sound as the reference for isModified. After a project is restored,
    // pass true if it had been edited when it was saved.
    void markClean (bool editedBefore = false);

    static juce::File getUserFolder();

private:
    void resetToDefaults();
    void setCurrent (const juce::String& name, int factoryIndex);

    static bool isPresetParameter (const juce::String& id) { return id != "ccMap"; }  // the CC map is set-up, not sound

    juce::AudioProcessorValueTreeState& parameters;
    std::vector<std::pair<juce::RangedAudioParameter*, float>> cleanValues;
    bool editedBeforeRestore = false;
};
