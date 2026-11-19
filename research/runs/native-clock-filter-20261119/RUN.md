# Native movie clock filter — component verification, 2026-11-19

The pure Release fixture passes all 440 planned rows across 25 cases against
the pinned independent Python-integer model. Every config/input, output,
physical history entry/cursor, cached mean, anchor, prior rational pair and
counter/extremum is compared with strict types and input order.
[complete-trace-oracle.json](validation-01/complete-trace-oracle.json) records
427 successful rows, four invalid-rational errors, two unsupported modes,
four invalid configurations and three overflow rejections.

The exercised observations include first/restart offsets, pause, inactive
reanchor, mode0 activation without an anchor, positive/negative correction,
large reset, exact threshold equality, bypass and previous-pair clamp. Windows
1/10/61/256 use native owned storage. Timer numerator−1 is ordinary data, not
the old absent-anchor sentinel. Wide intermediate cancellation and a wide
preliminary time followed by valid reset also pass.

The independent checker runs under Python `-O`; it rejects13 altered traces,
11 invalid execution identities/metadata and two malformed JSON inputs.
Separate C++ assertions verify move construction/assignment, self-move,
moved-owner mutations and retained result lifetime. Allocation/counter-overflow
fault injection and arbitrary configured diagnostic consumers are not tested.

Executable SHA256:
`8a653593f6bc4b7b48e65e38d00c989b8e2ce55a6afb5895df8478686180de2c`.
The scoped symbol scan pins the actual inspected target before/after and
matches the executed binary; it reports no forbidden symbols. Full-game
reachability is outside that scan. Sources, generated INPUT-only plan,
compiler/cache, checker/model and receipts join in validation-01.

This is an owned software arithmetic/state algorithm, not a clock/device
implementation. It takes explicit rational values, queries no clock and
introduces no startup arguments or backend switches. Full movie clock choice,
native scheduler/output binding, diagnostics ownership, player and game
acceptance remain OPEN. No game, audio/graphics device, driver, model or saved
setting was operated on; the two dirty IOP sources remain untouched.
