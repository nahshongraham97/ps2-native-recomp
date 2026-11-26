# Repository and Local Checkout Layout

**Current compiled R192 route:** The familiar parent launchers/launch_r192_hooks.bat (also called by launch_ps2recomp.bat) selects external/forks/ps2-native-recomp-r192-closure/build/r192-compiled-konquest/out/ps2run.exe, SHA-256 8823021a23e2a3fdd3b2dc4ca0c8eac9e7359beb3e5bce489b965587a0c2479b. It links its own AOT library and sealed compiled modules with no EE/VU live compiler. The latest Konquest recording passes 22,785 new frames / all 1,519 checkpoints; all 14 saved-state probes pass 90 new frames. Code/assets/keys are local to the build; normal play mounts the SAME original R192 persistent card and uses its existing quick-state directory. No replacement card is created. The original 8AB43BDA executable/launcher remain protected comparison evidence. Physical A/V, independent held-out paths and final native-port acceptance remain OPEN. See [the existing R192 record, section 6](research/runs/r192-hooks-playable-run-20261125/RUN.md).

**Audited & Consolidated: 2026-11-24 (Phase 2).**
This document defines the physical directory layout, repository boundaries, Phase 2 move map, and the relationship between the parent integration repository, the consolidated active fork, and the separate playable oracle dataset.

---

## 1. Directory Structure & Repository Relationships

```text
<workspace-root>/                                # Parent folder containing repos and separate oracle (../../ from parent repo)
├── milestone_work_verified_playable_20261110/   # Saved-state and hash-parity oracle (ps2run.exe b9eef7da...)
│   ├── ps2run.exe, launch_game.bat, SDL2.dll    # Restored oracle runner/launcher (3,256 original files plus authorized launcher additions)
│   ├── games/mkarmageddon/disc/states/          # 133 state-related files: 61 .state snapshots + 72 .inputs sidecars
│   └── games/mkarmageddon/ee-live/, vu1-live/   # Oracle runtime live caches (44 .dlls in vu1-live/, 2,212 .dlls in ee-live/)
│
├── milestone_build_kombat_practice/             # Direct descendant snapshot (8b763dcc...) sharing oracle hook lineage cd33a7f..67612aa
│
└── Armageddon-backup/
    └── armageddon-reimplementation/             # PARENT REPOSITORY (nahshongraham97/armageddon-reimplementation)
        ├── .git/                                # Parent Git root
        ├── AGENTS.md, NATIVE_PORT_ONLY.md...    # Canonical living project documents
        ├── research/                            # Empirical run records, decisions, ledgers, history, and test vectors
        │   ├── decisions/                       # Binding owner decisions (online exclusion, owner corrections, AOT scope)
        │   ├── ledger/                          # functions.csv, xbox_functions.csv, ps2_xbox_map.csv, aot_coverage_inventory.json
        │   └── history/
        │       └── superseded-status-docs-20261124/  # Consolidated historical status documents
        ├── tools/                               # Validation, audit, ledger generation, and acceptance test scripts
        └── external/forks/
            ├── ps2-native-recomp/               # CONSOLIDATED ACTIVE FORK (nahshongraham97/ps2-native-recomp; standalone primary .git)
            │   ├── .git/                        # Fork Git root (tracks origin/main)
            │   ├── CMakeLists.txt               # Native recompilation & runtime build configuration
            │   ├── recomp/                      # Build-time AOT C++ generator sources (eerecomp, vurecomp, overlay manifests)
            │   ├── runtime/                     # Native platform runtime services & partial recovered/ported modules (legacy recomp queued for removal)
            │   ├── tests/                       # Headless gates, component tests, and regression tests
            │   ├── research/runs/               # Dated run records and empirical measurements
            │   └── build/
            │       ├── r192-hooks/              # ACTIVE PLAYABLE RECOMP BUILD & ORACLE (8ab43bda...; persistent card armageddon.ps2)
            │       └── oracle-telemetry/        # Git-ignored destination for oracle parity manifests & telemetry
            └── ps2-native-recomp-backup-20261110/    # Protected duplicate of oracle game tree (3,249 shared files SHA-256 identical)
# (Historical Phase 3 removal log lists 41 checkouts; present relocation archive hashes are recorded, but no before-removal manifest proves completeness.)
            └── ps2-native-recomp-backup-20261110/    # Protected duplicate of oracle game tree (3,249 shared files SHA-256 identical)
```

---

## 2. Phase 2 Consolidation & Move Map

| Dataset / Checkout | Previous Location | Consolidated Target / Status | Disposition |
|---|---|---|---|
| **Saved-state and hash-parity oracle** | `../../milestone_work_verified_playable_20261110` | `../../milestone_work_verified_playable_20261110` (unchanged) | **Retain (Protected)**: Protected parity dataset outside the repository (`ps2run.exe` `B9EEF7DA...`, 3,256 files restored) |
| **Kombat Practice historical snapshot** | `../../milestone_build_kombat_practice` | `../../milestone_build_kombat_practice` (unchanged) | **Retain (Oracle Lineage)**: Hook-enabled descendant (`8B763DCC...`) sharing `cd33a7f..67612aa` |
| **Active Native Port Fork** | Pre-consolidation active worktree under `external/forks/` | `external/forks/ps2-native-recomp` | **Active Primary Fork**: Promoted to standalone primary `.git` tracking `origin/main` |
| **Legacy Fork Checkout** | `external/forks/ps2-native-recomp` (`3f015562`) | Relocated protected inputs -> `private-inputs/legacy-mkarmageddon-20261118/` | **Removed (Phase 3)**: present archive contains 982 hashed files; completeness/origin before deletion unproved without a prior manifest |
| **Protected Duplicate Backup** | `external/forks/ps2-native-recomp-backup-20261110` | `external/forks/ps2-native-recomp-backup-20261110` | **Protected Duplicate**: Retained until owner approves cleanup (`3,249` files SHA-256 identical to oracle) |
| **Phase Review / Clean Worktrees (`clean-*-22`, `ps2-native-recomp-*`)** | `external/forks/*` (40 sibling MKA worktrees per `checkout_inventory.json`) | Removed in Phase 3 (`phase3_checkout_removal_log.json`) | **Removed (Phase 3)**: The Phase 3 removal log and checkout audit record protected holdings and historical gaps |
| **External Tool / Reference Mirrors** | `../../Armageddon-backup/forks/*` (64 external repos per `checkout_inventory.json`) | `../forks/*` (sibling to `armageddon-reimplementation`) | **Read-Only External References**: `ghidra`, `pcsx2`, `PS2Recomp`, `parallel-gs`, etc.; never edit |

