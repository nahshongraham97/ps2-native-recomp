# ps2-native-recomp — Mortal Kombat: Armageddon Native Retail Port

**Current status (2026-11-23): Acceptance OPEN (In Progress). Do not ask the owner to run the game.**
This repository is the active ahead-of-time (AOT) static native C++ recompilation and retail port of *Mortal Kombat: Armageddon* (Sony PlayStation 2 `SLUS_214.10` and dynamic overlays). It executes 100% native C++ logic with zero emulators, zero guest CPU register state, and zero JIT translation on the accepted route.

<!-- PROGRESS:START -->
## Native Reimplementation Progress & Tracking

![Porting Progress](progress.svg)

**Verified Repository Commit**: Current `main` HEAD in [`ps2-native-recomp`](https://github.com/nahshongraham97/ps2-native-recomp)  
**Parent Canonical Repository**: [`armageddon-reimplementation`](https://github.com/nahshongraham97/armageddon-reimplementation)

### Primary Function Ledgers & Correspondence
- **PS2 Retail Function Ledger**: [`research/ledger/functions.csv`](research/ledger/functions.csv) (17,510 total catalog functions)
- **Xbox Donor Function Ledger**: [`research/ledger/xbox_functions.csv`](research/ledger/xbox_functions.csv) (22,579 donor functions in `default.xbe`)
- **Cross-Platform Correspondence Map**: [`research/ledger/ps2_xbox_map.csv`](research/ledger/ps2_xbox_map.csv) (17,511 correspondence mapping rows)
- **Task Board & Milestones**: [`TASK_BOARD.md`](TASK_BOARD.md)
- **Implementation Roadmap**: [`ROADMAP.md`](ROADMAP.md)
- **Port Status & Evidence**: [`PORT_STATUS.md`](PORT_STATUS.md)
- **Execution Contract**: [`NATIVE_PORT_EXECUTION_CONTRACT.md`](NATIVE_PORT_EXECUTION_CONTRACT.md)
- **Handoff & Repository Protocol**: [`HANDOFF.md`](HANDOFF.md)

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
> [!NOTE]
> **Investigation Authority**: Xbox default.xbe serves as an authoritative secondary binary investigation source for recovering algorithm contracts and data structures; donor credit requires verified dual-platform vectors.
> **Donor Credit Policy**: 0% donor credit means no donor credit has been awarded pending verified dual-platform vectors, not that work has stalled.

- **In-Scope Donor Utilization**: **0.00%** (0 / 21,321 in-scope donor functions)
- **Total Donor Catalog**: **22,579** discovered functions in `default.xbe`

| Category / Tier | Status Breakdown | Count | In-Scope % |
|---|---|---|---|
| **Implemented Donors** | `implemented-via-donor` (0) | 0 | 0.00% |
| **Classified Non-Executable / SDK** | `sdk-or-library` (2,524) + `no-counterpart` (9) | 2,533 | 11.88% |
| **Open Donor Frontier** | `blocked-on-dual-platform-vectors` (17 candidates pending vectors) + `investigating` (10 active recovery) + `not-started` (18,761 unmapped) | 18,788 | 88.12% |
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
1. **No-Direct-Callers Indirect Dispatch Resolution**: **2,648** functions untargeted by direct calls but referenced by data pointers or immediate formations in `SLUS_214.10` (indirect candidates across 5,400 computed call sites).
2. **Open Game Functions**: **10,141** retail functions across modules: **9,038** in `SLUS_214.10`, **1,102** in `MOVIER.ovl`, and **1** in `GAMER.ovl`.
3. **Dynamic Overlay Frontends**: **1,102** movie playback frontend functions in `MOVIER.ovl` and **1** entry stub in `GAMER.ovl`.
4. **Acceptance Gates**: Full-game entry link, visible render target presentation, synchronized audio mixing, and physical controller input parity.
<!-- PROGRESS:END -->

## Documentation & Repository Architecture

All primary documentation and tracking reside directly within this repository:

1. [`PORT_STATUS.md`](PORT_STATUS.md) — Current checkout evidence, acceptance status, and active blockers.
2. [`NATIVE_PORT_EXECUTION_CONTRACT.md`](NATIVE_PORT_EXECUTION_CONTRACT.md) — Required native behavior, no-regression rules, and acceptance criteria.
3. [`ROADMAP.md`](ROADMAP.md) — Phased implementation order, closure gates, and milestone checklists.
4. [`HANDOFF.md`](HANDOFF.md) — Repository workflow, evidence standards, and commit procedures.
5. [`TASK_BOARD.md`](TASK_BOARD.md) — Progress tracking against all 14 project milestones.
6. [`MODEL_COORDINATION.md`](MODEL_COORDINATION.md) — Agent supervision and model review procedures.

### Provenance & Historical Archive
- Historical design notes, old checkpoints, and upstream emulator documentation are archived for provenance under `research/history/` and `research/runs/`. They represent dated evidence for past builds and do not supersede live status or contract requirements.
- Retail assets (such as original game discs, `.ELF`, `.xbe`, `.bin`, `.ovl`, and audio/video containers) are not stored or distributed in this repository. All analysis is verified against pinned binary checksums on the owner's authorized system.
