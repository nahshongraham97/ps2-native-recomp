# Native-only rule for this fork

The binding rule is the parent project's
`../../../NATIVE_PORT_EXECUTION_CONTRACT.md`. Current status is
`../../../PORT_STATUS.md`; exact implementation order is
`../../../ROADMAP.md`. The old fork rule text is archived at
`research/history/native-port-docs-2026-11-18/NATIVE_PORT_ONLY.md` and does
not define current progress.

On the accepted retail boot/gameplay route: no interpreter or runtime code
generation for EE/VU0/VU1/IOP; no emulated CDVD/GS/SPU2/SIF/IPU device; no
fixed guest RAM/scratchpad/address map; no fallback. Retail logic compiled
to C++ may stay. All platform operations terminate in native host services.
Compatibility packets may be decoded as data, never handled by a simulated
PS2 device. Every game behavior must match the retail oracle on one clean,
pinned build. A zero census is not acceptance. Unknown behavior remains a
blocker, not a guessed response. The owner is not asked to launch until the
full parent contract passes.
