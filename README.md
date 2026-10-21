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
| Soft Tunnel | a smooth sech² (Pöschl–Teller) barrier, exact solution | clean high-pass, no ripples; Width sets the slope |
| Step | a potential step: below it the wave is evanescent | brick-wall high-pass at the barrier |
| Quantum Well | a deep square well, reflected (Ramsauer–Townsend effect) | low-pass with sharp notches where the well turns transparent |
| Crystal | five barriers in a row (a tiny Kronig–Penney crystal), transmitted | separate pass bands with band gaps between them |
| Band Gap | the same crystal, reflected | only the band gaps get through |
| Particle in a Box | infinite well, levels E = n² E₁ | inharmonic resonances at 1, 4, 9, 16 × the barrier (bell-like) |
| Harmonic Oscillator | levels E = (n + ½) ħω | resonances on the odd harmonics of half the barrier (hollow, clarinet-like) |
| Hydrogen | Rydberg series E = 1 − 1/n² and the ionisation continuum | resonances crowding up to the barrier, everything above it passes |

The scattering modes are solved exactly (transfer matrices, or the closed form for Soft
Tunnel). The last three are bound systems: their energy levels become spectral lines whose
sharpness (the state's lifetime) is set by **Width**.

**Width** is the barrier thickness: thicker means steeper and more resonant (in the bound
modes, sharper lines). **Gap** is the space between barriers in the Resonant, Crystal and
Band Gap modes and moves their peaks; it is dimmed when the mode doesn't use it.
The filter envelope and key tracking move the barrier height in octaves.

Modes like Tunnel, Crystal or the line spectra let through only a small part of a note's
harmonics, so the filter adds make-up gain: it looks at the notes being played and the
**Barrier** knob and makes up most (not all) of the level the mode takes away, up to 18 dB.
The envelope, LFO and joystick are left out of that, so their sweeps keep their full depth.
A soft clipper above −3 dBFS rounds off the occasional peak instead of hard-clipping.

The layout is paraphonic: 8 voices with two oscillators (saw, square, triangle, sine) and an
amp envelope each, all summed into one shared quantum filter. The filter envelope retriggers
on every new note. An LFO can move the barrier, the oscillator mix or the pitch.

## Playing from the computer keyboard

In the standalone app (and in a host that passes keys to plug-ins) the laptop keys play
notes whatever you clicked last: A S D F G H J K L ; ' are the white keys from C, W E T Y U
O P the black ones. **Z** and **X** move the octave down and up; it starts at C3 (middle C)
and is remembered. A bar over the on-screen keyboard shows which keys the laptop plays.

## Themes

**Theme** in the top right picks the look; the choice is remembered for every instance.

| Theme | Look |
| --- | --- |
| Quantum Night | the default dark blue |
| minilogue xd | black and silver, with the controls laid out like the minilogue xd's front panel |
| Paper Lab | light, like a printed physics diagram |
| Phosphor | green oscilloscope screen with an amber barrier |
| Synthwave | purple, pink and cyan |
| Graphite | warm greys with orange |

The minilogue xd layout puts each control where the xd knob or switch that moves it sits:
MASTER, then VCO 1 / VCO 2 / MULTI ENGINE rows, MIXER, FILTER, and AMP EG / EG / LFO rows,
labelled with the xd's panel names and what they do here underneath (VCO 1 OCTAVE, for
example, is the filter mode). Filter envelope sustain and release have no xd knob and sit at
the end of the EG row. Settings live in `~/.config/Quantum Synth/` on Linux.

## Presets

The bar at the top of the window browses the preset library. ◀ ▶ step through every preset;
clicking the name opens the full list by category. **Save** stores the current sound under a
name of your choice, and the name shows *(edited)* once a knob has moved since loading.

25 factory sounds come with it, each built around a different filter mode:

| Category | Presets |
| --- | --- |
| Bass | Barrier Bass, Well Sub, Resonant Wobble, Step Punch |
| Lead | Tunnel Lead, Soft Sync Lead, Crystal Lead, Hydrogen Scream |
| Keys | Box Keys, Oscillator Organ, Resonant EP |
| Pad | Band Gap Pad, Quantum Choir, Crystal Haze, Tunnel Strings |
| Pluck | Ramsauer Pluck, Twin Barrier Pluck, Tunnel Harp |
| Bell | Box Bells, Hydrogen Chimes, Oscillator Glass |
| FX | Tunnelling Sweep, Lattice Rain, Wavefunction Wah, Ionisation |

They are levelled to a similar loudness. Hosts list them as the plug-in's programs too (in
Reaper, the preset drop-down at the top of the FX window).

