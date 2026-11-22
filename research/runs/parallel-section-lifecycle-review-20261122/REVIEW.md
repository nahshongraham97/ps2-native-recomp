# Native asynchronous file read dependency review

Candidate6382224a8a9ca7893cd35e6b9ac7427e6b39c687 atop69e0f3: no concrete defect identified within the eight changed native/build/test paths. Review-only; no test execution, author/canonical edits or new GPU operation. This is not approval of the inherited69e0f3 GPU defects or the still-unbound section lifecycle.

## Ownership and native scheduling

ReOpenFile derives an independent OVERLAPPED handle from the actual opened object; no path or archive lookup is repeated by the job. Range/regular-disk/file-size checks precede buffer ownership and read initiation. State/buffer/public job/worker self-lease allocations precede ReadFile. Event/handle/thread creation failures remain named and unwind ownership. Zero bytes produce terminal empty data without a worker.

Both ReadFile submission (which can complete synchronously) and blocking GetOverlappedResult run on the CRT-aware worker, while owner poll performs only thread admission and atomic status observation. Buffer and ordinary failure fields are written before terminal atomic publication, so observer reads follow the matching synchronization. The independent worker self-lease retains State/event/OVERLAPPED/file/buffer through return; a terminal public drop can precede actual thread exit without freeing those resources early. State is still retained by the public job while begin publishes the worker handle and releases the temporary lease holder. Closing a thread handle is not a join and does not terminate the worker.

Cancellation requests first publish an atomic flag, then request cancellation of the specific OVERLAPPED. ERROR_NOT_FOUND is a race/not-issued outcome, not completion. Worker checks the flag before each issue and after issue; each issued request reaches GetOverlappedResult before its OVERLAPPED is reset for another chunk. A completed request is never retrospectively relabeled merely because cancel was requested. Completed/failed/cancelled storage remains retained; bytes are exposed only after complete. No concrete cancellation/ownership race found in these inspected paths. This does not establish kernel cancellation occurrence in the saved tests.

Public poll/cancel/bytes enforce the job's actual owner thread before ordinary fields/native operations. Deliberate pending public-drop termination is an explicit contract, not a detached-read fallback; the future Workspace must retain jobs, cancel/poll to terminal and preserve Main-transfer/error prefixes. This fatal misuse path is not exercised by the normal client and should not be claimed tested. No whole source callback/queue/active/Main pump ownership is supplied here.

## Package/source boundary

NativeRetailPackage::begin_read preserves its owner-thread check, existing normalized member lookup and original member-size clipping, then passes that member's same open NativeContentFile to the job. It leaves the FILE cursor unchanged and retains actual volume identity. Missing/out-of-range/shortened/read/open failures remain named; no replacement provider/index or synthetic zero completion. Native host byte extents and MAXDWORD chunks are implementation details of Win32 read submission, not a PS2 sector/address/pool model. Source327230 diagnostics and source virtual objects are not broadly claimed implemented.

Original source joins support the native read prerequisite and show that source waits pump Main; they do not prove the still-required queue/subrecord callbacks, pending release/cancellation scheduling, warm reuse, conversion/cache publication or whole3011D0. Candidate changes no GPU file. Root's separate movie-order/delete-outcome correction remains necessary before adopting the combined lineage.

## Saved result authentication

Authenticated164 artifact hashes,51 compressed/raw archive identities,872 exact final source hashes,8 raw/Git newline pairs,4 frozen binaries, tool/compiler identities and original gameart.ssf hash. Independently joined all1400 source-full words across29 views to the pinned original ELF;3270C0 interior orientation remains excluded. Three05clients executed by the author: new file-read job, existing native file, existing package. CPU-section client compiled only. This reviewer executed none.

The new client uses an independently generated offset-byte formula plus existing actual package reads of all5 original section ranges; it checks original cursor preservation, provider/volume/original-handle retirement, clipping/EOF/zero/missing/overflow, foreign-thread refusal, four pre-read allocation cuts and successful retry. Final05 records50 observations,16 cancelled terminal states and0 completed races. These16 may include worker pre-issue cancellations and are NOT16 proven ERROR_OPERATION_ABORTED kernel events. Earlier02's0cancelled/16completed and03/04 histories remain preserved and authenticated. Poll counts imply no exact timing or game comparison. No new context/window/presentation/audio/movie/game execution or asset writes.

## Primary Windows contracts

Microsoft ReOpenFile: https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-reopenfile — reopens the same object with specified flags/access/sharing.

Microsoft CancelIoEx: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-cancelioex — cancellation request does not complete an operation; retain its OVERLAPPED until completion.

Microsoft GetOverlappedResult: https://learn.microsoft.com/en-us/windows/win32/api/ioapiset/nf-ioapiset-getoverlappedresult — returns transferred length/result; waitTRUE can block and is confined here to the worker.

Microsoft CRT thread lifecycle: https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/beginthread-beginthreadex?view=msvc-170 — CRT-aware worker/handle ownership. Consulted2026-11-22; no secondary sources used.
