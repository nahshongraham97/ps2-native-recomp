# Native0404 read-stream resource

Scope:compiled/tested READ component, not full streams/renderer/game acceptance.
Parent status/order/contract remain authoritative.

## Authority and selected functions

Original Xbox XBE SHA-256:
`78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`.
Actual saved program is `default.xbe`, `x86:LE:32:default`. SOURCE_0404.json
pins12 complete contiguous original ranges, original XBE section/file mapping,
all actual instruction bytes, four dispatch tables, modes and native before
bytes. No EE processor/plugin inspection is claimed or required here.

| Original | Selected native behavior |
| --- | --- |
|2B8DE0/2B8E30|Real resource creation/retirement, publication/count after successful allocation; no36-byte/16-entry pool model.|
|2B95A0/2B8FB0|Owned native records initialized before publication; failed initialization frees actual storage. Source caller-embedded records remain unadopted.|
|2B8E80,caller241140|Actual filename READ using existing retained NativeContentVolume and its native opened files; original mode is rb. Writes/append remain unimplemented.|
|2B8F10|Immutable-memory READ birth:position0,length/backing retained. Write/append memory births are not guessed.|
|2B90B0|Actual partial bytes returned; bounded memory reads advance cursor, EOF code5 and non-EOF short/error8000001A reach the SAME40F error domain.|
|2B93C0|Zero skip has no backing effect; supported memory overshoot clamps to end before code5/failure; files use real native forward seek and EOF status on failure. Source32 wrapping-invalid memory skip is a named unadopted case, not silently changed equivalence. Host-wide file cursors exceed4GiB.|
|2B94D0|Borrowed file is not closed; owned file's actual checked close result is returned and record consumed even on failure. Read-only memory close has no output descriptor. Custom/write cases remain unimplemented.|
|2C0DD0/39B16E|Actual file callback installation/status bit inspected; native services use std::feof/fread/fclose, never an Xbox FILE layout/table.|

Register/stack rewrites in the initializer helpers were checked against the
actual listing. Flags/type/access comparisons are32-bit; memcpy counts/positions
are source unsigned words. Native interfaces use typed host-owned memory/files,
wide cursors and explicit failures, not console pointers or API capacity limits.
Valid selected behavior is preserved; unsupported source-wrap mapping remains OPEN.

## Implementation and required native dependency

`retail_renderer_streams.hpp/.cpp` own real heap-backed records through the
existing Core/Session/Permanent service. Immutable memory, explicitly borrowed
opened cursors and owned native-content read files have distinct lifetimes.
Weak handles cannot keep payload allocations alive after consumption.
Source diagnostics use the actual40F owner, not invented bank/token/status IDs.

NativeFile/NativeContentFile add checked-close and EOF-query methods. Old
void RAII close/read/seek code is unchanged. Error enum values are retained;
new close/status kinds append. This is directly needed by the source filename
close and failed-seek contracts, not optional file-service cleanup.

Core retirement/unwind continues required reverse cleanup, retains checked
close errors in cleanup_content, returns named after-effect failure and
prevents replay. Legitimate NativeMainReentry remains distinct and preserves
successful ownership/task lookup. Full device Open/Start/Stop remains required.

## Observed checks and limits

Canonical streams02 plus seven unchanged renderer/frame/matrix/registration/
allocator/native-file/content clients PASS. The new client exercises64 actual
records, native64 sparse positions, exact bytes/EOF/partial reads, memory
overshoot, unsupported wrap refusal, borrowed/owned close distinction, checked
close failure consuming once, normal cleanup AND constructor-unwind close
failure/no replay, module5/stream58 allocation cuts, identity/thread/Session/
Main transfer and subsequent real task lookup.

Test-only fclose/fread link wrappers use actual inspected import names; they
are not production flags, callbacks or injected release sampling fixes. The
old streams01 binary was not retained; its source/log/hash remain historical,
not current-binary authentication. Volume symlink creation skipped for
privilege1314 and remains OPEN. No window/audio/GPU endpoint or game ran.

COMPONENT_AUTHENTICATION.json binds909 source blobs, exact eight targets,
commands/binaries/logs/flags, source/data and exact53->51 removing ONLY0404's
constructor/destructor. Counts are diagnostic, not a remaining-work inventory.
Writes/append/custom callbacks/caller-embedded records/source-wrap/chunk helpers,
other factories, native save/reload,numerical/HD/oracle/full game remain OPEN.

Owner:no action. Agent next:0405's actual resource/consumer contracts, Xbox
first, through the same native owners; retain these read-only scope limits.
