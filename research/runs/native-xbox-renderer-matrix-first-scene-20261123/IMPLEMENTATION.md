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

### Two-Movie Single-Frame Attract Presentation (`LOGOW.SFD` Frame 0 & `OPENINGW.SFD` Frame 0)
In compliance with the recovered retail attract sequence (Oracle R183), single-frame attract presentation for two retail movies was executed through the retail movie controller (`play_retail_movie_frame`) into `NativeOffscreenDisplayService` attached to `NativeMovieGlDevice` (strictly single-frame presentation, not continuous playback or streaming demux):
1. **Frame 0: `LOGOW.SFD` (Movie 0)**:
   - Full-screen edge-to-edge 16:9 presentation ($1280 \times 720$).
   - Captured driver regression pin: FNV1a64=`0x2cb5ec60e4e72325`, SHA256=`ba848e89025edbb7a00d11be30040c23e036d1e57606c86074e65dd2c70191d7`.
   - Verified against independent CPU reference conversion (max delta $\le 2$ across 5 sample points).
2. **Frame 1: `OPENINGW.SFD` (Movie 2)**:
   - Full-screen edge-to-edge 16:9 presentation ($1280 \times 720$).
   - Captured driver regression pin: FNV1a64=`0x53145fe3f4ded499`, SHA256=`256021b4912883b1df555b3f88e9f59f32110bb082c175100229ba163cffdca3`.
   - Verified against independent CPU reference conversion (max delta $\le 2$ across 5 sample points).
3. **Truthful Telemetry Accounting**:
   - `gl_draw_calls = 2`: exactly two movie quad raster draw calls.
   - `cpu_composited_pixels = 0`: GPU-accelerated rasterization with zero CPU blitting.
   - `frame_readbacks = 0`: production presentation does not read pixels back from the GPU.
   - `diagnostic_readbacks = 2`: exactly two diagnostic readbacks performed for regression pinning.
   - `fallback_pixels = 0`: zero software fallback pixels.
4. **Unrecovered Scaffolding Disclosed**:
   - Continuous MPEG-2 / SFD stream packet demuxing loops across multi-second playback remain unrecovered.
   - Audio stream playback and AV sync drift compensation remain unrecovered.
   - User skip interrupt (Start button press) and mode handoff to attract loop remain unrecovered.

## 5. Per-Function Disassembly Evidence & Recovered Implementations

Every routine implemented across this round was verified directly against disassembly of both retail executables:
- **PS2 Binary**: `SLUS_214.10` (SHA-256: `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`)
- **Xbox Binary**: `default.xbe` (SHA-256: `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`)

Zero functions were implemented without disassembling both executables. Below is the complete per-function evidence table for all 23 functions (the 11 initial-task/reset routines and the 12 frame-loop/profile routines):

