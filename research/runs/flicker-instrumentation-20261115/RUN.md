# Flicker Discrimination & MSL Trace Expansion (2026-11-15)

## Summary & Delivery

- Binary: `build_native_gl_full/ps2run.exe`, SHA-256 `CD6866754C5932B7878EDA983E3A5072E2A95D44C4CB81D88D64CFA6982C656A`.
- Staged architecture: **Flicker Discrimination Diagnostics & 500-line MSL SIF Trace Cap**.
- Record type: Implementation, verification of 5 regression suites, and gate compliance.

## Implementation Details

1. **Movie Present & Flicker Discrimination (`runtime/src/gs/native_gl_backend.cpp`)**:
   - For every present cycle where a movie source (`FBP=280`) exists with writes:
     - Captures presented target ID (`target->fbp`), `prim_count`, `write_count`, and `last_write_tick`.
     - Logs offscreen movie source writes and whether the conditional blit ran (`target->prim_count == 0 && target->write_count == 0`).
     - Detects VRAM generation changes (`gs::g_vram_generation`) mid-frame to discriminate torn/stale texture cache uploads from blit starvation.
     - Tracks structured diagnostics: `[native-gl-movie-present #N] target_fbp=... (prims=..., writes=..., tick=...) movie280_writes=... blit=YES/NO vram_gen_changed=...`.
2. **Compliance Census (`runtime/src/compliance_census.{hpp,cpp}`)**:
   - Added `movie_present(bool blit_ran, bool source_existed)` to record movie presentation telemetry.
   - Reports `[census] MOVIE : presents with blit = %llu, blit skipped = %llu`.
3. **MSL SIF Payload Trace Expansion (`runtime/src/iop/sif.cpp`)**:
   - Raised the logging cap for SID `0x00012345` from 20 to 500 requests (`s_msl_log_count++ < 500`).
   - Ensures game SFX commands, button presses, and sound trigger RPC payloads are fully visible in the `--trace-iop` log.

## Regression Test Suite Verification

| Test Suite | Result | Details |
|---|---|---|
| `test_iop_native_module.exe` | **PASS** | Native SIF/IOP lifecycle, libsd start/stop stream, and census compliance clean. |
| `test_spu2_mix.exe` | **PASS** | SPU2 voice key-on, master volume, and audio generation verified clean. |
| `test_gl_vs_raster.exe` | **PASS** | 18/18 GS texture sampling & font atlas acceptance matrix tests passed. |
| `test_gl_lazy_readback.exe` | **PASS** | 10/10 lazy on-demand VRAM sync and readback tests passed. |
| `test_native_gl_alpha.exe` | **PASS** | Alpha blending matches reference. |
