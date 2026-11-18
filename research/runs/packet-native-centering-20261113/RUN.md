# Packet-native centering capture — 2026-11-13

> Dated-run audit boundary (2026-11-18): All implementation/current/verified language here refers to the named historical source, build and scenario. Original counter values, outputs, observations and artifact IDs are preserved as reports, not independently revalidated facts. “Native” component names, successful builds/tests and old census verdicts do not establish host resource ownership, card/profile behavior, correct sound, physical input, movie A/V sync or whole-game acceptance. Read the canonical parent PORT_STATUS.md and execution contract for current blockers; unresolved historical causal/source claims remain unresolved.

- Checkout: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp
- HEAD: f039e2587337526fe809a4df85f17edf93d64a53
- Runner: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\build_native_gl_full\ps2run.exe
- Runner SHA-256: AE8C54740DD653B1A65CE47A7F696427611175DE33C7E5D14D77F36C823FEA08
- Started: 2026-11-13T12:48:35.3262328-05:00
- PID: 18424
- Flags: PS2_GS_PACKET_NATIVE_STRICT=1; PS2_NATIVE_GL_DIAGNOSTICS=1; PS2_NATIVE_GL_DIAGNOSTIC_EVERY=60; PS2_TRACE_DISPLAY=1; PS2_EE_LIVE=1; PS2_EE_LIVE_BG=0; PS2_EE_LIVE_JOBS=2; MKA_INTERCEPT_COMBAT=1
- PS2_KEYS: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\config\armageddon-keys.ini
- ISO: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc.iso
- Diagnostic memory-card copy: C:\Users\nahsh\AppData\Local\Temp\armageddon-centering-diagnostic-20261113.ps2
- Original/card-copy SHA-256 at launch: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87
- Logs: logs/stdout.log and logs/stderr.log

## Purpose

Compare gold-pixel horizontal bounds in the native GS source crop and the presented window backbuffer at every 60th successful present. This distinguishes source-frame displacement from host destination placement. The original card is not passed to the runner.

