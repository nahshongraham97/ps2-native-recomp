# Xbox-first mode/scene implementation

Owner explicitly directs Xbox-first for ALL remaining implementation, not
only CPU work. Use the easier matching portion from either retail version
inside the existing native architecture; PS2 remains the behavioral reference.
The existing Ghidra plugins/programs are already prepared, not a setup task.

Start with Xbox mode/scene initialization corresponding to the still-required
PS2 mode2460B0 and12A4F0(0) boundaries. Reuse current PS2/source/default evidence
instead of redoing it. Original XBE identity/mapping comes from the existing
xbox-donor-20261121 records and is checked against actual bytes.
Private project: mode-scene-xbox-1c3c47b6b8d84fc5874adda06c554d02.

Stage: bounded original-data/source discovery; no donor implementation claim.
Frame sourcea36ee13 is already published and passes four clean-source clients.
No original game/user/audio files, drivers or model settings are changed.
No game, visible window or audio endpoint is launched.
Owner: no action. Agent next: identify Xbox mode-table callers and adapt the
simpler matching source into existing owners; unknown behavior stays OPEN.

Original-data finding: all nine existing PS2 sound-mode descriptor names are
present in the Xbox image, with corresponding descriptor name fields at
56C958+24h*index. The selector column begins56C950; this is not proof of
the complete descriptor's base/layout. These addresses come from
the XBE's own checked sections, not reused ELF offsets. Next inspect its real
callers and selector implementation in the existing x86 Ghidra program.

Source-mode-01 inspected the actual Xbox x86 listing:23C8A0 is the matching
bounded nine-mode selector and calls23D060 with descriptor/policy as two32-bit
cdecl arguments;185 instructions across three saved functions join original
XBE sections. Descriptor policy0 calls23D4D0 then23D830. This is source-derived
correspondence, not a new native implementation or complete policy/lifetime
proof. The mode-service boundary reaches audio catalog ownership, whose files
remain untouched; trace the startup caller next to locate mode-object setup.

The saved Xbox startup caller19C680 contains the matching sequence: current
sound23AD80, mode objects1F98D0, preferences235A40, mode selector23C8A0(0),
first-scene2330C0, then call368850 and initial task2001/20. The decompiler
labels that call as a thunk to3728C0; its actual body/terminal service is not
yet exported, so it is not accepted as verified work-storage semantics.
Its x86 listing and original-byte join are saved in source-callers-02.
Next export actual1F98D0, avoiding further PS2-only alias archaeology.
This identifies the connected donor boundary; it does not by itself prove
the downstream object fields or permit copying Xbox layout/addresses.

Actual1F98D0 is36 x86 instructions, all joined to the XBE. It uses one child
reset helper1F9BF0, writes two minus-one metadata words plus status0, and
publishes child+8 / child+240 aliases. Material platform difference: Xbox
does this for FOUR active player records; PS2's verified2460B0 does it for
TWO and publishes a different second child offset. Do not import four-player
layout or guess offset correspondence. The x86 helper/alias consumers are
the next focused source, reusing the existing native child reset/owners.

The source-child-04 original-data preflight correctly refused to read736500
as file-backed input; it is loader-zero alias storage. No Ghidra process ran
and no runtime changed. EXPORT_FAILURE_04.json preserves the failure.
The query exporter now distinguishes section-proven loader-zero queries
from original file-backed instructions; retry uses new outputsource-child-05.

Source-child-05 closes the first real consumer hop:736500 is read by105510,
which places the alias in field+8 of each12-byte player record at82ECA0.
Its caller1052B0 installs that array in the task and loads
"common/kart/kart_main". This is a Motor Kombat consumer, not proof that
the alias refers to named presets or KAF. The child helper1F9BF0 clears35E0,
sets initialized byte/name/action masks, calls188A60 then2532B0(child+278),
then fills40/30 indices with-1. PS2 child size3C30 and offsets differ.
Next inspect the Motor Kombat record consumers to recover the actual value
contract before binding it to a native field. No audio file changed.

