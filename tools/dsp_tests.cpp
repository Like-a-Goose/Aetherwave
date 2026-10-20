// Checks the quantum DSP core without JUCE, and renders a short demo WAV.
// Usage: qs_dsp_tests [demo.wav]

#include "../Source/dsp/MinilogueXd.h"
#include "../Source/dsp/SynthEngine.h"

#include <chrono>
#include <complex>
#include <cstdio>
#include <fstream>
#include <string>
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

static void testMinilogueXd()
{
    xd::Decoder decoder;

    // A 10-bit knob: CC63 carries the low 3 bits right before the knob's own CC.
    decoder.handle (63, 5);
    auto cutoff = decoder.handle (43, 64);
    check (cutoff && std::string (cutoff->control->paramId) == "barrier" && cutoff->normalised
               && std::abs (cutoff->value - ((64 << 3) | 5) / 1023.0f) < 1.0e-6f,
           "xd CUTOFF with CC63 decodes to 10 bits", cutoff ? cutoff->value : -1.0);

    // The low bits only belong to the next CC.
    auto resonance = decoder.handle (44, 127);
    check (resonance && std::abs (resonance->value - (127 << 3) / 1023.0f) < 1.0e-6f,
           "xd low bits are not reused by the following knob", resonance ? resonance->value : -1.0);

    // Full travel reaches both ends.
    decoder.handle (63, 7);
    auto top = decoder.handle (16, 127);
    auto bottom = decoder.handle (16, 0);
    check (top && bottom && top->value == 1.0f && bottom->value == 0.0f, "xd knob covers 0..1", top ? top->value : -1.0);

    // Switches.
    auto saw = decoder.handle (50, 127), sqr = decoder.handle (50, 0), tri = decoder.handle (51, 64);
    check (saw && sqr && tri && saw->value == 0.0f && sqr->value == 1.0f && tri->value == 2.0f,
           "xd VCO WAVE SAW/SQR/TRI pick Saw/Square/Triangle", saw ? saw->value : -1.0);

    bool modesOk = true;
    const int octavePositions[] = { 0, 42, 84, 127 };
    for (int i = 0; i < 4; ++i)
    {
        auto mode = decoder.handle (48, octavePositions[i]);
        modesOk = modesOk && mode && std::string (mode->control->paramId) == "filterMode" && mode->value == (float) i;
    }
    check (modesOk, "xd VCO1 OCTAVE picks the four filter modes", modesOk);

    auto down = decoder.handle (49, 0), upTwo = decoder.handle (49, 127);
    check (down && upTwo && down->value == -12.0f && upTwo->value == 24.0f, "xd VCO2 OCTAVE 16'/2' gives -12/+24 semitones",
           upTwo ? upTwo->value : -1.0);

    auto half = decoder.handle (83, 64);
    check (half && half->value == 0.5f, "xd KEY TRACK 50% gives 0.5", half ? half->value : -1.0);

    // VCO levels set the oscillator mix.
    decoder.handle (39, 127);
    auto onlyOsc1 = decoder.handle (40, 0);
    auto equal = decoder.handle (40, 127);
    check (onlyOsc1 && equal && onlyOsc1->value == 0.0f && std::abs (equal->value - 0.5f) < 1.0e-6f,
           "xd VCO1/VCO2 LEVEL set the oscillator mix", equal ? equal->value : -1.0);

    check (! decoder.handle (7, 100) && ! decoder.handle (1, 100), "xd unmapped CCs are ignored", 0.0);
}

static double rms (const std::vector<float>& x)
{
    double sum = 0.0;
    for (float v : x)
        sum += (double) v * v;
    return std::sqrt (sum / (double) x.size());
}

// Zero crossings per second of a single sine voice: about twice its frequency.
static double measureFrequency (SynthEngine& engine, int sr)
{
    std::vector<float> block ((size_t) sr / 2);
    engine.render (block.data(), (int) block.size());
    int crossings = 0;
    for (size_t n = 1; n < block.size(); ++n)
        crossings += (block[n - 1] < 0.0f) != (block[n] < 0.0f);
    return crossings / 2.0 / 0.5;
}

