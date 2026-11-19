# Native finite movie gain component — 2026-11-19

Current component result: PASS_EXHAUSTIVE_NATIVE_FINITE_GAIN in verification.json.
One native-only process checked4,278,190,080 finite signed binary32 inputs and
21,390,950,400 public API results in 219.5470546 seconds, exit0,
digest F04C33D8F3F068E7. domain-01/execution.json pins source, executable, raw
reference and output; stdout.jsonl retains progress and final counts. Its
transitive-header-supplement.json is explicitly post-run supplementary identity
proof, not a prelaunch dependency pin.

The raw reference stages 0..3 are checked for every input/sign. Native final 4
always returns-960 under the reviewed finite-wide cancellation extension;
the reference's extreme-value saturation artifact is deliberately removed.
All102 source UI/default rows/510 values remain unchanged. Runtime contains
only mandatory compiled integer lookup, with no reference arithmetic helper,
guest/device state, startup flags or runtime data-file selection. Inf/NaN and
unsupported requests fail named.

build.json, gain.json and control.json retain scoped native archive/gain/control
build and focused-test passes. verification.json records three new strict
symbol audits with zero defined forbidden namespace symbols. emitter-rejections.json
rejects 8 malformed interval cases; compile-rejections.json records 5 rejected
isolated invalid/missing datasets. These checks do not imply a new consolidated
21-case run: that earlier suite belongs to published checkpoint e3fbad8 and
precedes this full-interval delta.

Parent offline-gain/NATIVE_DOMAIN_API_SOURCE_REVIEW.md/json records independent
source/receipt review; raw full-reference, source/sign/native-capacity policies
and exhaustive offline evidence are retained separately. Numeric lookup and
explicit native capacity behavior are verified components, not historical
binary/hardware/FCR, profile storage/import, coefficient/mixer, player or full
game/audio fidelity acceptance. Those integration boundaries remain OPEN.
No game, audio/video device, driver, settings or backup write occurred.
