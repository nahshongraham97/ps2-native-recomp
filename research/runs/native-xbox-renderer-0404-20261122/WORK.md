# Native0404 resource — Xbox-first recovery

Entry:parent29ecd7d24dd38a01bf51b18fcf463378a5cecd7f,
forke81087e937b159a6c325c93a5f10fe78aa16150b, main/origin-main,
tracked source clean. Preserve all unrelated parent historical untracked
evidence, originals/saves, backup checkouts, drivers and reserved model/settings.

Owner requests0404's real native resource. Reuse the already joined Xbox
constructor2B8DE0..2B8E28 and destructor2B8E30..2B8E52 from
`../native-xbox-scene-frame-20261122/source-module-callbacks-09/`.
They create/release an actual resource and publish/increment only on success;
the concrete element and consumer contracts still need source proof. No
counter-only factory, copied36-byte object/pool or device model is authorized.

Reuse this agent's completed read-only private Xbox session and the bounded
exact-function/reference exporter from the0403 record. Do not open/change a
shared master, reinstall tools/plugins, query the local model or rerun
unchanged0403 work. Follow actual resource-offset readers/writers and native
consumer/error/lifetime effects before selecting runtime interfaces.

Stage:source recovery only; no production edit/build/client/game run yet.
Existing Core/Session/heaps/content/native files/audio/GPU services carry
forward. Full renderer/Open/Start/GPU/frames/input,0403 plugin/clone/sync,
native save/reload,numerical/HD/oracle and game acceptance remain OPEN.

Owner:no action. Agent next:original allocation configuration and0404 resource
consumers, then typed native implementation through existing owners.

source-0404-01 PASS4.60s:only actual resource consumers are2B95A0
(two reads) and2B94D0. Initial configuration is16 elements/one preallocation,
not a native capacity; resource offset/lifetime count are section-proven
loader-zero storage. Do not infer the concrete type from36-byte size.
Inspect both real allocation/cleanup callers and then their I/O/other
consumers before choosing native behavior.

source-0404-02 PASS4.62s:2B95A0 allocates then calls2B8FB0, freeing
the record on initialization failure.2B94D0 dispatches stream-kind close:
file-close result becomes a boolean, writable memory can return backing/
length, custom close is optional and its return is ignored, then owned
record storage is freed even on file-close failure. This establishes a
stream owner, NOT the separately registered raster family. Actual switch
table bytes, initialization/types and I/O contracts remain to be joined;
do not adopt decompiler case labels or invent close/ownership policies.

source-0404-03 PASS4.71s pins initialization and the close jump table:
types1/2/3/4 map to borrowed-file/filename-file/memory/custom-shaped paths.
Type1 checks the existing handle's position and does not close it; type2
delegates actual open; type3 delegates memory setup; type4 copies five
callbacks/data words. The original ABI uses register-carried arguments in
some helpers; verify actual rewrites rather than copying decompiled signatures.
Next inspect open/memory helpers and real callers to identify required I/O
entry points and native read/close destinations.

source-0404-04 PASS4.74s confirms access1/2/3 choose read/write/append
for owned filenames and distinct memory births. Actual caller241140 opens
type2/access1;241170 consumes2B90B0 then closes the stream. Memory access1
starts position0 with caller buffer/length; access2 starts empty; access3
starts at caller length. Source size/position words are game/API data, not
native storage capacities. Source file modes/dispatch tables and actual
read/write/skip helpers must be decoded before adopting those paths.

source-0404-05 PASS4.68s proves raw read2B90B0:memory returns bounded
partial bytes and advances position; file paths return actual bytes even
after short-read diagnostics; custom delegates its real callback. Modes are
original "rb"/"wb"/"ab".241140's actual production use is filename READ,
so it must reach existing native content files, not a memory-only substitute.
Need establish the file status slot and forward-skip arithmetic. The checked
close result is a direct native-file dependency:existing void RAII close
cannot by itself prove a source close-success result.

source-0404-06 PASS4.66s identifies the actual file table installation:
status slot835420 ->39B16E,seek ->39B125,close ->39B574. Forward skip0
returns the same stream without dispatch. Memory skip past end clamps the
cursor to end then returns failure/code5; its ADD is32-bit wrapping, so the
wraparound/invalid-input case cannot be silently reclassified as ordinary
overflow-safe source equivalence. Native host-wide cursors must not inherit
a4GiB capacity; any unsupported source-wrap mapping remains named/open.
Next pin status semantics/table targets before selecting valid read-only
memory/native-file paths and actual checked close/error propagation.

source-0404-07 PASS4.60s pins all four dispatch tables and status39B16E:
the file table's status reads CRT EOF bit10, distinct from adjacent error
bit20. Native FileRead already reports feof/ferror separately. Preserve actual
partial bytes and code5 at EOF versus8000001A for non-EOF short/error reads.
No CRT FILE layout is copied. Source read/skip/close/write/custom paths remain
distinct; source32-wrap skip is not a valid host-wide offset implementation.

Implementation decision:real Core-owned read stream resource, immutable
memory and borrowed/owned native content files, actual cursor/read/forward-
skip/close, same40F error domain, no36-byte/16-entry pool or4GiB ceiling.
Writes/append/custom/caller-embedded records/chunk helpers remain unimplemented.
Unsupported invalid source-wrap skip fails by name before effects rather
than fabricating source equivalence; valid wide host file offsets work.

Direct native dependency:append checked close APIs without changing existing
void RAII close/read/seek behavior. Close consumes a filename stream even on
failure. Native module retirement must close its owned handles; if that
checked cleanup fails, retain the named content error through real Core
unwind/cleanup and never return successful teardown or replay close.
Preserve legitimate Main transfer independently of ordinary I/O failure.
freeze_source.py pins12 full source spans/tables/modes and seven exact native
service/header/config originals before runtime edits.

