#pragma once

#include "MinPhaseFir.h"
#include "QuantumBarrier.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace qs
{

enum class FilterMode
{
    tunnel,            // keep what tunnels through one barrier: high-pass with ripples
    reflect,           // keep what bounces off one barrier: low-pass with notches
    resonantTunnel,    // twin barrier, transmitted: resonant peaks below the barrier
    resonantReflect,   // twin barrier, reflected: resonant notches

    softTunnel,        // smooth sech^2 barrier: a clean high-pass, width sets the slope
    step,              // potential step: nothing below the step, a sharp edge above it
    well,              // deep square well, reflected: a low-pass with Ramsauer-Townsend notches
    crystal,           // five barriers in a row, transmitted: allowed energy bands
    bandGap,           // the same crystal, reflected: only the band gaps come through

    box,               // particle in a box: inharmonic lines at n^2 times the barrier
    oscillator,        // harmonic oscillator: lines on the odd harmonics of half the barrier
    hydrogen           // Rydberg series crowding up to the barrier, everything above passes
};

constexpr int numFilterModes = 12;

inline bool isSpectral (FilterMode m)
{
    return m == FilterMode::box || m == FilterMode::oscillator || m == FilterMode::hydrogen;
}

inline bool usesGap (FilterMode m)
{
    return m == FilterMode::resonantTunnel || m == FilterMode::resonantReflect
           || m == FilterMode::crystal || m == FilterMode::bandGap;
}

// The scattering potential behind a mode (not used by the soft barrier or the spectral modes).
inline Potential potentialFor (FilterMode mode, double width, double gap)
{
    BarrierGeometry g;
    g.width = width;
    g.gap = gap;
    switch (mode)
    {
        case FilterMode::resonantTunnel:
        case FilterMode::resonantReflect: g.shape = BarrierShape::twin; break;
        case FilterMode::crystal:
        case FilterMode::bandGap:         g.shape = BarrierShape::lattice; break;
        case FilterMode::well:            g.shape = BarrierShape::well; break;
        case FilterMode::step:            g.shape = BarrierShape::step; break;
        case FilterMode::tunnel:
        case FilterMode::reflect:
        case FilterMode::softTunnel:
        case FilterMode::box:
        case FilterMode::oscillator:
        case FilterMode::hydrogen:        g.shape = BarrierShape::single; break;
    }
    return makePotential (g);
}

// Steepness of the smooth barrier: V(x) = sech^2(alpha x), about as wide as Width.
inline double softAlpha (double width) { return 2.0 / width; }

// Spectral line width for the bound modes: thicker means longer-lived states, sharper lines.
inline double lineWidth (double width) { return std::clamp (0.25 / width, 0.03, 0.5); }

struct FilterSettings
{
    double barrierHz = 800.0;
    double width = 3.0;
    double gap = 6.0;
    FilterMode mode = FilterMode::reflect;

    bool isCloseTo (const FilterSettings& o) const
    {
        auto near = [] (double a, double b) { return std::abs (a - b) <= 1.0e-4 * std::abs (b); };
        return mode == o.mode && near (barrierHz, o.barrierHz) && near (width, o.width) && near (gap, o.gap);
    }
};

// Magnitude response of the quantum filter at a given frequency.
inline double tunnelResponse (double frequencyHz, const FilterSettings& s)
{
    const double energy = frequencyHz / s.barrierHz;

    switch (s.mode)
    {
        case FilterMode::softTunnel:
            return scatterSoft (energy, softAlpha (s.width)).transmitted;

        case FilterMode::box:
        case FilterMode::oscillator:
        case FilterMode::hydrogen:
        {
            // A faint background keeps the gaps between lines from going completely silent.
            const auto kind = s.mode == FilterMode::box ? Spectrum::box
                            : s.mode == FilterMode::oscillator ? Spectrum::oscillator : Spectrum::hydrogen;
            constexpr double floor = 0.02;
            return floor + (1.0 - floor) * spectrum (energy, kind, lineWidth (s.width));
        }

        case FilterMode::reflect:
        case FilterMode::resonantReflect:
        case FilterMode::well:  // the Ramsauer-Townsend effect shows as gaps in what bounces back
        case FilterMode::bandGap:
            return scatter (energy, potentialFor (s.mode, s.width, s.gap)).reflected;

        case FilterMode::tunnel:
        case FilterMode::resonantTunnel:
        case FilterMode::step:
        case FilterMode::crystal:
            break;
    }
    return scatter (energy, potentialFor (s.mode, s.width, s.gap)).transmitted;
}

// Applies the quantum barrier's response to audio as a zero-latency minimum-phase FIR.
// When the settings change, the new FIR is crossfaded in over the next processed chunk.
class TunnelFilter
{
public:
    static constexpr int fftOrder = 10;
    static constexpr int numTaps = 384;

    TunnelFilter()
        : designer (fftOrder), magnitudes ((size_t) designer.getNumBins()),
          history ((size_t) numTaps * 2, 0.0f)
    {
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        reset();
        minSamplesBetweenTables = (int) (sampleRate / maxTablesPerSecond);
        tableValid = false;
        designFor (settings);
        samplesSinceTable = minSamplesBetweenTables;
        previous = current;
    }

    void reset()
    {
        std::fill (history.begin(), history.end(), 0.0f);
        position = 0;
    }

    struct Pitch
    {
        double hz;
        double weight;  // relative level of this oscillator
    };
    static constexpr int maxPitches = 16;

    // What the level make-up is judged on: the oscillator pitches playing and the barrier height
    // set on the panel, before the envelope, LFO and joystick move it. Those still sweep the level
    // as much as ever. Takes effect with the next setSettings call.
    void setLevelReference (const Pitch* newPitches, int count, double barrierHz)
    {
        auto near = [] (double x, double y) { return std::abs (x - y) <= 1.0e-3 * std::abs (y); };
        count = std::min (count, maxPitches);
        bool same = count == numPitches && near (barrierHz, referenceBarrierHz);
        for (int j = 0; same && j < count; ++j)
            same = near (newPitches[j].hz, pitches[(size_t) j].hz) && near (newPitches[j].weight, pitches[(size_t) j].weight);
        if (same)
            return;

        std::copy (newPitches, newPitches + count, pitches.begin());
        numPitches = count;
        referenceBarrierHz = barrierHz;
        makeupDirty = needsDesign = true;
    }

    // The level boost in the current FIR, as a gain factor.
    double getMakeupGain() const { return makeup; }

    void setSettings (const FilterSettings& newSettings)
    {
        const bool deferredShapeDue = shapePending && samplesSinceTable >= minSamplesBetweenTables;
        if (newSettings.isCloseTo (settings) && ! needsDesign && ! deferredShapeDue)
            return;

        // If the last change has not been heard yet, keep fading from what is actually playing.
        if (! crossfadePending)
            previous = current;

        designFor (newSettings);
        crossfadePending = true;
    }

    void process (float* samples, int numSamples)
    {
        const bool fading = crossfadePending;
        crossfadePending = false;
        samplesSinceTable += numSamples;

        for (int n = 0; n < numSamples; ++n)
        {
            position = (position == 0 ? numTaps : position) - 1;
            history[(size_t) position] = samples[n];
            history[(size_t) (position + numTaps)] = samples[n];

            const float* window = history.data() + position;
            float out = dot (current.data(), window);

            if (fading)
            {
                const float old = dot (previous.data(), window);
                const float ramp = (float) (n + 1) / (float) numSamples;
                out = old + (out - old) * ramp;
            }

            samples[n] = out;
        }
    }

private:
    // Eight independent partial sums so the compiler can vectorise without -ffast-math.
    static float dot (const float* a, const float* b)
    {
        static_assert (numTaps % 8 == 0, "numTaps must be a multiple of 8");
        float sums[8] = {};
        for (int j = 0; j < numTaps; j += 8)
            for (int lane = 0; lane < 8; ++lane)
                sums[lane] += a[j + lane] * b[j + lane];
        return ((sums[0] + sums[1]) + (sums[2] + sums[3])) + ((sums[4] + sums[5]) + (sums[6] + sums[7]));
    }

    void designFor (const FilterSettings& s)
    {
        settings = s;
        needsDesign = false;

        // The response only depends on f / barrierHz, so sweeping the barrier just reads a
        // table built for the current width, gap and mode. Solving the physics per bin on
        // every envelope step would cost several times more CPU.
        // A knob turned on a controller (RESONANCE on a minilogue xd, say) changes the shape on
        // every step, so rebuilds are capped; in between, the last shape stays in use.
        shapePending = false;
        if (! tableValid || ! sameShape (s, tableSettings))
        {
            if (! tableValid || samplesSinceTable >= minSamplesBetweenTables)
            {
                buildTable (s);
                samplesSinceTable = 0;
                makeupDirty = true;
            }
            else
            {
                shapePending = true;
            }
        }

        const int fftSize = designer.getFftSize();
        const double binEnergy = sampleRate / fftSize / s.barrierHz;
        const bool lines = isSpectral (s.mode);
        for (int k = 0; k < designer.getNumBins(); ++k)
        {
            double m = lookup (k * binEnergy);

            // Spectral lines can be narrower than an FIR bin and would fall between bins;
            // taking the strongest point across each bin keeps every line audible.
            if (lines)
                for (double offset : { -0.375, -0.25, -0.125, 0.125, 0.25, 0.375 })
                    m = std::max (m, lookup (std::max (0.0, (k + offset) * binEnergy)));

            magnitudes[(size_t) k] = m;
        }

        if (makeupDirty)
        {
            makeup = makeupGain();
            makeupDirty = false;
        }
        for (auto& m : magnitudes)
            m *= makeup;

        designer.design (magnitudes.data(), current.data(), numTaps);
    }

    static constexpr int tableSize = 2048;
    static constexpr double maxTablesPerSecond = 50.0;
    static constexpr double tableMinEnergy = 1.0e-3, tableMaxEnergy = 4.0e3;

    void buildTable (const FilterSettings& s)
    {
        tableSettings = s;
        tableSettings.barrierHz = 1.0;
        tableValid = true;

        const double step = std::log (tableMaxEnergy / tableMinEnergy) / (tableSize - 1);
        for (int j = 0; j < tableSize; ++j)
            table[(size_t) j] = tunnelResponse (tableMinEnergy * std::exp (j * step), tableSettings);
    }

    // Many shapes pass only a small part of a note's harmonics (a high-pass with the barrier above
    // the note, the narrow lines of the bound modes), so they sound much quieter than the others.
    // This estimates how much of the playing notes gets through, treating each as a sawtooth, and
    // makes up most of the loss. Only most of it, so turning the barrier still opens and closes
    // the sound; and never more than the cap, or the gaps between spectral lines would turn to hiss.
    double makeupGain() const
    {
        if (numPitches == 0)
            return makeup;  // nothing playing: keep the last level

        constexpr int harmonics = 48;
        constexpr double strength = 0.75, maxBoostDb = 18.0;
        const double topHz = std::min (16000.0, 0.45 * sampleRate);
        const double binEnergy = sampleRate / designer.getFftSize() / referenceBarrierHz;

        double passed = 0.0, total = 0.0;
        for (int j = 0; j < numPitches; ++j)
        {
            const auto& p = pitches[(size_t) j];
            for (int n = 1; n <= harmonics && n * p.hz < topHz; ++n)
            {
                const double h = binResponse (n * p.hz / referenceBarrierHz, binEnergy);
                const double weight = p.weight / ((double) n * n);  // sawtooth harmonic power
                passed += weight * h * h;
                total += weight;
            }
        }
        if (total <= 0.0)
            return makeup;

        const double lossDb = 10.0 * std::log10 (total / std::max (passed, 1.0e-12));
        return std::pow (10.0, std::clamp (strength * lossDb, 0.0, maxBoostDb) / 20.0);
    }

    // Roughly what the FIR does at this energy: it cannot resolve detail finer than a bin, so
    // average across one (or, for line spectra, take the strongest point, as designFor does).
    double binResponse (double energy, double binEnergy) const
    {
        const bool lines = isSpectral (tableSettings.mode);
        double sum = 0.0, strongest = 0.0;
        for (double offset : { -0.5, -0.25, 0.0, 0.25, 0.5 })
        {
            const double m = lookup (std::max (0.0, energy + offset * binEnergy));
            sum += m * m;
            strongest = std::max (strongest, m);
        }
        return lines ? strongest : std::sqrt (sum / 5.0);
    }

    double lookup (double energy) const
    {
        if (energy < tableMinEnergy || energy >= tableMaxEnergy)
            return tunnelResponse (energy, tableSettings);

        const double index = std::log (energy / tableMinEnergy) / std::log (tableMaxEnergy / tableMinEnergy) * (tableSize - 1);
        const int j = std::min ((int) index, tableSize - 2);
        const double frac = index - j;
        return table[(size_t) j] + (table[(size_t) j + 1] - table[(size_t) j]) * frac;
    }

    static bool sameShape (const FilterSettings& a, const FilterSettings& b)
    {
        auto near = [] (double x, double y) { return std::abs (x - y) <= 1.0e-6 * std::abs (y); };
        return a.mode == b.mode && near (a.width, b.width) && (near (a.gap, b.gap) || ! usesGap (a.mode));
    }

    std::array<double, tableSize> table {};
    FilterSettings tableSettings;
    bool tableValid = false;
    double makeup = 1.0;
    std::array<Pitch, maxPitches> pitches {};
    int numPitches = 0;
    double referenceBarrierHz = 800.0;
    bool makeupDirty = true;
    bool shapePending = false;  // width, gap or mode changed but the table has not caught up yet
    int samplesSinceTable = 0;
    int minSamplesBetweenTables = 960;

    MinPhaseDesigner designer;
    std::vector<double> magnitudes;
    std::array<float, numTaps> current {}, previous {};
    std::vector<float> history;
    int position = 0;

    double sampleRate = 48000.0;
    FilterSettings settings;
    bool needsDesign = true;
    bool crossfadePending = false;
};

} // namespace qs
