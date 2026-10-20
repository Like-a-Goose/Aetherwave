// Checks the quantum DSP core without JUCE, and renders a short demo WAV.
// Usage: qs_dsp_tests [demo.wav]

#include "../Source/dsp/SynthEngine.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace qs;

static int failures = 0;

static void check (bool ok, const char* what, double value)
{
    std::printf ("%s  %-58s %.6g\n", ok ? "PASS" : "FAIL", what, value);
    if (! ok)
        ++failures;
}

static void testPhysics()
{
    // Probability is conserved: |t|^2 + |r|^2 = 1 for every energy and shape.
    double worst = 0.0;
    for (auto shape : { BarrierShape::single, BarrierShape::twin })
        for (double e = 0.01; e < 50.0; e *= 1.07)
        {
            BarrierGeometry g;
            g.shape = shape;
            g.width = 4.0;
            g.gap = 6.0;
            const auto s = scatter (e, g);
            worst = std::max (worst, std::abs (s.transmitted * s.transmitted + s.reflected * s.reflected - 1.0));
        }
    check (worst < 1.0e-9, "|t|^2 + |r|^2 == 1 (unitarity)", worst);

    // Single barrier matches the textbook closed form.
    double worstAnalytic = 0.0;
    BarrierGeometry g;
    g.width = 2.5;
    for (double e : { 0.1, 0.4, 0.8, 1.3, 2.0, 7.0 })
    {
        const double w = g.width;
        const double T = e < 1.0
                             ? 1.0 / (1.0 + std::pow (std::sinh (w * std::sqrt (1.0 - e)), 2) / (4.0 * e * (1.0 - e)))
                             : 1.0 / (1.0 + std::pow (std::sin (w * std::sqrt (e - 1.0)), 2) / (4.0 * e * (e - 1.0)));
        const double t = scatter (e, g).transmitted;
        worstAnalytic = std::max (worstAnalytic, std::abs (t * t - T));
    }
    check (worstAnalytic < 1.0e-9, "single barrier matches analytic T(E)", worstAnalytic);

    // A symmetric twin barrier has perfect resonant tunnelling somewhere below the barrier.
    BarrierGeometry twin;
    twin.shape = BarrierShape::twin;
    twin.width = 3.0;
    twin.gap = 6.0;
    double peak = 0.0;
    for (double e = 0.01; e < 0.99; e += 1.0e-5)
        peak = std::max (peak, scatter (e, twin).transmitted);
    check (peak > 0.99, "twin barrier reaches ~100% resonant tunnelling", peak);
}

static void testFilterMatchesPhysics()
{
    const double sr = 48000.0;
    TunnelFilter filter;
    filter.prepare (sr);

    FilterSettings s;
    s.mode = FilterMode::reflect;
    s.barrierHz = 1000.0;
    s.width = 3.0;
    filter.setSettings (s);

    // Let the crossfade from the default settings finish before measuring.
    std::vector<float> silence (256, 0.0f);
    filter.process (silence.data(), (int) silence.size());

    std::vector<float> impulse (TunnelFilter::numTaps, 0.0f);
    impulse[0] = 1.0f;
    filter.process (impulse.data(), (int) impulse.size());

    // Compare the FIR's actual magnitude to the physics at a few frequencies.
    double worstDb = 0.0;
    for (double f : { 200.0, 500.0, 1500.0, 3000.0, 8000.0 })
    {
        std::complex<double> h = 0.0;
        for (size_t n = 0; n < impulse.size(); ++n)
            h += (double) impulse[n] * std::polar (1.0, -2.0 * 3.14159265358979323846 * f * (double) n / sr);
        const double target = tunnelResponse (f, s);
        worstDb = std::max (worstDb, std::abs (20.0 * std::log10 (std::abs (h) / target)));
    }
    check (worstDb < 1.5, "FIR response within 1.5 dB of physics (reflect mode)", worstDb);

    // Minimum phase: most of the energy arrives in the first few milliseconds.
    double early = 0.0, total = 0.0;
    for (size_t n = 0; n < impulse.size(); ++n)
    {
        total += impulse[n] * impulse[n];
        if (n < 96)
            early += impulse[n] * impulse[n];
    }
    check (early / total > 0.9, "energy in first 2 ms (minimum phase, no latency)", early / total);
}

