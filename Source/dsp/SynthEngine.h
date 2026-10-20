#pragma once

#include "TunnelFilter.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace qs
{

enum class Waveform { saw, square, triangle, sine };

struct SynthSettings
{
    Waveform osc1Wave = Waveform::saw;
    Waveform osc2Wave = Waveform::square;
    float osc2Semitones = 0.0f;
    float osc2Cents = 7.0f;
    float oscMix = 0.5f;  // 0 = only osc 1, 1 = only osc 2

    FilterSettings filter;
    float envAmountOctaves = 3.0f;  // how far the filter envelope pushes the barrier
    float keyTrack = 0.5f;          // 1 = barrier follows the played note exactly

    float ampAttack = 0.005f, ampDecay = 0.3f, ampSustain = 0.8f, ampRelease = 0.4f;
    float filterAttack = 0.01f, filterDecay = 0.5f, filterSustain = 0.2f, filterRelease = 0.5f;

    float outputGainDb = -6.0f;
};

class Adsr
{
public:
    void setSampleRate (double sr) { sampleRate = sr; }

    void setParameters (float attack, float decay, float sustain, float release)
    {
        attackStep = 1.0f / std::max (1.0f, attack * (float) sampleRate);
        decayCoeff = timeToCoeff (decay);
        sustainLevel = sustain;
        releaseCoeff = timeToCoeff (release);
    }

    void noteOn() { stage = Stage::attack; }
    void noteOff() { if (stage != Stage::idle) stage = Stage::release; }
    void reset() { stage = Stage::idle; level = 0.0f; }
    bool isActive() const { return stage != Stage::idle; }
    float getLevel() const { return level; }

    float next()
    {
        switch (stage)
        {
            case Stage::attack:
                level += attackStep;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                level = sustainLevel + (level - sustainLevel) * decayCoeff;
                break;
            case Stage::release:
                level *= releaseCoeff;
                if (level < 1.0e-4f) reset();
                break;
            case Stage::idle:
                break;
        }
        return level;
    }

private:
    enum class Stage { idle, attack, decay, release };

    // Exponential segment that falls by about 40 dB over the given time.
    float timeToCoeff (float seconds) const
    {
        return std::exp (-4.6f / std::max (1.0f, seconds * (float) sampleRate));
    }

    double sampleRate = 48000.0;
    Stage stage = Stage::idle;
    float level = 0.0f, attackStep = 0.01f, decayCoeff = 0.99f, sustainLevel = 1.0f, releaseCoeff = 0.99f;
};

// Band-limited oscillator using PolyBLEP corrections on the discontinuities.
class Oscillator
{
public:
    void reset() { phase = 0.0; }

    float next (Waveform wave, double increment)
    {
        const double t = phase;
        float out = 0.0f;

        switch (wave)
        {
            case Waveform::saw:
                out = (float) (2.0 * t - 1.0 - polyBlep (t, increment));
                break;
            case Waveform::square:
                out = (t < 0.5 ? 1.0f : -1.0f)
                      + (float) (polyBlep (t, increment) - polyBlep (std::fmod (t + 0.5, 1.0), increment));
                break;
            case Waveform::triangle:
                out = (float) (1.0 - 4.0 * std::abs (t - 0.5));
                break;
            case Waveform::sine:
                out = (float) std::sin (2.0 * 3.14159265358979323846 * t);
                break;
        }

        phase += increment;
        if (phase >= 1.0)
            phase -= 1.0;

        return out;
    }

private:
    static double polyBlep (double t, double dt)
    {
        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }
        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }
        return 0.0;
    }

    double phase = 0.0;
};

struct Voice
{
    int note = -1;
    float velocity = 0.0f;
    bool held = false;
    uint64_t startedAt = 0;
    Oscillator osc1, osc2;
    Adsr amp;
};

