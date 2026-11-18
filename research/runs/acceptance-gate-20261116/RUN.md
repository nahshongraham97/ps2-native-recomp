# Historical census observation — 2026-11-16 (the guest's VRAM writes reach the native textures)

> Audit correction (2026-11-18): This is a dated report, not current status. Broad native/acceptance/perfected/full-mode claims below are unsupported unless bounded to their exact recorded scenario; quoted tool verdicts are historical output, not proof of the execution contract. Preserve measured counts and original log quotations. Current status is the canonical root PORT_STATUS.md and acceptance remains OPEN.

## The build

- Fork: `external/forks/ps2-native-recomp`, `main`
- Commits: `a1ee3a2` (nothing is translated while the game runs any more),
  `b7ddffb` (the native texture pool holds the game's textures),
  `876845e` (a guest VRAM write lands in the native texture that covers it),
  `f6c190f` (the census counts guest code that was compiled ahead of time too),
  `32eeb27` (the push makes the GL context current before it writes)
- Build stamp: `commit 32eeb27 clean, built 2026-11-16T21:29:31Z, Release, Clang-23.1.3`
- Runner: `build_native_gl_full/ps2run.exe`
- **SHA-256: `6bcfff337725bdc1bc4c869bb159807b460f68b8304823ef71782e9c3997c821`**
- Flags: `PS2_NATIVE_GL=1` (the native OpenGL route; no WebGPU presentation),
  `--frames 2000`, `--frames-dir games\mkarmageddon\shots\acceptance-32eeb27`

## The census (the run's own words)

```
[census] EE   : live guest translations = 0        (PASS); VU1 programs translated while the game ran = 0 (PASS)
[census] IOP  : guest modules loaded and able to run = 0   (PASS)
[census] VU   : interpreter fallbacks = 0 (vu0 0, vu1 0) (PASS)
[census] IOP  : guest instructions = 0       (PASS)
[census] SIF  : RPCs dispatched natively = 462079, through guest-registered servers = 0, unknown = 0
[census] GS   : transfers reached the native surface = 49193, stopped in emulated VRAM = 0; primitives = 1297589
[census] SPU2 : chip-model ops at the device boundary = 0; key-ons = 0 key-offs = 0 DMA writes = 0 (0.0 MB) reads = 0 peak sample = 0
[census] CDVD : device sector reads = 0                   (no CDVD device on route); native content volume reads = 0
[census] GATE  : emulated PS2 devices on the accepted route (each must reach 0): GS-stopped-in-VRAM=0 SPU2-chip-model-ops=0 SIF-guest-registered=0 CDVD-device-reads=0; compiled while the game ran: EE=0 VU1=0; guest modules able to run=0
[census] VERDICT: ACCEPTANCE GATE MET (no interpreter and no emulated PS2 device on the accepted route)
```

Every base the game writes is listed as applied and none as skipped:

```
[census] GS   : transfers by base: base=0(fbp=0):app=1,skp=0 base=9632(fbp=301):app=3550,skp=0 base=9664(fbp=302):app=3186,skp=0
base=9696(fbp=303):app=3186,skp=0 base=9728(fbp=304):app=3186,skp=0 base=9760(fbp=305):app=3186,skp=0 base=9792(fbp=306):app=3186,skp=0
base=9856(fbp=308):app=1576,skp=0 base=9888(fbp=309):app=34,skp=0 base=9920(fbp=310):app=1576,skp=0 base=9952(fbp=311):app=3152,skp=0
base=9984(fbp=312):app=34,skp=0 base=10016(fbp=313):app=3169,skp=0 base=10048(fbp=314):app=1576,skp=0 base=10144(fbp=317):app=379,skp=0
base=10176(fbp=318):app=1955,skp=0 base=10208(fbp=319):app=1576,skp=0 base=10304(fbp=322):app=17,skp=0 base=10336(fbp=323):app=1576,skp=0
base=10368(fbp=324):app=3152,skp=0 base=10464(fbp=327):app=3152,skp=0 base=10560(fbp=330):app=3152,skp=0 base=10656(fbp=333):app=3152,skp=0
base=11200(fbp=350):app=362,skp=0 base=11232(fbp=351):app=122,skp=0
```

## The suites (final binaries, after the clean rebuild)

| suite | result |
|---|---|
| `test_spu2_mix` | exit 0 — "[PASS] SPU2 produced non-silent samples through the libsd bridge", peak sample 7166, chip-model ops 0 |
| `test_gl_vs_raster` | exit 0 — 18/18 matrix cases |
| `test_native_gl_alpha` | exit 0 — the census in the test reads ACCEPTANCE GATE MET | Historical tool wording only; not release acceptance.
| `test_gl_lazy_readback` | exit 0 — all 10 cases |
| `test_iop_native_module` | exit 0 — the census in the test reads ACCEPTANCE GATE MET | Historical tool wording only; not release acceptance.

## What the last change did

The last census violation left was **"GS transfers stopped in emulated VRAM"**:
the game wrote its texture pages into VRAM every frame (five transfers per
frame, 262 of each in 300 frames), and the write stayed in the host-side
array while the native path read the page back out of that array when a
primitive sampled it. The picture was right; the write itself never reached a
native resource.

The push now follows the VRAM's own structure. The VRAM is made of **8 KB
blocks**, and inside a block the GS's swizzle depends only on where the pixel
sits in that block. A write's block and a texture's block that are the same
block therefore hold the same pixels - as long as both use the same pixel
format, because a block's shape depends on the format. The write is resolved
into the texture's own pixel space (block row, block column and the pixel's
place inside the block) and uploaded into the native texture with
`glTexSubImage2D`; the texture is not stale afterwards, so the GS samples the
native resource.

