# Active handoff — retail Armageddon native port

**Audited 2026-11-24. Whole-game native acceptance: OPEN; no owner launch.**
The initial Nov 18 baseline handoff is preserved at `research/history/native-port-docs-2026-11-18/HANDOFF.md`.

1. **Active Repositories & Worktrees**:
   - Active retail runtime & AOT fork: `external/forks/ps2-native-recomp/` (consolidated in Phase 2; tracks `origin/main`).
   - Canonical parent integration repository: `armageddon-reimplementation` (`origin/main`).
   - **Active Playable Recomp Build & Behavioral Oracle**: `external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe` (`8AB43BDA05E7A621B946883F3195A583F0C2455F0351BF8608282CD3E97B4617`, launched via `launchers/launch_r192_hooks.bat` / `out/launch_r192_hooks.bat`, D3D12 + guest audio + all hooks). Verified by the 2026-11-25 `68,426`-frame session (`research/runs/r192-hooks-playable-run-20261125/RUN.md`; `60.0 FPS` median; **Konquest** and **Motor Kombat** working properly). Its slot-1 memory card (`external/forks/ps2-native-recomp/build/r192-hooks/game/disc/armageddon.ps2`, SHA-256 `f38dc412...`) is **persistent** (holds saved progress and a Konquest checkpoint) and must never be overwritten or swapped for a disposable card.
   - Preserved historical oracle/backup directories (`../../milestone_work_verified_playable_20261110/`, `B9EEF7DA...`, 3,256 original files + authorized additions; `../../milestone_build_kombat_practice/`, `8B763DCC...`; `external/forks/ps2-native-recomp-backup-20261110/`) remain immutable on disk; user saves, states, cards, and original assets stay protected. Noncompliant commits and dirty files remain discarded locally.
   - Offline xemu guest-trace capture (`<xemu-install>/xemu.exe` via `-s -S`) is permitted under the dated 2026-11-24 owner exception with muted audio and minimized window while acceptance remains OPEN.

2. **Required Reading & Orientation**:
   - Before editing, read [PORT_STATUS.md](PORT_STATUS.md) for live audited state, [NATIVE_PORT_EXECUTION_CONTRACT.md](NATIVE_PORT_EXECUTION_CONTRACT.md) for non-negotiable rules, [ROADMAP.md](ROADMAP.md) for implementation order, and the specific run/source record cited by the task.
   - Verify parent and fork `HEAD` cleanliness and ledger synchronization (`python tools/research/sync_verify.py`).

3. **Architecture, AOT Compilation & Donor Scope**:
   - The product is **ONE genuine native Armageddon port**: AOT static C++ compilation for all PS2 retail logic (`SLUS_214.10`, `MOVIER.ovl`, `GAMER.ovl`, VU0/VU1, and dynamic entry windows), transformed to typed native memory and ABI (zero `g_ram`, zero `c.r`, zero `$gp`, zero guest vtables), with hand-written C++ strictly at platform boundaries.
   - **Xbox investigation scope**: limited strictly to the renderer, viewport, and HD presentation (`DECISION_OWNER_CORRECTIONS_20261124.json`). GS packet decoding is an interim concession, not a target.
   - **Online/network exclusion**: `NETR.ovl` (1,834 functions) and `XONLINE` are out of scope per `research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json`.
   - Native save/reload is required; legacy console save byte import is an optional input adapter, not an acceptance gate.

4. **Zero-Emulation Boundary**:
   - Absolute zero guest console emulation on the accepted route: zero `g_ram`, `g_spr`, `g_memory`, renamed/relocated RAM arrays or guest-memory shims, zero `c.r[...]`, emulated SPU2/IOP, JIT/interpreter, or console memory header transplants.
   - Platform operations terminate in native Windows/host services (Win32, OpenGL, SDL2, host filesystem).

5. **Current Runtime State & Active Frontier**:
   - **Renderer & Presentation Component Clients (15/15 Passing)**: `0401`–`040b` (`matrix`, `frames`, `streams`, `cameras`, `images`, `rasters`, `raster_registration`, `textures`, `texture_dictionaries`, `materials`, `geometry`, `world`, `core`, `allocator`, `display_movie_presentation`) plus `72/72` xemu movie-variant oracle match (`test_native_retail_movie_variant_oracle`).
   - **Full-Entry Link & Headless Gate**: `0` undefined symbols remaining in negative-link check (`test_native_startup_missing_bindings`), with `26` registered unrecovered placeholders in `research/ledger/ported_function_evidence.json`; 60-frame headless entry (`test_native_headless_entry_run`) reaches frame 5 task `0x18e100` movie presentation (`LOGOW.SFD` frame 0, 1 GL draw, 1 present, `0` frame readbacks) and records 369 calls across 13/26 placeholders.
   - **Active AOT Work**: Published Phases 0–5 carry forward within their recorded scope, including the actual generated-unit prototype. Phase 6's bounded engine-family census and renderer decision are verified; exact library ownership remains OPEN at the TASK_BOARD.md resume line. Publish the partial checkpoint and stop before Phase 7. Unobserved programs, historical invocations and general hardware numeric equivalence remain OPEN.

6. **Acceptance & Launch Gate**:
   - Do NOT direct the owner to run `launch_game.bat` or any other build until the contract's complete, clean, same-binary acceptance matrix passes.
   - Whole-game native acceptance remains strictly **OPEN** (`238 / 15,670` credited in `research/ledger/functions.csv`, `0` whole-game acceptance).

7. **Doc Sync & Delivery Report Requirements**:
   - Update all relevant current docs in place (`README.md`, `TASK_BOARD.md`, `PORT_STATUS.md`, `ROADMAP.md`, `HANDOFF.md`, `REPO_LAYOUT.md`, `NATIVE_PORT_EXECUTION_CONTRACT.md`); regenerate and sync ledgers after every phase.
   - Every delivery report must be generated strictly from repository state and MUST conclude with:
     - **Not verified**: Open questions, unverified assumptions, and unreached code.
     - **Contradictions with earlier documents**: Discrepancies identified between earlier claims/documents and current disassembly/source truth.
     - **Falsification attempts**: Results of actively attempting to falsify the strongest claims before delivery, including negative and disconfirming findings.

Current resume point is TASK_BOARD.md. Coverage must use actual callable entry maps. The old restored oracle launcher is historical because its target is retired; the corrected additive launcher has passed static/disposable-data checks; bounded headless admission is instruction-grounded. Do not alter any existing oracle bytes.

Phase 4 has been published with synchronized ledgers and unchanged oracle
originals. Phase 5's actual generated GAMER/MOVIER initializer prototype passes
saved-state differential, cost and structural checks; its source/binary/proof
chain determines prototype credit. General pointer/vtable/dispatch recovery and
production integration remain OPEN. An ELF-word decoder alone does not fulfill
the generated-unit task. The checkpoint is published. Phase 6's bounded census
and renderer decision are verified, while exact RenderWare ownership remains
OPEN. Publish the partial checkpoint and stop at the TASK_BOARD.md resume line;
Phase 7 must not start before the exact-census gate passes.

Saved-state evidence correction: the old backwards-from-GS VU extraction crossed IPU. Version/ABI/bounds/variable-IPU regressions and provenance-based capture admission guard the corrected loader. The bounded GAMER boundary and independent selected VU snapshot replay pass, including full registers/memory and ordered packet bytes. This is not captured historical invocation or equivalence of untested functions/programs. No authenticated VU0 microprogram payload/launch stream was found in the recorded corpus.
