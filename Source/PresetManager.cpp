#include "PresetManager.h"

namespace
{
const juce::Identifier presetNameId { "presetName" };
const juce::Identifier presetFactoryIndexId { "presetFactoryIndex" };
const juce::Identifier presetTag { "QuantumSynthPreset" };
const juce::Identifier paramTag { "PARAM" };

// Category order in the browser; anything else sorts after these.
const juce::StringArray categoryOrder { "Init", "Bass", "Lead", "Keys", "Pad", "Pluck", "Bell", "FX" };
} // namespace

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state) : parameters (state)
{
}

juce::File PresetManager::getUserFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("Quantum Synth")
        .getChildFile ("Presets");
}

juce::Array<PresetManager::Entry> PresetManager::getEntries() const
{
    juce::Array<Entry> entries;
    const auto& factory = qs::factoryPresets();
    for (const auto& category : categoryOrder)
        for (size_t i = 0; i < factory.size(); ++i)
            if (category == factory[i].category)
                entries.add ({ factory[i].name, factory[i].category, (int) i, {} });

    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, juce::String ("*") + fileExtension);
    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
               { return a.getFileNameWithoutExtension().compareNatural (b.getFileNameWithoutExtension()) < 0; });
    for (const auto& file : files)
        entries.add ({ file.getFileNameWithoutExtension(), "User", -1, file });

    return entries;
}

int PresetManager::indexOfCurrent (const juce::Array<Entry>& entries) const
{
    const auto name = getCurrentName();
    const int factoryIndex = getCurrentFactoryIndex();
    for (int i = 0; i < entries.size(); ++i)
        if (entries[i].factoryIndex == factoryIndex && entries[i].name == name)
            return i;
    return -1;
}

void PresetManager::resetToDefaults()
{
    for (auto* param : parameters.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (isPresetParameter (ranged->getParameterID()))
                ranged->setValueNotifyingHost (ranged->getDefaultValue());
}

void PresetManager::loadFactory (int index)
{
    const auto& factory = qs::factoryPresets();
    if (index < 0 || index >= (int) factory.size())
        return;

    const auto& preset = factory[(size_t) index];
    resetToDefaults();
    for (const auto& v : preset.values)
    {
        auto* param = parameters.getParameter (v.id);
        jassert (param != nullptr);  // a factory preset names a parameter that doesn't exist
        if (param != nullptr)
            param->setValueNotifyingHost (param->convertTo0to1 (v.value));
    }
    setCurrent (preset.name, index);
}

void PresetManager::load (const Entry& entry)
{
    if (entry.isFactory())
    {
        loadFactory (entry.factoryIndex);
        return;
    }

    const auto xml = juce::XmlDocument::parse (entry.file);
    if (xml == nullptr || ! xml->hasTagName (presetTag.toString()))
        return;

    // Parameters the file doesn't mention (from a newer or older version) go to their defaults.
    resetToDefaults();
    for (const auto* p : xml->getChildWithTagNameIterator (paramTag.toString()))
        if (auto* param = parameters.getParameter (p->getStringAttribute ("id")))
            if (isPresetParameter (param->getParameterID()))
                param->setValueNotifyingHost (param->convertTo0to1 ((float) p->getDoubleAttribute ("value")));

    setCurrent (entry.name, -1);
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName (rawName.trim());
    if (name.isEmpty())
        return false;

    juce::XmlElement xml (presetTag);
    xml.setAttribute ("name", name);
    xml.setAttribute ("version", 1);
    for (auto* param : parameters.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (isPresetParameter (ranged->getParameterID()))
            {
                auto* p = xml.createNewChildElement (paramTag);
                p->setAttribute ("id", ranged->getParameterID());
                p->setAttribute ("value", ranged->convertFrom0to1 (ranged->getValue()));
            }

    const auto folder = getUserFolder();
    if (! folder.createDirectory())
        return false;
    if (! xml.writeTo (folder.getChildFile (name + fileExtension)))
        return false;

    setCurrent (name, -1);
    return true;
}

bool PresetManager::deleteUserPreset (const Entry& entry)
{
    // Deleting the preset that is loaded keeps the sound playing; it can be saved again.
    return ! entry.isFactory() && entry.file.deleteFile();
}

juce::String PresetManager::getCurrentName() const
{
    return parameters.state.getProperty (presetNameId, "Init").toString();
}

int PresetManager::getCurrentFactoryIndex() const
{
    return (int) parameters.state.getProperty (presetFactoryIndexId, 0);
}

void PresetManager::setCurrent (const juce::String& name, int factoryIndex)
{
    parameters.state.setProperty (presetNameId, name, nullptr);
    parameters.state.setProperty (presetFactoryIndexId, factoryIndex, nullptr);
    markClean();
}

void PresetManager::markClean (bool editedBefore)
{
    editedBeforeRestore = editedBefore;
    cleanValues.clear();
    for (auto* param : parameters.processor.getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (isPresetParameter (ranged->getParameterID()))
                cleanValues.emplace_back (ranged, ranged->getValue());
}

bool PresetManager::isModified() const
{
    return editedBeforeRestore || std::any_of (cleanValues.begin(), cleanValues.end(), [] (const auto& p)
                        { return std::abs (p.first->getValue() - p.second) > 1.0e-4f; });
}
