# 0407 raster recovery and donor decision

**Source-derived recovery only. No0407 runtime/config implementation,build,
component test or GPU/game run.** All915 measured native source files are
unchanged;both0407 factory bindings remain required. Parent status/order/
contract own current implementation and acceptance.

## Decision

Use the cross-checked common resource/context contract and **Xbox scalar
format/channel-conversion evidence** for the host-neutral bridge. Reuse
NativeRetailRendererImages,NativeGlTextureUpload,NativeRetailLoadedSystemArt
and existing Session/content/heaps/decoders. Do not port either console backend.

Concrete avoided work:host GPU storage does not need PS2 packed backing
descriptors OR Xbox texture headers,aligned console allocations/resource
registration. The recovered scalar Xbox format tables/byte converters are
an easier bridge authority than adopting the inspected EE packed create path.
This is an operation-level decision,not a measured speedup or proof every
0407 mode becomes substantially easier. Similar generic factories alone
establish no advantage;real target/depth/view/lock work remains required.

The existing public native texture API proves RGBA upload/sampler/revocation
ownership,not general targets/depth/subresource/readback. Implement missing
effects through real native GPU owners/queried limits. No source100-byte
module,52-byte record,ten-entry stack,extension offset,texture header or
numeric device dispatcher belongs in native memory.

## Authority and coverage

Xbox:`default.xbe`, `x86:LE:32:default`,
SHA-256`78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`.
PS2:`SLUS_214.10`, `r5900:LE:32:default`,
SHA-256`a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`.

Actual private programs/listings and each image's OWN XBE sections/ELF
PT_LOAD mapping are pinned in `source-*.original.json`/`.join.json`.
File-backed data differs from loader-zero storage. Reused session/exporter
identity is pinned;EE extension12.1.4/language1.4.0 is provenance,not
semantics proof. Decompiled C is orientation only.

Cached references missed real context users. Original.text candidates were
followed to actual instructions. PS2 system1B9CF0's old21-instruction
listing omitted964 bytes of real switch cases;separate source-ps2-08
decodes all262 words in the bounded original span. Earlier sparse bytes/
coverage remain unchanged. PS2 backing create1B34C0 still has omitted
words/helper semantics OPEN;it is dependency orientation,not an adopted
whole implementation. Exact detailed findings/negative candidates are in WORK.md.

## Recovered connected contracts

| Original Xbox boundary | Established behavior / native requirement |
| --- | --- |
|2C0020 /2B9F70 /2B9F10|0407 registration and actual default-context/record-resource ownership. Default raster fields zero except flags80;not an allocated/render-ready framebuffer. Count increments on resource success. Resource teardown is not a per-raster semantic destruction traversal.|
|2B9E50|Allocate;copy32-bit dimensions/depth,parent=self,null pixel/palette,zero flags/private bits/16-bit offsets. Call concrete create(0,raster,flags). Refusal frees record;success runs extension initialization and ignores its return. No invented stride/format/backing.|
|2B9C70|Reverse extension destruction BEFORE platform destroy;ignore platform return,release record,return1. A generic unconditional backing free would be wrong.|
|2C7140 /2C7370 request11|Actual backend targets:slot4 create2C9380,5 destroy2C95D0,7 image->raster2D14C0,6 readback2D0F10,12 view metadata2C9810,15/16 lock/unlock2C85F0/2C8960. Historical slots identify proof,not a native dispatcher.|
|2B9AE0 /2B9CB0 /2B9D50|Current target lookup;push publishes stack only AFTER context callback success. Pop decrements BEFORE previous-target callback and does NOT restore index on failure. Native stack grows;invalid underflow fails by name rather than accessing adjacent source memory.|
|2D03B0 /2B9B60|Context callback only publishes current target pointer,not a framebuffer bind/draw.2B9B60 guards positive context then invokes clear;it is NOT pop. Later rendering owns real GPU effects.|
|2B9DF0 /2C9810|Subview requires no-allocation flag80. Dimensions and wrapping16-bit parent+rectangle offsets store BEFORE callback. Only success aliases parent's ROOT;backend copies stride/depth/type/format. No clipping/refcount/lifetime validation is proved here.|
|2B9D90|Mip query,NOT subraster:format bit80 clear ->1;otherwise slot28 writes count through argument storage. Refusal returns signed-1. No guessed mip formula/out-pointer ABI.|
|2C8BD0 /format tables|Type=flags&7,flags byte=low byte&F8,format byte fromFF00 with type-specific defaults/depth/alpha. Some error-recording paths return1;an error latch alone is not proof callback returned failure.|
|2C9380 /2C91C0 /2C8F70|Actual backing/no-allocation/zero-dimension,format/mip/palette/header allocation and unwind. Source64-byte pitch/alignment/header restrictions are platform artifacts,not universal host requirements. Remaining helper/format semantics stay OPEN.|
|2C95D0|Root/view and borrowed/owned resources differ. Views do not release root storage;display/depth resources may be borrowed. Use typed native backing/view lifetimes,not source pointers or blanket release.|
|2C0E90 /2D14C0|Upload success precedes gamma-flag propagation. Reject compressed target conversion;conditionally acquire/release pixel/palette locks,convert by actual depth/format. CPU allocation is not upload success.|
|2D1170 /2D13C0 /2D1010|Byte-row copy is not RGBA upload. Other paths consume real pixel/palette bytes,strides and per-format converters. Do not guess serialized asset encoding from a helper name.|
|2D04E0 /2D04C0|RGBA bytes ->AARRGGBB for0500,FFRRGGBB for0600(BGRA bytes on LE). Native RGBA upload preserves sampled color/alpha,not intermediate console byte order.|

