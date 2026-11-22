# Native0406 — Xbox-first recovery and route selection

Entry:parentc194d23492ad09cc854ed7f6da3b8c531e403b8d,
forkf3fb51973e271c6187d94709915ee7fe976eb200,main/origin-main,
tracked clean. Existing untracked parent evidence is not part of this task.
User requests0406 recovery,Xbox first,and explicitly requires that the Xbox
route materially simplify the port rather than merely change disassembly.

Keep verified0403/0404/0405 and all existing native owners/services.
Original audio/assets,user saves/cards/states,backups,drivers and local model/
settings are untouched. The reserved local model is not accessed.
No game/window/audio/GPU endpoint is authorized by a source export.

Saved Xbox registration2C0020 lists0406 constructor2BCE10,
destructor2BBC00 and original module extent0x220. This extent alone
does NOT name a type or justify copying544 source bytes into native state.
Recover actual effects,consumers,failures and lifetime from original words.

Route decision gate:identify the platform-independent behavior and actual
platform-service dependency closure,then compare the matching PS2 contract.
Document concrete removed work/reused services or state that no substantial
advantage is established. Do not count reports/instruction totals as runtime
progress or import an Xbox device/runtime just to follow Xbox first.
Use easier verified matching portions;neither console layout defines native.

Stage:recovery only,no new runtime edit/build/test. Create private read-only
project copies with the existing helper;reuse installed tools and saved
program analysis,not a new setup. Original source identities/mappings and
full selected spans must be joined before semantic claims.
Owner:no action. Agent next:read0406 ctor/dtor and real consumers,Xbox first,
then bounded PS2 comparison and an evidence-backed adaptation decision.
Full native game acceptance remains OPEN.

Private-project preparation PASS0.80s:both saved programs copied/verified by
the existing helper;master bytes unchanged,no shared connector/settings used.
SESSION_REFERENCE.json pins the private manifest and helper. Bounded exporter
will verify the selected program/hash/language and join listed instructions
through each image's actual XBE section or ELF PT_LOAD. Gaps remain explicit
OPEN bytes,not silently treated as padding;decompiled C is orientation only.

source-xbox-01 PASS5.95s:actual0406 ctor2BCE10..2BCFB1 (108
instructions) and destructor2BBC00..2BBCD7 (56) are exported and original-
byte joined. Constructor owns TWO allocation resources plus two256-byte
buffers,and calls2BCD60(1.0). It increments the source module count before
the final buffer allocation;failure there frees prior owners but does NOT
decrement that source word. Do not repeat0405's simpler success-count claim.
Destructor frees final buffer,first buffer,linked records from resource2,
then resource2/resource1,and decrements count. Concrete types/consumers and
2BCD60 effects remain unresolved at this point.

Prior accepted PS2 registration listing1A66A0 supplies matching0406
constructor1A7DE0 and destructor1A8000:1A67A8's original ADDIU immediate
0x8000 is signed -32768,so1B0000-8000=1A8000,not1B8000.
Inspect their own complete original spans;do not use Xbox layouts/addresses.

source-xbox-02 PASS4.95s and source-ps2-01 PASS6.09s:the matching
constructors/destructors have the SAME two-resource/two-buffer allocation
closure and cleanup branches,including the pre-final-allocation count
increment. Xbox is easier to inspect but this alone does not establish
substantially less native implementation work. Both selected owner paths
are CPU/allocation logic,no console GPU operation is present in these spans.
Xbox2BCD60 fills two256-entry byte tables from floating operations/callees;
the exact numeric contracts still require those callees/constants/consumers.
Original first object extent is28 bytes;that alone is not a native type.
Next recover resource consumers and math dependencies,then assess actual
avoided PS2 work rather than treating shorter x86 as proof.

source-xbox-03 PASS6.07s and source-ps2-02 PASS4.79s. Consumers establish
CPU image services:2BBE30 creates width/height/depth records;2BCFC0 frees
owned pixel backing before extension cleanup and record release.2BC1D0 is
the mutable image-search path;2BBCE0 grows filename scratch.2BC590 registers
extension/read/write callbacks,2BC410 selects an extension reader and routes
through path search2BC2B0.2BBDD0 applies table1 to RGB bytes and copies alpha.
No GPU/device call is present in these recovered consumer spans.

Matching PS2 table builder1A8380 has the same two256-entry gamma/inverse-
gamma structure. Xbox normalizes with x87 multiply by a float reciprocal;
PS2 uses scalar divide by255. This is NOT proof of numeric equivalence,or a
reason to re-create EE arithmetic. Xbox helper399A4C is only a2-byte jump
to399A4E:its saved function span does not establish the math body. Recover
the real target and numeric contract rather than naming the thunk from a
decompiler. General gamma parity remains OPEN.