static void testPerformanceControls()
{
    const int sr = 48000;
    SynthSettings s;
    s.osc1Wave = s.osc2Wave = Waveform::sine;
    s.osc2Cents = 0.0f;
    s.filter.mode = FilterMode::tunnel;
    s.filter.barrierHz = 20.0;  // let the sine through
    s.envAmountOctaves = 0.0f;
    s.keyTrack = 0.0f;
    s.ampRelease = 0.01f;

    SynthEngine engine;
    engine.prepare (sr);
    engine.setSettings (s);
    engine.noteOn (69, 1.0f);
    const double plain = measureFrequency (engine, sr);
    engine.setPitchBend (1.0f);
    const double bent = measureFrequency (engine, sr);
    check (std::abs (bent / plain - std::exp2 (2.0 / 12.0)) < 0.01, "full pitch bend raises 2 semitones", bent / plain);

    // Damper pedal holds released notes until it comes up.
    std::vector<float> block (4800);
    engine.setPitchBend (0.0f);
    engine.setSustainPedal (true);
    engine.noteOff (69);
    engine.render (block.data(), (int) block.size());
    engine.render (block.data(), (int) block.size());
    const double held = rms (block);
    engine.setSustainPedal (false);
    engine.render (block.data(), (int) block.size());
    engine.render (block.data(), (int) block.size());
    check (held > 0.05 && rms (block) < 1.0e-4, "damper pedal sustains, then releases", held);

    // Joystick Y- pulls the barrier down. In Tunnel mode that lets more of a note below it through.
    s.filter.barrierHz = 2000.0;
    engine.setSettings (s);
    engine.noteOn (57, 1.0f);  // 220 Hz, well below the barrier
    engine.render (block.data(), (int) block.size());
    engine.render (block.data(), (int) block.size());
    const double closed = rms (block);
    engine.setBarrierDrop (1.0f);
    engine.render (block.data(), (int) block.size());
    engine.render (block.data(), (int) block.size());
    check (rms (block) > 2.0 * closed, "joystick Y- lowers the barrier", rms (block) / closed);
    engine.setBarrierDrop (0.0f);
    engine.render (block.data(), (int) block.size());

    // The mod wheel (joystick Y+) adds LFO depth, which wobbles the barrier.
    s.lfoRate = 3.0f;
    s.lfoTarget = LfoTarget::barrier;
    engine.setSettings (s);
    auto spread = [&]
    {
        double lo = 1.0e9, hi = 0.0;
        for (int i = 0; i < 40; ++i)
        {
            std::vector<float> chunk (1200);
            engine.render (chunk.data(), (int) chunk.size());
            lo = std::min (lo, rms (chunk));
            hi = std::max (hi, rms (chunk));
        }
        return hi / lo;
    };
    const double still = spread();
    engine.setModWheel (1.0f);
    const double wobbling = spread();
    std::printf ("      level spread without / with mod wheel: %.3f / %.3f\n", still, wobbling);
    check (still < 1.1 && wobbling > 2.0, "mod wheel brings in the LFO on the barrier", wobbling);
}

// A Width change that arrives right after another one is held back, but must still land.
static void testDeferredShapeLands()
{
    const double sr = 48000.0;
    TunnelFilter filter;
    filter.prepare (sr);

    FilterSettings s;
    s.mode = FilterMode::resonantReflect;
    s.barrierHz = 1000.0;
    s.width = 2.0;
    std::vector<float> chunk (128, 0.0f);
    filter.setSettings (s);
    filter.process (chunk.data(), (int) chunk.size());

    s.width = 5.0;  // too soon after the last rebuild: deferred
    for (int i = 0; i < 20; ++i)
    {
        filter.setSettings (s);
        filter.process (chunk.data(), (int) chunk.size());
    }

    std::vector<float> impulse (TunnelFilter::numTaps, 0.0f);
    impulse[0] = 1.0f;
    filter.process (impulse.data(), (int) impulse.size());

    double worstDb = 0.0;
    for (double f : { 300.0, 700.0, 2000.0, 6000.0 })
    {
        std::complex<double> h = 0.0;
        for (size_t n = 0; n < impulse.size(); ++n)
            h += (double) impulse[n] * std::polar (1.0, -2.0 * 3.14159265358979323846 * f * (double) n / sr);
        worstDb = std::max (worstDb, std::abs (20.0 * std::log10 (std::abs (h) / std::max (1.0e-3, tunnelResponse (f, s)))));
    }
    check (worstDb < 1.5, "deferred Width change reaches the filter within 50 ms", worstDb);
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

    // Turning the minilogue xd's RESONANCE knob changes Width on every control step, which
    // rebuilds the physics table each time: the worst case for a knob sweep.
    const auto knobStart = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        s.filter.width = 0.3 + 7.7 * (0.5 + 0.5 * std::sin (b * 0.05));
        engine.setSettings (s);
        for (int c = 0; c < 4; ++c)  // a new width for every 128-sample step
        {
            s.filter.width += 1.0e-3;
            engine.setSettings (s);
            engine.render (block.data() + c * 128, 128);
        }
    }
    const double knobSeconds = std::chrono::duration<double> (std::chrono::steady_clock::now() - knobStart).count();
    check (knobSeconds / 5.0 < 0.15, "CPU per realtime second, RESONANCE knob sweeping Width", knobSeconds / 5.0);
}

int main (int argc, char** argv)
{
    testPhysics();
    testFilterMatchesPhysics();
    testDeferredShapeLands();
    testMinilogueXd();
    testPerformanceControls();
    benchmark();
    renderDemo (argc > 1 ? argv[1] : "quantum-demo.wav");
    std::printf ("%s\n", failures == 0 ? "All checks passed." : "Some checks FAILED.");
    return failures == 0 ? 0 : 1;
}
