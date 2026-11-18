# Build: the picture and the sound back to a known-good state (2026-11-16)

> Dated-run audit boundary (2026-11-18): All implementation/current/verified language here refers to the named historical source, build and scenario. Original counter values, outputs, observations and artifact IDs are preserved as reports, not independently revalidated facts. “Native” component names, successful builds/tests and old census verdicts do not establish host resource ownership, card/profile behavior, correct sound, physical input, movie A/V sync or whole-game acceptance. Read the canonical parent PORT_STATUS.md and execution contract for current blockers; unresolved historical causal/source claims remain unresolved.

## What this build does

Two of the changes that went in on 2026-11-16 are **reverted**:

* **The GS write-through push** (`876845e`, the guest's VRAM write pushed into
  the native texture that covers it) and **the texture base unit change** (a
  texture's base pointer converted with its format) are both out of the
  rendering path. The build with them rendered **electronic static**; the
  build before them renders the screen the way the user's screenshot shows.
* **The bank's sample playback** is out of the sound path: the sound is
  queued and logged, and **no voice is keyed**, so the game is silent rather
  than loud with noise. The WAV of the windowed run with the sample playback
  in it was 10.26 s at **peak 32,768** (clipping): the bank's sample records
  do not point at the SPU2's ADPCM blocks (a record's offset lands on
  another table), so those bytes are not a sound.

What **stays**:

* the sound service's mixer starts when a sound is queued (the retail path
  started it by touching the sound registers; there is no guest driver left to
  do that), and the voices are routed to the dry bus (the retail server routed
  every voice there);
* the sound is looked up by its id, not by the bank's table position;
* the census counts the frames the voices rendered and the ADPCM blocks they
  decoded (`voice frames`, `ADPCM blocks` were gaps in the instrumentation).

## What it measures

The 900 frame native GL probe with the scripted input (`--press 30:cross
--press 90:start`, the memory-card prompt is answered by the script so the
game can move on without the user):

```
[census] GS   : transfers reached the native surface = ..., stopped in emulated VRAM = ...
[census] AUDIO: ... voices keyed = 0, voice frames = 0, ADPCM blocks = 0
[census] VERDICT: ...
```

The GS counter is back to the pre-push value (the transfers that do not reach
a native resource are counted again as a known gap, which is the honest
number until the push's mapping is proved).

## Why the two changes were reverted and not fixed in place

The picture could only be judged by the user, and the rule is that the game is
not opened. The headless `--frames-dir` capture does **not** contain the
game's picture (its frames are byte-identical across the whole run and show
the loading screen), so it could not tell a broken picture from an unchanged
one; the comparison I made with it was worthless and I reported it as if it
were evidence. Without an offline oracle for the picture, a rendering change
cannot be trusted, so both changes come out until there is one.

## What has to happen next (and it is the same list as the project's)

1. **An offline picture oracle**: a test that renders the game's picture the
   way the window does (no window, no device) or reads the words out of the
   captured framebuffer, so a rendering change can be judged without the user.
2. **The bank's sample format**: where the ADPCM data of a sound sits, so the
   game's sounds play. Verification: a capture test in the `test_spu2_mix`
   pattern (`set_sink(capture)`) asserting the samples are a sound.
3. **The RSP's 2D layer** (the garbled words): the GS side is ruled out by
   measurement, and the GS's 8-bit pages are empty (the game never uploads an
   8-bit page), so the words come from the RSP.
4. **Step 4**: the decoder's macroblock failure point, the motion
   compensation, the CRI audio, the native ADX SIF service (sid `0x90000200`,
   twelve function numbers), the IPU out of the route.
5. **Steps 5-7**: the native VRAM mirror, the 16-bit framebuffer's per-pixel
   destination alpha, the modes.

## The rule this build follows

A change that cannot be verified offline does not go in. The counters are not
the claim: the frames and the audio are the claim, and the frames have to be
looked at.
