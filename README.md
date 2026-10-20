# Quantum Synth

A subtractive synth whose filter is a quantum tunnelling barrier. Built with JUCE as a
VST3 and a standalone app, Linux first (tested target: Reaper).

## How the quantum filter works

Each frequency in the sound is treated as a particle with energy E = h·f that hits a
potential barrier. The **Barrier** knob sets the barrier height as a frequency.

- Frequencies above the barrier mostly pass, frequencies below it have to tunnel through,
  and how much gets through is the exact Schrödinger-equation result (transfer matrices
  for a piecewise-constant potential, see `Source/dsp/QuantumBarrier.h`).
- The resulting magnitude response is turned into a minimum-phase FIR, so the filter adds
  no latency and can be played live (`Source/dsp/MinPhaseFir.h`, `TunnelFilter.h`).

| Mode | Physics | Sounds like |
| --- | --- | --- |
| Tunnel | what gets through one barrier | high-pass with ripples above the cutoff |
| Reflect | what bounces back off one barrier | low-pass with notches (transmission resonances) |
| Resonant Tunnel | two barriers, transmitted | resonant peaks below the barrier (resonant tunnelling) |
| Resonant Reflect | two barriers, reflected | low-pass with sharp resonant notches |

**Width** is the barrier thickness: thicker means steeper and more resonant. **Gap** is the
space between the two barriers in the resonant modes and moves the resonant peaks.
The filter envelope and key tracking move the barrier height in octaves.

The layout is paraphonic: 8 voices with two oscillators (saw, square, triangle, sine) and an
amp envelope each, all summed into one shared quantum filter. The filter envelope retriggers
on every new note.

## Build on Linux

Ubuntu/Debian packages:

```sh
sudo apt install build-essential cmake ninja-build git pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libgl1-mesa-dev
```

Build (JUCE 8.0.4 is downloaded automatically):

```sh
cmake -B build -G Ninja
cmake --build build
```

To use a JUCE checkout you already have: `cmake -B build -G Ninja -DQS_JUCE_DIR=/path/to/JUCE`.

Outputs:

- VST3: `build/QuantumSynth_artefacts/Release/VST3/Quantum Synth.vst3`, also copied to `~/.vst3/`
- Standalone: `build/QuantumSynth_artefacts/Release/Standalone/Quantum Synth`

In Reaper, open Preferences, Plug-ins, VST, make sure `~/.vst3` is in the path list and
click "Re-scan". Quantum Synth then shows up under instruments.

The standalone app supports ALSA and JACK (PipeWire's JACK layer works too). Pick the
audio and MIDI devices from the Options button. The on-screen keyboard plays notes too.

## Tests

`qs_dsp_tests` checks the physics and DSP without JUCE: probability conservation
(|t|² + |r|² = 1), the single-barrier result against the textbook formula, perfect resonant
tunnelling for the twin barrier, the FIR matching the physics, zero latency, and CPU use.
It also renders `quantum-demo.wav` through all four filter modes.

```sh
cd build && ctest --output-on-failure
```

## Licence note

JUCE is dual-licensed (AGPLv3 or a commercial licence). If you release this as closed
source you need a JUCE licence; open-sourcing it under the AGPL also works.