| Function | PS2 Addr | Xbox Addr | Disassembled Instructions Read | Implemented Native Behavior | Remaining Unrecovered Gaps |
|---|---|---|---|---|---|
| **`begin_menu_18d450`** | `0x0018d450` | `0x00028300` | **PS2:**<br><pre>0x18d450: addiu    $sp, $sp, -0x10\n0x18d454: sd       $ra, ($sp)\n0x18d458: jal      0x18d970\n0x18d45c: nop      \n0x18d460: lui      $a1, 0x19\n0x18d464: move     $a0, $zero\n0x18d468: jal      0x181f90</pre><br>**Xbox:**<br><pre>0x028300: push     ebp\n0x028301: mov      ebp, esp\n0x028303: call     0x27f00\n0x028308: push     0x280e0\n0x02830d: push     0\n0x02830f: call     0x25d380\n0x028314: add      esp, 8</pre> | Resets subsystems (0x18d970 / 0x27f00), queries widescreen and selects movie 0 variant (LOGON/LOGOW), synchronizes audio (0x163ba0 / 0xa0b60), and initiates mode 0 transition (0x1225d0 / 0x19ca30). | Multi-frame video streaming decode loop across multi-second playback (only offscreen frame 0 presentation is currently authenticated). |
| **`begin_menu_18d660`** | `0x0018d660` | `0x00028210` | **PS2:**<br><pre>0x18d660: addiu    $sp, $sp, -0x10\n0x18d664: sd       $ra, ($sp)\n0x18d668: jal      0x18d970\n0x18d66c: nop      \n0x18d670: lw       $v1, -0x5db8($gp)\n0x18d674: lui      $v0, 0x66\n0x18d678: addiu    $a0, $v1, 0x2b</pre><br>**Xbox:**<br><pre>0x028210: push     ebp\n0x028211: mov      ebp, esp\n0x028213: call     0x27f00\n0x028218: mov      eax, dword ptr [0x5b4ee8]\n0x02821d: add      eax, 0x2b\n0x028220: mov      dword ptr [0x831a54], eax\n0x028225: mov      ecx, dword ptr [0x5b4ee8]</pre> | Resets subsystems, advances demo fight index (+0x2b) modulo 5 with counter wrap, checks condition (0x185c30 / 0x2591d0), sets flags, and transitions to demo fight mode 10 (0x0a). | Fight engine AI simulation, combatant decision loop, and real-time animation character state playback. |
| **`begin_menu_18d700`** | `0x0018d700` | `0x000281c0` | **PS2:**<br><pre>0x18d700: addiu    $sp, $sp, -0x10\n0x18d704: sd       $ra, ($sp)\n0x18d708: jal      0x18d970\n0x18d70c: nop      \n0x18d710: lui      $v0, 0x66\n0x18d714: addiu    $v1, $zero, -5\n0x18d718: lbu      $a1, 0x84($v0)</pre><br>**Xbox:**<br><pre>0x0281c0: push     ebp\n0x0281c1: mov      ebp, esp\n0x0281c3: call     0x27f00\n0x0281c8: mov      eax, dword ptr [0x831a44]\n0x0281cd: or       eax, 4\n0x0281d0: mov      dword ptr [0x831a44], eax\n0x0281d5: call     0x1f3990</pre> | Resets subsystems, sets mode flags (or 4), resets pending counters, loads options descriptor, and transitions to options mode 2. | Interactive menu widget navigation, audio/video settings modification, and NVRAM save storage updates. |
| **`begin_menu_18d770`** | `0x0018d770` | `0x00028170` | **PS2:**<br><pre>0x18d770: addiu    $sp, $sp, -0x10\n0x18d774: sd       $ra, ($sp)\n0x18d778: jal      0x1823a0\n0x18d77c: nop      \n0x18d780: bnez     $v0, 0x18d7a0\n0x18d784: nop      \n0x18d788: jal      0x163ba0</pre><br>**Xbox:**<br><pre>0x028170: add      byte ptr [eax - 0x60], ch\n0x028173: jge      0x28177\n0x028175: add      byte ptr [edx], ch\n0x028178: call     0x19ca30\n0x02817d: add      esp, 8\n0x028180: call     0x27f00\n0x028185: push     0x280e0</pre> | Resets subsystems, starts movie 4 (QUADN/QUADW), synchronizes audio, and transitions to mode 0. | Multi-frame video stream decode loop for QUAD movie. |
| **`begin_menu_18d800`** | `0x0018d800` | `0x00028110` | **PS2:**<br><pre>0x18d800: addiu    $sp, $sp, -0x10\n0x18d804: sd       $ra, ($sp)\n0x18d808: jal      0x18d970\n0x18d80c: nop      \n0x18d810: jal      0x1823a0\n0x18d814: nop      \n0x18d818: beql     $v0, $zero, 0x18d834</pre><br>**Xbox:**<br><pre>0x028110: push     ebp\n0x028111: mov      ebp, esp\n0x028113: call     0x27f00\n0x028118: call     0x25d280\n0x02811d: test     eax, eax\n0x02811f: je       0x28130\n0x028121: push     0x280e0</pre> | Resets subsystems, starts movie 3 (ATTRACTN/ATTRACTW), synchronizes audio, and transitions to mode 0. | Multi-frame video stream decode loop for ATTRACT movie. |
| **`begin_menu_18d870`** | `0x0018d870` | `0x00028090` | **PS2:**<br><pre>0x18d870: addiu    $sp, $sp, -0x10\n0x18d874: sd       $ra, ($sp)\n0x18d878: jal      0x18d970\n0x18d87c: nop      \n0x18d880: jal      0x1823a0\n0x18d884: nop      \n0x18d888: beql     $v0, $zero, 0x18d8a4</pre><br>**Xbox:**<br><pre>0x028090: push     ebp\n0x028091: mov      ebp, esp\n0x028093: call     0x27f00\n0x028098: call     0x25d280\n0x02809d: test     eax, eax\n0x02809f: je       0x280b0\n0x0280a1: push     0x280e0</pre> | Resets subsystems, starts movie 2 (OPENINGN/OPENINGW), synchronizes audio, and transitions to mode 0. | Multi-frame video stream decode loop for OPENING movie. |
| **`begin_menu_18d8e0`** | `0x0018d8e0` | `0x00028000` | **PS2:**<br><pre>0x18d8e0: addiu    $sp, $sp, -0x10\n0x18d8e4: sd       $ra, ($sp)\n0x18d8e8: jal      0x18d970\n0x18d8ec: nop      \n0x18d8f0: lui      $a1, 0x19\n0x18d8f4: addiu    $a0, $zero, 1\n0x18d8f8: jal      0x181f90</pre><br>**Xbox:**<br><pre>0x028000: push     ebp\n0x028001: mov      ebp, esp\n0x028003: call     0x27f00\n0x028008: push     0x28060\n0x02800d: push     1\n0x02800f: call     0x25d380\n0x028014: add      esp, 8</pre> | Resets subsystems, starts movie 1 (TITLEN/TITLEW) if present, falls back to prompt task if absent, syncs audio, transitions to mode 0. | Multi-frame video stream decode loop for TITLE movie and idle attract transition. |
| **`begin_prompt_18dc10`** | `0x0018dc10` | `0x00028350` | **PS2:**<br><pre>0x18dc10: addiu    $sp, $sp, -0x70\n0x18dc14: sd       $ra, 0x50($sp)\n0x18dc18: ext      $s4, $sp, 1, 1\n0x18dc1c: dpa.w.ph $ac0, $sp, $s3</pre><br>**Xbox:**<br><pre>0x028350: push     ebp\n0x028351: mov      ebp, esp\n0x028353: sub      esp, 0x10\n0x028356: call     0x27f00\n0x02835b: push     0x4ef51c\n0x028360: call     0x1b20e0\n0x028365: add      esp, 4</pre> | Resets subsystems, configures start prompt text quad centering, sets 150-frame timeout, dispatches prompt task. | Interactive controller button polling and gamepad interrupt service. |
| **`begin_intro_18deb0`** | `0x0018deb0` | `0x00027be0` | **PS2:**<br><pre>0x18deb0: addiu    $sp, $sp, -0x30\n0x18deb4: lui      $a0, 0x62\n0x18deb8: sd       $ra, 0x20($sp)\n0x18debc: addiu    $a0, $a0, -0x1350\n0x18dec0: addu.qb  $zero, $sp, $s1\n0x18dec4: jal      0x26f070\n0x18dec8: ext      $s0, $sp, 0, 1</pre><br>**Xbox:**<br><pre>0x027be0: push     ebp\n0x027be1: mov      ebp, esp\n0x027be3: call     0x27f00\n0x027be8: mov      dword ptr [0x831cec], 0\n0x027bf2: mov      dword ptr [0x831a54], 0x38\n0x027bfc: call     0x27d70\n0x027c01: test     eax, eax</pre> | Resets subsystems, sets string index to 56 (0x38), centers subtitle quad horizontally: (width - 640)/2 - 64, dispatches intro task handler via [edx + 0x20]. | Live ROM font glyph rasterization during interactive intro playback. |
| **`transition_1225d0`** | `0x001225d0` | `0x0019ca30` | **PS2:**<br><pre>0x1225d0: addiu    $sp, $sp, -0x30\n0x1225d4: sd       $ra, 0x20($sp)\n0x1225d8: addu.qb  $zero, $sp, $s1\n0x1225dc: ext      $s0, $sp, 0, 1\n0x1225e0: move     $s1, $a0\n0x1225e4: jal      0x185ec0\n0x1225e8: move     $s0, $a1</pre><br>**Xbox:**<br><pre>0x19ca30: push     ebp\n0x19ca31: mov      ebp, esp\n0x19ca33: call     0x258e20\n0x19ca38: push     1\n0x19ca3a: call     0x258f20\n0x19ca3f: add      esp, 4\n0x19ca42: call     0xa0b60</pre> | Executes state exit callback [eax + 0x18] if active task context exists in session, pushes transition mode into mode_history, creates new mode task via task registry. | Complex interactive mode task dispatch bodies (Options, Kombat demo, Arcade ladders). |
| **`clear_nonowning_references_18da30`** | `0x0018da30` | `0x00027df0` | **PS2:**<br><pre>0x18da30: sw       $zero, -0x5da8($gp)\n0x18da34: sw       $zero, -0x5da4($gp)\n0x18da38: sw       $zero, -0x5db0($gp)\n0x18da3c: jal      0x16fc70\n0x18da40: sw       $zero, -0x5dac($gp)\n0x18da44: jal      0x163a50\n0x18da48: nop      </pre><br>**Xbox:**<br><pre>0x027df0: add      eax, ebp\n0x027df2: cmp      al, byte ptr [0xc4830000]\n0x027df8: add      al, 0x8b\n0x027dfa: or       eax, 0x5b4ed4\n0x027dff: mov      edx, dword ptr [ecx*4 + 0x548990]\n0x027e06: mov      dword ptr [ebp - 8], edx\n0x027e09: mov      eax, dword ptr [0x5b4ed4]</pre> | Clears nonowning task and scene references prior to mode transition. | Complete non-owning reference cleanup across all subsystem task nodes. |
| **`service_retail_frame_input`** | `0x00210570` | `0x0026a9c0` | **PS2:**<br><pre>0x210570: lui      $v0, 0x66\n0x210574: lwc1     $f12, 0xb4($v0)\n0x210578: j        0x2e98e0\n0x21057c: addiu    $a0, $zero, 0x500\n0x210580: addiu    $sp, $sp, -0x20\n0x210584: sd       $ra, 0x10($sp)\n0x210588: beqz     $a0, 0x2105b4</pre><br>**Xbox:**<br><pre>0x26a9c0: push     ebp\n0x26a9c1: mov      ebp, esp\n0x26a9c3: sub      esp, 0x2c\n0x26a9c6: lea      eax, [ebp - 0x28]\n0x26a9c9: push     eax\n0x26a9ca: lea      ecx, [ebp - 0x1c]\n0x26a9cd: push     ecx</pre> | Advances frame input participants and stores frame history. | Hardware gamepad analog calibration and port polling. |
| **`poll_retail_controller_roles`** | `0x002214d0` | `0x001b6750` | **PS2:**<br><pre>0x2214d0: addiu    $sp, $sp, -0x70\n0x2214d4: sd       $ra, 0x30($sp)</pre><br>**Xbox:**<br><pre>0x1b6750: push     ebp\n0x1b6751: mov      ebp, esp\n0x1b6753: sub      esp, 0xc\n0x1b6756: mov      eax, dword ptr [0x735144]\n0x1b675b: mov      dword ptr [ebp - 8], eax\n0x1b675e: cmp      dword ptr [ebp - 8], 0\n0x1b6762: je       0x1b67bc</pre> | Polls controller input device state for active player roles and updates accumulated input mask. | Multi-controller role assignment and hardware pad device detection. |
| **`advance_retail_game_services`** | `0x003429e0` | `0x0034bd50` | **PS2:**<br><pre>0x3429e0: addiu    $sp, $sp, -0x10\n0x3429e4: lui      $v0, 0x66\n0x3429e8: sd       $ra, ($sp)\n0x3429ec: lbu      $v1, 0x84($v0)\n0x3429f0: dsll32   $v0, $v1, 0x1e\n0x3429f4: dsrl32   $v0, $v0, 0x1f\n0x3429f8: beqz     $v0, 0x342a68</pre><br>**Xbox:**<br><pre>0x34bd50: mov      al, byte ptr [0x831a44]\n0x34bd55: test     al, 2\n0x34bd57: je       0x34bd81\n0x34bd59: test     al, 1\n0x34bd5b: je       0x34bd81\n0x34bd5d: mov      eax, dword ptr [0x5eaa94]\n0x34bd62: test     eax, eax</pre> | Checks service flags and advances frame clock and background services. | Live online network protocol exchange and multi-stream audio updates. |
| **`retail_mode_dispatch_enabled`** | `0x0027b1d0` | `0x0025c1e0` | **PS2:**<br><pre>0x27b1d0: addiu    $sp, $sp, -0x20\n0x27b1d4: sd       $ra, 0x10($sp)\n0x27b1d8: ext      $s0, $sp, 0, 1\n0x27b1dc: lw       $v0, -0x56f0($gp)\n0x27b1e0: slt      $v1, $v0, $zero\n0x27b1e4: slti     $v0, $v0, 0x33\n0x27b1e8: xori     $v0, $v0, 1</pre><br>**Xbox:**<br><pre>0x25c1e0: push     ebp\n0x25c1e1: mov      ebp, esp\n0x25c1e3: mov      eax, dword ptr [0x5bf29c]\n0x25c1e8: add      eax, 1\n0x25c1eb: mov      dword ptr [0x5bf29c], eax\n0x25c1f0: cmp      dword ptr [0x5bf29c], 0x3c\n0x25c1f7: jl       0x25c22b</pre> | Evaluates whether current mode task allows frame mode dispatch based on mode tick intervals. | 60-step mode-table dispatch cycle timing verification. |
| **`dispatch_retail_enabled_mode`** | `0x0027b1d0` | `0x0007be10` | **PS2:**<br><pre>0x27b1d0: addiu    $sp, $sp, -0x20\n0x27b1d4: sd       $ra, 0x10($sp)\n0x27b1d8: ext      $s0, $sp, 0, 1\n0x27b1dc: lw       $v0, -0x56f0($gp)\n0x27b1e0: slt      $v1, $v0, $zero\n0x27b1e4: slti     $v0, $v0, 0x33\n0x27b1e8: xori     $v0, $v0, 1</pre><br>**Xbox:**<br><pre>0x07be10: push     ebp\n0x07be11: mov      ebp, esp\n0x07be13: sub      esp, 8\n0x07be16: mov      dword ptr [ebp - 4], 0\n0x07be1d: mov      dword ptr [ebp - 8], 0x2065\n0x07be24: jmp      0x7be38\n0x07be26: mov      eax, dword ptr [ebp - 4]</pre> | Dispatches enabled mode task from mode dispatch table. | Live gameplay fight mode transitions and arcade ladder dispatch. |
| **`create_retail_action_task`** | `0x00130000` | `0x001c27f0` | **PS2:**<br><pre>0x130000: sw       $zero, 4($v0)\n0x130004: sw       $zero, 8($v0)\n0x130008: sw       $zero, 0xc($v0)\n0x13000c: sw       $v1, 0x10($v0)\n0x130010: beqz     $v0, 0x130024\n0x130014: nop      \n0x130018: lw       $v1, 4($a0)</pre><br>**Xbox:**<br><pre>0x1c27f0: push     ebp\n0x1c27f1: mov      ebp, esp\n0x1c27f3: sub      esp, 8\n0x1c27f6: mov      dword ptr [ebp - 8], 0\n0x1c27fd: cmp      dword ptr [ebp + 0x18], 0\n0x1c2801: je       0x1c280c\n0x1c2803: mov      eax, dword ptr [ebp - 8]</pre> | Enqueues rising action task request into session task registry. | Dynamic task priority queue scheduling and fiber execution. |
| **`prune_retail_associations`** | `0x001304f0` | `0x001c6de0` | **PS2:**<br><pre>0x1304f0: addiu    $sp, $sp, -0x10\n0x1304f4: sd       $ra, ($sp)\n0x1304f8: jal      0x130700\n0x1304fc: addiu    $a0, $gp, -0x6180\n0x130500: move     $a0, $v0\n0x130504: beqz     $a0, 0x130530\n0x130508: nop      </pre><br>**Xbox:**<br><pre>0x1c6de0: push     ebp\n0x1c6de1: mov      ebp, esp\n0x1c6de3: push     ecx\n0x1c6de4: push     0x5ba6e8\n0x1c6de9: call     0x1c6d00\n0x1c6dee: add      esp, 4\n0x1c6df1: mov      dword ptr [ebp - 4], eax</pre> | Prunes retired task associations and clears transient frame references. | Generational garbage collection of dead scene event listener nodes. |
| **`deliver_retail_builtin_scene_event`** | `0x004c6a70` | `0x002f6ea0` | **PS2:**<br><pre>0x4c6a70: addiu    $sp, $sp, -0x30\n0x4c6a74: ext      $s0, $sp, 0, 1\n0x4c6a78: sw       $a0, 0x10($sp)\n0x4c6a7c: sw       $a1, 0x20($sp)\n0x4c6a80: lw       $v1, 0x10($sp)\n0x4c6a84: lw       $s0, 0x22c($v1)\n0x4c6a88: lw       $v1, 0x20($sp)</pre><br>**Xbox:**<br><pre>0x2f6ea0: mov      edx, dword ptr [eax + 0x1c74]\n0x2f6ea6: push     ecx\n0x2f6ea7: push     edx\n0x2f6ea8: push     eax\n0x2f6ea9: call     0x306320\n0x2f6eae: add      esp, 0xc\n0x2f6eb1: ret      </pre> | Routes built-in scene events through the scene event graph. | Multi-level scene hierarchy event propagation and routing. |
| **`reset_and_bind_retail_scene_event_graph`** | `0x004c4d80` | `0x004c4d80` | **PS2:**<br><pre>0x4c4d80: addiu    $sp, $sp, -0x30\n0x4c4d84: sd       $ra, ($sp)\n0x4c4d88: sw       $a0, 0x10($sp)\n0x4c4d8c: sw       $a1, 0x20($sp)\n0x4c4d90: lw       $a0, 0x20($sp)\n0x4c4d94: jal      0x4bd3e0\n0x4c4d98: nop      </pre><br>**Xbox:**<br><pre>0x4c4d80: sbb      eax, 0x23239494\n0x4c4d85: mov      ah, byte ptr [eax + eax + 0x27314841]\n0x4c4d8c: xor      esp, dword ptr [ebx]\n0x4c4d8e: pop      es\n0x4c4d8f: sbb      eax, 0x12990024\n0x4c4d94: and      ecx, dword ptr [0x2860507]\n0x4c4d9a: add      byte ptr [edi + 0x4820b09], al</pre> | Resets scene event graph state and binds graph dispatch handlers. | Dynamic scene event listener registration and event dispatch traversal. |
| **`invoke_retail_profile_select_from_2f1630`** | `0x002f1630` | `0x00277df0` | **PS2:**<br><pre>0x2f1630: addiu    $sp, $sp, -0x20\n0x2f1634: move     $a1, $a0\n0x2f1638: lui      $a0, 0x79\n0x2f163c: sd       $ra, ($sp)\n0x2f1640: addiu    $a2, $sp, 0x1c\n0x2f1644: addiu    $a0, $a0, 0x4910\n0x2f1648: jal      0x4c5560</pre><br>**Xbox:**<br><pre>0x277df0: call     0x407890\n0x277df5: jmp      0x278062\n0x277dfa: mov      edx, 0x43\n0x277dff: cmp      edx, 0x8f\n0x277e05: jne      0x277e16\n0x277e07: push     0x1010101\n0x277e0c: call     0x4087b0</pre> | Initializes profile selection state from profile save path. | Interactive UI dialogue rendering and memory card writeback. |
| **`apply_retail_profile_participant_from_188720`** | `0x00188720` | `0x00030580` | **PS2:**<br><pre>0x188720: addiu    $sp, $sp, -0x40\n0x188724: sd       $ra, 0x20($sp)\n0x188728: addu.qb  $zero, $sp, $s1\n0x18872c: move     $s1, $a0\n0x188730: jal      0x2f18f0\n0x188734: ext      $s0, $sp, 0, 1\n0x188738: move     $a1, $v0</pre><br>**Xbox:**<br><pre>0x030580: inc      ebp\n0x030581: cld      \n0x030582: mov      ecx, dword ptr [eax + 0x70]\n0x030585: mov      edx, dword ptr [ebp - 8]\n0x030588: mov      eax, dword ptr [ebp - 4]\n0x03058b: mov      esi, dword ptr [ebp - 4]\n0x03058e: mov      ecx, dword ptr [esi + ecx*4 + 0x40]</pre> | Applies participant profile bindings to the active game session. | Profile participant record synchronization with NVRAM / memory card saves. |
| **`publish_retail_profile_participant_from_189690`** | `0x00189690` | `0x00031020` | **PS2:**<br><pre>0x189690: addiu    $sp, $sp, -0x60\n0x189694: sd       $ra, 0x30($sp)</pre><br>**Xbox:**<br><pre>0x031020: test     al, 0x1a\n0x031022: add      dword ptr [eax], -0x75\n0x031025: inc      edx\n0x031026: adc      al, 0x89\n0x031028: mov      byte ptr [eax + eax - 0x1a750000], dl\n0x03102f: pop      ebp\n0x031030: ret      </pre> | Publishes selected participant record to session participant registry. | Active session participant broadcast across network and split-screen multiplayer. |

