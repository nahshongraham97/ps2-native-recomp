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

### Authentic registration site and binary identities
Plugin 0x0412 is registered in the engine plugin registration table:
- Xbox `default.xbe`: registered at `0x002c0194` with size `4` bytes (`push 4`), plugin ID `0x0412` (`push 0x412`), constructor `0x002cd7d0`, destructor `0x002cd820`, invoking engine plugin registration helper `0x002c2990` (`call 0x002c2990`).
- PS2 `SLUS_214.10`: registered at `0x001a6818` with size `4` bytes (`addiu a0, zero, 4`), plugin ID `0x0412` (`addiu a1, zero, 0x412`), constructor `0x001a3df0`, destructor `0x001a3e70` (in branch delay slot of `jal 0x1a6a20`).

### Authentic Xbox default.xbe source words
- `0x002cd7d0` (authentic constructor `construct_core_plugin_0412`):
  `0x6a880d8b 0x158b005a 0x005a6a84 0x0824448b 0x04041268 0x75f46800 0x6a51005c 0x216a5204 0x5c7618a3 0x6e26e800 0x0d8bffff 0x005c7618`
  Disassembly loads plugin global base, pushes plugin parameters (including size `4` and plugin ID `0x00040412`), stores return offset to `[0x005c7618]`, and at offset `0x25` (`0x002cd7f5`) calls shared helper `0x002c4620` (`call -0x91da`).
- `0x002cd820` (authentic destructor `destroy_core_plugin_0412`):
  `0x5c7618a1 0x40808b00 0x85008353 0x500974c0 0xff678be8 0x04c483ff 0x761c0dff 0x448b005c`
  Disassembly loads plugin offset from `[0x005c7618]`, tears down plugin callbacks via helper `0x002c3fc0`, and decrements the module counter at `0x005c761c`.
- `0x002c4620` (shared plugin extension registry and freelist block allocator helper):
  `0x1824448b 0x10244c8b 0x0824548b 0x24448b50 0x4c8b5108 0x50521424 0x2424448b 0xfffe7fe8`
  Plainly named: `0x002c4620` is the shared plugin extension / freelist block allocator helper (`_rwPluginRegistryAddPlugin` / freelist block creator). It is called across multiple plugins, including:
  1. Plugin 0409 (texture dictionaries): `0x002c9910` (module open) calls `0x002c4620` at `0x002c994c`.
  2. Plugin 0412 (world): `0x002cd7d0` (constructor) calls `0x002c4620` at `0x002cd7f5`.

### Authentic PS2 SLUS_214.10 cross-check
- `0x001a3df0` (authentic constructor):
  `0x27bdffe0 0x3c08006a 0xffbf0010 0x3c020004 0x7fb00000 0x24060004 0xaf85a3c8 0x0080802d 0x8f858388 0x24040021 0x8f87838c 0x2508f230`
  Disassembly checks module count, sets up plugin size `4` and ID `0x00040412`, calls extension helper `0x001a0fd0` at `0x001a3e20`, and increments module counter at `-23604($gp)`.
- `0x001a3e70` (authentic destructor):
  `0x27bdffe0 0x3c02006a 0xffbf0010 0x2442f290 0x7fb00000 0x8f83a3c8 0x0080802d 0x00431021`
  Disassembly unregisters plugin data via helper `0x001a10a0` at `0x001a3e9c` and decrements module counter at `-23604($gp)`.
- Registration site: `0x001a6818` - `0x001a6828` registers plugin ID `0x0412` with a 4-byte container size.

## Reconciliation of Earlier Misattributed Addresses

An earlier progress update mistakenly cited `0x002c4480` (Xbox) and `0x0019fc80` (PS2) as the constructor:
1. `0x002c4480` on Xbox is an inner extension helper function physically adjacent to the shared freelist allocator helper `0x002c4620`.
2. `0x0019fc80` on PS2 is an inner sector/vector math function containing MIPS COP1 floating-point instructions (`c.lt.s`, `add.s`).
3. Neither `0x002c4480` nor `0x0019fc80` is registered in the engine plugin table. Disassembly of the engine registration tables at Xbox `0x002c0194` and PS2 `0x001a6818` definitively proves that `0x002cd7d0` (Xbox) and `0x001a3df0` (PS2) are the authentic constructor entry points registered for Plugin `0x0412`.
4. Plugin 0412 does NOT call 0409's module open (`0x002c9910` / PS2 `0x001a7690`) or module close (`0x002c9890` / PS2 `0x001a75d0`). Both 0409 and 0412 call the shared plugin extension / freelist block allocator helper `0x002c4620` (PS2 `0x001a0fd0`).
5. Plugin 0412 manages its own dedicated module counter at `0x005c761c` (PS2 `-23604($gp)`), completely separate from 0409 (`0x005c75e4` / PS2 `-23664($gp)`).

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
