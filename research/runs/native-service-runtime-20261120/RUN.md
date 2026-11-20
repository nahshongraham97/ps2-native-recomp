# Actual Service-owned runtime references — results, 2026-11-20

PASS_NATIVE_SERVICE_RUNTIME_REFERENCE_BINDING. Native component verification only;
actual source output admission/clock/activation/callback and whole-game acceptance
remain OPEN. Build baseddce5516672a422d14bcdb329330172918677a55 plus exact pinned
sources, existing native-only Release configuration with PS2_GENERATED empty.
No accepted retail executable/game run.

Actual Service -> Protocol -> Session -> PreparedBank runtime acquisition is now
implemented. Exact live token admission reserves Service operations and global/
per-bank runtime ownership before calling Protocol. Protocol and Session snapshot
the actual handle/PreparedBank under short registry locks, then invoke native
runtime acquisition outside those locks. No registry spans resource waiting.

The returned actual RuntimeProgramReference owns a private Service admission
keeper, preserving the full token and actual Service State through transfer into
NativeProgramCopy. Semantic reference/data retirement finishes before keeper
destruction and count release, outside all bank/reference locks. No strong
backlink from Service State to individual references forms a cycle. Failed
lookup/acquisition/exception rolls reservations back exactly once. Named runtime
ownership prevents otherwise-admissible same-bank unload/close; unrelated banks
remain usable. Existing busy-load/work errors keep their previous precedence.
These are native safety counters, NOT source bank+2C callback outstanding counts.

All18 affected executables rebuilt/executed PASS,478 stable current pins,
436 unchanged prior. New actual Service test5 groups verifies exact constructor/
bank identities, two references, full token/invalid lookup rollback, real async
load completion publication, reference->copy->executor->real native voice/nonzero
PCM, mode0 stop retaining admission, exported immutable clone PCM surviving unload,
and moved/dropped public Service lifetime. New gated test5 boundaries covers
before/after real Protocol acquisition, known refusal and exceptions on either
side, with actual retry and exact-bank/unrelated-bank guards. Existing native
copy/runtime/Service/worker/owner/cache/program clients pass. Compiler/linker/NM/
execution identities are joined; only designated fault executables use wrappers.
New Protocol wrapper is test-local; production archive remains unwrapped.
Certificate SHA256: 3206a4d989b4509d812d5fb57b61d4cd9c13c3cdf38aa2296292a968bf0a08e5.

Core donor/sample/codec behavior did not change; no344-bank, decoder or unrelated
numeric corpus rerun occurred. Source proof20 exports/1306 original word rows
was independently mapped to original ELF bytes. Prior native inputs referenced by
source-only reports are dated snapshots preserved before this implementation.

New exact source facts constrain the remaining binding: incumbent replacement
calls30C54, not30854. Mode0 stop removes live-list membership/active flag but can
retain selected association and DOES retain clone/shared runtime reference.
Nonzero cleanup has separate release behavior. Slot release compares exact
occupant before clearing, so stale cleanup must not erase a new owner. Selection
-2 uses a real halfword load with unresolved caller meaning, not a proved packed
range. Activation and update share the same system-time family; underlying kernel
epoch/Windows equivalence is not proved. Source effects are not guessed.

OPEN: real output admission/config/priority/replacement, owner-list/public identity
commit and common clock producer, forced context cleanup1/hardware tail behavior,
source job cached/fresh callbacks/bank-count ordering, and retail/game integration.
Existing default NativePlaybackOwner still calls its earlier native prepare_program
data helper; this batch does not claim it was rerouted to the new runtime/copy
path or that source30000 activation occurs. Native Service API ownership is real;
complete game wiring, movies/rendering/input/saves/all-mode acceptance remain OPEN.

No game, physical output/input endpoint, local model/server, driver/settings or
card/save/key operation occurred. Backup and preexisting IOP edits are preserved.

Owner: no action. Coding agent: bind real output admission/identity/clock activation
and forced lifecycle, then route the actual playback producer and movie callbacks.
Offline decoder remains DONE and frozen.
