# Native 0407 shared raster service — implementation scope

Stage: compiled and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle or game-acceptance result.

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
| 2B9E50 | Allocate record; copy 32-bit dimensions/depth, parent=self, zero flags/offsets. Invoke backend create callback. Refusal frees record; success runs extension initialization and ignores extension refusal return. No invented stride/format/backing. |
| 2B9C70 | Reverse extension destruction BEFORE platform destroy; ignore platform return, release record, return true (1). Views do not release root storage. |
| 2B9AE0 / 2B9CB0 / 2B9D50 | Context target stack: push publishes stack entry ONLY AFTER context callback success. Successful push of null still returns null after updating the stack. Pop decrements BEFORE previous-target callback and does NOT restore index on failure. Stack grows dynamically; invalid underflow fails by name (`context_underflow`), rather than accessing adjacent memory. |
| 2D03B0 / 2B9B60 | Standard context setter only publishes current target pointer; does not bind a framebuffer or issue draw calls. Later rendering owns real GPU effects. |
| 2B9DF0 / 2C9810 | Subview requires flags 0x80 (no allocation). Dimensions and wrapping 16-bit parent+rectangle offsets store BEFORE callback: `(0xFFFF, 0x7FFF) + (1, 1) -> (0x0000, 0x8000)`. Only on callback success does root alias point to parent root. Backend copies stride/depth/type/format from root. Views do not free root storage. |
| 2B9D90 | Mip query: format bit 0x80 clear -> 1; otherwise backend query. Refusal returns signed -1. |
| 2C8BD0 / format tables | Type = `flags & 7`, flags byte = `low byte & 0xF8`, format byte from `0xFF00`. Type 4 (texture) without explicit format normalizes to `0x0500` / depth 32. Format `0x0500` -> RGBA 8888 (word `0x78123456` from RGBA `12 34 56 78`); format `0x0600` -> RGBX 8888 (word `0xFF123456`, alpha forced to 255). |
| 2C0E90 / 2D14C0 | Image to raster transfer: image conversion success precedes gamma flag 2 propagation into raster private bit 1 (`raster.private_flags |= 2`). Reject compressed target conversion. Conditionally acquire/release pixel/palette locks, convert by actual depth/format. |
| 2C85F0 / 2C8960 | Lock and unlock raster pixel and palette memory. Subview lock calculates offset into parent root backing `(subview.offset_y * root.stride) + (subview.offset_x * bpp)`. Double lock or unlock when not locked rejected explicitly. |
| NativeGlTextureUpload | Native GPU backing integration: GL texture upload and revocation via `NativeGlTextureUpload`. Subviews share root GPU texture without separate allocation. |

## Executed verification and regressions

- `test_native_retail_renderer_rasters`:
  - Verified default sentinel (flags 0x80, zero dimensions/depth, no GPU readiness).
  - Verified format normalization (`0x0500` / depth 32 for Type 4 textures, Type 5 camera targets, Type 1 depth buffers).
  - Verified context target stack push refusal leaves stack unchanged; push success updates target; push null target; pop decrements before callback and does not restore index on callback refusal; named underflow prevention.
  - Verified 16-bit offset wrapping for subviews: `(0xFFFF, 0x7FFF) + (1, 1) -> (0x0000, 0x8000)`.
  - Verified subview backend refusal preserves updated metadata stores but does NOT publish root alias.
  - Verified extension forward construction, reverse rollback on refusal, caller ignoring extension failure, and reverse extension destruction before platform destroy.
  - Verified mip queries (non-mipped format bit 0x80 clear -> 1; mipped format bit 0x80 set -> backend query).
  - Verified 0406 image -> 0407 raster transfer: scalar conversion for `0x0500` (RGBA `0x78123456`), `0x0600` (RGBX `0xFF123456`), and gamma flag 2 propagation into raster private bit 1.
  - Verified pixel locking, subview root-offset locking, and double lock/unlock rejections.
  - Verified `NativeGlTextureUpload` GPU texture upload and revocation on raster destruction.
  - Verified thread safety (`wrong_thread`) and Session expiration (`retired`).
  - Verified Main transfer and Core plugin forward construction and reverse unwind on later (0408) refusal.

- Full Regression Suite Passing:
  - `test_native_retail_renderer_images` PASS
  - `test_native_retail_renderer_cameras` PASS
  - `test_native_retail_renderer_streams` PASS
  - `test_native_retail_renderer_frames` PASS
  - `test_native_retail_renderer_core` PASS
  - `test_native_retail_renderer_matrix` PASS
  - `test_native_retail_renderer_raster_registration` PASS
  - `test_native_retail_renderer_allocator` PASS

- Production Symbols Verified:
  - `mka::retail::renderer_required::construct_core_plugin_0407` -> Defined (`T`) in `libmka_native_retail_startup.a`.
  - `mka::retail::renderer_required::destroy_core_plugin_0407` -> Defined (`T`) in `libmka_native_retail_startup.a`.