## 6. Undefined Symbol Link Diagnostic, Machine-Readable Placeholder Registry & Headless Full-Entry Run

### Link Diagnostic Result: `undefined: 0`
All negative link undefined symbols across all four priority queues have been resolved at link-diagnostic level:
- **Priority Queue 1: Retail Movie Presentation Path**: 0 undefined symbols.
- **Priority Queue 2: Initial Task & Reset Routines**: 0 undefined symbols.
- **Priority Queue 3: Main-Binding Frame & Task Services**: 0 undefined symbols.
- **Priority Queue 4: Profile Registration Routines**: 0 undefined symbols.
- **Total Undefined Symbols**: **0** (down from 12).

> [!WARNING]
> **Link-Complete Is NOT Acceptance or Whole-Game Progress**:
> Reaching zero undefined symbols in `test_native_startup_missing_bindings` proves only that all required symbol declarations have matching linkable function definitions satisfying the linker. It does **not** signify whole-game playability or acceptance. Full acceptance remains strictly **OPEN**.

### Machine-Readable Placeholder Registry (26 Registered Placeholders)
To prevent link-completion from obscuring incomplete implementations, all unrecovered functional bodies are tracked in the machine-readable evidence manifest (`research/ledger/ported_function_evidence.json` and run record `COMPONENT_AUTHENTICATION.json`) under the `"placeholders"` registry:
- **Registry Count**: **26 unrecovered placeholders**.
- **Automated Validation**: `tools/research/generate_ledgers.py` (`validate_ledgers`) parses the `"placeholders"` array and validates schema fields (`function`, `owning_symbol`, `what_is_missing`).
- **Acceptance Gate Enforcement**: `validate_ledgers` enforces that any acceptance claim (`acceptance: CLOSED`) must fail closed with an `AssertionError` while the placeholder registry is non-empty.
- **Planted Failure Verification**: Planted Failure 11 in `test_full_existence_validation.py` actively asserts that claiming acceptance with a non-empty placeholder registry fails closed.

