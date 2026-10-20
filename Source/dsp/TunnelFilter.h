#pragma once

#include "MinPhaseFir.h"
#include "QuantumBarrier.h"

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
    resonantReflect    // twin barrier, reflected: resonant notches
};

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
    BarrierGeometry geometry;
    geometry.width = s.width;
    geometry.gap = s.gap;
    geometry.shape = (s.mode == FilterMode::resonantTunnel || s.mode == FilterMode::resonantReflect)
                         ? BarrierShape::twin
                         : BarrierShape::single;

    const auto result = scatter (frequencyHz / s.barrierHz, geometry);
    const bool transmitted = (s.mode == FilterMode::tunnel || s.mode == FilterMode::resonantTunnel);
    return transmitted ? result.transmitted : result.reflected;
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
            }
            else
            {
                shapePending = true;
            }
        }

        const int fftSize = designer.getFftSize();
        for (int k = 0; k < designer.getNumBins(); ++k)
            magnitudes[(size_t) k] = lookup (k * sampleRate / fftSize / s.barrierHz);

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
        return a.mode == b.mode && near (a.width, b.width) && near (a.gap, b.gap);
    }

    std::array<double, tableSize> table {};
    FilterSettings tableSettings;
    bool tableValid = false;
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
