# Native Graphics Architecture & Backend Specification

## 1. Executive Summary & Purpose

This document establishes the official graphics architecture for the native Windows PC recompilation of *Mortal Kombat: Armageddon* (`SLUS_214.10`).

The target platform for this port is **modern 64-bit Windows PC hardware**. All rendering operations terminate directly in modern native PC graphics APIs with high-performance desktop GPU drivers.

---

## 2. Official Graphics Backends

The project recognizes two authoritative graphics pipelines for Windows:

### A. Primary Production Backend: Microsoft DirectX 12
- **Implementation**: `runtime/src/gs/gpu/gpu.cpp` (Windows D3D12 device & swapchain).
- **Shader Compilation**: Microsoft DirectX Shader Compiler (DXC) via `dxcompiler.dll` and `dxil.dll`.
- **Target Feature Level**: Direct3D 12 Feature Level 12_0+.
- **Execution Model**: Hardware command queues, compute pipeline states, descriptor heaps, and explicit frame fences.

### B. High-Performance Direct Route: Desktop OpenGL 4.6
- **Implementation**: `runtime/src/gs/native_gl_backend.cpp`.
- **Target Profile**: OpenGL 4.6 Compatibility Profile Context.
- **Execution Model**: Direct framebuffer objects (FBOs for color and depth per FBP target), asynchronous GL timer queries, direct texture uploads (`glTexSubImage2D`), and immediate scene blits.

---

## 3. Formal Declaration Regarding WebGPU (Unused Upstream Artifact)

> **Architectural Policy:** WebGPU is formally declared as an **unused upstream transitional artifact**. It is **not** part of the production design, identity, or purpose of this Windows PC port.

1. **Origin**: WebGPU headers and Dawn abstractions were inherited from the generic upstream multi-platform emulation harness.
2. **Deactivation on Accepted Route**: On the accepted boot and retail gameplay path (`PS2_NATIVE_GL=1` or `PS2_GS_PACKET_NATIVE=1`), WebGPU is entirely deactivated:
   ```text
   [gs] native OpenGL/direct-packet mode selected; WebGPU presentation disabled
   ```
3. **Windows Native Direct3D 12 Usage**: When the GPU backend is selected on Windows, the runtime bypasses browser/WebGPU semantics and directly targets Direct3D 12 via DXC (`[gpu] graphics API: Direct3D 12 (the default on Windows)`).
4. **Acceptance Gate Relationship**: Unused WebGPU source files have zero presence on the accepted execution route, execute zero guest instructions, and create zero emulated state machines. They remain harmlessly inert and out of the runtime path.

---

## 4. Native Texture & Geometry Ingestion

### A. Dynamic Texture Uploads (`guest_wrote_vram`)
Rather than staging texture data in an emulated 4 MB PS2 VRAM array, the native graphics layer intercepts HostToLocal transfers and directly provisions native OpenGL / Direct3D 12 textures:
- PS2 texture formats (**PSMCT32**, **PSMCT16**, **PSMT8**, and **PSMT4** with CLUT palettes) are unswizzled and decoded into 32-bit linear RGBA buffers via `raster::decode_rect_rgba`.
- Native texture cache entries (`TextureCache g_pool[kTexturePool]`) allocate standard 2D GPU textures and upload the decoded rectangles immediately via `glTexImage2D` / `glTexSubImage2D`.
- Result: **Zero GS transfers stop in emulated VRAM** (`GS-stopped-in-VRAM = 0`).

### B. AOT Vector Unit (VU1) Geometry Pipeline
- Proprietary geometry streams and skinning transformations are handled by **490 statically precompiled vector programs** (`runtime/src/vu_micro.cpp`).
- Vertex coordinates, normals, UVs, and RGBA colors are converted ahead of time into native vertex buffers, bypassing any microcode or instruction interpreter.
- Primitives are submitted directly to the native GPU pipeline (`native-gl` or `Direct3D 12`).