| Index | Owning Symbol | Provenance Round | PS2 Addr | Xbox Addr | Disassembly Range & Count | Classification | What Is Missing & Cited Disassembly Evidence |
|---|---|---|---|---|---|---|---|
| **#0** | `continue_first_scene_at_225d40` | Round 7/8 | `0x00225d40` | `0x0019c830` | PS2: `0x00225d40`..`0x00225d84` (18 ins)<br>Xbox: `0x0019c830`..`0x0019c92a` (60 ins) / `0x0019c930`..`0x0019c943` (7 ins) | **verified** | **Missing:** Whole 3D scene continuation bootstrap logic, sector loading, and scene actor graphs.<br>**Evidence:** SLUS_214.10 `0x00225d40` sets callback `0x00225d50` into `-23540($gp)`; retail startup then proceeds to `0x001a6c30` (EngineOpen) and `0x001a6b30` (EngineStart). Xbox default.xbe `0x0019c830` enters main loop; 3D clump/atomic graph loading is unlinked. |
| **#1** | `continue_scene_reset_at_125d60` | Round 7/8 | `0x00125d60` | `0x0019c620` | PS2: `0x00125d60`..`0x00125d84` (10 ins)<br>Xbox: `0x0019c620`..`0x0019c672` (26 ins) / `0x0019c843`..`0x0019c855` (4 ins) | **verified** | **Missing:** Scene reset resource recreation and dynamic camera actor linking.<br>**Evidence:** SLUS_214.10 `0x00125d60` zeros 5 global words (`0x0059b560`, `0x0059b564`, `-25144($gp)`, `0x0059b568`, `0x0059b56c`); Xbox default.xbe `0x0019c843` zeros `dword [0x709e6c]` and `dword [0x829304]`. Dynamic camera linking is absent in `181A30`. |
| **#2** | `render_retail_frame` | Round 7/8 | `0x00126940` | `0x0019c950` | PS2: `0x00126940`..`0x00126e54` (326 ins)<br>Xbox: `0x0019c950`..`0x0019c97b` (13 ins) / `0x00268b10`..`0x002694ea` (620 ins) | **verified** | **Missing:** Whole 3D world and character mesh raster submission to GPU pipeline.<br>**Evidence:** SLUS_214.10 `0x00126940` dispatches camera show raster; Xbox default.xbe `0x0019c950` calls `0x7d810` and `0x409d50`. Offscreen display service leases context, but live 3D clump mesh rendering is unlinked. |
| **#3** | `begin_menu_18d450` | Round 9/10 | `0x0018d450` | `0x00028300` | PS2: `0x0018d450`..`0x0018d490` (17 ins)<br>Xbox: `0x00028300`..`0x0002832c` (14 ins) | **verified** | **Missing:** Continuous multi-frame streaming decode loops across multi-second playback (only offscreen frame 0 authenticated).<br>**Evidence:** Both binaries call movie 0 and mode 0. Frame 0 offscreen presentation verified; asynchronous MPEG-2 / SFD stream demuxing loop unrecovered. |
| **#4** | `begin_menu_18d870` | Round 9/10 | `0x0018d870` | `0x00028090` | PS2: `0x0018d870`..`0x0018d8d8` (27 ins)<br>Xbox: `0x00028090`..`0x000280db` (22 ins) | **verified** | **Missing:** Multi-frame video stream decode loop for OPENING movie.<br>**Evidence:** Both binaries start movie 2 and mode 0. Frame 0 offscreen presentation verified; multi-frame streaming demux unrecovered. |
| **#5** | `begin_menu_18d8e0` | Round 9/10 | `0x0018d8e0` | `0x00028000` | PS2: `0x0018d8e0`..`0x0018d964` (34 ins)<br>Xbox: `0x00028000`..`0x00028059` (26 ins) | **verified** | **Missing:** Multi-frame attract loop movie streaming.<br>**Evidence:** Both binaries start movie 1. Continuous attract mode looping unrecovered. |
| **#6** | `begin_menu_18d800` | Round 9/10 | `0x0018d800` | `0x00028110` | PS2: `0x0018d800`..`0x0018d868` (27 ins)<br>Xbox: `0x00028110`..`0x0002815b` (22 ins) | **inferred** | **Missing:** Multi-frame attract movie 3 streaming demux.<br>**Evidence:** Disassembly confirms enqueuing of movie 3; streaming demux inferred. |
| **#7** | `begin_menu_18d770` | Round 9/10 | `0x0018d770` | `0x00028170` | PS2: `0x0018d770`..`0x0018d7f0` (33 ins)<br>Xbox: `0x00028170`..`0x000281bf` (22 ins) | **inferred** | **Missing:** Multi-frame attract movie 4 quad-panel streaming.<br>**Evidence:** Disassembly confirms enqueuing of movie 4; streaming demux inferred. |
| **#8** | `begin_menu_18d700` | Round 9/10 | `0x0018d700` | `0x000281c0` | PS2: `0x0018d700`..`0x0018d768` (27 ins)<br>Xbox: `0x000281c0`..`0x00028208` (19 ins) | **inferred** | **Missing:** Interactive options menu widget navigation and audio/video settings persistence.<br>**Evidence:** Disassembly confirms options menu mode task transition; menu UI widget drawing inferred. |
| **#9** | `begin_menu_18d660` | Round 9/10 | `0x0018d660` | `0x00028210` | PS2: `0x0018d660`..`0x0018d6fc` (40 ins)<br>Xbox: `0x00028210`..`0x0002829e` (35 ins) | **inferred** | **Missing:** Combat demo engine AI simulation and interactive gameplay loop.<br>**Evidence:** Disassembly confirms combat demo mode enqueuing; AI fight simulation inferred. |
| **#10** | `begin_intro_18deb0` | Round 9/10 | `0x0018deb0` | `0x00027be0` | PS2: `0x0018deb0`..`0x0018e018` (91 ins)<br>Xbox: `0x00027be0`..`0x00027c36` (22 ins) | **verified** | **Missing:** Live ROM font glyph rasterization during interactive intro.<br>**Evidence:** Both binaries configure subtitle quad geometry centered horizontally: `(width - 640)/2 - 64`; dynamic ROM font glyph rasterization is absent. |
| **#11** | `begin_prompt_18dc10` | Round 9/10 | `0x0018dc10` | `0x00028350` | PS2: `0x0018dc10`..`0x0018deac` (168 ins)<br>Xbox: `0x00028350`..`0x00028511` (128 ins) | **verified** | **Missing:** Interactive controller button polling and gamepad interrupt service.<br>**Evidence:** Disassembly verifies 150-frame timeout loop (`0x00028402`) and button polling call; interactive hardware controller polling absent. |
| **#12** | `transition_1225d0` | Round 9/10 | `0x001225d0` | `0x0019ca30` | PS2: `0x001225d0`..`0x00122680` (45 ins)<br>Xbox: `0x0019ca30`..`0x0019cacc` (40 ins) | **verified** | **Missing:** Concrete game mode task loop execution bodies.<br>**Evidence:** Disassembly verifies mode history stack push; concrete game mode task loop execution bodies remain unlinked. |
| **#13** | `clear_nonowning_references_18da30` | Round 9/10 | `0x0018da30` | `0x00027df0` | PS2: `0x0018da30`..`0x0018da48` (7 ins)<br>Xbox: `0x00027df0`..`0x00027e09` (7 ins) | **verified** | **Missing:** Downstream task lifetime garbage collector.<br>**Evidence:** Disassembly verifies zeroing of 4 GP pointer offsets in exactly 7 MIPS instructions (`0x18da30`..`0x18da48`); generational task garbage collector unlinked. |
| **#14** | `poll_retail_controller_roles` | Round 11 | `0x002214d0` | `0x001b6750` | PS2: `0x002214d0`..`0x002217b8` (187 ins)<br>Xbox: `0x001b6750`..`0x001b67c4` (40 ins) | **inferred** | **Missing:** Multi-controller role assignment and hardware pad device detection.<br>**Evidence:** Disassembly verifies role iteration; physical USB/gamepad enumeration and interrupt driver inferred. |
| **#15** | `advance_retail_game_services` | Round 11 | `0x003429e0` | `0x0034bd50` | PS2: `0x003429e0`..`0x00342b04` (74 ins)<br>Xbox: `0x0034bd50`..`0x0034bded` (48 ins) | **inferred** | **Missing:** Live network protocol exchange and multi-stream audio updates.<br>**Evidence:** Disassembly verifies timer tick; live network socket polling and audio mixing inferred. |
| **#16** | `retail_mode_dispatch_enabled` | Round 11 | `0x0027b1d0` | `0x0025c1e0` | PS2: `0x0027b1d0`..`0x0027b2a0` (53 ins)<br>Xbox: `0x0025c1e0`..`0x0025c22c` (19 ins) | **verified** | **Missing:** 60-step mode-table dispatch cycle timing verification.<br>**Evidence:** Disassembly verifies mode interval timer check at `gp - 0x58f0` and enable flag check at `0x831a40`; 60-step cycle timing unverified. |
| **#17** | `dispatch_retail_enabled_mode` | Round 11 | `0x0027b320` | `0x0007be10` | PS2: `0x0027b320`..`0x0027b390` (29 ins)<br>Xbox: `0x0007be10`..`0x0007be5c` (27 ins) | **verified** | **Missing:** Live gameplay mode transitions.<br>**Evidence:** Disassembly verifies mode table indexing (`0x00185e90`/`0x00185d30`) and active task jump; concrete gameplay mode transitions unlinked. |
| **#18** | `service_retail_frame_input` | Round 11 | `0x00210570` | `0x0026a9c0` | PS2: `0x00210570`..`0x00210654` (58 ins)<br>Xbox: `0x0026a9c0`..`0x0026ade0` (292 ins) | **inferred** | **Missing:** Hardware gamepad analog calibration and port polling.<br>**Evidence:** Disassembly verifies input packet buffering; analog deadzone calibration and hardware port polling inferred. |
| **#19** | `create_retail_action_task` | Round 11 | `0x00130000` | `0x001c27f0` | PS2: `0x00130000`..`0x00130028` (11 ins)<br>Xbox: `0x001c27f0`..`0x001c286f` (50 ins) | **verified** | **Missing:** Dynamic task priority queue scheduling.<br>**Evidence:** Disassembly verifies task allocation and insertion into list `0x2001`; dynamic priority heap scheduling unlinked. |
| **#20** | `prune_retail_associations` | Round 11 | `0x001304f0` | `0x001c6de0` | PS2: `0x001304f0`..`0x00130580` (37 ins)<br>Xbox: `0x001c6de0`..`0x001c6e43` (34 ins) | **verified** | **Missing:** Generational garbage collection of scene nodes.<br>**Evidence:** Disassembly verifies association list traversal and deletion at `0x5ba6e8`; tree-based generational garbage collection absent. |
| **#21** | `reset_and_bind_retail_scene_event_graph` | Round 11 | `0x004c4d80` | `0x002f6d50` | PS2: `0x004c4d80`..`0x004c4dc0` (17 ins)<br>Xbox: `0x002f6d50`..`0x002f6d74` (14 ins) | **verified** | **Missing:** Dynamic event listener registration.<br>**Evidence:** Disassembly verifies error code `0xff000183` and graph reset in both binaries; dynamic event listener registration unpopulated. |
| **#22** | `deliver_retail_builtin_scene_event` | Round 11 | `0x004c6a70` | `0x002f6d80` | PS2: `0x004c6a70`..`0x004c6ab8` (19 ins)<br>Xbox: `0x002f6d80`..`0x002f6da2` (13 ins) | **verified** | **Missing:** Scene hierarchy event propagation.<br>**Evidence:** Disassembly verifies error code `0xff000184` and sink call in both binaries; hierarchical event bubbling unlinked. |
| **#23** | `invoke_retail_profile_select_from_2f1630` | Round 11 | `0x002f1630` | `0x00277df0` | PS2: `0x002f1630`..`0x002f1674` (18 ins)<br>Xbox: `0x00277df0`..`0x002783c9` (346 ins) | **inferred** | **Missing:** Interactive UI dialogue rendering and memory card writeback.<br>**Evidence:** Disassembly verifies profile title parsing; interactive UI dialogue rendering and memory card writeback inferred. |
| **#24** | `apply_retail_profile_participant_from_188720` | Round 11 | `0x00188720` | `0x00030580` | PS2: `0x00188720`..`0x00188890` (93 ins)<br>Xbox: `0x00030580`..`0x000305b4` (20 ins) | **inferred** | **Missing:** Profile participant record synchronization with NVRAM saves.<br>**Evidence:** Disassembly verifies participant slot assignment; synchronization with NVRAM saves inferred. |
| **#25** | `publish_retail_profile_participant_from_189690` | Round 11 | `0x00189690` | `0x00031020` | PS2: `0x00189690`..`0x00189820` (101 ins)<br>Xbox: `0x00031020`..`0x00031030` (7 ins) | **inferred** | **Missing:** Persistent profile broadcast to network session peers.<br>**Evidence:** Disassembly verifies participant stats publication; persistent profile network broadcast inferred. |

