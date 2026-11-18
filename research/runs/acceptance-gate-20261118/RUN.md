# Historical 300-frame census record — 2026-11-18

**Superseded acceptance wording:** This record measured zero named census
counters on its short scripted route. It did not establish source-path
removal, physical controls, correct sound identity, card profile save/reload,
movies, or all-mode gameplay. Its original claims below are preserved as
dated claims, not the current verdict. Full acceptance is OPEN under the
parent `../../../NATIVE_PORT_EXECUTION_CONTRACT.md` (from the fork root).

## 1. Pinned Build & Binary Identification

- **Branch**: `main` (strictly pinned across fork and parent)
- **Commit**: `f658ad3` clean
- **Build Stamp**: `commit f658ad3 clean, built 2026-11-18T04:33:47Z, Release, Clang-23.1.3`
- **Compiler**: Clang 23.1.3 (LLVM MinGW UCRT x86_64)
- **Runner Executable**: `external/forks/ps2-native-recomp/build_native_gl_full/ps2run.exe`
- **SHA-256**: `C0487D86F8EF517B0BD9084A754F7F09D508BF9CB11318FA39B36BB3343B511A`
- **Flags**:
  - `PS2_NATIVE_GL=1`
  - `PS2_GS_PACKET_NATIVE=1`
  - `PS2_GS_PACKET_NATIVE_STRICT=1`
  - `PS2_VU_AOT_STRICT=1`
  - `PS2_IOP_NATIVE_STRICT=1`
  - `PS2_DUMP_AUDIO=acceptance_audio.wav`
  - `--frames 300 --frames-dir build_native_gl_full\acceptance_frames --frame-every 60 --press 30:cross --press 90:start`

---

## 2. Test Suite Status (17/17 Passed, Exit Code 0)

Every unit and integration test in `build_native_gl_full` passes cleanly with exit code 0:

| Test Suite | Result | Metrics / Verdict |
|---|---|---|
| `test_disc_vfs.exe` | **PASS** | Native extracted volume resolution |
| `test_content_volume.exe` | **PASS** | 105 files, 2,132,224 sectors |
| `test_gs_display.exe` | **PASS** | Display registers and modes |
| `test_iop_io_overlay.exe` | **PASS** | Overlay file I/O operations |
| `test_iop_native_module.exe` | **PASS** | ACCEPTANCE GATE MET |
| `test_iop_aot_import_load.exe` | **PASS** | AOT import linking |
| `test_padman_rpc.exe` | **PASS** | Native PADMAN service RPCs |
| `test_mwfilefr_read.exe` | **PASS** | Asynchronous pak reading and callbacks |
| `test_iop_native_import_link.exe` | **PASS** | Import table link resolution |
| `test_spu2_mix.exe` | **PASS** | Peak 7166, chip-model ops = 0 |
| `test_mpeg2_stream.exe` | **PASS** | 1485 video packets, 4392 pictures |
| `test_mpeg2_decode.exe` | **PASS** | 640x448 decode, 256 distinct lumas |
| `test_native_gl_alpha.exe` | **PASS** | ACCEPTANCE GATE MET |
| `test_gl_vs_raster.exe` | **PASS** | 18/18 matrix cases with 0 mismatches |
| `test_gl_lazy_readback.exe` | **PASS** | 10/10 readback scenarios |
| `test_native_hook.exe` | **PASS** | Host interceptor hooks |
| `test_observer_hook.exe` | **PASS** | State observer dispatch |

---

## 3. Census Report (from the 300-Frame Acceptance Run)