Source-kart-06 follows the real record consumers and source character chooser;
it confirms a Motor Kombat player-selection record, but those reached native
script handlers do not directly read the profile alias payload. Its exact
payload contract remains OPEN; do not invent a56-byte host record or call
the alias a named-preset pointer. The simpler Xbox reset algorithm is identified
but NOT yet adopted. Next inspect the actual kart script/data consumer or
an equivalent verified native field, then implement/test mode-object startup.
The saved partial proof is retained without blocking use of Xbox elsewhere.

## Per-mode profile reconciliation continuation

Owner requested reconciling actual per-mode ownership and the Motor Kombat
reference, preserving PS2 save compatibility. Baselines: parent525fb5b0,
fork2baccd4, both main/clean at entry. Reuse this private Xbox project and
all valid original-byte joins. Numerical and HD-presentation gates remain
OPEN and are outside adoption in this change. Audio, originals, user saves,
the preserved dirty checkout and reserved model/settings stay untouched.

Next source boundary is the actual570A0 kart-start call and105590 continuation
from the saved1052B0 listing. Follow the12-byte selection-record payload into
its true readers/writers rather than guessing the child+240 representation.
The PS2 file codec remains a separate3C30-byte encoded format; extending
native per-mode ownership must not enlarge or reinterpret that wire image.

Source-kart-profile-07:570A0 is scene/frame association, not a profile-copy
routine. Actual105590 counts four local participants and calls105830 before
per-player reset1F9A10 for nonparticipating records, then enters the kart mode.
Next inspect those two actual callees. Do not promote the decompiler's guessed
types or treat the scene association as profile publication.

Source-kart-profile-08:105830 is a bounded frame-yield wait, not the alias
payload reader.1F9A10 explicitly selects each of four active records, releases
its saved selection when both metadata words are not-1, clears the captured
input control, resets the child, then metadata/mapping/status in that order.
Its four-player selection is source-proved; no save-layout equivalence is
inferred from that count. Follow actual race initializationE4B70/E5F80/E66C0,
already called by the savedE47B0, for the carried selection-record reference.

Source-kart-profile-09:the race allocatorE4B70 owns the actual107C-byte race
state and invokesE4E70/E5A90 before its name/UI consumerE68E0. E5F80 loads
actual kart_core/kart_fx managed resources; their calls are not replaced with
success or an interpreter. E66C0 binds split-screen camera callbacks and is
outside this profile change's numerical/HD gates. Follow the race profile
setup callees only; do not implement those rendering/resource effects here.

Source-kart-profile-10:E5A90 publishes the SAME four active child owners and
their SAME status-word addresses into race state, then derives ordered player
indices only for status==1 and fills the remaining entries with-1. Thus race
state must not own duplicate save/profile values. E4E70 is track-object setup,
not the remaining alias payload reader. Its renderer effects stay unadopted.
Next inspect original selected kart resources for the actual payload consumer;
all encoded PS2 save sizes/checksum behavior remain unchanged.

KART_RESOURCE_HEADERS.json is a read-only SEC-candidate probe, not proof of
module layout: the inspected SEC resource directories contain kart art but
did not locate the managed-module payloads. Original asset bytes are unchanged.
Use the actual XBE descriptors55AB68/55AB74 and their1B04C0 consumer rather
than assuming modules must be named SEC records. The probe's verbose stdout
is not another verification claim or a reason to repeat the asset scan.

KART_DESCRIPTORS.json now saves actual original descriptor bytes:55AB68 names
kart_core.mko and points to table4ED6C8, not directly to a file offset;
55AB74 similarly names kart_fx.mko. Source-kart-profile-11 shows1B04C0 is a
managed-record wrapper, not the byte decoder. Next read the selected table
entries and original module name/type data using each image/container's own
mapping. No interpreted execution or runtime resource adoption is authorized.

The first selected-module probe failed before extraction because it incorrectly
restricted references to.data; actual4ED6C8 is in.rdata. The saved failure is
MODULE_PREFLIGHT_FAILURE_01.json. Correct only the section selection using
the table's proved VMA; preserve the empty first private directory and use
attempt-02. No original file, runtime, save or earlier measurement changed.

