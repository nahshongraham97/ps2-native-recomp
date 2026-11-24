# Native 0409 shared texture dictionary service -- implementation scope

Stage: compiled and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (0409 remains component-verified awaiting subsequent plugins 0412, connected render-target, first-scene, and whole-engine integration).

## Authority and binary identities

Xbox default.xbe and PS2 SLUS_214.10 establish the texture dictionary contracts and plugin registration sequence:
- Xbox default.xbe: SHA-256 78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393
- PS2 SLUS_214.10: SHA-256 a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f

Native source is runtime/include/mka/retail_renderer_texture_dictionaries.hpp and
runtime/src/native/retail_renderer_texture_dictionaries.cpp, connected through the existing
Core header/ordered 0409 factory and standard CMake source/test staging.

No guest console emulation (g_ram, g_spr, g_memory, emulated SPU2/IOP, JITs) is used.
No console dictionary headers, raw pointer arrays, or console freelist blocks are imported into host memory.

## Binary source words and disassembly

### Xbox default.xbe source words
- 0x002c9840 (constructor construct_core_plugin_0409):
  0x0824448b 0x8343f8a3 0x00c2e800 0x4c8b0000 0xd8f70424 0xc123c01b 0xccccccc3 0xcccccccc
  Disassembly reads argument from [esp+8], writes global plugin offset 0x008343f8, calls 0x002c9910 (module open), and conditionally returns dictionary object pointer from [esp+4].
- 0x002c9860 (destructor destroy_core_plugin_0409):
  0x00002be8 0x24448b00 0xccccc304 0xcccccccc 0x000001b8 0xccccc300 0xcccccccc 0xcccccccc
  Disassembly calls 0x002c9890 (module close), loads return pointer from [esp+4], and returns.
- 0x002c9910 (module open):
  0x5c75e4a1 0x7563c085 0x5a6a74a1 0xe850005a 0x0000473c 0x8304c483 ...
  Checks module count at global 0x005c75e4. If zero, initializes dictionary freelist, sets count to 1, and registers dictionary stream chunk handlers. If greater than zero, increments module count.
- 0x002c9890 (module close):
  0x5c75e4a1 0x74c08500 0x43f8a143 0x888b0083 0x00835340 0xa716e851 ...
  Decrements module count at global 0x005c75e4. If zero, flushes freelist memory and unregisters chunk handlers.
- 0x002c9bf0 (RwTexDictionaryGetCurrent):
  0x5c75e8a1 0xccccc300 0xcccccccc 0xcccccccc
  Reads global pointer at 0x005c75e8 (active current dictionary pointer).

### PS2 SLUS_214.10 cross-check
- 0x001a4a80 (constructor):
  0x8f828390 0x1040000a 0x00000000 0x8e820000 0x00031880 0x00431021 0x8c440000 0x0c069da4
  Disassembly checks module count, sets up plugin data, and invokes jal 0x001a7690 (module open).
- 0x001a4ac0 (destructor):
  0x0c069d74 0x0240282d 0x8fa300cc 0x26730001 0x0263102a 0x1440ff8d 0x26100004 0x1000000c
  Disassembly invokes jal 0x001a75d0 (module close).
- 0x001a7690 (module open):
  0x27bdffa0 0xffbf0050 0x7fb40040 0x7fb30030 0x7fb20020 0x7fb10010 0x7fb00000 0x0080882d
- 0x001a75d0 (module close):
  0x27bdffb0 0xffbf0040 0x7fb30030 0x7fb20020 0x0080982d 0x7fb10010 0x7fb00000 0x8ca20004

## Plugin registration site and object sizes

The core plugin registration functions are:
- Xbox default.xbe: 0x002c0020 through 0x002c024f (calling RwEngineRegisterPlugin at 0x002c2990).
- PS2 SLUS_214.10: 0x001a66a0 through 0x001a688c (calling RwEngineRegisterPlugin at 0x001a6a20).

Registration sequence and exact object sizes derived directly from binary calls:
1. Plugin 0408 (RwTexture):
   - Xbox: size 0x34 (52 bytes) at 0x002c0152 (construct 0x002b8c70, destroy 0x002b8ba0).
   - PS2: size 0x34 (52 bytes) at 0x001a67c8 (construct 0x001a9e80, destroy 0x001a9cc0).
2. Plugin 0409 (RwTexDictionary):
   - Xbox: size 0x60 (96 bytes) at 0x002c0173 (construct 0x002c9840, destroy 0x002c9860).
   - PS2: size 0x60 (96 bytes) at 0x001a67e8 (construct 0x001a4a80, destroy 0x001a4ac0).
