# Historical selected-surface diagnostic record (not full movie verification)

- **Date:** 2026-11-14
- **Working Tree:** `C:\Users\nahsh\mk port\Armageddon-backup\armageddon-reimplementation\external\forks\ps2-native-recomp`
- **Git Commit:** `c87b585964ccc6e3cf5cec92d2561f8dbe49ee4a` (dirty)
- **Modified Files:**
  - `runtime/src/gs/native_gl_backend.cpp` (Target selection rule, write tick tracking, direct scene targets)
  - `runtime/src/gs/native_gl_backend.hpp` (Public interface declarations)
  - `runtime/src/gs/gs.cpp` (Surface clear on scene boundary, display write hooks)
  - `runtime/src/hw.cpp` (SetGsCrt boundary clear invocation)
  - `runtime/src/iop/sio2.cpp` (Diagnostic pad tracking)
  - `tests/test_gl_vs_raster.cpp` (8-cell texture sampling acceptance matrix)

---

## 1. Historically reported changes and unverified causal interpretation

### Plan B: "Newest Write Wins" Target Selection
- **Historical defect interpretation (not independently source/binary matched):** MKA's front end leaves `DISPFB` designated at `FBP 0` while rendering all front-end UI primitives into `FRAME_1` (`FBP 91`). Previously, Target 0 received 2 boot primitives at frame 0 and was considered "active" indefinitely, causing presentation to remain black.
- **Historical reported change (not current implementation proof):** Added `last_write_tick` tracking to `SceneTarget`. Every primitive batch drawn increments `draw_target->last_write_tick = ++g_scene_write_tick`. Every guest transfer via `draw_guest_rect` increments `target->last_write_tick = ++g_scene_write_tick`.
- **Historical proposed presentation rule:** `present_frame()` compares `active_target->last_write_tick` vs `display_target->last_write_tick`:
  - Historical target-selection hypothesis, front-end: Target 91 receives continuous primitive writes and is selected over idle Target 0 (`selected_target_fbp=91`).
  - Historical target-selection hypothesis, movie/double-buffering: Targets 0 and 140 receive movie quads and transfers and win over stale front-end targets.
  - Historical target-selection hypothesis: offscreen scratch targets (e.g. FBP 496) do not usurp presentation at frame boundaries.

### Plan C: Narrowed Surface Clearing (Ghost Elimination)
- **Historical defect interpretation (not independently source/binary matched):** Surface clearing on `display_psm != last_psm` risked clearing active surfaces mid-transition.
- **Historical reported change (not current implementation proof):** Restricted surface clearing strictly to hardware scene boundaries (`SetGsCrt` via `gs::on_scene_boundary()`).
- **Historically reported measurements (not reproduced from the currently linked files):**
  - GameSpy screen (frame 240): 0 green ghost pixels under the GameSpy logo (previously 737 ghost pixels from prior legal text).
  - Dialog margins (frame 480): 0 non-black pixels in top (y: 0..150) and bottom (y: 750..894) margins (previously residue leaked into both margins).

### Plan D: GS Texture Sampling Matrix (8-Cell Acceptance)
- Historically reported synthetic test output from `test_gl_vs_raster.exe`:
  1. Sprite 1:1 Scale (4x4 UV): 16 px, **0 mismatches (PASS)**
  2. Sprite 2× Magnification (8x8 UV): 64 px, **0 mismatches (PASS)**
  3. Sprite 3× Magnification (12x12 UV): 144 px, **0 mismatches (PASS)**
  4. Sprite 1.5× Non-Integer (6x6 UV): 36 px, **0 mismatches (PASS)**
  5. Sprite 2× Magnification (8x8 ST): 64 px, **0 mismatches (PASS)**
  6. Triangle Strip 1:1 (4x4 UV): 16 px, **0 mismatches (PASS)**
  7. Triangle Strip 2× (8x8 UV): 64 px, **0 mismatches (PASS)**
  8. Midpoint Tie-Break (0.5 texel center): 64 px, **0 mismatches (PASS)**
- Superseded mathematical claim: the prior universal `-0.49f` / `+0.01f` GS-equivalence explanation was not proved by eight synthetic cases. Current source uses conditional/path-specific adjustments: `native_gl_backend.cpp:2734-2746` defaults general texel offset 0, sprite shift X defaults -0.49 and shift Y 0 at 2799–2807, and triangle shift applies conditionally near2829–2834. Do not use this historical test set as a universal sampling proof.

---

## 2. Historical sample counts (conflict with currently linked frames)

Run log: [`target_test.log`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/movie-step4-verified-20261114/target_test.log)
Total dumped frames: 39 (sampled every 30 frames from frame 30 to 1170)

| Frame Range | Description | Non-Black Pixels | % of Output | Status |
|---|---|---|---|---|
| **Frames 90 – 360** | Legal screen & GameSpy logo | 505,220 / 1,144,320 | **44.2%** | Historically reported PASS (selected metric; full correctness unverified) |
| **Frame 390** | Scene boundary transition | 0 / 1,144,320 | **0.0%** | Historically reported PASS (black sample does not prove transition correctness) |
| **Frames 420 – 570** | Memory card prompt dialog | 117,988 / 1,144,320 | **10.3%** | Historically reported PASS (selected metric; current frames conflict) |
| **Frames 600 – 690** | Crimson background & transition | 1,058,564 / 1,144,320 | **92.5%** | Historically reported PASS (non-black pixels alone do not prove picture/movie correctness) |
| **Frames 720 – 1170** | MKA Title / Menu dialog prompt | 933,896 / 1,144,320 | **81.6%** | Historically reported PASS (non-black pixels alone do not prove picture/movie correctness) |

---

## 3. Proof Images & Artifacts

The listed historical diagnostic frames reside directly alongside this run record in [`frames/`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/movie-step4-verified-20261114/frames/):
- Frame 240 (GameSpy screen, ghost eliminated): [`frames/frame_240.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/movie-step4-verified-20261114/frames/frame_240.png)
- Frame 480 (Memory card dialog, clean margins): [`frames/frame_480.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/movie-step4-verified-20261114/frames/frame_480.png)
- Frame 600 (Full-screen crimson presentation): [`frames/frame_600.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/movie-step4-verified-20261114/frames/frame_600.png)
- Frame 720 (Post-prompt dialog presentation): [`frames/frame_720.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/movie-step4-verified-20261114/frames/frame_720.png)
- Frame 1140 (Stable rendered state): [`frames/frame_1140.png`](file:///C:/Users/nahsh/mk%20port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/diagnostics/movie-step4-verified-20261114/frames/frame_1140.png)

## Documentation audit: actual artifacts and contrary measurements

All five linked PNGs and the frames directory exist; the directory contains 39 PNG and 39 PPM files. The linked log line3 stamps **c87b585 dirty, 2026-11-14T18:03:27Z**; no immutable matching runner hash is supplied. Recounting pixels after converting the linked PNGs to RGB, with a pixel non-black if any channel is nonzero, gives: frame 240=505,220; frame 480=162,328; frame 600=162,328; frame 720=162,328; frame 1140=162,328; every image is 1280×894. The frame 240value matches the report; the other selected files do not substantiate the table’s 117,988 / 1,058,564 / 933,896 figures. Original table numbers are retained as historical reported values, not corrected silently. Recover the exact original measurement inputs/masks to reconcile them.

Non-black occupancy does not identify a decoded movie frame, validate sequence continuity, correct target selection, complete pixels, output timing or A/V sync. No full movie proof follows from these samples. The target-selection/clearing causality above remains the historical interpretation pending a matched source/binary comparison.
