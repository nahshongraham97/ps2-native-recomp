# Movie-2 Reload: Resident AOT Module Reuse (2026-11-15)

## Summary & Delivery

- Binary: `build_native_gl_full/ps2run.exe`, SHA-256 `67E71461BA0953458CA43527E37FD0685AF2DF49423B73BF12651F01355DF874`.
- Staged architecture: **Resident AOT Module Reuse (resolve Movie-2 `CRI_ADXI` reload crash)**.
- Record type: Implementation, verification of 5 regression suites, and gate compliance.

## Implementation Details

1. **Resident AOT Module Reuse (`runtime/src/iop/kernel.cpp`)**:
   - In `load_module_buffer`:
     - Before invoking mode-2 `alloc` for `aot_layout->base` (`0x00071300`), the loader scans resident modules in `g_modules`.
     - When an existing module with matching name (`CRI_ADXI` / `CRI_ADX_Driver` / layout name) and `m.base == aot_layout->base` is found, the loader logs:
       `[iop-aot-layout] reusing resident AOT module <name> (id=<id> base=0x... size=0x...)`.
     - Calls completion callback `if (done) done(resident_id, 0);` and returns `resident_id`.
     - Prevents fixed-address mode-2 reservation collision and duplicate thread initialization.
2. **Resident Native Replacement Reuse & Base Tracking**:
   - Added matching reuse check in native replacement path before `reserved_aot_base = alloc(...)`.
   - Updated native module registration to record `reserved_aot_base` and `reserved_aot_size` into `g_modules[id]`.

## Regression Test Suite Verification

| Test Suite | Result | Details |
|---|---|---|
| `test_iop_native_module.exe` | **PASS** | Native SIF/IOP lifecycle, libsd start/stop stream, and census compliance clean. |
| `test_spu2_mix.exe` | **PASS** | SPU2 voice key-on, master volume, and audio generation verified clean. |
| `test_gl_vs_raster.exe` | **PASS** | 18/18 GS texture sampling & font atlas acceptance matrix tests passed. |
| `test_gl_lazy_readback.exe` | **PASS** | 10/10 lazy on-demand VRAM sync and readback tests passed. |
| `test_native_gl_alpha.exe` | **PASS** | Alpha blending matches reference. |
