# Visible retail run: native GL direct window, 2026-11-13

## Target and setup

- Checkout: `external/forks/ps2-native-recomp/`, branch `codex/native-vfs-sif-overlay-20261110`, source HEAD `f039e25` before this uncommitted change set.
- Executable: `build_native_gl_full/ps2run.exe`.
- Retail ELF: `games/mkarmageddon/disc/SLUS_214.10`.
- Disc image: `games/mkarmageddon/disc.iso`.
- Memory card: `games/mkarmageddon/disc/armageddon.ps2`.
- Added run-only environment: `PS2_NATIVE_GL=1`, `PS2_EE_LIVE=1`, `PS2_EE_LIVE_BG=0`, `PS2_EE_LIVE_JOBS=2`, `MKA_INTERCEPT_COMBAT=1`, `PS2_KEYS=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\config\armageddon-keys.ini`.
- No artificial frame or wall-clock limit. Standard output and error are captured in `logs/`.

## Screen check prepared before launch

1. Observe the visible `ps2run` window and confirm it is the retail runner window.
2. Capture only that window's client area and inspect it alongside the runtime log's direct-context and crop/viewport records.

Acceptance requires an attached SDL-owned WGL context, a consumed retail GS primitive, a direct-present crop record with a centered destination rectangle, and visible game pixels in the same window. A source-level build or primitive counter alone does not prove this.

## Exit path

Leave the game running while its logs or frames progress. If the window has no scene/log progress after investigating the renderer path, close only this diagnostic run through its window's normal close action and record that it was manually closed; do not label that a natural crash.

## Result

**Not accepted: the user supplied a capture showing a completely black client area.** The screenshot is direct visible evidence; the source run did not produce a visible game image.

The process was responsive when checked (`Responding=True`, about 60 FPS, 3–8% CPU, roughly 63 MB working set), and its log continued to advance through `LGKBM` zero-event polling. This means the Windows process was not frozen; it does not establish game-state or rendering progress. The log stopped reporting EE translation after frame 1622, but that alone is not a guest-frame counter. The process was no longer present when a normal-close request was attempted, so its final exit code and who closed it are unknown; do not label this a crash or an agent-initiated close.

Same-run log evidence: SDL's visible WGL context attached to an AMD Radeon RX 6800 XT; the native GL path logged its first consumed primitive; and `present_frame` logged crop `x=0 y=0 w=640 h=224` into client rectangle `x=0 y=192 w=960 h=336` using nearest filtering. No packet rejection was logged. These markers prove neither that the scene FBO contained nonblack pixels nor that the default back buffer received them.

Source review of `runtime/src/gs/native_gl_backend.cpp:1019-1103` confirms `present_frame()` checks context/FBO/client-size and GS display-rectangle bounds, issues `glBlitFramebuffer`, and returns success without reading pixel data or checking GL errors. Qwen's first answer reached that central point but cited nonexistent relative line numbers and added an unsupported `prepare_texture()` explanation. One correction request was canceled after 11,000 reasoning events without a final answer; `lms ps --json` then reported `status: idle`. Local source verification is authoritative for this claim; no more Qwen prompt is planned for it.

Next diagnostic build adds an opt-in `PS2_NATIVE_GL_DIAGNOSTICS=1` check for eight presents. It counts nonblack pixels in the full scene target, selected GS crop, and output back buffer, records source bounds and GL errors, then restores the render target. The next run must distinguish empty/off-crop source content from a failed blit before changing aspect or centering math again.

## Launch

- Started: 2026-11-13T11:21:38.2650995-05:00
- PID: 29492
- Command: "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\build_native_gl_full\ps2run.exe" "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc\SLUS_214.10" --disc "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc.iso" --window --memcard "C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\games\mkarmageddon\disc\armageddon.ps2"