static void writeWav (const char* path, const std::vector<float>& audio, int sr)
{
    std::ofstream f (path, std::ios::binary);
    auto u32 = [&] (uint32_t v) { f.write ((const char*) &v, 4); };
    auto u16 = [&] (uint16_t v) { f.write ((const char*) &v, 2); };
    const uint32_t bytes = (uint32_t) audio.size() * 2;
    f.write ("RIFF", 4); u32 (36 + bytes); f.write ("WAVEfmt ", 8);
    u32 (16); u16 (1); u16 (1); u32 ((uint32_t) sr); u32 ((uint32_t) sr * 2); u16 (2); u16 (16);
    f.write ("data", 4); u32 (bytes);
    for (float x : audio)
        u16 ((uint16_t) (int16_t) std::lround (std::clamp (x, -1.0f, 1.0f) * 32767.0f));
}

static void renderDemo (const char* path)
{
    const int sr = 48000;
    SynthEngine engine;
    engine.prepare (sr);

    struct Part { FilterMode mode; double barrier, width, gap; float env; };
    const Part parts[] = {
        { FilterMode::reflect, 400.0, 3.0, 6.0, 3.0f },
        { FilterMode::tunnel, 300.0, 5.0, 6.0, 2.0f },
        { FilterMode::resonantTunnel, 2500.0, 2.5, 7.0, 1.5f },
        { FilterMode::resonantReflect, 600.0, 2.0, 5.0, 3.0f },
    };
    const int chord[] = { 48, 55, 60, 63 };

    std::vector<float> audio;
    std::vector<float> block (512);
    double peak = 0.0;

    for (const auto& p : parts)
    {
        SynthSettings s;
        s.filter.mode = p.mode;
        s.filter.barrierHz = p.barrier;
        s.filter.width = p.width;
        s.filter.gap = p.gap;
        s.envAmountOctaves = p.env;
        engine.setSettings (s);

        for (int step = 0; step < 4; ++step)
        {
            engine.noteOn (chord[step], 0.8f);
            engine.noteOn (chord[(step + 2) % 4] + 12, 0.6f);
            for (int b = 0; b < sr / 2 / 512; ++b)
            {
                engine.render (block.data(), 512);
                audio.insert (audio.end(), block.begin(), block.end());
            }
            engine.allNotesOff();
            for (int b = 0; b < sr / 8 / 512; ++b)
            {
                engine.render (block.data(), 512);
                audio.insert (audio.end(), block.begin(), block.end());
            }
        }
    }

    for (float x : audio)
        peak = std::max (peak, (double) std::abs (x));

    check (peak > 0.01 && peak < 1.0, "demo renders audible audio without clipping (peak)", peak);
    writeWav (path, audio, sr);
    std::printf ("wrote %s (%.1f s)\n", path, audio.size() / (double) sr);
}

static void benchmark()
{
    const double sr = 48000.0;
    SynthEngine engine;
    engine.prepare (sr);
    SynthSettings s;
    s.filter.mode = FilterMode::resonantTunnel;
    s.filterDecay = 5.0f;  // keep the filter sweeping so it is redesigned constantly
    engine.setSettings (s);
    for (int i = 0; i < 8; ++i)
        engine.noteOn (48 + i * 3, 0.7f);

    std::vector<float> block (512);
    const int blocks = (int) (sr * 5.0 / 512);
    const auto start = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
        engine.render (block.data(), 512);
    const double seconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();

    check (seconds / 5.0 < 0.2, "CPU per realtime second, 8 voices, sweeping filter", seconds / 5.0);
}

int main (int argc, char** argv)
{
    testPhysics();
    testFilterMatchesPhysics();
    benchmark();
    renderDemo (argc > 1 ? argv[1] : "quantum-demo.wav");
    std::printf ("%s\n", failures == 0 ? "All checks passed." : "Some checks FAILED.");
    return failures == 0 ? 0 : 1;
}