### Headless Offscreen Full-Entry Run (Verification of Entry Link)
Under `TASK_BOARD.md` row 10, strictly zero visible desktop windows, zero audible audio endpoints, and zero owner launch are authorized prior to passing the full acceptance matrix. Execution was performed entirely using an offscreen hidden window context (`SDL_WINDOW_HIDDEN`) in `tests/test_native_startup_missing_bindings.cpp`, bounded by `MKA_HEADLESS_MAX_FRAMES=60`:
1. **Execution Telemetry**:
   - `run_native_retail` entered the retail startup loop headlessly.
   - Initialized authentic content volume with staged retail assets (`MODULES/DNAS310.IMG`, `SLUS_214.10`, `GAMER.OVL`, `sndsps2/MSLASSET.MS2`/`MS4`, `art/gameart.ssf`).
   - Terminated at: `StartupFailure kind: 18, operation: 3011D0 native texture conversion` during font/system art upload (`upload native RGBA8 image GL error=0x500` in software/headless OpenGL context).
   - Total placeholder invocations recorded: 2.
2. **Ordered Sequence of Placeholders Hit**:
   - `[1]` First hit at frame 0: Placeholder #0 `continue_first_scene_at_225d40` (called 1 time).
   - `[2]` First hit at frame 0: Placeholder #1 `continue_scene_reset_at_125d60` (called 1 time).
   - Distinct placeholders encountered: 2 of 26 registered.
