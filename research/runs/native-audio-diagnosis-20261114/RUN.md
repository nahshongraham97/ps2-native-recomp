# Native audio diagnosis (Phase D, offline) — 2026-11-14

Offline diagnosis of the reported "no sound" against the recorded user run
`games/mkarmageddon/disc/logs/ps2run_2026-11-14_16-57-24.log`. No game was
launched; everything below comes from that log, the runtime source, the pinned
PS2SDK reference, and the new synthetic mixer test.

## What the recorded run proves

- `[audio] sound device FxSound Speakers (FxSound Audio Enhancer) (wasapi):
  48000 Hz, 2 channels` — the SDL/WASAPI device opened at the game's rate.
- End-of-run summary: `the device got 150.11 s of sound in 234.02 s; the
  queue ran dry 95 times (about 20.81 s of silence in between), 0 samples
  dropped` and `stretching: 150.09 s of game sound played as 213.24 s`.
- So 150 seconds of samples were produced and handed to the device; the 95
  "ran dry" events are a timing symptom (the game runs slower than real time
  than the stretcher can absorb), not an amplitude symptom. The pipeline is
  alive end to end; the silence is either sample content (digital zeros) or
  the output device not reaching the speakers.

## `missing libsd #28`

- `kernel.cpp hle_id_for` logs `[iop] missing libsd #28 called from
  0x000C3E18` exactly once per run (also in five earlier 2026-11-13 runs).
- The native LIBSD v1.5 bridge registers ordinals 4-27 and 30-33. The pinned
  PS2SDK `iop/sound/libsd/src/exports.tab` is libsd **1.4** and ends at 27;
  ordinals 28 and 29 are 1.5-only additions the bridge never implemented.
- The call happens once during startup and sound still flowed for 150 s, so
  it is not the silence cause. Recorded as an open gap: identify ordinals
  28/29 (the disc's LIBSD.IRX is the 1.5 export source; IOP exports are
  ordinal-based and carry no names) and implement or prove them harmless.

## New offline acceptance: `tests/test_spu2_mix.cpp`

Synthetic fixture, no game, no device. It loads the LIBSD native replacement,
calls SdInit (ordinal 4) through the real bridge, programs voice 0 the way a
game does, and drives the IOP clock:

1. Phase 1 documents the reference-faithful silence gate: freesd's
   `sceSdInit(flag=0)` zeroes MVOLL/MVOLR (`InitCoreVolume`, freesd.c:459),
   the bridge does the same, and a fully keyed voice stays silent until the
   game sets the master volume. Observed: peak 0.
2. Phase 2 sets MVOL and requires real output from the same voice without
   further programming. Observed: peak 28667 (theory ~28671).

Result: `[PASS] SPU2 produced non-silent samples through the libsd bridge`.
The mixer chain (key-on -> ADPCM decode -> ADSR -> routing -> master volume
-> sink) works end to end offline, so an in-game silence must be game
register state or the host output device, not the mixer.

## Findings made while building the test

- SPU2 ring buffers occupy words 0x0000-0x1FFF (byte 0x2000, matching the
  hardware layout) and are rewritten every tick; a sound bank placed there is
  destroyed. The test bank lives at word 0x4000.
- `SdInit` leaves voice pitch at the freesd default 0x3FFF (about 4x rate;
  freesd.c:380 writes the same value). A game must set the playback pitch
  (0x1000 = 1.0x = 48 kHz) or the voice drains its bank four times too fast
  and goes silent. The test sets 0x1000, as a game would.
- `mka_process_audio` (host/mka_combat_hooks.hpp) takes `const` samples and
  only forwards them to an observer registry; the host tap cannot mute.
- The mixer does not implement the CORE_ATTR mute bit, so mute cannot be the
  silence cause in this runtime.

## What remains for the audible acceptance (needs one user run)

The offline work cannot split "digital silence" from "device routing". That
needs one instrumented run (launched by the user via launch_game.bat, with
these environment variables set before it):

- `PS2_DUMP_AUDIO=<chip.wav>` — everything the sound chip outputs, before the
  host queue (spu2.cpp).
- `PS2_AUDIO_HEARD=<heard.wav>` — what the speaker receives, silence
  included (host/window.cpp).
- optionally `PS2_SPU2_COUNT=1` — voices-per-batch log, to see whether the
  game keys voices at all.

Interpretation:
- chip.wav silent -> the game's register state produces silence (key-on,
  MVOL, pitch, or data); PS2_SPU2_COUNT and the dump WAV then localize it.
- chip.wav loud but heard.wav silent -> the samples are fine and the problem
  is downstream (the pipe/render path or the FxSound device routing).
- both loud -> the device output is fine and the listening path (FxSound
  enhancer/mute/device selection) is the issue.

`finish_audio` already prints `the device got ... s of sound ... ran dry
NN times`, and `PS2_AUDIO_DUMP`/`PS2_AUDIO_HEARD` headers are updated about
once a second, so even a killed run leaves usable files.

Phase D acceptance (audible non-silent output, zero underruns) is therefore
not yet met: the mixer is proven, the bisect run is outstanding.