KART_MODULE_SLICES.json now pins selected original table slices and privately
saved bytes. Actual kart_core has0 functions and61 variables; it is data,
not the missing executable profile reader. kart_fx has120 functions, but its
inspected names concern effects, not evidence of the profile contract.
No module interpreter was added and no original asset changed. Follow the
already-saved69000 command's207480 callee and race setupE5280 next.

Source-kart-profile-12 shows207480 is the ordinary two-player character-grid
handler, not the kart profile payload reader; E5280 is track navigation
geometry and remains outside this change. Stop expanding those unrelated
callees. Query the actual105020 selection-owner accessor's complete saved
reference list, then inspect only callers that consume its record pointer.

PROFILE_OWNER_REFS.tsv identifies twelve saved direct callers of105020;
six are already inspected. The only saved direct reference to82ECA8 is its
initial store, so absence of a reader is not yet proof the payload is unused:
inspect the remaining six owner-accessor callers and any pointer escapes.

Source-kart-profile-13 covers the remaining saved105020 callers. Their
inspected paths use character selection, readiness or art lists, not the
third profile-reference word. E5A90 gives the race whole-profile pointers
independently, so the next bounded check is direct child+240/+278 accesses
within the already identified kart/gameplay/frontend code range. Do not
claim the alias payload unused merely because the frontend does not read it.

The bounded kart displacement listing is saved in KART_OFFSET_USERS.tsv and
joined to original bytes. It does not establish a child+240 payload contract;
the common240 comparisons are object fields, not a justified profile mapping.
Do not substitute that scan for a type proof.

Implementation decision:reconcile the PROVED whole-profile ownership from
E5AD8..E5BB5 now. Keep one Session-owned set of four active nodes, expose
first-two and Motor-Kombat scopes separately, retain the Session in a race
binding and snapshot only the source status==1 index order. The binding must
resolve live values/status from those SAME nodes. Keep246070's reset limited
to its original first two; do not loop over all native storage after growth.
The separate startup child+240/+8F8 projection remains a named OPEN contract,
not a guessed field or a completed2460B0 binding. This is a directly needed
ownership dependency, not adoption of wholeE5A90, its input-ordinal prefix,
its display-query suffix, numerical code or rendering.

Implemented/tested-component:the existing NativeRetailSavedProfileRecords now owns
four zero-born active identities with explicit first-two/Motor-Kombat access.
The existing accessor and246070 reset remain first-two only. A native kart
binding retains the same Session, resolves live child/status owners and stores
only the independently derived status==1 ordering. New checks cover256 status
patterns, native identity, unchanged first-two cleanup, scope/foreign/thread/
moved errors and Main/last-owner lifetime. No codec or serialized size changed.
This does not resolve the separate startup child+240 payload or complete
mode-object initialization; numerical/HD/loaded-save/gameplay gates stay OPEN.

Build gate:use the existing configuredb/ incremental build, no cleanup or
reconfiguration. The seven selected clients are records, Session, unchanged
save-codec compatibility, aggregate/Main, profile input reset, initial reset
and Main frame gates. They use disposable fixtures or memory only, no visible
game window/GPU/audio endpoint. Verify the exact prior59-name negative without
claiming a closed mode binding. Prior code bytes remain in baseline2baccd4 and
the authenticated frame source manifest; unchanged codec/child/store hashes
are checked before the build. Commands, logs, binary/compiler/cache/source
identities are saved by verify_profile_ownership.py through the direct shell.

validation-profile-01 passed all seven affected clients, the exact unchanged
59-name negative and native symbol audit on dirty source based2baccd4.
The records client passes all256 source-derived status cases plus scope/
identity/reset/move/thread/Main/last-owner checks. Existing PS2 image checksum,
size, body-byte and malformed-image vectors pass. Codec/child/native-store
source hashes are exactly unchanged. This is NOT full mode initialization,
complete Motor Kombat, populated save import or accepted gameplay.

Actual verifier schema preflight:status is the string
PASS_NATIVE_PER_MODE_PROFILE_OWNERSHIP_COMPONENTS; clients is a list of seven
name/sha256 objects; remaining_undefined_symbols and resolved_bindings are
string lists (59 and0 respectively); unchanged_codec_sources is a path/hash
object; status_cases is integer256. Identity keys are source_commit,
source_manifest_sha256, verifier_sha256, compiler_sha256 and cache_sha256.
All missing production/numerical/HD/oracle/gameplay gates remain OPEN.

