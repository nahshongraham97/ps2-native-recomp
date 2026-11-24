# First GPU Draw Path: Component Dependencies and Verification Note

Location: research/runs/native-xbox-renderer-0409-implementation-20261123/FIRST_DRAW_PATH_DESIGN.md

This note analyzes what must exist between the completion of the RenderWare Core plugin registration chain (0401 through 040b) and the emission of the first genuine host OpenGL draw call. Every dependency is explicitly classified as **Recovered** (backed by binary instructions) or **Assumed / Open**.

---

## 1. Verification of `continue_first_scene_at_225d40`

- **Claim Checked**: The previous ranking report claimed that the offscreen `HiddenGl` render target and context attach inside `continue_first_scene_at_225d40`.
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

---

## 2. Dependencies Between Plugin Chain and First GPU Draw

### A. Render Target & Framebuffer Creation
- **Classification**: **Assumed / Open**
- **Requirements**:
  - Primary color raster creation (`RwRasterCreate` format `0x0500`, type `0x01` camera raster) and Z-buffer raster creation (format `0x0600`, type `0x04` depth).
  - Host OpenGL texture framebuffer attachment backed by the offscreen `HiddenGl` WGL context (`glGenFramebuffers`, `glFramebufferTexture2D`, `glCheckFramebufferStatus`).
  - Native status: Core 0407 provides native raster creation and `NativeGlTextureUpload` / `HiddenGl` texture objects, but the high-level binding that links the main window/camera to the primary render target in `run_frame` has not yet been connected.

### B. Camera Attachment & Viewport Setup
- **Classification**: **Partially Recovered (Interfaces) / Assumed (Wiring Open)**
- **Requirements**:
  - Camera object creation (`RwCameraCreate` / 0405 base camera).
  - World link attachment via `NativeRetailCameraWorldLink::attach_after_frame_update_from_1cf300`.
  - Setting camera viewport dimensions (640x448 logical viewport) and view window aspect ratio.
  - Native status: `retail_camera_world.hpp` defines the recovered link interfaces (`0x001cf300`, `0x001cf380`, `0x001cf350`), but runtime camera construction during `run_frame` is still an unresolved dependency.

### C. Matrix & Transformation SSE Math
- **Classification**: **Recovered (Signatures & Tolerances) / Implementation Open**
- **Requirements**:
  - `multiply_affines_19f680`: 4x3 affine transform matrix multiplication.
  - `transform_point_exact` / `transform_points_exact`: affine vertex point projection.
  - `transform_vector_exact` / `transform_vectors_exact`: vector directional transformation.
  - `classify_matrix_1a0240`: matrix identity and orthogonality classification against tolerances.
  - `invert_nonidentity_matrix_1a07a0`: full matrix inversion.
  - Native status: Required for view/projection matrix computation before submitting vertex batches to the GPU. These 7 functions remain unresolved symbols in the full-entry link diagnostic.

### D. Frame Render Dispatch & Draw Submission (`render_retail_frame`)
- **Classification**: **Assumed / Open**
- **Requirements**:
  - `main_binding::render_retail_frame`: frame execution hook in `run_frame`.
  - RenderWare `RwCameraBeginUpdate` / `RwCameraEndUpdate`: clears color/depth buffers (`glClear`).
  - Scene clump traversal: iterates atomic meshes in `RwWorld`.
  - Draw submission: host OpenGL vertex buffer binding and draw calls (`glDrawElements` / `glDrawArrays`) for atomic geometry.
  - Complete zero-CPU-rasterization compliance: strictly GPU-executed, zero CPU scanline fill, zero readback.
  - Native status: `render_retail_frame` is currently an unresolved linker symbol. No draw path exists yet.

---

## 3. Critical Path Summary

```
[Core Plugins Completed (0401..040b)]
                │
                ▼
[Explicit Host HiddenGl Render Target Allocation] (Assumed / Open)
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
