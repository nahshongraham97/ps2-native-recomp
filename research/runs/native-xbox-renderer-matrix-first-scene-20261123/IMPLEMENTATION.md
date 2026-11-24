# Native Xbox Matrix Kernels (Option C) & First Scene Bootstrap Continuation

Stage: compiled and tested components through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (Matrix kernels and scene bootstrap remain component-verified awaiting connected render-target, FBO attachment, GPU draw path, and whole-engine integration).

## 1. Authority and Binary Identities

Xbox `default.xbe` and PS2 `SLUS_214.10` establish the matrix transform contracts, engine initialization call graph, and scene bootstrap sequence:
- Xbox `default.xbe`: SHA-256 `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- PS2 `SLUS_214.10`: SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Implementation files:
- `runtime/src/native/retail_renderer_matrix.cpp`
- `runtime/src/native/retail_renderer_transforms.cpp`
- `runtime/src/native/retail_first_scene_core.cpp`
- `tests/test_retail_renderer_matrix.cpp`

Zero CPU rasterization / zero guest console emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JITs) is used.

## 2. Option C (Partitioned) Mathematical Authority & Matrix Kernels

### Owner Decision
Option C (Partitioned): Rendering transforms (view, projection, model, raster) use native host IEEE 754 `float` math with documented tolerances. Gameplay-critical arithmetic is classified function-by-function based on Xbox x86 behavior or documented tolerance, with zero EE COP1 bit-exact emulation unless a specific retail oracle replay requires it.

### Classification of Callers & Kernel Equivalents
1. **`multiply_affines_19f680` (Xbox `0x002be460`)**:
   - Classification: Rendering transform pipeline (view/model/projection matrix concatenation).
   - Behavior: 3D affine matrix multiplication matching Xbox x86 `RwMatrixMultiply`.
   - Verified tolerance: Max observed delta against host math = 0.00000000.
2. **`classify_matrix_1a0240` (Xbox `0x002be150`)**:
   - Classification: Rendering matrix state evaluation.
   - Behavior: Computes basis length error squared, orthogonality error squared, and identity error squared against tolerance bounds. Flags: `0x01` = orthonormal, `0x20000` = identity shortcut.
   - Verified tolerance: Exact match on identity (flag `0x20001`), orthonormal rotation (flag `0x01`), and scaled matrix (flag `0x00`).
3. **`invert_nonidentity_matrix_1a07a0` (Xbox `0x002be560`)**:
   - Classification: Rendering transform pipeline (view matrix inversion, world-to-local transforms).
   - Behavior: 3x3 determinant and adjugate affine inversion matching Xbox x86 `RwMatrixInvert`. Singular matrices ($|\det| < 10^{-12}$) are rejected cleanly.
   - Verified tolerance: Max observed delta on $M \cdot M^{-1}$ vs identity = 0.00000000 (tolerance limit $10^{-6}$).
4. **`transform_point_exact` (Xbox `0x002be080`)**:
   - Classification: Rendering coordinate transformation ($P' = P \cdot M + T$).
   - Verified tolerance: Delta = 0.00000000.
5. **`transform_points_exact` (Xbox `0x002be080` batch)**:
   - Classification: Batch vertex coordinate transformation.
   - Verified tolerance: Delta = 0.00000000.
6. **`transform_vector_exact` (Xbox `0x002be080`)**:
   - Classification: Directional vector transformation ($V' = V \cdot M$, translation ignored).
   - Verified tolerance: Delta = 0.00000000.
7. **`transform_vectors_exact` (Xbox `0x002be080` batch)**:
   - Classification: Batch directional vector transformation.
   - Verified tolerance: Delta = 0.00000000.

## 3. Engine Startup Addresses Setteled & Separated

To prevent cross-console address mixing and recalled naming errors, addresses are settled directly from authentic disassembly:

| Stage | Xbox `default.xbe` | PS2 `SLUS_214.10` | Role / Observed Behavior |
|---|---|---|---|
| Engine Startup Entry | `0x0007d460` | `0x00127200` | Retail engine initialization caller |
| Initial Heap Buffer | `0x0027c5d0` | N/A (PS2 OS heap) | Allocates 1MB heap buffer (`push 0x100000; push 0`) |
| Engine Initialize (`RwEngineInit`) | `0x002c03c0` | `0x001a6c30` | Called with allocated 1MB heap buffer |
| Plugin Registration Hook | `0x00269520` | `0x001a6dd0` | Calls plugin registration table (0401..0412) |
| Central Freelist Extension Helper | `0x002c4620` | `0x0019fc80` | Central extension freelist allocation helper |
| Video / Subsystem Setup | `0x0007ebf0` | `0x0012726c` | Video mode selector & display setup |
| Engine Open (`RwEngineOpen`) | `0x002c0310` | `0x001a6c30` | Public engine open entrypoint called at `0x0007d4b7`. (Reconciliation: `0x002c0370` is an inner device registration helper; `0x002c0350` is an internal state transition helper called inside `0x002c0310`) |
| Engine Start (`RwEngineStart`) | `0x002bfeb0` | `0x001a6d70` | Dispatches command 2 (`START`) to device driver `[0x00835354]` |
| Camera & Raster Subroutine | `0x001b2760` | `0x001b03c0` | Allocates camera, frame, and color/z-buffer rasters |
| Camera Frustum & Aspect | `0x0007d520` | `0x001b0450` | Subview and aspect ratio configuration |
| Callback Hook Setter | N/A | `0x00225d40` | Callback hook setter storing `0x00225d50` into `-23540($gp)` |

## 4. Native-Owner Mapping (Steps `0x0007d460` to Camera & Raster Attachment)

Mapping of each retail startup step to host native owners:
1. **1MB Heap Buffer Allocation (`0x0027c5d0`)**: Covered by host memory heap / `NativeRetailRendererAllocatorService`.
2. **Engine Initialization (`0x002c03c0`)**: Covered by `NativeRetailRendererCore::init_default`.
3. **Plugin Registrations (`0x00269520`)**: Covered by `construct_core_plugin_*` (plugins 0401..0412 registered).
4. **Subsystem Setup (`0x0007ebf0`)**: Covered by `HiddenGlContext` WGL context.
5. **Engine Open (`0x002c0310`)**: Covered by `NativeRetailRendererCore` device state transition.
6. **Engine Start (`0x002bfeb0`)**: Covered by `NativeRetailRendererCore` device state transition.
7. **Camera Allocation (`0x001b2760`)**: Covered by `NativeRetailCamera::allocate_camera_resource`.
8. **Color Raster Allocation (`0x001b2760`)**: Covered by `NativeRetailRaster` + `NativeGlTextureUpload`.
9. **Depth Raster Allocation (`0x001b2760`)**: Covered by `NativeRetailRaster` + `HiddenGl`.
10. **Raster Attachment to Camera**: **MISSING**. `NativeRetailCamera` has frame/z raster handle slots, but lacks explicit attachment binding methods to bind rasters to camera targets.
11. **Subview & Aspect Configuration (`0x0007d520`)**: Covered by `NativeRetailCamera` view frustum parameters.
12. **Frame Rendering Pipeline (`render_retail_frame`)**: **MISSING**. Requires modern GL FBO binding, clear, viewport setup, shader pipeline, and GPU draw dispatch.

## 5. First-Scene Bootstrap Continuation

Implemented in `runtime/src/native/retail_first_scene_core.cpp`:
- `continue_first_scene_at_225d40`: Validates Session lifetime, constructs registered plugins via `core->construct_registered(cf)`, returns `RetailStartupGate::ready`. Real resource ownership, zero stubs, zero CPU rasterization.
- `continue_scene_reset_at_125d60`: Validates Session lifetime, detaches camera hierarchy via `cameras->detach_cameras_from_181a30(cf)` clearing active scene links (`0x005ab560`, `0x005ab564`), returns `true`. Real resource ownership, zero stubs, zero CPU rasterization.

### What Remains Missing for a Render Target to Reach `render_retail_frame`
1. Explicit raster-to-camera attachment methods in `NativeRetailCamera` (`set_frame_buffer_raster`, `set_z_buffer_raster`).
2. Modern OpenGL Framebuffer Object (FBO) creation and texture/depth attachment (`glFramebufferTexture2D`).
3. Frame begin/end lifecycle binding (`RwCameraBeginUpdate` / `RwCameraEndUpdate`, Xbox `0x002b8500` / `0x002b8ba0`).
4. Modern OpenGL vertex shader, fragment shader, VAO, and draw submission (`glDrawElements` / `glDrawArrays`) without CPU rasterization.

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