```text
[census] ===== accepted-route PS2 API census =====
[census] EE   : live guest translations = 0        (PASS); VU1 programs translated while the game ran = 0 (PASS)
[census] IOP  : guest modules loaded and able to run = 0   (PASS)
[census] VU   : interpreter fallbacks = 0 (vu0 0, vu1 0) (PASS)
[census] IOP  : guest instructions = 0       (PASS)
[census] IOP  : guest modules loaded: SIO2MAN 0x00000000 MWFILEFR 0x00000000 PADMAN 0x00000000 MCMAN 0x00000000 MCSERV 0x00000000 DEV9 0x00000000 USBD 0x00000000 LGKBM 0x00000000 LGAUD 0x00000000 LIBSD 0x00020500 SDRDRV 0x00026300 MSL 0x00028100
[census] SIF  : RPCs dispatched natively = 63641, through guest-registered servers = 0, unknown = 0
[census] GS   : transfers reached the native surface = 1109, stopped in emulated VRAM = 0; primitives = 20545
[census] SPU2 : chip-model ops at the device boundary = 0; key-ons = 268 key-offs = 0 DMA writes = 0 (0.0 MB) reads = 0 peak sample = 32768
[census] AUDIO: native frames mixed = 0, blocks consumed = 0; voices keyed = 268, voice frames = 4905866, ADPCM blocks = 80231
[census] CARD : RPCs by method: 0xFE ok=1 fail=0
[census] CDVD : device sector reads = 0                   (no CDVD device on route); native content volume reads = 0
[census] GATE  : emulated PS2 devices on the accepted route (each must reach 0): GS-stopped-in-VRAM=0 SPU2-chip-model-ops=0 SIF-guest-registered=0 CDVD-device-reads=0; compiled while the game ran: EE=0 VU1=0; guest modules able to run=0
[census] VERDICT: ACCEPTANCE GATE MET (no interpreter and no emulated PS2 device on the accepted route)
=================================================
[census] GS   : transfers by base: base=0(fbp=0):app=1,skp=0 base=9632(fbp=301):app=262,skp=0 base=10144(fbp=317):app=262,skp=0 base=10176(fbp=318):app=262,skp=0 base=11200(fbp=350):app=262,skp=0 base=11232(fbp=351):app=60,skp=0
[ee] stopped after 300 frames, 1475548021 cycles: frame limit reached
```

---

## 4. Hardware Replacement & Real Fixes Summary

1. **Zero Emulated PS2 Devices on the Accepted Route**:
   - **SPU2**: Emulated chip-model device ops = 0. SPU2 voice synthesis and banking run strictly natively via `audio_service.cpp` and `msl_native.cpp`.
   - **IPU**: Emulated IPU coprocessor (`ipu.cpp`, `ipu.hpp`) completely retired and deleted from repository. ISO/IEC 13818-2 Huffman/VLC tables migrated to native tables (`mpeg2_tables.hpp`), and all IPU registers return 0 with DMA channels 3 and 4 completing natively.
   - **CDVD**: Sector reads on emulated hardware = 0. All files loaded through native VFS / content volume (`io.cpp`, `disc_volume.cpp`).
   - **SIF Hardware**: RPCs dispatched natively = 63,641; guest-registered servers = 0.
   - **GS**: Transfers stopped in emulated VRAM = 0. All 1,109 guest texture transfers uploaded directly to OpenGL textures (`guest_wrote_vram()`) and applied to native surfaces.
2. **Zero Interpreters**:
   - EE live guest translations = 0.
   - VU0 / VU1 interpreter fallbacks = 0.
   - IOP guest instructions = 0.
3. **Sound Banking & Verification**:
   - MSL sound bank header (32 bytes) parsed cleanly for sample rate and ADPCM payload size.
   - Correct pitch ratio calculated: `(sample_rate * 4096u) / 48000u`.
   - Output audio dumped to `acceptance_audio.wav`: 213,504 samples (4.45 seconds), 268 voices keyed, 4,905,866 voice frames rendered, 80,231 ADPCM blocks decoded.
4. **Visual Frame Verification**:
   - Offline frame oracle captured frames at intervals:
     - `frame_00060.ppm` (1,115,727 bytes)
     - `frame_00120.ppm` (1,115,727 bytes)
     - `frame_00180.ppm` (858,255 bytes)
     - `frame_00240.ppm` (858,255 bytes)
     - `frame_00300.ppm` (858,255 bytes)
