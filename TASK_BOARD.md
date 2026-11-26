# Retail native-port task board

**Audited 2026-11-25. Whole-game native acceptance: OPEN.** The full instructions, order,
files, and closure evidence are in [ROADMAP.md](ROADMAP.md). This board indexes
each deliverable's verified component progress and whole-game acceptance state;
[PORT_STATUS.md](PORT_STATUS.md) is the live status. The previous baseline board is archived at
`research/history/native-port-docs-2026-11-18/TASK_BOARD.md`.
**Current compiled R192 route:** The familiar parent launchers/launch_r192_hooks.bat (also called by launch_ps2recomp.bat) selects external/forks/ps2-native-recomp-r192-closure/build/r192-compiled-konquest/out/ps2run.exe, SHA-256 8823021a23e2a3fdd3b2dc4ca0c8eac9e7359beb3e5bce489b965587a0c2479b. It links its own AOT library and sealed compiled modules with no EE/VU live compiler. The latest Konquest recording passes 22,785 new frames / all 1,519 checkpoints; all 14 saved-state probes pass 90 new frames. Code/assets/keys are local to the build; normal play mounts the SAME original R192 persistent card and uses its existing quick-state directory. No replacement card is created. The original 8AB43BDA executable/launcher remain protected comparison evidence. Complete precompilation of all executable families remains OPEN: recorded replays do not establish it; runtime-generated EE variants, VU upload/patch coverage and the IOP interpreter must be closed from original code/data. Physical A/V and final native-port acceptance remain OPEN. See [the existing R192 record, section 6](research/runs/r192-hooks-playable-run-20261125/RUN.md).


