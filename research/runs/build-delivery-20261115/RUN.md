# Build delivery — 2026-11-16 (native sound service, native textures, hardware GL)

The full record is in [`../build-delivery-20261116/RUN.md`](../build-delivery-20261116/RUN.md).

- Commits: `79a17e6` (the native sound service replaces the emulated SPU2 chip),
  `b03ff62` (the game's textures live in native GL resources; the CDVD counter
  counts devices)
- Build stamp: `commit b03ff62 clean, built 2026-11-16T19:39:32Z, Release, Clang-23.1.3`
- Runner SHA-256: `482f906a65789aedadb9985a7607aadd78f481a37dcf924b9583cedc347dd5b3`
- Suites: all five pass (`test_spu2_mix` PASS with non-silent samples and
  chip-model ops 0, `test_gl_vs_raster` 18/18, `test_native_gl_alpha` PASS,
  `test_gl_lazy_readback` PASS, `test_iop_native_module` PASS)
- The in-game sound was missing because the volume register (a 15-bit
  magnitude) was shifted into a signed 16-bit value: `0x7FFF` became `-2`, so
  every voice and the whole main mix were multiplied by a negative level. The
  sound that arrives by DMA was scaled unsigned, which is why the movies had
  sound and the game did not.

# Build delivery — 2026-11-16 (native sound service; MSL/SDRDRV bodies retired)

- Commit: `ccaf554` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit ccaf554 clean, built 2026-11-16T18:15:57Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 18:15
- SHA-256: `d3ea5c2522e79edd97245108ee3089badf771fea7e3e91939780eb08e08c8b74`
- Increment 4 step 3 — **the runtime now owns the sound server's SIF endpoint** (`runtime/src/iop/msl_native.cpp`):
  - **Protocol (decoded from the captured MSL body, the retail bank and the 11:14 log):** the play
    request is `op3` id 0 with the sound number as the value (the run shows MSL storing 101 then
    102 at `0x4CDA4`); `op3` id `0x100FC` is the staging-buffer handshake (answered with
    `0x4C9E4`, as the retail server did); `op6` stages a block, `op4` executes it, `op9`/`op0A`
    load a bank, `op0x0B`/`0x0D`/`0x11`/`0x0C` are flags, `op0x4B` is a query. The reply is four
    words, as the retail server's was.
  - **Bank format (from the retail file, `disc-files/SNDSPS2/MSLASSET.MS2`):** header
    `[2][8705 sound count][110748 table bytes]`, then 9229 twelve-byte entries
    `[id][offset][size]`; the data at an entry's offset is `[count][count * sub-sample index]`, and
    each sub-sample is another table entry whose `[offset][size]` points at the sample in the
    SPU2's own ADPCM block format.
  - **Playback:** the sample is copied into the sound RAM at the voice's start address,
    VOLL/VOLR/PITCH are set, the voice is keyed on, and the sequencer advances with the mixer's
    frame tick (the native engine grew a frame hook for it).
  - **The captured MSL and SDRDRV bodies are no longer loaded:** the loader keeps each module's
    captured IOP base reserved (so the layout other modules expect still holds) but does not load
    the guest IRX bytes - **no guest SIF service remains for the sound server**.
  - The native file service gained a guest-path byte-range reader
    (`mwfilefr_read_guest_range`) so the bank is read through the same disc-path resolution the
    guest MWFILEFR used.
- Suites (final binaries): `test_spu2_mix` PASS, `test_gl_vs_raster` 18/18, `test_native_gl_alpha`
  PASS, `test_gl_lazy_readback` PASS, `test_iop_native_module` PASS.
- Replaced runner: `4d154395e072e8acc3917b85636852b333694e0bfd1ffd15706d219dde974371`
  (`01d80fc clean` — sound decode, channel-pool watch, missing mwfile exports)