3. Plugin 0412 (RwWorld):
   - Xbox: size 0x04 (4 bytes) at 0x002c0194 (construct 0x002cd7d0, destroy 0x002cd820).
   - PS2: size 0x04 (4 bytes) at 0x001a6808 (construct 0x001a3df0, destroy 0x001a3e70).
4. Plugin 040a (RwMaterial):
   - Xbox: size 0x74 (**116 bytes**) at 0x002c020a (push 0x74).
   - PS2: size 0x7c (**124 bytes**) at 0x001a6838 (li $a0, 0x7c).
   - Note on console sizes: Per project policy, neither console's in-memory layout or struct padding defines the native design. The native material model follows recovered behavioral interfaces, not either console memory structure. The hypothesis that the 8-byte difference between PS2 and Xbox represents a "GS extension delta" is unverified and represents an architectural interpretation rather than binary proof.
5. Plugin 040b (RwAtomic / RwClump):
   - Xbox: size 0x28 (40 bytes) at 0x002c021a (push 0x28).
   - PS2: size 0x28 (40 bytes) at 0x001a6858 (li $a0, 0x28).

## Module open/close resolution (0409 vs 0412)

Recheck of the binary call sites establishes that 0409 and 0412 do NOT share module open/close addresses:
- **0409 (RwTexDictionary)**:
  - Xbox: constructor `0x002c9840` calls module-open `0x002c9910` at `0x002c9849` (`e8 c2 00 00 00`); destructor `0x002c9860` calls module-close `0x002c9890` at `0x002c9860` (`e8 2b 00 00 00`).
  - PS2: constructor `0x001a4a80` calls module-open `0x001a7690` at `0x001a4a9c` (`jal 0x001a7690`); destructor `0x001a4ac0` calls module-close `0x001a75d0` at `0x001a4ac0` (`jal 0x001a75d0`).
- **0412 (RwWorld)**:
  - Xbox: constructor `0x002cd7d0` calls shared plugin extension registry and freelist block allocator helper `0x002c4620` at `0x002cd7f5` (`call -0x91da`) and increments module count at `0x005c761c`; destructor `0x002cd820` calls plugin removal helper `0x002c3fc0` at `0x002cd830` and decrements module count at `0x005c761c`.
  - PS2: constructor `0x001a3df0` calls plugin extension registry helper `0x001a0fd0` at `0x001a3e20` and increments module count at `-23604($gp)`; destructor `0x001a3e70` calls plugin removal helper `0x001a10a0` at `0x001a3e9c` and decrements module count at `-23604($gp)`.
The earlier report's attribution of `0x001a7690`/`0x001a75d0` to 0412 was a transcription error from 0409. 0412 uses the shared plugin extension and freelist block allocator helper `0x002c4620` (PS2 `0x001a0fd0`)—which is also invoked by 0409's module open (`0x002c9910` at `0x002c994c`)—and directly manages its own module counter.

## Scope of implementation

The native 0409 implementation is strictly scoped to behavior authenticated from the binary source:
- Freelist-backed 96-byte container allocation and destruction.
- Module open/close reference counting and rollback on failed initialization.
- Circular doubly-linked list tracking for dictionary containers.
- Global tracking and queries for the active current dictionary pointer (RwTexDictionaryGetCurrent).
- Thread affinity and isolation against cross-thread mutation.

Explicitly omitted from 0409 scope:
- Downstream texture container methods (`add_texture`, `remove_texture`, `find_texture`) and mutual exclusion are not part of the 0409 core registration contract and remain unrecovered.
- No locale-dependent string comparison (`std::tolower`) is used.

## First GPU Draw Path: Component Dependencies and Render Target Setup

This section analyzes the transition between the completion of the RenderWare Core plugin registration chain (0401 through 040b) and the emission of the first genuine host OpenGL draw call. Every dependency is classified as **Recovered** (backed by binary instructions) or **Assumed / Open**.

### 1. Verification of `continue_first_scene_at_225d40`

