# Native Xbox 1280x720 Target Presentation & Retail Video Mode Architecture

## 1. Authority and Binary Identities

Verification and disassembly authority are established directly from retail executables:
- **Xbox Retail Binary**: `Armageddon-backup/armageddon-reimplementation/private-inputs/xbox-mka-20261121/default.xbe`
  - SHA-256: `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- **PS2 Retail Binary**: `Armageddon-backup/armageddon-reimplementation/private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/SLUS_214.10`
  - SHA-256: `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Policy requirements:
- **Xbox-First Policy**: All rendering, video mode selection, display target parameters, and 2D UI layouts follow Xbox retail architecture as the primary authority, cross-checked with PS2.
- **Strictly Headless Execution**: In compliance with `TASK_BOARD.md` row 10, no visible desktop window, audible sound stream, or owner game loop is opened prior to passing the full acceptance matrix. Offscreen WGL double-buffered surfaces (`NativeOffscreenDisplayService`) host all presentation verification.
- **Tracked Assets Verification**: In compliance with repository policy, no retail game assets or decoded video planes/frames are committed to version control. `git ls-files` confirms zero retail movie frames/planes are tracked; `.gitignore` excludes `*.plane*`, `*.s16`, `*.sfd`, `*.yuv`, and decoded directories. Decoded frames are referenced strictly by hash in run records.
- **Single Runner Invariant**: `tools/research/run_check.py` is the single canonical test and build runner. `run_14_tests.py` has been deleted via `git rm`.

## 2. Xbox Video Mode Recovery, 1280x720 Target Parity & Presentation Architecture

### Owner Confirmation & Binary Recovery Evidence
Owner confirmation mandates **1280x720 (720p HD, 16:9)** as the primary native target resolution. Disassembly of `default.xbe` confirms retail Xbox video mode initialization:
- **Startup Entry & Mode Selection (`0x0007ebf0`)**:
  - Subroutine `0x0007ebf0` reads video configuration from the dashboard via static helper `0x002a37cb`.
  - Tests bit 3 (`test eax, 8`) for 720p capability.
  - If enabled, branches to `0x002695f0` which initializes the video mode:
    - Sets display width to 1280 (`mov dword [0x005bf9a0], 1280`).
    - Sets display height to 720 (`mov dword [0x005bf9a4], 720`).
    - Sets widescreen active flag (`mov byte [0x005bfa64], 1`).
    - Sets viewport aspect ratio float constant to $1.7777778\text{f}$ (`16.0f / 9.0f` at `0x0054ca88`).
  - If 720p is not enabled, falls back to 480p widescreen ($853 \times 480$, aspect $1.7777778\text{f}$) or 480i standard ($640 \times 480$, aspect $1.3333334\text{f}$).
  - *Conclusion*: 1280x720 is an authenticated retail video mode gated by dashboard configuration flags. The native port adopts 1280x720 as its primary baseline display target while supporting standard 4:3 modes without distortion.

### PS2 Comparison (`SLUS_214.10`)
- PS2 retail startup (`0x0007d460` -> `0x001a6dd0`) configures the GS for NTSC standard 640x448 logical framebuffer resolution.
- PS2 widescreen mode (selected in the options menu) keeps the logical framebuffer at 640x448 but adjusts 3D camera horizontal FOV from 4:3 to 16:9 anamorphic squeeze.
- 640x448 is retained in the codebase strictly as a named reference mode (`DisplayReferenceMode::ps2_640x448`) for differential verification; 1280x720 is the primary native target.

### Recovered Retail Movie Table (33 Entries)
Disassembly of descriptor table at Xbox `0x00579828` and PS2 `0x005a24c0` confirms exactly 33 movie entries (40 bytes per entry):
- Offsets:
  - `+0x00`: Pointer to standard 4:3 filename (`*N.SFD`), width at `+0x04`, height at `+0x08`.
  - `+0x0c`: Pointer to widescreen 16:9 filename (`*W.SFD`), width at `+0x10`, height at `+0x14`.
  - `+0x18`: Pointer to PAL 60Hz 4:3 filename (`*6N.SFD`) on PS2; zero on Xbox.
  - `+0x1c`: Pointer to PAL 60Hz 16:9 filename (`*6W.SFD`) on PS2; zero on Xbox.