- Acceptance for this increment (measured on the next run): the census's `SIF ... through
  guest-registered servers` must be 0 for sid `0x00012345`, `voices keyed` must be > 0, and the
  game must still boot and navigate.
- Still open (the rest of the gate, with the code sites mapped): **SPU2 chip-model ops** (961) -
  the emulated chip's register file has to move into the native engine: `libsd_native.cpp` mirrors
  every voice register into `spu2::write/read` (about 30 sites) and the native engine's parameter
  indices use a different convention than the chip's (`libsd_native.cpp:33` holds the chip's
  register map), so the mapping has to be ported before the mirror can go; **CDVD device reads**
  (15,836) - `cdvd_native.cpp:118` counts the device reads that the native content volume
  already serves; **GS transfers stopped in emulated VRAM** (272,440) - the transfers that end in
  the emulated VRAM instead of a native GL texture; **EE live translations** (2 per run) - the
  PCs are logged now, so the missing windows can be captured.
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075BB1…` → `78a0c1f6…` → `daef9fb5…`
  → `d8266fcf…` → `52102daf…` → `0190f570…` → `4d154395…` → `d3ea5c25…`

# Build delivery — 2026-11-16 (sound-service decode + channel-pool watch + missing mwfile exports)

- Commit: `01d80fc` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit 01d80fc clean, built 2026-11-16T17:53:02Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 17:53
- SHA-256: `4d154395e072e8acc3917b85636852b333694e0bfd1ffd15706d219dde974371`
- Increment 4 step 2 continued - the oracle decode of MSL's command protocol (the captured
  body plus the 11:14 run's log):
  - **The play request is `op3` with id 0 and the sound number as the value** - the run shows
    MSL storing `0x65` (101) at `0x4CDA4`, then `0x66` (102): a sequence of sounds, one after
    another.
  - **The sequencer gates the play on the sound number being below the bank's sound count**
    (worker compare at `0x2EBC0`, skip when out of range).
  - **Bank format (from the retail file itself, `disc-files/SNDSPS2/MSLASSET.MS2`):** header
    `[2][8705][110748]`, then 9,229 twelve-byte entries `[index][size][file_offset]` at offset
    12, sample data after the table. The header's **8705 is the sound count** (the sequencer's
    limit) and sound 101 is inside it - so the play is **not** out of range.
  - **MSL is alive**: it reads the header and the 110,748-byte index, registers the bank
    (`0x04CDB8/BC/C4`), signals its sequencer semaphore (`thsemap` #6) and keeps queueing sounds.
    **No voice ever starts**: `voice_start` (`0x33B68`) returns early when the channel's state
    byte (record + `0x0A`) is 0 or 4, and the channel pool (`0x50620-0x50E34`) was outside the
    watched window. The EE also **never writes the staging buffer `0x4C9E4`** it was given (MSL's
    reply `op3` id `0x100FC`) - no DMA, no IOP store, for the whole run.
- Changes: (1) the IOP store watch is now a **list of ranges** - the control block/staging buffer
  `[0x4C9C0,0x50000)` **plus the channel pool `[0x50600,0x50E40)`** whose state byte decides
  whether a voice starts (`PS2_WATCH_IOP` still overrides); (2) **unresolved native imports are
  reported** (`[iop-native-miss]`, first occurrence per library/ordinal with its arguments) - the
  native MWFILEFR exports 4, 5, 10, 13, 15, 16 while the retail IRX had nine (4, 5, 8, 9, 10,
  13, 14, 15, 16), so MSL's calls to 8, 9 and 14 failed silently; (3) **the three missing mwfile
  exports are provided** as measured no-ops that log their arguments (the native read completes
  synchronously, so a completion wait has nothing to wait for; v0 is preserved as the retail
  ordinal 13 does); (4) the census's live-translation counter **logs the EE PC**, so the two live
  translations in the 11:14 run can be located and captured.
- Suites (final binaries): `test_spu2_mix` PASS, `test_gl_vs_raster` 18/18,
  `test_native_gl_alpha` PASS, `test_gl_lazy_readback` PASS, `test_iop_native_module` PASS.
- Replaced runner: `0190f5706a4c818a60ec2252b7343ece017c0bd54f91ed339ebfd46efe71bf14`
  (`c2c3bf0 clean` - GL state cache and dedicated movie surface)
- Next: the user's run measures the channel states and the staging buffer; then increment 4 step 3
  (runtime-owned endpoint + native command engine + native voice start) and step 4 acceptance
  (`guest-registered → 0` for sid `0x00012345`, `voices keyed > 0`, audible SFX).
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075BB1…` → `78a0c1f6…` → `daef9fb5…`
  → `d8266fcf…` → `52102daf…` → `0190f570…` → `4d154395…`

# Build delivery — 2026-11-16 (GL state cache: 16.8 fps fix; dedicated movie surface)

- Commit: `c2c3bf0` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit c2c3bf0 clean, built 2026-11-16T17:18:22Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 17:18, 75,977,728 bytes
- SHA-256: `0190f5706a4c818a60ec2252b7343ece017c0bd54f91ed339ebfd46efe71bf14`
- Run record this fixes (acceptance evidence): `ps2run_2026-11-16_11-14-49.log` (a12423b,
  5,067 frames in 302.2 s wall = **16.8 fps**, audio stretched, movie shake present,
  key-ons 0, census 5 gate violations incl. a new `EE live translation`). Diagnosis: `b07558f` pushed the previously rejected 77% of
  primitives (6,985,985 of 9,105,842) through the immediate-mode GL path — 9.1M packets ×
  ~15 redundant per-packet state calls ≈ 36k GL calls/frame; the a12423b instrumentation was
  measured NOT the cost (2.5M watch lines ≈ 10k lines/s, below what the 42 fps run already
  carried).
- What it changes (`runtime/src/gs/native_gl_backend.cpp`):
  1. **GL state cache** — `GlStateCache` + `cache_*` setters re-emit scissor, fog, depth,
     alpha test, blend (+FIX constant), colour mask, FBO bind/viewport and the
     TEXTURE_2D toggles only on change; `prepare_texture` skips bind + 4× `glTexParameteri`
     + ~8× `glTexEnvi` when the texture content and TEX0/TEX1/CLAMP/TXA/CLUT match and it is
     still bound. Every GL write outside the setters (present, image transfers, readbacks,
     clears, flush, context init) calls `gl_state_invalidate()` first — each site wired
     explicitly, since a missed invalidation means stale state.
  2. **Dedicated movie presentation surface** — the 11:14 run proved the pin worked
     (`target_fbp=140` on every present) but the game's own movie quad (`prims=2` every
     present) overwrote the scene targets, so blit=NO presents showed the quad and blit=YES
     showed the blit (the remaining shake). `ensure_movie_fbo()` creates a runtime-owned FBO
     the game never draws into; the quiet-gated blit writes it first (scene targets too), the
     first present of a movie seeds it, and the present reads it while a movie plays; the log
     reports `movie surface=N`.
- Increment 4 step-2 evidence from the same run (recorded, decode continues): MSL's advertised
  staging buffer `0x4C9E4` (reply `op3 id 0x100FC`) is zeroed once at init and **never
  written again** — no DMA, no IOP code, all run; `[sif:other-data]` never fires; every window
  DMA carries command lists to the RPC buffer `0x4EDF0`. The command stream is the whole
  protocol; record arrays live at `0x49A60/0x4BD90` and the channel pool at `0x50620–0x50E34`
  (both outside the watched window).
- Suites (final binaries): `test_spu2_mix` PASS, `test_gl_vs_raster` 18/18,
  `test_native_gl_alpha` PASS, `test_gl_lazy_readback` PASS, `test_iop_native_module` PASS.
- Replaced runner: `52102daf2159eb0d80d67e1f15ec3e137553633c10d27df6e091e7ecdb2df93b`
  (`a12423b clean` — sound-service instrumentation)
- Next: user run measures fps (target: cycles/294.912 MHz ≈ wall) and the shake; then
  increment 4 steps 2–4 (op4 gate decode via the command-stream walker, runtime-owned
  endpoint + native command engine, acceptance: `guest-registered → 0` for sid 0x00012345,
  `voices keyed > 0`, audible SFX).
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075BB1…` → `78a0c1f6…` → `daef9fb5…`
  → `d8266fcf…` → `52102daf…` → `0190f570…`

# Build delivery — 2026-11-16 (native sound service instrumentation, step 1 of 4)

- Commit: `a12423b` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit a12423b clean, built 2026-11-16T16:00:55Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 16:00, 75,977,728 bytes
- SHA-256: `52102daf2159eb0d80d67e1f15ec3e137553633c10d27df6e091e7ecdb2df93b`
- What it adds — increment 4 step 1, the instrumentation that answers the open question for the
  native sound service (does the EE's event data reach MSL's advertised staging buffer
  `0x4C9E4`, and how does the channel state machine advance?):
  1. **MSL window watch on by default** — `PS2_WATCH_IOP` now defaults to `[0x4C9C0, 0x50000)`
     (MSL's command/state area: request globals `0x4CDD8/DDC/DF0`, staging `0x4C9E4`, state
     table records at `0x4C9C0` with field `+0xC`), printing every IOP-code store with
     time/PC/RA/size/address/value; the env var still overrides the window. `watch_copy`
     covers DMA copies into the window from both EE-push paths.
  2. **Window DMA content dumps** — an EE→IOP DMA (`sceSifSetDma` generic branch) or
     `sceSifGetOtherData` whose destination is inside the window logs every word
     (`[sif:msl-window-dma]`); the bytes are the event format the native command engine must
     reproduce.
  3. **`other_data` logged** — the IOP→EE direction was invisible without an env-set watch;
     every transfer now prints direction/size/leading words (`[sif:other-data]`).
- Decode state for steps 2-3 (from the 00:40 log + the in-repo oracle): MSL owns sid
  `0x00012345` (handler entry `0x32BF4`, request globals, dispatch cascade `0x32F00-0x32FF8`);
  the play path exists in the body (voice-start `0x33B68`, `SetAddr` at `0x33C24`, 14 call
  sites; libsd stubs `SetParam=0x28388 SetSwitch=0x28390 SetAddr=0x283A0 VoiceTrans=0x283B0`)
  but never executes — the sequencer polls the state table instead; **no DMA to `0x4C9E4`
  appears anywhere in the 00:40 log** (`dtx-dma` targets the ADX buffers `0x84440/0xE1940`),
  which is exactly what this build records.
- Suites (final binaries): `test_spu2_mix` PASS, `test_gl_vs_raster` 18/18,
  `test_native_gl_alpha` PASS, `test_gl_lazy_readback` PASS, `test_iop_native_module` PASS.
- Replaced runner: `D8266FCFAEA80FAA95E7AD7EADF7997A73513920780ABD154C5401B4886D7013`
  (`7768a5d clean` — movie pin + blit-into-every-target)
- Next: step 2 (finish the op4-gate and `0x33B68` decode against the captured bytes from this
  build's run), step 3 (runtime-owned endpoint + native command engine + native voice start),
  step 4 (acceptance: `guest-registered → 0` for this sid, `voices keyed > 0`, audible SFX).
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075BB1…` → `78a0c1f6…` → `daef9fb5…`
  → `d8266fcf…` → `52102daf…`

# Build delivery — 2026-11-16 (movie presentation pin + blit-into-every-target)

- Commit: `7768a5d` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit 7768a5d clean, built 2026-11-16T14:49:59Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 14:49, 75,977,728 bytes
- SHA-256: `d8266fcfaea80faa95e7ad7eadf7997a73513920780abd154c5401b4886d7013`
- What it adds — the movie shake fix (the regression the quiet-gated present introduced):
  the 00:40 run measured `blit=NO` on write-presents / `blit=YES` on the next with the
  presented target alternating FBP 0/140, so presents alternated between the fresh blit and
  the other buffer's quad. Two changes in `present_frame`:
  1. **Pinned presentation surface** — while a movie surface (FBP 280) exists, presentation
     sticks to the target selected first instead of re-running the last-write-tick selection
     (the game quads into whichever buffer drew last, which is what alternated them). The
     pin releases when no movie surface exists and normal selection resumes.
  2. **Blit into every scene target** — the quiet-gated blit now copies the complete movie
     frame into all ready scene targets (not just the selected one), so buffer alternation is
     invisible. Each blitted target takes the movie's `write_count`/`last_write_tick` and
     `cpu_synced` is cleared (the CPU-side copy is stale; `ensure_scene_target` already syncs
     an unsynced target before recycling it).
- Suites (final binaries): `test_spu2_mix` PASS, `test_gl_vs_raster` 18/18,
  `test_native_gl_alpha` PASS, `test_gl_lazy_readback` PASS, `test_iop_native_module` PASS.
- Replaced runner: `daef9fb5b24edaa7927d387508747c81e3d04062bb8172ea96ccf9ffee4bf570`
  (`b07558f clean` - no CPU raster, general ALPHA blend, atexit census UAF fix)
- Next: increment 4 - the native sound service (decision B: runtime-owned SIF endpoint for
  `sid 0x00012345 / fn 56026`, native command engine over the captured MSL/SDRDRV bodies as
  oracle, those AOT bodies no longer loaded).
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075BB1…` → `78a0c1f6…` → `daef9fb5…`
  → `d8266fcf…`

# Build delivery — 2026-11-16 (no CPU raster, general ALPHA blend, atexit census UAF fix)

- Commit: `b07558f` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit b07558f clean, built 2026-11-16T07:57:38Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 02:57, 75,977,728 bytes
- SHA-256: `daef9fb5b24edaa7927d387508747c81e3d04062bb8172ea96ccf9ffee4bf570`
- What it adds, from the 00:40 run's diagnosis (host at 54% of real time, 77% of primitives
  dropped, character select broken):
  1. **No CPU rasterization on the native route** (user directive 2026-11-16). `kick()` fell
     back to `raster::draw` for every rejected packet, and 6,985,985 of 9,105,842 primitives
     were rejected - millions of software-rasterized primitives per run were where the frame
     time went. Rejected packets are now counted in the census and dropped; the software
     rasterizer stays available when native GL is not selected.
  2. **The general GS ALPHA blend** - the character-select fix and the drop-count fix in one:
     the tuple whitelist is replaced by an exact mapper of the rasterizer's equation
     `(A - B) * C / 128 + D` (raster.cpp:267-272, alpha from source per raster.cpp:282-286)
     onto GL blend factors (`{0, 1, C, 1-C}` per coefficient; `C+1`/`-C` stay rejected and
     counted). C ∈ {As, Ad, FIX}; FIX>0x80, non-PSMCT32, missing COLCLAMP and TFX>=2 stay
     rejected with **per-tuple census keys** (pointer-keyed census map, stable key table).
     `glBlendColor`/`GL_CONSTANT_ALPHA` are GL 1.4, loaded via `wglGetProcAddress` with
     guarded defines. The depth check accepts any `0x3X` ZBUF format to match the
     rasterizer's default-to-32-bit behavior (`z_max`/`write_pixel`), which is where the
     front-end's ZBUF nibble `0x0A` (zpsm `0x3A`, FBP 91) lands.
  3. **Atexit census use-after-free fix** - the flaky `0xC0000005` in `test_spu2_mix`
     (4 crashes in 7 runs), caught live with the project's x64dbg MCP: RIP on
     `mov rax,[rax]` with `RAX=0xFEEEFEEEFEEEFEEE` (heap freed-fill) inside the census
     report's `std::map` walk. The test only reaches `report()` from atexit (registered in
     compliance_census.cpp); `g_modules` is a `std::map` in another translation unit,
     cross-TU static destruction order is unspecified, and its destructor ran before the
     atexit report, which then walked freed nodes - surviving runs were the ones where the
     freed heap had been reused (one printed a mangled module name); an empty module map
     cannot crash, which is why the GL-only suites never showed it. `g_modules` is now
     allocated once and never destroyed. Verified **10/10** `test_spu2_mix` runs exit 0 with
     complete census + PASS line (was 3/7) and **3/3** `test_iop_native_module` runs.
  4. **test_native_gl_alpha contract upgrade**: `ALPHA=0xA8 FIX=0x40` is no longer asserted
     rejected (old fail-closed contract for unimplemented tuples); it now checks agreement
     with the reference rasterizer, same as the `FIX=0x80` case.
