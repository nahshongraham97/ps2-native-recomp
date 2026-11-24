# Active handoff — retail Armageddon native port

**Audited 2026-11-22. Whole-game native acceptance: OPEN; no owner launch.**
The initial Nov 18 baseline handoff is preserved at `research/history/native-port-docs-2026-11-18/HANDOFF.md`.

1. **Active Repositories & Worktrees**:
   - Work in `external/forks/retail-main-22/` for the retail runtime source (HEAD `d1bfc0e2`).
   - Parent repository is `C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation` (HEAD `effbbbf4`).
   - `external/forks/ps2-native-recomp-backup-20261110/`, user saves, states, cards, and original assets are strictly preserved.
   - Isolated private Ghidra analysis sessions (`a92b59f4`) are established for Xbox (`default.xbe`) and PS2 (`SLUS_214.10`) binaries.

2. **Required Reading & Orientation**:
   - Before editing, read [PORT_STATUS.md](PORT_STATUS.md) for live audited state, [NATIVE_PORT_EXECUTION_CONTRACT.md](NATIVE_PORT_EXECUTION_CONTRACT.md) for non-negotiable rules, [ROADMAP.md](ROADMAP.md) for implementation order, and the specific run/source record cited by the task.
   - Verify parent and fork HEAD cleanliness. Both parent (`origin/main` `effbbbf4`) and retail fork (`origin/main` `d1bfc0e2`) are clean and in sync.

3. **Port Strategy & Donor Selection**:
   - The product is **ONE genuine native Armageddon port**, not a PS2 compatibility runtime or an emulated engine.
   - **Xbox-first investigation** for everything still to be implemented: investigate the 32-bit x86 Xbox binary first to recover algorithms and desktop-like boundaries, and select verified portions that reduce connected native work.
   - Cross-check with PS2 for differences, compatibility, or simpler verified logic.
   - Both versions join **one authoritative native architecture**: shared native Session, task system, memory owners, host loose-file VFS, and host OpenGL GPU services.
   - Native save/reload is required; legacy console save byte import is an optional input adapter, not an acceptance gate.

4. **Zero-Emulation Boundary**:
   - Absolute zero guest console emulation: zero `g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JIT/interpreter, or console memory header transplants.
   - Platform operations terminate in native Windows/host services (Win32, OpenGL, SDL2, host filesystem).

5. **Current Runtime State & Active Frontier**:
   - **Renderer Modules 0402–0407 Implemented & Passing**:
     - 0402 matrix owner defaults, setter dispatch, exact math.
     - 0403 base-frame hierarchy, dirty order, physical release (120 allocations).
     - 0404 read streams, partial/EOF/skip64, checked close without ISO/CDVD.
     - 0405 base cameras, 64 records, 7 exact SSE depth cases.
     - 0406 shared CPU images, 512 independent gamma bytes, real decoder search.
     - 0407 shared rasters: defect-remediated in place (destination capacity/bounds checks in `set_from_image` rejecting 16bpp; real offscreen `HiddenGl` WGL fixture with `glIsTexture` verification; gamma bit 0 propagation per 2C0EB0; `pop_context` returning popped target per 2B9D50; unconditional type-4 depth 32 normalization per 2C8BD0; resumable `destroy_cursor` in destructor; explicit GPU texture revocation).
   - **All 9 renderer component clients pass**: `rasters`, `images`, `cameras`, `streams`, `frames`, `core`, `matrix`, `registration`, `allocator`.
   - **Full-Entry Link Diagnostic**: Exactly 45 undefined symbols remaining (neither `construct_core_plugin_0407` nor `destroy_core_plugin_0407` appear).
   - **Active Stage**: 0407 remains **OPEN** awaiting connected render-target, first-scene, and whole-engine integration; no advancement to 0408 is claimed.

6. **Acceptance & Launch Gate**:
   - Do NOT direct the owner to run `launch_game.bat` or any other build until the contract's complete, clean, same-binary acceptance matrix passes.
   - Whole-game native acceptance remains strictly **OPEN**.

7. **Delivery Report Requirements & Falsification Checklist**:
   - Every delivery report must be generated strictly from repository state (counts from ledgers, SHAs from `git rev-parse`, links to fork `ps2-native-recomp` and parent `armageddon-reimplementation`).
   - Every delivery report MUST conclude with explicit sections for:
     - **Not verified**: Open questions, unverified assumptions, and unreached code.
     - **Contradictions with earlier documents**: Discrepancies identified between earlier claims/documents and current disassembly/source truth.
     - **Falsification attempts**: Results of actively attempting to falsify the five strongest claims before delivery, including negative and disconfirming findings.