---

## 3. Parent-Fork Checkout Mechanics

1. **Parent Repository**:
   - Location: `.` (relative to `armageddon-reimplementation`).
   - Remote: `https://github.com/nahshongraham97/armageddon-reimplementation.git` (branch `main`).
   - Role: System integration, cross-platform acceptance test runner, research records, and project governance.
   - Submodule / Fork Policy: The parent's `.gitignore` ignores `/external/forks/`. The active fork checkout at `external/forks/ps2-native-recomp` is an independent standalone Git repository and is never committed as parent tree contents. Ledgers (`research/ledger/*`, `progress.svg`, `README.md`, `TASK_BOARD.md`, `PORT_STATUS.md`) synchronize via `tools/research/generate_ledgers.py` and `tools/research/sync_verify.py`.

2. **Consolidated Active Fork Repository**:
   - Local Checkout Location: `external/forks/ps2-native-recomp` inside parent.
   - Remote: `https://github.com/nahshongraham97/ps2-native-recomp.git` (branch `main`).
   - Role: Active ahead-of-time (AOT) static native C++ recompilation, memory/ABI transform, and native runtime implementation of *Mortal Kombat: Armageddon*.
   - Path Rules (Binding): Inside every repository document, use **strictly repo-relative paths**; never use machine-local user paths or file-scheme links, and never reintroduce the retired pre-consolidation checkout name.

3. **Playable Recomp Build & Behavioral Oracle (`r192-hooks`) and preserved parity data**:
   - **Protected Original Playable Recomp Reference**: `external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe` (`8AB43BDA...`, original colocated launcher `build/r192-hooks/out/launch_r192_hooks.bat` retained; R192 `67612aa` lineage + `mka_unit_wrappers.cpp` + all 7 overlay hooks + `0x00197BC0`). Verified in the 2026-11-25 68,426-frame session (`research/runs/r192-hooks-playable-run-20261125/RUN.md`, `60.0 FPS` median; Konquest and Motor Kombat work cleanly). Its slot-1 memory card `build/r192-hooks/game/disc/armageddon.ps2` (`8,650,752` bytes, SHA-256 `f38dc412...`) is **persistent** (holds saved progress + Konquest checkpoint) and must never be overwritten or replaced with a disposable card.
   - **Parity oracle and historical snapshot**: `../../milestone_work_verified_playable_20261110` (`B9EEF7DA...`) and `../../milestone_build_kombat_practice` (`8B763DCC...`) remain preserved on disk (`milestone_work_verified_playable_20261110` is verified immutable for `-Mode Parity` state telemetry written to `build/oracle-telemetry/`). Neither historical snapshots nor `r192-hooks` satisfy the separate zero-emulation shipped-port acceptance contract.

---

## 4. Canonical Living Root Documents

Following the 2026-11-24 documentation consolidation, overlapping and superseded root status documents reside in `research/history/superseded-status-docs-20261124/`. The project maintains these living root documents:

| Document | Purpose / Scope | Authority Level |
| :--- | :--- | :--- |
| `README.md` | Primary project overview, generated progress summary, and entry points | Overview |
| `NATIVE_PORT_ONLY.md` | Non-negotiable zero-emulation rules | Binding |
| `NATIVE_PORT_EXECUTION_CONTRACT.md` | Detailed acceptance contract, non-regression matrix, and gate definitions | Binding |
| `PORT_STATUS.md` | Authoritative live port status, test receipts, and generated ledger summary | Authoritative |
| `ROADMAP.md` | Ordered implementation sequence, milestone objectives, and work queue | Planning |
| `TASK_BOARD.md` | Release criteria matrix across all 10 project milestones | Status |
| `HANDOFF.md` | Active repository orientation, boundaries, and verification protocol | Operational |
| `MODEL_COORDINATION.md` | Local source-review model workflow and supervision rules | Operational |
| `REPO_LAYOUT.md` | Directory map, checkout paths, move map, and repository relationship rules | Infrastructure |
| `AGENTS.md` | Binding operating rules for AI agents, git commit/push rules, single-main policy | Operational |
| `AGENT_ASSETS.md` / `AGENT_ASSETS_FULL.md` | Local tool, compiler, SDK, and script directory index (parent repo) | Tooling |

Phase 0 launcher correction: preserve the oracle's existing `launch_game.bat` unchanged. The corrected `launch_verified_oracle.bat` addition invokes the tracked `launchers/launch_oracle.ps1`, which uses active-fork ignored telemetry and disposable data. The authorized addition map is tracked separately; reverify the recorded original manifest plus exactly those additions after each phase.
