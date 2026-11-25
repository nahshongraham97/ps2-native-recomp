# Agent instructions — active retail recomp fork

[`PORT_STATUS.md`](PORT_STATUS.md) is the sole live status;
[`ROADMAP.md`](ROADMAP.md) is the implementation order;
[`NATIVE_PORT_EXECUTION_CONTRACT.md`](NATIVE_PORT_EXECUTION_CONTRACT.md) is the binding acceptance and
whole-game non-regression rule. Read those before editing this fork. The
old fork guidance is preserved at
`research/history/native-port-docs-2026-11-18/AGENTS.md` as history only.

Work in this active fork, not `../ps2-native-recomp-backup-20261110/` or
historical snapshots (`../../milestone_work_verified_playable_20261110/` `B9EEF7DA...`, `../../milestone_build_kombat_practice/` `8B763DCC...`). The restored R192-lineage build at `build/r192-hooks/out/ps2run.exe` (`8AB43BDA...`, launched via `build/r192-hooks/out/launch_r192_hooks.bat`) IS the active verified playable recomp build and behavioral oracle (`research/runs/r192-hooks-playable-run-20261125/RUN.md` in parent; Konquest and Motor Kombat verified); its persistent slot-1 memory card `build/r192-hooks/game/disc/armageddon.ps2` (`f38dc412...`, Konquest checkpoint + user saves) must NEVER be overwritten or reset. Discard noncompliant commits,
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
compile and check directly affected changes without rechecking unchanged
components unless a failure or material concern requires it. Never replace
production progress with isolated wrappers, repeated reports or test counts.

## Xbox renderer, viewport, and HD presentation rules

The owner's corrected complete instruction set controls phases 0 through 7.
Do not start the next phase until the prior phase is committed, pushed,
passing its baseline, ledger-synchronized and oracle-manifest verified.
If a phase cannot finish, publish verified work, put the exact resume line
in TASK_BOARD.md and stop. Existing oracle contents are immutable; Phase 0
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

Per `DECISION_OWNER_CORRECTIONS_20261124` and parent `ROADMAP.md` / `PORT_STATUS.md`:
Xbox investigation is **limited strictly to the renderer, viewport, and HD presentation**,
with GS packet decoding as an interim concession only (not a final target). Retail game
logic outside platform boundaries comes from AOT-generated PS2 retail C++ transformed
to eliminate `g_ram`, `c.r`, `$gp`, and guest vtables. Within the renderer/HD scope,
Xbox reference is an investigation priority, not automatic adoption or a claim
that every module becomes substantially easier. Follow the parent AGENTS.md
donor-selection rule:choose verified behavior that reduces actual connected
native work/dependencies,record concrete savings or their absence,and implement
shared CPU behavior once using existing services. Shorter listings are not
savings evidence;do not clone either console SDK/CRT/device or redo working
native implementations. Bound comparisons to the current decision rather
than building two versions.0406 establishes no substantial Xbox-specific
dependency reduction within its recovered CPU-image scope.

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