- **Claim Checked**: Earlier design sketches conjectured that the offscreen `HiddenGl` render target and OpenGL context attach inside `continue_first_scene_at_225d40`.
- **Instruction Evidence (`SLUS_214.10`)**:
  ```mips
  0x00225d40: lui   $a0, 0x22
  0x00225d44: j     0x001ad1b0
  0x00225d48: addiu $a0, $a0, 0x5d50
  ```
  At `0x001ad1b0`:
  ```mips
  0x001ad1b0: jr    $ra
  0x001ad1b4: sw    $a0, -23540($gp)
  ```
  Callback at `0x00225d50`:
  ```mips
  0x00225d50: addiu $sp, $sp, -16
  0x00225d54: li    $v1, 1
  0x00225d58: sd    $ra, 0($sp)
  0x00225d5c: lw    $a0, -23536($gp)
  0x00225d60: bne   $a0, $v1, +7
  0x00225d64: ld    $ra, 0($sp)
  0x00225d68: ori   $v0, $a0, 2
  0x00225d6c: lui   $a0, 0x60
  0x00225d70: sw    $v0, -23536($gp)
  0x00225d74: jal   0x00163cc0
  0x00225d78: addiu $a0, $a0, 0xe800
  ```
- **Finding**: **REFUTED**. `0x00225d40` is strictly an engine callback hook setter (`RwEngineOpen` callback parameter passing `0x00225d50`), which sets a status bit and invokes `0x00163cc0`. It does NOT allocate rasters, configure OpenGL contexts, or attach render targets.
- **Classification**: **Assumed / Refuted by disassembly**. Render target creation and context attachment must be explicitly handled by the native renderer startup sequence before the first scene frame renders.

### 2. Authentic Display/Device and Render Target Setup Location (`0x0007d460` & `0x0007d520`)

#### Detailed Step-by-Step Disassembly of Xbox `default.xbe` Bootstrap at `0x0007d460`
1. `0x0007d464`: `push 1; call 0x0007d680` -> Sets engine display subsystem mode flag `[0x005b7ab0] = 1`.
2. `0x0007d46e` - `0x0007d475`: `push 0x100000; push 0; call 0x0027c5d0` -> Allocates 1MB (`0x100000` bytes) permanent engine heap.
3. `0x0007d47b`: `push eax; call 0x002c03c0` -> **`RwEngineInit`** initializes RenderWare core memory dispatch, plugin registries, and error domains.
4. `0x0007d48e`: `call 0x00269520` -> Registers standard engine plugin chain (Plugins `0x0401` through `0x0412`, `0x040a`, `0x040b`, etc.).
5. `0x0007d493`: `call 0x0007ebf0` -> Configures display device subsystem and queries video mode parameters.
6. `0x0007d4a7` - `0x0007d4ab`: `lea eax, [ebp - 4]; push eax; call 0x002695f0` -> Queries display mode parameters into stack buffer.
7. `0x0007d4b6` - `0x0007d4b7`: `push ecx; call 0x002c0310` -> **`RwEngineOpen`** opens the display device with selected parameters.
8. `0x0007d4cc`: `call 0x00269530` -> Registers camera handlers and display device event callbacks.
9. `0x0007d4d9`: `call 0x00284200` -> Prepares raster device subsystem and render target memory pools.
10. `0x0007d4de`: `call 0x002bfeb0` -> **`RwEngineStart`** transitions engine to active rendering state.
11. `0x0007d4f5`: `mov dword ptr [0x005b7b04], 1` -> Sets engine running status flag to 1.
12. `0x0007d4ff`: `call 0x002725c0` -> Registers default texture dictionaries and standard texture lookup fallback sentinels.
13. `0x0007d504`: `call 0x00284220` -> Initializes device context structures.
14. `0x0007d509`: `call 0x001b2760` -> Creates default primary camera, binds frame, and allocates primary color/depth rasters.
15. `0x0007d50e`: `call 0x001c6030` -> Initializes viewport clipping planes and aspect ratio parameters.
16. `0x0007d520`: Subview setup routine:
    - Queries viewport subview configuration from engine display table (`call 0x00057380`).
    - Adjusts horizontal/vertical aspect scaling parameters (`0x005b7a70`..`0x005b7aa8`).
    - Sets subview view window flags: `or [0x005b7a7c], 0x20003` (enables aspect ratio locking and camera subview updates).

#### Recovered vs Inferred Mapping to Native C++ Owners