## Bounded PS2 comparison

Original registration selects1A89C0/1A8950 through signed ADDIU89C0/8950,
not1B89C0/1B8950. Generic create1A87F0 agrees with Xbox's record/callback/
failure/ignored-extension-result shape.

Actual device path1A6C30 ->1BA110(descriptor5A4130) ->1A68F0 ->
1B9CF0(request11 arm1BA0C0) ->1B7060 installs original table5A4050.
Slots4/5/7/15/16 select1B34C0/1B53D0/1BE5D0/1B6050/1B6BF0.
Some other entries are explicit null;never mix platform tables/layouts.
Create1B34C0 has wide/packed backing construction;image transfer1BE5D0
has concrete lock/palette/conversion/release paths. Helper internals and
omitted create words remain unverified/not adopted. Stop this bounded
comparison here,not reconstruct the entire second graphics library.

## Independent future expectations and next gate

- Default sentinel:zero dimensions/depth/backing,flags80,no render readiness.
  Failed resource/create admission must not publish a usable owner.
- Check source callback/destruction order,prior effects and Main transfer
  without invented cleanup replay;native owners must retain real backing.
- Push refusal leaves stack unchanged;pop refusal leaves it decremented.
  Exercise nesting beyond the old array and named underflow.
  Distinguish callback success from the raw returned target:successful
  push of null still returns null AFTER changing the stack. Valid target/
  sentinel/empty domains must be explicit,not inferred from that raw result.
- Parent offsets(FFFF,7FFF)+rect(1,1) ->(0000,8000). Dimensions/offsets
  change before callback;root alias changes only on success. Establish valid
  caller lifetime before choosing a parent-retirement policy.
- RGBA12,34,56,78 ->78123456 for0500,FF123456 for0600. Check
  alpha00/80/FF,RGB extremes,palette indices,strides/padding and refusals.
- Type4/no explicit format normalizes to0500/depth32;input depth alone
  is not authority after format selection. Derive error-plus-success cases.
- Real GPU create/upload/delete,targets/depth,subviews/mips,compressed
  formats,readback and uncertain deletion/context outcomes need connected
  tests and same-scenario/retail comparison;none ran in this recovery.

Next implement real typed0407 ownership/context and a connected verified
native backing path. Required effects fail by name until implemented;an
allocation-only/always-success backend is not a port. Prior47-name link
evidence is retained,not rerun or a remaining-work inventory.

No game/window/speaker/GPU endpoint or local model used. Native source,
assets/audio/saves/backups/drivers/settings unchanged.
Owner:no action. Agent next:implement0407,not skip to0408. No run active.
