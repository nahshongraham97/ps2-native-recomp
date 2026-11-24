# Native 0412 Shared World Service -- Implementation Scope

Stage: compiled and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (0412 remains component-verified awaiting subsequent plugins 040a, 040b, connected render-target, first-scene, and whole-engine integration).

## Authority and binary identities

Xbox `default.xbe` and PS2 `SLUS_214.10` establish the world module contracts and plugin registration sequence:
- Xbox `default.xbe`: SHA-256 `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`
- PS2 `SLUS_214.10`: SHA-256 `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`

Native source is `runtime/include/mka/retail_renderer_world.hpp` and `runtime/src/native/retail_renderer_world.cpp`, connected through the existing Core header/ordered 0412 factory and standard CMake source/test staging.

No guest console emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JITs) is used.
No console world structures, raw pointer arrays, or sector freelist blocks are imported into host memory.

## Binary source words and disassembly

### Xbox default.xbe source words
- `0x002c4480` (constructor `construct_core_plugin_0412`):
  `0xe8044289 0xfffffad8 0x5c745ca1 0x745c3d00 0xdc75005c 0x748c3d8b 0x4f8b005c 0x1c578b20`
  Disassembly checks module count, registers plugin offset, and calls helper `0x002c4620`.
- `0x002c3fc0` (destructor `destroy_core_plugin_0412`):
  `0x247c8b57 0x20478b08 0x891c4f8b 0x1c578b08 0x8920478b 0x85e80442 0xb8ffffff 0x00000001`
  Disassembly decrements module counter, unregisters plugin callbacks, and clears the module pointer.
- `0x002c4620` (registry helper called by constructor):
  `0x1824448b 0x10244c8b 0x0824548b 0x24448b50 0x4c8b5108 0x50521424 0x2424448b 0xfffe7fe8`
  Disassembly demonstrates this is a generic registry helper (`0x002c4620`), not plugin 0409's texture dictionary module open (`0x002c9910`).

### PS2 SLUS_214.10 cross-check
- `0x0019fc80` (constructor):
  `0x4600001c 0x46000144 0x00000000 0x00000000 0x46042836 0x45010008 0x00000000 0x3c023f80`
- `0x0019fb20` (destructor):
  `0x27a50020 0x1000000a 0x27a30010 0x27a50020 0x10000007 0x27a30010 0x27a50030 0x10000004`
- Registration site: `0x001a6860` registers plugin ID `0x0412` with a 4-byte container size.

## Correction of Disassembly Typo & Call Target Analysis

An earlier inspection note contained a transcription typo citing `0x002c461d` instead of `0x002c4620`. Disassembly confirms:
1. The call target from constructor `0x002c4480` is `0x002c4620`.
2. Plugin 0412 does NOT call 0409's module open (`0x002c9910` / PS2 `0x001a7690`) or module close (`0x002c9890` / PS2 `0x001a75d0`).
3. Plugin 0412 manages its own module counter at `0x008354b8` (PS2 `-23544($gp)`), independent of 0409.

## Scoped 4-Byte World Container & Unrecovered RenderWare Convention Names

In authentic retail binaries, plugin `0x0412` registers a minimal 4-byte container (Xbox `0x002c0228`, PS2 `0x001a6860`).
High-level RenderWare world names (e.g. `RpWorld`, `RpWorldAddCamera`, `RwBBox`) are unrecovered conventional library names. The native implementation adheres strictly to:
- Recovered 4-byte container semantics and generational handles.
- Module lifecycle tracking and fail-closed rollback.
- Elimination of unevidenced bounding-box queries, name strings, and observation queries.

## Matrix Runner Fixture Exclusivity Collision and Resolution

During initial test matrix execution across all component clients, tests executing `images` (`test_native_retail_renderer_images`) and `streams` (`test_native_retail_renderer_streams`) experienced collision when sharing default working directory fixtures.
The runner `run_check.py` was extended to:
1. Auto-supply dedicated per-target output and fixture paths via `KNOWN_CLIENTS`.
2. Automatically clean fixture directories prior to execution.
3. Validate required fixture existence before starting clients.
4. Enforce deterministic gzip compression (`mtime=0`, `filename=""`, recording uncompressed SHA-256).

## Production symbols verified

- `mka::retail::renderer_required::construct_core_plugin_0412` -> Defined in `libmka_native_retail_startup.a`.
- `mka::retail::renderer_required::destroy_core_plugin_0412` -> Defined in `libmka_native_retail_startup.a`.
- Full-entry link diagnostic: exactly 37 remaining undefined symbols at 0412 milestone.
