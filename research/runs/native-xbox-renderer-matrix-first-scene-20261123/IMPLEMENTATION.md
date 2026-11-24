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
- **Tracked Assets Verification & Refined `.gitignore`**:
  - In compliance with repository policy, no retail game assets or decoded video planes/frames are committed to version control.
  - `.gitignore` specifically excludes decoded-frame subdirectories (`/research/runs/native-movie-decode-*/**/decoded*/`) rather than entire run folders, ensuring run documentation (`RUN.md`, `ORACLE.md`, `COMPONENT_AUTHENTICATION.json`) remains trackable.
  - Video stream and plane rules are case-insensitive (`*.[sS][fF][dD]`, `*.[pP][lL][aA][nN][eE]*`, `*.[sS]16`, `*.[yY][uU][vV]`, `*.[rR][gG][bB][aA]`, `*.[rR][aA][wW]`).
  - `git check-ignore` explicitly proves evidence files (`RUN.md`, `ORACLE.md`) are trackable (not ignored), while decoded plane files and `.SFD` files are ignored.
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

### Binary-Separated Movie Tables (PS2 vs Xbox)

The retail binaries store distinct movie descriptor tables reflecting their native hardware targets:

#### 1. PS2 Movie Descriptor Table (`SLUS_214.10` at `0x005a2660`)
- **Table Size & Stride**: Exactly **29 entries** ($0 .. 28$) with a 40-byte stride ($0x28$).
- **Dimensions**: Full-screen video streams are authored at **640×448** (matching the native PS2 NTSC logical frame height, NOT 480). Bonus/promotional videos are authored at **320×240**.
- **PAL Suffix Rows**: European PAL variants appear as separate entries in this table (e.g. index 2 `TITLE6N`/`TITLE6W`, index 4 `OPENIN6N`, index 5 `OPENIN6W`, index 8 `ATTRAC6N`, index 9 `ATTRAC6W`, index 12 `QUAD6N`, index 13 `QUAD6W`).
- **Clarification on `movie_descriptors[4]`**: The retail startup attract state machine only loops through 4 primary movie indices ($0 .. 3$: LOGO, OPENING, TITLE, ATTRACT), explaining the earlier reference to a 4-movie startup set. The complete table has 29 entries.

