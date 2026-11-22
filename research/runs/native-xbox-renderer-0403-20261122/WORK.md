# Renderer0403 — allocation/list consumers and native owner

Entry:parent88638ad2cb66eee836d3e857e12fabf194f58374,
fork91aefc25a8d49111c07b63cf3aaf992ee93cf0df, main/origin-main,
tracked source clean. Preserve all unrelated parent untracked historical
evidence, original assets/saves, backup checkouts and driver/model settings.

Owner requests the real0403 allocation/list consumers and native resource
owner, Xbox first. Reuse the already joined2BEF60..2BEFBB constructor and
2BEFC0..2BEFF2 destructor from
`../native-xbox-scene-frame-20261122/source-module-callbacks-09/`.
They create a real allocation resource, publish it only after success,
initialize a list and increment/decrement module lifetime. The concrete
element type/list consumers are not yet established; a counter-only factory
or copied console free-list/extension-offset model is not implementation.

Reuse this agent session's private saved Xbox project from the prior
SCENE_SESSION.json; the last read-only command completed and no background
run is active. Do not open or modify the shared master or local model.
Export bounded raw data/references, then actual allocation/list consumers,
join original bytes and establish ordering/errors/lifetime before runtime
edits. Use existing native Core/Session/Permanent heap/math services.

Stage:source recovery only, no production edits/build/client/game yet.
Full renderer/Open/Start/GPU/frames, numerical/HD/oracle/native save-reload
and whole-game acceptance remain OPEN.

Owner:no action. Agent next:identify all actual readers/writers of0403's
resource offset/list and the pinned allocation configuration; select the
connected native operations from verified instructions, not labels.

source-0403-01 PASS7.02s:resource-offset readers are2BF170/2BF230/
2BF640/2BF840; dirty-list users include2BF2C0,2CD630 and game8D390
(plus animation consumers). Original registry/allocation configuration is
176-byte source elements,50 entries per block,one preallocation; these are
not native capacity/layout requirements. Resource offset/list storage is
section-proven loader-zero, not fabricated file bytes.2C4620 forwards to
2C44C0;2C3FC0 unlinks the resource before cleanup2C3F60. These platform
allocation internals will terminate in existing native heap services.
Next inspect the concrete allocation/destruction/list consumers; no runtime
edit or type/complete-frame claim yet.

source-0403-02 PASS5.19s:2BF170 allocates then initializes via2BF090.
2BF230 recursively destroys children before plugin cleanup, dirty-list unlink
and allocation release.2BF840 performs single-object teardown via2BF7D0
before release.2BF2C0 queues the root at the list HEAD only when its low2
flag bits were clear, then ORs root bits3 and changed-node bits0C.2CD630
and game8D390 traverse queued roots and reset the sentinel after actual
updates.2BF640 is hierarchy clone with source alias rewrites and partial-
allocation unwind; it is NOT a required trivial allocation helper and will
remain explicitly outside an initial native create/list lifecycle claim.

The concrete records contain local/world matrices, parent/child/sibling/root
links and an attached-object list. These support a typed native transform-
frame owner rather than a generic byte pool. Next establish2BF090 birth,
2BF7D0 detach and2CD580/game update ordering, including required numeric/
attached-object effects. The pool cleanup's skipped2C3F6D..6F bytes are an
unadopted alignment gap, not a full-body proof for a new pool implementation.

source-0403-03 PASS5.17s:birth2BF090 initializes both affine matrices to
identity with properties20003, clears object flags/parent/children/sibling,
sets self root, then invokes the frame-extension registry. Read original
1.0 constant bytes and registry empty/dispatch semantics before claiming
that empty case. Single teardown2BF7D0 runs extension cleanup BEFORE
parent detach, dirty unlink and clearing children's parent links; its9-byte
listing gap is actual skipped alignment (LEA ESP,[ESP+0];MOV EDI,EDI), to
be pinned explicitly before adoption. Do not infer child-root rewrites that
this routine does not perform.

Generic2CD580 and game8D390 are DISTINCT synchronization paths. The game
path specializes a concrete attached-object callback and must not be replaced
by generic success. Initial native owner work will not claim clone, unknown
object/plugin effects or full game synchronization. Inspect public hierarchy
detach/wrappers, registry lifetime/empty semantics and bit-preserving matrix
births next; preserve required numeric/attached-object blockers explicitly.