- Suites: `test_spu2_mix` PASS (census complete), `test_gl_vs_raster` 18/18,
  `test_native_gl_alpha` PASS, `test_gl_lazy_readback` PASS, `test_iop_native_module` PASS
  (the two `fn=0x06/0x07 -> FAIL` lines are its pre-existing negative-path probes).
- Replaced runner: `78a0c1f6aad99fad5e8a32d0c62a83d325b2dca6a321a5dd903d77cf5bd43533`
  (`00c0cee clean` - full payloads, quiet movie present, batched flush, character-select work)
- Still open (named by the census): the remaining per-tuple ALPHA rejects (if any appear as
  per-tuple keys), the movie shake (blit alternates FBP 0/140), game SFX silence
  (native sound service, decision B), reflections, and the gate items
  (`SPU2 chip-model ops`, `GS stopped in emulated VRAM`, `CDVD device reads`,
  `SIF guest-registered`).
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075BB1…` → `78a0c1f6…` → `daef9fb5…`

# Build delivery — 2026-11-16 (full payloads, quiet movie present, batched flush, character-select rendering)

- Commit: `00c0cee` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit 00c0cee clean, built 2026-11-16T05:26:12Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 00:26, 75,976,192 bytes
- SHA-256: `78a0c1f6aad99fad5e8a32d0c62a83d325b2dca6a321a5dd903d77cf5bd43533`
- What it adds, from the 22:50 run's diagnosis:
  1. **Full MSL payload dump** — all 1,536 words of the MSL command buffer (was 16): the SFX
     question lives past the setup prefix.
  2. **Quiet movie present** — the blit runs while FBP 280 is quiet (no VRAM write since the
     previous present), holding the last whole frame between complete frames; the old gate never
     fired (0 of 16,691) and the quad tore on 8,328 presents. Fallback blit after 8 changed
     presents so a never-quiet stream cannot freeze.
  3. **Batched log flush** — every 64 KB / 50 ms instead of per record (~14k write syscalls/s
     were a share of the frame time); census lines still flush immediately.
  4. **Character select** — the census now counts dropped packets per rejecting site (the log
     fires once per site, which hid the scale; all three sites hit the front-end surfaces FBP 91
     and 140), and two of the three were implemented: **alpha test** AFAIL=0 via `glAlphaFunc`
     (ATST/AREF from TEST bits 1-3/4-11 per raster.cpp:335-336, reference `aref/128`; aref>128
     and AFAIL=2/3 stay rejected and counted), and **fog** via GL fixed-function fog with the
     per-vertex coordinate `(255-F)/255`, matching the rasterizer's
     `(F*color + (255-F)*fogcol) >> 8` (within one 8-bit unit; `glFogCoordf` loads through
     `wglGetProcAddress`, else the primitive is rejected and counted).
- Replaced runner: `47075BB124FFFAB3225287A39DF56CFC941F428D22EA02CC1EFDCF4FBC28D0BF`
  (`c9cfbb7 clean` — no-truncation logging)
- Still open, named by the new counters: the **ALPHA tuple for textured DECAL** (alpha=0x44 with
  tfx=1, rejected on FBP 140) and **reflections** (planned: verify the framebuffer-as-texture
  path; check whether a rejected packet blocks them).
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075BB1…` → `78a0c1f6…`
- The `13424cdb…` runner was run live once (`ps2run_2026-11-15_17-57-43.log`): card fixed, all
  four AOT bases loaded, crashed at the movie's `stdio v0103` import — record in
  `research/runs/aot-layout-crash-20261115/RUN.md`.
