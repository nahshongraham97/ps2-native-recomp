# Native movie position timecode checkpoint — 2026-11-19

The source80A608 position conversion is implemented using explicit owned PES
provenance, rate, drop and temporal-reference inputs. It preserves separately
rounded binary64 operations, exact original inverse tables and the unusual
source drop branches. Missing timestamps and unsupported inputs fail by name.
No launch argument or enabling variable selects this behavior.

Independent root review checked original MOVIER table bytes and arithmetic
branches plus the existing R5900 Ghidra session identity and software-double
helpers. `source-review.json` retains that source evidence; original code was
not executed. `build.json` and `execution.json` pin the current source and
native-only arithmetic fixture. The scoped target passes, including 20,000
independent wide/cancellation oracle cases and literal source boundaries.

This is a component checkpoint. No movie master clock, source admission,
player/lifecycle, audio drain or retail host binding is claimed. No game,
viewer, device output, driver, backup, card, save or model setting was touched.
The old ps2run binary was not linked or launched. Whole-game acceptance is OPEN.