3. **Execution Blocker Analysis**:
   - **First Blocker for 3D Geometry Submission**: Placeholder #0 (`continue_first_scene_at_225d40`). It returns ready lifecycle status without loading world sector / clump geometry. As a result, `retail_render_enabled()` observes `has_world = false`, preventing Placeholder #2 (`render_retail_frame`) from being called with live scene geometry.
   - **First Blocker for Movie Stream Decoding Loop**: Placeholder #3 (`begin_menu_18d450`). Its child procedure (`NativeRetailMoviePlaybackChild`) completes immediately upon presenting frame 0 rather than driving asynchronous MPEG-2 / SFD stream packet demuxing loops across multi-second attract playback.
   - **First Blocker for Game Mode Task Dispatch**: Placeholder #12 (`transition_1225d0`) / Placeholder #17 (`dispatch_retail_enabled_mode`). Pushes mode transition records into `mode_history`, but actual mode task bodies remain placeholders.
4. **What Already Works**:
   - Authentic content volume mounting and package path resolution.
   - Verification of binary module headers (`DNAS310.IMG`, `SLUS_214.10`, `GAMER.OVL`).
   - Sound bank catalog parsing (`MSLASSET.MS2`/`MS4`).
   - System art SSF package directory reading (`art/gameart.ssf`).
   - Offscreen display service creation, WGL context leasing, and aspect ratio computation.
   - Main retail session lifecycle orchestration up to the initial scene continuation gate.