A write that only partly lands (a block the texture does not use, a rectangle
that does not fit the texture's extent) leaves the texture stale, so it is
decoded again when it is sampled: the picture stays right either way.

`32eeb27` makes the push make the WGL context current before it writes: the
push runs where the guest's transfer finished, which is the thread that runs
the guest, not the render thread that owns the context (the other native call
from that path, `draw_guest_rect`, already did this).

### The evidence that the push is faithful

* **The frames are byte-identical.** The 300-frame run before the change and
  the 300-frame run after it produce the same pictures: frames 60, 120, 180,
  240 and 300 have the same SHA-256 (`9D53181A…`) in both runs. The push and
  the re-decode agree pixel for pixel.
* **The counter moved to zero and stayed there.** `GS-stopped-in-VRAM` went
  152,728 (the 13:21 run) → 49,192 (the pool fix) → **0** (this build).
* **Nothing else moved.** The same run reports 49,193 transfers applied, the
  same 1,297,589 primitives, and 0 on every other counter.

## The census gap this build also closed

`f6c190f` counts the instructions of guest code that was **compiled ahead of
time** as guest instructions. Before it, only the interpreter's retired
instructions were counted, so a run in which the CRI ADX module was loaded and
running still read "guest instructions = 0". The 13:21 run is the case: it
loaded `CRI_ADXI` at 0x00071300 and ran it (769,094 `[iop-aot]` entries in its
log) and the census still passed - that build (`ccaf554`) did not have the
module counter at all.

The gate line and the verdict now see the AOT execution ("IOP guest
execution" in the verdict list). On this route the counters stay at 0,
because the native GL route never reaches the point where the game loads the
ADX module - the movie path is still open work (programme step 4).

## What is still open (and how it is measured)

| item | state |
|---|---|
| CRI_ADXI native (ADX sound server, then ADX video player) | open - the last guest module that can run; the census now counts it when it loads |
| the host-side VRAM array as the source of truth | open - the guest's writes reach the native textures now; the *first* fill of a native texture still comes from the host-side array |
| the 16-bit framebuffer's per-pixel destination alpha | open - the last fidelity gap the census names |
| the modes, played and fixed | open - a gameplay list, not a coverage list |

## Later history (this record is a snapshot of that build, not the current state)

* The build this record describes had its **rendering** reverted later the
  same day (`13c62b0`): the GS write-through push and the texture-base-unit
  change made the picture electronic static, so both came out and the GS
  counter went back to counting the transfers that do not reach a native
  resource (16,192 in a 900 frame probe). The gate line in this record is
  what that build measured; it is not the current state.
* The census in this record does not count the IPU, which is still emulated.
  See `../audio-and-text-20261116/RUN.md` and the project's `PORT_STATUS.md`
  for the current state and the work list.