SOURCE_0404.json PASS before edits:12 full original ranges, actual dispatch/
mode data and seven exact native originals are frozen. Implemented/unbuilt:
real0404 native read-stream owner with immutable memory, borrowed opened
files and owned native-content reads. Reads preserve partial bytes and
original40F diagnostics; memory overshoot skip clamps then fails, unsupported
source-wrap mapping refuses before effects, host-wide file cursors remain.
Explicit close consumes the stream even on checked file-close failure.

NativeFile/NativeContentFile add checked-close methods; existing void close,
read/seek behavior and existing error enum values are unchanged. Core retains
named checked cleanup errors through full reverse unwind and refuses replay/
successful teardown, without changing legitimate Main-transfer handling.
Actual Core0404 now owns a real resource, not a counter or console pool.
Writes/append/custom/caller-embedded records/chunk helpers remain OPEN.

Prebuild source review closes a directly needed status dependency:original
file skip failure queries the same EOF slot and reports code5 only if set.
Native files/content files now expose an actual feof observation (no cursor
change or reopen); skip retains its primary seek failure and conditionally
reports the original diagnostic. No old read/seek/void-close implementation
is altered, and new error enum entries append rather than renumber old values.

Close-fault test preflight:the direct quoted llvm-nm CMD launch was rejected
by the shell harness before any tool process ran (quoted executable treated
as literal escaped quotes). No build/client or source result exists for that
failure. Use the saved inspect_native_imports.py argv-array process instead;
inspect the actual native object's CRT import spelling before selecting a
test-only link hook, not a guessed wrapper or production sampling fix.

NATIVE_FILE_IMPORTS.json confirms actual undefined CRT names fclose/fread
(not import-pointer guesses). The new client uses only test-link wrappers
for those two real symbols; defaults forward to the real CRT, faults are
explicit component cases, not production flags/settings/sampling fixes.

Added tests (unbuilt):actual0404/Core/Main prefix,64 allocations, memory/
owned/borrowed files, original EOF/non-EOF diagnostics, native64 sparse seek,
checked close failure consuming once, Core cleanup failure/no replay,
allocation cuts preserving an existing cursor, identity/thread/lifetime/
Main transfer/task lookup. Other factories/math are explicit fixtures/abort
guards. run_check.py uses streamed bounded execution (build240s/client90s),
exclusive receipts and process-local approved TEMP/TMP only. No game/window/
audio/device route. Compile these connected changes and direct regressions now.

compile-01 PASS84.35s:all changed0404/Core/native-file/content services and
eight directly affected client targets compile/link. Existing unchanged
u8path/tautological-size warnings remain; no warning/check was weakened.
No client had executed yet. Run all eight CPU/native-file-only checks with
fresh disposable stream/content fixture directories and approved process-
local temp. No game/window/audio/GPU endpoint is started by these routes.

All eight initial clients PASS:streams-01 0.42s (module5/stream58 allocation
cuts),frames-01 0.34s,core-01 0.43s,matrix-01 0.42s,registration-01
0.40s,allocator-01 0.34s,native-file-01 0.34s,volume-01 0.34s.
Native-file/content sparse64/containment/identity/move checks pass in their
recorded scope; volume symlink creation was SKIPPED with privilege1314 and
that execution branch remains OPEN, never an all-containment acceptance.

Final source review adds a directly relevant new Core branch check:owned
file close fails during reverse unwind after later0405 constructor refusal.
Preserve streams01's measured test bytes, add the named failure/no-replay/
retirement expectation, rebuild/rerun ONLY the expanded new client. Production
code and seven passing regressions are unchanged and will be authenticated,
not rerun merely to inflate counts.

compile-02 PASS2.41s recompiles only the expanded streams client; all runtime
and other client bytes are unchanged. Execute streams02 with a fresh owned
fixture directory, preserving streams01 and its exact prior source/log.

streams-02 PASS0.14s, including real later-constructor unwind with checked
close failure, preserved cleanup error, completed retirement and no replay.
Canonical component set is streams02 plus seven exact unchanged01 regressions.
Next require previous53 names minus ONLY0404 constructor/destructor
(expected51), audit native symbols/test guards and authenticate exact source/
results before scoped publication. All full-stream/write/custom/chunk/GPU/
game and skipped symlink/oracle gates remain OPEN.

negative01 produced nonzero1.06s and symbols01 exited0 in0.06s; raw statuses
alone are not a51-name pass. finalize.py performs actual-schema preflight,
exact eight-target/source/binary/log/config checks,12 full original byte
joins/data, old native-file method retention and53-minus-ONLY0404 equality.
It records seven unchanged regressions, historical streams01/import-object
limits, the skipped symlink branch and test-only fault hooks explicitly.
Strict staged path/byte/LF/no-asset checks, commit and push are one
failure-propagating process. IMPLEMENTATION.md consolidates authority/
selected scope/native lifetimes/independent expectations and OPEN gates.

Published runtime128778d7851e1d0635f98ed67875ef1097f1f6e7 to origin/main.
All169 approved authored paths passed staging/byte/LF/no-asset gates; all909
measured source blobs match committed source exactly. Eight canonical target
identities and exact53->51 authenticate; seven unchanged regressions and all
historical/skipped limitations remain explicit. COMMITTED_AUTHENTICATION.json
binds recorded precommit executions to identical committed sources, not a
fresh-checkout/game build. Next publish that receipt and parent live status;
then0405's real resource, Xbox first. Owner:no action; no run remains active.