- Earlier the same day the `6b2335a1…` runner was run once (`ps2run_2026-11-15_16-54-43.log`),
  crashed at `LIBSD` (fixed in `7f2f3d9`), and verified the shutdown flush: `census.txt` was
  written with the complete report including the `VERDICT:` line.

## Why the census was lost, and what actually limits a log

Run 2 (`games/mkarmageddon/disc/logs/ps2run_2026-11-15_14-21-00.log`) ends mid-table at line
44571. The cause is not size:

- the **log file ceiling is 64 MB** (`LogTee::kFileLimit`, `host/main.cpp:238`, enforced at
  `:277` — the file stops and appends "[log] this file reached 64 MB and stops here");
- the **1 MB pipe** (`_pipe(fds, 1 << 20, ...)`) is backpressure only: a writer blocks when it is
  full, it never drops bytes;
- what loses data is **process death with undrained bytes in the pipe**: the census is printed from
  the SIGABRT handler, `raise(SIGABRT)` ends the process without the SEH filter, so
  `LogTee::flush()` never runs and the pump thread never moves those bytes into the file.

## What changed

- `compliance_census`: `set_exit_flush_hook()`, called from `report()` and from every shutdown
  path (atexit, SIGABRT, SIGTERM); the verbose GS by-base table is kept **complete** (the earlier
  12-base cap was reverted: it removed detail for a problem it could not cause — the table is
  ~1.5 KB).
