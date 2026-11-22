# Original sound parameters — source-only handoff

Private branch section-timing-20261122, base01fc1a60acdcb05245d945f9f5c25ad40a121380. Primary owns all audio implementation. No production code was edited. SOURCE_JOIN.json authenticates27 EE listings (3333 listed instructions, including reused duplicated views),10 Xbox listings (479 instructions), original data and three prior primary evidence files. join.py rechecks every original byte and65536/256 signed-field extraction cases. This is source-derived analysis, not native implementation, compiled testing or gameplay equivalence.

## Actual ABI and parameter flow

12C030 receives semantic ID in a0 and request volume in f12. Its only discovered direct caller is12C130, which sets f12=1.0 at12C1B4 immediately before JAL12C030. This caller census is discovered Ghidra references, not proof that indirect/external callers cannot exist.12BF10 is a separate direct12C440/315C70 caller using gain1.0 and no extra flags9.

12C030 copies56 original bytes5F7320..357 using LQ/SQ x3 then LD/SD x1. It overwrites+0 with semantic ID in12C090 call delay slot,+4 with f12 at12C084,and+1C with1 at12C088.12C440 mutates this private request and returns0/1. On success12C030 admits a real game-reference slot, then passes the following to315C70:

|Typed meaning established by this call/consumer|Request source|ABI at315C70|
|---|---|---|
|Actual loaded bank identity|+28, catalog row's live first word|a0|
|Program selector|+14, packed sound bits0..9|a1|
|Signed output field|+30, sign-extended bits20..22|a2|
|Unsigned priority|+2C, bits23..24|a3|
|Resolved gain|+4|f12|
|Control flags|+34 OR9|t0|

Do not transplant bank/return words as native tokens. Resolve the same real native AudioBankHandle through the retained native catalog/Session. Existing record ownership pairs native playback/bank identity with the semantic ID. Field names output/priority describe existing downstream contracts; this record does not prove an output endpoint mapping or an envelope curve.

Defaults are exact words: ID0,gain1,pan0,pitch1,+10=0,selector0,+18=0,+1C=1,+20=0,+24=0,bank0,priority1,output-1,flags1. Thus the actual flags sent here are9. Pan/pitch are clamped by12C440 but not forwarded by this315C70 call. No envelope default is inferred from them.

## Packed semantic data and validation

Bank index is semantic ID's high16 bits; sound-table index is low16. Original arrays are344 catalog records of40 bytes at59BC30,9656 packed words at5C0D20,and39 category records of8 bytes at5E2940. A semantic all-ones word is rejected. Out-of-range bank index is normalized to0; out-of-range sound index is independently normalized to0. These are source fallback rules, not permission to treat an unknown native bank as loaded. The selected bank's live first word must be nonzero. Initial file-backed bank handles are not invented runtime handles.

From each packed sound word, exact extraction is selector=bits0..9; percent=bits13..19 unsigned; output=bits20..22 signed three-bit; priority=bits23..24 unsigned two-bit; category=bits25..31 unsigned. Selector uses LHU/ANDI3FF. Priority uses LHU at+2,DSLL55,DSRL62. Output uses LB at+2,DSLL57,DSRA61: signed three-bit extraction, not an unsigned nibble. The unsigned percent extraction uses LW(sign-extended),DSLL44,DSRL57,leaving0..127; its BLTZL conversion alternative cannot run after this zero extension. The original<=100 admission is retained; every9656 pinned record meets it. No percent conversion at2^31 is reached.

12C440 rejects category>=39,category gain-index>=6,percent>100 or unloaded bank, returning0 after any earlier request clamps/bank publication. It returns1 only after all selector/priority/output stores and+34=1. It does not roll back partial private-request changes on failure.

All9656 original words decode to priority1. Output-1 occurs9519 times,output0 occurs137. Actual categories are0..36 but not every category occurs; all percents are0..100. The category table's second word supplies a byte gain index. All sound records reach only gain indices0..4. Category row4 selects gain5, but no pinned sound record uses category4: the sixth read-capable alias is NOT reached by this sound table. Raw catalog/sound/category data and full distributions are in SOURCE_JOIN.json.

## Float operations, comparisons and limits

Source multiplication order is12C7D4 request_volume*sound_percent_fraction,12C7F4 result*category_scalar,12C7F8 live_gain*result. Preserve separate EE operations and operand order; the decompiler reorders the expression. Percent fraction is CVT.S.W(unsigned but bounded<=100) then DIV.S by100. Source first clamps request gain to[0,1],pan[-2,2],pitch[0.5,3],then clamps the computed gain again to[0,1]. Upper clamps use C.LE.S followed by BC1TL, not reversed C.LT: preserve the actual predicate and annulled delay slots.12C804 stores the computed gain in the lower-comparison branch delay slot; an actual negative comparison then overwrites it with+0. Comparisons regard+0=-0,so negative zero can survive.

The original EE manuals are pinned in MANUAL_REFERENCES.json. UserManualp156 defines exponent1..255 as finite numbers,including the bit patterns a host would call NaN/Inf; exponent0 is signedzero. Host IEEE NaN/unordered semantics are therefore not the source policy. p164 states only rounding toward0,with arithmetic potentially differing from IEEE-toward0 in the low bit. A compiler's ordinary host float chain or x87 result is not a proved numerical replacement. Preserve known native numeric owners; exceptional/underflow/overflow parity requires independently verified bounded math if exposed, not a device interpreter. The existing initial/default request and script setting paths are finite; raw saved-preference copies perform no floating validity check in these bodies. This record does not claim every arbitrary saved word matches host NaN handling.