Clean committed runtime sourced0c78a08511d9bb6d852adf6ca27fd1e09b06385 now
passes the same seven clients in validation-profile-clean-02. The saved clean
authentication checks900 exact committed source blobs, all seven current
binary/log identities, unchanged codec/child/store sources and the unchanged
59-name negative. No flags/checks were relaxed; failed exploratory probes and
initial dirty-source results remain intact. This was an incremental build,
not a fresh-checkout build or complete native game run.

Ownership reconciliation is tested-component only:four native identities in
one Session, two explicit participant scopes, live retained race references,
and unchanged first-two reset/PS2 wire format. The separate startup child-field
payload is still unresolved. Whole mode/race callers, additional input/reset/
load producers, loaded-save import, oracle comparison and numerical/HD/game
acceptance remain OPEN. Owner: no action. Agent next:finish the actual startup
child-field contract and connected callers without changing numerical or
HD implementations before their gates. No background job remains after these
completed checks.

## Remaining field and real-caller continuation

Owner requested resolving the still-open startup field and connecting real
callers. Entry baselines parent67d8e6e4/fork98d5eba, tracked trees clean.
The completed owner projection is reused, not rerun or mistaken for mode
initialization. Trace original active-profile storage/getters beyond the
already-inspected kart frontend; do not repeat the broad asset/offset probes.
Audio, save codec, original/user data, reserved model, numerical and HD gates
remain unchanged. Reuse the existing private Xbox project and installed tools.

refs-profile-storage-14 finds the missing direct payload hop:actual child+240
address7316A0 is read AND written by104AE0. This is distinct from reads of
the published736500 alias. Inspect that exact function and its nearby
profile/status callers; do not infer payload semantics from the address alone.

source-kart-field-14 establishes concrete payload members:104AE0 and104B80
increment the first/second32-bit words only when the SAME active status equals1;
unsigned player bounds admit0..3 and ADD wraps32 bits.104C20 writes five
eight-byte track-result records beginning child+248 after folding track43..52
to43..47. Decompiled helper calls in the time arithmetic are NOT ABI proof;
inspect actual instructions before adopting that arithmetic. Existing native
child statistics already have counter-pair/timed-result values; prove the
PS2 offsets and caller meanings before choosing the shared field projection.

The preserved PS2241210 listing directly reads child1C/20,24/28 and8F8/8FC
as the three displayed counter pairs, then five eight-byte900+8*i results.
Its high-word presence check and packed low-byte display agree with the
existing Native child statistics model. Thus the startup Motor projection
can reference EXISTING statistics.mode_pairs[2]/timed_results, with no new
storage or raw byte record. The third PS2 startup alias at child+238 points
to the already-proved six named preset slots; names remain child+8.
Next verify actual result caller and metadata helper arguments, then connect
the real mode-object startup binding to these live native projections.

source-kart-field-16 retains the real result callerF1330 and FA500 metadata
helper. The caller passes the actual player's logical index to104AE0/104B80
on opposite result branches; first/second counter labels remain neutral.
FA500 confirms the first metadata argument selects the source save-parent
and the second selects its child. No nine-parent Xbox file layout is adopted.

Implementation plan:connect actual Main initialize_retail_mode_objects to the
existing Session owner; reset actual child values/metadata in pairs, publish
live name/Motor-statistic/preset views after the source first-pair prefix,
then publish Xbox-proved extra-player name/statistic views after their pair.
Presets remain the proved PS2 first-two projection. Field publication remains
live across value reset; failed child resets must preserve ordered prefixes
and refuse readiness. The field uses existing storage, not a new payload.

The first mode-field join preflight rejected a partial preset listing.
MODE_FIELD_JOIN_FAILURE_01.json preserves it. Prior SOURCE_JOIN explicitly
records0034DC94 as original00000000/SLL zero,zero,0; it lies after the
unconditional branch's actual delay-slot NOP and before the alternate target.
Reuse that exact omission proof and require all original full-span hashes,
not an assumption that missing instructions are padding. No runtime changed.

