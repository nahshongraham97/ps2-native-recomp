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

The source-ordered native entry remains incomplete. Keyboard-feed integration07
passes thirteen native CPU/file checks against47 source/tool pins. The real
platform owns and feeds the keyboard event queue; two obsolete USB module
registration calls and their five-word record are removed. The complete entry
still refuses49 native integration names; this count is not missing retail
algorithms or a full work estimate. Real scene keyboard delivery, participant
assignment and complete startup/render/audio/game behavior remain open.
Parent live status owns current evidence. No owner launch is requested.