| PS2 ID | Normal Stream | Normal Res | Widescreen Stream | Wide Res | Inferred PAL60 Normal | Inferred PAL60 Wide |
|---|---|---|---|---|---|---|
| 0 | `LOGON.SFD` | 640×448 | `LOGOW.SFD` | 640×448 | `LOGO6N.SFD` | `LOGO6W.SFD` |
| 1 | `TITLEN.SFD` | 640×448 | `TITLEW.SFD` | 640×448 | `nullptr` | `nullptr` |
| 2 | `TITLE6N.SFD` | 640×448 | `TITLE6W.SFD` | 640×448 | `nullptr` | `nullptr` |
| 3 | `OPENINGN.SFD` | 640×448 | `OPENINGW.SFD` | 640×448 | `nullptr` | `nullptr` |
| 4 | `OPENIN6N.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 5 | `OPENIN6W.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 6 | `ATTRACTN.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 7 | `ATTRACTW.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 8 | `ATTRAC6N.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 9 | `ATTRAC6W.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 10 | `QUADN.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 11 | `QUADW.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 12 | `QUAD6N.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 13 | `QUAD6W.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 14 | `STRANGN.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 15 | `STRANGW.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 16 | `SPYHUNN.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 17 | `SPYHUNW.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 18 | `BLITZN.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 19 | `BLITZW.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 20 | `V_SNDOUT.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 21 | `V_ERMACB.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 22 | `V_MOTORK.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 23 | `V_SCORP.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 24 | `V_RAIDON.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 25 | `V_SZCCKY.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 26 | `V_LOAD2.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 27 | `V_BLOOPR.SFD` | 320×240 | `nullptr` | 0×0 | `nullptr` | `nullptr` |
| 28 | `V_LOAD1.SFD` | 640×448 | `nullptr` | 0×0 | `nullptr` | `nullptr` |

#### 2. Xbox Movie Descriptor Table (`default.xbe` at `0x00579828`)
- **Table Size & Stride**: Exactly **33 entries** ($0 .. 32$) with a 40-byte stride ($0x28$).
- **Dimensions**: Full-screen video streams are authored at **640×480** (standard NTSC full raster), while widescreen streams are **704×480** (`LOGOW`, `QUADW`) or **736×480** (`TITLEW`, `OPENINGW`, `ATTRACTW`, `SPYHUNW`), or **736×544** (`STRANGW`, `BLITZW`).
- **Promotional Videos**: Entries $8 .. 32$ are 4:3 only (`V_*.SFD`) at 640×480, 320×240, and 160×128 (`V_FIN.SFD`).
- **PAL Fields**: Zeroed (`nullptr`) because video modes and timings are handled by dashboard hardware flags.
- **Disc Availability**: 32 of 33 movies exist on disc under `disc-files/MOVIEPS2/`; only index 31 (`V_FIN.SFD`, $160 \times 128$) is absent.

| Xbox ID | Normal Stream | Normal Res | Widescreen Stream | Wide Res | Disc Availability |
|---|---|---|---|---|---|
| 0 | `LOGON.SFD` | 640×480 | `LOGOW.SFD` | 704×480 | Present |
| 1 | `TITLEN.SFD` | 640×480 | `TITLEW.SFD` | 736×480 | Present |
| 2 | `OPENINGN.SFD` | 640×480 | `OPENINGW.SFD` | 736×480 | Present |
| 3 | `ATTRACTN.SFD` | 640×480 | `ATTRACTW.SFD` | 736×480 | Present |
| 4 | `QUADN.SFD` | 640×480 | `QUADW.SFD` | 704×480 | Present |
| 5 | `STRANGN.SFD` | 640×480 | `STRANGW.SFD` | 736×544 | Present |
| 6 | `SPYHUNN.SFD` | 640×480 | `SPYHUNW.SFD` | 736×480 | Present |
| 7 | `BLITZN.SFD` | 640×480 | `BLITZW.SFD` | 736×544 | Present |
| 8 | `V_SNDOUT.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 9 | `V_ERMACB.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 10 | `V_MOTORK.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 11 | `V_SCORP.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 12 | `V_RAIDON.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 13 | `V_SZCCKY.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 14 | `V_LOAD2.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 15 | `V_BLOOPR.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 16 | `V_LOAD1.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 17 | `V_SHINOK.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 18 | `V_SUBZER.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 19 | `V_KANO.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 20 | `V_MOTRBK.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 21 | `V_MOTRFT.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 22 | `V_BLOOP5.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 23 | `V_BLOOP4.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 24 | `V_BLOOP2.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 25 | `V_BLOOP3.SFD` | 320×240 | `nullptr` | 0×0 | Present |
| 26 | `V_KONQIN.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 27 | `V_MKDACC.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 28 | `V_BLUES.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 29 | `V_MKAKON.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 30 | `V_MKHAND.SFD` | 640×480 | `nullptr` | 0×0 | Present |
| 31 | `V_FIN.SFD` | 160×128 | `nullptr` | 0×0 | **Absent** on disc |
| 32 | `V_MOTOGI.SFD` | 640×480 | `nullptr` | 0×0 | Present |

#### 3. Inference Disclosure for PAL60 Explanation
- The interpretation of `6N` and `6W` as European PAL 60 Hz mode variants is an **inferred interpretation** based on Midway's historical PAL release nomenclature.
- In the NTSC-U `SLUS_214.10` binary, these strings are present in data tables, but the code and debug strings do not explicitly document their meaning.

### Xbox Video & Movie Function Reconciliation

Detailed disassembly of `default.xbe` reconciles the movie playback and selection subroutines:

1. **`0x0025d5b0`: Movie Variant Selector (`select_retail_movie_variant`)**:
   - Takes input `movie_id` at `[ebp + 8]`.
   - Clamps ID to $[0 .. 32]$: if `movie_id >= 33` or `< 0`, clamps to `0`.
   - Calls `0x0027c790` (`is_widescreen_active()`).
   - If widescreen is active AND descriptor offset `+0x0c` (`wide_name`) is non-null, returns pointer to wide filename (`char*`).
   - Otherwise returns pointer to standard 4:3 filename (`char*`) at `+0x00`.
2. **`0x0025d380`: Movie Playback Player (`play_movie`)**:
   - Takes input `movie_id` at `[ebp + 8]`.
   - Queries `0x0027c790` to select width and height from descriptor (`+0x10`/`+0x14` if widescreen, `+0x04`/`+0x08` if standard).
   - Calls `0x0025d5b0` passing `movie_id` to get the selected filename string.
   - Initializes movie stream playback engine and submits video frame decoding requests.
3. **`0x00028300`: Boot Menu 0 Handler (`begin_menu_18d450`)**:
   - Subsystem reset (`call 0x00027f00`).
   - Calls movie player: `push 0; call 0x0025d380`.
   - Synchronizes audio (`call 0x000a0b60`).
   - Initiates mode 0 transition: `push 0x27da0; push 0; call 0x0019ca30`.

### Xbox Counterpart for `begin_intro_18deb0`

