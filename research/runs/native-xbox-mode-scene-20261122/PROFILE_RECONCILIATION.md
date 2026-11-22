# Per-mode active-profile ownership

This records the prior ownership-only checkpoint. The later field resolution
is source-proved in MODE_FIELD_CONTRACT.json; its production startup/field
implementation passes initial component checks, as recorded in
MODE_FIELD_IMPLEMENTATION.md. Clean sourceb698338 passes eight affected
clients; this older record's d0c78a0 measurements remain preserved separately.

## Original input and actual decoding

Xbox default.xbe SHA-256:
`78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393`.
The existing private Ghidra program is `x86:LE:32:default`.
`source-kart-profile-10/PROGRAM.txt`, its saved81-instruction E5A90 listing,
and `source-kart-profile-10.join.json` identify the actual decoding used.
Every saved instruction is joined through this XBE's own file-backed section,
not ELF offsets. `PROFILE_OWNERSHIP_DATA.json` saves the original projection
bytes, actual section/file mapping and supporting source hashes.

The implemented projection is **E5AD8..E5BB5 inclusive bytes**, not whole
E5A90. It publishes references to four active children and the same four
status words, scans players0..3, appends only status==1, then pads with-1.
The actual call chain is Xbox19C680->1F98D0 startup and
E47B0->E4B70->E5A90 race setup; earlier saved listings/joins cover those
callers. Existing E68E0 subsequently reads the same child/status references.

Four complete original metadata/child ranges are loader-zero storage, not
file bytes; the separate four player-status initial words are file-backed8.
The data receipt establishes both distinctions. Native owner births preserve
them; this change does NOT implement the later1F98D0 child resets or aliases.

## Established behavior and native mapping

- Original E5ADB..E5B36 stores four32-bit image addresses. These become one
  retained actual `NativeRetailGameSession`, never numeric pointer tokens.
  `NativeRetailMotorKombatProfiles::profile` resolves the same stable node
  in `NativeRetailSavedProfileRecords`, including its live child/status.
- Original E5B6E is `CMP dword ptr [EAX],1`, followed by JZ. Equality uses the
  full32 bits:0,2,FFFFFFFF and80000001 do not match. E5B7E preserves ascending
  local-player order. E5BA9 writes full32-bitFFFFFFFF as the unused sentinel.
  The native result is a fixed game-participant index snapshot with int32-1,
  not a second copy of status or a host resource limit.
- The source loops are bounded0..3; no native overflow, floating-point, SSE,
  EE rounding or graphics policy is introduced. X86 has no branch delay slots.
- The original routine has no parameter/result error ABI. The new native
  boundary explicitly rejects wrong owner thread, foreign/missing Session,
  invalid participant scope/index and a moved capability. It returns no
  successful stand-in for missing payload or service behavior.
- A binding is a scene/race-owned value OUTSIDE Session. It retains Session;
  Session owns the nodes. There is no ownership cycle. Main retirement does
  not retire these persistent game globals; the final binding release can
  release Session. Access still requires the original owner thread.

Runtime paths:
`runtime/include/mka/retail_saved_profile_records.hpp` and
`runtime/src/native/retail_saved_profile_records.cpp`.
These modify the existing production owner used by Session creation and
171B10/246070. No new Session, card owner or shadow active profile is created.

The old `active_player` API remains first-two. New explicit participant scopes
admit first-two or the source four-player Motor Kombat set. Most importantly,
the connected original246070 reset now iterates its **source two players**,
not the enlarged native storage. Its existing source evidence is reused from
`native-retail-saved-profile-records-20261122/` and
`parallel-saved-profile-child-20261122/INLINE_EQUIVALENCE.json`.

## PS2 save compatibility

The existing external codec remains unchanged:3C30-byte children, eight
children per1E1E0-byte image, existing checksum and footer rules. The two
serialized card parents remain separate from the four native active game
identities. Xbox35E0 memory extents never become PS2 file extents.

The verifier authenticates unchanged codec/child/native-store source hashes
against the prior clean frame manifest and runs the existing independent
fixed-vector profile-image checks. User saves and the preserved oracle are
not written. Populated profile import is still required and is not claimed
from codec or ownership checks.

## Independent expectations and scope

The records client derives256 status patterns from four literal source cases
(0,1,80000001,FFFFFFFF), with only the case corresponding to literal1
contributing an index. Additional checks verify exact stable identity,
first-two reset preservation of players2/3, unchanged serialized constants,
foreign/scope/thread/move errors, allocation-free binding, live status changes,
call-time ordering, Main retirement and final Session lifetime.

Current stage: **compiled/tested component on clean committed source**
`d0c78a08511d9bb6d852adf6ca27fd1e09b06385`.
`validation-profile-clean-02/VERIFICATION.json` and
`PROFILE_CLEAN_AUTHENTICATION.json` authenticate900 exact committed source
blobs, seven passing binary/log identities and the unchanged59-name negative.
The records client passes all256 status patterns and its owner/reset/error/
lifetime checks; existing codec fixed vectors pass. The initial dirty-source
check and failed exploratory probes are preserved. This is an incremental
build/check, not a fresh-checkout build or game run. No visible window, GPU
or speaker endpoint is used.

## Explicitly OPEN

- The startup field projection was OPEN at this checkpoint. Subsequent direct
  Xbox readers/writers and the preserved PS2 statistics/preset readers now
  resolve it to existing typed data; compilation/behavioral verification of
  that production connection is pending, not a full-game acceptance claim.
- Whole E5A90's input-ordinal prefix and display-query suffix; actual race
  creation/callers are not implemented by this projection.
- The full mode-object initialization binding, extra-player input/reset/load
  producers, populated profile import and complete Motor Kombat.
- Same-scenario playable-oracle comparison and full-game acceptance.
- Numerical and HD-presentation gates. No affected mathematical/rendering
  implementation is adopted or accepted by this ownership change.
