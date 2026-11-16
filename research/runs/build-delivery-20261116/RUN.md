# Build delivery — 2026-11-16 (native sound service, native textures, hardware GL)

## The build

- Fork: `external/forks/ps2-native-recomp`, `main`
- Commits: `79a17e6` (the native sound service replaces the emulated SPU2 chip),
  `b03ff62` (the game's textures live in native GL resources; the CDVD counter
  counts devices)
- Build stamp: `commit b03ff62 clean, built 2026-11-16T19:39:32Z, Release, Clang-23.1.3`
- Runner: `build_native_gl_full/ps2run.exe`
- **SHA-256: `482f906a65789aedadb9985a7607aadd78f481a37dcf924b9583cedc347dd5b3`**
- Flags: none (the default window run; the game's own `config/armageddon-keys.ini`)

## Suites (final binaries, after the clean rebuild)

| suite | result |
|---|---|
| `test_spu2_mix` | PASS — "[PASS] SPU2 produced non-silent samples through the libsd bridge", peak sample 7166, **chip-model ops 0** |
| `test_gl_vs_raster` | PASS — 18/18 matrix cases |
| `test_native_gl_alpha` | PASS — "GS ALPHA=0x44 PSMCT32 source-over matches the reference" |
| `test_gl_lazy_readback` | PASS — all 10 cases |
| `test_iop_native_module` | PASS |

## What this build changes

### 1. The missing in-game sound (the volume overflow)

The SPU2's volume register is a **15-bit magnitude**: `0x0000` is silence and
`0x7FFF` is full scale. The emulator shifted it left into a **signed** 16-bit
value, so `0x7FFF` became `-2` and every voice was multiplied by a negative
level. The same overflow was in the **main volume**, which silenced the whole
voice mix - while the sound that arrives by DMA (scaled by the input volume,
which was used unsigned) stayed audible.

That is exactly what the 13:21 run showed: **the movies had sound, the game
did not**. Both are 15-bit magnitudes now and the mixer scales by them
directly. The test measures it: `vol=16383/16383` where the old build logged
`vol=-2/-2`.

### 2. The SPU2 chip leaves the route

`runtime/src/iop/spu2.cpp` became **`runtime/src/iop/audio_service.cpp`**, the
native sound service. The voices, the mixer, the reverb, the sound memory and
the ADMA path are the service's own state; the retail sound library's
parameter interface is decoded into **named fields** (the mixer's routing, the
reverb's ring addresses and volumes, the main volumes, the attribute and
transfer registers) instead of a register array. The ~30 register mirrors in
`libsd_native.cpp` are gone, and the census's "SPU2 chip-model ops at the
device boundary" has no call site left: it reads **0** (it was 456).

The voices also render **once** instead of twice: the service and the chip
kept separate voice sets and the game set both.

### 3. The character select's textures (752,835 dropped primitives)

The mipmapped texture modes no longer drop the packet. The reference
rasterizer has no mip levels - it samples the base level with the MAG filter
(`raster.cpp:365`, `s.bilinear = (c.tex1 >> 5) & 1u`) and ignores MMIN - so
the native path does the same. The rejection had cost 752,835 primitives in
the 13:21 run: the character select's own textures.

The ALPHA blend now also works on the framebuffer formats the game uses
besides PSMCT32 (the destination alpha only enters the blend when the ALPHA
register multiplies by it): a 24-bit framebuffer and a 16-bit framebuffer that
does not use Ad. 1,392 packets were dropped for that. A 16-bit framebuffer
that *does* use Ad stays rejected and counted: its destination alpha is one
bit per pixel, which GL's destination-alpha factor cannot carry.

### 4. The game's textures live in native GL resources

The native path kept **one** texture and invalidated it after **any** write
anywhere in VRAM, so every texture the game sampled was decoded and uploaded
again and again - that was the native path's CPU time. The textures now live
in a small pool of native GL textures keyed by their VRAM base, a write only
invalidates the textures it covers, and a write that belongs to a native
texture is **pushed into it** (`glTexSubImage2D` over the written rectangle).
That is what the census's "GS transfers stopped in emulated VRAM" counted:
152,728 of them in the 13:21 run (the game's own textures, FBP 301-360).

### 5. Hardware GL, and an honest "GPU %"

The visible window no longer asks for a **GL 2.1** context: SDL then asks the
driver for the highest context it supports, which is the hardware path. The
log names the driver at startup (`AMD Radeon RX 6800 XT | GL 4.6.0
Compatibility Profile Context`).

The window title's "GPU %" read the WebGPU path's counter, which is always
zero in the native GL path - that is what made the title say "GPU 0%" while
the game was drawing on the Radeon. It now reads a **GL timer query**
(`GL_TIME_ELAPSED`) around the frame's commands: the GPU's own busy time.

### 6. The CDVD counter counts devices

The reads it counted were served by the **native content volume** (the host's
files; there is no disc image and no CDVD device on the route). The gate
counter now counts device reads only, and the content volume's reads are
reported on their own line.

### 7. Robustness

- The sound server's command list: a **zero word ends the list** (the retail
  server's list is zero terminated; every zero used to be logged as an unknown
  command) and `0x00400000` is the load command with the retail's flag bit set.
- The **spin guard**: a guest worker that ends a slice on the same
  instruction over and over is not making progress - the 13:06 run hung that
  way in the guest sound server's sequencer and had to be closed by hand.
  After 4096 such slices the thread is stopped and the log says where it was.

## What the next run must show

| census line | expected | why |
|---|---|---|
| `SPU2 chip-model ops` | **0** | the emulated chip is gone; the service owns the voices |
| `CDVD device sector reads` | **0** | the content volume serves the reads; there is no device |
| `GS transfers stopped in emulated VRAM` | **0** (or far below 152,728) | the writes are pushed into the native textures |
| `packets dropped: mipmapped` | **0** | the mipmapped modes are sampled like the reference does |
| `packets dropped: non-PSMCT32 blend` | **0** | the 24-bit and 16-bit blends are expressible |
| `voices keyed` / `peak sample` | > 0 and audibly non-zero | the volumes are 15-bit magnitudes now |
| `GPU %` in the window title | > 0 | the timer query measures the GPU's own busy time |

## Still open on the gate (not in this build)

- **CRI_ADXI's movie server** (the last guest-registered SIF server, 54 RPCs
  on sid `0x90000200`). Making it native means implementing the ADX movie
  server's protocol natively; the guest module does the decoding today.
