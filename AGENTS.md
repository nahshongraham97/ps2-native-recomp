# Agent instructions — active retail recomp fork

**Current playable R192 route:** The parent launchers/launch_r192_hooks.bat (also called by launch_ps2recomp.bat) calls the unchanged original external/forks/ps2-native-recomp/build/r192-hooks/out/launch_r192_hooks.bat and ps2run.exe (SHA-256 8ab43bda05e7a621b946883f3195a583f0c2455f0351bf8608282cd3e97b4617). Its linked base AOT code and all3569 precompiled EE cache DLLs remain available; every cached DLL was verified byte-identical to the preserved compiled package. EE/VU Live remain enabled for misses. The SAME original persistent card and quick-save directory in build/r192-hooks/game/disc are retained, with no protected card/save/executable/original-launcher modification; verification runs use disposable headless copies. The separate compiled882302... package is preserved but is no longer the default route. AOT closure remains frozen, OPEN and not required for the native port. Native Phase6 continues on main in both existing repositories under the latest owner direction; the ledger records20 verified owners,2 transformed and236 credited native after withdrawing two false legacy raster bindings; no new native credit is awarded. Final native-only acceptance remains OPEN.

[`PORT_STATUS.md`](PORT_STATUS.md) is the sole live status;
[`ROADMAP.md`](ROADMAP.md) is the implementation order;
[`NATIVE_PORT_EXECUTION_CONTRACT.md`](NATIVE_PORT_EXECUTION_CONTRACT.md) is the binding acceptance and
whole-game non-regression rule. Read those before editing this fork. The
old fork guidance is preserved at
`research/history/native-port-docs-2026-11-18/AGENTS.md` as history only.

The owner's latest direction keeps native Phase6 work on main in both existing
repositories. Stage A uses disposable outputs: proved typed parameters/returns with typed guest-memory
accessors remaining temporarily. Stage B removes that memory model through
proved native structures. This intermediate permission does not alter accepted
architecture, ownership admission or credit rules. The 5S.9 full-catalogue leaf
cohort is capability only and cannot add to either credited tier. Apply the
measured-pass stop rule in the execution contract before further bindings.

**Current bounded batch:** The bounded SSF/SEC component checkpoint is verified. All 245 outer members parse; 1,992 inner SEC sections open and 20 return named unsupported-format errors. Bundle, texture and connected-scene clients pass 339, 48 and 1,202 checks; the scene covers 978 allocation cuts, 152 visible depth-written pixels and 4,096 uploaded texels. Main load/render/reset, 13 source-defect plants, the 59-target/65-variant baseline, separate 23 clients, 20 earlier controls, fresh proofs and all 16 distinct project gates pass with the required OPEN/UNKNOWN refusals. Stop after publication directly to main in both repositories. Retail rendering policy, native geometry, remaining formats and gameplay acceptance stay OPEN. Ownership 20 / transformed 2 / native 236; all deltas are zero. Protected oracle files, cards, saves, backups and the frozen branch are unchanged.

**Branch rule:** Every change is worked on, committed and pushed directly to main in both repositories. The only permitted side branch is the existing frozen r192-aot-closure-proof in the fork. Do not create, restore, switch to or commit on any other branch. The owner explicitly directed removal of the two merged Phase6 branches; their commits remain on main. Do not interpret historical branch records as permission to recreate them.

Work in this active fork, not `../ps2-native-recomp-backup-20261110/` or
historical snapshots (`../../milestone_work_verified_playable_20261110/` `B9EEF7DA...`, `../../milestone_build_kombat_practice/` `8B763DCC...`). The restored R192-lineage build at `build/r192-hooks/out/ps2run.exe` (`8AB43BDA...`, launched via `build/r192-hooks/out/launch_r192_hooks.bat`) remains the protected verified original gameplay reference (`research/runs/r192-hooks-playable-run-20261125/RUN.md` in parent; Konquest and Motor Kombat verified); its persistent slot-1 memory card `build/r192-hooks/game/disc/armageddon.ps2` (`f38dc412...`, Konquest checkpoint + user saves) must NEVER be overwritten or reset. Discard noncompliant commits,
dirty files, and prior checkouts locally without rewriting pushed history.
Preserve protected backup files, user cards, saves, states, keys, and local model/settings.

This is one native Armageddon port in the existing repositories, not a PS2
compatibility product. Choose the best/easiest verified retail portions and
retain useful native services regardless of origin. Native saves/reload are
required; PS2 save bytes/import are optional. Do not rewrite a native format
merely to rename it. Do not create another repository or upstream PR.

Do not claim acceptance from `runtime/src/compliance_census.cpp` or the
300/2,500/3,000-frame scripted records. The census checks counters, not
source reachability or correct card, audio, physical input, movie, visual,
combat, and mode behavior. No feature may regress while replacing a PS2
boundary. Unknown IDs, imports, formats, packets, and calls must fail by
name and block the release, not select a default or silently succeed.

For each change, follow the parent ROADMAP step and contract evidence row:
exact retail caller/bytes, typed native behavior including errors, source
diff, normal/error/lifetime tests, same-scenario behavioral comparison,
forbidden-path source audit, clean binary hash and matching run record.
Keep the backup and old run measurements intact. Commit only authored files
to this fork's `main` and push to `origin/main` after verifying the diff.

