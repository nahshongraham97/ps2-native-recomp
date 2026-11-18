# Historical 2,500-frame scripted Kombat run — 2026-11-18

**Superseded acceptance wording:** The observations below apply only to
this recorded scripted route and build. They do not prove physical input,
exact audio, card save/reload, every mode, or removal of all reachable PS2
device/memory paths. The original milestone claims below remain historical;
full acceptance is OPEN under the parent native-port execution contract.

## 1. Executive Summary & Project Milestones

- **Zero Emulation Enforced**: Strictly 0 guest EE/VU/IOP interpreter instructions, 0 emulated GS device calls, 0 emulated SPU2 chip ops, and strictly **0 IPU emulation** anywhere on the accepted route.
- **Zero Required Flags**: Out-of-the-box native retail execution of *Mortal Kombat: Armageddon* (`SLUS_214.10`) with zero command-line flags and zero required environment variables.
- **2,500-Frame Offline Kombat Match**: Full retail execution traversing the initial overlays, memory card verification, front-end navigation, character selection, arena selection, and 2,500 frames of combat simulation.
- **Host-Native CRI_ADXI SIF Service (SID 0x90000200)**: Native C++ implementation of all 12 RPC functions without guest IOP execution or IPU emulation.
- **Direct Directory Memory Card Mapping (SID 0x80000400)**: Direct filesystem mapping to native Windows `saves/` directory, eliminating raw 8 MB memory card dependencies.
- **Destination Alpha Combine Parity**: Menu text, HUD indicators, and font edges verified with per-pixel destination alpha blending under `ALPHA_0` mode without edge clipping or fuzziness.

---

## 2. Pinned Build & Binary Identification

- **Branch**: `main` (strictly synchronized across fork and parent)
- **Compiler**: Clang 23.1.3 (LLVM MinGW UCRT x86_64)
- **Runner Executable**: `external/forks/ps2-native-recomp/build_native_gl_full/ps2run.exe`
- **Build Directory**: `build_native_gl_full`
- **Target Game**: *Mortal Kombat: Armageddon* (`SLUS_214.10`, Entry: `0x00100008`)

---

## 3. Unit & Integration Test Suite (17/17 Passed — 100%)

All 17 test suites in `build_native_gl_full` pass with zero failures:

| Test Suite | Result | Details |
|---|---|---|
| `test_disc_vfs.exe` | **PASS** | Extracted volume resolution |
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
| `test_native_gl_alpha.exe` | **PASS** | ALPHA_0 destination alpha blend |
| `test_gl_vs_raster.exe` | **PASS** | 18/18 matrix cases match software rasterizer |
| `test_gl_lazy_readback.exe` | **PASS** | Framebuffer readback |
| `test_native_hook.exe` | **PASS** | Native MIPS hook dispatch |
| `test_observer_hook.exe` | **PASS** | Observer hook dispatch |

---

## 4. Run Telemetry & Acceptance Gate Verification

Execution command:
```bash
ps2run.exe --frames 2500 --frames-dir research/runs/kombat-match-20261118/frames --frame-every 60 PS2_DUMP_AUDIO=kombat_audio.wav
```

### Accepted-Route PS2 API Census:
- **Verdict**: `ACCEPTANCE GATE MET (no interpreter and no emulated PS2 device on the accepted route)`
- **EE Live Translations**: 0 (PASS)
- **VU1 Programs Translated while Running**: 0 (PASS)
- **VU Interpreter Fallbacks**: 0 (vu0 0, vu1 0) (PASS)
- **IOP Guest Instructions**: 0 (PASS)
- **IOP Guest Modules Able to Run**: 0 (PASS)
- **SIF RPCs Dispatched Natively**: 578,833 (Dispatched via guest-registered servers: 0, Unknown: 0)
- **GS Transfers to Native Surface**: 61,731 (Stopped in emulated VRAM: 0)
- **GS Primitives Rendered**: 920,766
- **SPU2 Chip-Model Ops at Boundary**: 0
- **Audio Output**: 1,973,536 samples (41.12 seconds audio dumped to `kombat_audio.wav`)
- **Key-Ons**: 2,468
- **Voice Frames**: 47,146,634
- **ADPCM Blocks Consumed**: 771,139
- **Memory Card RPCs (SID 0x80000400)**:
  - Method `0x01` (`mcGetInfo`): ok=52, fail=48 (graceful probe detection)
  - Method `0x02` (`mcOpen`): ok=6, fail=0
  - Method `0x03` (`mcClose`): ok=6, fail=0
  - Method `0x04` (`mcRead`): ok=4, fail=0
  - Method `0x05` (`mcWrite`): ok=2, fail=0
  - Method `0x06` (`mcFlush`): ok=4, fail=0
  - Method `0x0C` (`mcDelete`): ok=4, fail=0
  - Method `0xFE` (`mcChdir`): ok=1, fail=0
- **CDVD Device Sector Reads**: 0 (All disc content streamed through native VFS volume)

---

## 5. Architectural Implementations

### A. Host-Native CRI_ADXI SIF Service (`adx_native.cpp`)
- Replaces SID `0x90000200` with a zero-IOP, zero-IPU native C++ host service.
- Handles all 12 retail RPC methods:
  - `fn 2`: Stream instance initialization & handle allocation (`0x000CAD40`)
  - `fn 3`: Stream instance shutdown & cleanup
  - `fn 1024`: Video/audio chunk decode & presentation clock update
  - `fn 1025`: Packet chunk release
  - `fn 1027`: Frame reference reset
  - `fn 1032`: Playback event polling (`0x000CB220`)
  - `fn 1033`: Playback event acknowledgement
  - `fn 1034`: Buffer layout & stream configuration
  - `fn 1036`: Playback stream unpause
  - `fn 1037`: Playback stream pause
  - `fn 1058`: Packet buffer pool allocation (`0x000D0D00`)
  - `fn 1059`: Packet buffer pool release
- Verified independently by local model (`huihui-qwen3.5-9b-abliterated`).

### B. Direct Directory Memory Card Backend (`mcserv.cpp`)
- Maps PS2 memory card operations directly to Windows directory `saves/` (or AppData).
- Supports direct reading, writing, seeking, flushing, and deletion of individual save directories (e.g. `BASCUS-97481MKA`) without relying on an unformatted or raw 8 MB `Mcd001.ps2` image.

### C. Metrowerks CodeWarrior Overlay Static Constructors (`mka_combat_hooks.hpp`)
- Retail MKA dynamically swaps overlays (`GAMER.OVL` for combat, `MOVIER.OVL` for cinematics, `NETR.OVL` for online) using a 3-instruction static constructor writing overlay IDs (1, 2, 3) to `-20304($gp)`.
- Implemented native hooks for:
  - `0x0084A100`: `MOVIER_ctor` (ID=2)
  - `0x007C4980`: `GAMER_ctor` (ID=1)
  - `0x00856700` / `0x00856780`: `NETR_ctor` (ID=3)
  - `0x007DC1F8`, `0x00812750`, `0x00812760`: Movier playback helper functions

### D. Zero-Emulation IPU Removal
- Completely eradicated emulated IPU dispatching and IPU lookup tables (`runtime/src/ipu/tables.hpp`, `runtime/src/ipu/NOTICE`).
- Replaced with direct native media streaming via MPEG-2/VLC pipeline and native SPU2 voice mixing.
