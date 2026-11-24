# Native 040a Shared Material Service -- Implementation Scope

Stage: compiled and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (040a remains component-verified awaiting subsequent plugin 040b, connected render-target, first-scene, and whole-engine integration).

## Authority and binary identities

Xbox `default.xbe` and PS2 `SLUS_214.10` establish the material contracts, property setters/getters, and plugin registration sequence:
- Xbox `default.xbe`: SHA-256 `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- PS2 `SLUS_214.10`: SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Native source is `runtime/include/mka/retail_renderer_materials.hpp` and `runtime/src/native/retail_renderer_materials.cpp`, connected through the existing Core header/ordered 040a factory and standard CMake source/test staging.

No guest console emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JITs) is used.
No console material headers, raw pointer arrays, or GS extensions are imported into host memory.

## Binary source words and disassembly

### Xbox default.xbe source words
- `0x002c0700` (constructor `construct_core_plugin_040a`):
  `0x00ae840f 0x20a10000 0x8b008353 0x8d10244c 0x835384b0 0x2046c700 0x00000000 0xc7244e89`
  Initializes material subsystem and registers plugin ID `0x040a`.
- `0x002c0920` (destructor `destroy_core_plugin_040a`):
  `0x00022c46 0xe5e80000 0x660000d5 0x1c244c8b 0x66284689 0x568b0889 0x448b6628 0x006a2024`
  Decrements module counter, flushes material freelist, and unregisters plugin.
- `0x002c09d0` (color setter `RpMaterialSetColor`):
  `0x24ff6b77 0x2c0a6885 0x5320a100 0x808b0083 0x00835344 0x08c4835e 0x200d8bc3 0x8b008353`
  Sets 4-byte RGBA color on material container.
- `0x002c0ae0` (color getter `RpMaterialGetColor`):
  `0x002c0c64 0x53200d8b 0x81890083 0x00835344 0xc308c483 0x5320158b 0x82890083 0x00835348`
  Retrieves 4-byte RGBA color pointer.
- `0x002c09b0` (surface properties setter `RpMaterialSetSurfaceProperties`):
  `0x835320a1 0x40808b00 0xc3008353 0xcccccccc 0x04244c8b 0x5608ec83 0x33ff418d 0x05f883f6`
  Stores ambient, diffuse, and specular float coefficients.
- `0x002c0ad0` (surface properties getter `RpMaterialGetSurfaceProperties`):
  `0x24548b00 0xff4a8d10 0x7705f983 0x8d24ff67 0x002c0c64 0x53200d8b 0x81890083 0x00835344`
  Returns pointer to surface property floats.
- `0x002c08e0` (texture setter `RpMaterialSetTexture`):
  `0x5608ec83 0x9306e857 0xf88b0000 0x835320a1 0x7c888b00 0x85008353 0x83840fc9 0x8d000000`
  Attaches texture handle to material and increments texture refcount.
- `0x002c0c70` (add reference `RpMaterialAddRef`):
  `0x002c0b04 0x002c0af4 0x002c0b34 0x002c0beb 0x002c0c06 0x002c0b82 0x002c0bd0 0x002c0ba9`
  Increments material refcount.
- `0x002c0cd0` (destroy / dereference `RpMaterialDestroy`):
  `0xa10000d6 0x00835324 0x4808c483 0x835324a3 0x24448b00 0xccccc304 0xcccccccc 0xcccccccc`
  Decrements refcount; frees material to freelist when count reaches 0.

### PS2 SLUS_214.10 cross-check
- `0x001a2130` (constructor):
  `0x27bdffb0 0x3c04006a 0xffbf0040 0x2484f290 0x7fb30030 0x7fb20020 0x7fb10010 0x7fb00000`
- `0x001a2190` (destructor):
  `0x8e730000 0x0040f809 0x0240202d 0x00000000 0x8e420010 0x10400002 0x00000000 0xac400000`
- Registration site: `0x001a6850` registers plugin ID `0x040a` with a 124-byte container size (Xbox default.xbe `0x002c0213` registers 116 bytes).

## Console Container Size Variance Policy

Material container sizes vary between Xbox (116 bytes) and PS2 (124 bytes). In accordance with project policy:
- Console container byte sizes do NOT dictate native host object layouts.
- Host implementation uses typed C++ objects with RAII and generational handles (`NativeRetailRendererMaterials`).
- Neither console's struct layout is adopted; typed interfaces provide exact color, surface property, texture binding, and refcounting semantics.

## Production symbols verified

- `mka::retail::renderer_required::construct_core_plugin_040a` -> Defined in `libmka_native_retail_startup.a`.
- `mka::retail::renderer_required::destroy_core_plugin_040a` -> Defined in `libmka_native_retail_startup.a`.
- Full-entry link diagnostic: exactly 35 remaining undefined symbols at 040a milestone.
