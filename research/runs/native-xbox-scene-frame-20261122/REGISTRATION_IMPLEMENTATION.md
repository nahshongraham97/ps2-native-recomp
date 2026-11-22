# Selected Xbox renderer registration — implementation receipt

Scope: **tested component**, not renderer/GPU/frame/game acceptance.
Parent `PORT_STATUS.md`, `ROADMAP.md` and the execution contract own live state,
implementation order and acceptance.

## Authority and actual source used

Original Xbox XBE SHA-256:
`78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`.
Its own file-backed section map is reused from
`../native-xbox-mode-scene-20261122/MODE_ANCHORS.json`.
Every new saved listing instruction joins those original bytes in its
`source-*.join.json`. Actual program identity/language is saved in each
`PROGRAM.txt`: `default.xbe`, that SHA-256, `x86:LE:32:default`.

`REGISTRATION_SOURCE.json` pins full contiguous ranges and listing/join hashes
for the selected routines and consumers:

- `2C84D0..2C84F3`: core tail registers object extension `040C`, original
  size24 and three explicit null callbacks; saves the returned result and
  returns signed32 `result >= 0`. Original size/offset are not native layouts.
- `2B9CF0..2B9D16`: five32-bit stack arguments forwarded to concrete
  registry `5A66F8` and existing `2C2990`; result is unchanged. Argument
  loads were checked against the intervening pushes, not decompiler types.
- `2B9E50..2B9EDE`: actual object allocation/base-field initialization and
  device creation precede registry construction; failed device creation
  frees the actual object and returns null. This is a **consumer boundary**,
  not a ported GPU/object constructor.
- `2B9C70..2B9CA7`: registry destruction precedes the separate device cleanup
  and allocator release. No native device cleanup is synthesized here.

The existing generic `2C2990` metadata/duplicate/error behavior is reused
from the already verified registry implementation and its pinned
`../parallel-renderer-engine-init-20261122/XBOX_WORD_JOIN.json` evidence.
No EE semantics are inferred for the adopted x86 code. There are no delay
slots/MMI lanes in these adopted routines; comparisons/arguments are32-bit.

## Source reconciliation

`7D460`, not the old late `2330C0` label, is the actual Xbox renderer
initialization chain. `2330C0` sets up system art/permanent strings.
The Xbox core tail is one raster-family metadata registration. PS2's
`1B72B0` instead contains92/216-byte device extensions and0110 stream hooks;
those platform-specific layouts/policies are not a universal native gate.
No Xbox startup placement is transplanted: this selected operation joins
the existing native Main -> allocator -> CoreInit order and its one Session.

Three source callback nulls are independently verified, not successful empty
stand-ins for unknown services. The operation being implemented is actual
metadata registration, **not** creating the device payload, constructing a
raster or opening/starting a graphics device. Those consumers remain named
production blockers. Existing native GPU/content/heap/input/audio services
and all verified functionality are retained.

## Native implementation and lifetime

- `runtime/include/mka/retail_renderer_core.hpp`: private-constructible,
  noncopyable typed native registration and immutable observation spec.
- `runtime/src/native/retail_renderer_core.cpp`: real metadata allocated
  through the existing Permanent heap capability, published only after full
  allocation, retained by the actual Core, duplicate identity/error behavior,
  sealed/callout admission, Session/thread validation and retirement before
  owner release. No console bytes, source offset or device register exists.
- `runtime/src/native/retail_renderer_raster_registration.cpp`: connects
  the selected operation and real owner retirement through the existing
  historical integration names. This is not a second renderer.
- `cmake/native_game.cmake`: ordinary production source and test target;
  normal SDL dependency staging, no launch flags or fallback.

Retained metadata cannot keep a Session alive or remain usable after native
owner retirement. Null callbacks do not imply GPU lifecycle completion.

## Independent expectations and observations

Original `2C84D0` pushes determine ID040C/null callbacks and signed success.
The existing source-proved registry determines duplicate identity/error and
closed admission. Its unchanged ErrorDomain forces plugin0 for high-bit
codes (`19F494..19F4A0` in the saved prior listing); the initial new test's
plugin1 expectation was wrong and its bytes/failure remain preserved.
Host-owned lifetime/thread/allocation expectations derive from the existing
native owner contract, not a copied console pool or arbitrary resource limit.

`scene-registration-02` invokes actual Main first-scene and CoreInit with the
production tail; its still-unported full225D40 suffix explicitly refuses.
Unported constructors/math abort if invoked. All3 reached allocation cuts,
duplicate/sealed/callout/thread/retirement/Session checks pass.

Existing Core/allocator/matrix component checks pass. Core's first invocation
used the wrong fixture root; its failed result is preserved and the corrected
invocation uses the same unchanged executable. Allocator/matrix results are
retained, not rerun: only the unrelated new test expectation differs in their
source manifests and the authentication records that exact distinction.

`REGISTRATION_COMPONENT_AUTHENTICATION.json` authenticates all four target
identities,903 source blobs, commands/binaries/logs/compiler/cache/flags,
original listing joins and failed-source correction. Exact57->55 removes
ONLY registration/cleanup seams; it is diagnostic, not a work inventory.
No game executable is produced by the negative-link check. Symbol audit
finds actual native metadata functions/SDL abort guard and no audited guest
runtime paths.

Open: all unported factories, device/raster creation, EngineOpen/Start,
whole scene/frame/input/render integration, native save/reload, numerical/HD,
same-scenario oracle and game acceptance. No visible window, physical input/
audio endpoint, GPU device or game ran. Source/assets/saves/driver/model
settings and preserved checkouts are unchanged outside the authored scope.

Owner: no action. Agent next: recover the first real renderer module's
resource creation/consumers from the prepared Xbox program, then implement
through existing native owners; do not substitute counter-only factories.
