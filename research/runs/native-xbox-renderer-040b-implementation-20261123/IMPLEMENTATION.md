# Native 040b Shared Geometry Service -- Implementation Scope

Stage: compiled and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (040b remains component-verified awaiting connected render-target, first-scene, and whole-engine integration).

## Authority and binary identities

Xbox `default.xbe` and PS2 `SLUS_214.10` establish the geometry contracts, container sizes, and plugin registration sequence:
- Xbox `default.xbe`: SHA-256 `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- PS2 `SLUS_214.10`: SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Native source is `runtime/include/mka/retail_renderer_geometry.hpp` and `runtime/src/native/retail_renderer_geometry.cpp`, connected through the existing Core header/ordered 040b factory and standard CMake source/test staging.

No guest console emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JITs) is used.
No console geometry structures, raw pointer arrays, or GS packets are imported into host memory.

## Binary source words and disassembly

### Xbox default.xbe source words
- `0x002c2d40` (constructor `construct_core_plugin_040b`):
  `0x0824448b 0x3d8b5756 0x00835494 0x5340b08d 0x18a30083 0xe8008353 0xffffff24 0x5e5fc085`
  Initializes geometry subsystem, registers plugin ID `0x040b` with 40-byte container size, and increments module counter at `0x0083531c`.
- `0x002c30a0` (destructor `destroy_core_plugin_040b`):
  `0xffff3be8 0x5318a1ff 0x888b0083 0x0083534c 0xb47ae851 0x158b0000 0x00835318 0x534c828b`
  Decrements module counter at `0x0083531c`, flushes geometry freelist, and unregisters plugin callbacks.
- Registration site: `0x002c021a` registers plugin ID `0x040b` with a 40-byte container size.

### PS2 SLUS_214.10 cross-check
- `0x001a1b30` (constructor):
  `0x27bdffe0 0x3c02006a 0xffbf0010 0x2442f290 0x7fb00000 0x0080802d 0xaf85a3a0 0x00452021`
  Initializes geometry plugin data and increments module counter at `-23648($gp)`.
- `0x001a1b90` (destructor):
  `0x27bdffe0 0xffbf0010 0x7fb00000 0x0c06884c 0x0080802d 0x8f83a3a0 0x3c02006a 0x2442f290`
  Decrements module counter at `-23648($gp)` and unregisters plugin.
- Registration site: `0x001a6858` registers plugin ID `0x040b` with a 40-byte container size.

## Native Implementation & Generational Handles

The native implementation:
- Enforces RAII and thread affinity on all geometry operations.
- Represents geometry resources via typed generational handles (`RetailRendererGeometryHandle`).
- Supports vertex and triangle count description, flag configuration, and explicit/refcounted destruction.
- Does not expose console-specific microcode or hardware vertex buffers.

## Production symbols verified

- `mka::retail::renderer_required::construct_core_plugin_040b` -> Defined in `libmka_native_retail_startup.a`.
- `mka::retail::renderer_required::destroy_core_plugin_040b` -> Defined in `libmka_native_retail_startup.a`.
- Full-entry link diagnostic: exactly 33 remaining undefined symbols (reduced from 35).