| ID | Normal Name | Normal Res | Wide Name | Wide Res | PAL60 Normal (PS2) | PAL60 Wide (PS2) | Disc Availability (`MOVIEPS2/`) |
|---|---|---|---|---|---|---|---|
| 0 | `LOGON.SFD` | 640x480 | `LOGOW.SFD` | 704x480 | `LOGO6N.SFD` | `LOGO6W.SFD` | Present |
| 1 | `TITLEN.SFD` | 640x480 | `TITLEW.SFD` | 736x480 | `TITLE6N.SFD` | `TITLE6W.SFD` | Present |
| 2 | `OPENINGN.SFD` | 640x480 | `OPENINGW.SFD` | 736x480 | `OPENIN6N.SFD` | `OPENIN6W.SFD` | Present |
| 3 | `ATTRACTN.SFD` | 640x480 | `ATTRACTW.SFD` | 736x480 | `ATTRAC6N.SFD` | `ATTRAC6W.SFD` | Present |
| 4 | `QUADN.SFD` | 640x480 | `QUADW.SFD` | 704x480 | `QUAD6N.SFD` | `QUAD6W.SFD` | Present |
| 5 | `STRANGN.SFD` | 640x480 | `STRANGW.SFD` | 736x544 | `nullptr` | `nullptr` | Present |
| 6 | `SPYHUNN.SFD` | 640x480 | `SPYHUNW.SFD` | 736x480 | `nullptr` | `nullptr` | Present |
| 7 | `BLITZN.SFD` | 640x480 | `BLITZW.SFD` | 736x544 | `nullptr` | `nullptr` | Present |
| 8 | `V_SNDOUT.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 9 | `V_ERMACB.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 10 | `V_MOTORK.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 11 | `V_SCORP.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 12 | `V_RAIDON.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 13 | `V_SZCCKY.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 14 | `V_LOAD2.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 15 | `V_BLOOPR.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 16 | `V_LOAD1.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 17 | `V_SHINOK.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 18 | `V_SUBZER.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 19 | `V_KANO.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 20 | `V_MOTRBK.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 21 | `V_MOTRFT.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 22 | `V_BLOOP5.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 23 | `V_BLOOP4.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 24 | `V_BLOOP2.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 25 | `V_BLOOP3.SFD` | 320x240 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 26 | `V_KONQIN.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 27 | `V_MKDACC.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 28 | `V_BLUES.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 29 | `V_MKAKON.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 30 | `V_MKHAND.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |
| 31 | `V_FIN.SFD` | 160x128 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Absent on disc |
| 32 | `V_MOTOGI.SFD` | 640x480 | `nullptr` | 0x0 | `nullptr` | `nullptr` | Present |

**PAL60 Variants Explanation (`6N` and `6W`)**:
- In the European PAL release on PS2, 50Hz/60Hz video selection is supported. Movies 0..4 contain additional variants ending in `6N.SFD` (PAL 60Hz 4:3) and `6W.SFD` (PAL 60Hz 16:9).
- On Xbox, video refresh and output timings are handled transparently by the dashboard/hardware encoder; therefore, the PAL60 fields in the Xbox table are 0 (`nullptr`).

**Disc Inputs Audit**:
- 32 of the 33 movies exist in the repository's disc inputs under `milestone_build_kombat_practice/disc-files/MOVIEPS2/`.
- Only index 31 (`V_FIN.SFD`, a low-resolution $160 \times 128$ bonus clip) is absent from the disc media.

### Retail Movie Variant Selection (4:3 N vs 16:9 W Streams)
- **Xbox Movie Selection (`0x0025d5b0`)**:
  - Clamps input `movie_id` to $[0 .. 32]$.
  - Queries `0x0027c790` (`is_widescreen_active()`).
  - If widescreen is active AND descriptor offset `+0x0c` (`wide_name`) is non-null, selects pointer at `+0x0c` (`*W.SFD`).
  - If widescreen is inactive OR descriptor `+0x0c` is null, falls back to pointer at `+0x00` (`*N.SFD`).
