# Actual native program copies and ordinary schedule primitives — results, 2026-11-20

PASS_NATIVE_PROGRAM_COPY_AND_ORDINARY_SCHEDULE_PRIMITIVES. Component verification
only; source output admission, actual activation, callback and full-game acceptance
remain OPEN. Build base2df46c0f7179199a6c4e4eab721575bbfe44fe15 plus pinned final
sources; native Release with PS2_GENERATED empty. No game executable/run.

Normal donor state now records the verified prepared flags/repeat/counter/control/
start values. NativeProgramCopy copies that donor value and private command bytes,
sharing immutable PCM while retaining the actual RuntimeProgramReference. Copied
normal occurrences have EMPTY cache_reference: no second source sample acquisition
is fabricated. Each copy owns independent mutable normal controls and optional real
HostVoice output. Destruction clears outputs/instances/data before releasing its
actual runtime-template semantic reference. Exported copied data keeps PCM safely
but does not keep the original sample-cache association alive.

A shared source operand decoder now serves ProgramExecutor and the copy. Actual
kind1 operands update per-instance factors and sticky flags after copying, then an
explicit native voice factory may create output. All context/program factors,
envelope/output mode/rate remain supplied, with no guessed profile. Failed voice
construction retains applied operands but creates no output; full original failure
side-effect equivalence is not claimed. Active/repeated output replacement stays
named unsupported. Forced stop1 is unbound. Frozen actual play flags determine stop
mode; caller-modified metadata rejects before output changes. Successful stop closes
the copy to new starts. Owner serializes mutable copy operations and borrowed views.

Ordinary helpers preflight a first exact kind7 terminator, clear only kind6 command
word+10 before it, and compute low32(explicit sampled clock + raw first delay).
Negative first delays do not use the later-command FFFFFFFF rule. Flag8 scheduling
is named unbound. A published ordinary plan freezes the data, uses actual supplied
per-play flags, and cannot be prepared twice. This does not sample a clock, admit an
output owner, install identity/list membership or set an activation success flag.

All17 affected executables rebuilt/executed PASS:445 stable inputs,410 untouched
prior. Actual ANNOUNCR copies exercise independent raw command/data/state owners,
shared PCM, exact16384 donor->16383 applied gain, actual executor->native voice->
nonzero PCM, independent voice positions, foreign/tampered/duplicate event guards,
frozen stop-mode spoof rejection, native cleanup/cache identity and PCM survival.
An actual BG_ARMORY eager clone checks inherited preserve40/all typed donor values
and zero-reference cache retention. Pure fixtures verify reset/terminator atomicity,
signed/zero/wrap first-delay arithmetic and distinction from later-delay behavior.
Test-only allocator injection rejects each allocating prefix until success, preserving
caller ownership and bank usability, then actual voice failure/retry is exercised.
Existing runtime/Service/admission/domain/exception/owner/reservation/cache/Work/program
clients pass. The donor metadata change justified one344-bank preparation pass:
5500eager/4156deferred/179skipprecreation. Decoder and unrelated numeric corpora did
not rerun. Native archive is unwrapped; only designated fault targets use1 allocator,
1 unload,1 preparation and5 owner-call wraps. Exact executable/tool/receipt/build-rule
joins are saved and independently reviewed. Certificate SHA256: 7516bf0ce11c3657843d601f8e1b9ad87ec615560eb92c6ef4786ce3958d42dd.

Source-only normal instance and ordinary activation records plus root original-word
check cover13 exports/1149 rows; preimplementation code hashes have preserved source
snapshots. Old clone/control/clock guesses were not implemented. Draft stop metadata
and fixture coverage corrections are preserved; final source review binds all9 inputs.
Initial edit orchestration syntax was rejected before shell execution, with no source
mutation; it is not a build/test failure. No failed execution was relabeled.

OPEN: actual native Service/PlaybackOwner producer integration, real output selection/
priority/replacement and owner-list/public identity commit, clock sampler and flag8,
source fresh/cached worker callbacks/count order, forced command12 and renderer-wide
movie surround. Unmapped alternate/history/loop/start cases stay named failures.
No source activation/callback/heard fidelity or accepted native retail build follows
from the component test. No hardware endpoint/game/local model/server/driver/settings/
card/save/key operation occurred. Backup and preexisting dirty IOP edits preserved.

Owner: no action. Coding agent: bind actual native output admission/identity and
clock-backed activation to these copies, then source callbacks and movie entry/exit.
Offline movie decoding remains DONE and frozen.