**Current work follows A1 dependencies.** Phase 5S.9 measured zero whole-function passes in the frozen 30-leaf capability sample; stop general binding rounds. The original 17 admitted functions have zero leaves. Separate E1 admission of heap-close 001A3C40 makes the current roster 18 and supports one Stage A ABI function with 189 bounded comparisons; Stage B and production callers remain OPEN. Full 17/30 tables, failures, ownership/ABI evidence and publication status are in [the existing fork record](https://github.com/nahshongraham97/ps2-native-recomp/blob/8e3e2a8675500620030bfacd5d5920b6cf38c5d6/research/runs/renderer-scope.json), section native_phase6_leaf_slice. Transformed/native credit remains 2/238; protected inputs, handwritten setters and the admission/credit rules are unchanged.

**Exact resume:** Complete offline game-code closure remains the priority. The conservative original-upload VU set now has 99 compiled native DLLs / 65,924 entry instances / 65,026 unique keys. Separate candidate25d6ae51... passes sealed startup, the 22,785-frame/all1,519-checkpoint recording and14 fresh90-frame probes. Whole-disc structured MPG search found99 existing pairs across105 files, with0 new pairs. Four source-positive partial EE shapes pass270 output comparisons and4410 full-Cpu constructed continuation boundaries; actual caller/scheduler ownership remains open. Continue actual scheduler shadow and its bounded connected adapter; authenticate fresh IOP loader owners and invalidate restore generations before registry adoption. VU0/other upload formats and full integration remain open. These measured gates do not establish universal or native-only acceptance. Normal882302... and originalR192/card remain unchanged. Detailed receipts/limits are in the existing R192 run record. Do not add branches or restart general-transform binding rounds.

**R192 observed-cache integration, complete closure OPEN:** own AOT library plus 3,569 sealed precompiled EE modules and 987 offline VU keys; no runtime EE/VU compiler objects or VU1 interpreter fallback. Exact source/build/binary/package/recording evidence is in the existing R192 run record section 6. All future paths, held-out behavior and full native acceptance remain OPEN; this adds zero native credit.

R192 supplies gameplay and telemetry evidence. The November 10 playable milestone supplies saved-state and hash parity. Kombat Practice is a historical snapshot. The primary oracle designation is **pending owner confirmation**.

**Branch consolidation:** The fork now has only main and r192-aot-closure-proof, locally and on origin. Reviewed phase6 work was fast-forwarded to main in parent/fork; redundant branches were removed after preserving unmerged tips as published archive tags. Parent has main only. R192 integration now builds its own AOT library instead of the protected backup archive; final replay results belong in the existing R192 run record.

<!-- PROGRESS:START -->
## Native Reimplementation Progress & Multi-Tier Ledger

![Porting Progress](progress.svg)

**Verified Fork Commit**: [`e5be661d`](https://github.com/nahshongraham97/ps2-native-recomp/commit/e5be661d5504f7493d3d5e7124013184016adb47) (source and evidence checkpoint)

### Tier 1 · Catalogued & Classified (`research/ledger/functions.csv`)
- Bounded ownership of 15,669 callable entries · verified-renderware 6, verified-game 12, verified-other-library 0, inferred-renderware 141, inferred-game 130, inferred-other 0, unresolved 15,380.
- Ownership evidence by kind (overlapping entries) · E1 6, E2 0, E3 26, E4 13, E5 165, E6 117, E7 1.
- Native credit retains its published component evidence. This ownership pass awards zero native credit.
- **Total PS2 Function Catalog**: **17,510** raw catalogue entries (`13,138` base `SLUS_214.10` + `4,372` dynamic overlays: `1` `GAMER.ovl`, `2,537` `MOVIER.ovl`, `1,834` `NETR.ovl`)
- **In-Scope Retail Path**: **15,670** (`2,643` non-exec/library: `sdk-or-library` 2,011 + `pointer-scan-proven` 632; `12,789` open frontier) | **Excluded Online (`out-of-scope-stubbed`)**: **1,840**
- **Code-Computed Role Partition (`function_classification_inventory.json`)**: `49` `platform-boundary` + `15,450` `retail-logic` + `2,011` `sdk-or-library` = **17,510** (in-scope: `49` + `13,610` + `2,011` = **15,670**)

### Tier 2 · Build-Time Static AOT Generated (`research/ledger/aot_coverage_inventory.json`)
- **In-Scope Static AOT C++**: **99.99%** (**15,669 / 15,670** reproduced via `eerecomp` into untracked `build/generated-aot/`, `395` units total; `0` tracked generated `.cpp` files in Git)
- **Audited Data Contradictions**: **1** raw catalogue entry/entries (`0x005BF218`) are original descriptor data, not generated functions. Original catalogue rows and denominators stay visible; no failure stub or data address receives AOT credit.

| Module / Artifact | Build-Time Static AOT Status | Static AOT / Catalog | Reference % |
|---|---|---|---|
| **Base `SLUS_214.10`** | `311` static `unit_*.cpp` files reproduced from pinned ELF (`reproduce_aot_pipeline.py`; `13,131` in-scope + `6` excluded online) | 13,137 / 13,138 | 99.99% |
| **`MOVIER.ovl` Overlay** | `34` static `m_MOVIER_unit_*.cpp` files reproduced from `MWo3` overlay | 2,537 / 2,537 | 100.00% |
| **`GAMER.ovl` Overlay** | `1` static `m_GAMER_unit_0000.cpp` file reproduced from `MWo3` overlay | 1 / 1 | 100.00% |
| **In-Scope AOT Subtotal** | *Build-time static C++ across `SLUS_214.10` (`13,131`) + `MOVIER.ovl` (`2,537`) + `GAMER.ovl` (`1`)* | **15,669 / 15,670** | **99.99%** |
| **Excluded `NETR.ovl`** | Out of scope (`DECISION_ONLINE_OUT_OF_SCOPE.json`) | 0 / 1,834 | — |
| **Dynamic Overlay Windows (`kModules`)** | `4` authenticated runtime RAM window snapshots (`49` measured static `m_007C*` units) | 4 captured windows | — |
| **VU1 Microprograms (Separate VU Domain)** | `493` static keys from `211` hash-admitted input images; state evidence: `CORRECT_SOURCE_AND_INDEPENDENT_SNAPSHOT_REGISTER_MEMORY_PACKET_COMPARISON_PASS`. Independent snapshot replay: `61` state(s), `41,296` exact ordered packet bytes. This is selected-program replay; historical invocations and untested programs remain unverified. | 493 / 493 recorded-input keys | — |

### Tier 3 · Memory & ABI Transformed (`Phase 5` Target)
- **Contract-Transformed Functions**: **0.01%** (**2 / 15,670** — authenticated compiled prototype functions only; production integration is separate. Remaining generated AOT units retain `c.r[...]`, `g_ram`, `$gp`, and guest vtables.)

### Tier 4 · Credited Native Implementations (`functions.csv` & `ported_function_evidence.json`)
- **Credited Completion**: **1.52%** (238 / 15,670 in-scope: `227` `ported` + `11` `host-replaced` + `0` `implemented-via-donor`; code partition: `49` `platform-boundary` + `189` `retail-logic`)
- **Open Frontier**: **12,789** (`9,038` `SLUS_214.10`, `1,102` `MOVIER.ovl`, `1` `GAMER.ovl`, `2,648` `no-direct-callers` across 5,400 indirect call sites)

### Tier 5 · Verified Vectors & Acceptance Placeholders
- **Verified Differential Vectors**: **72 / 72** bit-exact match on xemu movie-variant oracle (`LOGON`/`LOGOW` selector); **0** dual-platform whole-function vectors awarding donor credit (`ps2_xbox_map.csv`: `17,519` rows = `0` exact, `0` structural, `0` composite, `52` mapped-by-role, `17,467` unmapped)
- **Acceptance Placeholders**: **26** registered unrecovered placeholders (`0` undefined link symbols; `13` hit across 369 calls in 60-frame headless probe); whole-game acceptance strictly **OPEN**

### Xbox Reference (`default.xbe` — read-only identification and comparison; no completion percentage)
- **Reference Catalog**: **22,579** `default.xbe` routines inspected (`21,321` in non-XONLINE candidate set, `0` donor-credited, `52` role-mapped candidates). Read-only library identification and measured Xbox-versus-PS2 capability comparisons are authorized. Algorithm adoption remains limited to renderer, viewport, and HD presentation after the per-capability comparison; the owner decides the route. GS packet decoding is an interim concession only.
<!-- PROGRESS:END -->

| Order | Deliverable | State | Verified progress & open frontier |
| --- | --- | --- | --- |
| 0 | Preserve and hash playable/card oracles, assets, saves, checkout | **Verified / Preserved** | Active playable recomp build & behavioral oracle: `external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe` (`8AB43BDA05E7A621B946883F3195A583F0C2455F0351BF8608282CD3E97B4617`, `launchers/launch_r192_hooks.bat`), owner-verified 2026-11-25 (`68,426` frames @ `60.0 FPS` median; Konquest & Motor Kombat verified; persistent slot-1 memory card `build/r192-hooks/game/disc/armageddon.ps2` SHA-256 `f38dc412...` holding Konquest checkpoint & user saves, see `research/runs/r192-hooks-playable-run-20261125/RUN.md`). Historical `2026-11-10` oracle (`B9EEF7DA...`, 3,256 originals, 61 states, 72 `.inputs` sidecars) & protected backup preserved immutable. |
| 1 | Manifest validator and isolated packager tested; counter check remains diagnostic | **Verified / Tested** | Manifest validator and isolated packager pass (32 tests in `tools/acceptance/TEST_RUN_2026-11-18.txt`, packaging pipelines in `research/runs/parallel-agent-preparation-20261122/`). Staging enforced; counter checks remain diagnostic. |
| 2 | Offline executable/platform/asset inventory with evidence for every entry | **Offline Complete (Dynamic Reachability Open)** | Offline inventories complete with exact counts: 13,138 base functions in `SLUS_214.10` ([INSTRUCTION_COVERAGE.md](research/runs/ghidra-function-coverage-20261118/INSTRUCTION_COVERAGE.md)), 4,372 dynamic overlay functions (1 in `GAMER.ovl`, 2,537 in `MOVIER.ovl`, 1,834 in `NETR.ovl` in [OVERLAY-MAP-MANIFEST.json](recomp/overlays/OVERLAY-MAP-MANIFEST.json)), yielding 17,510 total catalog functions; 1,840 functions excluded out-of-scope per Owner Decision ([DECISION_ONLINE_OUT_OF_SCOPE.json](research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json): 1,834 in `NETR.ovl` + 6 in `SLUS_214.10`), leaving 15,670 active in-scope retail functions; 22,579 donor functions in `default.xbe` ([xbox-donor-20261121](research/runs/xbox-donor-20261121/)); 245 extracted PS2 art files / 1,408,827,392 bytes ([loose-assets SUMMARY.md](research/runs/native-retail-loose-assets-20261121/SUMMARY.md)); 38 actual `.SFD` movie files ([movie provenance.json](research/runs/native-movie-inventory-20261119/provenance.json)); 69,440 pinned PCM bytes / 344 catalog entries ([audio SOURCE.md](research/runs/native-audio-codec-contract-20261118/SOURCE.md)). Dynamic runtime reachability and indirect call graph expansion remain open. |
| 3 | Native host memory objects/handles; remove fixed guest map and ABI | **Verified in Prefix** (Whole-game open) | Fixed guest map (`g_ram`, `g_spr`, `g_memory`) completely removed from all implemented native components. Real host memory ownership: `NativeRetailRendererAllocatorService` (allocator 220900 passing `test_native_retail_renderer_allocator`), typed `NativeRetailGameSession`, `NativeRetailRasterRecord`, `NativeRetailCameraRecord`, `NativeRetailFrameNode`, and memory owners (`libmka_native_retail_memory_owners.a`, `libmka_native_heap.a`). Whole-game acceptance remains Open. |
| 4 | EE/VU0/VU1 linked AOT coverage; remove interpreters/runtime compilers | **Verified recorded corpus and selected comparisons; release OPEN** | Actual callable entry maps cover 15,669/15,670 raw in-scope catalogue entries. 1 audited descriptor-data entry/entries remain visible and receive no code credit. Selected EE boundaries and independent VU register/memory/packet comparisons pass; unobserved execution and whole-game acceptance remain OPEN. |
| 5 | Extracted-file VFS, native saves, IOP services; remove CDVD/SIF/guest IRX | **Verified in Prefix** (Whole-game open) | Extracted loose-file native VFS implemented (`NativeFile`, `NativeContentVolume`, `NativeRetailPackage` in `libmka_native_retail_file_paths.a`), eliminating runtime ISO/CDVD/archive dependencies. Native read streams tested (`test_native_retail_renderer_streams`). Native save store implemented (`libmka_native_save_store.a`, profile startup `171B10`, child resets `246070`/`245DD0`). Guest IRX and SIF RPC modules excised from native prefix. |
| 6 | Physical keyboard and controller input with retail action parity | **In-Progress: Mapped** (Action parity open) | Native keyboard and controller mapping implemented (`libmka_native_retail_keyboard.a`, SDL2 input hooks, LOGON input tasks, `NativeRetailInputParticipant`). Binding `poll_retail_controller_roles` and action tasks identified. Physical action parity against playable oracle remains Open until runtime game loop execution. |
| 7 | Correct bank/sample identity and native sound output | **In-Progress: 25 Clients Pass** (Playback open) | Native sound programs, ADX streaming decoder, sample payload readers, gain/pan curve corrections, bank loaders, and held audio playback/resume checkpoint verified across 25 audio component clients (`research/runs/native-sound-program-20261118/`, `research/runs/native-audio-codec-contract-20261118/`). Live sound mixing, streaming music playback, and whole-game audio output remain Open. |
| 8 | Native video decode, presentation, and synchronized audio | **In-Progress: Offline Passes** (In-game presentation open) | Offline native movie ingress, MPEG2 video demuxing/decoding, ADX audio decoding, and movie clock filter arithmetic pass on retail movie corpus (`LOGON.SFD`, `TITLEN.SFD`, `OPENINGN.SFD` in `research/runs/native-movie-decode-20261119/`). Real offscreen GL device (`NativeMovieGlDevice`) verified. In-game synchronized presentation in active game loop remains Open. |
| 9 | Native GPU movie/loading presentation and connected scene rendering | **Tested movie component; whole-scene rendering OPEN** | The native movie path uploads decoded frames to OpenGL, submits the GPU presentation plan, waits for its fence and presents through the native display service. The asset-backed bounded headless test presents `LOGOW.SFD` frame 0 with one GL draw, one present and zero frame-path readbacks; its test-only diagnostic capture performs one readback and checks the framebuffer hash. Source also contains the loading-image RGB upload and `glDrawArrays` path; this is not proof of complete scene rendering. Source/readback bans, reference-raster fences and binary symbol audits are tested gates; they do not establish absence of CPU rasterization in unexecuted full-game paths. Continuous movie/audio playback, gameplay render targets/depth, connected 3D world rendering and whole-game visual parity remain OPEN. Evidence in the active fork: [movie path](https://github.com/nahshongraham97/ps2-native-recomp/blob/main/runtime/src/native/retail_initial_movie_path.cpp), [headless assertions](https://github.com/nahshongraham97/ps2-native-recomp/blob/main/tests/test_native_headless_entry_run.cpp), [loading draw](https://github.com/nahshongraham97/ps2-native-recomp/blob/main/runtime/src/native/retail_platform.cpp). |
| 10 | One clean whole-game non-regression and no-emulation acceptance build | **Open** | Acceptance remains strictly Open (238/15,670 in-scope implemented per research/ledger/functions.csv, 1.52%). Full multi-tier ledger (`Tier 1–5`, `platform-boundary`/`retail-logic`/`sdk-or-library` partition, and Xbox Reference line without %) tracked in compact [progress.svg](progress.svg) and the generated summary above. Full entry link and whole-game acceptance remain open; no visible window, no audible endpoint, no owner launch authorized until all remaining functions and link effects pass the clean non-regression acceptance matrix. |

> [!NOTE]
> **Historical 2026-11-24 xemu guest-trace exception — superseded for current verification**:
> The earlier minimized-window/muted-audio xemu route is historical evidence only. Current verification follows the controlling headless-only instruction: no visible window, no audio and no writes to the persistent card. Existing trace records and retail assets remain protected; whole-game acceptance stays OPEN.

> [!NOTE]
> **2026-11-24 Phase 0 Preflight, Oracle Restoration & Baseline Gate**:
> Earlier local discard decisions and restoration measurements remain historical. Current Phase 0 repair re-verifies the immutable oracle, corrects the additive launcher and reruns baseline checks. No pushed history is rewritten.

> [!NOTE]
> **Historical Phase 3 Pipeline, Multi-Tier Ledger & Classification — callable coverage superseded by the current generated block**:
> Fresh-clone EE/VU generation was verified at that checkpoint. Its former base-function coverage claim used unit ranges and was invalidated; the current generated block measures actual callable entries. The dated VU reconciliation, source inventories and three-way role partition remain historical evidence. That role partition does not establish exact RenderWare library ownership. Current counts and the unresolved Phase 6 census are recorded above.

Close a row only with the ROADMAP evidence and a same-build run record.
No owner launch is a task while any row remains open.

## Open owner decisions

- Primary oracle designation remains pending owner confirmation. R192 is the protected playable oracle copy and supplies gameplay evidence.
- Renderer core, materials, geometry and world source routes remain undecided. The existing capability comparison records the evidence.
- Xbox-only content remains an open owner decision after the asset comparison.

## Oracle snapshot integrity and scene index

The recorded 133 state-related files comprise 61 snapshots and 72 input files, not a separately established 133-snapshot build. The parent original-manifest snapshot authenticates every hash. Existing `hud-health-geometry/ORACLE_CAPTURE.json` and `ORACLE_SCENARIOS` records establish only the bounded joins noted below; remaining scene identities require matched frame evidence and a pinned runner/configuration. No scene is inferred from a filename or global value.

| Oracle snapshot filename | SHA-256 | Existing scene evidence |
|---|---|---|
| `quick_2026-11-08_17-29-12_f017793.state` | `97c5701c799c00f763d5a30a87b17dfe07206762923c5882f69e99d64fb67b22` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_17-29-53_f020277.state` | `f22a94eaa502433313e9f608a6ee602f09689e90e25cdc943d7b0415e315e338` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_17-32-36_f028432.state` | `0e98972075595ecef85b88f26d3a43d5923228de358274cd0f43762a79911f4b` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_18-26-05_f005307.state` | `dfd22e2ddfcda7cff080ff68d9c0774f83f11b05691b0582e9c17aba9b0a8045` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_18-28-12_f012355.state` | `06bfc314031d653449b89a4b425de1aa2b095781d29818f8ae5d1cd94fc5a4ed` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_18-28-36_f013787.state` | `f8d5a2e93e5b55e22d84c1e0065870af20b5c2bcbadb936214a35082941c104c` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_18-29-01_f015158.state` | `0feadda8b703225dd35ed1a40c536fff59d283351b1d9f9a4c56a19a54f0f4f6` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-02-00_f005331.state` | `a58917a83f8b6f2339caedb654de46a1d5294b284b8f1edbf2b643eed35ebbab` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-02-51_f008330.state` | `292585f9ddafc0ddf18820a89f2ab9371039e23e9e197a2777b7ee197a1943c7` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-03-16_f009867.state` | `99cbc07f0b1e81fb9a08ed03ca0b17d139c16dbb525172f38671b96a589ea308` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-03-55_f012128.state` | `dc25cdf19e72859638776dc1f2ed9a93f66017de112e884ae0fc0216fdefa733` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-07-30_f024832.state` | `dd356866d5f9af807f0ade84495bd6352976a9f6375e085817d93f44b0b48b27` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-07-54_f026286.state` | `1aba3a1f0a6e790074d12fdf404aa4b9d64023977842c46aea322316b4d81ce7` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-08-20_f027841.state` | `88cf31c39bf9a685dacce309fc72632230e38eb665e52e7fea6d7d27fcc3450c` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-08-41_f029092.state` | `c509e1889912cfc936c9c8471bcb122b68a1cf3e8a1d6b374d214916a1f5065d` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-09-09_f030550.state` | `a108e8c07536eef5fbe953a957cfa7be7082c280aa9c9fe8a146c71e8501cbe7` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-09-30_f031825.state` | `afcb8f2fd0c5de86532ad6a644fcac917d2d26a31f8c6c572390c929a6959da7` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-10-12_f033890.state` | `6286ee8450759dac16d25a58222491daa8d08483cba038e710e3dbf4f429e6ef` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-10-35_f035307.state` | `bdf816dc18f1b21a6a085c206da3cebb8fa5c304dd429eaed1d2d99be5ebb43e` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-11-05_f036665.state` | `0acf20af1c2e950ed73a8bde4a7e8d0b4335b9cfaa28099799fc21cf3bbb74fb` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-11-52_f039225.state` | `a91bd8c6d0353996db2f724fd8ba6738d8bf46b2f4e112dae95aa2129091edc8` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-12-23_f040934.state` | `185fe6514ab838734996157d171c4f87164bc24e37afdb7b0f80afcb6b89a953` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-12-55_f042580.state` | `664a3605ab63a99e382afea910a4b2009dd63f26e396953cf945ac12986af432` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-13-16_f043759.state` | `a9788058767e0a7d50c784a603079631d5c29c4f45fbac52b3c2a0f6827534d0` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-13-58_f046252.state` | `9b0ed62381c5268e374e6a44a4a191c703840bd978a7378718b7a443ebca9924` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-14-36_f048217.state` | `51698528b33a16cc3f65b39aa91761768149d890015382a3a2775eab8190e1cc` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-14-59_f049618.state` | `79f068615b86c39bd4fb7762901d56f9736f155a0fef3c0dc00c95c6722f7ab8` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-15-21_f050891.state` | `26ad996ee061a9cf05e45dab53e961e8e204cf910898f74291cfcad12b5c415c` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-15-44_f052314.state` | `a1aa7c799e702112eea2deb5c5daa765828a2d691537daf2f97ccf08d67dee74` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-16-12_f053607.state` | `ac494145079f51a8494d5b21af1c1995de4fa16fc8176638fcc107bfd55cecb5` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-16-42_f055111.state` | `73022363fc324d0a2dac95087f3434f662233aaa374d00511e3727535506dc75` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-17-06_f056559.state` | `52eb533d0883d26409cc498ee38bc4779e71a5596618bbd4b23f017290035738` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-18-36_f061518.state` | `cba9b5e4293a886eaacd8032f675527c09500d85e6e19399546df2ad9c43f4eb` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-19-21_f064175.state` | `bf6f6bb014a34af67bda084f9f57a96db8f056b67f722338f553254f3a75e2e1` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-19-46_f065670.state` | `c9859a91692d8a30992029698173f6564d1d87ecfd2ac69708def0e2573a2dab` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-20-19_f067422.state` | `46a10ecf8d7387d4f0887636520fbcc6caf5942bd39836fae4ba62e8534731ea` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-20-56_f069124.state` | `da89c2cfbdcf89611fb80a698392e2c433d0bca78611fea3f29dc323eba66e38` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-21-28_f070810.state` | `02e478f6080c3c3690e59e5a856adcf0f0c61764ce7769e8703313a7d6c64270` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-21-51_f072144.state` | `5bbf0603b336fe5860ef98ed49000074a292116d0ae3c7ef1e10ec4a6c9e6eaa` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-22-18_f073706.state` | `a801f5351d3a301a52a7e12656b7694f9d4cdf09cfa6659161cf2c700dc06bab` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-23-00_f075709.state` | `ac939cf32a8b14d6a128bd48ff6a6307f76dc4ee3abe538a26a8f3c150bc1486` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-23-24_f077194.state` | `18d144c4c82321fd9b8defe176a00e8b11a20df1274314854133e3a6583f1d21` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-23-50_f078683.state` | `a4bbf64b9f6861a3b8f495a3a4960d8ee354098af1f21f3aa9d10616b11d4a13` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-24-14_f080156.state` | `a53b9d812f9f47f5a5cfb9716cf0ee06adb310d39796c7d65f94ee615064d97d` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-24-48_f082176.state` | `f2824fe159a7220219ce20bb030f1d7b95cf6bfa23277ae00d9dd2c61b986d55` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-25-16_f083628.state` | `14b216b17b1231e8802a7460b0f4010fa423d63c45e988fccce058f0aad2acbf` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-25-41_f085069.state` | `2527c7831e08f558b70b85db15c8581c384057c90b54211db0b64363e76a1d3c` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-26-23_f087138.state` | `a7676898cc9941e7f22aaf4383bf480a6fcd69aaef5bbdff96ee4f19bf804418` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-26-53_f088919.state` | `38523cfe370a2caf9ffd0bda3012016a13bcb7196368796b5ae0143aba6bb121` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-27-17_f090343.state` | `ad33086577ccf485048a5d378ca3e5405a5bddc816f1e6f9a7e22efa4cf5f952` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-27-52_f092294.state` | `498313adf6161eab849b23442c35d76e828bf28088fa22d7ce8a25bbc3582bab` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-28-20_f093860.state` | `9dffc0086368090466239916ecd949f9d15d905e1023306ee86232a979f54e07` | Post-load intercepted-hit log names Sheeva→Sektor; instantaneous scene/arena unverified. |
| `quick_2026-11-08_19-28-53_f095357.state` | `bdf8842f1f4c93437ee31ce37e23d0cf1b75ea207b165d3f028bc1e36f6a1e19` | Post-load intercepted-hit log names Baraka→Sektor; instantaneous scene/arena unverified. |
| `quick_2026-11-08_19-29-15_f096669.state` | `c85438d9caf33b308237d79a5f95b8c82bbcf1fd5ffddce7abc49169ced5135a` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-29-38_f098020.state` | `74118a1d9778f949ba2a2155ae78e2733614c109ff68fafd6b831d8566cbf0ef` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-30-05_f099490.state` | `f92096880dc5a06e9e5f2a6e61e3227bde593aab89b19b8c3b98feb3f5ac419c` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-30-32_f101063.state` | `50e3ca3c88d9476645e4cf921edf49073c0bf826f67320172dcb947fbfd46164` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-30-53_f102353.state` | `2582224085ac4612a8cacf3ba44f0285a994555983eb57659b044d68b8046231` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-31-19_f103892.state` | `833df25c78e4360c13ccd7c33c5424bcdb418189ccfdd91e85f056e034a505f4` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-31-46_f105495.state` | `c5c5abcb17a6e543365704bf89f90a99a57b14cfd82422b371a96b284689e177` | Scene unverified; no matched scene/frame join found in the scoped records. |
| `quick_2026-11-08_19-32-33_f108075.state` | `8d5e72d8c645e8ad1650f9919e37b105d169248f8c268713cfbfe9f9c5410e58` | Historical report names Onaga/Raiden practice; scene imagery and runner join unverified. |