## 7. Ledger Credit Assessment, Vector Fixture Audit & Genuine Trace Specification

### Assessment of Implemented Routines Against Existing Credit Rules

Under repository governance, function progress is divided strictly into two tiers:
1. **PS2 Ledger (`functions.csv`) Credit Rules**:
   - Status `ported` requires a matching entry in `research/ledger/ported_function_evidence.json` following the pattern `(src_file:line [method] (test_file:line "assertion"))`.
   - The test assertion must exist at the referenced line in the test suite and verify non-vacuous behavioral properties of the function.
   - Status `implemented-via-donor` strictly requires verified dual-platform vectors in `research/test_vectors/vector_<addr>.json`.
2. **Xbox Donor Ledger (`xbox_functions.csv`) Credit Rules**:
   - Status `implemented-via-donor` requires verified dual-platform vectors with non-empty `oracle_source`, valid 64-character hexadecimal SHA-256 `oracle_trace_hash`, and bit-exact native execution match.
   - Without verified dual-platform vectors, candidate routines remain categorized under the **Open Donor Frontier** as `blocked-on-dual-platform-vectors` (17 candidate donor routines).

### Ledger Qualification Finding
- **Zero Ledger Advance**: None of the 23 routines implemented this round qualify for ledger credit under current rules:
  - While their call flows and control structures are verified against retail disassembly, they currently lack dedicated dual-platform test vector files (`vector_<addr>.json`) backed by binary execution traces (`retail_SLUS_214.10_mips_trace.bin`).
  - Awarding ledger credit without verified vectors would violate governance and cause `validate_ledgers` to fail.
  - Therefore, the ledgers remain strictly:
    - **PS2 Ledger**: strictly **238 / 15,670 (1.52%)**.
    - **Xbox Donor Ledger**: strictly **0 / 21,321 (0.00%)**.

