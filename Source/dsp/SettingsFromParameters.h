#pragma once

#include "SynthEngine.h"

namespace qs
{

// Builds the engine settings from parameter values. `value` maps a parameter ID to its plain
// value (choice index, Hz, seconds, ...). Shared by the plug-in and the JUCE-free tests.
template <typename ValueOf>
SynthSettings settingsFromParameters (ValueOf&& value)
{
    SynthSettings s;
    s.osc1Wave = (Waveform) (int) value ("osc1Wave");
    s.osc2Wave = (Waveform) (int) value ("osc2Wave");
    s.osc2Semitones = value ("osc2Semi");
    s.osc2Cents = value ("osc2Fine");
    s.oscMix = value ("oscMix");
    s.filter.mode = (FilterMode) (int) value ("filterMode");
    s.filter.barrierHz = value ("barrier");
    s.filter.width = value ("width");
    s.filter.gap = value ("gap");
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
    s.lfoWave = (LfoWave) (int) value ("lfoWave");
    s.lfoTarget = (LfoTarget) (int) value ("lfoTarget");
    s.lfoRate = value ("lfoRate");
    s.lfoAmount = value ("lfoAmount");
    s.bendRange = value ("bendRange");
    s.outputGainDb = value ("gain");
    return s;
}

} // namespace qs