- **Xbox Widescreen Query (`0x0027c790`)**:
  - Calls `0x002a37cb` to read video flags.
  - Tests bit 0 (`test eax, 1`). If set, returns 1.
  - Tests global runtime override `dword [0x005bfa64]`. If non-zero, returns 1.
  - Otherwise returns 0.
- **PS2 Movie Selection (`0x00181fd0..0x001820bc`)**:
  - Indexes table at `0x005a24c0`. Checks video settings widescreen flag.
  - Selects `+0x0c` (`*W.SFD`) if widescreen is active, otherwise `+0x00` (`*N.SFD`).

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
  - Constant pool entries `0x00508ffc` (`0.898f`) and `0x0050911c` (`0.934f`) define Xbox boundary margins:
    - `0.898f`: active bounds $1150 \times 647$, leaving $65\text{px}$ horizontal margin and $37\text{px}$ vertical margin per edge.
    - `0.934f`: active bounds $1196 \times 672$, leaving $42\text{px}$ horizontal margin and $24\text{px}$ vertical margin per edge.
  - **Inferred Labeling**: The labels "title-safe" ($0.898\text{f}$) and "action-safe" ($0.934\text{f}$) are **inferred interpretations** based on standard video broadcast practices. Neither string appears in retail binary symbols or debug strings.
- Proposed layout transform for native HD HUD/UI:
  - Anchor HUD meters and gauges to screen edges using recovered $0.898\text{f}$ margins ($65\text{px}$ horizontal, $37\text{px}$ vertical on 720p).
  - Maintain 1:1 pixel aspect ratio for 2D system-art fonts and textures to avoid stretching.
  - Apply Hor+ widescreen camera projection ($1.7777778\text{f}$ aspect ratio at `0x0054ca88`) for 3D viewports.
  - *Status*: Proposed layout transform, not yet implemented in production UI code.

## 3. Option C (Partitioned) Mathematical Authority & Matrix Kernels

### Owner Decision
Option C (Partitioned): Rendering transforms (view, projection, model, raster) use native host IEEE 754 `float` math with documented tolerances. Gameplay-critical arithmetic is classified function-by-function based on Xbox x86 behavior or documented tolerance, with zero EE COP1 bit-exact emulation unless a specific retail oracle replay requires it.

### Classification of Callers & Kernel Equivalents
1. `classify_matrix_1a0240` (PS2 `0x001a0240`, Xbox `0x002be160`):
   - Category: Verified rendering transform classifier.
   - Xbox equivalent: `0x002be160` (computes matrix type flags: identity, translate, scale, general affine).
2. `invert_nonidentity_matrix_1a07a0` (PS2 `0x001a07a0`, Xbox `0x002be560`):
   - Direct caller: Call site `0x002be81e` inside function `0x002be770` (camera inverted view matrix update).
   - Inversion tolerance: $1\text{e}-6$; determinant threshold: $|\det| < 1\text{e}-12$. Explicitly documented as native host IEEE 754 stability design choices.
3. `multiply_affines_19f680` (PS2 `0x0019f680`, Xbox `0x002be290`):
   - Callers:
     - Bone/skeletal matrix callers (`0x00261915`, `0x00261a80`, `0x00261c47`): Classified as **inferred / potentially gameplay-coupled**. In fighting games, character bone hierarchy transforms update hitbox/hurtbox physics and collision volumes.
     - 2D UI presentation callers (`0x0027488f`, `0x00274eaa`, `0x00275c90`, `0x00276f43`, `0x00278725`, `0x002794af`, `0x00279625`): Classified as **inferred** UI quad root transform callers.
4. Transform Kernels (`transform_point_exact`, `transform_points_exact`, `transform_vector_exact`, `transform_vectors_exact`):
   - PS2 functions: `0x0019ff20`, `0x001a0030`, `0x0019fc80`, `0x0019fda0`.
   - Xbox counterparts: Marked as **unverified candidates**; no verified 1:1 address pair is claimed.
   - Verified against independent host float references; stage tags read `[native-design-check]`.

