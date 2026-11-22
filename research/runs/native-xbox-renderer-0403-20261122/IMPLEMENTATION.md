# Native0403 resource and base-frame lifecycle

Evidence level: **compiled/tested component**, not full frame/plugin,
synchronization, renderer or game acceptance. Parent live documents remain
authoritative.

## Selected original input and actual decoding

Xbox `default.xbe` SHA-256:
`78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`.
The actual saved program reports that image and `x86:LE:32:default`.
Original bytes are mapped through its own pinned XBE sections, never ELF
offsets. `SOURCE_0403.json` pins each selected listing/hash/range, referenced
data, native pre-edit bytes and two decoded skipped alignment spans.

All selected instruction bytes and referenced1.0/initial-empty registry data
are joined. This is the actual x86 listing used, not an EE-plugin claim.
No EE delay slots/MMI apply. Source flags are byte-wide with MOVZX loads;
module increments/decrements are32-bit; matrix birth writes exact0/1 bits and
properties20003 without arithmetic. Skipped LEA ESP/[ESP+0], MOV EDI/EDI and
LEA EBX/[EBX+0] spans are saved/decoded and bypassed by unconditional jumps;
they do not change registers/flags. Coverage is not complete native equivalence.

## Function/consumer mapping and selected behavior

| Original | Native implementation / verified scope |
| --- | --- |
| `2BEF60..2BEFBB`, `2BEFC0..2BEFF2` | Actual Core0403 creates/owns a real growing native resource, initializes its empty dirty queue and increments lifetime only after successful allocation. Retirement invalidates records, releases payloads and retires the module count. Original176-byte/50-entry/preallocation configuration is not a native layout/capacity. |
| `2BF170..2BF19F`, `2BF090..2BF169` | `create_base_frame`: actual native allocation; both identity matrices/properties20003, empty links, self root, flags0. This explicitly covers the source-proved initial EMPTY frame-extension registry only, not complete game FrameCreate. Original FrameCreate ignores plugin-constructor return; that nonempty behavior is not guessed or silently corrected. |
| `2BF2C0..2BF31C` | Dirty root prepends once when its low2 bits were clear; root OR3 then changed node OR0C. Repeated marking preserves membership/order. |
| `2BF000..2BF036`, `2BF350..2BF3B8` | Typed subtree root rewrite; detach unlinks first/nonfirst child, clears parent/sibling, rewrites subtree roots to itself, then marks dirty. Native iterative traversal preserves source order without a fixed stack/element limit. |
| `2BF8E0..2BF948` | Detach if parented, prepend, rewrite roots, remove the child's former dirty-root entry/clear low2 bits, then mark its new root. Native invalid-cycle/foreign/thread preconditions fail before effects; no raw-pointer crash behavior is claimed for invalid native inputs. |
| `2BF230..2BF298`, public `2BF2A0..2BF2B2` | Postorder ROOT hierarchy release under the empty-registry/base-record contract, including actual NULL input. Attached-hierarchy deletion is explicitly refused pending its full caller contract. |
| `2BF7D0..2BF831`, public `2BF840..2BF867` | Leaf-only single teardown: source-empty plugin cleanup, parent detach if needed, dirty unlink, actual allocation release. Nonleaf single-delete/root lifetime remains a named unsupported branch, not an invented root repair. |
| `2CD580/2CD630`, game `8D390/8D450/8D760/8D810` | Source inspected to distinguish generic/game synchronization and special object callback paths. NOT implemented here. `require_fully_synchronized` refuses outstanding dirty work instead of returning a ready/stale world matrix. |

Native operations return explicit status/failure plus caller-held native
handles rather than console context pointers or registry offsets. Snapshots
observe actual typed records; they are not synchronized world-matrix getters.
Nonempty frame extensions, attached objects, clone/alias rewrites, transform
writers and unsupported teardown cases remain OPEN.

## Native ownership and connected caller

- `runtime/include/mka/retail_renderer_frames.hpp` and
  `runtime/src/native/retail_renderer_frames.cpp`: real host records/matrices/
  native links, private native weak handles, geometrically growing ownership,
  actual Permanent-heap allocations and native dirty queue. No guest maps,
  fixed console pool, source byte/object offsets or emulated device.
- `runtime/include/mka/retail_renderer_core.hpp`: same Session/Core retains
  the real module. Existing ordered production constructor/destructor entries
  resolve directly to these implementations.
- `cmake/native_game.cmake`: ordinary production source/test connection;
  no runtime-enabling flags or fallback.

Weak handles do not retain payloads: the strong-owner deleter destroys the
typed object and releases its actual NativeAllocation immediately, not when
the final weak control block disappears. Module handles cannot retain the
Session or usable records after Core retirement. The initial const-capture
compile failure and exact old bytes remain preserved; no test was weakened.

## Independently expected checks and observed results

`tests/test_retail_renderer_frames.cpp` derives identity/flags/list/tree
expectations from the pinned instructions/data, not copied native outputs.
It checks head/middle/tail unlink, reparenting/root boundaries, duplicate dirty
marking, source null/root/leaf cases, named unsupported cases, actual120
allocations and physical payload release while weak handles remain.
Native owner contracts supply allocation/thread/foreign/stale/retirement
expectations. Module3/frame4 allocation cuts preserve existing dirty records
and retry after real cleanup.

Actual Main/Core registration construction executes production0403, then
refuses at an explicit later0404 fixture. A real Main transfer retains the
successful0403 prefix and permits subsequent actual task lookup; explicit
cleanup retires it once. Other-factory lifetime fixtures and abort-only math
are labeled; they do not prove those implementations.

Canonical results: expanded `frames-02` plus unchanged Core/matrix/raster-
registration/allocator01, all PASS. The four old manifests differ only in
the unrelated expanded new test and their actual binaries remain identical.
Historical `frames-01` binary was not retained; its hash/log/source are
historical, not authenticated as the current binary.

`COMPONENT_AUTHENTICATION.json` authenticates five exact targets,906 source
blobs, original words/gaps/data, compiler/config/commands/binaries/logs and
exact55->53 (ONLY0403 constructor/destructor resolved). This is diagnostic,
not the full remaining-work inventory. No game executable or game run exists
for this checkpoint. No window/audio/GPU endpoint or local model was used.

Owner:no action. Agent next:the next connected0404 resource/factory through
the same native owners; retain full frame plugin/clone/synchronization and
renderer/Open/Start/GPU/native-save/oracle/game gates as OPEN.
