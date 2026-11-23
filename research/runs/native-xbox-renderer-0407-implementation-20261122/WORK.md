# Native 0407 raster implementation and defect remediation

Entry:
- Parent: `35ab91fe7396c79f9d024440a6b6c8fa25a05353` (origin/main)
- Fork: `2ee8738d82d82d62c4418bfde21db4804e5861aa` (origin/main)
- Full-entry link diagnostic: exactly 45 remaining undefined symbols (resolving `construct_core_plugin_0407` and `destroy_core_plugin_0407`).
- Zero guest emulation boundary: zero `g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP, JIT/interpreter, or console memory header transplants.
- Acceptance status: **OPEN** (0407 remains OPEN awaiting subsequent render-target and whole-scene integration; not claiming completion or advancement to 0408).

## In-place defect remediation (5 review defect areas resolved)

1. **Memory safety & bounds validation (`set_from_image`)**:
   - Explicit destination capacity validation: destination width and height must be $\ge$ source image width and height (`extent`).
   - Image format validation: source image depth must be 24 or 32 bpp; unsupported depths (such as 16bpp) fail with named refusal `unsupported_depth`.
   - Destination raster validation: destination depth must be 32, and format must be `0x0500` or `0x0600`.
   - Stride multiplication and buffer capacity: validated against `raster_span.size()` with overflow checks; source stride validated against `img_bytes.size()`.

2. **Real GPU backing & offscreen WGL verification**:
   - Replaced mock `TestGlTextureUpload` with real offscreen `HiddenGl` fixture (`WS_POPUP | WS_DISABLED` Win32 window, real WGL OpenGL context, `NativeMovieGlDevice`, `NativeMovieGpuRetirementOwner`).
   - Texture uploads verified with `glIsTexture(texture_id) == GL_TRUE`.
   - Strictly enforced GPU requirement: type 4/5 raster image conversion fails with `missing_backing` if GPU device is not attached.
   - Configurable sampler filtering and address modes (`set_sampler`, `sampler`, `NativeTextureFilter`, `NativeTextureAddress`).

3. **Source semantic alignment**:
   - **Gamma mask**: Original 2C0EB0 sets bit 0 (`raster->private_flags |= 1;`), corrected from bit 1 (`|= 2`).
   - **Pop result**: Original 2B9D50 returns the popped target after successful restoration while setting the current target to previous; corrected from returning previous target.
   - **Default format/depth**: Original 2C8BD0 type-4 default normalization unconditionally sets depth to 32 (`desc.depth = 32;`), not only when incoming depth is zero.

4. **Extension and Main-transfer lifetime**:
   - Refused extension construction sizes `rec->extensions` to full `extensions_.size()` without truncation, ensuring subsequent destruction traverses the full reverse definition chain.
   - Records published into `records_` before invoking backend create callback and extension constructors to ensure valid ownership during constructor Main reentry.
   - Resumable destruction cursor (`destroy_cursor`) on `NativeRetailRasterRecord` ensures that if destructor reentry occurs, retries resume from the uncompleted extension slot without re-executing already destroyed slots.
   - Destructor and constructor callback catch blocks catch and rethrow `detail::NativeMainReentry`.

5. **GPU retirement failures & lifecycle**:
   - Revocation failures return `after_effect_unknown` without swallowing.
   - Re-upload explicitly retires prior GPU texture before allocating a new texture backing.
   - Device replacement verifies live-resource ownership and revokes existing backings.
   - Subview destruction / root destruction explicitly invalidates GPU texture handles on aliasing views.

## Verification results

- `test_native_retail_renderer_rasters`: **PASS** (`PASS_NATIVE0407_SHARED_RASTERS`)
  - 12 comprehensive test sections:
    1. Default sentinel raster (flags 0x80, zero dimensions/depth, non-renderable).
    2. Creation, format normalization, backend create refusal.
    3. Context target stack push/pop, underflow prevention, popping returns popped target per 2B9D50.
    4. Subviews, 16-bit offset wrapping `(0xFFFF, 0x7FFF) + (1, 1) -> (0x0000, 0x8000)`.
    5. Extension lifecycle, forward construction, reverse rollback, caller ignores refusal.
    6. Mip queries (non-mipped format bit 0x80 clear -> 1, mipped format bit 0x80 set -> backend query).
    7. 0406 image -> 0407 raster transfer, Little-Endian scalar conversion for `0x0500` and `0x0600`, gamma bit 0 propagation per 2C0EB0, missing GPU rejection.
    8. Bounds safety, 2x2 into 1x1 rejection (`extent`), unsupported 16bpp rejection (`unsupported_depth`).
    9. Pixel and subview offset locking, double-lock and invalid-unlock rejection.
    10. Real GPU backing via `HiddenGl`, texture allocation verification via `glIsTexture`, subview texture aliasing, re-upload prior texture retirement, sampler options.
    11. Thread safety (`wrong_thread`) and Session expiration (`retired`).
    12. Extension refusal full-definition reverse destruction, destructor Main reentry resumption from `destroy_cursor`.

- Regressions (all 8 existing renderer component clients pass):
  - `test_native_retail_renderer_allocator`: **PASS**
  - `test_native_retail_renderer_cameras`: **PASS**
  - `test_native_retail_renderer_core`: **PASS**
  - `test_native_retail_renderer_frames`: **PASS**
  - `test_native_retail_renderer_images`: **PASS**
  - `test_native_retail_renderer_matrix`: **PASS**
  - `test_native_retail_renderer_raster_registration`: **PASS**
  - `test_native_retail_renderer_streams`: **PASS**

- Full-entry link diagnostic:
  - Exact 45 remaining undefined symbols; neither `construct_core_plugin_0407` nor `destroy_core_plugin_0407` appear.
