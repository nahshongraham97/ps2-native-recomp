# Native movie/file component checkpoint — 2026-11-19

All 43 standalone cases pass. `verification.json` retains exact executable
hashes, arguments, asset/library hashes, stdout/stderr and exit codes.
`BUILD.json` binds current authored source and build cache, and separately
records the two preserved dirty IOP files which are excluded from this commit.

This batch adds owned native movie assets/64-bit file identity, exact catalog
decoder handoff, native RGBA GPU resources/layout/fences and a separate
standard-OpenGL movie device with no legacy GS scene/readback initialization.
It does not connect movie playback to the live retail platform boundary.

Two actual failures were corrected without weakening tests. Extended Windows
paths require native separators across the complete joined relative path, and
whole-movie AVIO must rewind the same already-open asset, including prior EOF.
The original cursor reproducer fails on TITLEN; corrected catalog decode
matches every original full movie byte/hash/count and timestamp golden for
LOGON, TITLEN and OPENINGN after inspected/nonzero/past-EOF handoff.

The isolated GL fixture checks real decoded pixels, dynamic 2053x769 resources,
filter/alpha/SAR, ownership and fences using standard APIs. Source and executable
symbol review show no guest execution/device dependencies in that fixture.
This does not establish other-vendor hardware testing or complete gameplay.

Symlink creation is NOT_EXERCISED (missing Windows privilege). Case-sensitive
directory containment is an identified open boundary. No game, card migration,
physical input, audible output, driver installation/configuration or local
model-setting change occurred. Movie control/clock/drain, exact caller policy,
retail binding and the whole-game acceptance contract remain OPEN.
