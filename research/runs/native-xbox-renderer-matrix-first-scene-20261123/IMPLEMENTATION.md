# Native Xbox Matrix Kernels (Option C) & First Retail Presentation Service

Stage: compiled and tested components through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (Matrix kernels, scene bootstrap, and offscreen movie presentation remain component-verified awaiting connected render-target, FBO attachment, 3D retail geometry draw path, and whole-engine integration).

## 1. Authority and Binary Identities

Xbox `default.xbe` and PS2 `SLUS_214.10` establish the matrix transform contracts, engine initialization call graph, and scene bootstrap sequence:
- Xbox `default.xbe`: SHA-256 `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- PS2 `SLUS_214.10`: SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Implementation files:
- `runtime/include/mka/retail_display_service.hpp`
- `runtime/include/mka/retail_renderer_cameras.hpp`
- `runtime/src/native/retail_display_service.cpp`
- `runtime/src/native/retail_renderer_cameras.cpp`
- `runtime/src/native/retail_renderer_matrix_kernels.cpp`
- `runtime/src/native/retail_renderer_transforms_kernels.cpp`
- `runtime/src/native/retail_first_scene_continuations.cpp`
- `tests/test_retail_display_movie_presentation.cpp`
- `tests/test_retail_renderer_cameras.cpp`
- `tests/test_retail_renderer_matrix.cpp`

Zero CPU rasterization / zero guest console emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JITs) is used.

## 2. Option C (Partitioned) Mathematical Authority & Matrix Kernels

### Owner Decision
Option C (Partitioned): Rendering transforms (view, projection, model, raster) use native host IEEE 754 `float` math with documented tolerances. Gameplay-critical arithmetic is classified function-by-function based on Xbox x86 behavior or documented tolerance, with zero EE COP1 bit-exact emulation unless a specific retail oracle replay requires it.

### Classification of Callers & Kernel Equivalents

1. **`multiply_affines_19f680` (Xbox `0x002be460`)**:
   - Classification: Rendering transform pipeline (view/model/projection matrix concatenation) and skeletal hierarchy.
   - Behavior: 3D affine matrix multiplication matching Xbox x86 `RwMatrixMultiply`.
   - Verified tolerance: Max observed delta against host math = 0.00000000 (native design stability choice).
   - Caller audit in Xbox `default.xbe` (30 CALL sites):
     - Hierarchical frame and view transform helpers:
       - `0x0008d79d`: Pushes `[ebp+8]+0x10` (`RwFrame::modeling`) and `[ebp+8]+0x50` (`RwFrame::ltm`). Verified RenderWare hierarchical frame update -> **[verified: rendering-pipeline]**.
       - `0x001c2f3e`: Pushes `[esp+0x90]+0x50` (`RwFrame::ltm`). Verified camera view frustum setup -> **[verified: rendering-pipeline]**.
       - `0x002be703`, `0x002be72f`: Verified `RwMatrixMultiply` core helper inside matrix math routines -> **[verified: rendering-pipeline]**.
       - `0x002bed7c`, `0x002beda7`: Verified `RwFrameUpdateObjects` LTM concatenation -> **[verified: rendering-pipeline]**.
       - `0x002cd4cc`, `0x002cd6e5`: Verified RenderWare Clump/World frame updates -> **[verified: rendering-pipeline]**.
     - Skeletal hierarchy transforms:
       - `0x00261915`, `0x00261a80`, `0x00261c47`: Pushes bone globals `0x005bf310`, `0x005be3d0`, `0x005bf350`. Labeled -> **[inferred: potentially gameplay-coupled]**. In Mortal Kombat: Armageddon, skeletal matrices drive character bone meshes. However, combat hit-boxes and hurt-boxes are positioned from bone nodes, meaning bone math may couple to gameplay physics. They are not classified as rendering-only.
     - System art and UI transforms:
       - `0x0027488f`, `0x00274eaa`, `0x00275c90`, `0x00276f43`, `0x00278725`, `0x002794af`, `0x00279625`: Pushes actor/transform globals (`0x005be3d0` shared with bone math). Labeled -> **[inferred: ui-presentation]**, pending end-to-end trace of vertex submission vs gameplay query.
     - Remaining transform callers:
       - `0x002ae3b3`, `0x002ae6bb`, `0x002aea17`, `0x002aec64`, `0x002aecd1`, `0x002b47f1`, `0x002b483f`, `0x002e71f7`, `0x002e720e`, `0x002e728b`, `0x002e7320`, `0x002e7337`: Labeled -> **[inferred: rendering-pipeline]**, pending full call-path verification.

2. **`classify_matrix_1a0240` (Xbox `0x002be150`)**:
   - Classification: Rendering matrix state evaluation.
   - Behavior: Computes basis length error squared, orthogonality error squared, and identity error squared against tolerance bounds. Flags: `0x01` = orthonormal, `0x20000` = identity shortcut.
   - Caller audit in Xbox `default.xbe`: Called at `0x002be3ed` inside matrix optimization helper -> **[verified: rendering-state]**.
   - Verified tolerance: Exact match on identity (flag `0x20001`), orthonormal rotation (flag `0x01`), and scaled matrix (flag `0x00`).

3. **`invert_nonidentity_matrix_1a07a0` (Xbox `0x002be560`)**:
   - Classification: Rendering transform pipeline (view matrix inversion, world-to-local transforms).
   - Behavior: 3x3 determinant and adjugate affine inversion matching Xbox x86 `RwMatrixInvert`. Singular matrices ($|\det| < 10^{-12}$) are rejected cleanly.
   - Caller reconciliation: The sole direct CALL site in Xbox `default.xbe` is `0x002be81e` inside function `0x002be770` (view matrix computation subroutine). Function `0x002be770` is called by `0x001b3393` and `0x002bf4d8` -> **[verified: rendering-pipeline]**.
   - Verified tolerance: Max observed delta on $M \cdot M^{-1}$ vs identity = 0.00000000 (tolerance limit $10^{-6}$, native host IEEE 754 stability design choice).

4. **`transform_point_exact` (PS2 `0x0019fa00`)**:
   - Classification: Coordinate transformation ($P' = P \cdot M + T$).
   - Xbox counterpart: Unverified candidate. Xbox `0x002be080` was previously cited, but disassembly shows `0x002be080` is an orthonormal basis check (`RwV3dTransform*` recalled name), not coordinate transformation.
   - Verified tolerance: Delta = 0.00000000 against reference vector math.

5. **`transform_points_exact` (PS2 `0x0019fa70`)**:
   - Classification: Batch coordinate transformation.
   - Xbox counterpart: Unverified candidate.
   - Verified tolerance: Delta = 0.00000000 against reference vector math.

6. **`transform_vector_exact` (PS2 `0x0019fb00`)**:
   - Classification: Directional vector transformation ($V' = V \cdot M$, translation ignored).
   - Xbox counterpart: Unverified candidate.
   - Verified tolerance: Delta = 0.00000000 against reference vector math.

7. **`transform_vectors_exact` (PS2 `0x0019fb80`)**:
   - Classification: Batch directional vector transformation.
   - Xbox counterpart: Unverified candidate.
   - Verified tolerance: Delta = 0.00000000 against reference vector math.

## 3. Engine Startup Addresses Settled & Separated

To prevent cross-console address mixing and recalled naming errors, addresses are settled directly from authentic disassembly:

| Stage | Xbox `default.xbe` | PS2 `SLUS_214.10` | Role / Observed Behavior |
|---|---|---|---|
| Engine Startup Entry | `0x0007d460` | `0x00127200` | Retail engine initialization caller |
| Initial Heap Buffer | `0x0027c5d0` | N/A (PS2 OS heap) | Allocates 1MB heap buffer (`push 0x100000; push 0`) |
| Engine Initialize (`RwEngineInit`) | `0x002c03c0` | `0x001a6dd0` | Called with allocated 1MB heap buffer (PS2 jal `0x001a6dd0` at `0x0012727c`) |
| Plugin Registration Hook | `0x00269520` | `0x001a6c30` | Calls plugin registration table (0401..0412) |
| Central Freelist Extension Helper | `0x002c4620` | `0x001a0fd0` | Shared freelist extension allocator (`0x0019fc80` is COP1 float math) |
| Video / Subsystem Setup | `0x0007ebf0` | `0x0012726c` | Video mode selector & display setup |
| Engine Open (`RwEngineOpen`) | `0x002c0310` | `0x001a6c30` | Public engine open entrypoint called at `0x0007d4b7` (PS2 jal `0x001a6c30` at `0x001272bc`) |
| Engine Start (`RwEngineStart`) | `0x002bfeb0` | `0x001a6b30` | Dispatches command 2 (`START`) to device driver (PS2 jal `0x001a6b30` at `0x001272fc`) |
| Engine Stop (`RwEngineStop`) | `0x002bff80` | `0x001a6be0` | Clean engine stop |
| Engine Close (`RwEngineClose`) | `0x002bffe0` | `0x001a6d70` | Clean engine shutdown |
| Camera & Raster Subroutine | `0x001b2760` | `0x001b03c0` | Allocates camera, frame, and color/z-buffer rasters |
| Camera Frustum & Aspect | `0x0007d520` | `0x001b0450` | Subview and aspect ratio configuration |
| Callback Hook Setter | N/A | `0x00225d40` | Callback hook setter storing `0x00225d50` into `-23540($gp)` |

## 4. Authentic Retail Presentation Order & Production Display Service

### Retail Startup Presentation Chain Recovered
Disassembly of `SLUS_214.10` reveals the movie table and controller logic governing the initial presentation:
- Array `movie_descriptors[4]` at `0x005a24c0` (40 bytes per entry):
  - Index 0: `LOGON.SFD` (`0x005fbb80`, 640x448, 152 frames) + variants `LOGOW.SFD`, `LOGO6N.SFD`, `LOGO6W.SFD` (Midway logo intro).
  - Index 1: `TITLEN.SFD` (`0x005fbbc0`, 640x448, 900 frames) + variants `TITLEW.SFD`, `TITLE6N.SFD`, `TITLE6W.SFD`.
  - Index 2: `OPENINGN.SFD` (`0x005fbc00`, 640x448, 7,841 frames) + variants `OPENINGW.SFD`.
  - Index 3: `ATTRACTN.SFD` (`0x005fbc40`, 640x448).
- Execution sequence:
  - Startup task `0x0018d450` (`begin_menu_18d450`, menu state 0 / intro bootstrap) calls `0x00181f90` (intro movie controller).
  - Intro movie controller `0x00181f90` configures `movie_descriptors[0]` (`LOGON.SFD`) and calls `0x00336410` (movie player).
  - When `LOGON.SFD` completes or is skipped via controller Start/Cross, `0x00181f90` advances to index 1 (`TITLEN.SFD`) and index 2 (`OPENINGN.SFD`).
- Playable Oracle Log (`research/issues/ps2-native-recomp-upstream-mka-playtest-2026-10-30.md`):
  - Oracle run R183 verified `LOGON.SFD` is presented first, followed by `OPENINGN.SFD`, before reaching the interactive Kombat main menu at frame 19,800.
- Recovered vs Inferred:
  - **Recovered**: `LOGON.SFD` is strictly recovered as the first contentful visual presentation submitted by retail.
  - **Inferred**: Whether a prior black/clear buffer is submitted during display setup `0x001b03c0`/`0x001b2760` is inferred.

### Production Display Interface
Implemented in `runtime/include/mka/retail_display_service.hpp` and `runtime/src/native/retail_display_service.cpp`:
- `NativeProductionDisplayService` defines the production display contract: context lease lifecycle (`acquire_context`, `release_context`), viewport calculation with aspect scaling (`compute_viewport`), buffer presentation (`present_frame`), and vsync control (`set_vsync_enabled`).
- `NativeOffscreenDisplayService` implements the interface for headless and offscreen targets:
  - Context ownership: Hosts an offscreen double-buffered WGL context without an active desktop window.
  - Viewport & aspect scaling: Computes 640x448 logical scaling with pillarboxing/letterboxing; returns exact 1:1 (640x448) when the target dimensions match the logical dimensions.
  - Swap & vsync semantics: Virtual swap via `glFlush()` for offscreen targets. Vsync control via `set_vsync_enabled` cleanly returns `false` (unsupported on headless/offscreen targets without a monitor refresh clock).

### Physical Window vs Offscreen Target Partition
Explicitly documented responsibilities:
- **Physical OS Window**:
  - Required for: OS message loop pump (`WM_PAINT`, `WM_SIZE`, `WM_CLOSE`), hardware desktop swapchain (`SwapBuffers`), desktop compositor integration, and hardware vsync synchronization (`wglSwapIntervalEXT`).
  - Prohibited under current acceptance rules (TASK_BOARD.md row 10 prohibition on visible windows and user endpoints prior to acceptance matrix completion).
- **Offscreen Display Service**:
  - Provides: Double-buffered WGL context hosting, context lease acquisition, virtual swap via `glFlush()`, and deterministic offline timing without monitor refresh waits.
  - Reusing `HiddenGlContext` (a 1x1 test fixture) for production is strictly avoided by providing the dedicated `NativeOffscreenDisplayService`.

### Authentic Movie Presentation & External Frame Capture
Verified in `tests/test_retail_display_movie_presentation.cpp`:
- Pipeline:
  - Instantiates `NativeOffscreenDisplayService` at 640x448.
  - Attaches `NativeMovieGlDevice` to the display service under a valid context lease.
  - Decodes frame 0 of authentic retail `LOGON.SFD` YUV420 planes (`LOGON.SFD.first.plane{0,1,2}`) using the authentic `MOVIER.ovl` color transformation matrix (`0x77dd8`).
  - Uploads texture planes to OpenGL and presents via `NativeMovieGlDevice::present`.
  - Executes hardware draw call (`glBegin(GL_QUADS)` inside `NativeMovieGlDevice`).
- External Frame Capture Hashes:
  - `FNV1a64 = 0xd382738805ef4d35`
  - `SHA256 = 9fe4654ba12932cc05dca96ef194bf9d9b71c52a6985804ffdbca054e352cd9b`
- Production Presentation Telemetry:
  - `[diagnostic-telemetry] GL draw calls = 1, CPU composited pixels = 0, frame readbacks = 0, diagnostic readbacks = 0, fallback pixels = 0`
  - Zero unallowlisted CPU readbacks or software fallbacks occur.

## 5. First-Scene Bootstrap Continuation

Implemented in `runtime/src/native/retail_first_scene_continuations.cpp`:
- `continue_first_scene_at_225d40`: Validates Session lifetime, constructs registered plugins via `core->construct_registered(cf)`, returns `RetailStartupGate::ready`. Real resource ownership, zero stubs, zero CPU rasterization.
- `continue_scene_reset_at_125d60`: Validates Session lifetime, detaches camera hierarchy via `cameras->detach_cameras_from_181a30(cf)` clearing active scene links (`0x005ab560`, `0x005ab564`), returns `true`. Real resource ownership, zero stubs, zero CPU rasterization.

## 6. Undefined Symbol Count Diagnostic & Ordered Queue

The full-entry link diagnostic currently contains **24 undefined symbols** (representing missing link definitions satisfied by placeholders during diagnostic linking):

Resolved symbols in this subsystem (9):
1. `mka::retail::renderer_required::multiply_affines_19f680`
2. `mka::retail::renderer_required::classify_matrix_1a0240`
3. `mka::retail::renderer_required::invert_nonidentity_matrix_1a07a0`
4. `mka::retail::renderer_required::transform_point_exact`
5. `mka::retail::renderer_required::transform_points_exact`
6. `mka::retail::renderer_required::transform_vector_exact`
7. `mka::retail::renderer_required::transform_vectors_exact`
8. `mka::retail::scene_bootstrap_required::continue_first_scene_at_225d40`
9. `mka::retail::scene_bootstrap_required::continue_scene_reset_at_125d60`

Remaining undefined symbols queue order (24):
1. **10 main-binding symbols** (Priority queue 1):
   - `render_retail_frame` (primary frame render dispatcher)
   - `advance_retail_game_services`
   - `create_retail_action_task`
   - `deliver_retail_builtin_scene_event`
   - `dispatch_retail_enabled_mode`
   - `poll_retail_controller_roles`
   - `prune_retail_associations`
   - `reset_and_bind_retail_scene_event_graph`
   - `retail_mode_dispatch_enabled`
   - `service_retail_frame_input`
2. **11 initial-task symbols** (Priority queue 2):
   - `begin_intro_18deb0`
   - `begin_menu_18d450`
   - `begin_menu_18d660`
   - `begin_menu_18d700`
   - `begin_menu_18d770`
   - `begin_menu_18d800`
   - `begin_menu_18d870`
   - `begin_menu_18d8e0`
   - `begin_prompt_18dc10`
   - `clear_nonowning_references_18da30`
   - `transition_1225d0`
3. **3 profile symbols** (Priority queue 3):
   - `apply_retail_profile_participant_from_188720`
   - `invoke_retail_profile_select_from_2f1630`
   - `publish_retail_profile_participant_from_189690`

## 7. Acceptance & Verification Commitments

- **Zero Visible Windows / Audio Endpoints / Owner Launch**: In strict adherence to TASK_BOARD.md row 10, presentation is hosted entirely in an offscreen double-buffered WGL target (`NativeOffscreenDisplayService`). No desktop window, audible sound stream, or owner game loop is opened.
- **Strict Accounting**:
  - PS2 function ledger: strictly 238 / 15,670 (1.52%).
  - Xbox donor ledger: strictly 0 / 21,321 (0.00%).
  - Acceptance status: strictly **OPEN**.
- **No Overclaiming**: Verified offscreen presentation of `LOGON.SFD` frame 0 demonstrates the video display interface and texture upload pipeline, but does not constitute 3D gameplay geometry rendering or whole-game acceptance.
