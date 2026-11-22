# Agent instructions — active retail recomp fork

The parent project's `../../../PORT_STATUS.md` is the sole live status;
`../../../ROADMAP.md` is the implementation order;
`../../../NATIVE_PORT_EXECUTION_CONTRACT.md` is the binding acceptance and
whole-game non-regression rule. Read those before editing this fork. The
old fork guidance is preserved at
`research/history/native-port-docs-2026-11-18/AGENTS.md` as history only.

Work in this active fork, not `../ps2-native-recomp-backup-20261110/` or the
Kombat Practice package. Preserve existing uncommitted `runtime/src/iop/
mcserv.cpp` and `msl_native.cpp` work; neither is verified by a matching
clean run. Preserve user cards, saves, states, keys, and local model/settings.

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

For Qwen/LM Studio, follow `../../../MODEL_COORDINATION.md`; do not change
settings or impose inference caps. If no instance is loaded, read source
directly. Do not ask the owner to launch while any contract row is open.

The parent AGENTS.md section "ELF authority, implementation and verification
order" is binding here, including its mandatory five-step protocol for every
ported function: original ELF words/data -> verified EE instruction semantics
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

## Xbox-first recovery and HD presentation rules

Per the architecture plan in parent ROADMAP.md and PORT_STATUS.md:
1. **Preserve existing native foundation**: Native files, content volume, async reads, heaps, Session, tasks, Main reentry, clocks, typed profiles, input sampling, audio decoders/jobs, textures/fonts, and GPU resources carry forward. Do not replace them with an Xbox runtime or emulated device.
2. **Reconcile profile and player records**: Transfer field meanings, not structure offsets. Preserve existing PS2 save file decoding independently of runtime layout. Establish player limits per mode (do not impose 2-player limit where 4-player is required, such as Motor Kombat).
3. **Reconcile numerical behavior**: Distinguish gameplay-critical arithmetic from rendering arithmetic. Xbox x87/SSE routines can replace complex EE math only when proven functionally or visually equivalent without weakening parity.
4. **Enforce explicit asset provenance**: Do not assume matching filenames mean matching bytes. Distinguish version-specific decoders from shared native objects.
5. **HD presentation policy**: Use Xbox 720p as the HD reference. Keep logical coordinates (640×448), render target resolution, and presentation resolution distinct. Apply widescreen camera projections instead of 4:3 stretching. Maintain simulation timing strictly independent of render resolution.

After each code/config change, test result, verified finding, decision,
blocker, or rollback, update every affected active document before beginning
the next implementation step. Parent `PORT_STATUS.md` owns live state;
`ROADMAP.md` owns task order; the execution contract owns acceptance; dated
run records own observations. Reconcile all conflicting current claims in
place and mark superseded material historical. Include doc updates with the
authored change; do not accumulate contradictory addenda or independent
current snapshots.