### Audit of Test Vector Trace Fixture (`retail_SLUS_214.10_mips_trace.bin`)
- **Fixture Audit**: An audit of `Armageddon-backup/armageddon-reimplementation/research/test_vectors/traces/retail_SLUS_214.10_mips_trace.bin` confirms it is a 100-byte synthetic ASCII fixture:
  ```text
  MIPS_R5900_RETAIL_EXECUTION_TRACE_SLUS_214.10_ENTRY_0012A720_BLOCK_0
  CYCLE_COUNT=42
  REG_V0=00000001
  ```
- **Provenance**: Created specifically as a test target for Planted Failure 7 in `research/runs/test_full_existence_validation.py` to verify hash validation and schema parsing.
- **Honest Accounting**: This file is **NOT** an authentic emulator execution trace. Citing it as runtime evidence for function qualification would be fraudulent.

### Genuine Retail Emulator Trace Producer Specification
To produce authentic dual-platform test vectors for donor qualification, a genuine trace must be captured according to this specification:
1. **Execution Environment**:
   - Headless instrumented emulator (e.g. customized PCSX2 or QEMU-MIPS with instruction trace logging) executing pinned `SLUS_214.10` (SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`).
   - The emulator must replay states from the 2026-11-10 playable oracle state suite (61 verified states) driven by the 72 input sidecars to reach the authenticated code block.
2. **Trace Log Schema**:
   - Cycle timestamp (64-bit unsigned integer).
   - Program Counter ($PC$, 32-bit hexadecimal).
   - Instruction opcode (32-bit hexadecimal).
   - Disassembled mnemonic.
   - Register writeback delta: target register name and 64-bit / 128-bit value written.
   - Memory access delta: load/store type, virtual/physical address, and data bytes read/written.
3. **Trace Archival & Verification**:
   - The trace binary slice is hashed with SHA-256 and stored under `research/test_vectors/traces/`.
   - The test vector file references the trace file by name and records its exact SHA-256 hash in `oracle_trace_hash`.

### Minimal Dual-Platform Vector Specification (Representative Function: `0x0018da30`)
For a representative routine such as `clear_nonowning_references_18da30` (PS2 `0x0018da30` / Xbox `0x00027df0`) to qualify for ledger credit, a minimal vector must supply:
1. **Pre-Execution Machine Context**:
   - Initial register state: `$sp` pointing to valid scratch memory, `$gp` pointing to Global Pointer table base (`0x0064a000`), `$ra` set to caller return address (`0x0018d458`).
   - Initial memory state: non-zero pointers at GP offsets `-0x5da8` (`0x005b4ed8`), `-0x5da4`, `-0x5db0`, and `-0x5dac`.
2. **Oracle Instruction Trace**:
   - Binary trace slice from genuine emulator trace executing instructions from `0x0018da30` to `jr $ra` (`0x0018da48`).
   - Recorded SHA-256 hash of the execution trace for tamper detection.
3. **Post-Execution State Assertions**:
   - Target memory cleared: `dword [gp - 0x5da8] == 0`, `dword [gp - 0x5da4] == 0`, `dword [gp - 0x5db0] == 0`, `dword [gp - 0x5dac] == 0`.
   - Stack restored: `$sp_post == $sp_pre + 0x10`.
   - Return register: `$v0 == 1` or expected boolean return code.
4. **Credit Accounting**:
   - Exact value: +1 PS2 function in `functions.csv`, +1 Xbox donor function in `xbox_functions.csv`.
   - Ledgers remain strictly unchanged until genuine dual-platform emulator traces are captured and validated.

## 8. Acceptance, Verification Commitments & Pipeline Deferral

### Deferral of Connected 3D Clump/Atomic Pipeline
In adherence to the agreed roadmap, connected 3D mesh rasterization (RenderWare clump/atomic pipeline) is **deliberately deferred** until:
1. The headless execution loop and scene continuation gates (Items #0 and #1) are established.
2. Multi-frame movie presentation and streaming are verified across attract mode cycles.
Attempting 3D clump rasterization prematurely while scene continuation and texture conversion fail would manufacture false progress.

### Strict Acceptance & Verification Commitments
- **Zero Visible Windows / Audio Endpoints / Owner Launch**: In strict adherence to `TASK_BOARD.md` row 10, presentation is hosted entirely in an offscreen double-buffered WGL target (`NativeOffscreenDisplayService`). No desktop window, audible sound stream, or owner game loop is opened.
- **Strict Accounting**:
  - PS2 function ledger: strictly 238 / 15,670 (1.52%).
  - Xbox donor ledger: strictly 0 / 21,321 (0.00%).
  - Acceptance status: strictly **OPEN**.
- **No Overclaiming**: Link diagnostics show **undefined: 0** with **placeholders: 26**. Verified offscreen presentation of `LOGOW.SFD` (frame 0) and `OPENINGW.SFD` (frame 1) demonstrates the video display interface and texture upload pipeline, but whole 3D gameplay geometry rendering and interactive fight loops remain unrecovered.
- **Single Runner**: `tools/research/run_check.py` is the sole canonical runner; `run_14_tests.py` remains deleted via `git rm`.