315C70 multiplies gain by16384 at315D34,tests2^31<=scaled_gain using C.LE.S at315D50,and either CVT.W.S directly or subtracts2^31,converts,and ORs0x80000000. InstructionManualp356 confirms CVT.W.S truncates toward0 and saturates signed limits when exponent exceeds0x9D; its introductory purpose sentence is inconsistent,so the actual opcode/Description/Operation clauses govern. On this resolved finite12C030 path gain[0,1] implies scaled[0,16384],so the unsigned split and conversion overflow are unreachable. Ordinary fractional conversion still truncates; do not round-to-nearest. Other315C70 callers are outside this scoped reachability claim.

315C70 returns0 for null bank,failed identifier allocation,or failed8-word command allocation. Command allocation failure removes/retires the newly reserved identifier before returning0. Success emits command54 with bank,selector,output,priority,scaled gain,flags,play identifier in that order.12C030 allocates its game-reference slot BEFORE this call and publishes semantic/bank/returned-play even when returned-play is0; it does not decrement the reference count on this failure. The original fixed16-reference capacity is not a native capacity limit; native owner policy remains primary's work.

## Six live gain producers and current owner mapping

The persistent owner is NativeRetailGameSession::game_records_values()->profile_preferences. First five gains are levels[0..4]. Sixth is std::bit_cast<float>(word_14),NOT a separately initialized field or integer numeric conversion. Current encode/decode copies word_14 exactly and the existing default function sets0x3F800000. The header's comment claiming only five actual float consumers is incomplete for the general resolver,although sixth is not selected by pinned records. No header/code correction is made in this evidence-only branch.

- Loader birth6B6F60..677 is zero-filled BSS,not file-backed initial floats.295660 is the actual Main122920 producer: copies88 default bytes5BE650 to current preferences,then clears64EAC4 and sets64EAC0=-1. Existing production initialize_retail_current_profile_preferences already writes this exact retained owner. First five defaults are0.75,sixth1.0.
-294D20 independently copies24 bytes via SQ16+SD8,including sixth in the JR delay slot. Its discovered caller is2F5C70 at2F6C90. It resets these six values to the same defaults; it is not just a five-float loop.
-294F10 takes signed source percentage,clamps<=0 to0,>100 to100,computes binary32(percent)/100 then adds exact0x3BA3D70A(0.005) only for original positive input. Thus0=>0,1=>approximately0.015,100 or larger=>approximately1.005. It has no index bound in its body. Actual discovered2F34B0 calls use indices0..4,with argument rewrite in each JAL delay slot,for script IDs2B5C..2B60. No discovered call writes index5.
-295380 loads88 preferences from a selected,valid saved-profile record (index0/1),then marks64EAC4=1,64EAC0=index,and invokes1531D0. It does not create a parallel sound-gain snapshot. Full saved-profile ownership remains primary's separate lane.
-154410 copies88 current preferences to a stack backup through2955C0,begins its menu/task transition,then on the2F3FE0==0 path restores all88 via2955C0 and applies225E50. The other path invokes294FC0. Generic2955C0 is memcpy88,not proof of arbitrary additional producers.
-2955E0 copies current88 INTO a writable selected profile when permitted; it is an outward persistence consumer,not a current-gain producer.2955D0 copies static defaults to a caller-provided destination; it only changes live gains when that destination is actually current preferences.
-336410 and336D30 read the first gain for movie/audio setup,not producers.294F80 is a percentage-read consumer. Original candidate1FCBE0 resolves5A6F60 and3667D8 resolves5E6F60,not6B6F60; those equal-immediate false matches are explicitly excluded.

These are bounded discovered original producer/caller proofs. Indirect callbacks/dynamic writes outside the traced settings/save paths are not exhaustively excluded. Full native setting mutation,profile load/restore and movie callers must share the same Session preference values; no copied six-gain owner is justified.

## Xbox cross-check and non-adoption

Original XBE SHA25678f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393 is mapped by its own section headers,never PS2 offsets.23BE30 copies defaults502A08,ORs9,records playback/bank/semantic ID;23BEE0 resolves packed parameters;3797D0 admits playback with flags and gain. All56 request-default bytes,all9656 packed words,and first24 preference defaults are identical to PS2. Xbox current gains live at723BA0;235BD0 and2362E0 copy six defaults. These addresses are evidence only.

Xbox categories5783C8 have the same gain indices but different scalar values in rows0,5,7,15,16,29,31,33,35,36. Xbox resolver executes x87 FMUL percent*volume,then category,then live gain,with only final FSTP tobinary32; PS2 separately rounds each MUL.S and lacks IEEE NaN semantics. Xbox3797D0's direct object/queue admission differs from PS2 command54 producer315C70. Logical parameter intent is corroborated; donor scalar/numeric/SDK/device behavior is NOT adopted.

Open: exact native numerical policy for EE multiplication/division outside already verified bounded primitives; actual settings/profile mutations and loaded-bank publication; runtime output-field meaning beyond the proved signed parameter; real envelope/control consumers and flags9 downstream behavior (owned by primary); same-scenario oracle/gameplay equivalence. No success envelope,output route,bank handle or endpoint mapping is invented.