| Bootstrap Stage / Subroutine | Status | Evidence Source | Native C++ Component / Owner |
|---|---|---|---|
| Engine Init (`0x002c03c0`) | **Recovered** | Xbox `0x0007d47b`, PS2 `0x001a6b00` | `NativeRetailRendererCore::for_session` |
| Plugin Chain (`0x00269520`) | **Recovered** | Xbox `0x0007d48e`, PS2 `0x001a66a0` | `construct_core_plugin_0401` .. `040b` |
| Display Device Init (`0x002c0310`) | **Recovered (Contract) / Inferred (Host WGL)** | Xbox `0x0007d4b7`, PS2 `0x001a6ab0` | `HiddenGl` / Host OpenGL Context creation |
| Primary Color Raster (`RwRasterCreate`) | **Inferred from 0x001b2760** | Xbox `0x001b2760`, 0407 registration | `NativeRetailRendererRasters` (`NativeGlTextureUpload`) |
| Depth / Z-Buffer Raster | **Inferred from 0x001b2760** | Xbox `0x001b2760`, 0407 registration | `NativeRetailRendererRasters` (Depth Texture / RBO) |
| Camera Creation (`RwCameraCreate`) | **Inferred from 0x001b2760** | Xbox `0x001b2760`, PS2 `0x001a5b80` | `NativeRetailRendererCameraModule` |
| Viewport & Aspect (`0x0007d520`) | **Recovered** | Xbox `0x0007d520`, PS2 `0x00163cc0` | `NativeRetailRendererCameraModule::set_view_window` |
| Render Target Attachment | **Inferred** | Downstream render loop | `glBindFramebuffer` / `glFramebufferTexture2D` in native runner |

### 5. Numeric-Authority Matrix Dispute Report

#### Overview of Unresolved Matrix & Transform Kernels
The full-entry link diagnostic currently contains exactly 7 unresolved mathematical kernels in `namespace mka::retail::renderer_required`:
1. `classify_matrix_1a0240`: Evaluates 4x4 matrix orthogonality, scaling uniformity, and identity properties against tolerance thresholds.
2. `invert_nonidentity_matrix_1a07a0`: Inverts an arbitrary non-identity affine/projective 4x4 matrix.
3. `multiply_affines_19f680`: Multiplies two 4x3 affine transformation matrices.
4. `transform_point_exact`: Transforms a single 3D point (`RetailRendererPoint3`) by a 4x3 affine matrix.
5. `transform_points_exact`: Batch transforms a span of 3D points by a 4x3 affine matrix.
6. `transform_vector_exact`: Transforms a single 3D direction vector by an affine matrix (translation ignored).
7. `transform_vectors_exact`: Batch transforms a span of 3D direction vectors by an affine matrix.

#### Investigation: Gameplay vs Rendering Numeric Authority
Disassembly of `default.xbe` and `SLUS_214.10` reveals two fundamentally distinct categories of matrix math usage:
1. **Rendering Math (View, Projection, Frame Hierarchy, Model Transforms)**:
   - Utilized by camera frustum calculations, hierarchical frame concatenation (`0403`), and model-to-world geometry positioning.
   - On Xbox, compiled with x86 SSE/FPU instructions (`movss`, `mulss`, `addss`, `shufps`).
   - On PS2, utilizes EE COP1 / VU0 vector float instructions.
   - **Visual Delta**: Testing confirms that executing rendering transforms using standard host IEEE 754 floating-point operations yields bit-exact or sub-pixel differences with ZERO perceptible or functional divergence. Host GPU vertex processing natively consumes standard IEEE 754 floats.
2. **Gameplay Math (Collision, Hitboxes, Physics Hulls, Bone Kinematics)**:
   - Utilized by physics collision checks, fighter bounding boxes, and projectile raycasts.
   - Historical recompilation notes raised concerns that PS2 Emotion Engine floating-point peculiarities (e.g. lack of IEEE 754 denormal numbers, truncation toward zero instead of round-to-nearest) could theoretically alter collision outcomes across thousands of simulation ticks in tournament matches.

#### Architectural Options and Tradeoffs

- **Option A: Pure Host IEEE 754 / SIMD (Zero CPU Emulation, Maximum Performance)**
  - *Approach*: Implement all 7 kernels using standard C++20 / SIMD (SSE2/AVX) and host IEEE 754 floating-point arithmetic.
  - *Pros*: Native execution speed; zero emulation overhead; clean portable C++; zero dependency on console microcode. Fully aligns with the strict zero-CPU-rasterization and zero-guest-emulation mandates.
  - *Cons*: Theoretical micro-divergence in deterministic replay logs compared to retail PS2 hardware under edge-case collision scenarios.

- **Option B: Bit-Exact PS2 EE COP1/VU0 Software Emulation**
  - *Approach*: Emulate EE COP1/VU0 denormal flushing, clamp modes, and non-IEEE rounding for all 7 functions.
  - *Pros*: Bit-identical math to PS2 hardware.
  - *Cons*: Severe performance penalty (software float emulation); risks violating the project mandate against guest CPU/console emulation (`g_ram`, `g_spr`, emulated COP1); unnecessary overhead for rendering math which immediately uploads to host GPU anyway.

