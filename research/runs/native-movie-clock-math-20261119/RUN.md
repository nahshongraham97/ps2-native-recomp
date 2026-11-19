# Native integer movie-clock helpers — 2026-11-19

The isolated Release build and complete trace comparison pass: 422 planned
operations, 413 successful results, four overflow errors, two zero divisors
and three invalid denominators. Twelve malformed trace/binary-identity checks
are rejected. [verification.json](validation-01/verification.json) joins the
source/CMake pins, execution and independent exact Python-integer oracle.
Executable SHA256:
`46aaba549ca80c31ae636f32a9829142f9a949d942952696fd4e37decb6e4bab`.

The symbol receipt pins both the inspected target and llvm-nm before/after;
the target equals the executed executable. Its scoped forbidden-symbol scan
is empty. This does not establish whole-game source reachability.
`validate.py` records the exact build command, compiler, native-only cache,
source hashes and preserved dirty IOP hashes. The fixture runs with no arguments.

The helpers use ordinary native signed128 intermediates and signed64 results.
They preserve normal integer operation order while rejecting invalid division
and final overflow. Intermediate values outside signed64 may cancel to valid
results. No PS2 wrapping, fixed memory, device model or clock choice is retained.
The separate software-double position/timecode converter is unchanged.

Review found that the initial Python checker allowed equal null/empty binary
identities. It was corrected before this execution; current checks require
matching 64-digit lowercase SHA256 strings and exercise that rejection.
Parent source/result reviews are under
`research/runs/native-movie-ingress-20261119/CLOCK_RATIONAL_*`.

Full filter, real clock providers, movie/output scheduling and gameplay remain
OPEN. No game, graphics/audio device, model, driver or settings operation was
performed by this run. No owner launch is requested.
