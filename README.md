# Mortal Kombat: Armageddon retail native port

**Current playable R192 route:** The parent launchers/launch_r192_hooks.bat (also called by launch_ps2recomp.bat) calls the unchanged original external/forks/ps2-native-recomp/build/r192-hooks/out/launch_r192_hooks.bat and ps2run.exe (SHA-256 8ab43bda05e7a621b946883f3195a583f0c2455f0351bf8608282cd3e97b4617). Its linked base AOT code and all3569 precompiled EE cache DLLs remain available; every cached DLL was verified byte-identical to the preserved compiled package. EE/VU Live remain enabled for misses. The SAME original persistent card and quick-save directory in build/r192-hooks/game/disc are retained, with no protected card/save/executable/original-launcher modification; verification runs use disposable headless copies. The separate compiled882302... package is preserved but is no longer the default route. AOT closure remains frozen, OPEN and not required for the native port. Native Phase6 continues on main in both existing repositories under the latest owner direction; admission/credit remain2 transformed/238 native until the existing gates pass. Final native-only acceptance remains OPEN.

**Current status (2026-11-26): Protected R192 remains the playable reference. The current Phase7 correction batch is tested component/capture work; ownership18, transformed2 and credited native238 remain unchanged. Full native-game acceptance is OPEN.**
This project targets a native Windows port of retail *Mortal Kombat: Armageddon* (Sony PlayStation 2 `SLUS_214.10` and overlays).
The target architecture uses AOT static C++ transformed to typed native memory and ABI for retail game logic, with hand-written platform boundaries and runtime services. Generated intermediate code still needs the memory/ABI transform; the measured tiers below distinguish it from credited native implementations. The Xbox image supports read-only library identification and measured PS2/Xbox capability comparisons. The owner decides the route. Xbox-derived algorithms are eligible only for renderer, viewport and HD presentation after comparison. GS packet decoding remains interim. Whole-game zero-emulation native acceptance remains **OPEN**.

**Protected original R192 gameplay reference:** `external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe` (`8AB43BDA05E7A621B946883F3195A583F0C2455F0351BF8608282CD3E97B4617`, original colocated launcher: `build/r192-hooks/out/launch_r192_hooks.bat`) remains the protected original gameplay reference. In the verified 2026-11-25 owner session (`68,426` frames at `60.0 FPS` median on D3D12, see [the 2026-11-25 run record](research/runs/r192-hooks-playable-run-20261125/RUN.md)), **Konquest** and **Motor Kombat** worked cleanly alongside VS/Practice combat with all 8 replacement hooks, `--wrap` wrappers, observers, profiler, and `PS2_NATIVE_VERTEX` active. Its slot-1 memory card (`external/forks/ps2-native-recomp/build/r192-hooks/game/disc/armageddon.ps2`, SHA-256 `f38dc412...`) is **persistent** (holds substantial saved data and a Konquest checkpoint) and must never be overwritten or replaced with a disposable card. R192 supplies gameplay and telemetry evidence. The November 10 milestone supplies saved-state and hash parity. Kombat Practice is the historical snapshot. Owner direction on 2026-11-26 keeps the original R192 as the playable oracle with live translation; universal oracle closure is not a prerequisite for the native port.

<!-- PROGRESS:START -->
## Native Reimplementation Progress & Multi-Tier Ledger

![Porting Progress](progress.svg)

**Verified Fork Commit**: [`6fe8d075`](https://github.com/nahshongraham97/ps2-native-recomp/commit/6fe8d075c6eba17b2e9c18f2162d83552fa2ffba) (source and evidence checkpoint)

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

## Documentation & Entry Points

Read in order:

1. [PORT_STATUS.md](PORT_STATUS.md) — audited checkout state, component receipts, and active blockers.
2. [NATIVE_PORT_ONLY.md](NATIVE_PORT_ONLY.md) — non-negotiable zero-emulation architecture rules.
3. [NATIVE_PORT_EXECUTION_CONTRACT.md](NATIVE_PORT_EXECUTION_CONTRACT.md) — acceptance gates and non-regression contract.
4. [ROADMAP.md](ROADMAP.md) — ordered implementation sequence and closure checks.
5. [TASK_BOARD.md](TASK_BOARD.md) — milestone status and exact resume point.
6. [HANDOFF.md](HANDOFF.md) — active repository workflow and falsification checklist.
7. [REPO_LAYOUT.md](REPO_LAYOUT.md) — directory layout and parent/fork mechanics.
8. [AGENTS.md](AGENTS.md) — binding agent rules and transform-and-verify protocol.
9. [MODEL_COORDINATION.md](MODEL_COORDINATION.md) — local source-review model workflow.

The protected duplicate oracle game copy is R192 at `external/forks/ps2-native-recomp/build/r192-hooks/`. Preserve it along with all user saves, memory cards, key files, and model settings; noncompliant local commits and dirty files are discarded per `DECISION_OWNER_CORRECTIONS_20261124.json`.
Historical logs and experiments live in `research/runs/` and `research/history/`; they are evidence for dated builds, never live instructions. The consolidated status archive lives in `research/history/superseded-status-docs-20261124/`. Keep retail inputs and generated code outside the current Git index. The integration repository’s checkout audit records the historical index entries, local preservation hashes and verified recovery paths. Use `python -B tools/research/hydrate_retail_inputs.py` in the integration repository to prepare the pinned inputs for a local build.

Published Phases 0–5 carry forward within their recorded evidence scope. The Phase 5 actual generated-unit prototype passes for both overlay initializers, with typed ownership, explicit scalar effects, saved-input differentials and structural guest-accessor rejection. Its measured transform tier is separate from production integration and credited implementations. Phase 6 identifies renderer plugins, camera and matrix operations, device requests and pipeline callbacks. Every in-scope callable entry has a generated AOT body. The separate descriptor row is data. The exact RenderWare function count remains open. The [renderer scope evidence](https://github.com/nahshongraham97/ps2-native-recomp/blob/main/research/runs/renderer-scope.json) records the ownership evidence and its limits. The compiled R192 route and protected original reference have the separate roles recorded at the top of this README.

The Xbox comparison in the correspondence ledger favors PS2 for matrix transforms and Xbox as a reference for viewport and HD presentation. Streams, plugin registration, camera view window, heap and texture work have comparable measured routes. Renderer core, materials, geometry and world still lack enough evidence to choose. The owner decides each route. The generated PS2 renderer and native GPU services remain the recommended implementation path. A higher-resolution GS/VU replay alone does not establish correct asset layouts or scene presentation.

Generation uses the user's pinned ELF, overlay captures and existing dependency packages. From the integration repository, run `python -B tools/research/reproduce_aot_pipeline.py`, regenerate with `python -B tools/research/generate_ledgers.py`, then synchronize with `python -B tools/research/sync_verify.py --sync-to-fork`. Generated retail C++ remains ignored.

Windows verification clones must preserve LF bytes: use `git -c core.autocrlf=false clone --no-hardlinks`. For a verification checkout inside the integration repository, run that clone's pipeline with explicit `--parent-root` and `--fork-root`. The pipeline builds fresh generators and reads existing SDL2/FFmpeg dependencies; it does not copy a prior generator or generated source tree. The work-item record contains the measured clean-clone result and retained failed attempts.
