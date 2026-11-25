# Mortal Kombat: Armageddon retail native port

**Current status (2026-11-25): Active playable recomp build & behavioral oracle verified (`r192-hooks`, `8AB43BDA...`); zero-emulation native-port acceptance remains OPEN.**
This project targets a native Windows port of retail *Mortal Kombat: Armageddon* (Sony PlayStation 2 `SLUS_214.10` and overlays).
The target architecture uses AOT static C++ transformed to typed native memory and ABI for retail game logic, with hand-written platform boundaries and runtime services. Generated intermediate code still needs the memory/ABI transform; the measured tiers below distinguish it from credited native implementations. Xbox investigation (`default.xbe`) is limited to renderer, viewport and HD presentation (`DECISION_OWNER_CORRECTIONS_20261124.json`). Whole-game zero-emulation native acceptance remains **OPEN**.

**Playable Recomp Build & Active Behavioral Oracle:** `external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe` (`8AB43BDA05E7A621B946883F3195A583F0C2455F0351BF8608282CD3E97B4617`, launched via `launchers/launch_r192_hooks.bat` in the parent repo or the colocated `build/r192-hooks/out/launch_r192_hooks.bat`) IS the working playable recomp build and the project's behavioral oracle. In the verified 2026-11-25 owner session (`68,426` frames at `60.0 FPS` median on D3D12, see [the 2026-11-25 run record](research/runs/r192-hooks-playable-run-20261125/RUN.md)), **Konquest** and **Motor Kombat** worked cleanly alongside VS/Practice combat with all 8 replacement hooks, `--wrap` wrappers, observers, profiler, and `PS2_NATIVE_VERTEX` active. Its slot-1 memory card (`external/forks/ps2-native-recomp/build/r192-hooks/game/disc/armageddon.ps2`, SHA-256 `f38dc412...`) is **persistent** (holds substantial saved data and a Konquest checkpoint) and must never be overwritten or replaced with a disposable card. Historical snapshots (`../../milestone_work_verified_playable_20261110/`, `B9EEF7DA...` and `../../milestone_build_kombat_practice/`, `8B763DCC...`) remain preserved read-only on disk for comparison telemetry, and neither they nor `r192-hooks` satisfy the separate zero-emulation shipped-port acceptance contract.

<!-- PROGRESS:START -->
## Native Reimplementation Progress & Multi-Tier Ledger

![Porting Progress](progress.svg)

**Verified Fork Commit**: [`405bf00a`](https://github.com/nahshongraham97/ps2-native-recomp/commit/405bf00acce22c861afe98bdf5d61ec41ab6a4e7) (pinned active fork)

### Tier 1 · Catalogued & Classified (`research/ledger/functions.csv`)
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
- **Verified Differential Vectors**: **72 / 72** bit-exact match on xemu movie-variant oracle (`LOGON`/`LOGOW` selector); **0** dual-platform whole-function vectors awarding donor credit (`ps2_xbox_map.csv`: `17,511` rows = `0` exact, `0` structural, `0` composite, `18` mapped-by-role, `17,493` unmapped)
- **Acceptance Placeholders**: **26** registered unrecovered placeholders (`0` undefined link symbols; `13` hit across 369 calls in 60-frame headless probe); whole-game acceptance strictly **OPEN**

### Xbox Reference (`default.xbe` — Renderer / Viewport / HD Presentation Scope Only; no completion percentage)
- **Reference Catalog**: **22,579** `default.xbe` routines inspected (`21,321` in non-XONLINE candidate set, `0` donor-credited, `18` role-mapped candidates). Limited strictly to renderer, viewport, and HD presentation per `DECISION_OWNER_CORRECTIONS_20261124.json` (not a separate target; GS packet decoding is an interim concession only).
<!-- PROGRESS:END -->

## Documentation & Entry Points

Read in order:

1. [PORT_STATUS.md](PORT_STATUS.md) — audited checkout state, component receipts, and active blockers.
2. [NATIVE_PORT_ONLY.md](NATIVE_PORT_ONLY.md) — non-negotiable zero-emulation architecture rules.
3. [NATIVE_PORT_EXECUTION_CONTRACT.md](NATIVE_PORT_EXECUTION_CONTRACT.md) — acceptance gates and non-regression contract.
4. [ROADMAP.md](ROADMAP.md) — ordered implementation sequence and closure checks.
5. [TASK_BOARD.md](TASK_BOARD.md) — milestone status table and xemu trace exception.
6. [HANDOFF.md](HANDOFF.md) — active repository workflow and falsification checklist.
7. [REPO_LAYOUT.md](REPO_LAYOUT.md) — directory layout and parent/fork mechanics.
8. [AGENTS.md](AGENTS.md) — binding agent rules and transform-and-verify protocol.
9. [MODEL_COORDINATION.md](MODEL_COORDINATION.md) — local source-review model workflow.

The protected duplicate oracle game copy is `external/forks/ps2-native-recomp-backup-20261110/`. Preserve it along with all user saves, memory cards, key files, and model settings; noncompliant local commits and dirty files are discarded per `DECISION_OWNER_CORRECTIONS_20261124.json`.
Historical logs and experiments live in `research/runs/` and `research/history/`; they are evidence for dated builds, never live instructions. The consolidated status archive lives in `research/history/superseded-status-docs-20261124/`. Retail binary assets (`.ELF`, `.xbe`, `.bin`, `.ovl`, audio/video streams) remain local and untracked.

Published Phases 0–5 carry forward within their recorded evidence scope. The Phase 5 actual generated-unit prototype passes for both overlay initializers, with typed ownership, explicit scalar effects, saved-input differentials and structural guest-accessor rejection. Its measured transform tier is separate from production integration and credited implementations. Phase 6 identifies renderer plugins, camera and matrix operations, device requests and pipeline callbacks. Every in-scope catalogue function has a callable static AOT entry. The exact RenderWare function count remains open. See the current measurement in [PORT_STATUS.md](PORT_STATUS.md). The active gameplay oracle is the R192 build identified at the top of this README.

Generation uses the user's pinned ELF, overlay captures and existing dependency packages. From the integration repository, run `python -B tools/research/reproduce_aot_pipeline.py`, regenerate with `python -B tools/research/generate_ledgers.py`, then synchronize with `python -B tools/research/sync_verify.py --sync-to-fork`. Generated retail C++ remains ignored.

Windows verification clones must preserve LF bytes: use `git -c core.autocrlf=false clone --no-hardlinks`. For a verification checkout inside the integration repository, run that clone's pipeline with explicit `--parent-root` and `--fork-root`. The pipeline builds fresh generators and reads existing SDL2/FFmpeg dependencies; it does not copy a prior generator or generated source tree. The work-item record contains the measured clean-clone result and retained failed attempts.