Disassembly confirms the Xbox equivalent of PS2 `0x0018deb0` / `0x0018dd10`:
- **Xbox Routine**: **`0x00027be0`** (with subtitle quad setup helper **`0x00027c40`**).
- **Exact Call Flow Correspondence**:
  1. Calls subsystem reset `0x00027f00` (PS2 `0x0018d970`).
  2. Sets string index to 56 (`0x38`) via `mov dword ptr [0x831a54], 0x38` (PS2 `addiu $a0, $zero, 0x38`).
  3. Calls `0x00027c40` to configure subtitle art and quad layout:
     - Queries widescreen active via `0x0027c790`.
     - Centers subtitle horizontally based on display width: `(width - 640) / 2 - 64` (e.g. 256px on 720p).
     - Instantiates subtitle text quad via string renderer `0x0009dfb0`.
  4. Dispatches intro task handler via task vtable at offset `0x20` (`call dword ptr [edx + 0x20]`).

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

## 3. Option C (Partitioned) Mathematical Authority & Matrix Kernels

Option C (Partitioned): Rendering transforms (view, projection, model, raster) use native host IEEE 754 `float` math with documented tolerances. Gameplay-critical arithmetic is classified function-by-function based on Xbox x86 behavior or documented tolerance, with zero EE COP1 bit-exact emulation unless a specific retail oracle replay requires it.

## 4. Authentic Retail Presentation Order & Production Display Service

### Retail Startup State Table (`0x005a3510` PS2 / `0x00548990` Xbox)
The complete 22-slot dispatch table matches 1:1 between PS2 and Xbox:
- Slot 0: `begin_menu_18d450` (Movie 0: `LOGON`) $\leftrightarrow$ Xbox `0x00028300`
- Slot 1: `begin_menu_18d870` (Movie 2: `OPENINGN`) $\leftrightarrow$ Xbox `0x00028090`
- Slot 2: `begin_menu_18d8e0` (Movie 1: `TITLEN`) $\leftrightarrow$ Xbox `0x00028000`
- Slot 3: `begin_menu_18d700` (Menu options mode) $\leftrightarrow$ Xbox `0x000281c0`
- Slot 4: `begin_menu_18d800` (Movie 3: `ATTRACTN`) $\leftrightarrow$ Xbox `0x00028110`
- Slot 5: `begin_menu_18d660` (Kombat demo mode) $\leftrightarrow$ Xbox `0x00028210`
- Slot 6: `begin_menu_18d770` (Movie 4: `QUADN`) $\leftrightarrow$ Xbox `0x00028170`
- Slots 7..21: Repeated attract cycle through slots 2..6.

### Production Display Interface (1280x720 Primary Target)
Implemented in `runtime/src/native/retail_display_service.cpp` (`mka::retail`):
- `NativeProductionDisplayService` defines the production display contract: context lease, aspect ratio conversion, virtual swapchain, and telemetry.
- `NativeOffscreenDisplayService` implements headless offscreen rendering via invisible WGL double-buffered context.
- Target configuration: primary Xbox HD target $1280 \times 720$ (16:9), with $640 \times 448$ preserved solely as named PS2 reference mode.

## 5. First-Scene Bootstrap Continuation & Recovered Initial Task Implementations

### Implemented Recovered Routines & Disclosed Remainder

Implemented in `runtime/src/native/retail_initial_movie_path.cpp`:

1. **`begin_menu_18d450` (PS2 `0x0018d450` $\leftrightarrow$ Xbox `0x00028300`)**:
   - Recovered flow: Subsystem reset, movie 0 variant selection via `select_retail_movie_variant(0, widescreen)`, audio sync, and mode 0 transition via `transition_1225d0`.
   - True recovered: Variant selection, display service presentation binding, audio sync call structure, and mode 0 transition.
   - What remains unrecovered: Multi-frame streaming decode loops across multi-second playback (currently offscreen frame 0 presentation is authenticated).
2. **`transition_1225d0` (PS2 `0x001225d0` $\leftrightarrow$ Xbox `0x0019ca30`)**:
   - Recovered flow: Executes state exit handler at `[eax + 0x18]` if active task context exists in session / mode state, pushes transition mode into `main.mode_history`, creates new mode task via task registry.
   - What remains unrecovered: Concrete game loop execution bodies for complex interactive game modes.
3. **`begin_intro_18deb0` (PS2 `0x0018deb0` / `0x0018dd10` $\leftrightarrow$ Xbox `0x00027be0` / `0x00027c40`)**:
   - Recovered flow: Subsystem reset, sets string index to 56 (`0x38`), centers subtitle art quad horizontally: `(width - 640) / 2 - 64`, dispatches intro task handler via `[edx + 0x20]`.
   - What remains unrecovered: Live ROM font glyph rasterization during interactive playback.
4. **`begin_menu_18d870` (PS2 `0x0018d870` $\leftrightarrow$ Xbox `0x00028090`)**:
   - Recovered flow: Plays movie 2 (`OPENINGN`/`OPENINGW`), audio sync, transitions to mode 0.
