# Retail native-port task board

**Audited 2026-11-23. Whole-game native acceptance: OPEN.** The full instructions, order,
files, and closure evidence are in [ROADMAP.md](ROADMAP.md). This board indexes
each deliverable's verified component progress and whole-game acceptance state;
[PORT_STATUS.md](PORT_STATUS.md) is the live status. The previous baseline board is
archived at `research/history/native-port-docs-2026-11-18/TASK_BOARD.md`.

<!-- PROGRESS:START -->
## Native Reimplementation Progress & Tracking

![Porting Progress](progress.svg)

**Verified Fork Commit**: [`14650fe1`](https://github.com/nahshongraham97/ps2-native-recomp/commit/14650fe1f790e2b57f3352af47ee5798e6a8649a) (in `external/forks/retail-main-22`)

### Primary Function Ledgers & Correspondence
- **PS2 Retail Function Ledger**: [`research/ledger/functions.csv`](research/ledger/functions.csv) (17,510 total catalog functions)
- **Xbox Donor Function Ledger**: [`research/ledger/xbox_functions.csv`](research/ledger/xbox_functions.csv) (22,579 donor functions in `default.xbe`)
- **Cross-Platform Correspondence Map**: [`research/ledger/ps2_xbox_map.csv`](research/ledger/ps2_xbox_map.csv) (17,511 correspondence mapping rows)

### PlayStation 2 Retail Catalog (`SLUS_214.10` + Overlays)
- **In-Scope Completion**: **1.52%** (238 / 15,670 in-scope retail functions)
- **Total Function Catalog**: **17,510** functions (13,138 base `SLUS_214.10` + 4,372 dynamic overlays: 1 `GAMER.ovl`, 2,537 `MOVIER.ovl`, 1,834 `NETR.ovl`)

| Category / Tier | Status Breakdown | Count | In-Scope % |
|---|---|---|---|
| **Implemented** | `ported` (227) + `host-replaced` (11) + `implemented-via-donor` (0) | 238 | 1.52% |
| **Classified Non-Executable / Library** | `sdk-or-library` (2,011) + `pointer-scan-proven` (632) | 2,643 | 16.87% |
| **Open Frontier** | `open` (10,141: 9,038 `SLUS_214.10`, 1,102 `MOVIER.ovl`, 1 `GAMER.ovl`) + `no-direct-callers` (2,648 `SLUS_214.10`) | 12,789 | 81.61% |
| **In-Scope Subtotal** | *Active retail code path to implement/verify* | **15,670** | **100.00%** |
| **Excluded / Stubbed** | `out-of-scope-stubbed` (1,834 `NETR.ovl` overlay functions + 6 `SLUS_214.10` main-executable online functions) | 1,840 | — |
| **Total PS2 Catalog** | *Complete binary disassembly denominator* | **17,510** | — |

### Xbox Donor Catalog (`default.xbe`)
- **In-Scope Donor Utilization**: **0.00%** (0 / 21,321 in-scope donor functions)
- **Total Donor Catalog**: **22,579** discovered functions in `default.xbe`

| Category / Tier | Status Breakdown | Count | In-Scope % |
|---|---|---|---|
| **Implemented Donors** | `implemented-via-donor` (0) | 0 | 0.00% |
| **Classified Non-Executable / SDK** | `sdk-or-library` (2,524) + `no-counterpart` (9) | 2,533 | 11.88% |
| **Open Donor Frontier** | `open` (18,771 unmapped donor candidate code + 17 candidate donor routines pending vectors) | 18,788 | 88.12% |
| **In-Scope Subtotal** | *Donor executable candidate surface* | **21,321** | **100.00%** |
| **Excluded / Stubbed** | `out-of-scope-stubbed` (XONLINE thunks and symbols) | 1,258 | — |
| **Total Xbox Catalog** | *Total default.xbe function inventory* | **22,579** | — |

### Cross-Platform Function Correspondence (`ps2_xbox_map.csv`)
- **Total Mapping Entries**: **17,511**
- **Exact Matches**: **0** (0 isomorphic CFG)
- **Structural Correspondence Matches**: **0** (verified non-vacuous multi-block CFG isomorphism and common mapped callees; simple offsets disregarded)
- **Composite 1-to-Many Matches**: **0** (modular donor splits)
- **Mapped-by-Role Candidates**: **18** (subsystem role correspondence pending dual-platform vectors)
- **Unmapped Retail Functions**: **17,493**

### Still Needed (Open Acceptance Frontier)
1. **No-Direct-Callers Indirect Dispatch Resolution**: **2,648** functions untargeted by direct calls but referenced by data pointers or immediate formations (indirect candidates across 5,400 computed call sites).
2. **Open Game Functions**: **10,141** retail game functions across modules: **9,038** in `SLUS_214.10`, **1,102** in `MOVIER.ovl`, and **1** in `GAMER.ovl`.
3. **Dynamic Overlay Frontends**: **1,102** movie playback frontend functions in `MOVIER.ovl` and **1** entry stub in `GAMER.ovl`.
4. **Acceptance Gates**: Full-game entry link, visible render target presentation, synchronized audio mixing, and physical controller input parity.
<!-- PROGRESS:END -->

| Order | Deliverable | State | Verified progress & open frontier |
| --- | --- | --- | --- |
| 0 | Preserve and hash playable/card oracles, assets, saves, checkout | **Verified / Preserved** | 2026-11-10 playable oracle safety copy (SHA-256 `B9EEF7DAD3CFA0E1216493CF12301F14AA4B3A8B1456FCDC60D9C39502A0D860`), 61 states, 72 input sidecars, disc files, and original cards preserved and hashed. Clean isolated worktrees and private Ghidra sessions (`a92b59f4`) maintained. |
| 1 | Manifest validator and isolated packager tested; counter check remains diagnostic | **Verified / Tested** | Manifest validator and isolated packager pass (32 tests in `tools/acceptance/TEST_RUN_2026-11-18.txt`, packaging pipelines in `research/runs/parallel-agent-preparation-20261122/`). Staging enforced; counter checks remain diagnostic. |
| 2 | Offline executable/platform/asset inventory with evidence for every entry | **Offline Complete (Dynamic Reachability Open)** | Offline inventories complete with exact counts: 13,138 base functions in `SLUS_214.10` ([INSTRUCTION_COVERAGE.md](research/runs/ghidra-function-coverage-20261118/INSTRUCTION_COVERAGE.md)), 4,372 dynamic overlay functions (1 in `GAMER.ovl`, 2,537 in `MOVIER.ovl`, 1,834 in `NETR.ovl` in [OVERLAY-MAP-MANIFEST.json](recomp/overlays/OVERLAY-MAP-MANIFEST.json)), yielding 17,510 total catalog functions; 1,840 functions excluded out-of-scope per Owner Decision ([DECISION_ONLINE_OUT_OF_SCOPE.json](research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json): 1,834 in `NETR.ovl` + 6 in `SLUS_214.10`), leaving 15,670 active in-scope retail functions; 22,579 donor functions in `default.xbe` ([xbox-donor-20261121](research/runs/xbox-donor-20261121/)); 245 extracted PS2 art files / 1,408,827,392 bytes ([loose-assets SUMMARY.md](research/runs/native-retail-loose-assets-20261121/SUMMARY.md)); 38 actual `.SFD` movie files ([movie provenance.json](research/runs/native-movie-inventory-20261119/provenance.json)); 69,440 pinned PCM bytes / 344 catalog entries ([audio SOURCE.md](research/runs/native-audio-codec-contract-20261118/SOURCE.md)). Dynamic runtime reachability and indirect call graph expansion remain open. |
| 3 | Native host memory objects/handles; remove fixed guest map and ABI | **Verified in Prefix** (Whole-game open) | Fixed guest map (`g_ram`, `g_spr`, `g_memory`) completely removed from all implemented native components. Real host memory ownership: `NativeRetailRendererAllocatorService` (allocator 220900 passing `test_native_retail_renderer_allocator`), typed `NativeRetailGameSession`, `NativeRetailRasterRecord`, `NativeRetailCameraRecord`, `NativeRetailFrameNode`, and memory owners (`libmka_native_retail_memory_owners.a`, `libmka_native_heap.a`). Whole-game acceptance remains Open. |
| 4 | EE/VU0/VU1 linked AOT coverage; remove interpreters/runtime compilers | **In-Progress: Function Coverage** | 244/15,670 in-scope retail functions implemented (1.56%). Complete accounting across all 17,510 catalog functions indexed in [progress.svg](progress.svg) and [functions.csv](research/ledger/functions.csv). Diagnostic count: 45 remaining unresolved link symbols in full-entry link diagnostic. Pure native C++ AOT compilation with zero runtime interpreter or JIT recompiler on native path. |
| 5 | Extracted-file VFS, native saves, IOP services; remove CDVD/SIF/guest IRX | **Verified in Prefix** (Whole-game open) | Extracted loose-file native VFS implemented (`NativeFile`, `NativeContentVolume`, `NativeRetailPackage` in `libmka_native_retail_file_paths.a`), eliminating runtime ISO/CDVD/archive dependencies. Native read streams tested (`test_native_retail_renderer_streams`). Native save store implemented (`libmka_native_save_store.a`, profile startup `171B10`, child resets `246070`/`245DD0`). Guest IRX and SIF RPC modules excised from native prefix. |
| 6 | Physical keyboard and controller input with retail action parity | **In-Progress: Mapped** (Action parity open) | Native keyboard and controller mapping implemented (`libmka_native_retail_keyboard.a`, SDL2 input hooks, LOGON input tasks, `NativeRetailInputParticipant`). Binding `poll_retail_controller_roles` and action tasks identified. Physical action parity against playable oracle remains Open until runtime game loop execution. |
| 7 | Correct bank/sample identity and native sound output | **In-Progress: 25 Clients Pass** (Playback open) | Native sound programs, ADX streaming decoder, sample payload readers, gain/pan curve corrections, bank loaders, and held audio playback/resume checkpoint verified across 25 audio component clients (`research/runs/native-sound-program-20261118/`, `research/runs/native-audio-codec-contract-20261118/`). Live sound mixing, streaming music playback, and whole-game audio output remain Open. |
| 8 | Native video decode, presentation, and synchronized audio | **In-Progress: Offline Passes** (In-game presentation open) | Offline native movie ingress, MPEG2 video demuxing/decoding, ADX audio decoding, and movie clock filter arithmetic pass on retail movie corpus (`LOGON.SFD`, `TITLEN.SFD`, `OPENINGN.SFD` in `research/runs/native-movie-decode-20261119/`). Real offscreen GL device (`NativeMovieGlDevice`) verified. In-game synchronized presentation in active game loop remains Open. |
| 9 | GPU-owned GS packet rendering; remove host VRAM/CPU raster/state model | **In-Progress: 0402–0407 Verified** (Draw path open) | no draw path exists yet; absence of CPU raster is untested. Enforceable build-failing acceptance gates active: (1) Source audit failing on per-pixel framebuffer/depth writes, scanline/edge fill loops, barycentric code, CPU color/depth arrays, and structural frame-sized buffers across entire native and host trees (1,121 sources/build files verified); (2) Readback ban with hash-pinned allowlist `ALLOWLIST_DIAGNOSTIC_READBACK` (`capture_display_rgba` body pinned to SHA-256 `8fef861dad0017b9a53e30f47ece56f8cad3213548022831d3999b070a1983d4`) strictly forbidding frame-path glReadPixels/glGetTexImage, modified function bodies, and comment-marker bypasses; (3) Runtime proof requiring mandatory telemetry counters (GL draw calls > 0, CPU composited pixels = 0, frame readbacks = 0, diagnostic readbacks = 0 in release runs, software rasterizer fallbacks = 0) with external frame capture requirement; (4) Reference raster fence (`audit_reference_raster_fence`) ensuring `MKA_REFERENCE_RASTER` is defined exclusively for verified test targets; (5) 0406 image output restricted exclusively to NativeGlTextureUpload; (6) Binary/link-map symbol scan for forbidden rasterizer symbols; (7) Online scenario guard requiring authenticated `DECISION_ONLINE_OUT_OF_SCOPE.json` to skip decommissioned online tests. Native renderer architecture avoids host VRAM emulation, backing textures directly to host OpenGL via `NativeGlTextureUpload` / offscreen `HiddenGl` fixture. Verified core plugins: 0402 (matrix), 0403 (frames), 0404 (streams), 0405 (cameras), 0406 (CPU images), 0407 (rasters with real offscreen `HiddenGl` WGL context and `glIsTexture`). Render targets, depth buffers, draw calls, and whole-scene rendering remain Open. |
| 10 | One clean whole-game non-regression and no-emulation acceptance build | **Open** | Acceptance remains strictly Open (244/15,670 in-scope implemented). Full breakdown, open frontier tiers, and donor mappings tracked in [progress.svg](progress.svg) and the progress summary above. Full entry link and whole-game acceptance remain open; no visible window, no audible endpoint, no owner launch authorized until all remaining functions and link effects pass the clean non-regression acceptance matrix. |

Close a row only with the ROADMAP evidence and a same-build run record.
No owner launch is a task while any row remains open.
