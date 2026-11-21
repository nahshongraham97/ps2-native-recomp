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

The work28 component checkpoint connects the compiled typed scalar getter
to the existing native selector component. Its affected CPU client passes,
and selector/generated getter/real session are linked without a guest
runtime, emulated device or offline compiler dependency. Source281930
directly loads this shared field; reusing the equivalent getter does not
establish an original call edge.138 current source pins and40 unchanged
prior binary identities are recorded. Work24/26 also supplies ordered
task lookup/retirement and generated-source build authentication.
The complete retail entry still refuses39 native integration names.
Actual task bodies/dispatch/reset, complete input/render/audio and full
game acceptance remain OPEN. No game or physical endpoints were opened.
Parent live status and dated work28 results own current scope.
No owner launch is requested.