Your own presets are files in `~/.config/Quantum Synth/Presets/` on Linux (the menu's
*Show presets folder* opens it), one `.qspreset` XML file each, so they can be copied to
another machine or shared. Presets leave the CC map setting alone. Factory presets are defined
in `Source/dsp/FactoryPresets.h`.

## Playing it from a Korg minilogue xd

Quantum Synth understands a minilogue xd out of the box (the **CC map** menu is set to
"minilogue xd"; set it to "Off" if another controller's CCs clash). Its knobs move the
synth's parameters, so the on-screen knobs follow and Reaper can record them as automation.
The top right of the display names the last xd control you touched and what it moved.

On the minilogue xd, open the Global settings and set **MIDI Route** to USB+MIDI (or USB)
and **MIDI Tx CC** to On, otherwise the knobs send nothing. To hear only the plugin, turn the
xd's own volume down or switch its local control off.

| minilogue xd | Quantum Synth |
| --- | --- |
| Keyboard, velocity | notes, velocity |
| Joystick left/right | pitch bend (range set by the **Bend** knob, 2 semitones by default) |
| Joystick up (CC1) | adds LFO depth, like a mod wheel |
| Joystick down (CC2) | pulls the barrier down by up to 4 octaves |
| Damper pedal (CC64) | sustain |
| CUTOFF | Barrier height |
| RESONANCE | Barrier width |
| MULTI ENGINE SHAPE | Barrier gap (resonant modes) |
| MULTI TYPE (NOISE VPM USR) + VCO1 OCTAVE (16' 8' 4' 2') | Filter mode: MULTI TYPE picks a bank of four, VCO1 OCTAVE the mode in it. NOISE: Tunnel, Reflect, Resonant Tunnel, Resonant Reflect. VPM: Soft Tunnel, Step, Quantum Well, Crystal. USR: Band Gap, Particle in a Box, Harmonic Oscillator, Hydrogen |
| KEY TRACK (0% 50% 100%) | Key tracking |
| EG INT, EG ATTACK, EG DECAY | Filter envelope amount, attack, decay |
| AMP EG ATTACK, DECAY, SUSTAIN, RELEASE | Amp envelope |
| VCO1 WAVE, VCO2 WAVE | Osc 1 and Osc 2 wave (SQR, TRI, SAW) |
| VCO2 OCTAVE | Osc 2 transpose: -12, 0, +12, +24 semitones |
| VCO2 PITCH | Osc 2 fine tune |
| VCO1 LEVEL, VCO2 LEVEL | Oscillator mix |
| LFO RATE, INT, WAVE | LFO rate, amount, wave |
| LFO TARGET (CUTOFF, SHAPE, PITCH) | LFO target: barrier, oscillator mix, pitch |

The xd's continuous knobs are 10-bit: it sends the low bits on CC63 just before each knob's
CC, and Quantum Synth combines the two, so sweeps are smooth. Controls not in the table
(VCO shapes, cross mod, sync, ring, effects, the sequencer's own sounds) are ignored.

**Standalone app:** when a minilogue xd is plugged in, the app enables its MIDI input by
itself (only the xd's first, keyboard and knob, port, so notes are not doubled). You can still
change this in Options.

**Reaper:** in Preferences, MIDI Devices, enable the minilogue xd input. On the Quantum Synth
track, arm it, set the record input to the minilogue xd (or all MIDI inputs) and turn on
record monitoring. The knobs, joystick and pedal then work while playing and recording.

## Build on Linux

Ubuntu/Debian packages:

```sh
sudo apt install build-essential cmake ninja-build git pkg-config \
  libasound2-dev libjack-jackd2-dev libfreetype-dev libfontconfig1-dev \
  libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxcomposite-dev libgl1-mesa-dev
```

Build (JUCE 8.0.16 is downloaded automatically):

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

The window can be resized (by its frame, in Reaper and in the standalone app) from 75% to 200%;
everything, text included, scales with it.

## Tests

`qs_dsp_tests` checks the physics and DSP without JUCE: probability conservation
(|t|² + |r|² = 1), the single-barrier result against the textbook formula, perfect resonant
tunnelling for the twin barrier, the FIR matching the physics, zero latency, CPU use (also
while a controller knob sweeps the barrier width), the minilogue xd CC decoding, pitch bend,
the damper pedal and the joystick.
It also renders `quantum-demo.wav` through all four filter modes.

```sh
cd build && ctest --output-on-failure
```

## Licence note

JUCE is dual-licensed (AGPLv3 or a commercial licence). If you release this as closed
source you need a JUCE licence; open-sourcing it under the AGPL also works.
