# The two visible defects — what was found and what is fixed (2026-11-16)

Both defects the user reported from their own run (the memory card screen's
words drawn as noise, and no in-game audio) are worked on here. The audio is
fixed inside the sound service; the text's cause is *not* the one that was
first suspected, and that is written down so the next agent does not repeat
the search.

## 1. No in-game audio — the engine is fixed, the delivery is not

### What was wrong

The census said it all: every run read `voices keyed = 0, voice frames = 0,
ADPCM blocks = 0` while the log showed `[msl-native] play sound 99: 11
sub-samples`. The sound was queued and never played, for three separate
reasons:

1. **The mixer never ran.** The native sound service's tick is what advances
   the MSL sequencer (the sequencer is stepped by the mixer's frame hook). The
   retail path started it by touching the sound registers; the guest MSL
   driver is gone (the native MSL service replaced it), so nothing did. The
   MSL service now starts the mixer itself when it queues a sound
   (`runtime/src/iop/msl_native.cpp`, `play_sound`), and keying a voice starts
   it too (`runtime/src/iop/audio_service.cpp`, `key_on`).
2. **The sound was looked up by the wrong key.** The bank's table entries
   carry an id, and from the 345th entry on the id is not the table's
   position. The service looked the sound up by position; it now looks it up
   by id.
3. **The sample data was the wrong bytes.** A sound in the bank is a *group*:
   the sub-sounds it lists, and then the *sample records* — the words with the
   `0x20000000` flag — and each record carries the offset and the size of the
   ADPCM data in the bank. The service read the group's table as if it were
   ADPCM, so the voice decoded noise and stopped at once. It now collects the
   group's sample records (and the records of the sub-sounds it lists) and
   plays their bytes. The groups may name each other in a circle, so every
   entry is visited once and the walk is bounded.

The bank's own numbers, read from `games/mkarmageddon/disc-files/SNDSPS2`:
`MSLASSET.MS2` is 98 MB (the index and the groups) and `MSLASSET.MS4` is
584 MB (the sample data of the other bank). A sound's group is
`[count][sub-sound ids…][…][sample records: id|0x20000000, flags, offset,
size]…`.

4. **The voices were routed nowhere.** The retail server routed every voice to
   the dry bus when the library was initialised (the mixer's voice masks).
   Nothing set them, so the voices were keyed and mixed into nothing. The
   native MSL service now sets them (`set_switch(0, 24, …)` and
   `set_switch(0, 26, …)`).

### What it measures now

The 900 frame native GL probe (commit `876845e` plus these changes) reads:

```
[census] AUDIO: native frames mixed = 0, blocks consumed = 0;
                voices keyed = 4340, voice frames = 12845888, ADPCM blocks = 456292
```

The voices are keyed, they render, and the ADPCM blocks decode. The census's
`voice frames` and `ADPCM blocks` counters were gaps in the instrumentation
until now — they were never incremented, so they measured nothing.

### What is still open

The **device** still gets nothing: the run's `last_run_device.wav` is 44 bytes
(the header, no frames) and the window's title counts 0 ms of audio. The
service's mix calls the sink (the sink is called once per mixed batch), so
what is left is between the sink and the device: the host's audio delivery
(the ring the device drains and the WAV writer). **That is the next thing to
fix, and it is in the host's audio path, not in the sound service.**

## 2. The garbled text — the texture base unit was wrong, and that was not the cause

### The fix that is in

A texture's base pointer (TEX0.TBP) is in units of **64 texels**, so the
address it stands for depends on the pixel format: 64 texels are 256 bytes of
a 32-bit texture, 128 of a 16-bit one and **64 of an 8-bit one**. The runtime
used 256 bytes for every format, so every 8-bit and 16-bit texture was
decoded from four times (or twice) too far into the VRAM.

* `gs::texel_block_bytes` and `gs::read_texture_pixel` do the conversion; the
  texture decodes (`raster.cpp`) read through them.
* The transfer's destination base pointer (DBP, 256 bytes) and the frame
  buffer's (FBP, 8 KB) are in other units and are unchanged.
* The write-through into the native textures compared the write's base pointer
  with the texture's as if they were the same unit. Both are now compared as
  bytes, and the texture's first block is derived from its byte base.

### Why it did not fix the text

The captured frames are **byte-identical** before and after the change
(`9D53181A…` for frame 900 in both runs), so the font page was not being
decoded through that path — the garbling has another cause.

The next place to look is the **VU1**: the 2D layer (the text) is drawn by the
RSP, and the RSP's texture reads have their own base-pointer convention
(64 texels as well). The VU model's texture addressing has not been checked
against that convention yet; that is where the remaining suspicion lies, and
the frames are the judge.

### How to tell whether a change altered the picture

Compare the captured frames by SHA-256. The ASCII rendering used here
(96x40 characters) is far too coarse for the memory card screen's text: it
showed the same picture for a change that did alter it and for one that did
not.

### What is ruled out for the text

* **The GS write-through** (the push into the native textures): the captured
  frames are byte-identical before and after it (`9D53181A...`).
* **The texture base unit**: the frames are byte-identical before and after the
  fix (same hash), so the font page did not come through the GS texture
  decode at all.
* **The VU1''s own texture reads**: the runtime''s VU sources never compute a
  texture address (`vu_jit.cpp`, `vu_micro.cpp` have no texture fetch) - the
  *guest''s RSP microcode computes the address itself and reads the VRAM
  through the memory interface, so its addressing is the guest''s own and is
  correct.

That leaves: the font is not sampled from a GS texture at all (the RSP may
read it from another source - the I/O area''s font, or a page the game writes
by a path that is not a transfer), or the font page''s *format* is not what the
GS texture decode assumes (a PSMT8 page with a CLUT, or a 16-bit page), or the
page''s content is wrong where the RSP reads it. The next agent should look at
the RSP''s own texture coordinates for the memory card screen''s words (the
guest''s microcode, in `generated/mkarmageddon` and the VU1 capture) rather
than at the GS side.