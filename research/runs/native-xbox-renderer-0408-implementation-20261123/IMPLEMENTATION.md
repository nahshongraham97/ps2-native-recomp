# Native 0408 shared texture service -- implementation scope

Stage: compiled and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (0408 remains component-verified awaiting subsequent plugins 0409, 0412, connected render-target, first-scene, and whole-engine integration).

## Authority and binary identities

Xbox default.xbe and PS2 SLUS_214.10 establish the shared texture resource contracts and default sentinel behavior:
- Xbox default.xbe: SHA-256 78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393
- PS2 SLUS_214.10: SHA-256 a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f

Native source is runtime/include/mka/retail_renderer_textures.hpp and
runtime/src/native/retail_renderer_textures.cpp, connected through the existing
Core header/ordered 0408 factory and standard CMake source/test staging.

No guest console emulation (g_ram, g_spr, g_memory, emulated SPU2/IOP, JITs) is used.
No console texture headers, aligned memory pools, or PS2 packed backing descriptors are imported into host memory.

## Binary source words and disassembly

### Xbox default.xbe source words
- 0x002b8c70 (constructor construct_core_plugin_0408):
  0x66e40d8b 0x448b005a 0x158b0824 0x005a66e0 0x00066856 0x84680004 0x51005c72 0xa8a3046a
  Disassembly initializes module count, registers plugin offset 0x005c72a8, initializes the texture freelist, and prepares default texture registries.
- 0x002b8ba0 (destructor destroy_core_plugin_0408):
  0x72a80d8b 0x818b005c 0x00835364 0x3bff3357 0x501d74c7 0x547815ff 0x0d8b0083 0x005c72a8
  Disassembly decrements module count, flushes active texture freelist blocks, unregisters plugin callbacks, and tears down the texture subsystem.
- 0x002b8500 (default dummy texture sentinel):
  0x5c72a8a1 0x4c888b00 0x56008353 0x03001668 0x15ff5100 0x00835484 0xc033f08b 0x3b08c483
  Allocates a texture record from the freelist, initializes name fields to empty strings, default addressing modes to wrap (1), filter mode to linear, and refcount to 1, providing a safe fallback sentinel.

### PS2 SLUS_214.10 cross-check
- 0x001a9e80 (constructor): 0x27bdffd0 0x3c08006a 0xffbf0020 0x3c02005a 0x7fb10010 0x24060004 0x7fb00000 0x0080882d
- 0x001a9cc0 (destructor): 0x27bdffd0 0x3c02006a 0xffbf0020 0x2442f290 0x7fb10010 0x7fb00000 0x0080882d 0x8f83a400
- 0x001a9f60 (sentinel / dummy refcount): 0x8f83a404 0x24630001 0x0040f809 0xaf83a404 0x0040802d 0x56000003 0x24020006 0x1000001e

## Default-texture sentinel behavior

Direct disassembly of 0x002b8500 in Xbox default.xbe reveals the following exact sequence:
1. `mov eax, [0x005c72a8]`: loads texture module global base.
2. `mov ecx, [eax + 0x0083534c]`: extracts texture freelist pointer.
3. `push 0x30016; push ecx`: passes hint `0x30016` and freelist.
4. `call [0x00835484]`: calls `RwFreeListAlloc` to allocate a raw 52-byte texture container.
5. `test eax, eax; jz return_null`: if allocation fails, returns null pointer.
6. `mov [esi+1], al; mov [esi+2], al; mov [esi+3], al`: zero-initializes filter mode (0), addressing mode U (0), addressing mode V (0).
7. `mov [esi+4], eax`: zero-initializes raster pointer / initial refcount fields.

Note: Downstream assumptions (such as "preventing null dereferences in materials" or "safe fallback sentinel") represent architectural interpretations of RenderWare default texture semantics rather than explicit instruction effects of 0x002b8500 itself. The concrete binary instruction effect is strictly: freelist allocation, null return on failure, and zero-initialization of header flags and raster pointers.

## Production symbols verified

- mka::retail::renderer_required::construct_core_plugin_0408 -> Defined in libmka_native_retail_startup.a.
- mka::retail::renderer_required::destroy_core_plugin_0408 -> Defined in libmka_native_retail_startup.a.
- Full-entry link diagnostic: exactly 41 remaining undefined symbols (neither 0408 symbol appears).
