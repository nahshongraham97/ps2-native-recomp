# Native movie-font geometry checkpoint — 2026-11-19

Eight standalone cases pass in the isolated native-only build: the previous
seven metadata/font/pipeline checks plus source-derived glyph geometry.
The actual title query→language→layout→geometry chain retains112 quads after
original owners retire. Separate mandatory logical-screen, draw-canvas and
atlas extent pairs prevent hidden equality/default assumptions. Both source
vertical scales, original float32 operation/conversion order, negative
fractional cases, overflow/failure behavior and copied-value lifetime pass.

Geometry remains native64 without source sixteen-bit wrapping. Synthetic
10000×7000 coordinate/8192×4096 atlas-data cases exercise math only, not GPU
texture-capacity acceptance. Ordered source calculations retain the serialized
coordinate-origin arithmetic;2048 is not a framebuffer/window/resource cap.
Per-source strict floating-point compiler options disable fast-math and FMA;
the parent FONT_STRICT_FP_BUILD_POLICY.json records actual LLVM-MinGW commands.
MSVC option presence is source configuration, not an executed MSVC test.

No framebuffer coverage, sampler/CLAMP/PABE/FBA policy, final composition,
actual clock/A-V/audio completion, game binding or accepted build is claimed.
Raw GPU/video and metadata/font data tests remain separate from gameplay.
Exact source, binaries, inputs, commands, exits and symbol checks are in
verification.json. No game, GPU driver/settings, backup or card was changed.
Whole-game acceptance remains OPEN.
