# Native movie stream composition — component validation, 2026-11-19

The concrete stream owner joins an owned source, required native business
policies, color/GPU resources and an explicit audio output behind the existing
retail control sequence. The source and WASAPI adapters consume the existing
native owners; they do not reopen paths or select an endpoint. Actual retail
timing/audio/metadata policies remain unbound and have no default implementation.

`validate.py` builds the deterministic fake-endpoint fixture, real adapter/media
archive, pure audio fixture and existing controller fixture. The composition
fixture takes the pinned original overlay as read-only color-table data, never
executable code. It verifies paused preparation, actual-format agreement, full
and racing-full retries, chunk/source/client receipt accounting, malformed
receipt termination, frame selection before upload, GPU fence ownership, EOF
versus explicit drain, missing required audio, both cleanup failures, prestart
cleanup and wrong-thread rejection. Native adapter compilation is distinct from
native device execution. No game, window, GPU context, sound or driver/settings
operation is performed by this runner.

`validation-01/verification.json` reports
`PASS_ISOLATED_NATIVE_MOVIE_STREAM_COMPOSITION`. Executed fixture SHA256:
`3e503154491dd10e41eb032874e43fb182cec310f879e5b413013484f7bb7dd1`.
Compiled native media archive SHA256:
`e3b9c4167e68954d3ddb764c97af8df3c3e19290ed790d984973417f2c110198`.
All deterministic assertions, the existing controller fixture and pure audio
validation pass. Source before/after and preserved IOP bytes match. Both scoped
symbol scans are empty and required component/adapter symbols are present.
The original archive symbol receipt misnames its expected target digest
`executed_target_sha256`; the archive was compiled and inspected, never executed.
`validation-01/symbol-target-interpretation.json` disambiguates that field without
rewriting raw receipts. The exact original runner remains in the parent dated
`playback-composition-before-results/validate.py` snapshot. Its current revision
corrects the field for future runs; it is not substituted into the prior run.
Actual native adapter/device execution and live-owner format getter remain open.

A successful component fixture must not be described as complete intro playback
or a reason to ask the owner to launch. Parent source,
design and test records are under
`research/runs/native-movie-ingress-20261119/MOVIE_PLAYBACK_COMPOSITION_*` and
`MOVIE_PLAYBACK_TEST_PLAN.*`. Owner action: none. Coding agent: bind the remaining
source contracts and actual native endpoints before claiming playback.
