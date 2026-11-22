# Native0405 — recovery and implementation

Entry:parent3c1e77889b7744a8a45be09b43fd72bb51b23ce2,
fork0218826aa8c7cd2d80f5787edfaf491dd5352dbd, main/origin-main,
tracked source clean. User explicitly requests recover AND implement0405.
Do not redo0403/0404 or confuse their published scope with0405 completion.

Reuse the original Xbox2BB510..2BB55C constructor and2BB4B0..2BB4E2
destructor in the prior scene record. They own a real allocation resource,
publish only after success and maintain module lifetime. Element type,
consumers/defaults/callbacks/error/cleanup contracts need their own proof.
No counter-only factory, copied console object/pool or device model.

Use this agent's completed read-only private Xbox project and bounded
existing exporter; never modify master projects/plugins or access the
reserved local model. Preserve originals/saves/backups/drivers/settings and
all unrelated untracked parent evidence. Existing Core/Session/heaps,
0403/0404, content/audio/GPU services carry forward.

Stage:source recovery only; no new runtime edit/build/client/game yet.
Owner:no action. Agent next:pin configuration/resource readers and recover
actual consumers, then implement/test the verified resource and publish.
Full renderer/numerical/HD/oracle/native-save/game gates remain OPEN.

source-0405-01 PASS4.99s:only actual resource consumers are2BB920
and2BB960. Original registry/allocation configuration is400-byte elements,
four per block/one preallocation; those are not native layouts/capacities.
Resource offset/count are section-proven loader-zero storage. Inspect both
consumers and their initialization/cleanup callees before naming the object.

source-0405-02 PASS5.02s:2BB960 allocates then initializes camera-shaped
view/reciprocal/offset/clip fields and concrete callbacks2BB3F0/2BB470/
2BB440. Depth coefficients use actual device values835358/83535C,
not arbitrary zero/default constants.2BB920 runs object-extension cleanup,
then2C04D0 frame/object detach, then allocation release. Recover callback/
default/depth provider and real callers next; do not manufacture render-ready
state or transplant source400-byte layout.

source-0405-03 PASS5.12s confirms camera consumers:game7F0F0 creates
two rasters,a frame and camera,attaches them,then applies clip/view settings;
236410 is another real camera/frame/raster producer with checked failures.
Begin2BB470 publishes current camera, synchronizes dirty frames, calls the
device and then a renderer hook; end2BB440 clears current only after device
success. Those are required real GPU/frame effects, not empty callbacks.
Camera destruction detaches only if an actual frame exists.

Original defaults include view/inverse1,offset0,near0.05,far10,fog5,
projection1; depth scale/shift uses ten separately rounded scalar SSE
operations and explicit device endpoint values. Uninitialized view/frustum
fields must not become invented identity/ready data. Inspect setters/device
handoff to establish meanings and select typed native owner/base creation
with required explicit depth inputs, leaving render/sync/plugins named OPEN.

source-0405-04 PASS5.12s verifies near/far/view/projection setters and actual
object-frame linking. The indexed835358 references in unrelated module
storage are NOT proof of a direct depth writer; that production backend
handoff remains OPEN. Native creation must require explicit depth endpoint
inputs, not assume a0..1 device or use loader-zero values. No existing
camera-world/frame/raster producer is replaced with a dummy camera.

Selected implementation:real0405 Core/Session/Permanent resource with actual
typed base-camera records, source defaults and ten-step binary32 depth
calculation, view-window/reciprocal update and actual record retirement.
Base cameras are explicitly initial-empty-registry/unattached; unknown view/
frustum data is unavailable, not identity. Rendering readiness refuses by
name until actual frame/raster/plugin/sync/device effects exist.
freeze_source.py pins six full source spans/data/pre-edit native bytes and
derives independent exact-rational binary32 fixtures from original SSE order.

SOURCE_0405.json PASS before native edits:six complete original spans,data/
defaults,initial-empty camera registry and native pre-edit snapshots are
frozen. Seven exact-rational cases preserve per-instruction binary32 RNE,
including reversed/equal/narrow endpoints and signed-zero scale.

