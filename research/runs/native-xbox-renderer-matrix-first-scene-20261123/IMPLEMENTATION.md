# Native Xbox 1280x720 Target Presentation & Retail Video Mode Architecture

Stage: compiled and tested components through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (Matrix kernels, scene bootstrap, and offscreen movie presentation remain component-verified awaiting connected render-target, FBO attachment, 3D retail geometry draw path, and whole-engine integration).

## 1. Authority and Binary Identities

Xbox `default.xbe` and PS2 `SLUS_214.10` establish the video mode selection, matrix transform contracts, engine initialization call graph, and scene bootstrap sequence:
- Xbox `default.xbe`: SHA-256 `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- PS2 `SLUS_214.10`: SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Implementation files:
- `runtime/include/mka/retail_display_service.hpp`
- `runtime/include/mka/retail_renderer_cameras.hpp`
- `runtime/src/native/retail_display_service.cpp`
- `runtime/src/native/retail_initial_movie_path.cpp`
- `runtime/src/native/retail_renderer_cameras.cpp`
- `runtime/src/native/retail_renderer_matrix_kernels.cpp`
- `runtime/src/native/retail_renderer_transforms_kernels.cpp`
- `runtime/src/native/retail_first_scene_continuations.cpp`
- `tests/test_retail_display_movie_presentation.cpp`
- `tests/test_retail_renderer_cameras.cpp`
- `tests/test_retail_renderer_matrix.cpp`

Zero CPU rasterization / zero guest console emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JITs) is used.

## 2. Xbox Video Mode Recovery, 1280x720 TargetParity & Presentation Architecture

### Owner Confirmation & Binary Recovery Evidence
Owner confirmed: **1280x720 is the primary Xbox target resolution** for native reimplementation. Disassembly of Xbox `default.xbe` conclusively proves this configuration:
- In `0x0007d460` (retail engine startup), `0x002695f0` is invoked to configure `RwEngineOpenParams` at `0x005bf520`.
- In `0x002695f0`:
  - Queries Xbox dashboard / hardware video flags via `call 0x002a37cb`.
  - Bit 1 test (`test eax, 2` / HDTV 720p enabled):
    - Sets logical width: `0x005bf520 = 0x500` (1280)
    - Sets logical height: `0x005bf524 = 0x2d0` (720)
    - Sets backbuffer count: `0x005bf534 = 2` (double-buffered presentation)
    - Sets refresh rate: `0x005bf540 = 0x3c` (60 Hz)
    - Sets widescreen mode: `0x005bf53c = 0x50` (`0x40 | 0x10`)
    - Sets format/depth: `0x005bf528 = 0x11` (32-bit ARGB8888 color, 24-bit D24S8 depth)
    - Sets camera aspect ratio: `0x0054ca88 = 0x3fe38e39` (1.7777778f = 16:9 widescreen)
    - Sets render target dimensions: `0x00831e1c = 1280`, `0x00831e18 = 720`
  - Bit 0 test (480p widescreen): Sets 720x480 (`0x2d0` x `0x1e0`) with 16:9 aspect (`0x3fe38e39`).
  - Standard 480i: Sets 640x480 (`0x280` x `0x1e0`) with 4:3 aspect (`0x3faaaaab` = 1.3333334f).
  - **Evidence conclusion**: 640x448 does NOT exist on Xbox; it is strictly a PS2 GS NTSC legacy resolution.

### PS2 Comparison (`SLUS_214.10`)
- PS2 startup at `0x0012726c`, `0x001b03c0`, and `0x001b0450`:
  - Mode 7 in the Sky2 GS subsystem selects 640x448 NTSC active raster (interlaced/progressive) with anamorphic CRT scanout.

### Retail Movie Variant Selection (4:3 N vs 16:9 W Streams)
Retail binaries contain dual authored streams for movies: standard 4:3 (`*N.SFD`) and widescreen 16:9 (`*W.SFD`):
- **Xbox Movie Selection (`0x0025d5b0`)**:
  - Clamps input `movie_id` to $[0 .. 32]$.
  - Calls `0x0027c790` (`is_widescreen_active()`).
  - Accesses 40-byte movie descriptor entry at `movie_descriptors[movie_id]`:
    - Offset `+0x00`: Pointer to 4:3 filename (`*N.SFD`), width at `+0x04`, height at `+0x08`.
    - Offset `+0x0c`: Pointer to 16:9 filename (`*W.SFD`), width at `+0x10`, height at `+0x14`.
  - If `is_widescreen_active()` returns 1 AND descriptor offset `+0x0c` is non-null, selects pointer at `+0x0c` (`*W.SFD`).
  - If widescreen is not active OR descriptor `+0x0c` is null, falls back to pointer at `+0x00` (`*N.SFD`).
- **Xbox Widescreen Query (`0x0027c790`)**:
  - Calls `0x002a37cb` to read video flags.
  - Tests bit 0 (`test eax, 1`). If set, returns 1.
  - Tests global runtime override `dword [0x005bfa64]`. If non-zero, returns 1.
  - Otherwise returns 0.
- **PS2 Movie Selection (`0x00181fd0..0x001820bc`)**:
  - Computes `entry_idx * 40` into `movie_descriptors[4]` at `0x005a24c0`.
  - Checks the video settings widescreen flag.
  - If widescreen is active, selects `+0x0c` (`*W.SFD`); otherwise selects `+0x00` (`*N.SFD`).

### Static Helper Audit (`0x002a37cb`)
- Statically audited the Xbox XBE import table in `default.xbe`:
  - `0x002a37cb` is **NOT** an imported function.
  - `default.xbe` imports Xbox Kernel Ordinal 24 (`ExQueryNonVolatileSetting`) via thunk `0x002eac82` -> IAT `0x004d77f8` (raw ordinal `0x80000018`).
  - Function `0x002a37cb` is a statically linked Microsoft XDK helper (named `XGetVideoFlags` in XDK headers) that invokes `ExQueryNonVolatileSetting(XC_VIDEO_FLAGS = 8, ...)` and returns `(Value >> 16) & 0x5f`.
  - In our implementation, it is identified strictly by its static address `0x002a37cb`.

### Movie Layout Rules on 1280x720 Target
- **Standard 4:3 Streams (`*N.SFD`, e.g. `LOGON.SFD`)**:
  - Destination geometry: `[160, 0, 960, 720]` (`MovieWindowPolicy::centered_fit`, `MovieAspectMode::explicit_display_aspect` 4:3).
  - 160-pixel black pillarbox margins on left and right maintain correct 4:3 geometry without stretching.
- **Widescreen 16:9 Streams (`*W.SFD`, e.g. `LOGOW.SFD`)**:
  - Destination geometry: `[0, 0, 1280, 720]` (`MovieWindowPolicy::centered_fit`, `MovieAspectMode::explicit_display_aspect` 16:9).
  - Edge-to-edge full-screen presentation filling the entire $1280 \times 720$ canvas with zero pillarboxing.

### Classification of GS Emulation Code for TASK_BOARD.md Row 9
`TASK_BOARD.md` row 9 mandates removing the host VRAM and state model emulation. Code classification:
1. `runtime/src/gs/gs.cpp` and `runtime/src/gs/gs.hpp`:
   - Legacy PS2 recomp GS hardware emulation (`g_state`, synthetic VRAM pages, GIF packet parsing, GS registers).
   - Slated for complete removal under row 9 once native scene rendering replaces GS packet interception.
2. `runtime/src/gs/native_gl_backend.cpp`:
   - Transitional primitive interceptor with compatibility VRAM readback.
   - Slated for removal under row 9 once native scene geometry draw calls replace GS packet interception.
3. `runtime/src/gs/gpu/*.wgsl`:
   - WebGPU compute shaders implementing legacy GS rasterization pipelines.
   - Slated for removal under row 9.
4. Production display services (`runtime/src/native/retail_display_service.cpp`) and native renderer plugins (`retail_renderer_*.cpp` 0401..0412):
   - Accepted native route.

### Xbox 2D UI Layout Recovery & Safe Area Transform Proposal
- Disassembly of `0x00293820` (quad rasterizer) and string renderer `0x002907ff`:
  - Retail 2D UI and HUD positioning operate in normalized virtual coordinate space $[0.0 .. 1.0]$.
  - At 720p widescreen (`dword [0x005bfa64] != 0`), `0x00293820` scales normalized coordinates by `1280.0f x 720.0f`.
  - Constant pool `0x00508ffc` (`0.898f`) and `0x0050911c` (`0.934f`) define Xbox title-safe (89.8%) and action-safe (93.4%) margins.
- Proposed layout transform for native HD HUD/UI:
  - Anchor HUD meters and gauges to screen edges using 90% title-safe margins ($64\text{px}$ horizontal, $36\text{px}$ vertical on 720p).
  - Maintain 1:1 pixel aspect ratio for 2D system-art fonts and textures to avoid stretching.
  - Apply Hor+ widescreen camera projection ($1.7777778$ aspect ratio at `0x0054ca88`) for 3D viewports.
  - *Status*: Proposed layout transform, not yet implemented in production UI code.

## 3. Option C (Partitioned) Mathematical Authority & Matrix Kernels

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

### Retail Startup Presentation Chain & Sequence Reconciliation
Disassembly of `SLUS_214.10` and Xbox `default.xbe` reconciles the movie descriptor table with runtime state progression:
- Array `movie_descriptors[4]` at `0x005a24c0` (40 bytes per entry) is an indexed asset array:
  - Index 0: `LOGON.SFD` (`0x005fbb80`, 640x448, 152 frames) + widescreen variant `LOGOW.SFD` (Midway logo intro).
  - Index 1: `TITLEN.SFD` (`0x005fbbc0`, 640x448, 900 frames) + widescreen variant `TITLEW.SFD` (idle title screen).
  - Index 2: `OPENINGN.SFD` (`0x005fbc00`, 640x448, 7,841 frames) + widescreen variant `OPENINGW.SFD` (intro story cutscene).
  - Index 3: `ATTRACTN.SFD` (`0x005fbc40`, 640x448) + widescreen variant `ATTRACTW.SFD` (gameplay attract demo).
- State Machine Dispatch Table at `0x005a3410`:
  - State 0 (`0x005a3410`): Calls `0x0018d450` (`begin_menu_18d450`) -> calls `0x00181f90` with parameter $a0 = 0$ (`LOGON.SFD`).
  - State 1 (`0x005a3414`): Calls `0x0018d870` (`begin_menu_18d870`) -> calls `0x00181f90` with parameter $a0 = 2$ (`OPENINGN.SFD`).
  - State 2 (`0x005a3418`): Calls `0x0018d8e0` (`begin_menu_18d8e0`) -> calls `0x00181f90` with parameter $a0 = 1$ (`TITLEN.SFD`).
- Sequence reconciliation:
  - In retail boot / Playable Oracle R183, boot executes State 0 (`LOGON.SFD`) followed immediately by State 1 (`OPENINGN.SFD`), and upon completion (or button press) transitions directly to the Kombat main menu at frame 19,800.
  - State 2 (`TITLEN.SFD`) is an idle title screen entered only when the attract timer expires while waiting at the main menu. Thus `TITLEN.SFD` is conditional, explaining why Oracle R183 observes `LOGON -> OPENINGN -> Menu`.

### Production Display Interface (1280x720 Primary Target)
Implemented in `runtime/include/mka/retail_display_service.hpp` and `runtime/src/native/retail_display_service.cpp`:
- `NativeProductionDisplayService` defines the production display contract: context lease lifecycle (`acquire_context`, `release_context`), viewport calculation with aspect scaling (`compute_viewport`), buffer presentation (`present_frame`), and vsync control (`set_vsync`).
- Primary logical resolution: `LOGICAL_WIDTH = 1280`, `LOGICAL_HEIGHT = 720` (Xbox target resolution).
- Named PS2 reference mode: `PS2_LOGICAL_WIDTH = 640`, `PS2_LOGICAL_HEIGHT = 448`.
- `NativeOffscreenDisplayService` implements the interface for headless and offscreen targets:
  - Context ownership: Hosts an offscreen double-buffered WGL context without an active desktop window.
  - Viewport & aspect scaling: Computes exact 1:1 on 1280x720 16:9 widescreen, 4:3 pillarboxing (960x720 centered with 160px black borders) on 1280x720, and 1:1 on named 640x448 PS2 reference mode.
  - Swap & vsync semantics: Virtual swap via `glFlush()` for offscreen targets. Vsync control via `set_vsync` cleanly returns `false` (`vsync_unsupported_offscreen`).
  - Telemetry: Tracks `gl_draw_calls`, `cpu_composited_pixels`, `frame_readbacks`, `diagnostic_readbacks`, `fallback_pixels`.

### Authentic Movie Presentation & External Frame Capture
Verified in `tests/test_retail_display_movie_presentation.cpp`:
- Pipeline:
  - Instantiates `NativeOffscreenDisplayService` at 1280x720.
  - Attaches `NativeMovieGlDevice` to the display service under a valid context lease.
  - Decodes frame 0 of authentic retail `LOGOW.SFD` YUV420 planes (`LOGOW.SFD.first.plane{0,1,2}`) using the authentic `MOVIER.ovl` color transformation matrix (`0x77dd8`).
  - Uploads texture planes to OpenGL and presents via `NativeMovieGlDevice::present` with `MovieWindowPolicy::centered_fit` and explicit 16:9 display aspect.
  - Destination rect computed: `[0, 0, 1280, 720]` (edge-to-edge 16:9 full-screen, no pillarboxing).
  - Also verifies standard 4:3 pillarbox destination geometry rule `[160, 0, 960, 720]` for standard 4:3 N streams without an extra draw call.
  - Executes hardware draw call (`glBegin(GL_QUADS)` inside `NativeMovieGlDevice`).
- Diagnostic Frame Capture & Regression Pin:
  - Frame buffer readback via allowlisted diagnostic readback:
    - `FNV1a64 = 0x2cb5ec60e4e72325`
    - `SHA256 = ba848e89025edbb7a00d11be30040c23e036d1e57606c86074e65dd2c70191d7`
    - Documented: Frame hashes are driver-dependent regression pins.
- Independent CPU Reference Verification (`[native-design-check]`):
  - Active movie quad fills entire 1280x720 canvas edge-to-edge with no pillarbox bars.
  - Sample points across active movie quad (center logo and near quad corners) compared against CPU-converted reference `expected_frame.rgba` with maximum absolute delta $\le 2$.
- Production Presentation Telemetry:
  - `[diagnostic-telemetry] GL draw calls = 1, CPU composited pixels = 0, frame readbacks = 0, diagnostic readbacks = 1, fallback pixels = 0`
  - Zero unallowlisted CPU readbacks or software fallbacks occur.

## 5. First-Scene Bootstrap Continuation & Retail Movie Path

Implemented in `runtime/src/native/retail_first_scene_continuations.cpp`:
- `continue_first_scene_at_225d40`: Validates Session lifetime, constructs registered plugins via `core->construct_registered(cf)`, returns `RetailStartupGate::ready`. Real resource ownership, zero stubs, zero CPU rasterization.
- `continue_scene_reset_at_125d60`: Validates Session lifetime, detaches camera hierarchy via `cameras->detach_cameras_from_181a30(cf)` clearing active scene links (`0x005ab560`, `0x005ab564`), returns `true`. Real resource ownership, zero stubs, zero CPU rasterization.

Implemented in `runtime/src/native/retail_initial_movie_path.cpp`:
- `begin_menu_18d450`: Advances video frame with $1.0\text{f}$ wait tick then completes, returning `NativeRetailMovie0Child`.
- `begin_intro_18deb0`: Advances with $300.0\text{f}$ frame wait tick then completes, returning `NativeRetailIntro18deb0Child`.
- `transition_1225d0`: Pushes transition mode into `main.mode_history`.

## 6. Undefined Symbol Count Diagnostic & Ordered Queue

The full-entry link diagnostic currently contains **21 undefined symbols** (representing missing link definitions satisfied by placeholders during diagnostic linking; this reduction from 24 to 21 is recorded strictly as a link diagnostic, not recovered progress):

Resolved symbols in this subsystem (12):
1. `mka::retail::renderer_required::multiply_affines_19f680`
2. `mka::retail::renderer_required::classify_matrix_1a0240`
3. `mka::retail::renderer_required::invert_nonidentity_matrix_1a07a0`
4. `mka::retail::renderer_required::transform_point_exact`
5. `mka::retail::renderer_required::transform_points_exact`
6. `mka::retail::renderer_required::transform_vector_exact`
7. `mka::retail::renderer_required::transform_vectors_exact`
8. `mka::retail::scene_bootstrap_required::continue_first_scene_at_225d40`
9. `mka::retail::scene_bootstrap_required::continue_scene_reset_at_125d60`
10. `mka::retail::initial_task_required::begin_menu_18d450`
11. `mka::retail::initial_task_required::begin_intro_18deb0`
12. `mka::retail::initial_task_required::transition_1225d0`

Remaining undefined symbols queue order (21) - Reordered with Retail Movie Path First:
1. **Priority Queue 1: Retail Movie Presentation Path (1 symbol)**:
   - `mka::retail::main_binding::render_retail_frame` (primary frame presentation pipeline dispatcher)
2. **Priority Queue 2: Remaining Initial Task & Reset Symbols (8 symbols)**:
   - `mka::retail::initial_task_required::begin_menu_18d660`
   - `mka::retail::initial_task_required::begin_menu_18d700`
   - `mka::retail::initial_task_required::begin_menu_18d770`
   - `mka::retail::initial_task_required::begin_menu_18d800`
   - `mka::retail::initial_task_required::begin_menu_18d870`
   - `mka::retail::initial_task_required::begin_menu_18d8e0`
   - `mka::retail::initial_task_required::begin_prompt_18dc10`
   - `mka::retail::initial_reset_required::clear_nonowning_references_18da30`
3. **Priority Queue 3: Remaining Main-Binding Dispatch Symbols (9 symbols)**:
   - `mka::retail::main_binding::advance_retail_game_services`
   - `mka::retail::main_binding::create_retail_action_task`
   - `mka::retail::main_binding::deliver_retail_builtin_scene_event`
   - `mka::retail::main_binding::dispatch_retail_enabled_mode`
   - `mka::retail::main_binding::poll_retail_controller_roles`
   - `mka::retail::main_binding::prune_retail_associations`
   - `mka::retail::main_binding::reset_and_bind_retail_scene_event_graph`
   - `mka::retail::main_binding::retail_mode_dispatch_enabled`
   - `mka::retail::main_binding::service_retail_frame_input`
4. **Priority Queue 4: Profile Registration Symbols (3 symbols)**:
   - `mka::retail::apply_retail_profile_participant_from_188720`
   - `mka::retail::invoke_retail_profile_select_from_2f1630`
   - `mka::retail::publish_retail_profile_participant_from_189690`

## 7. Acceptance & Verification Commitments

- **Zero Visible Windows / Audio Endpoints / Owner Launch**: In strict adherence to TASK_BOARD.md row 10, presentation is hosted entirely in an offscreen double-buffered WGL target (`NativeOffscreenDisplayService`). No desktop window, audible sound stream, or owner game loop is opened.
- **Strict Accounting**:
  - PS2 function ledger: strictly 238 / 15,670 (1.52%).
  - Xbox donor ledger: strictly 0 / 21,321 (0.00%).
  - Acceptance status: strictly **OPEN**.
- **No Overclaiming**: Verified offscreen presentation of `LOGOW.SFD` frame 0 demonstrates the video display interface and texture upload pipeline, but does not constitute 3D gameplay geometry rendering or whole-game acceptance.
