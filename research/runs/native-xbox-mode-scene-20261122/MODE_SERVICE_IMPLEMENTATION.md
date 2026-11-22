# Mode-zero service — source and implementation receipt

Evidence level: **compiled/tested component**, with exact source/result
authentication. Not a full-game build or oracle comparison.
Live status/order/acceptance remain owned by the parent project documents.

## Original inputs and decoding actually used

- Xbox `default.xbe`, SHA-256
  `78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`.
  `MODE_ANCHORS.json` owns its section mapping; each saved `*.join.json`
  maps listing instruction bytes through that section's file-backed range.
- The inspected program reports `default.xbe`, that hash and
  `x86:LE:32:default` in each `PROGRAM.txt`. These are the actual listings
  inspected, not a claim that an EE plugin was used.
- `SERVICE_COMPONENT_AUTHENTICATION.json` records each function's inclusive
  range, listing SHA-256, instruction count and any unlisted byte gap. The
  selection/reference functions below have complete contiguous coverage.
- `MODE_ZERO_DATA.json` saves the original descriptor/list/name bytes and
  each image's own mapping. PS2 comparison input SHA-256 is
  `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`;
  its mappings use its actual PT_LOAD metadata, not Xbox offsets.
- `MODE_SERVICE_DISPATCH.json` saves the original table at `23D154`.
  Policy0 targets `23D0B1`: scan first, requests second. The failed assumption
  about the order of **unadopted policies1..3** is preserved in
  `MODE_SERVICE_DISPATCH_FAILURE_01.json`; the policy0 expectation never changed.

## Behavior established from instructions/callers

| Original function/listing | Established behavior and native destination |
| --- | --- |
| `source-mode-01/0023c8a0.asm`, `0023d060.asm` | Stack-passed 32-bit descriptor/policy arguments; caller cleans stack. Signed descriptor/policy validation. Descriptor policy at +24 is0 for both adopted lists. `23D091` stores selection before policy0 scan/request effects. Native entry points expose only the two verified descriptors, not arbitrary console indices/policies. |
| `source-mode-zero-17/0023d4d0.asm` | Ascending scan0..343, signed32 loop bound344. Only nonzero handle or pending word exactly1 enters membership. Nonmember goes to retirement. Catalog size is retail data, not host resource capacity. |
| `source-mode-zero-18/0023d540.asm` | Signed32 list-count loop, first word of each12-byte request compared by equality after the existing index-normalizer call. Native adopted entries are already valid known catalog indices. |
| `source-mode-zero-17/0023d830.asm`, `source-mode-zero-19/0023d8d0.asm` | Traverse requests in list order; normalize bank, then bind slot even for existing loaded/pending bank. Slot flags are an original **byte** load, not sign-extended data or the request's third word. The request's third word is not consumed by this traversal. Previously bound slots outside the selected list are not cleared. Existing native jobs retain their verified pending suppression/callback behavior. |
| `source-mode-zero-18/0023d230.asm`, `source-mode-zero-19/0023c700.asm` | Live bank path calls reference cleanup before bank unload, then clears bank field. Pending-only path requests pathname cancellation, not an unconditional pending clear. Native catalog uses actual callback results; real head work retains pending until its completion. Native errors are explicit rather than donor assertion/debug loops. |
| `source-mode-zero-20/0023baf0.asm`, `source-mode-zero-21/0023bb20.asm` | Repeatedly find first matching **bank**, not semantic sound ID; reference scan is ascending and compares the bank field. Native typed bank identity selects the same records in the existing Main vector. |
| `source-mode-zero-21/0023bb70.asm` | Null record returns; otherwise recover record index, look up actual play, retire it if present, then remove record. Native observation/forced retirement supplies the existing resource cleanup; a named lookup/cleanup error preserves the uncompleted record/backing. |
| `source-mode-zero-22/0023bbc0.asm` | Decrements count, removes selected record and clears tail. Its non-tail copy passes a raw count difference to byte `398910` memmove; **this storage behavior is not adopted or claimed equivalent**. Native typed record erasure preserves unrelated live shared ownership. |

These x86 functions have no EE delay slots/MMI operations. Signed loop
comparisons, full32-bit identities/sentinels and byte flags were established
from instructions rather than decompiler variable types. No donor pointer,
fixed address, register file, device model or record-array capacity enters
the native runtime. Lower donor playback/transport internals are supporting
context, not replacement implementations: existing verified native services
remain the production destination.

## Native code and independently expected checks

- `runtime/include/mka/retail_sound_catalog_data.hpp`: original mode0
  banks `{0,8}`/slots `{0,4}`; mode1 `{0,8,2,1}`/`{0,4,22,21}` retained.
- `runtime/src/native/retail_sound_bank_catalog.cpp` and its header:
  descriptor-first shared policy0 selection, ordered retire/unload,
  request/bind, real cancellation, selection/reentry/after-effect guards.
- `runtime/src/native/retail_sound_bank_main.cpp`: actual Main
  `initialize_retail_mode_service` and reset mode1 both use the SAME native
  Session/resource domain and typed bank-reference retirement.
- `tests/test_retail_mode_service.cpp`: original list bytes determine exact
  requests; the listings determine descriptor-before-cleanup, ascending
  retirement, bank-specific selection, cleanup-before-unload and stale slots.
  Existing verified317960/3175E4 job semantics determine immediate nonhead
  versus deferred-head cancellation expectations. Native service contracts
  determine named failure, thread/lifetime, reentry and Main-transfer checks.
  Ordinary failure is terminal rather than replaying partial effects;
  legitimate Main transfer releases admission and permits the next call.
- Original shell metadata contains no selector0; selector10 is the first
  present entry. The fixture reads that metadata before requesting a real
  held play. Failed fixture results and measured source bytes remain saved;
  no failed play is skipped and no decoder/assertion was weakened.

## Observed verification and limits

`service-compile-05` compiled the service/reset/metadata clients; all three
executions passed (`service-mode-zero-05`, `service-reset-01`,
`service-metadata-01`). The service test includes queued/deferred-head
cancellation and real selective held-play cleanup. Metadata checks all9656
original rows. `service-negative-01` matches the prior exact58-name list
minus ONLY `initialize_retail_mode_service` (57 remaining); it does not
produce a game executable. `service-symbols-01` verifies actual typed service
definitions, SDL-startup abort guard and absence of audited guest symbols.
`SERVICE_COMPONENT_AUTHENTICATION.json` pins commands, binaries, logs,
compiler/cache/flags, all901 source files, original assets and evidence.

The context is an explicit48000-Hz/three-role native fixture with no output
device/runtime worker. The selective pending-**play** branch requiring the
actual `sound_runtime` remains unexercised here; bank-job pending cancellation
is exercised. Policies1..3/other descriptors, full scenes/game loop, native
save/reload, physical input/audio, numerical/HD, same-scenario oracle and
whole-game acceptance remain **OPEN**. No game/window/speaker endpoint ran.
Original sound assets and decoder/playback algorithms are unchanged.

Owner: no action. Agent next: publish this bounded service checkpoint, then
continue the connected scene/frame/input/render implementation, Xbox first.
