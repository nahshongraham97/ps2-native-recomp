# Historical scene-clear diagnostic record (scope and artifact discrepancies)

- **Date:** 2026-11-14
- **Base Commit:** `c87b585964ccc6e3cf5cec92d2561f8dbe49ee4a`
- **Dirty Tree Status:**
  - `runtime/src/gs/native_gl_backend.cpp` (multi-target management `kMaxSceneTargets = 4`, `clear_surfaces()`, boundary clear hook)
  - `runtime/src/gs/native_gl_backend.hpp` (`on_scene_boundary()`, target management)
  - `runtime/src/gs/gs.cpp` (`SetGsCrt` invokes `on_scene_boundary()`)
  - `runtime/src/iop/sio2.cpp` (pressure sensitivity fix for DS2 cross/start buttons)
  - `tests/test_gl_vs_raster.cpp` (pixel-exact A/B test harness)
- **Binary:** `build_native_gl_full/ps2run.exe`
- **Game Executable:** `games/mkarmageddon/disc/SLUS_214.10`
- **Run Command:**
  ```cmd
  set PS2_NATIVE_GL=1
  set PS2_VU_AOT_STRICT=1
  set PS2_EE_LIVE=1
  set PS2_EE_LIVE_BG=0
  set PS2_EE_LIVE_JOBS=2
  set MKA_INTERCEPT_COMBAT=1
  set PS2_KEYS=config/armageddon-keys.ini
  set PS2_NATIVE_GL_DIAGNOSTIC_DUMP_EVERY=30
  set PS2_NATIVE_GL_DIAGNOSTIC_DUMP_PREFIX=build_native_gl_full\diagnostics\scene-clear-step3-20261114\frames\frame_present_
  build_native_gl_full\ps2run.exe games\mkarmageddon\disc\SLUS_214.10 --window --memcard build_native_gl_full\diagnostics\scene-clear-step3-20261114\armageddon.ps2 --quick-dir build_native_gl_full\diagnostics\scene-clear-step3-20261114 --frames 720
  ```

## Historically cited diagnostic artifacts (presence checked below)
1. **Console Log:**
   - `build_native_gl_full/diagnostics/scene-clear-step3-20261114/target_test.log`
   - `build_native_gl_full/diagnostics/scene-clear-step3-20261114/logs/ps2run_2026-11-14_11-42-08.log`
2. **Raw Frame Dumps (PPM):**
   - Frames 30 through 720 dumped every 30 frames in `build_native_gl_full/diagnostics/scene-clear-step3-20261114/frames/`
3. **PNG Proof Images:**
   - [`new_frame_240.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/scene-clear-step3-20261114/new_frame_240.png): Legal screen with GameSpy / Sofdec / MK Dragon logos. Historically reported ghost-text count under GameSpy logo: **0** (not reproduced by the stated RGB rule; see audit below) (was 737 non-black pixels in previous runs).
   - [`new_memcard_720.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/scene-clear-step3-20261114/new_memcard_720.png): "NO MEMORY CARD (PS2) IS INSERTED" dialog box. Historically reported left margin non-black pixels: **0**, right margin: **0** (neither is reproduced from the linked PNG; see audit below) (was 52,184 ghost legal text pixels in previous runs).
   - [`memcard_dialog_crop.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/scene-clear-step3-20261114/memcard_dialog_crop.png): Historical dialog crop; the prior clean/sharp description is a visual interpretation, not a matched fidelity measurement.
   - [`green_mask.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/scene-clear-step3-20261114/green_mask.png): Historically described binary mask; its current nonzero pixel content does not substantiate a zero-green result.
   - [`crop_new.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/scene-clear-step3-20261114/crop_new.png) & [`crop_user.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/scene-clear-step3-20261114/crop_user.png): Historical comparative crops; elimination of accumulation is not established by file presence or a non-black count.

## Empirical Measurements
- GameSpy logo region (x: 40-160, y: 150-320):
  - Green ghost pixels: **0** historically reported (Threshold: G > 60 and G > R*1.4 and G > B*1.4; contrary recount below)
- Memory card screen margins (x: 0-140 and 1140-1280, y: 0-894):
  - Non-black pixels: **0** historically reported (complete elimination not substantiated by the currently linked images)
- Historically reported audio telemetry:0 queue underruns. The inference “sound continuous” is withdrawn: queue counters do not establish audible content, identity, continuity, pitch or synchronization.

## Documentation audit: exact availability and measurement limitations

The linked PNGs, frame directory and `target_test.log` exist. The explicitly named `logs/ps2run_2026-11-14_11-42-08.log` is absent. `target_test.log` instead identifies `11-57-06.log`, stamps **c87b585 dirty, 2026-11-14T16:54:45Z** at line3 and says 1200 frames at line14031, whereas the historical command requests 720 frames. These artifacts do not establish one matched run/binary. No historical runner hash is pinned; the current mutable executable path cannot provide that identity.

Independent RGB recount of `new_frame_240.png` for the exact half-open ROI x[40,160),y[150,320) with the stated threshold G>60,G>1.4R,G>1.4B gives **5,364 qualifying pixels**, rather than the reported 0. This test counts green pixels, not semantically identified ghost text; recover the original exclusion/mask procedure before claiming ghost removal. The current `green_mask.png` has 4,524 nonzero RGB pixels. For `new_memcard_720.png`, half-open left margin x[0,140),y[0,894) has 3,276 non-black pixels; right margin x[1140,1280) has 1,872. Thus the published zero-margin figures are not reproduced from these linked files. Original reported values and images are retained unchanged; do not use them as currently substantiated zero-ghost/zero-margin evidence.

The linked log reports 19.52 seconds of audio delivered over 58.39 seconds wall time and stretching statistics. Those are queue/device telemetry, not a listened-to audio capture or correct sound proof. The command block is historical and does not authorize a launch.