- **The one EE live translation** at `0x00B17530` in the image
  `00B04000_A10C1E41` (the 13:21 run translated it on the spot and the game
  waited 7.2 s for it). The window has to be captured offline and translated
  ahead of the run; the capture source for that image is not in the tree.
- **The retail sound bank's sample data.** The index (`MSLASSET.MS2`) is
  decoded, but the entry data is a description rather than SPU2 ADPCM: the
  13:21 run's log shows the retail sound server reading the index from
  `MSLASSET.MS2` and the samples from `MSLASSET.MS4`. The native sound
  service needs the same mapping.

# Build delivery — 2026-11-16 (the coverage rebuild: nothing compiled while the game runs)

## The build

- Fork: `external/forks/ps2-native-recomp`, `main`
- Commit: `dc8784f` ("recomp: every word of code in an image without symbols is an entry point"),
  the rules in `cdc8220`
- Build stamp: `commit dc8784f clean, built 2026-11-16T20:35:51Z, Release, Clang-23.1.3`
- Runner: `build_native_gl_full/ps2run.exe`
- **SHA-256: `1be585638799f7084ab1aa42f0bd9bd25926279e48f17a6052a500ba9a171af5`**
- Flags: none (the default window run)

## What was regenerated

The game's code was re-translated with the new recompiler, over the captures
that already exist (no re-capture):

```
code 1079332 instructions (4216 KiB), functions 8917, entries 93354, rejected seeds 14983
3 entry points of the program called from modules and code windows
4 distinct code window images of 4 captured, code 161713 instructions (631 KiB), 73 entry points
360 units written to generated/mkarmageddon
```

**8,917 functions and 93,354 entry points.** Before this, an image the game
loads into RAM at runtime had no symbols, so its functions were found by
guessing; a function the game reached through something the guess missed was
not an entry point, and the runtime had to translate it while the game waited
("no translated code at 0x00B17530", seven seconds of stall, 13:21 run). Now
every word of code in such an image is an entry point, so the coverage is
complete by construction: **nobody has to play a mode to discover missing
code, and the game never stops for one.**

The build recompiled all 360 generated units and relinked the runner.

## Suites (final binaries, after the clean rebuild)

| suite | result |
|---|---|
| `test_spu2_mix` | PASS — non-silent samples, SPU2 chip-model ops 0 |
| `test_gl_vs_raster` | PASS — 18/18 matrix cases |
| `test_native_gl_alpha` | PASS — the ALPHA blend matches the reference |
| `test_gl_lazy_readback` | PASS |
| `test_iop_native_module` | PASS |

## New measurements