// Polyphonic oscillators and amp envelopes feeding one shared quantum filter
// (a paraphonic layout, like many classic analogue polysynths).
class SynthEngine
{
public:
    static constexpr int numVoices = 8;
    static constexpr int controlInterval = 128;  // samples between filter updates

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        for (auto& v : voices)
        {
            v.amp.setSampleRate (sampleRate);
            v.amp.reset();
            v.note = -1;
        }
        filterEnv.setSampleRate (sampleRate);
        filterEnv.reset();
        filter.prepare (sampleRate);
    }

    void setSettings (const SynthSettings& s) { settings = s; }

    void noteOn (int note, float velocity)
    {
        Voice* target = nullptr;

        for (auto& v : voices)
            if (! v.amp.isActive()) { target = &v; break; }

        if (target == nullptr)  // steal the oldest voice
            target = &*std::min_element (voices.begin(), voices.end(),
                                         [] (const Voice& a, const Voice& b) { return a.startedAt < b.startedAt; });

        target->note = note;
        target->velocity = velocity;
        target->held = true;
        target->startedAt = ++noteCounter;
        target->osc1.reset();
        target->osc2.reset();
        target->amp.noteOn();

        lastNote = note;
        filterEnv.noteOn();
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
        {
            if (v.note == note && v.held)
            {
                v.held = false;
                v.amp.noteOff();
            }
        }

        if (std::none_of (voices.begin(), voices.end(), [] (const Voice& v) { return v.held; }))
            filterEnv.noteOff();
    }

    void allNotesOff()
    {
        for (auto& v : voices)
        {
            v.held = false;
            v.amp.noteOff();
        }
        filterEnv.noteOff();
    }

    // Renders mono audio into out (overwrites it).
    void render (float* out, int numSamples)
    {
        while (numSamples > 0)
        {
            const int chunk = std::min (numSamples, controlInterval);
            renderChunk (out, chunk);
            out += chunk;
            numSamples -= chunk;
        }
    }

    FilterSettings getCurrentFilterSettings() const { return currentFilter; }

private:
    void renderChunk (float* out, int numSamples)
    {
        const auto& s = settings;

        for (auto& v : voices)
            v.amp.setParameters (s.ampAttack, s.ampDecay, s.ampSustain, s.ampRelease);
        filterEnv.setParameters (s.filterAttack, s.filterDecay, s.filterSustain, s.filterRelease);

        // The filter envelope and key tracking move the barrier height.
        const float octaves = s.envAmountOctaves * filterEnv.getLevel()
                              + s.keyTrack * (float) (lastNote - 60) / 12.0f;
        currentFilter = s.filter;
        currentFilter.barrierHz = std::clamp (s.filter.barrierHz * std::exp2 ((double) octaves), 10.0, 20000.0);
        filter.setSettings (currentFilter);

        const double osc2Ratio = std::exp2 ((s.osc2Semitones + s.osc2Cents / 100.0) / 12.0);

        std::array<double, numVoices> inc1 {}, inc2 {};
        for (size_t i = 0; i < voices.size(); ++i)
        {
            inc1[i] = 440.0 * std::exp2 ((voices[i].note - 69) / 12.0) / sampleRate;
            inc2[i] = std::min (inc1[i] * osc2Ratio, 0.49);
        }

        for (int n = 0; n < numSamples; ++n)
        {
            float sum = 0.0f;

            for (size_t i = 0; i < voices.size(); ++i)
            {
                auto& v = voices[i];
                if (! v.amp.isActive())
                    continue;

                const float a = v.osc1.next (s.osc1Wave, inc1[i]);
                const float b = v.osc2.next (s.osc2Wave, inc2[i]);
                sum += (a + (b - a) * s.oscMix) * v.amp.next() * v.velocity;
            }

            filterEnv.next();
            out[n] = sum * 0.25f;
        }

        filter.process (out, numSamples);

        const float gain = std::pow (10.0f, s.outputGainDb / 20.0f);
        for (int n = 0; n < numSamples; ++n)
            out[n] *= gain;
    }

    double sampleRate = 48000.0;
    SynthSettings settings;
    std::array<Voice, numVoices> voices;
    Adsr filterEnv;
    TunnelFilter filter;
    FilterSettings currentFilter;
    uint64_t noteCounter = 0;
    int lastNote = 60;
};

} // namespace qs