- `host/main.cpp`: registers the hook, which now (a) flushes the log pipe and (b) copies the
  `[census]` lines out of `LogTee`'s in-memory ring into `census.txt` next to the log, so the
  report survives even a death no handler can catch.
- `kernel.cpp` / `iop/cpu.cpp`: `report()` is called before the strict-mode abort at the AOT
  placement stop and the IOP stop, so the report is produced in normal control flow.

## Gates

1. A run that aborts under `PS2_IOP_NATIVE_STRICT=1` must leave a log containing the whole
   `[census]` block down to `VERDICT:`, and `census.txt` must contain the same block.
   **Met live** by `ps2run_2026-11-15_16-54-43.log` + `census.txt` (891 bytes): the report
   survives the abort; the log's final line is the GS by-base table, which `report()` prints
   after the verdict by design.
2. A normal run must leave both the log and `census.txt` complete (VERDICT line present).

## Suites against this binary

`test_spu2_mix` PASS · `test_gl_vs_raster` 18/18 PASS · `test_native_gl_alpha` PASS ·
`test_gl_lazy_readback` 10/10 PASS · `test_iop_native_module` PASS (all exit 0).

Verified live: the shutdown flush and `census.txt` (see the 16:54 run), and the AOT footprint
protection — which was exercised live and found a real bug (the reservation rejected its own
footprint; fixed in `7f2f3d9`, record in `research/runs/aot-layout-crash-20261115/RUN.md`).
Still unverified live: the shape-based XMCSERV 0x0C/0x0D dispatch, the named `libsd` tracing,
and the mode-2 reservation fix itself. Newest live evidence before this build: the Run 2 census
(fork `c25fef6`) plus the 16:54 crash run.
