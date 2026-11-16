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