5. **`begin_menu_18d8e0` (PS2 `0x0018d8e0` $\leftrightarrow$ Xbox `0x00028000`)**:
   - Recovered flow: Plays movie 1 (`TITLEN`/`TITLEW`), falls back to prompt if absent, audio sync, transitions to mode 0.
6. **`begin_menu_18d800` (PS2 `0x0018d800` $\leftrightarrow$ Xbox `0x00028110`)**:
   - Recovered flow: Plays movie 3 (`ATTRACTN`/`ATTRACTW`), audio sync, transitions to mode 0.
7. **`begin_menu_18d770` (PS2 `0x0018d770` $\leftrightarrow$ Xbox `0x00028170`)**:
   - Recovered flow: Plays movie 4 (`QUADN`/`QUADW`), audio sync, transitions to mode 0.
8. **`begin_menu_18d700` (PS2 `0x0018d700` $\leftrightarrow$ Xbox `0x000281c0`)**:
   - Recovered flow: Options menu mode task initialization and mode state transition.
   - What remains unrecovered: Interactive menu widget navigation and input event loop.
9. **`begin_menu_18d660` (PS2 `0x0018d660` $\leftrightarrow$ Xbox `0x00028210`)**:
   - Recovered flow: Kombat demo mode task initialization and demo fight profile index cycling.
   - What remains unrecovered: Fight engine AI simulation and character state playback.
10. **`begin_prompt_18dc10` (PS2 `0x0018dc10` $\leftrightarrow$ Xbox `0x00028350`)**:
    - Recovered flow: Subsystem reset, prompt text quad centering, 150-frame timeout, prompt task dispatch.
    - What remains unrecovered: Live gamepad button event polling.
11. **`clear_nonowning_references_18da30` (PS2 `0x0018da30` $\leftrightarrow$ Xbox `0x00027df0`)**:
    - Recovered flow: Subsystem cleanup clearing nonowning task and scene references prior to mode transition.
12. **`render_retail_frame` (PS2 `0x00126940` and Xbox main loop)**:
    - Display service context presentation and frame enablement validation.
    - What remains unrecovered: Whole 3D scene raster submission.

## 6. Undefined Symbol Count Diagnostic & Ordered Queue

The negative link symbol diagnostic currently contains **12 undefined symbols** (representing missing link definitions satisfied during diagnostic linking; the reduction from 20 to 12 via resolution of Priority Queue 2 is recorded strictly as an ordered link diagnostic, NOT recovered progress):

- **Priority Queue 1: Retail Movie Presentation Path (0 symbols)**: Fully resolved at link-diagnostic level.
- **Priority Queue 2: Initial Task & Reset Symbols (0 symbols)**: Fully resolved at link-diagnostic level (all 8 routines implemented from recovered call flows).
- **Priority Queue 3: Remaining Main-Binding Dispatch Symbols (9 symbols)**:
  1. `mka::retail::main_binding::advance_retail_game_services`
  2. `mka::retail::main_binding::create_retail_action_task`
  3. `mka::retail::main_binding::deliver_retail_builtin_scene_event`
  4. `mka::retail::main_binding::dispatch_retail_enabled_mode`
  5. `mka::retail::main_binding::poll_retail_controller_roles`
  6. `mka::retail::main_binding::prune_retail_associations`
  7. `mka::retail::main_binding::reset_and_bind_retail_scene_event_graph`
  8. `mka::retail::main_binding::retail_mode_dispatch_enabled`
  9. `mka::retail::main_binding::service_retail_frame_input`
- **Priority Queue 4: Profile Registration Symbols (3 symbols)**:
  10. `mka::retail::apply_retail_profile_participant_from_188720`
  11. `mka::retail::invoke_retail_profile_select_from_2f1630`
  12. `mka::retail::publish_retail_profile_participant_from_189690`

## 7. Acceptance & Verification Commitments

- **Zero Visible Windows / Audio Endpoints / Owner Launch**: In strict adherence to `TASK_BOARD.md` row 10, presentation is hosted entirely in an offscreen double-buffered WGL target (`NativeOffscreenDisplayService`). No desktop window, audible sound stream, or owner game loop is opened.
- **Strict Accounting**:
  - PS2 function ledger: strictly 238 / 15,670 (1.52%).
  - Xbox donor ledger: strictly 0 / 21,321 (0.00%).
  - Acceptance status: strictly **OPEN**.
- **No Overclaiming**: Verified offscreen presentation of `LOGOW.SFD` frame 0 demonstrates the video display interface and texture upload pipeline, but does not constitute 3D gameplay geometry rendering or whole-game acceptance.
- **Single Runner**: `tools/research/run_check.py` is the sole canonical runner; `run_14_tests.py` has been deleted via `git rm`.
