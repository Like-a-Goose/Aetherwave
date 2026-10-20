#pragma once

#include <algorithm>
#include <array>
#include <optional>

namespace qs
{

// Front-panel controls of a Korg minilogue xd and the synth parameter each one drives.
// CC numbers follow the minilogue xd MIDI implementation (revision 1.01).
namespace xd
{

enum class Kind
{
    knob,         // 10-bit knob: CC63 carries the low 3 bits just before the knob's own CC
    vcoWave,      // SQR / TRI / SAW switch
    vcoOctave,    // 16' / 8' / 4' / 2' switch as an oscillator transpose
    modeSwitch,   // 16' / 8' / 4' / 2' switch picking one of the four filter modes
    keyTrack,     // 0% / 50% / 100% switch
    threeWay,     // any other 3-position switch, matching the parameter's choice order
    level         // VCO 1 or VCO 2 level, combined into the oscillator mix
};

struct Control
{
    int cc;
    const char* panelName;   // as printed on the minilogue xd
    const char* paramId;     // synth parameter it moves
    Kind kind;
};

constexpr std::array<Control, 22> controls { {
    { 43, "CUTOFF", "barrier", Kind::knob },
    { 44, "RESONANCE", "width", Kind::knob },
    { 54, "MULTI SHAPE", "gap", Kind::knob },
    { 48, "VCO1 OCTAVE", "filterMode", Kind::modeSwitch },
    { 83, "KEY TRACK", "keyTrack", Kind::keyTrack },
    { 22, "EG INT", "envAmount", Kind::knob },
    { 20, "EG ATTACK", "filtA", Kind::knob },
    { 21, "EG DECAY", "filtD", Kind::knob },

    { 16, "AMP ATTACK", "ampA", Kind::knob },
    { 17, "AMP DECAY", "ampD", Kind::knob },
    { 18, "AMP SUSTAIN", "ampS", Kind::knob },
    { 19, "AMP RELEASE", "ampR", Kind::knob },

    { 50, "VCO1 WAVE", "osc1Wave", Kind::vcoWave },
    { 51, "VCO2 WAVE", "osc2Wave", Kind::vcoWave },
    { 49, "VCO2 OCTAVE", "osc2Semi", Kind::vcoOctave },
    { 35, "VCO2 PITCH", "osc2Fine", Kind::knob },
    { 39, "VCO1 LEVEL", "oscMix", Kind::level },
    { 40, "VCO2 LEVEL", "oscMix", Kind::level },

    { 24, "LFO RATE", "lfoRate", Kind::knob },
    { 26, "LFO INT", "lfoAmount", Kind::knob },
    { 57, "LFO WAVE", "lfoWave", Kind::threeWay },
    { 56, "LFO TARGET", "lfoTarget", Kind::threeWay },
} };

constexpr int lsbController = 63;
constexpr int joystickUpController = 1;    // joystick Y+
constexpr int joystickDownController = 2;  // joystick Y-
constexpr int damperController = 64;

inline const Control* findControl (int cc)
{
    for (const auto& c : controls)
        if (c.cc == cc)
            return &c;
    return nullptr;
}

// A parameter change decoded from a CC. Knobs give a normalised 0..1 position;
// switches and the levels give the parameter's plain value (choice index, semitones, ...).
struct Change
{
    const Control* control;
    bool normalised;
    float value;
};

// Turns the minilogue xd's CC stream into parameter changes.
class Decoder
{
public:
    std::optional<Change> handle (int cc, int value)
    {
        value = std::clamp (value, 0, 127);

        if (cc == lsbController)
        {
            // The low 3 bits normally arrive as 0..7; accept them in bits 4..6 as well.
            pendingLsb = value <= 7 ? value : (value >> 4) & 7;
            return std::nullopt;
        }

        const int lsb = pendingLsb;
        pendingLsb = 0;  // the low bits belong to the very next CC only

        const auto* c = findControl (cc);
        if (c == nullptr)
            return std::nullopt;

        switch (c->kind)
        {
            case Kind::knob:
                return Change { c, true, (float) ((value << 3) | lsb) / 1023.0f };

            case Kind::vcoWave:
            {
                // minilogue xd SQR / TRI / SAW to the synth's Saw, Square, Triangle, Sine list.
                static constexpr int waveIndex[] = { 1, 2, 0 };
                return Change { c, false, (float) waveIndex[threeWay (value)] };
            }

            case Kind::vcoOctave:
                return Change { c, false, (float) ((fourWay (value) - 1) * 12) };  // 8' is in tune with VCO 1

            case Kind::modeSwitch:
                return Change { c, false, (float) fourWay (value) };

            case Kind::keyTrack:
                return Change { c, false, 0.5f * (float) threeWay (value) };

            case Kind::threeWay:
                return Change { c, false, (float) threeWay (value) };

            case Kind::level:
            {
                levels[cc == 39 ? 0 : 1] = (float) ((value << 3) | lsb) / 1023.0f;
                const float total = levels[0] + levels[1];
                if (total <= 0.0f)
                    return std::nullopt;
                return Change { c, false, levels[1] / total };
            }
        }
        return std::nullopt;
    }

    void reset()
    {
        pendingLsb = 0;
        levels = { 1.0f, 1.0f };
    }

private:
    static int threeWay (int value) { return value <= 42 ? 0 : (value <= 85 ? 1 : 2); }
    static int fourWay (int value) { return std::min (value / 32, 3); }

    int pendingLsb = 0;
    std::array<float, 2> levels { 1.0f, 1.0f };
};

} // namespace xd
} // namespace qs
