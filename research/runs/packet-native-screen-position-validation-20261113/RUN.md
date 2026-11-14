# Screen-position replay — 2026-11-13

- Runner: C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp\build_native_gl_full\ps2run.exe
- Runner SHA-256: B70FECCAB9EB8D7EC90E694498F1D99B772761C81BCDBE61C05C2A832B2CA1B0
- PID: 40792
- Disposable memory-card copy: C:\Users\nahsh\AppData\Local\Temp\armageddon-screen-position-validation-20261113.ps2
- Source card SHA-256: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87
- Copy SHA-256 at launch: 628099505162BD62EB68D5C0CECBDAEAA16AC3CA935264239D892667E911FE87
- Input presses: 30:cross and 500:cross (6 frames each)
- Native GL source/output dump requested at present 800
- `logs/stdout.log` and `logs/stderr.log` were not produced. The runner writes its own timestamped log to `%LOCALAPPDATA%\Temp\logs`; the actual log is `C:\Users\nahsh\AppData\Local\Temp\logs\ps2run_2026-11-13_13-59-27.log` (4,206,828 bytes; SHA-256 `82CEEFC425FCE73825FD7A5B05B5E1AE714ED3B13EE8F8AB3C69D1CD91D4BCCB`).
- The user confirmed the screen-position adjustment screen is centered in Screenshot (121). The requested present-800 PPM was not present in this run directory, so this visual confirmation is from the user's screenshot rather than that diagnostic dump.
- The log records one video-stream request: `movieps2/LOGON.sfd`; MWFILEFR opened the extracted file (3,321,856 bytes). It also reports 216,704 IPU output qwords and zero IPU stalls. These counters show activity but do not prove a decoded frame reached presentation.
- Native-GL diagnostics contain 109 samples from present 2040 through present 8520 where `scene_nonblack=0`, `crop_nonblack=0`, and `output_nonblack=0`; the accepted-primitive count remained 1,029,684. This proves the direct GL presentation source was black throughout those samples, but does not establish whether the GS VRAM display surface held movie pixels.
- The log ends after 8,585 frames with `window closed`; it reports no crash. Who closed the window was not recorded.
- For live visibility in a future run, launch the game first, then in a separate PowerShell window run `& .\scripts\watch_ps2run_log.ps1`. It selects the newest runner log and follows appended lines. Use `-Once` to print the latest 80 lines without following.