## 4. Authentic Retail Presentation Order & Production Display Service

### Retail Startup Presentation Chain & Sequence Reconciliation
- Authentic retail startup sequence recovered from indexed movie descriptor table (`0x005a24c0` / `0x00579828`) and startup state machine table (`0x005a3410`):
  - Index 0: `LOGON.SFD` / `LOGOW.SFD` (Midway logo).
  - Index 2: `OPENINGN.SFD` / `OPENINGW.SFD` (Opening cinematic).
  - Index 1: `TITLEN.SFD` / `TITLEW.SFD` (Title screen attract loop).
- Sequence reconciliation:
  - Oracle R183 sequence (`LOGON` -> `OPENINGN` -> Menu) is authentic for the normal boot path.
  - `TITLEN` is played as an idle attract loop when the start menu remains unprompted.
  - Retail startup controller routines (`begin_menu_18d450`, `begin_intro_18deb0`, `transition_1225d0`, `render_retail_frame`) advance through this state machine.

### Production Display Interface (1280x720 Primary Target)
Implemented in `runtime/src/native/retail_display_service.cpp` (`mka::retail`):
- `NativeProductionDisplayService` defines the production display contract: context lease, aspect ratio conversion, virtual swapchain, and telemetry.
- `NativeOffscreenDisplayService` implements headless offscreen rendering via invisible WGL double-buffered context.
- Target configuration: primary Xbox HD target $1280 \times 720$ (16:9), with $640 \times 448$ preserved solely as named PS2 reference mode.

### Authentic Movie Presentation & External Frame Capture
- Verification in `tests/test_retail_display_movie_presentation.cpp`:
  - Drives offscreen presentation via `play_retail_movie_frame(0, *display, *backend, *table, video, /*widescreen_active=*/true)`.
  - Movie quad fills $1280 \times 720$ edge-to-edge with zero pillarboxing for `LOGOW.SFD`.
  - Frame hash: FNV1a64 = `0x2cb5ec60e4e72325`, SHA-256 = `ba848e89025edbb7a00d11be30040c23e036d1e57606c86074e65dd2c70191d7` (driver regression pin).
  - Independent CPU reference conversion verification: max absolute delta $\le 2$ against `expected_frame.rgba`.
  - Telemetry counters: `[diagnostic-telemetry] GL draw calls = 1, CPU composited pixels = 0, frame readbacks = 0, diagnostic readbacks = 1, fallback pixels = 0`.

## 5. First-Scene Bootstrap Continuation & Retail Movie Path

### Recovered Disassembly Semantics & Placeholder Status
Disassembly of original functions in PS2 and Xbox binaries reveals authentic runtime behavior:
- **`begin_menu_18d450` (PS2 `0x0018d450` <-> Xbox `0x00028300`)**:
  - Resets subsystem state (PS2 `0x0018d970`, Xbox `0x00027f00`).
  - Calls movie playback routine (PS2 `0x00181f90`, Xbox `0x0025d380`) with `movie_id = 0` (`LOGON.SFD` / `LOGOW.SFD`).
  - Calls audio synchronization (PS2 `0x00163ba0`, Xbox `0x000a0b60`).
  - Initiates mode transition 0 (PS2 `0x001225d0`, Xbox `0x0019ca30`).
- **`begin_intro_18deb0` (PS2 `0x0018deb0`)**:
  - Configures boot subtitle art and layout quad parameters via `0x00172e40`.
  - Dispatches intro task handler vtable at offset 20.
- **`transition_1225d0` (PS2 `0x001225d0` <-> Xbox `0x0019ca30`)**:
  - Pushes mode transition into mode history (`0x001ddf98` on PS2, `[esi+0x10]` on Xbox).
  - Invokes previous mode exit callback from table at `[eax+0x18]`.
  - Creates new mode task via `0x0012dcc0` / `0x0005a7e0`.
