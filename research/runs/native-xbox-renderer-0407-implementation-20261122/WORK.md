# Native 0407 raster implementation

Entry:
Parent: e87884fc92f00643a4abe3ca5ec473fcd9d90331 (main/origin-main)
Fork: c27fbc44495fb7f355ed7ce39c6465f713c6b1ca (main/origin-main)
Baseline link diagnostic: 47 required link effects (including construct_core_plugin_0407 and destroy_core_plugin_0407)

Preserve unrelated parent untracked evidence and all original assets/audio/user saves/cards/states/backups/drivers/model/settings.
Zero guest emulation (no g_ram, g_spr, g_memory, IOP/SPU2 emulation, JITs).

## Implemented Architecture & Contracts:

1. Real typed raster records and ownership (`NativeRetailRasterHandle`, `NativeRetailRasterRecord`, `NativeRetailRendererRasters`).
   - Managed via Core/Session/Permanent allocator ownership.
   - Resource teardown releases actual payloads directly; does not invent semantic per-raster callbacks.
2. Default Sentinel:
   - Zero dimensions, depth 0, format 0, flags 0x80.
   - Initial context target at stack index 0.
   - Non-allocated, non-render-ready metadata sentinel.
3. Raster Creation & Format Normalization:
   - Type 4 (texture) defaults to format `0x0500` (RGBA 8888) and depth 32.
   - Format `0x0600` retains RGBX 8888 (alpha forced to 255).
   - Stride calculation enforced with 4-byte host alignment.
   - Backend create callback invoked; refusal frees record and returns failure.
4. Extensions:
   - Forward construction of registered extensions.
   - Extension failure triggers reverse rollback of earlier extensions, but raster creation still succeeds (extension return ignored by caller).
   - Raster destruction executes reverse extension destruction BEFORE platform backend destroy callback.
5. Context Target Stack:
   - Push publishes stack entry ONLY AFTER context callback success.
   - Successful push of null target still returns null after updating the stack.
   - Pop decrements stack index BEFORE selecting previous target; callback refusal does NOT restore the index.
   - Named underflow prevention (`context_underflow`) prevents popping below default sentinel.
6. Subviews & 16-bit Offset Wrapping:
   - Subview requires flags 0x80.
   - Dimensions (`width`, `height`) and 16-bit wrapping offsets stored BEFORE backend callback:
     `parent_offset = (0xFFFF, 0x7FFF)` + `rect = (1, 1)` -> `(0x0000, 0x8000)`.
   - Backend refusal preserves stored metadata on record, but root alias is NOT set.
   - On backend success, `root_alias` points to root; stride, depth, type, format copied from root.
   - Destroying subview does NOT free root storage.
7. Mip Count Query:
   - Format bit 0x80 clear -> returns 1.
   - Format bit 0x80 set -> backend query (returns signed -1 on refusal).
8. Image to Raster Transfer (`set_from_image`):
   - Integrates with `NativeRetailRendererImages` (0406).
   - Converts source RGBA pixels to Little-Endian scalar words:
     - Format `0x0500` -> word `0x78123456` from RGBA `12 34 56 78` (in memory: `56 34 12 78`).
     - Format `0x0600` -> word `0xFF123456` (in memory: `56 34 12 FF`).
   - Image conversion success precedes gamma flag 2 propagation into raster private bit 1 (`raster.private_flags |= 2`).
9. Locking & Unlocking:
   - Pixel locking returns span of root buffer.
   - Subview locking returns span offset into root pixels: `(subview.offset_y * root.stride) + (subview.offset_x * bpp)`.
   - Double lock and unlock-when-not-locked rejected with `lock_state`.
10. Native GPU Integration:
   - Connects with `NativeGlTextureUpload` via `attach_gpu_device`.
   - Uploads converted RGBA bytes to host GL texture without guest console texture headers or console memory pitch artifacts.
   - Revokes GPU texture on raster destruction.
11. Core Integration:
   - `NativeRetailRendererCore::rasters`
   - `NativeRetailRendererCore::module_0407_count`
   - `NativeRetailRendererCore::register_raster_extension`
   - Production bindings `construct_core_plugin_0407` and `destroy_core_plugin_0407`.

## Verification Results:
- `test_native_retail_renderer_rasters`: PASS
- Regressions:
  - `test_native_retail_renderer_images`: PASS
  - `test_native_retail_renderer_cameras`: PASS
  - `test_native_retail_renderer_streams`: PASS
  - `test_native_retail_renderer_frames`: PASS
  - `test_native_retail_renderer_core`: PASS
  - `test_native_retail_renderer_matrix`: PASS
  - `test_native_retail_renderer_raster_registration`: PASS
  - `test_native_retail_renderer_allocator`: PASS
- Missing bindings diagnostic:
  - Previous baseline: 47 undefined symbols.
  - Resolved: `construct_core_plugin_0407`, `destroy_core_plugin_0407`.
  - Current count: exactly 45 undefined symbols.
  - Next required bindings: `construct_core_plugin_0408` and `destroy_core_plugin_0408`.
