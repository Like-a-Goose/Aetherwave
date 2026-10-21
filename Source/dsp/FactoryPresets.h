#pragma once

#include <vector>

namespace qs
{

// The sounds that ship with the synth. Each preset lists only the parameters it changes;
// loading one puts every other parameter back to its default first. Values are plain
// parameter values: choice indices, Hz, seconds, octaves, dB.
//
// Choice indices, for reference:
//   osc1Wave / osc2Wave: 0 Saw, 1 Square, 2 Triangle, 3 Sine
//   filterMode: 0 Tunnel, 1 Reflect, 2 Resonant Tunnel, 3 Resonant Reflect, 4 Soft Tunnel,
//               5 Step, 6 Quantum Well, 7 Crystal, 8 Band Gap, 9 Particle in a Box,
//               10 Harmonic Oscillator, 11 Hydrogen
//   lfoWave: 0 Square, 1 Triangle, 2 Saw      lfoTarget: 0 Barrier, 1 Mix, 2 Pitch
//
// New presets go at the end of their category; hosts address factory presets by position,
// so reordering changes what a saved host program number loads.

struct PresetValue
{
    const char* id;
    float value;
};

struct FactoryPreset
{
    const char* name;
    const char* category;
    std::vector<PresetValue> values;
};

inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> presets {
        { "Init", "Init", {} },

        // Bass
        { "Barrier Bass", "Bass",
          { { "osc2Semi", -12 }, { "osc2Fine", 0 }, { "oscMix", 0.4f }, { "filterMode", 1 }, { "barrier", 180 },
            { "envAmount", 3.5f }, { "keyTrack", 0.3f }, { "filtD", 0.35f }, { "filtS", 0.0f },
            { "ampA", 0.002f }, { "ampD", 0.5f }, { "ampS", 0.9f }, { "ampR", 0.15f }, { "gain", -3.5f } } },
        { "Well Sub", "Bass",
          { { "osc2Wave", 3 }, { "osc2Semi", -12 }, { "osc2Fine", 0 }, { "oscMix", 0.6f }, { "filterMode", 6 },
            { "barrier", 120 }, { "width", 2 }, { "envAmount", 2.5f }, { "keyTrack", 0.3f }, { "filtD", 0.25f },
            { "filtS", 0.1f }, { "ampA", 0.002f }, { "ampS", 1.0f }, { "ampR", 0.12f }, { "gain", -3.5f } } },
        { "Resonant Wobble", "Bass",
          { { "osc2Wave", 0 }, { "osc2Semi", -12 }, { "filterMode", 2 }, { "barrier", 250 }, { "width", 2.5f },
            { "gap", 5 }, { "envAmount", 1 }, { "keyTrack", 0.4f }, { "ampS", 1.0f }, { "ampR", 0.15f },
            { "lfoWave", 1 }, { "lfoTarget", 0 }, { "lfoRate", 3 }, { "lfoAmount", 0.5f }, { "gain", -4 } } },
        { "Step Punch", "Bass",
          { { "osc1Wave", 1 }, { "osc2Wave", 0 }, { "osc2Semi", -12 }, { "osc2Fine", 0 }, { "filterMode", 5 },
            { "barrier", 300 }, { "envAmount", -3 }, { "keyTrack", 0.5f }, { "filtA", 0.001f }, { "filtD", 0.3f },
            { "filtS", 0.0f }, { "ampA", 0.001f }, { "ampD", 0.4f }, { "ampS", 0.7f }, { "ampR", 0.1f }, { "gain", 2 } } },

        // Lead
        { "Tunnel Lead", "Lead",
          { { "filterMode", 0 }, { "barrier", 400 }, { "width", 1.5f }, { "envAmount", 1.5f }, { "keyTrack", 0.8f },
            { "filtD", 0.4f }, { "filtS", 0.4f }, { "ampS", 0.9f }, { "ampR", 0.25f },
            { "lfoTarget", 2 }, { "lfoRate", 5.5f }, { "bendRange", 2 }, { "gain", -1 } } },
        { "Soft Sync Lead", "Lead",
          { { "osc2Wave", 0 }, { "osc2Semi", 12 }, { "osc2Fine", 4 }, { "oscMix", 0.35f }, { "filterMode", 4 },
            { "barrier", 500 }, { "width", 4 }, { "envAmount", 2 }, { "keyTrack", 1 }, { "filtA", 0.05f },
            { "filtD", 0.6f }, { "filtS", 0.3f }, { "ampS", 0.9f }, { "ampR", 0.3f }, { "gain", -4.5f } } },
        { "Crystal Lead", "Lead",
          { { "filterMode", 7 }, { "barrier", 300 }, { "width", 1.2f }, { "gap", 4 }, { "envAmount", 2 },
            { "keyTrack", 1 }, { "filtD", 0.5f }, { "filtS", 0.3f }, { "ampS", 0.9f }, { "ampR", 0.3f }, { "gain", 0 } } },
        { "Hydrogen Scream", "Lead",
          { { "osc2Wave", 0 }, { "osc2Semi", 12 }, { "filterMode", 11 }, { "barrier", 1200 }, { "width", 3 },
            { "envAmount", 1.5f }, { "keyTrack", 0.8f }, { "filtD", 0.8f }, { "filtS", 0.5f }, { "ampS", 0.9f },
            { "ampR", 0.3f }, { "lfoTarget", 0 }, { "lfoRate", 5 }, { "lfoAmount", 0.15f }, { "gain", -2.5f } } },

        // Keys
        { "Box Keys", "Keys",
          { { "osc2Wave", 0 }, { "osc2Fine", 3 }, { "filterMode", 9 }, { "barrier", 220 },
            { "width", 1.2f }, { "envAmount", 2 }, { "keyTrack", 1 }, { "filtD", 0.6f }, { "filtS", 0.2f },
            { "ampD", 1.2f }, { "ampS", 0.2f }, { "ampR", 0.5f }, { "gain", 3 } } },
        { "Oscillator Organ", "Keys",
          { { "osc1Wave", 1 }, { "osc2Wave", 0 }, { "osc2Semi", 12 }, { "osc2Fine", 0 }, { "oscMix", 0.3f },
            { "filterMode", 10 }, { "barrier", 523 }, { "width", 1.5f }, { "envAmount", 0 }, { "keyTrack", 1 },
            { "ampA", 0.005f }, { "ampD", 0.1f }, { "ampS", 1.0f }, { "ampR", 0.1f },
            { "lfoTarget", 2 }, { "lfoRate", 6 }, { "gain", -4.5f } } },
        { "Resonant EP", "Keys",
          { { "osc1Wave", 3 }, { "osc2Wave", 2 }, { "osc2Semi", 12 }, { "osc2Fine", 0 }, { "filterMode", 3 },
            { "barrier", 900 }, { "width", 2 }, { "gap", 8 }, { "envAmount", 1 }, { "keyTrack", 1 },
            { "filtD", 0.8f }, { "ampD", 1.5f }, { "ampS", 0.3f }, { "ampR", 0.6f }, { "gain", -0.5f } } },

        // Pad
        { "Band Gap Pad", "Pad",
          { { "osc2Wave", 0 }, { "osc2Semi", 12 }, { "osc2Fine", 9 }, { "filterMode", 8 }, { "barrier", 500 },
            { "width", 1 }, { "gap", 6 }, { "envAmount", 1.5f }, { "keyTrack", 0.5f }, { "filtA", 2 }, { "filtD", 2 },
            { "filtS", 0.5f }, { "filtR", 2 }, { "ampA", 1.2f }, { "ampS", 0.9f }, { "ampR", 2 },
            { "lfoWave", 1 }, { "lfoTarget", 0 }, { "lfoRate", 0.3f }, { "lfoAmount", 0.2f }, { "gain", -1.5f } } },
        { "Quantum Choir", "Pad",
          { { "osc2Wave", 0 }, { "osc2Fine", 12 }, { "filterMode", 11 }, { "barrier", 1800 }, { "width", 4 },
            { "envAmount", 0.5f }, { "keyTrack", 0.3f }, { "filtA", 1 }, { "ampA", 0.8f }, { "ampS", 0.9f },
            { "ampR", 2.5f }, { "lfoWave", 1 }, { "lfoTarget", 1 }, { "lfoRate", 0.5f }, { "lfoAmount", 0.2f },
            { "gain", -1.5f } } },
        { "Crystal Haze", "Pad",
          { { "osc2Wave", 0 }, { "osc2Fine", 8 }, { "filterMode", 7 }, { "barrier", 700 }, { "width", 0.8f }, { "gap", 10 }, { "envAmount", 1 },
            { "filtA", 1.5f }, { "filtD", 2 }, { "filtS", 0.4f }, { "ampA", 1.5f }, { "ampS", 0.9f }, { "ampR", 3 },
            { "lfoWave", 1 }, { "lfoTarget", 0 }, { "lfoRate", 0.15f }, { "lfoAmount", 0.35f }, { "gain", 0 } } },
        { "Tunnel Strings", "Pad",
          { { "osc2Wave", 0 }, { "osc2Fine", 10 }, { "filterMode", 4 }, { "barrier", 250 }, { "width", 2 },
            { "envAmount", 0.5f }, { "keyTrack", 0.5f }, { "filtA", 0.5f }, { "ampA", 0.6f }, { "ampS", 0.85f },
            { "ampR", 1.2f }, { "gain", -0.5f } } },

        // Pluck
        { "Ramsauer Pluck", "Pluck",
          { { "filterMode", 6 }, { "barrier", 600 }, { "width", 3 }, { "envAmount", 3.5f }, { "keyTrack", 0.7f },
            { "filtA", 0.001f }, { "filtD", 0.2f }, { "filtS", 0.0f }, { "ampA", 0.001f }, { "ampD", 0.5f },
            { "ampS", 0.0f }, { "ampR", 0.3f }, { "gain", 5.5f } } },
        { "Twin Barrier Pluck", "Pluck",
          { { "osc2Wave", 0 }, { "filterMode", 2 }, { "barrier", 200 }, { "width", 3 }, { "gap", 6 },
            { "envAmount", 4 }, { "keyTrack", 0.7f }, { "filtA", 0.001f }, { "filtD", 0.25f }, { "filtS", 0.0f },
            { "ampA", 0.001f }, { "ampD", 0.6f }, { "ampS", 0.0f }, { "ampR", 0.4f }, { "gain", 5.5f } } },
        { "Tunnel Harp", "Pluck",
          { { "osc1Wave", 2 }, { "osc2Wave", 0 }, { "osc2Semi", 12 }, { "osc2Fine", 0 }, { "oscMix", 0.3f },
            { "filterMode", 0 }, { "barrier", 300 }, { "width", 2 }, { "envAmount", 2 }, { "keyTrack", 1 },
            { "filtA", 0.001f }, { "filtD", 0.4f }, { "filtS", 0.0f }, { "ampA", 0.001f }, { "ampD", 0.9f },
            { "ampS", 0.0f }, { "ampR", 0.9f }, { "gain", 6 } } },

        // Bell
        { "Box Bells", "Bell",
          { { "osc2Wave", 0 }, { "osc2Semi", 19 }, { "osc2Fine", 0 }, { "oscMix", 0.4f }, { "filterMode", 9 },
            { "barrier", 400 }, { "width", 4 }, { "envAmount", 1 }, { "keyTrack", 1 }, { "filtA", 0.001f },
            { "filtD", 1 }, { "filtS", 0.0f }, { "ampA", 0.001f }, { "ampD", 2.5f }, { "ampS", 0.0f }, { "ampR", 2 },
            { "gain", -1.5f } } },
        { "Hydrogen Chimes", "Bell",
          { { "osc2Semi", 12 }, { "osc2Fine", 0 }, { "filterMode", 11 }, { "barrier", 1500 }, { "width", 3 },
            { "envAmount", 1 }, { "keyTrack", 1 }, { "filtA", 0.001f }, { "filtD", 1 }, { "filtS", 0.0f },
            { "ampA", 0.001f }, { "ampD", 1.8f }, { "ampS", 0.0f }, { "ampR", 1.8f }, { "gain", 6 } } },
        { "Oscillator Glass", "Bell",
          { { "osc2Wave", 0 }, { "osc2Semi", 24 }, { "osc2Fine", 0 }, { "oscMix", 0.4f }, { "filterMode", 10 },
            { "barrier", 700 }, { "width", 5 }, { "envAmount", 0.5f }, { "keyTrack", 1 }, { "ampA", 0.001f },
            { "ampD", 2 }, { "ampS", 0.1f }, { "ampR", 2 }, { "gain", 0 } } },

        // Effects and textures
        { "Tunnelling Sweep", "FX",
          { { "osc2Wave", 0 }, { "osc2Fine", 15 }, { "filterMode", 0 }, { "barrier", 2000 }, { "width", 1 },
            { "envAmount", -5 }, { "keyTrack", 0 }, { "filtA", 2 }, { "filtD", 3 }, { "filtS", 0.0f },
            { "ampA", 0.3f }, { "ampS", 1.0f }, { "ampR", 2 },
            { "lfoWave", 2 }, { "lfoTarget", 0 }, { "lfoRate", 0.2f }, { "lfoAmount", 0.6f }, { "gain", -4 } } },
        { "Lattice Rain", "FX",
          { { "filterMode", 7 }, { "barrier", 1000 }, { "width", 0.5f }, { "gap", 15 }, { "envAmount", 0 },
            { "ampD", 0.3f }, { "ampS", 0.3f }, { "ampR", 0.8f },
            { "lfoWave", 0 }, { "lfoTarget", 0 }, { "lfoRate", 8 }, { "lfoAmount", 0.5f }, { "gain", 2.5f } } },
        { "Wavefunction Wah", "FX",
          { { "filterMode", 1 }, { "barrier", 500 }, { "width", 6 }, { "envAmount", 0 }, { "ampS", 1.0f },
            { "ampR", 0.3f }, { "lfoWave", 1 }, { "lfoTarget", 0 }, { "lfoRate", 1.5f }, { "lfoAmount", 0.6f },
            { "gain", -5.5f } } },
        { "Ionisation", "FX",
          { { "osc2Wave", 0 }, { "osc2Semi", -12 }, { "filterMode", 11 }, { "barrier", 300 }, { "width", 2 },
            { "envAmount", 5 }, { "keyTrack", 0.3f }, { "filtA", 1.5f }, { "filtD", 2 }, { "filtS", 0.6f },
            { "filtR", 2 }, { "ampA", 0.2f }, { "ampS", 1.0f }, { "ampR", 2 }, { "gain", -5 } } },
    };
    return presets;
}

} // namespace qs
