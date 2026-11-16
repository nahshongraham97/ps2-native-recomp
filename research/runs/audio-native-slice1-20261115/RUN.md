# Audio Removal Slice 1 — Native ADMA Stream Path (2026-11-15)

## Summary & Delivery

- Binary: `build_native_gl_full/ps2run.exe`, SHA-256 `7B520BE8CD16D26F723FCB9B438A65BA103E9C0FE47D2AC4C5D82C5A011E813B`.
- Staged architecture: **Audio Removal Slice 1 (Native ADMA Stream Path)**.
- Record type: Implementation, verification of 5 regression suites, and gate compliance.

## Implementation Details

1. **Native Stream Mixer (`runtime/src/iop/audio_native.{hpp,cpp}`)**:
   - Manages per-core stream state: `iop_base`, `total_bytes`, `half_bytes`, `active_buf` (0/1), `current_iop_addr`, `bytes_pulled_in_half`, `active`, `loop`, `direction`.
   - Ring buffer (`kRingFrames = 512` stereo frames = 2048 bytes) mirroring hardware SPU2 FIFO.
   - Refills 1024-byte chunks (256 stereo frames: 512B left, 512B right) from IOP RAM at `current_iop_addr`.
   - When a 2048-byte half-buffer is consumed:
     - Increments `ps2::census::audio_native_block_consumed()`.
     - Signals DMA completion job (`kernel::kJobDmaDone`) to wake guest interrupt handlers.
     - Flips `active_buf = 1 - active_buf` and updates `current_iop_addr` to the next half.
   - `render_stream_sample`: pulls sample-by-sample, applies core `bvol` scaling, adds to `dry`/`wet` mix, and records `ps2::census::audio_native_frame_mixed()` and `ps2::census::spu2_peak_sample()`.

2. **Decoupled libsd Bridge (`runtime/src/iop/libsd_native.cpp`)**:
   - `block_transfer()`: routes ADMA write streams (`kDmaWrite` / `kDmaWriteFrom`) directly to `audio_native::start_stream` without touching SPU2 chip DMA or writing TSA registers 0x1A8/0x1AA.
   - `export_block_trans_status()`: queries `audio_native` for active buffer and address, returning `(active_buf << 24) | (address & 0x00FFFFFFu)`.
   - `stop_transfer()`: queries status and invokes `audio_native::stop_stream(core)`.
   - `on_dma_done()`: omits chip DMA re-queueing when `audio_native::has_active_stream(core)` is active.

3. **SPU2 Chip Model Bypass (`runtime/src/iop/spu2.cpp`)**:
   - `feed_input()`: returns early if `audio_native::has_active_stream(core)`.
   - `render_input()`: delegates to `audio_native::render_stream_sample()`.

4. **Compliance Census (`runtime/src/compliance_census.{hpp,cpp}`)**:
   - Added atomic counters: `audio_native_frames_mixed` and `audio_native_blocks_consumed`.
   - Census report prints `[census] AUDIO: native frames mixed = %llu, blocks consumed = %llu`.

## Regression Test Suite Verification

| Test Suite | Result | Details |
|---|---|---|
| `test_spu2_mix.exe` | **PASS** | SPU2 voice key-on, master volume, and audio generation verified clean. |
| `test_iop_native_module.exe` | **PASS** | Native SIF/IOP lifecycle, libsd start/stop stream, and census compliance clean. |
| `test_gl_vs_raster.exe` | **PASS** | 18/18 GS texture sampling & font atlas acceptance matrix tests passed. |
| `test_gl_lazy_readback.exe` | **PASS** | 10/10 lazy on-demand VRAM sync and readback tests passed. |
| `test_native_gl_alpha.exe` | **PASS** | Alpha blending matches reference. |
