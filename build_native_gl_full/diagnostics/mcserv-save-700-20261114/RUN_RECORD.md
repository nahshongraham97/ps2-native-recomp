# Historical scripted MCSERV diagnostic record (700 frames; no save acceptance)

- **Date:** 2026-11-14
- **Historically claimed commit (does not match linked log):** [`c34fd60`](https://github.com/nahshongraham97/ps2-native-recomp/commit/c34fd60)
- **Branch:** `codex/native-vfs-sif-overlay-20261110`
- **Target:** `games/mkarmageddon/disc/SLUS_214.10`
- **Executable:** [`build_native_gl_full/ps2run.exe`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/ps2run.exe)
- **Log:** [`press_test.log`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/mcserv-save-700-20261114/press_test.log)
- **Diagnostic Card:** [`armageddon.ps2`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/mcserv-save-700-20261114/armageddon.ps2)

---

## 1. Environment & Flags

```bat
set PS2_NATIVE_GL=1
set PS2_VU_AOT_STRICT=1
set PS2_EE_LIVE=1
set PS2_EE_LIVE_BG=0
set PS2_EE_LIVE_JOBS=2
set MKA_INTERCEPT_COMBAT=1
set PS2_KEYS=C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\config\armageddon-keys.ini

ps2run.exe games\mkarmageddon\disc\SLUS_214.10 --window --memcard armageddon.ps2 --press 500:cross:20 --frames 700
```

---

## 2. Empirical Execution Metrics

- **Total Frames Executed:** 700 frames
- **EE Cycles:** 3,441,632,579 cycles
- **GS Primitives Drawn:** 155,746 primitives
- **VIF1 Commands:** 758,817 commands
- **VIF0 Fallback:** 0 unhandled aborts
- **VU AOT Strict (historically reported):** 0 fallback aborts;490 registered programs is an inventory count, not proof that all 490 executed or all modes are covered
- **Reported audio telemetry (not listened-to output proof):** 11.19 s sound in 32.00 s wall time, 0 underruns, 0 samples dropped (running through `libsd_native.cpp` shim over `spu2.cpp`)
- **Unhandled RPC Stops:** 0

---

## 3. Subsystem Findings

### A. MCSERV SIF Card Inquiries
- `[iop-native] MCSERV host SIF replacement active at 0x80000400`
- `[iop-native] MCSERV XMCMAN init accepted with retail minimum versions 0x20A/0x20E`
- **Slot 1 (Port 0, Slot 0), historical handler report:** Handled by the named host handler by `mcserv.cpp:mc_get_info()`, reporting a formatted-card response with `status=0`, `free_clusters=8000`.
- **Slot 2 (Port 1, Slot 0), historical response:** The record reports `status=-13` (no card inserted).

### B. Screen Progression & State Reached
- The game polled `GetInfo2` across frames 200–700 while displaying the memory card check dialog.
- The scheduled input injection `--press 500:cross:20` occurred at frame 500, but within the 700-frame budget the game remained at the memory card prompt / polling loop.
- **Empirical Gate Result:** The run did **not** reach profile creation or invoke the newly linked file-level write handlers (`mc_open`, `mc_write`, `mc_close`).
- **Blocker Status:** The Memory Card save blocker remains **OPEN** until profile creation and filesystem writes are empirically observed in a longer interactive trace.

## Documentation audit: source/binary and artifact limitations

The linked `press_test.log`, diagnostic card and current `ps2run.exe` path exist. The log’s line3 stamps **c87b585 dirty, 2026-11-14T18:30:38Z**, contradicting this document’s c34fd60 commit attribution. No historical executable hash is pinned here; today’s mutable executable at that path cannot certify the earlier build. The diagnostic directory has no frames/PNG/PPM captures, so its screen-state interpretation is reported rather than independently visually verified. Scripted Cross input does not prove physical keyboard/controller delivery. A successful GetInfo response does not establish card insertion transitions, profile creation, file writes or save/reload; the original OPEN blocker is retained. The command block is historical, not a request to run it.
