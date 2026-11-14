# Native GL present-pixel diagnostic run — 2026-11-13

- Active checkout: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp
- Branch: codex/native-vfs-sif-overlay-20261110
- Source HEAD before diagnostic instrumentation: f039e25
- Executable: build_native_gl_full/ps2run.exe (rebuilt successfully after instrumentation)
- Added run-only environment: PS2_NATIVE_GL=1, PS2_NATIVE_GL_DIAGNOSTICS=1, PS2_EE_LIVE=1, PS2_EE_LIVE_BG=0, PS2_EE_LIVE_JOBS=2, MKA_INTERCEPT_COMBAT=1, and PS2_KEYS=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\config\armageddon-keys.ini.
- The original memory card was copied to C:\Users\nahsh\AppData\Local\Temp\armageddon-native-gl-diag-20261113.ps2 before launch; diagnostic-copy SHA-256: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87. The original card path is not passed to the runner.
- Acceptance: eight direct-present diagnostics must distinguish whole-scene pixels, selected crop pixels, back-buffer pixels, and GL errors. Visible game pixels remain a separate acceptance check.
- No synthetic frame or wall-clock limit. Do not launch a duplicate while PID is active.
- Logs: logs/stdout.log and logs/stderr.log.

## Result

Completed after the black-frame checks; details are recorded below.

## Launch

- Started: 2026-11-13T11:44:41.0939474-05:00
- PID: 12140
- Command: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\build_native_gl_full\ps2run.exe "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc\SLUS_214.10" --disc "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc.iso" --window --memcard "C:\Users\nahsh\AppData\Local\Temp\armageddon-native-gl-diag-20261113.ps2"

## Observed result

- The user-visible capture for the previous direct-present run was a fully black client area. This new run reproduced black in the native OpenGL path.
- All eight enabled present probes reported scene_nonblack=0, crop_nonblack=0, backbuffer_nonblack=0, and gl_errors=0000/0000/0000; accepted_primitives=2 on every sample. GS registers were PMODE 0x66, SMODE2 0x2, DISPFB 0x1400, DISPLAY 0x000DF9FF0181927C, crop 0,0 640x224.
- Route-selection correction: this invocation set `PS2_NATIVE_GL=1` but did not set `PS2_GS_PACKET_NATIVE` or `PS2_GS_PACKET_NATIVE_STRICT`. It therefore used the compatibility `GsSink` in `gif::render`; it did not exercise `NativeRendererSink`. It still tested the native OpenGL backend reached from `gs::kick`, but it is not evidence for the direct GIF-event renderer path.
- The window remained responsive (about 23-24 FPS, ~93-97% CPU, ~232 MB RAM) while the guest-side LGKBM method-2 poll count reached 1,089,000 with every reply reporting zero events. EE live cache loaded at frame 0; the observed early file-read activity completed, and no additional scene primitives were accepted in the eight sampled presents. This is consistent with a guest busy-poll on the intentionally inert keyboard service, but does not prove that it is the only render blocker.
- Source: runtime/src/iop/sif.cpp:1203-1213 zeroes the 0x1D0-byte event reply, sets only reply[0]=1, leaves the event count at +2 zero, and returns true. It does not forward SDL/host keyboard events.
- Stagnant black diagnostic closed with a normal window-close request (CloseRequested=True); PID 12140 exited. This is a manual diagnostic closure, not a crash. Exit code was not captured.
- Original memory-card SHA-256 after the run: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87; temporary diagnostic copy SHA-256: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87. The runner used only the temporary copy; the original was not passed to the process.

## Counter interpretation (source-checked)

The final `[gs] 3775124 primitives drawn` line is not proof of 3,775,124 native OpenGL submissions. In `runtime/src/gs/gs.cpp`, `trace::draw()` can reject a primitive for the active draw filter, while `++g_primitives` at line 755 still executes outside that conditional whenever the GS kick's `draw` argument is true. The run's `[native-gl-diag] accepted_primitives=2` is a separate counter. The defensible result is that all eight sampled GL scene/crop/back-buffer reads were black and the GL backend had accepted two primitives by those presents; the total count alone does not explain the gap. The next investigation must trace the draw-filter values and native GL submission/context path in the same run before attributing the black frame.

## Next implementation investigation

Run the same retail command with `PS2_GS_PACKET_NATIVE_STRICT=1` so the packet route is selected and cannot silently fall back to `GsSink`; inspect the first native-renderer stop or pixel result. Separately trace the exact LGKBM method-2 request/reply contract before implementing keyboard forwarding. Do not change aspect/crop math based on this evidence.
