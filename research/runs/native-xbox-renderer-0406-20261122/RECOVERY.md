# 0406 recovery — CPU image services, not a GPU module

## Outcome and route decision

**Source-derived recovery; no0406 runtime implementation or component run.**
Xbox first identified the actual resource/consumers. A bounded comparison
with the matching original PS2 functions does **not** establish substantially
less native implementation work for0406. Shorter x86 listings are not that
proof. Do not advertise an Xbox-specific porting speedup for this module.

The resource owner,record creation/disposal,path/format services and gamma
tables are CPU work. The matching PS2 owner is not a GS/VIF/VU implementation
that Xbox allows us to discard. Conversely,Xbox's CRT/x87 power path is not
a free correct host-math implementation. General numeric parity is OPEN.

Choose one shared typed native CPU image service using the recovered Xbox
contract and PS2 cross-check. Reuse existing Core/Session/Permanent allocation,
NativeContentVolume/NativeFile,decoded texture/font data and native GPU owners.
Do not rewrite those services,clone either console's CRT,copy544/28/52-byte
runtime layouts,or carry a console pool/address/device model into native code.
This avoids duplicate platform implementations;it is not a measured Xbox-only
benefit or a reason to weaken verification.

## Original authority and actual decoding

| Platform | Original SHA-256 | Actual private program/language |
| --- | --- | --- |
|Xbox|`78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`|`default.xbe`, `x86:LE:32:default`|
|PS2|`a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`|`SLUS_214.10`, `r5900:LE:32:default`|

SESSION_REFERENCE.json pins new private copies of the existing saved projects.
Every export pins program/hash/language,actual instruction bytes and its OWN
image mapping. Xbox uses its original XBE sections;PS2 uses actual ELF
PT_LOAD file offset=p_offset+(VMA-p_vaddr). File-backed data and loader-zero
storage are distinguished. Master projects were not edited;no shared connector
or local model was used. The installed EE extension is version12.1.4,
language definition1.4.0;the authentication receipt pins its available files.
Installation alone is not evidence of semantics. Actual listings are saved.

Each `source-*.original.json` saves original query data/mapping and each
`source-*.join.json` joins the exported listing instruction-by-instruction.
RECOVERY_AUTHENTICATION.json binds them,full spans,explicit gaps and stage
limits. Decompiled C remains orientation only,not ABI/type/numeric authority.

## Recovered connected contracts

| Xbox source | Established selected behavior / supporting consumer |
| --- | --- |
|2C0020 registration,2C010A..129|0406 constructor2BCE10/destructor2BBC00,original module extent0x220. Extent does not define a native runtime object.|
|2BCE10..2BCFB1|Creates image-record allocation resource,format-record resource,path buffer and scratch buffer;initial path is empty. Builds tables with scalar input1.0. Distinct partial-failure cleanup paths are real.|
|2BBC00..2BBCD7|Releases scratch,path,linked format records,format allocation resource,image allocation resource,in that order. Does not call image destructors for every remaining image;native lifetime must be explicit,not infer live-image teardown from a pool name.|
|2BBE30..2BBE83|cdecl width,height,depth are32-bit values stored without validation. Allocation failure returns null. Success sets flags0,pixels/palette null,then invokes image-extension constructors;their return is not tested here. **Stride is not initialized by this routine.**|
|2BBE90..2BBF49|Derives4-byte-aligned stride and backing size;depth4/8 add16/256 four-byte palette entries. Indices occupy one byte per pixel,including depth4. Allocates backing,sets palette after pixel bytes and owned flag1 on success. No source clamping/zero fill is established.|
|2BCFC0..2BD00E|If owned bit1 is set,free pixels,clear pixel/palette pointers and owned bit before extension cleanup. Then release record. Borrowed backing is not freed. Successful return1 is tied to real effects,not an empty callback.|
|2BC1D0..2BC26F|Image-search path length+terminator,checked growth,copy including terminator;returns original argument,not internal copied storage. Allocation failure preserves prior pointer/capacity and records source error0x80000013.|
|2BBCE0..2BBD61|Filename scratch grows only as needed,uses allocate/reallocate accordingly,publishes pointer/capacity only after success and reports the same allocation error on refusal.|
|2BC590..2BC6AD|Creates or updates extension format records,linked at head. Two text representations and read/write callbacks are consumed by reader selection. Nonnull callbacks update;null does not erase an existing callback. Concrete callback implementations/asset formats remain separate obligations.|
|2BC410..2BC4D3,2BC2B0..2BC3D2|Finds extension after path punctuation,selects a registered reader,then searches semicolon-separated paths or the direct filename through the actual callback. Missing format/reader or allocation refuses;no decoder/file-success fallback is established. Exact callback/path edge behavior still needs the eventual connected implementation checks.|
|2BBDD0..2BBE29|Applies table1 independently to three RGB bytes,copies alpha unchanged,and advances one RGBA record. Zero count does no work. No SIMD/guest register runtime is needed.|
|2BCC40..2BCD42|For depth32,requires pixels and applies RGB mapping by rows using stored stride;for4/8,requires palette and maps16/256 RGBA entries. Unsupported depth uses error0x80000008;missing backing uses0x80000016. Flag2 is set on the successful paths,including nonpositive row count when backing exists.|
|2BCD60..2BCE0D|Stores gamma,input reciprocal rounded by DIVSS,and builds two256-byte tables. Both zero entries are explicitly0. Source x87 operands,rounding and helper chain must be resolved before claiming host-math parity.|
|2BFEB0..2BFF72|After constructing actual Core modules,rebuilds tables from the real835350 word. That word's producer/domain is unresolved. This handoff cannot be replaced with assumed1.0.|
|7D350..7D427|A real game consumer converts an image to a raster,disposes the image,then creates/publishes a texture. CPU image readiness is not native rendering readiness.|

