# Repository and Local Checkout Layout

**Audited & Consolidated: 2026-11-24 (Phase 2).**
This document defines the physical directory layout, repository boundaries, Phase 2 move map, and the relationship between the parent integration repository, the consolidated active fork, and the separate playable oracle dataset.

---

## 1. Directory Structure & Repository Relationships

```text
<workspace-root>/                                # Parent folder containing repos and separate oracle (../../ from parent repo)
├── milestone_work_verified_playable_20261110/   # Canonical playable oracle (ONLY separate reference dataset dir; ps2run.exe b9eef7da...)
│   ├── ps2run.exe, launch_game.bat, SDL2.dll    # Restored oracle runner/launcher (3,256 total files; 3,250 original files untouched)
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
            │   └── build/oracle-telemetry/      # Git-ignored destination for oracle parity manifests & telemetry
            └── ps2-native-recomp-backup-20261110/    # Protected duplicate of oracle game tree (3,249 shared files SHA-256 identical)
# (41 noncompliant checkouts including legacy dirty tree removed in Phase 3 after relocating 982 unique files to private-inputs/legacy-mkarmageddon-20261118/)
            └── ps2-native-recomp-backup-20261110/    # Protected duplicate of oracle game tree (3,249 shared files SHA-256 identical)
```

---

## 2. Phase 2 Consolidation & Move Map

| Dataset / Checkout | Previous Location | Consolidated Target / Status | Disposition |
|---|---|---|---|
| **Canonical Playable Oracle** | `../../milestone_work_verified_playable_20261110` | `../../milestone_work_verified_playable_20261110` (unchanged) | **Retain (Protected)**: SOLE separate directory outside repo (`ps2run.exe` `B9EEF7DA...`, 3,256 files restored) |
| **Kombat Practice Oracle Descendant** | `../../milestone_build_kombat_practice` | `../../milestone_build_kombat_practice` (unchanged) | **Retain (Oracle Lineage)**: Hook-enabled descendant (`8B763DCC...`) sharing `cd33a7f..67612aa` |
| **Active Native Port Fork** | Pre-consolidation active worktree under `external/forks/` | `external/forks/ps2-native-recomp` | **Active Primary Fork**: Promoted to standalone primary `.git` tracking `origin/main` |
| **Legacy Fork Checkout** | `external/forks/ps2-native-recomp` (`3f015562`) | Relocated protected inputs -> `private-inputs/legacy-mkarmageddon-20261118/` | **Removed (Phase 3)**: 982 unique files relocated before deletion; AOT reproduced cleanly via `reproduce_aot_pipeline.py` |
| **Protected Duplicate Backup** | `external/forks/ps2-native-recomp-backup-20261110` | `external/forks/ps2-native-recomp-backup-20261110` | **Protected Duplicate**: Retained until owner approves cleanup (`3,249` files SHA-256 identical to oracle) |
| **Phase Review / Clean Worktrees (`clean-*-22`, `ps2-native-recomp-*`)** | `external/forks/*` (40 sibling MKA worktrees per `checkout_inventory.json`) | Removed in Phase 3 (`phase3_checkout_removal_log.json`) | **Removed (Phase 3)**: Verified 0 protected/dirty files before deletion (`God-Of-War-Native-Port` & `MortalKombat` retained) |
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

3. **Playable Oracle Reference Dataset**:
   - Location: `../../milestone_work_verified_playable_20261110` relative to parent repo (`../../milestone_build_kombat_practice` is its hook-enabled descendant).
   - Policy: `milestone_work_verified_playable_20261110` is the **only separate reference dataset directory** outside the repository tree. It serves solely as an offline behavioral/parity oracle (supports `--no-hooks` / `--parity` and writes telemetry only to git-ignored `external/forks/ps2-native-recomp/build/oracle-telemetry/`). Neither oracle nor Kombat Practice is an accepted zero-emulation shipped port.

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
