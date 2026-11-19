# Native movie control and audio-layout checkpoint — 2026-11-19

NativeMovieController lifts the proven retail blocking start/poll/skip/fade/
stop sequence to typed native objects. It requires an actual prepared native
playback port and explicit current volume/fade length; none is synthesized.
It does not call PS2 providers, vtables, devices or guest memory, or impose
source pool/word limits. Native service failures remain distinct from normal
completion. NativeMoviePlayback is a contract, not a working default backend.

Owned MovieAudioFrame now retains actual FFmpeg channel-layout order, count,
optional mask and dynamic plane-order IDs/names. Actual three-movie ADX
output is UNSPEC count2, with no invented speaker assignment. Full YUV/PCM/
count/timestamp goldens through both entry paths remain unchanged.

`validation-01/verification.json` records20 component cases,17 symbol checks,
38 actual source mappings and3 media goldens on its exact pre-correction
source hashes. Root then found a32-bit callback narrowing seam. The exact
old header reconstructed by its prior hash FAILS the added0x100000001 case;
the corrected int64 callback PASSes. `callback-correction/verification.json`
checks that only the header/test changed since consolidation, pins all current
source hashes and records the rebuilt control binary and symbol audit. It
does not claim all20 cases ran again after this isolated correction.

Independent source review preserves its initial miss and corrected recheck in
parent MOVIE_CONTROL_INDEPENDENT_SOURCE_REVIEW. OUTER_LIFECYCLE_SOURCE pins
original control/skip/error/fade behavior. ROOT_AUDIO_LAYOUT_SOURCE_REVIEW pins
the separate audio source review. No game was linked/launched or hardware
output played. Hidden-context GL tests use standard APIs; no driver changes.

Actual native movie factory/initial gain, source eligibility and played-audio
clock, speaker routing, native rendering/overlay/fence/swap binding, supplied
game callback ABI/side effects, poll cadence and nonblocking movie route remain
OPEN. This is not a native game, movie playback, audio or full acceptance pass.