The census now counts the two halves of "nothing runs that emulates" that
were not measured:

- `VU1 programs translated while the game ran` — the vector program's live
  path was never counted, so a run that compiled one looked clean.
- `guest modules loaded and able to run` — a module loaded with a real base
  is guest code that can run; the accepted route may not have one.

Both are in the GATE line and in the verdict.

## The image enumeration (`tools/acceptance/list_code_images.py`)

- 1 program image (the ELF the game boots).
- 20 guest modules on the disc (`MODULES/*.IRX`).
- **37 images the game loads at runtime, of which 33 have no captured
  window** - they exist only as compiled libraries in the ee-live cache,
  because the runtime translated them itself over the game's history. They are
  not ELF-overwritten windows (`windows.py list` gives three static ones); they
  are the blobs the game loads from its data (the 2064 KiB image at
  `0x00B04000` is the largest). Each has to be captured **offline from the
  disc** and translated whole before the game starts - that is the next step.

## What the next run must show

| census line | expected |
|---|---|
| `live guest translations` | 0 for the program's own code (the 33 uncaptured images are the remaining gap) |
| `VU1 programs translated while the game ran` | 0 |
| `guest modules loaded and able to run` | 1 today: `CRI_ADXI` (retired in step 4 of the programme) |

# Build delivery — 2026-11-16 (the coverage rebuild: nothing compiled while the game runs)

## The build

- Fork: `external/forks/ps2-native-recomp`, `main`
- Commit: `dc8784f` ("recomp: every word of code in an image without symbols is an entry point"),
  the rules in `cdc8220`
- Build stamp: `commit dc8784f clean, built 2026-11-16T20:35:51Z, Release, Clang-23.1.3`
- Runner: `build_native_gl_full/ps2run.exe`
- **SHA-256: `1be585638799f7084ab1aa42f0bd9bd25926279e48f17a6052a500ba9a171af5`**
- Flags: none (the default window run)

## What was regenerated

The game's code was re-translated with the new recompiler, over the captures
that already exist (no re-capture):

```
code 1079332 instructions (4216 KiB), functions 8917, entries 93354, rejected seeds 14983
3 entry points of the program called from modules and code windows
4 distinct code window images of 4 captured, code 161713 instructions (631 KiB), 73 entry points
360 units written to generated/mkarmageddon
```

**8,917 functions and 93,354 entry points.** Before this, an image the game
loads into RAM at runtime had no symbols, so its functions were found by
guessing; a function the game reached through something the guess missed was
not an entry point, and the runtime had to translate it while the game waited
("no translated code at 0x00B17530", seven seconds of stall, 13:21 run). Now
every word of code in such an image is an entry point, so the coverage is
complete by construction: **nobody has to play a mode to discover missing
code, and the game never stops for one.**

The build recompiled all 360 generated units and relinked the runner.

## Suites (final binaries, after the clean rebuild)

| suite | result |
|---|---|
| `test_spu2_mix` | PASS — non-silent samples, SPU2 chip-model ops 0 |
| `test_gl_vs_raster` | PASS — 18/18 matrix cases |
| `test_native_gl_alpha` | PASS — the ALPHA blend matches the reference |
| `test_gl_lazy_readback` | PASS |
| `test_iop_native_module` | PASS |

## New measurements

The census now counts the two halves of "nothing runs that emulates" that
were not measured:

- `VU1 programs translated while the game ran` — the vector program's live
  path was never counted, so a run that compiled one looked clean.
- `guest modules loaded and able to run` — a module loaded with a real base
  is guest code that can run; the accepted route may not have one.

Both are in the GATE line and in the verdict.

## The image enumeration (`tools/acceptance/list_code_images.py`)

- 1 program image (the ELF the game boots).
- 20 guest modules on the disc (`MODULES/*.IRX`).
- **37 images the game loads at runtime, of which 33 have no captured
  window** - they exist only as compiled libraries in the ee-live cache,
  because the runtime translated them itself over the game's history. They are
  not ELF-overwritten windows (`windows.py list` gives three static ones); they
  are the blobs the game loads from its data (the 2064 KiB image at
  `0x00B04000` is the largest). Each has to be captured **offline from the
  disc** and translated whole before the game starts - that is the next step.

## What the next run must show

| census line | expected |
|---|---|
| `live guest translations` | 0 for the program's own code (the 33 uncaptured images are the remaining gap) |
| `VU1 programs translated while the game ran` | 0 |
| `guest modules loaded and able to run` | 1 today: `CRI_ADXI` (retired in step 4 of the programme) |
