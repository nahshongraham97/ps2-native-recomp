# Mortal Kombat: Armageddon retail native port

**Current status (2026-11-24): acceptance OPEN. Do not ask the owner to run the game.**
This project builds a native Windows port of retail *Mortal Kombat: Armageddon* (Sony PlayStation 2 `SLUS_214.10` and overlays).
Retail game logic outside platform boundaries comes from ahead-of-time (AOT) static C++ transformed to typed native memory and ABI (zero `g_ram`, zero `c.r`, zero `$gp`, zero guest vtables, zero runtime JIT or interpreter); hand-written C++ is restricted to platform boundaries and verified runtime services. Xbox investigation (`default.xbe`) is limited strictly to renderer, viewport, and HD presentation (`DECISION_OWNER_CORRECTIONS_20261124.json`). Whole-game native acceptance remains **OPEN** (`238 / 15,670` credited functions; headless 60-frame probe hits `13 / 26` registered placeholders; active fork build-time AOT regeneration pending Phase 3).
Neither the canonical playable oracle (`../../milestone_work_verified_playable_20261110/`, `ps2run.exe` `B9EEF7DA...`) nor its descendant Kombat Practice (`../../milestone_build_kombat_practice/`, `8B763DCC...`) satisfies the zero-emulation shipped-port acceptance contract.

<!-- PROGRESS:START -->
## Native Reimplementation Progress & Multi-Tier Ledger

![Porting Progress](progress.svg)

**Verified Fork Commit**: [`499e49c0`](https://github.com/nahshongraham97/ps2-native-recomp/commit/499e49c0865dce875fa9f28a4e66c5cb98b5b423) (pinned active fork)

### Tier 1 · Catalogued & Classified (`research/ledger/functions.csv`)
- **Total PS2 Function Catalog**: **17,510** functions (`13,138` base `SLUS_214.10` + `4,372` dynamic overlays: `1` `GAMER.ovl`, `2,537` `MOVIER.ovl`, `1,834` `NETR.ovl`)
- **In-Scope Retail Path**: **15,670** (`2,643` non-exec/library: `sdk-or-library` 2,011 + `pointer-scan-proven` 632; `12,789` open frontier) | **Excluded Online (`out-of-scope-stubbed`)**: **1,840**
- **Code-Computed Role Partition (`function_classification_inventory.json`)**: `49` `platform-boundary` + `15,450` `retail-logic` + `2,011` `sdk-or-library` = **17,510** (in-scope: `49` + `13,610` + `2,011` = **15,670**)

### Tier 2 · Build-Time Static AOT Generated (`research/ledger/aot_coverage_inventory.json`)
- **In-Scope Static AOT C++**: **83.84%** (**13,138 / 15,670** reproduced via `eerecomp` into untracked `build/generated-aot/ee/`, `311` units; `0` tracked generated `.cpp` files in Git)

| Module / Artifact | Build-Time Static AOT Status | Static AOT / Catalog | Reference % |
|---|---|---|---|
| **Base `SLUS_214.10`** | `311` static `unit_*.cpp` files reproduced from pinned ELF (`reproduce_aot_pipeline.py`) | 13,138 / 13,138 | 100.00% |
| **`MOVIER.ovl` Overlay** | Build-time static overlay C++ NOT generated (`Phase 4` target) | 0 / 2,537 | 0.00% |
| **`GAMER.ovl` Overlay** | Build-time static overlay C++ NOT generated (`Phase 4` target) | 0 / 1 | 0.00% |
| **In-Scope AOT Subtotal** | *Build-time static C++ across `SLUS_214.10` + `MOVIER.ovl` + `GAMER.ovl`* | **13,138 / 15,670** | **83.84%** |
| **Excluded `NETR.ovl`** | Out of scope (`DECISION_ONLINE_OUT_OF_SCOPE.json`) | 0 / 1,834 | — |
| **Dynamic Overlay Windows (`kModules`)** | Runtime memory snapshots (`private-inputs/mkarmageddon-aot-inputs/overlays/`) | 4 captured windows | — |
| **VU1 Microprograms (Separate VU Domain)** | `vurecomp` generates `493` keys from `211` `.bin` captures (`490` legacy table keys + 3 continuations; `44` oracle `vu1-live/` DLLs; earlier `114` claim retracted) | 490 / 493 keys | — |

### Tier 3 · Memory & ABI Transformed (`Phase 5` Target)
- **Contract-Transformed Functions**: **0.00%** (**0 / 15,670** — generated AOT units still retain `c.r[...]`, `g_ram`, `$gp`, and guest vtables pending Phase 5)

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