Widths and effect order matter. Xbox pixel extent arithmetic uses32-bit low
products/wrapping,arithmetic right shift for(depth+7)/8 and a4-byte row mask.
Do not invoke signed C++ overflow or turn a wrapped console buffer size into
an unsafe host allocation. Prove caller domains and use checked native spans/
ownership;unverified extents fail by name rather than silently change pixels.

Pixel-allocation refusal occurs AFTER storing the derived stride and null
pixel pointer;palette/owned flag are only updated on success. Failure of the
constructor's final scratch allocation occurs AFTER the source count increment;
it frees prior allocations without decrementing that diagnostic word. The saved
count references are constructor/destructor only,but its ultimate native
diagnostic projection is not decided here. Do not claim a success-only count
from0405's different implementation.

The original image registry starts with base extent28 and no linked callbacks
in the saved file-backed data. This is an initial state,not proof that every
reached game image remains plugin-free. Do not use an empty-registry success
stub to erase later registrations or invent initialized stride/pixels.

## PS2 comparison: what is and is not easier

Actual prior registration1A66A0 selects1A7DE0 and1A8000. The latter address
is established by signed ADDIU0x8000 in the call delay slot,not1B8000.

| Matching PS2 source | Comparison with Xbox |
| --- | --- |
|1A7DE0..1A7FFF /1A8000..1A8183|Same two allocation resources,two buffers,table initialization,partial-failure ownership and release order. No MMIO/VU/device dependency in these owner spans.|
|1A8190..1A8233|Same width/height/depth,flags/pointers and deferred-stride creation contract. BNE-likely at1A81E0 annuls its width-store delay slot on null;Xbox has a conventional branch. This reduces inspection complexity,not the native service's work.|
|1A8240..1A82AB|Same owned-backing -> image-extension cleanup -> record-release order,with1A82B0 handling owned pixels. This matches an ownership contract,not byte-layout reuse.|
|1A8380..1A849F,122580->11EA20|Same two gamma tables,but scalar EE divide-by255 and float power path versus Xbox float reciprocal multiply/x87/binary64 helper chain. Numerical equivalence is NOT established.|
|2DF860..2DFA3B|Actual caller creates128x128 depth8 image and supplies borrowed pixels/stride. Owned-only image storage would miss real retail behavior. This inspected PS2 consumer is not proof of identical Xbox asset bytes.|

PS2 math11EA20's positive exponent1 path is verified from actual words:
F13 supplies exponent,F12 base;the branch-likely comparison's untaken delay
slot is annulled,and11EAB0's instruction sets the base inF0 on the taken
positive-exponent return path. This does not certify general power numerics.
There is no MMI/VU algorithm to preserve in the selected image owner/create/
disposal comparison. EE save/restore operations do not justify a guest runtime.

Xbox's helper399A4C is a jump thunk;399A4E marshals operands into doubles
and calls399A70. That body uses FYL2X,control-word checks,exponential/error
helpers and tail continuations. Importing it is not simpler than implementing
the required quantized native image operation. For table1,the original
normalized x87 value is retained across anFST(float) before being marshaled
as double;table2 reloads the rounded float. Do not collapse these dataflows
into one guessed `powf` expression. Host math is a candidate only after
independent output/edge-case comparison for the actual reached gamma domain.
Do not change host FP settings or implement a floating-point device model.

General PS2 power math and all Xbox CRT helper/error continuations remain
orientation/dependency evidence,not selected port implementations. Candidate
1A84A0 was NOT image creation;see REVIEW_CORRECTIONS.md.

## Independent expectations and next implementation gate

Future behavior checks must derive from original instructions/callers:

- Record allocation refusal publishes no handle;creation has unavailable
  stride until a real producer sets it.
- For3x2 images:depth4 -> stride4,pixels8,palette64,total72;
  depth8 -> stride4,pixels8,palette1024,total1032;
  depth16 -> stride8,total16;depth24/32 -> stride12,total24.
  Add zero/negative/overflow/domain cases without assuming they all succeed.
- Pixel-allocation refusal preserves the documented prior effects;path/scratch
  growth refusal retains its previous owner/content. Exercise all constructor
  allocation cuts and the final-allocation count discrepancy.
- Borrowed pixel/palette owners remain alive through callbacks;owned backing
  is released before extension destructors. Plugin errors/lifetime and actual
  concrete callback ABI must be proved,not guessed.
- RGB-table mapping preserves alpha,row padding and indexed pixels while
  modifying the actual palette. Check RGB0/255,zero count/rows,unsupported
  depth,missing backing and source errors.
- Initial1.0 tables,835350 handoff and any later values need independently
  justified quantized expected bytes and connected producer checks. No math
  fixture or native table implementation is claimed in this source-only batch.

Next implement the shared CPU owner and actual connected callers only after
resolving the required initialization math/extension contract. Keep named
0406 constructor/destructor production link blockers until those effects
exist. Retain the prior49-name diagnostic unchanged;it is not a completion
metric. No component build/test,game/oracle/physical output pass is inferred.

Owner:no action. Agent next:resolve the required gamma-table initialization/
producer domain,then implement the recovered CPU contract once in existing
native services. Do not skip to0407 or redo0403/0404/0405. No run is active.
