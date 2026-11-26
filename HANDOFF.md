# Active handoff — retail Armageddon native port

**Native Phase6 criterion correction (2026-11-26):** Under the original61-state-plus-constructed criterion,0/30 original and0/30 independent-sample functions are established; this is missing game-state evidence, not30 observed mismatches. The61 saved EE prefixes have no selected entry. Separately, constructed-only control results remain29/30 and25/30 (1070 exact comparisons,226 declined inputs); they are not comparable to the earlier0/30 refusal-before-execution result. The prior constructed-only N5 continuation claim is superseded. Ownership and credit stay2/238. The owner separately requested continued work on the existing44 return obligations,six refusals and camera integration. Full per-function scopes are in [the existing Phase6 work item](https://github.com/nahshongraham97/ps2-native-recomp/blob/main/research/runs/renderer-scope.json), native_phase6_memory_abi_20261126.original_criterion_review_20261126.

**Current playable R192 route:** The parent launchers/launch_r192_hooks.bat (also called by launch_ps2recomp.bat) calls the unchanged original external/forks/ps2-native-recomp/build/r192-hooks/out/launch_r192_hooks.bat and ps2run.exe (SHA-256 8ab43bda05e7a621b946883f3195a583f0c2455f0351bf8608282cd3e97b4617). Its linked base AOT code and all3569 precompiled EE cache DLLs remain available; every cached DLL was verified byte-identical to the preserved compiled package. EE/VU Live remain enabled for misses. The SAME original persistent card and quick-save directory in build/r192-hooks/game/disc are retained, with no card/save/executable/original-launcher modification or new game run. The separate compiled882302... package is preserved but is no longer the default route. AOT closure remains frozen, OPEN and not required for the native port. Native Phase6 continues on main in both existing repositories under the latest owner direction; admission/credit remain2 transformed/238 native until the existing gates pass. Final native-only acceptance remains OPEN.

**Audited 2026-11-24. Whole-game native acceptance: OPEN; no owner launch.**
The initial Nov 18 baseline handoff is preserved at `research/history/native-port-docs-2026-11-18/HANDOFF.md`.

1. **Active Repositories & Worktrees**:
   - Active retail runtime & AOT fork: `external/forks/ps2-native-recomp/` (consolidated in Phase 2; tracks `origin/main`).
   - Canonical parent integration repository: `armageddon-reimplementation` (`origin/main`).
   - **Protected Original Playable Recomp Reference**: `external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe` (`8AB43BDA05E7A621B946883F3195A583F0C2455F0351BF8608282CD3E97B4617`, original colocated launcher `build/r192-hooks/out/launch_r192_hooks.bat` retained, D3D12 + guest audio + all hooks). Verified by the 2026-11-25 `68,426`-frame session (`research/runs/r192-hooks-playable-run-20261125/RUN.md`; `60.0 FPS` median; **Konquest** and **Motor Kombat** working properly). Its slot-1 memory card (`external/forks/ps2-native-recomp/build/r192-hooks/game/disc/armageddon.ps2`, SHA-256 `f38dc412...`) is **persistent** (holds saved progress and a Konquest checkpoint) and must never be overwritten or swapped for a disposable card.
   - R192 supplies gameplay and telemetry evidence. The November 10 milestone supplies saved-state and hash parity. Kombat Practice is the historical snapshot. Owner direction on 2026-11-26 keeps the original R192 as the playable oracle with live translation; universal oracle closure is not a prerequisite for the native port. Preserve the backups, saves and cards.
   - Run headless with disposable data, hidden output and muted audio. The earlier xemu captures provide historical comparison evidence.

2. **Required Reading & Orientation**:
   - Before editing, read [PORT_STATUS.md](PORT_STATUS.md) for live audited state, [NATIVE_PORT_EXECUTION_CONTRACT.md](NATIVE_PORT_EXECUTION_CONTRACT.md) for non-negotiable rules, [ROADMAP.md](ROADMAP.md) for implementation order, and the specific run/source record cited by the task.
   - Verify parent and fork `HEAD` cleanliness and ledger synchronization (`python tools/research/sync_verify.py`).

3. **Architecture, AOT Compilation & Donor Scope**:
   - The product is **ONE genuine native Armageddon port**: AOT static C++ compilation for all PS2 retail logic (`SLUS_214.10`, `MOVIER.ovl`, `GAMER.ovl`, VU0/VU1, and dynamic entry windows), transformed to typed native memory and ABI (zero `g_ram`, zero `c.r`, zero `$gp`, zero guest vtables), with hand-written C++ strictly at platform boundaries.
   - The Xbox image supports read-only library identification and measured PS2/Xbox capability comparisons. The owner decides the route. Xbox-derived algorithms are eligible only for renderer, viewport and HD presentation after comparison. GS packet decoding remains interim.
   - **Online/network exclusion**: `NETR.ovl` (1,834 functions) and `XONLINE` are out of scope per `research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json`.
   - Native save/reload is required; legacy console save byte import is an optional input adapter, not an acceptance gate.

4. **Zero-Emulation Boundary**:
   - Absolute zero guest console emulation on the accepted route: zero `g_ram`, `g_spr`, `g_memory`, renamed/relocated RAM arrays or guest-memory shims, zero `c.r[...]`, emulated SPU2/IOP, JIT/interpreter, or console memory header transplants.
   - Platform operations terminate in native Windows/host services (Win32, OpenGL, SDL2, host filesystem).

5. **Current Runtime State & Active Frontier**:
   - **Priority oracle work:** Use branch r192-aot-closure-proof in sibling worktree ps2-native-recomp-r192-closure. Keep the running R192 v8 source/executable/card protected; current main uses v14 and is not the same source snapshot. Closure audit stays OPEN pending exact image/binary binding, template evidence, full-suite and held-out comparisons. The reviewed native-study work is now consolidated in main. See the existing R192 run record.
   - **5S.9 / initial native Phase6:** The frozen 30-leaf capability cohort returned 30 refusals and zero whole-function passes. General binding rounds stop; camera stays parked. The original 17 admitted-function table is frozen. One separate E1 dependency admission adds 001A3C40 and one tested Stage A ABI function (189 boundary comparisons); current roster 18, transformed 2/native 238. Stage B and production caller routing remain open. Use [the existing fork record](https://github.com/nahshongraham97/ps2-native-recomp/blob/8e3e2a8675500620030bfacd5d5920b6cf38c5d6/research/runs/renderer-scope.json), section native_phase6_leaf_slice, for all tables, proofs, limits and publication.
   - **Renderer & Presentation Component Clients (15/15 Passing)**: `0401`–`040b` (`matrix`, `frames`, `streams`, `cameras`, `images`, `rasters`, `raster_registration`, `textures`, `texture_dictionaries`, `materials`, `geometry`, `world`, `core`, `allocator`, `display_movie_presentation`) plus `72/72` xemu movie-variant oracle match (`test_native_retail_movie_variant_oracle`).
   - **Full-Entry Link & Headless Gate**: `0` undefined symbols remaining in negative-link check (`test_native_startup_missing_bindings`), with `26` registered unrecovered placeholders in `research/ledger/ported_function_evidence.json`; 60-frame headless entry (`test_native_headless_entry_run`) reaches frame 5 task `0x18e100` movie presentation (`LOGOW.SFD` frame 0, 1 GL draw, 1 present, `0` frame readbacks) and records 369 calls across 13/26 placeholders.
   - Phases 0–5 retain their published evidence. Step H precedes the bounded ownership census and transform scale-out study. Each Phase 7 replacement requires ownership evidence and differential execution.

6. **Acceptance & Launch Gate**:
   - Do NOT direct the owner to run `launch_game.bat` or any other build until the contract's complete, clean, same-binary acceptance matrix passes.
   - Whole-game native acceptance remains strictly **OPEN** (`238 / 15,670` credited in `research/ledger/functions.csv`, `0` whole-game acceptance).

7. **Doc Sync & Delivery Report Requirements**:
   - Update all relevant current docs in place (`README.md`, `TASK_BOARD.md`, `PORT_STATUS.md`, `ROADMAP.md`, `HANDOFF.md`, `REPO_LAYOUT.md`, `NATIVE_PORT_EXECUTION_CONTRACT.md`); regenerate and sync ledgers after every phase.
   - Every delivery report must be generated strictly from repository state and MUST conclude with:
     - **Not verified**: Open questions, unverified assumptions, and unreached code.
     - **Contradictions with earlier documents**: Discrepancies identified between earlier claims/documents and current disassembly/source truth.
     - **Falsification attempts**: Results of actively attempting to falsify the strongest claims before delivery, including negative and disconfirming findings.

Follow the resume line in TASK_BOARD.md. Phase 5 measured two generated overlay initializer transformations. General pointer recovery, virtual dispatch and production integration remain open.

Saved-state evidence correction: the old backwards-from-GS VU extraction crossed IPU. Version/ABI/bounds/variable-IPU regressions and provenance-based capture admission guard the corrected loader. The bounded GAMER boundary and independent selected VU snapshot replay pass, including full registers/memory and ordered packet bytes. This is not captured historical invocation or equivalence of untested functions/programs. No authenticated VU0 microprogram payload/launch stream was found in the recorded corpus.
