# Frame-work initialization source and native scope

Original ELF SHA-256:
`a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`.
The saved private program/listing is SLUS_214.10, r5900:LE:32:default,1.4.
`source-01.original-heads.json` saves original words through actual PT_LOAD
mapping before decoding. The join receipts compare every contiguous exported
word with the original image. Decompiled output is orientation only.

## Actual source used

-2E9930..2E9934 (inclusive instruction addresses) is J2E8E10/NOP, not an absent graphics call.
-2E8E10..2E8ED0 clears two28-byte work descriptors, sets their initial3/2
 windows, source byte capacity249F0, and aligned backing addresses.
 `FRAME_DATA.json` derives the addresses/capacity from actual original words
 with signed ADDIU immediates and proves the full backing ranges are loader
 zeroes, not file-backed data.
-2E8FE0..2E906C admits one pending reservation per category, rounds requested
 data with ADDIU15/AND-16 and returns backing+used. It refuses a second pending
 reservation; source capacity and32-bit address-space restrictions are not
 native host limits.
-2E8F80..2E8FD4 clears the pending byte BEFORE checking actual committed count,
 then advances by sixteen-byte-rounded actual count when it fits.
-2E8EE0 is source window rotation. Native storage does NOT simulate that
 hardware-era ring or preserve its fixed addresses. Actual strong consumer
 leases decide when native backing can be reused.
-2E8D50 toggles a source mode byte; its wider consumer semantics are not
 implemented by this work.2E90A0 is only a saved function suffix and is not
 promoted as a full source function.

The native destination is `NativeRetailFrameWorkStorage`, called by the real
Main `initialize_retail_frame_work_buffers` binding. It acquires the SAME
existing Permanent heap through the existing renderer allocator capability.
It creates actual cold-zero native backing, uses dynamic warm pools, permits
growth beyond source byte/window counts and never indexes a guest map.

The original Main caller is122964 (`source-01/002e9930.refs.tsv`), after
first-scene/post-scene setup and before stopwatch/seed/task admission; the
existing `runtime/src/native/retail_main.cpp` preserves that order. The prior
`native-retail-main-frame-gates-20261122/SOURCE_JOIN.json` and its saved
source-domain listing retain the wider Main evidence. No complete Main run
is claimed while its other required integrations remain undefined.

The contiguous listing joins cover all2/49/36/22 words of9930/8E10/8FE0/8F80,
including every delay slot; no omitted instruction is treated as padding.
The saved PROGRAM.txt identifies the actual language version1.4; an installed
extension/package version was not separately exported. Source SLL/SUBU/ADDU
and ADDIU perform32-bit arithmetic with sign-extended results; LW sign-extends
its32-bit load, LBU zero-extends8 bits and SLT compares signed64-bit GPR
values. AND uses the GPR operands, and immediates in ADDIU are signed, unlike
ORI's zero-extended immediate. For the admitted nonnegative source extents
these yield the stated sixteen-byte alignment and comparisons. At9018 the
subtraction at901C executes on either BEQ path. BEQL at9044 executes9048
only when pending is zero; pending nonzero takes904C/9050 and returns zero.
The JAL at8E34 passes a2=28 in8E38's delay slot;8E48 advances the descriptor
on both loop-branch paths. Source commit8FA4 consumes pending before the
signed comparison at8FAC; it has no success-status return contract.

Native reservation/data APIs replace the admitted memory contract, not an
EE register ABI. Size arithmetic uses checked host extents; bogus overflowing
requests fail by name instead of wrapping to a host pointer. Source16-byte
format alignment and pending/commit effect order are preserved. This is not
full equivalence for malformed negative guest byte counts or generic EE
pointer arithmetic. No such guest API is exposed on this native route.
This implements the native nonnegative-size service, not a claim of
instruction-equivalent malformed/negative32-bit calls. Those cases cannot
be admitted through the size_t API and remain outside its stated scope.

## Ownership and independent checks

Mutable reservations require the actual Main's live owner-thread witness.
The public constructor publishes only after all actual backing/metadata
allocations succeed. A failed host allocation does not become ready storage.
One pending request per category is independent of native capacity.

Committed data owns its actual NativeAllocation-backed block. Live leases
are not overwritten or made dangling by pool growth, Main retirement or
reinitialization. Native heap revocation is checked before publishing a
borrowed span; borrowed spans must not cross an explicit native heap-reset
boundary. No GPU fence/completion or draw has been fabricated.

Expected initialization hints/cold zeroes come from source words and PT_LOAD
zero initialization. Literal17-byte/32-byte alignment, independent categories,
consumed overcommit, zero-size and foreign/thread/retired cases exercise the
verified protocol. Greater-than249F0 requests and more-than-three retained
blocks prove removal of the old host ceilings, not game acceptance.

## Still open

Clean committed runtime sourcea36ee13 passes the four affected clients in
validation-frame-clean-02. CLEAN_AUTHENTICATION.json records900 exact source
blob matches, four binary/log identities and the exact59-name diagnostic.
This is component evidence from an incremental build, not accepted gameplay.

Actual packet/draw producers, native rendering/renderer completion and
same-scenario playable-oracle comparison remain OPEN. This storage API is not
an EE-address compatibility arena. It does not supply a GS model, fallback,
software renderer or complete game loop.

2460B0 active defaults reuse valid prior helper evidence, but its remaining
published alias consumers are not fully established. The raw word scan's
52BF74 candidate has no saved containing Ghidra function; the failed metadata
probe/log is preserved and is not proof that the candidate is an actual EE
consumer. No missing nonempty-preset bug was established.

Online functionality is deferred at the owner's request. Its absent native
bindings remain named rather than falsely successful. Source12A4F0(0) is not
a no-op; its audio mode API remains unchanged in this batch.
