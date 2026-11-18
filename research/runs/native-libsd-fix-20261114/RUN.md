# Native LIBSD Fidelity Fix (Phase D-1, Audio & Movie Unblock) — 2026-11-14

> Dated-run audit boundary (2026-11-18): All implementation/current/verified language here refers to the named historical source, build and scenario. Original counter values, outputs, observations and artifact IDs are preserved as reports, not independently revalidated facts. “Native” component names, successful builds/tests and old census verdicts do not establish host resource ownership, card/profile behavior, correct sound, physical input, movie A/V sync or whole-game acceptance. Read the canonical parent PORT_STATUS.md and execution contract for current blockers; unresolved historical causal/source claims remain unresolved.

## Context & 23:00 Analysis

Analysis of the recorded run `games/mkarmageddon/disc/logs/ps2run_2026-11-14_23-00-16.log` against the rebuilt binary (`9e15d6b5...`):
- **Audio delivery**: Healthy — "the device got 23.52 s of sound in 23.81 s; the queue ran dry 0 times, 0 samples dropped". The device routing is functioning properly.
- **Chip voice count**: `[spu2] 0.0 voices per batch` — the sound chip never started a sound.
- **Movie playback break**: At line 9786 (CRI ADX movie decoder init), `[iop] missing libsd #28 called from 0x000C3E18`.
- **DTX pipeline stall**: At line 9820, the movie GS transfer (`dbp=8960, 16x448`) starts and never finishes; presentation remains on the scene path (`owner=primitive / write=none`) and the EE idles in ~134 zero-event polls.
- **Root cause comparison**: The working backup loaded the real Sony `LIBSD.IRX` into IOP (`module 10 LIBSD at 0x00099E00`), where ordinal 28 existed and DMA transfer interrupts were delivered. The native bridge replaced it but lacked ordinals 28/29 and never dispatched transfer interrupts to the registered guest handlers (`g_transfer_intr_handler`), freezing ADX audio/video synchronization.

## Sony LIBSD.IRX Disassembly & Audit

Inspection of `games/mkarmageddon/disc-files/MODULES/LIBSD.IRX` (Sony LIBSD v1.5):
1. **Ordinal 28 (`0x41a8`)**: Loads `g_transfer_intr_argument[core]` stored at offset `0x565c` by `sceSdSetTransIntrHandler` (export 26). This is `sceSdGetTransIntrArg(int channel)`.
2. **Ordinal 29 (`0x4208`)**: Loads `g_spu2_intr_argument` stored at offset `0x5634` by `sceSdSetSpu2IntrHandler` (export 27). This is `sceSdGetSpu2IntrArg()`.
3. **`sceSdProcBatchEx` (`0x35e8`)**: Disassembly confirms that Sony LIBSD iterates over matching voices using `(entry & ~0x3E) | (voice << 1)` with `sll $v0, $s0, 1`. The native bridge previously inherited freesd's typo `1u << (voice + 1u)` and subtracted from `return_index`, corrupting voice parameter writes for voice indices $\ge 1$.
4. **Transfer completion**: On DMA channel 4/7 completion, `TransIntrHandlers[core](core, data)` must be invoked via `call_guest`, setting `VoiceTransComplete` and updating block loop buffers.

## Implementation Details

1. **`libsd_native.cpp` / `libsd_native.hpp`**:
   - Implemented `export_get_trans_intr_arg` (ordinal 28) and `export_get_spu2_intr_arg` (ordinal 29).
   - Export table updated with entries 28 and 29; all 34 ordinals (4..33) of Sony LIBSD 1.5 are now fully registered.
   - Corrected `process_batch` voice entry bit-formatting to `(entry & ~0x3Eu) | (voice << 1u)` and removed the invalid decrement of `return_index`.
   - Added `on_dma_done(core)` to toggle loop buffers and fire `g_transfer_intr_handler[core]` or `g_transfer_callback[core]`.
2. **`hw.cpp`**:
   - In `dma_done(unsigned ch)`, routed DMA channels 4 and 7 to `libsd_native::on_dma_done(0)` and `libsd_native::on_dma_done(1)`.
3. **`tests/test_spu2_mix.cpp`**:
   - Added Phase 3 test validating ordinals 26, 27, 28, and 29 end-to-end.

## Verification

- `test_spu2_mix`: PASS (Phase 1 silence gate, Phase 2 master volume peak 28667, Phase 3 ordinals 26..29 verified).
- `test_gl_vs_raster`: ALL 18 MATRIX TESTS PASSED (including real font atlas, nearest-neighbor, and alpha source-over).
- `test_native_gl_alpha`: PASS (`ALPHA=0x44` source-over match).
- `test_gl_lazy_readback`: ALL 10 TESTS PASSED (lazy CPU VRAM synchronization and cache eviction).
- Rebuilt runner: `build_native_gl_full/ps2run.exe` (SHA-256 `60FBC95024D30A5F311709009516BA77E3821F95391121639796362F8882D64A`).