- **Wait Ticks Explanation**:
  - The $1.0\text{f}$ wait tick in `begin_menu_18d450` and $300.0\text{f}$ wait tick in `begin_intro_18deb0` from earlier scaffolding were **synthetic heuristics**, NOT recovered from disassembly.
  - Neither binary contains timer wait ticks in these routines.
  - **Placeholder Status**: `begin_menu_18d450`, `begin_intro_18deb0`, and `transition_1225d0` are explicitly designated as **unrecovered placeholders** in the codebase and run record; their linking does not count as recovered progress.

### Wiring Retail Movie Start
- Implemented in `runtime/include/mka/retail_movie_controller.hpp` and `runtime/src/native/retail_initial_movie_path.cpp`:
  - `select_retail_movie_variant(movie_id, widescreen_active)`: authentic 33-entry table lookup, selecting `wide_name` when widescreen is active and falling back to `norm_name`.
  - `play_retail_movie_frame(...)`: drives `NativeProductionDisplayService` and `video::NativeMovieGlDevice`, performing YUV420->RGBA conversion, texture upload, destination quad configuration, draw call, and swap buffer.
  - Verified in `tests/test_retail_display_movie_presentation.cpp` where the movie controller path starts `LOGO*` and presents frame 0 through the display service.

### `render_retail_frame` Implementation
- Implemented `render_retail_frame(NativeRetailMainState&, StartupFailure&)` in `runtime/src/native/retail_initial_movie_path.cpp`.
- Recovered from PS2 `0x00126940` and Xbox main loop frame rendering structure.
- **Placeholder Status**: While display context presentation and state validation are authentic, 3D scene raster submission is explicitly marked as an **unrecovered placeholder** awaiting connected 3D geometry paths.

## 6. Undefined Symbol Count Diagnostic & Ordered Queue

The negative link symbol diagnostic currently contains **20 undefined symbols** (representing missing link definitions satisfied during diagnostic linking; the reduction from 21 to 20 via `render_retail_frame` is recorded strictly as a link diagnostic, NOT recovered progress):

Resolved symbols in this subsystem (13):
1. `mka::retail::renderer_required::multiply_affines_19f680`
2. `mka::retail::renderer_required::classify_matrix_1a0240`
3. `mka::retail::renderer_required::invert_nonidentity_matrix_1a07a0`
4. `mka::retail::renderer_required::transform_point_exact`
5. `mka::retail::renderer_required::transform_points_exact`
6. `mka::retail::renderer_required::transform_vector_exact`
7. `mka::retail::renderer_required::transform_vectors_exact`
8. `mka::retail::scene_bootstrap_required::continue_first_scene_at_225d40`
9. `mka::retail::scene_bootstrap_required::continue_scene_reset_at_125d60`
10. `mka::retail::initial_task_required::begin_menu_18d450` (unrecovered placeholder)
11. `mka::retail::initial_task_required::begin_intro_18deb0` (unrecovered placeholder)
12. `mka::retail::initial_task_required::transition_1225d0` (unrecovered placeholder)
13. `mka::retail::main_binding::render_retail_frame` (unrecovered placeholder for 3D scene)

Remaining undefined symbols queue order (20):
1. **Priority Queue 1: Retail Movie Presentation Path (0 symbols)**:
   - Fully resolved to link diagnostic level (all movie path symbols satisfied).
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

- **Zero Visible Windows / Audio Endpoints / Owner Launch**: In strict adherence to `TASK_BOARD.md` row 10, presentation is hosted entirely in an offscreen double-buffered WGL target (`NativeOffscreenDisplayService`). No desktop window, audible sound stream, or owner game loop is opened.
- **Strict Accounting**:
  - PS2 function ledger: strictly 238 / 15,670 (1.52%).
  - Xbox donor ledger: strictly 0 / 21,321 (0.00%).
  - Acceptance status: strictly **OPEN**.
- **No Overclaiming**: Verified offscreen presentation of `LOGOW.SFD` frame 0 demonstrates the video display interface and texture upload pipeline, but does not constitute 3D gameplay geometry rendering or whole-game acceptance.
- **Single Runner**: `tools/research/run_check.py` is the sole canonical runner; `run_14_tests.py` has been deleted via `git rm`.
