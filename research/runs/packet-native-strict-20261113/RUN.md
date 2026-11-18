# Packet-native strict GS run — 2026-11-13

> Dated-run audit boundary (2026-11-18): All implementation/current/verified language here refers to the named historical source, build and scenario. Original counter values, outputs, observations and artifact IDs are preserved as reports, not independently revalidated facts. “Native” component names, successful builds/tests and old census verdicts do not establish host resource ownership, card/profile behavior, correct sound, physical input, movie A/V sync or whole-game acceptance. Read the canonical parent PORT_STATUS.md and execution contract for current blockers; unresolved historical causal/source claims remain unresolved.

- Checkout: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp
- Runner: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\build_native_gl_full\ps2run.exe
- Started: 2026-11-13T12:08:11.7365324-05:00
- PID: 32564
- Route flags: PS2_GS_PACKET_NATIVE_STRICT=1; PS2_NATIVE_GL_DIAGNOSTICS=1; PS2_EE_LIVE=1; PS2_EE_LIVE_BG=0; PS2_EE_LIVE_JOBS=2; MKA_INTERCEPT_COMBAT=1
- Keys path used: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\config\armageddon-keys.ini (incorrect; the intended file is C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\config\armageddon-keys.ini, so this run does not verify the configured key mapping)
- Command: "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc\SLUS_214.10" --disc "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc.iso" --window --memcard "C:\Users\nahsh\AppData\Local\Temp\armageddon-packet-native-strict-20261113.ps2"
- Original card SHA-256: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87
- Diagnostic card copy SHA-256: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87
- Logs: logs/stdout.log and logs/stderr.log

## Observed result

- The run logged `[native-renderer] consuming GIF packet events before the compatibility GsSink`; `PS2_GS_PACKET_NATIVE_STRICT=1` therefore selected the direct GIF packet route. No `[native-renderer-stop]` or `[native-gl] rejected GS packet` line was recorded.
- The first eight present probes were all black (`scene_nonblack=0`, `crop_nonblack=0`, `backbuffer_nonblack=0`, GL errors `0000/0000/0000`) and counted two accepted GL primitives. These probes cover only the early startup presents, not the later screen.
- Later in this same run, the user-provided screenshots showed the real GameSpy/legal boot screen followed by the retail profile/memory-card caution prompt. The prompt text is visibly smeared/doubled and hard to read; no main menu or combat was shown. This is evidence that the packet-native route produced later retail boot imagery, with a significant fidelity defect.
- The final `[gs]` counter was 221,035. In the direct packet path, `native_renderer.cpp` increments this through `note_native_primitive()` only when `native_gl::submit()` returns true; it is a later accepted-primitive count, unlike the compatibility path's unconditional kick counter.
- The runner stopped at frame 1,080 with `window closed` after a normal `CloseMainWindow()` request. This was a manual diagnostic closure, not a crash. Exit code was not captured.
- The original memory card and diagnostic copy both had SHA-256 `628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87` after the run; only the copy was passed to the runner.

## Next investigation

Capture pixels after the early startup presents (at the profile/caution screen) and trace the texture/scanout path to isolate the smeared text. The presentation code centers the selected crop and blits with `GL_NEAREST`; do not assume the destination scaling is the cause without inspecting those later source pixels. Repeat with the correct `PS2_KEYS` path before testing interactive input. This run does not establish main-menu access, gameplay, save behavior, or zero emulation.