For Qwen/LM Studio, follow [`MODEL_COORDINATION.md`](MODEL_COORDINATION.md); do not change
settings or impose inference caps. If no instance is loaded, read source
directly. Do not ask the owner to launch while any contract row is open.

The parent AGENTS.md section "ELF authority, implementation and verification
order" is binding here, including its mandatory five-step transform-and-verify
protocol for AOT-transformed retail logic and platform-boundary implementations: selected original executable/data -> verified instructions/ABI
-> decompiler used only for orientation and caller/consumer cross-checks ->
typed native C++ -> independently derived behavioral checks. Record input
identity, exact ranges, instruction widths/extensions, delay-slot effects,
errors/lifetime and unresolved behavior. The EE plugin, a wrapper name or a
decompiled type does not prove correctness. Unknown behavior is an explicit
blocker, never a guessed implementation. An exact-byte disassembly check is
distinct from behavioral equivalence and full-game acceptance. Keep the full
protocol in the parent rule section; do not maintain a conflicting local copy.
Its "Required per-function evidence and claim gate" is also binding: record
the actual EE listing used and original-word comparison, proved behavior,
native destination, independent expectations and separate verification status.
An installed plugin, agent agreement or a passing component test is not proof
of source semantics or accepted gameplay.
Advance retail entry -> native initialization -> game loop -> real input ->
rendering and sound. Defer optional work until the connected full build;
run the full recorded baseline before and after each step and check affected changes directly. Never replace
production progress with isolated wrappers, repeated reports or test counts.

## Xbox renderer, viewport, and HD presentation rules

The owner's amendments A1-A8 control the work dependencies. Step 5S depends
on completed 6.1, 6.3 and 6.4; asset steps 7.2-7.4 are independent. Each 7.1
replacement requires its own ownership, transformation and differential proof.
Publish completed phase changes with baseline, ledger and oracle verification.
A blocked item pauses its dependents. Continue independent work before
publishing a stop record. Existing oracle contents are immutable; Phase 0
permits only specifically recorded additions. Protect assets, saves, states,
keys, settings, the duplicate backup and required generator/toolchain files.
Keep writes within repositories except authorized oracle additions and the
Phase 7 extraction into existing private-inputs. Raw telemetry belongs in an
existing ignored folder; append to the existing work-item record and cite raw
logs by hash. Create no new top-level documents/directories or dated notes.
Do not access the reserved local model.

The parent saved-state evidence gate is mandatory. Version 8 serializes VU,
then variable-length IPU, then GS; never infer VU code by subtracting from GS.
Pin producer/version/ABI and exact source regions, validate bounds before
restoring state, and keep IPU-gap/truncation/version regressions. Program-key
agreement cannot authenticate a mis-extraction. Synthetic inputs, code
identity and register/memory/packet equivalence remain distinct evidence.

The Xbox image supports read-only library identification and measured PS2/Xbox capability comparisons. The owner decides the route. Xbox-derived algorithms are eligible only for renderer, viewport and HD presentation after comparison. GS packet decoding remains interim. Transform retail logic from pinned PS2 AOT output and retain the verified Windows platform services. The 0406 CPU-image comparison found no substantial Xbox-specific dependency reduction.

1. **Preserve existing native foundation**: Native files, content volume, async reads, heaps, Session, tasks, Main reentry, clocks, typed profiles, input sampling, audio decoders/jobs, textures/fonts, and GPU resources carry forward. Do not replace them with an Xbox runtime or emulated device.
2. **Reconcile profile and player records**: Transfer state meanings, not console offsets. Native save/load is required; PS2 save-byte compatibility/import is optional. Keep useful native codecs where easiest without constraining the native state model. Establish actual per-mode player requirements rather than console limits.
3. **Reconcile numerical behavior**: Distinguish gameplay-critical arithmetic from rendering arithmetic. Xbox x87/SSE routines can replace complex EE math only when proven functionally or visually equivalent without weakening parity.
4. **Enforce explicit asset provenance**: Do not assume matching filenames mean matching bytes. Distinguish version-specific decoders from shared native objects.
5. **HD presentation policy**: Use Xbox 720p as the HD reference. Keep logical coordinates (640×448), render target resolution, and presentation resolution distinct. Apply widescreen camera projections instead of 4:3 stretching. Maintain simulation timing strictly independent of render resolution.

After each code/config change, test result, verified finding, decision,
blocker, or rollback, update all relevant **current documents** in place
(`README.md`, `TASK_BOARD.md`, `PORT_STATUS.md`, `ROADMAP.md`, `HANDOFF.md`,
`NATIVE_PORT_EXECUTION_CONTRACT.md`) before beginning the next implementation step;
do not spawn overlapping root status docs or arbitrary run directories.
Parent & fork `PORT_STATUS.md` own live state;
`ROADMAP.md` owns task order; the execution contract owns acceptance; dated
run records own observations. Reconcile all conflicting current claims in
place and mark superseded material historical. Include doc updates with the
authored change; do not accumulate contradictory addenda or independent
current snapshots.
