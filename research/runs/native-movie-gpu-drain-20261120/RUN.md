# Native movie GPU completion checkpoint — 2026-11-20

Status: PASS_NATIVE_MOVIE_GPU_COMPLETION_CPU_AND_WGL_BUILD (component scope).

The actual standalone NativeMovieGlDevice now uses a native work owner:
pre-admitted source lifetime, real active/retired completion, monotonic device
admission seal, exact context-tail receipt, cached terminal consumer result,
explicit retry and completion before native IDs/textures are released.
A failed/no-sync presentation may become safe to retire after a later real tail,
but never becomes a successful consumer submission. No glFlush, destructor,
context-current check or retirement marker is accepted as GPU completion.

An explicit host retirement owner is mandatory at attachment and registered
before GPU work. Backend destruction uses the shared production registration
marker; the host retains/polls the actual session, including failed-tail retry.
MovieLease's weak session identity removes the Session/work/frame back-edge.
The host retirement owner must outlive devices and be drained before context
teardown. Boolean ownership observations are confined to their owner thread;
only retirement marks/registration destruction are safe cross-thread operations.

Validation-01:339 current stable pins,324 untouched prior pins. The ONLY launched
fixture is test_native_movie_gpu_completion.exe using deterministic native fence
ports. It checks retained source lifetime, active/retired receipts, failed/null
sync submission, tail sealing, exact/foreign receipt identity, allocation failure
without reopening, partial native sync deletion, sticky consumer failure,
273 dynamic native jobs, shared production marker destruction and host-owned
retry/cleanup. This is native ownership software evidence, not GPU emulation,
physical GL completion or rendered-pixel verification.

The real WGL backend and test_native_movie_gl_device.exe plus
test_native_movie_pipeline.exe compiled/linked and passed symbol/linkage audits.
Both physical fixtures were NEVER executed. Native media/completion archives
have no forbidden runtime/device symbols. The driver has one explicit CPU
execution target; independent saved execution review joined source, compiler,
cache, Ninja rules, artifacts and tools without rerunning anything.

Verification SHA-256: 733df6a563f170c7ddbebbe6ed67dc81c1875082b705f8e7ffcc9ae8f1577051.
Saved evidence: validation-01/verification.json, completion.json, build.json,
linkage-audit.json, exact target-specific symbol receipts. Source review records
four observed/corrected draft defects. Exact before bytes, old fixture review,
correction closure and final execution review remain intact.

No game, physical endpoint, decoder/old numerical corpus, local model/server,
driver, original Ghidra project, backup, card/save/state or key-file action.
The preserved dirty IOP edits and inventory are excluded.

Remaining source/game integration: whole-renderer admission and owned destination;
actual begin/end boundary draw and inherited FBA/alpha/edge state; current sound
context315680/command12 cleanup/cancellation and forced cleanup mode1; explicit
pending/finish LOGON phases; ordinary game/render/host bootstrap and all-mode
physical output/non-regression acceptance. This is not an accepted game release.
The native clear/boundary draw was NOT guessed or implemented from vertex alpha
alone. NATIVE_MOVIE_BOUNDARY_ALPHA_SCOPE_20261120 supersedes that stronger wording
in the frozen packet report.

Sound source supplements now prove queued cancellation policies differ from
running callback suppression, and public handle revocation/cancel request do NOT
prove worker retirement.37FD4 is cancellation state, correcting the old generic
immediate/deferred-query phrase. NativePlaybackOwner.stop's flag-derived mode
is not a source-exact command12 forced-mode1 replacement. These source findings
are not a claim that cancellation/context cleanup is implemented.

Next coding action: implement the owned sound-context operation and phased
LOGON surround with actual renderer target/admission and genuine pending waits.
The decoder stays frozen. Owner: no action.