source-0403-04 PASS5.29s:the original1.0 bits, empty initial frame
registry and skipped2BF817..81F alignment bytes are saved. Generic registry
construction walks forward, unwinds only prior successful constructors on
refusal; destruction walks reverse. FrameCreate2BF170 ignores initializer's
registry return, so do not invent a successful nonempty plugin lifecycle or
silently "fix" that behavior. The initial native owner will expose explicitly
base-frame/empty-registry allocation; real game frame-plugin/attached-object
construction and clone remain named OPEN work, not guessed mappings.

Detach2BF350 unlinks from parent's child chain, clears parent/sibling,
rewrites subtree roots via2BF000, then queues dirty. Need those actual
root/add-child/base-matrix writers before implementing native links.
Generic/game synchronization includes extra transform policies and concrete
object callbacks; it is NOT adopted in this base resource/list checkpoint.

source-0403-05 PASS5.14s establishes subtree root rewrites and add-child's
actual order:detach if parented, prepend child, rewrite roots, unlink the
child's former dirty-root entry/clear low2 bits, then dirty its new root.
Empty initial frame registry means no object plugin callbacks (original
first/last fields zero and generic loop's empty branch are joined).

Selected implementation:real0403 Core-owned dynamic base-frame resource,
source-derived identity matrices/properties, native handles, hierarchy
attach/detach and exact dirty-root queue membership/order. Expose explicitly
base-frame/initial-empty-registry allocation, NOT complete game frame/plugin
construction. Support whole ROOT hierarchy destruction and leaf-only single
destruction; unsupported attached-hierarchy/nonleaf single-delete contracts
fail by name before effects. Synchronization-required reads refuse dirty
frames rather than report stale matrices as ready. Clone, nonempty plugin/
attached-object callbacks, matrix writers and generic/game sync remain OPEN.
freeze_source.py pins full selected ranges/decoded skipped words/actual data
and native before bytes before runtime edits. No source/callback is guessed.

SOURCE_0403.json PASS before runtime edit:13 selected original function
ranges, all instruction bytes and both fully decoded skipped alignment spans,
1.0/initial-empty registry and pre-edit Core header/CMake bytes are pinned.
Implemented/unbuilt:real0403 owns native typed frame allocations and a native
intrusive dirty-root queue through existing Session/Permanent services.
Base birth, prepend/reparent/detach/subtree-root changes, duplicate dirty
suppression, root-hierarchy and leaf teardown have source-derived effects.
No176-byte/50-entry console pool, guest offset or source pointer is retained.

Core's existing0403 constructor/destructor now bind that actual resource.
Native handles cannot keep records/Session alive or refer to reused storage.
Unsupported plugin/clone/nonleaf/attached hierarchy/full synchronization paths
are not reported complete; APIs explicitly scope base records and reject
unsupported teardown/dirty-ready claims by name. Next compile connected
callers and test native allocation/errors/list order/lifetime, not rendering.

Prebuild ownership review corrected this NEW unbuilt resource's deleter:
weak frame handles must not retain NativeAllocation captures in a shared
control block after strong ownership ends. The deleter now destroys the
typed object and immediately releases captured storage/allocator capability.
Tests will independently observe actual Permanent live allocations while
retaining weak handles. Host record ownership grows geometrically, without
source block counts/capacity or quadratic one-element reservations.

Added the actual0403 client (unbuilt):source-derived identity/properties,
120 real host allocations, actual Permanent payload release while weak handles
remain, source hierarchy/root/dirty ordering, named unsupported branches,
native thread/foreign/retirement and all reached module/frame allocation
cuts. An actual Main/Core construction prefix executes production0403 and
refuses at explicit0404 fixture; legitimate Main transfer preserves the
owner and the next real task lookup. Other-factory lifetime fixtures and
abort-only math are explicit; no game/plugin/synchronization claim.
run_check.py uses the existing bounded streamed pattern with exclusive
source/build/binary/log receipts (build240s,clients90s). The routes have no
game/window/audio endpoint; SDL startup aborts if unexpectedly invoked.

compile-01 FAILED4.47s before clients:the new deleter's ordinary capture
preserved the const-qualified shared_ptr parameter, so allocator.reset()
was ill-formed despite a mutable lambda. Compiler diagnostic and exact
measured source are preserved in compile-01 and before-capture-fix.cpp.
Correct only the capture to an explicitly nonconst owned shared_ptr named
heap, then release it after typed destruction/storage release. No source
behavior, test expectation, error check or prior result is changed.

compile-02 PASS5.00s:production0403 resource/base-frame code, actual Core/
matrix/first-scene callers and all five directly affected clients compile/
link on the corrected capture. No client had executed at this point.
Run new0403 plus existing Core/matrix/raster-registration/allocator clients
headlessly. Core's content-root argument remains its recorded
research/runs/parallel-renderer-engine-init-20261122 fixture; the others
only require an existing root. No window/audio/device/game route is used.

All first clients PASS:frames-01 0.18s (module3/frame4 allocation cuts),
core-01 0.22s, matrix-01 0.16s, registration-01 0.20s,allocator-01 0.20s.
Actual120 allocations, identity/link/dirty order, physical payload release
with weak handles retained, native errors/lifetimes, actual Main prefix
unwind and Main transfer with subsequent real task lookup are exercised.
These remain component results, not plugin/synchronization/GPU/game proof.

Source review found material unexercised normal paths:2BF350's nonhead
sibling unlink and allocation refusal preserving an already-live dirty
record. Preserve frames-01's exact passing test bytes, add middle/tail/root-
boundary expectations from source prepend/unlink order, and require all
allocation cuts to retain the existing record/queue. Production code is
unchanged. Rebuild/rerun ONLY the expanded new client; retain the four
unchanged directly affected regressions with exact identity authentication.

compile-03 PASS1.75s:only expanded frames client was recompiled/linked.
No production source changed since the five passing initial clients.
Execute expanded frames-02; previous runtime/regression measurements remain
immutable and will be authenticated, not silently repinned or rerun.

frames-02 PASS0.10s with the expanded source-derived middle/tail sibling
and existing-dirty-record failure checks; module3/frame4 allocation cuts
still pass. Canonical component set is this result plus exact unchanged
core/matrix/registration/allocator01 results. Next require prior55 names
minus ONLY actual0403 constructor/destructor (expected53), and audit actual
linked owner/consumer symbols before scoped source/result publication.

negative-01 produced nonzero0.35s; symbols-01 exited0 in0.08s. These
raw statuses are not yet an authenticated53-name claim. Actual prior receipt
status is AUTHENTICATED_NATIVE_RASTER_REGISTRATION_COMPONENT with a list
of55 strings; current run status/argv/source maps and SOURCE_0403 function/
gap field types are inspected, not inferred from task text.

authenticate.py now checks exact five-target lists/lengths, all current906
source bytes, saved compiler failure/capture correction, source-derived
full spans plus decoded gaps, real typed symbols/no guest/SDL abort guard
and exact55-minus-two names. Four retained regression manifests differ ONLY
in the unrelated expanded test. The superseded frames-01 binary was not
retained; its saved hash/log/source remain historical and are NOT promoted
to a current-binary authentication. Canonical new-client evidence is frames02.

COMPONENT_AUTHENTICATION.json PASS:five exact client targets,906 current
source blobs, module3/frame4 cuts, original selected ranges/gaps/data and
all canonical binaries/logs/config authenticate. Exact55->53 removes ONLY
0403 constructor/destructor; all other names match. Actual native owner/
allocation/hierarchy symbols and no-guest/SDL abort audit pass. The four
unchanged regressions and historical frames01 distinction are explicit.
IMPLEMENTATION.md consolidates per-function authority, selected empty/base
scope, return/lifetime adaptation, independent expectations and open paths.
Next failure-propagating staging/commit/push of only authored paths.

publish.py enforces actual component/source authentication, exact approved
staged paths and bytes, LF authored files, no game assets/binaries and all906
measured source blobs against both index and resulting commit. Validation,
commit and push are one failure-propagating process. Its postcommit receipt
will distinguish authenticated precommit measurements from a fresh rebuild
and retain the base/empty-registry scope and full-game OPEN gates.