MODE_FIELD_CONTRACT.json now joins all original full spans (including the
explicit prior omission), derives all signed-immediate alias biases, confirms
the same16 action-mask words in both versions and records both actual Xbox
counter jump tables. The type contract is source-resolved.

Implemented/tested-component:actual Main initialize_retail_mode_objects now calls the
existing Session owner to perform verified child resets, metadata stores and
pair-ordered field publication. Live capabilities resolve the SAME name,
third counter pair/five track results and first-two six-preset arrays, never a
byte slice or second profile. Publication survives later value resets.
104AE0/104B80 counter updates use exact status==1 and unsigned32-bit wrap.
Full race result/camera/timing bodies, packed-time arithmetic, loaded imports,
numerical/HD and whole-game gates are not adopted by this change.

Authored/unbuilt tests now invoke the real Main mode-object binding, check all
three live first-pair projections and extra-player names/statistics, verify
source-localized names, no duplicate data, no unintended parent/input cleanup,
exact status/unsigned counter wrap and unchanged neighboring track results.
Allocation cuts cover delayed pair publication/partial failure; reentry,
foreign/move/thread/Main/last-owner lifetime and repeat reset are included.
Typed const-cast mutations in the test are explicit source-state fixtures on
nonconst owned values, not a loaded-save producer or runtime mutation API.

Code review added the existing owner's mutation-admission guards to counter
writes: reentrant or prior ordinary after-effect failure refuses by name.
Read-only published prefixes remain inspectable. Authored checks also cover
failure during repeat initialization: old publication is not cleared, the
failed value refuses observation, and the untouched second child remains
live. Stage remains implemented/unbuilt; no prior check was weakened.

Build gate:existing configuredb/, incremental only, no cleanup/reconfigure.
Eight affected native clients include real mode/records, child, Session,
unchanged PS2 codec, aggregate, input reset, initial reset and Main frame gates.
They use disposable fixtures/memory only, no visible window/GPU/audio endpoint.
The new verifier preserves prior manifests, requires exactly five changed
source paths and unchanged codec/store/child-body bytes, and checks the exact
expected59->58 single mode-binding difference. Numerical/HD/time/race gates
are separate; no previous verifier or required-symbol check is weakened.

validation-mode-01 completed:all eight clients pass, including the real Main
startup binding,64 allocation cuts and the exact59->58 single-binding check.
Actual status is PASS_NATIVE_MODE_STARTUP_AND_RESOLVED_FIELDS; clients is a
list of eight name/sha256 objects, resolved/remaining names are exact lists
of1/58, mode_allocation_cuts is integer64 and source_field_contract_sha256
pins the field proof. This is dirty-source component evidence, not gameplay.

The final legacy records PASS string still said the startup payload was open.
Measured test bytes/results are preserved in the gzip snapshot and
MODE_PRECOMMIT_AUTHENTICATION.json; only that reporting string is corrected,
with no changed assertion or runtime behavior. The publisher requires exact
byte replacement, then compiles/tests the clean commit before pushing.
The wait-status tasklist commands failed due host quoting; a read-only Win32
process snapshot and the completion notification established the job ended.
No game process, driver, model or setting was touched by that diagnosis.

Clean committed runtime sourceb698338b99363714a5d3ce29b7d8505d80fe753a now
passes all eight clients and64 mode allocation cuts in validation-mode-clean-02.
MODE_CLEAN_AUTHENTICATION.json authenticates900 exact committed source blobs,
eight binary/log identities, original field-contract identity and unchanged
codec/store/child-body sources. The label-only correction was compiled and
tested without changing any assertion. Full-entry negative is exactly58,
resolving only initialize_retail_mode_objects from the prior59.

The remaining field contract is now source-resolved AND component-tested:
live names, existing Motor counter/result values and first-two preset slots;
the actual production Main startup caller is connected. This is not a claim
that F1330/FA3A0 or the whole race/input/camera/mode-service path is native.
Packed-time arithmetic is unadopted. Loaded saves, numerical/HD/oracle and
whole-game acceptance remain OPEN. No game/window/speaker endpoint ran.
Owner:no action. Agent next:the actual remaining mode-service/scene/race
callers through existing owners, maintaining the required gates.
No background job remains active after the completed clean verification.
