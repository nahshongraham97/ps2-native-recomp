# Native 0407 shared raster service — implementation scope

Stage: compiled, defect-remediated, and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or game-acceptance result.
Acceptance status: **OPEN** (0407 remains OPEN awaiting subsequent render-target, first-scene, and whole-engine integration; not advancing to 0408).

## Authority and recovery decision

Keep the published source-only record at
`../native-xbox-renderer-0407-20261122/RECOVERY.md` unchanged. It owns the
original Xbox/PS2 hashes, mappings and recovered raster algorithms.
Native source is `runtime/include/mka/retail_renderer_rasters.hpp` and
`runtime/src/native/retail_renderer_rasters.cpp`, connected through the existing
Core header/ordered 0407 factory and normal CMake source/test staging.

Xbox `default.xbe` (`78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`)
and PS2 `SLUS_214.10` (`a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`)
establish the shared generic raster resource contracts and Xbox scalar format conversion rules.
No guest console emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JITs) is used.
No console texture headers, aligned memory pools, or PS2 packed backing descriptors are imported into host memory.

## Native ownership and effect contracts

| Source behavior | Native destination / verification |
| --- | --- |
| 2C0020 / 2B9F70 / 2B9F10 | Actual Core/Session/Permanent raster resource. Default sentinel raster initialized with zero dimensions, depth 0, no backing, and flags 0x80 (metadata sentinel, not an allocated/render-ready framebuffer). Published as target 0 on context target stack. Module count increments on resource success. Module retirement releases actual payloads; like source resource teardown, it does not invent per-raster semantic callback calls. |
| 2B9E50 | Allocate record; copy 32-bit dimensions/depth, parent=self, zero flags/offsets. Invoke backend create callback. Refusal frees record; success runs extension initialization and ignores extension refusal return. Published into `records_` before callback to preserve ownership during Main reentry. |
| 2B9C70 | Reverse extension destruction BEFORE platform destroy; ignore platform return, release record, return true (1). Views do not release root storage. Resumable via `destroy_cursor` on Main reentry. |
| 2B9AE0 / 2B9CB0 / 2B9D50 | Context target stack: push publishes stack entry ONLY AFTER context callback success. Successful push of null still returns null after updating the stack. Pop decrements BEFORE previous-target callback and returns popped target per 2B9D50. Stack grows dynamically; invalid underflow fails by name (`context_underflow`), rather than accessing adjacent memory. |
| 2D03B0 / 2B9B60 | Standard context setter only publishes current target pointer; does not bind a framebuffer or issue draw calls. Later rendering owns real GPU effects. |
| 2B9DF0 / 2C9810 | Subview requires flags 0x80 (no allocation). Dimensions and wrapping 16-bit parent+rectangle offsets store BEFORE callback: `(0xFFFF, 0x7FFF) + (1, 1) -> (0x0000, 0x8000)`. Only on callback success does root alias point to parent root. Backend copies stride/depth/type/format from root. Views do not free root storage. |
| 2B9D90 | Mip query: format bit 0x80 clear -> 1; otherwise backend query. Refusal returns signed -1. |
| 2C8BD0 / format tables | Type = `flags & 7`, flags byte = `low byte & 0xF8`, format byte from `0xFF00`. Type 4 (texture) unconditionally normalizes depth to 32 and format `0x0500`. Format `0x0500` -> RGBA 8888 (word `0x78123456` from RGBA `12 34 56 78`); format `0x0600` -> RGBX 8888 (word `0xFF123456`, alpha forced to 255). |
| 2C0E90 / 2D14C0 | Image to raster transfer: memory safety bounds checked (destination extent $\ge$ source extent, depth 32, stride overflow checks, source depth 24/32 bpp, 16bpp rejected with `unsupported_depth`). Image conversion success precedes gamma flag 1 propagation into raster private bit 0 (`raster.private_flags |= 1`) per 2C0EB0. Type 4/5 raster conversion strictly requires attached GPU device (`missing_backing`). |
| 2C85F0 / 2C8960 | Lock and unlock raster pixel and palette memory. Subview lock calculates offset into parent root backing `(subview.offset_y * root.stride) + (subview.offset_x * bpp)`. Double lock or unlock when not locked rejected explicitly with `lock_state`. |
| Real GPU Backing | Native GPU backing integration: real offscreen `HiddenGl` fixture (`WS_POPUP | WS_DISABLED` Win32 window, real WGL OpenGL context, `NativeMovieGlDevice`, `NativeMovieGpuRetirementOwner`). Texture upload verified with `glIsTexture`. Prior textures explicitly revoked before re-upload and upon device replacement. Subviews alias root GPU texture; destruction invalidates aliased handles. |

## Defect remediation summary

1. **Memory safety**: Added destination extent and buffer capacity checks against `raster_span.size()`; source stride checks against `img_bytes.size()`; supported depth checks (rejecting 16bpp).
2. **Real GPU backing**: Replaced mock upload with real `HiddenGl` WGL context and `NativeMovieGlDevice`; verified real OpenGL texture IDs (`glIsTexture`); type 4/5 rasters require GPU device; added configurable sampler filters/addresses.
3. **Source semantics**: Gamma propagation corrected to bit 0 per 2C0EB0; pop context returns popped target per 2B9D50; type 4 default normalization unconditionally sets depth 32 per 2C8BD0.
4. **Lifetime & reentry**: Full extension definition chain preserved on refusal; records published prior to constructors for constructor Main reentry; resumable `destroy_cursor` in destructor; `NativeMainReentry` caught and rethrown.
5. **GPU retirement**: Revocation failures return `after_effect_unknown`; re-upload and device replacement retire existing GPU backings; subview destruction invalidates shared texture handles.

## Production symbols verified

- `mka::retail::renderer_required::construct_core_plugin_0407` -> Defined (`T`) in `libmka_native_retail_startup.a`.
- `mka::retail::renderer_required::destroy_core_plugin_0407` -> Defined (`T`) in `libmka_native_retail_startup.a`.
- Remaining undefined symbols in full-entry link: exactly 45 (neither 0407 symbol appears).
