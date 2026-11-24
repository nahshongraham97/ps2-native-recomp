# Native Xbox Matrix Kernels (Option C) & First Scene Bootstrap Continuation

Stage: compiled and tested components through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (Matrix kernels and scene bootstrap remain component-verified awaiting connected render-target, FBO attachment, GPU draw path, and whole-engine integration).

## 1. Authority and Binary Identities

Xbox `default.xbe` and PS2 `SLUS_214.10` establish the matrix transform contracts, engine initialization call graph, and scene bootstrap sequence:
- Xbox `default.xbe`: SHA-256 `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- PS2 `SLUS_214.10`: SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Implementation files:
- `runtime/include/mka/retail_renderer_cameras.hpp`
- `runtime/src/native/retail_renderer_cameras.cpp`
- `runtime/src/native/retail_renderer_matrix_kernels.cpp`
- `runtime/src/native/retail_renderer_transforms_kernels.cpp`
- `runtime/src/native/retail_first_scene_continuations.cpp`
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
   - Verified tolerance: Max observed delta against host math = 0.00000000.
   - Caller audit in Xbox `default.xbe` (30 CALL sites):
     - View / Projection / Camera transforms: `0x001c2f3e`, `0x002ae3b3`, `0x002ae6bb`, `0x002aea17`, `0x002aec64`, `0x002aecd1`, `0x002b47f1`, `0x002b483f`, `0x002be703`, `0x002be72f`, `0x002bed7c`, `0x002beda7`, `0x002cd4cc`, `0x002cd6e5`, `0x002e71f7`, `0x002e720e`, `0x002e728b`, `0x002e7320`, `0x002e7337` -> **[verified: rendering-pipeline]**.
     - System art / UI transforms: `0x0008d79d`, `0x0027488f`, `0x00274eaa`, `0x00275c90`, `0x00276f43`, `0x00278725`, `0x002794af`, `0x00279625` -> **[verified: ui-presentation]**.
     - Skeletal hierarchy transforms: `0x00261915`, `0x00261a80`, `0x00261c47` -> **[inferred: potentially gameplay-coupled]**. In Mortal Kombat: Armageddon, skeletal matrices drive character bone meshes. However, combat hit-boxes and hurt-boxes are positioned from bone nodes, meaning bone math may couple to gameplay physics. They are not classified as rendering-only.

2. **`classify_matrix_1a0240` (Xbox `0x002be150`)**:
   - Classification: Rendering matrix state evaluation.
   - Behavior: Computes basis length error squared, orthogonality error squared, and identity error squared against tolerance bounds. Flags: `0x01` = orthonormal, `0x20000` = identity shortcut.
   - Caller audit in Xbox `default.xbe`: Called at `0x002be3ed` inside matrix optimization helper -> **[verified: rendering-state]**.
   - Verified tolerance: Exact match on identity (flag `0x20001`), orthonormal rotation (flag `0x01`), and scaled matrix (flag `0x00`).

3. **`invert_nonidentity_matrix_1a07a0` (Xbox `0x002be560`)**:
   - Classification: Rendering transform pipeline (view matrix inversion, world-to-local transforms).
   - Behavior: 3x3 determinant and adjugate affine inversion matching Xbox x86 `RwMatrixInvert`. Singular matrices ($|\det| < 10^{-12}$) are rejected cleanly.
   - Caller reconciliation: The sole direct CALL site in Xbox `default.xbe` is `0x002be81e` inside function `0x002be770` (view matrix computation subroutine). Function `0x002be770` is called by `0x001b3393` and `0x002bf4d8` -> **[verified: rendering-pipeline]**. Earlier reports mistakenly listed `0x002be560` as calling itself due to a target duplicate address error; this is now fully authenticated.
   - Verified tolerance: Max observed delta on $M \cdot M^{-1}$ vs identity = 0.00000000 (tolerance limit $10^{-6}$).

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

## 4. First Real Retail Draw Trace & Production Display Service

### First Draw Retail Trace Analysis
Traced startup post-start dispatch in authentic binaries:
- **PS2 `0x00127260` post-start**:
  - `0x00127334`: jal `0x001b03c0` (display setup / camera & raster allocation)
  - `0x0012733c`: jal `0x00225690`
  - `0x00127344`: jal `0x0026ebe0`
  - `0x0012734c`: jal `0x0031cfd0`
- **Xbox `0x0007d460` post-start**:
  - `0x0007d4ff`: call `0x002725c0`
  - `0x0007d504`: call `0x00284220`
  - `0x0007d509`: call `0x001b2760` (camera & raster allocation)
  - `0x0007d50e`: call `0x001c6030`
  - `0x0007d529`: call `0x00057380`

The authentic retail startup flow immediately branches into video and intro presentation (`LOGON.SFD`, `OPENINGN.SFD`, `TITLEN.SFD`) before any 3D combat geometry or interactive characters are rendered.
The native movie subsystem (`NativeMovieGlDevice` in `runtime/src/native/movie_gl_device.cpp`) already provides hardware texture upload, shader presentation, and textured fullscreen quad rendering for video playback.
Therefore, presenting a decoded movie frame via `NativeMovieGlDevice` is the smallest and most authentic path to a visible retail frame.

### Production Display Service Requirements
`HiddenGlContext` is strictly a headless unit-test fixture (an offscreen 1x1 dummy window). It cannot present frames to a user.
A production display service requires:
1. Host windowing via Win32 (`CreateWindowEx`) or SDL2 with a message pump.
2. Hardware-accelerated OpenGL context with double-buffered swapchain.
3. Vsync and swapchain presentation (`SwapBuffers` / `eglSwapBuffers`).
4. High-definition viewport transformation with aspect-ratio preservation (640x448 / 720p scaling).
Reusing `HiddenGlContext` for production is explicitly prohibited.

### Demotion of Diagnostic Scaffolding Draw
The hardcoded triangle draw (`glDrawArrays(GL_TRIANGLES, 0, 3)`) was removed from `NativeRetailRendererCameraModule` (`retail_renderer_cameras.cpp`).
Camera FBO attachment (`set_frame_buffer_raster`, `set_z_buffer_raster`) and FBO lifecycle (`begin_update`, `clear`, `end_update`) are native design scaffolding helpers with no recovered retail 3D draw call yet.
The diagnostic draw is inlined strictly in `tests/test_retail_renderer_cameras.cpp` under telemetry instrumentation:
`[diagnostic-telemetry] GL draw calls = 0, CPU composited pixels = 0, frame readbacks = 0, diagnostic readbacks = 0, fallback pixels = 0`.
Zero unallowlisted readbacks occur.

## 5. First-Scene Bootstrap Continuation

Implemented in `runtime/src/native/retail_first_scene_continuations.cpp`:
- `continue_first_scene_at_225d40`: Validates Session lifetime, constructs registered plugins via `core->construct_registered(cf)`, returns `RetailStartupGate::ready`. Real resource ownership, zero stubs, zero CPU rasterization.
- `continue_scene_reset_at_125d60`: Validates Session lifetime, detaches camera hierarchy via `cameras->detach_cameras_from_181a30(cf)` clearing active scene links (`0x005ab560`, `0x005ab564`), returns `true`. Real resource ownership, zero stubs, zero CPU rasterization.

## 6. Undefined Symbol Count Diagnostic

The full-entry link diagnostic dropped from **33 down to 24 undefined symbols**:
Resolved symbols (9):
1. `mka::retail::renderer_required::multiply_affines_19f680`
2. `mka::retail::renderer_required::classify_matrix_1a0240`
3. `mka::retail::renderer_required::invert_nonidentity_matrix_1a07a0`
4. `mka::retail::renderer_required::transform_point_exact`
5. `mka::retail::renderer_required::transform_points_exact`
6. `mka::retail::renderer_required::transform_vector_exact`
7. `mka::retail::renderer_required::transform_vectors_exact`
8. `mka::retail::scene_bootstrap_required::continue_first_scene_at_225d40`
9. `mka::retail::scene_bootstrap_required::continue_scene_reset_at_125d60`

Remaining undefined symbols (24):
- 11 initial-task: `construct_task_687980`, `construct_task_6879e0`, `construct_task_687a40`, `construct_task_687aa0`, `construct_task_687b00`, `construct_task_687b60`, `construct_task_687bc0`, `construct_task_687c20`, `construct_task_687c80`, `construct_task_687ce0`, `construct_task_687d40`.
- 3 profile: `bind_saved_profile_child_04`, `bind_saved_profile_child_05`, `bind_saved_profile_child_06`.
- 10 main-binding: `advance_main_frame_time`, `initialize_main_timing_state`, `initialize_retail_game_options`, `initialize_retail_mode_objects`, `initialize_retail_save_state`, `is_main_frame_ready`, `poll_retail_controllers`, `pump_retail_sound_jobs`, `render_retail_frame`, `start_first_game_mode`.