Implemented/unbuilt:actual0405 owns growing typed native base-camera
allocations through existing Core/Session/Permanent services. Source default
values and ten-operation depth calculation are preserved with explicit
required depth inputs; no guessed backend or400-byte/four-entry pool.
View-window update computes source reciprocals for unattached base records.
Native handles,physical release,thread/lifetime/identity and count retirement
are real; render readiness refuses by name rather than inventing view/frustum/
frame/raster/plugin/device success. Actual Core0405 bindings now resolve to
this resource, not counters or a test implementation.

New actual0405 client is added (unbuilt):seven independent exact-rational
SSE cases,source scalar/default/view-window signed-zero behavior,64 actual
payloads and physical release with weak handles retained,all reached resource/
record allocation cuts,foreign/thread/lifetime checks,actual Main/Core
construction prefix then explicit0406 refusal,and Main transfer followed by
real task lookup. Nonempty camera/plugins/depth provider/render effects
are not supplied by fixtures. Other-factory lifetime fixtures and abort-only
unimplemented math are explicit. run_check.py uses streamed240s builds/90s
clients,exclusive receipts and approved process-local temp; no game/window/
audio/GPU endpoint. Compile the connected changes and direct regressions now.

compile-01 PASS11.16s:actual0405 owner/base-camera code and seven affected
renderer clients compile/link. No client pass is inferred yet. Execute the
new client with frozen depth-expected.tsv and the existing fork root; stream
regression gets a fresh disposable fixture directory,Core its recorded
parallel-renderer-engine-init root. All routes are CPU/native-file only,
with SDL abort guards and no game/window/audio/GPU endpoint.

All initial seven clients PASS:cameras01 0.28s (module3/camera4 allocation
cuts,seven exact depth cases,64 actual records),streams01 0.44s,frames01
0.40s,core01/matrix01 0.28s,registration01 0.30s,allocator01 0.34s.
Native defaults/physical release/lifetime/Main transfer/task lookup pass
within component scope; no render/camera-plugin/device acceptance.

Source review found a window-update ordering mismatch in the new C++:
original2BB870 stores X,computes inverse-X,stores Y,computes inverse-Y,
then stores reciprocals. Initial code copied both window components before
division. Preserve measured01 source,correct this exact sequence (no expected
value/test relaxation),then rebuild/check current connected source. Floating
trap-mode/full rendering equivalence still remains OPEN; source effect order
is not waived by matching final default bits.

compile-02 PASS1.52s on the source-order correction; seven connected targets
relink. Run canonical02 clients on these exact current bytes,with fresh stream
fixtures. Prior01 logs/source and their before-order source remain historical,
not silently repinned as the corrected implementation.

All canonical02 clients PASS:cameras0.34s,streams/frames/matrix/allocator
0.29s,core0.28s,registration0.43s. The corrected exact current source passes
seven independent depth-bit cases,64 actual payloads,module3/camera4
allocation cuts,physical release,window signs,actual Main prefix/unwind/
transfer and subsequent real task lookup. No source/test expectation was
weakened. Next verify exact prior51 minus ONLY0405 ctor/dtor (expected49),
native symbols/source/result identities and publish the authored scope.

negative01 returned nonzero0.28s; symbols01 exited0 in0.06s. Raw statuses
are not yet a49-name pass. finalize.py inspects actual schemas and requires
exact seven-target lists/lengths,all912 source/binary/log/config identities,
six original full ranges/data/seven independent fixtures,recorded source-order
correction and exact51-minus-ONLY0405 equality. Historical01 source/logs remain
superseded,not current-source authentication. Staging/raw-byte/LF/no-assets
validation,commit and push are one failure-propagating process.
IMPLEMENTATION.md consolidates per-function authority,ABI/effect/lifetime
adaptation,selected base scope,independent expectations and OPEN gates.
