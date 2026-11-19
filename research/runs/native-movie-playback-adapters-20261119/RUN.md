# Actual native movie adapters — scoped results, 2026-11-19

`validation-01/verification.json` reports
`PASS_SCOPED_ACTUAL_NATIVE_MOVIE_ADAPTERS_ZERO_ONLY`. Fixture SHA256
`d4235aea23c3d95f7d3e5ebecc838b17b5b005e333448a02464380e23cf3a3f5`.
The wrapped/direct LOGON source first video/audio retain exact owned bytes,
request and timing/color/PES metadata after both source owners are destroyed.
Only first frames are checked; two instances of the same decoder are a forwarding
and ownership check, not an independent decoder oracle.

Actual WASAPI operations queue only separately allocated, checked positive-zero
floats. The observed console-default endpoint client format is48000Hz,2channels,
mask3; this is not evidence of a retail source-plane speaker mapping. Capacity
4800: first receipt epoch0 [0,4800), Reset epoch1 [0,1). Pause preserves epoch/
submitted frames; deferred empty Start, typed buffer-full, raw device clock,
foreign-thread rejection and actual reset pass. The original native-audio
fixture additionally passes same-open format, moved/closed-owner and foreign
format-read checks. No decoded movie PCM reaches the endpoint.

Source/catalog/LOGON/codec-DLL/compiler/cache and executable hashes are retained
before/after. The scoped forbidden-symbol scan is empty; the inspected target
is the exact executed fixture and the symbol tool has a separate identity.
Dirty IOP sources are preserved. No game/window/GPU context, driver/settings
change, audible movie, source timeline, synchronization or playback claim.
Owner action: none. Coding agent: bind/verify remaining source policies.
