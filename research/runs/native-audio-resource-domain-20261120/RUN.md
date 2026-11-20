# Native sound resource owner checkpoint — 2026-11-20

Status: PASS_NATIVE_AUDIO_RESOURCE_DOMAIN_INTEGRATION (component scope only).

The native sound resource library now builds without PS2_GENERATED or
ps2runtime. One owned context joins program/sample preparation, associated
MS2/MS4 backing operations and actual immutable PCM publication. Movie entry
completes three separate barriers then retains only publication exclusion;
voices, playback acknowledgment and future queued jobs are not paused.

Private sample construction releases local/request-table locks before waiting.
Actual unload revokes a blocked candidate; failed preparation retires the whole
candidate and its admission. Final opened files are retained in preallocated
native retirement nodes and closed under the real backing owner's release.
Finalized token reservations cannot erase a newer same-token retry.

Validation-04 rebuilt and passed all12 affected native executables, with328
stable source/tool/asset/evidence pins and287 unchanged previous pins. The
preparation regression covers344 original banks,5500 eager programs,4156
source-deferred programs and179 skipped precreation cases. This is native
preparation evidence, not playback completion or audible fidelity.

New concurrency scenarios cover actual Service/Protocol unload while publication
is excluded, active-close refusal, private/cached data, recursive cycle rejection,
shared associated backing identity, deferred final close and32 concurrent token
retry trials. A separate executable injects3 unexpected preparation exceptions
through real Session/sync/async paths and then performs3 real native retries.
The linker wrapper is confined to that test; production library and other
fixtures remain unwrapped. Test scope includes sample identity/extent, not an
exhaustive PCM-word or scheduling proof.

`validation-04/verification.json` owns exact source and executable identities;
its SHA-256 is C81C6852340FA509A538F534CFF66379873434EFA5B21BA24CFC4DE6ACA23A5E.
Every executed fixture is joined to its own saved symbol audit; the native
library's17 objects have no forbidden device/runtime symbols. Independent saved
execution review checked these joins without rerunning tests.

Validation-01 stopped at target lookup before compilation. Validation-02/03
passed their earlier scopes; independent review then found service-lock,
final-close, reservation and exception-ownership defects. Exact before bytes,
original findings and separate correction closures remain preserved in the
parent ingress run. No earlier record was silently repinned or overwritten.

All game/physical endpoints remain unopened. No decoder or old numeric corpus
was rerun; no local model/server, drivers, backup, card/save/state or key files
were changed. Preserved dirty IOP edits are excluded from this checkpoint.

Remaining: actual LOGON audio/graphics surround and315680 resource effect,
whole native renderer drain/resume, full deferred/alternate callback/upload
binding, checked close errors, host/bootstrap/physical output and all-game
comparisons. Graphics finish is unconditional in source even if entry reports
already-quiesced; never substitute changed-only restoration. This is not an
accepted game release or a request for the owner to launch.

Parent reports: NATIVE_AUDIO_RESOURCE_BINDING_REVIEW_20261120,
NATIVE_AUDIO_RESOURCE_IMPLEMENTATION_REVIEW_20261120,
NATIVE_AUDIO_RESOURCE_FINAL_PEER_20261120,
NATIVE_AUDIO_RESOURCE_PEER_CLOSURE_20261120,
NATIVE_AUDIO_RESOURCE_OWNER_CLOSURE_20261120,
NATIVE_AUDIO_RESOURCE_EXCEPTION_CLOSURE_20261120 and
NATIVE_AUDIO_RESOURCE_EXECUTION_REVIEW_20261120. Prior reports remain scoped;
final exception and execution closures bind the current validated source.

Next coding action: bind actual native graphics drain/resume and the same
sound-resource owner into the source-compiled LOGON surround, preserving
source ordering and real failure cleanup. Owner: no action.
