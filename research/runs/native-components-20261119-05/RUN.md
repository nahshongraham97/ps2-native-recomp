# Native media/audio service checkpoint — 2026-11-19

All45 consolidated standalone cases pass. Exact binary/asset hashes, arguments,
outputs and exits are in verification.json. BUILD.json pins authored source
and separately hashes preserved dirty IOP files, excluded from this commit.

Native file/codec/color/GPU services now form mka_native_media; Windows audio
forms mka_native_audio_device. A fresh configuration with empty PS2_GENERATED
built and ran the movie pipeline and audio validation without PS2runtime or
generated-game linkage. The two isolated executable symbol checks found zero
defined ps2::/gen::/spu2::/iop:: symbols. Configuration/build/run receipts are
media-isolated-*.json. This is a native service boundary, not a completed game.

Actual movie pipeline: all three full MPEG2/ADX decode SHA256/count goldens
match, then first/last frames pass six full RGBA GPU comparisons after decoder
and file teardown. Raw/best timestamps, SAR and all source color metadata
survive native handoff. Role/filter/aspect are explicit fixture policies;
they are not claimed selected by the actual retail caller. Readback exists
only in the fixture. No guessed playback clock, end callback or movie skip.

Actual WASAPI: read-only endpoint discovery reports native mix layout/rate;
explicit exact-format stream queues only zero-valued float samples. Full queue
rejection, owner moves/foreign-call rejection, start/stop/reset/close and raw
clock advancement pass. Raw position advanced0to4432 at frequency384000; these
are not presumed48000PCMframes/s. No nonzero sound, audible comparison, retail
DSP/gain identity, device-loss recovery or true movie drain is proven. No
endpoint default, volume, driver, service or update setting was changed.

Native file containment now compares exact normalized actual directory/file
handle paths, preserving legitimate host case aliases. Distinct case-sensitive
roots and original UTF8/longpath/>4GiB/lifetime fixtures pass. Symlink-negative
branches remain NOT_EXERCISED because Windows denied creation privilege.

No game executable launched, backup modified, card migrated or local model
setting changed. Full retail acceptance remains OPEN.
