# ps2-native-recomp fork — Armageddon retail port

**Armageddon acceptance is OPEN (audited 2026-11-18).** This is the active
retail-game fork. The parent project's
[`PORT_STATUS.md`](../../../PORT_STATUS.md) gives current evidence,
[`ROADMAP.md`](../../../ROADMAP.md) gives the implementation order, and
[`NATIVE_PORT_EXECUTION_CONTRACT.md`](../../../NATIVE_PORT_EXECUTION_CONTRACT.md)
defines native-only and whole-game acceptance. Do not ask the owner to
launch until every contract row passes on one clean pinned build.

This fork contains ahead-of-time retail C++ and transitional PS2-oriented
runtime code. A zero census in a short run does not establish that all
interpreters/devices/guest memory are removed or that the game behaves
correctly. The card/audio source and owner-reported card, sound and input regressions
still lack matching current complete-game evidence and remain release blockers. The safety copy at
`../ps2-native-recomp-backup-20261110/` and user data must remain intact.

The old upstream-style README, including ISO launch instructions and
runtime-translation descriptions, is preserved for provenance at
`research/history/native-port-docs-2026-11-18/README.md`. Those instructions
do not describe the accepted Armageddon route.

The work24/26 component checkpoint adds source-backed ordered task
lookup/retirement and an offline compiler that emits the original scalar
getter/setter as typed native C++. The generated output compiles and executes
against the actual native session without guest/compiler/device linkage.
Three current client identities are measured;38 earlier client binaries are
unchanged. A deliberately modified generated source fails before compilation.
The complete retail entry still refuses39 unimplemented native integration
names. Real task bodies/dispatch/reset, whole-game input/render/audio and
acceptance remain OPEN. No game or physical endpoint was opened.
The parent live status and dated work24/26 results own the full scope.
No owner launch is requested.
