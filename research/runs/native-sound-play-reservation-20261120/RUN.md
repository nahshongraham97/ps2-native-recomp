# Native sound context and play reservations — 2026-11-20

Status: PASS_NATIVE_SOUND_PLAY_CONTEXT_RESERVATION_AND_SERVICE_GAPS.
Source build base a562eca57577faa2479d1ff7213310c47dae1dad plus exact350-file
source/tool/asset/evidence map in validation-01/verification.json. Release native
component build with PS2_GENERATED empty. Not an accepted retail executable/run.

Actual NativePlaybackOwner now requires an explicit NativeSoundContext for play;
there is no default serialized context0 or gain-to-owner conversion. Contexts
belong to the actual private Service resource domain. Host-owned pending Entry
identity is admitted before program construction and survives vector growth.
Owner Activity spans service pre-entry, real work, return and final commit.
Resource construction and publication no longer hold the registry mutex.
Executor/voice mutation and all-voice transactional mix remain serialized.

Unload closes exact bank admission before Service mutation. Close rejects all
outstanding owner activity and closes admission before Service close. Ordinary
false results restore their own gate; exceptional results quarantine exact bank
or owner admission because native mutation completion is unknown. No implicit
retry, changed sound, fallback or guessed successful recovery is supplied.
Failed unpublished play rolls back its exact slot with no permanent tombstone;
published handle tombstones preserve stale-owner rejection. Explicit context
release is idle-only native teardown, NOT source command12/forced cleanup1.

Three affected executables rebuilt/run PASS: existing playback behavior,
actual ANNOUNCR publication/context/concurrent mix, and fixture-only Service
pre/post gaps with64 simultaneous reservations and exceptional mutation gates.
350 stable pins,340 untouched prior pins. Real PCM remains nonzero and an
existing voice source position advances during another program's publication
wait. This does not prove waveform/heard identity or full-game audio parity.
The five wraps are on the gap test only; actual library/other targets are
unwrapped. No physical endpoint or game ran; no local model/server/driver action.
Decoder and unchanged numerical/original sound corpus records remain frozen.

Full native current-context bootstrap, source315680/command12 forced cleanup1,
source-distinct queued/running deferred-program cancellation, callback ordering,
global public handle revocation, alternate paths, physical output and whole
retail acceptance remain OPEN. Fresh program candidate/commit must replace the
current synchronous-helper shortcut before cancellation is claimed.

Owner: no action. Coding agent: implement fresh deferred-program candidate and
atomic identity/cancellation commit, then actual owned worker retirement.
