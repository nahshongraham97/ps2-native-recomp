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

After each code/config change, test result, verified finding, decision,
blocker, or rollback, update every affected active document before beginning
the next implementation step. Parent `PORT_STATUS.md` owns live state;
`ROADMAP.md` owns task order; the execution contract owns acceptance; dated
run records own observations. Reconcile all conflicting current claims in
place and mark superseded material historical. Include doc updates with the
authored change; do not accumulate contradictory addenda or independent
current snapshots.
