# Native eligibility business calculation — component verification, 2026-11-19

The pure native Release fixture passes all57 declared inputs against the
independent integer model:32 source-normal cases,18 native64 extensions and
seven validation cases. A separately compiled offline reference using the
unchanged backup helper also matches all32 source-normal cases exactly.
Every adjusted numerator, four materialized metric bit patterns and three
comparison booleans are compared. No reference code is linked into the native
target; its scoped forbidden-symbol audit is empty and pins target/tool hashes.

Native executable SHA256:
`0b52cabea26a7bcfd4c66ff30041203b29af4f94ab59ccde349488732541fe64`.
Separate offline-reference executable SHA256:
`48d80353e1596bab95b7d7608dabc503d92ef65d7fb77ef02d7edcec8bd25cc0`.
The checker runs under Python `-O` and rejects13 altered traces,11 invalid
metadata records and two malformed JSON inputs. Reference header/source,
compiler, standalone input plan, model/checker, executions and native inspected
target identities are retained in validation-01. The runner path and prebuild
hash are explicit. Backup header bytes and dirty IOP source hashes are unchanged.

This is a statically compiled game calculation with function-specific integer
quantization and exact native metrics. It retains neither generic FPU/device
state nor instruction interpretation, flags, source exponent saturation,
guest memory or host rounding-mode changes. Native64 input/cancellation is an
explicit extension; source-helper parity is claimed only for the32 safe normal
inputs. Neither hardware nor historical cached-binary fidelity is established.

Reachable near-half cases exercise both sides of the quotient rounding boundary.
Exact-half division ties are algebraically unreachable for the quantized
operands, so no fabricated tie case is counted. Signed negative tolerance can
make early/late both true; the outer adaptive decision must preserve early-first
ordering. This stateless calculation does not implement that owner or select
clock availability, target origins, cadence, playback or presentation.

Actual queue/SDK-output association, adaptive state, audio routing/gain/drain,
host-clock binding, movie player and game acceptance remain OPEN. No game,
audio/graphics device, driver, local model or saved setting was operated on.