At this stage the concrete ownership/format/path closure still matches PS2;
no substantial platform-dependency saving is established. Native files,
content,heaps and existing decoded-texture/GPU services carry forward. A
new console-shaped image owner or copying a CRT math implementation would
not qualify as an easier native route.

source-xbox-04 PASS4.98s/source-ps2-03 PASS4.88s:2BBE90 allocates
pixel/palette backing with4-byte row alignment;depth4/8 get16/256
four-byte palette entries and STILL one byte per index pixel. It sets stride
and pixel pointer before allocation-refusal reporting;success sets owned bit1.
2BBE30 leaves stride uninitialized until this producer;do not invent it at
record birth. The only gap in2BBDD0's selected listing is skipped by its
unconditional forward jump;original bytes are saved separately for decoding.

Xbox numeric helper399A4E marshals x87 values to binary64 and calls399A70;
PS2 helper122580 tail-jumps11EA20. Neither wrapper proves a host pow ABI or
numeric parity. Gamma has real callers28990 and2BFEB0,so declaring
gamma1-only coverage must not erase the later source Open handoff.
Read these callers plus actual math/format/pixel consumers next. Native
system-art CPU decoders already own parsed textures;keep those implementations,
not a new decoder or assumed same-name Xbox asset representation.

source-xbox-05 PASS6.03s/source-ps2-04 PASS5.01s. Source2BCC40
applies table1 to each RGBA row (using stride) or indexed palette,requires
backing,refuses unsupported depths and sets flag2 after the relevant path.
2BFEB0 builds the module prefix then REBUILDS tables using actual835350
word;28990 supplies constant1.0 (289CE's immediate push),not a proven live
setting. The835350 producer/domain remains OPEN;do not guess it is variable.
The actual Xbox math body399A70 uses x87 FYL2X,control-word checks and
multiple helper/error continuations;PS2 11EA20 is a float power algorithm
with a real exponent1 early return. Full math bodies/helpers/rounding are
not yet an equivalence proof and will not be ported as a console CRT.

Xbox7D350 demonstrates actual image -> raster conversion -> image disposal
-> texture creation. Its native counterpart must join existing CPU data and
GPU owners,not report image allocation as renderer success.
PS2 1A84A0 was inspected only as a candidate and is NOT image creation:
it calls a raster callback and propagates flag2. Keep this negative finding.
Locate matching PS2 image/pixel allocation through original allocator tags
instead of guessing from function order. No runtime code changed.

Bounded original-tag search PASS:1A81C8 is the image-allocation-tag
candidate in1A8190;no semantic claim is inferred from the tag alone.
REVIEW_CORRECTIONS.md preserves the withdrawn28990 live-setting inference
and its exact contrary instruction. The new source recovery does not silently
convert hypotheses into field contracts.

source-ps2-05 PASS4.54s:matching image create1A8190..1A8233
has the SAME width/height/depth,zero flags/pixel/palette and deferred stride
contract as Xbox2BBE30. Its branch-likely1A81E0 annuls the width store on
null allocation;the nonnull case performs that store in the delay slot.
Original allocator tag0x30018 and callback arguments agree independently.
This is simpler x86 inspection,not a different/more capable native algorithm.
Recover matching disposal and the actual sole saved direct caller to finish
this bounded comparison;do not expand into a console image/CRT rewrite.

source-ps2-06 PASS4.90s:1A8240 is matching image disposal and2DF860
demonstrates a real128x128 depth8 image with borrowed backing/explicit stride.
The bounded comparison is sufficient to reject a claimed substantial Xbox-
specific0406 shortcut:the same native CPU services are needed. Source words
are easier to inspect on Xbox,but neither old CRT/math implementation is a
free native contract. Keep one shared implementation,retain existing decoders/
files/heaps/GPU services,and resolve gamma initialization/producer domain
before removing required0406 factory blockers.

RECOVERY.md consolidates authority,source spans,actual consumers/ABI/order/
errors/ownership,numeric/extension limits,independent future expected behavior
and the explicit no-material-Xbox-saving verdict. Stage remains source-derived
only;no0406 runtime/config edit or component/game execution was made.

source-ps2-07 PASS4.80s:actual1A82B0 frees backing,clears pixel/palette
and owned bit before returning to image-extension cleanup. This closes that
specific disposal-helper question,not generic plugin behavior. No further
cross-console export is needed to decide0406's bounded route comparison.
Publish only the recovered source/evidence;authenticate original mappings,
listing gaps/actual selected spans and all912 UNCHANGED runtime source
identities. Preserve prior49-name link evidence as retained/not rerun.
No source-only receipt may be labeled compiled or tested-native0406.