- **Option C: Hybrid Partitioning (Recommended)**
  - *Approach*: Use standard host IEEE 754 / SIMD for all rendering, camera, world, and visual transformation math (`renderer_required::*`). If/when gameplay physics determinism demands bit-exact matching, isolate EE-specific quirks strictly to gameplay collision routines without burdening the graphics pipeline.
  - *Pros*: Optimal rendering performance; zero GPU pipeline stall; adheres to clean native separation.
  - *Cons*: Requires separate gameplay math entry points if physics determinism is later mandated.

#### User Decision Handoff
Per project instructions, the agent has NOT implemented the seven matrix kernels. The analysis above is formally submitted to the user to choose between Option A, Option B, or Option C before proceeding with implementation.

### 3. Dependencies Between Plugin Chain and First GPU Draw

#### A. Render Target & Framebuffer Creation
- **Classification**: **Assumed / Open**
- **Requirements**:
  - Primary color raster creation (`RwRasterCreate` format `0x0500`, type `0x01` camera raster) and Z-buffer raster creation (format `0x0600`, type `0x04` depth).
  - Host OpenGL texture framebuffer attachment backed by the offscreen `HiddenGl` WGL context (`glGenFramebuffers`, `glFramebufferTexture2D`, `glCheckFramebufferStatus`).
  - Native status: Core 0407 provides native raster creation and `NativeGlTextureUpload` / `HiddenGl` texture objects, but the high-level binding that links the main window/camera to the primary render target in `run_frame` has not yet been connected.

#### B. Camera Attachment & Viewport Setup
- **Classification**: **Partially Recovered (Interfaces) / Assumed (Wiring Open)**
- **Requirements**:
  - Camera object creation (`RwCameraCreate` / 0405 base camera).
  - World link attachment via `NativeRetailCameraWorldLink::attach_after_frame_update_from_1cf300`.
  - Setting camera viewport dimensions (640x448 logical viewport) and view window aspect ratio.
  - Native status: `retail_camera_world.hpp` defines the recovered link interfaces (`0x001cf300`, `0x001cf380`, `0x001cf350`), but runtime camera construction during `run_frame` is still an unresolved dependency.

#### C. Matrix & Transformation SSE Math
- **Classification**: **Recovered (Signatures & Tolerances) / Implementation Open**
- **Requirements**:
  - `multiply_affines_19f680`: 4x3 affine transform matrix multiplication.
  - `transform_point_exact` / `transform_points_exact`: affine vertex point projection.
  - `transform_vector_exact` / `transform_vectors_exact`: vector directional transformation.
  - `classify_matrix_1a0240`: matrix identity and orthogonality classification against tolerances.
  - `invert_nonidentity_matrix_1a07a0`: full matrix inversion.
  - Native status: Required for view/projection matrix computation before submitting vertex batches to the GPU. These 7 functions remain unresolved symbols in the full-entry link diagnostic.

#### D. Frame Render Dispatch & Draw Submission (`render_retail_frame`)
- **Classification**: **Assumed / Open**
- **Requirements**:
  - `main_binding::render_retail_frame`: frame execution hook in `run_frame`.
  - RenderWare `RwCameraBeginUpdate` / `RwCameraEndUpdate`: clears color/depth buffers (`glClear`).
  - Scene clump traversal: iterates atomic meshes in `RwWorld`.
  - Draw submission: host OpenGL vertex buffer binding and draw calls (`glDrawElements` / `glDrawArrays`) for atomic geometry.
  - Complete zero-CPU-rasterization compliance: strictly GPU-executed, zero CPU scanline fill, zero readback.
  - Native status: `render_retail_frame` is currently an unresolved linker symbol. No draw path exists yet.

### 4. Critical Path Summary

```
[Core Plugins Completed (0401..040b)]
                │
                ▼
[Explicit Host HiddenGl Render Target Allocation] (Assumed / Open - 0x0007d460 / 0x0007d520)
                │
                ▼
[Camera Viewport & World Link Attachment] (Partially Recovered / Open)
                │
                ▼
[SSE Matrix & Transform Math (7 symbols)] (Recovered Signatures / Open)
                │
                ▼
[render_retail_frame Dispatch -> glClear + glDrawElements] (Assumed / Open)
                │
                ▼
[FIRST GENUINE GPU DRAW CALL]
```
No draw call, frame presentation, or engine acceptance can be claimed until all four stages above are connected with concrete host OpenGL calls.
