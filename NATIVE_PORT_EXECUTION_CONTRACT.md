# Retail Armageddon native-port execution contract

**Current playable R192 route:** The parent launchers/launch_r192_hooks.bat (also called by launch_ps2recomp.bat) calls the unchanged original external/forks/ps2-native-recomp/build/r192-hooks/out/launch_r192_hooks.bat and ps2run.exe (SHA-256 8ab43bda05e7a621b946883f3195a583f0c2455f0351bf8608282cd3e97b4617). Its linked base AOT code and all3569 precompiled EE cache DLLs remain available; every cached DLL was verified byte-identical to the preserved compiled package. EE/VU Live remain enabled for misses. The SAME original persistent card and quick-save directory in build/r192-hooks/game/disc are retained, with no protected card/save/executable/original-launcher modification; verification runs use disposable headless copies. The separate compiled882302... package is preserved but is no longer the default route. AOT closure remains frozen, OPEN and not required for the native port. Native Phase6 continues on main in both existing repositories under the latest owner direction; admission/credit remain2 transformed/238 native until the existing gates pass. Final native-only acceptance remains OPEN.

**Capture and replay claim gate:** A SHA-valid PS2CAP01 file may still end at a scheduler suspension. Compare exitPC to savedRA and all SP lanes before calling it completed; authenticate the original caller/code identity and relevant effects separately. Requested recording length is not observed duration. Report endpoint completion and checkpoint parity separately. Normalized typed-state/lifetime composition, excluded dead-stack bytes and explicit unbound services must never be reported as complete CPU/RAM equivalence or native admission.

This governs the **retail game** in `external/forks/ps2-native-recomp/`.
It describes acceptance, not current success. No agent may call a build
accepted or ready for the owner to run until every gate passes on one clean,
pinned binary. The authored replacement application is outside scope.

The Phase 5 generated-unit prototype must consume actual build-generated C++
and lower its verified control/data flow to typed owners and a recovered native
ABI. A hand-written equivalent, a direct ELF-word compiler, matching source
labels or renamed memory helpers do not establish that transformation. Bind
fields from original inputs and callers, reject unbound memory/indirect effects
by name, and audit semantic structure so renaming an accessor or RAM array
cannot conceal a guest-memory shim. Credit only authenticated generated
functions with compiled differential evidence; keep prototype and production
integration status separate.

The owner's Phase 6 instruction separates two intermediate transformations.
The latest owner direction places this native work on main in both existing
repositories, with disposable experimental outputs. Stage A replaces the register ABI with
proved typed C++ parameters and returns; typed guest-memory accessors may remain
in that intermediate experiment. Stage B replaces those accesses with native
structures only after layout, aliasing and lifetime proof. Keep the AOT-only
reference and an explicit per-function comparison switch. Stage A is not final
native-memory acceptance and does not relax ownership, equivalence or credit
rules. Capability-cohort results earn no transformed or native credit.

The original Phase6 gate requires authenticated game-state comparisons plus constructed cases. Constructed-only passes, state-seeded invented calls and a zero-match entry inspection do not discharge it. The2026-11-26 review supersedes the prior N5 interpretation. Continue only the owner-requested existing return/refusal/camera slice without promoting that evidence to original-criterion equivalence. All work, commits and pushes go directly to main. The only permitted side branch is the existing frozen r192-aot-closure-proof in the fork; no other branch is to be created, restored, used or committed on. The owner explicitly directed retirement of the merged Phase6 refs.

Phase 5S.9 measures a seeded leaf cohort from the full in-scope catalogue
separately from ownership-admitted functions. Preserve failures and report
whole-function passes by size and instruction mix. The owner's2026-11-26 Phase6 N1–N5 authorizes one bounded memory/ABI/control attempt from that0/30 result; the initial isolated-branch work is now integrated into main under the latest owner direction. Measure the initial memory stage on an independent held-out cohort before enabling later control stages; subsequent stages require a measured held-out whole-function pass. If N1–N3 still yield zero, stop and report whether to abandon the general transformer. Keep the camera as an independent native subsystem check; diagnostic changes at the same instruction are not passes.

The product is one genuine native Armageddon port in the existing repositories,
not preservation of a PS2 runtime because of an upstream/fork name. Recover
the best/easiest verified retail implementations into ONE native architecture.
Neither console's ABI, memory layout, hardware limits or save encoding is
mandatory. Native persistent save/load must work; legacy PS2 save import/byte
compatibility is optional. An existing native codec may remain when easiest;
no new format is required merely for branding. No new repository/upstream PR
is requested; the owner may later curate generally reusable improvements.

**Compiler matching experiment gate:** The owner-authorized compiler experiment is separate from port/admission work and awards no ownership, transformed or native credit. Keep the fixed nontrivial denominator and tiny controls separate; retain draft/compiler/relocation refusals. Exact matching requires original-identity/range proof, complete untrimmed emitted function bytes and supported original-address relocations, with malformed-object and planted-byte controls. Normalized instruction scores, section/idiom heuristics or a backend version string do not establish equivalence or a unique toolchain. Undefined external addresses/mainGP may be mechanically joined; unproved local/overlay placement remains refused. A matched PS2 body does not prove portableCdefined behavior, native64bit pointers/ABI, data ownership or production integration. All existing native admission and ownership rules remain unchanged. Mechanical drafts plus a short permuter run do not test iterative source refinement. Keep any original-toolchain byte match separate from a proved native memory/64-bit ABI conversion. Report union-across-toolchains and single-configuration rates separately. A matching-source pilot does not admit pointer width, source-level defined behavior, original type declarations, ownership, callees or production integration. Preserve the untransformed AOT reference and require the existing native gates.

## Normal startup requires no flags

The accepted game starts with zero command-line arguments. Native execution,
asset discovery, window creation, graphics, sound, physical input, saves,
timing and required service registration are inherent behavior. They must not
depend on native-enabling flags, environment variables, experimental sampling
overrides, a disc image, a wrapper injecting settings, or a fallback backend.
Loose assets use their existing filenames in the native content folders. The accepted route must not require or fall back to MKDA.PAK. Offline extraction may prepare byte-identical files from a preserved original archive; the archive is an input to that tool, never the runtime asset provider.

Missing content, dependencies or native implementations produce named errors.
They never select an old build or emulated service.

Acceptance captures must use `build.runtime_flags: []` and candidate
`run.flags: []`. Record the actual environment and source defaults in the
configuration/source reviews: an empty argument list alone cannot prove that
hidden environment overrides were unnecessary. Inventory must include
`startup_no_flags` and verify packaged executable/content/dependency discovery
from an unrelated working directory. Optional diagnostics have separate
supplemental records and cannot substitute for the default-path captures.
Build-time compiler/code-generation inputs are distinct from startup flags.
Historical milestone launchers remain explicit, pinned comparison tools.
The normal launcher may bind only a fully accepted current release; until
one exists it must report acceptance open, never pick an unverified binary.

## Preserve the retail game; replace its platform implementation

**No PS2 resource limit may constrain the native build.** Native game
allocations, audio clips/streams/voices, rendering resources, executable
coverage, asset reads, and save storage use host-managed resources and host
capacity. Remove fixed PS2 RAM/scratchpad maps, 2 MiB sound RAM, two-core
24-voice allocation, VU memory/program windows, CD sectors, and raw-card
capacity ceilings from accepted paths. Moving those arrays to the host,
increasing their constants, or giving chip registers field names fails the
gate. A required GPU/Windows format limit must be identified as a host limit.
The port must use standard graphics APIs and queried capabilities without
requiring a particular GPU vendor. Agents must never install, remove, update,
roll back, or reconfigure GPU drivers or their update settings. An unsupported
host capability must produce a named error; driver modification is not a fix.
Retail combat rules, animation/event timing, and deliberately specified game
behavior remain the reference; hardware capacities and chip clocks do not.

The task does not authorize a gameplay redesign or a replacement authored
engine. Preserve retail movement, collision, combat rules, move timing,
animation, camera, AI, RNG use, mode scripts, menus, and save semantics.
Native replacements must preserve observable results and callback ordering.
Do not replace known retail behavior with an invented approximation.

Compiled retail algorithms may remain, including ahead-of-time VU math and
asset decoding. Removing the fixed guest address map requires transforming
memory access and call contracts; it does not by itself require manually
rewriting every function into a new C++ class. Native handles may represent
serialized IDs and file offsets. Host allocations alone do not prove that a
retained PS2 address/device model has been removed.

Do not require WAV/OGG conversion, JSON profiles, glTF models, a new physics
engine, or direct model rendering as prerequisites without source evidence.
Existing sample codecs and retail save payloads can be decoded/stored by
native services. Graphics packets may be decoded into native GPU resources
and pipeline parameters; this does not authorize a simulated GS device.
The owner authorized feeding loose art into the native GPU pipeline. Decode the verified SEC/resource, geometry and texture representation into native buffers/textures and preserve the actual retail transforms, material/topology, animation and draw behavior. A filename or extracted container alone is not proof of an uploadable uncompressed format. This route must remove the GS device/raster/VRAM model, not reproduce it under a native name.

Preserve audio identity, envelopes, effects, looping, timing, and video sync
while replacing chip control and emulated transport. A simpler native mixer
is not acceptable if it changes the sound.

Unverified schedule estimates, guessed function counts, and format
assumptions are not implementation evidence. Verify each required field and
call site against the assets/source; do not schedule or replace behavior on
those estimates.

## Verified reuse across retail platforms

The native Windows service architecture and verified PS2 work are retained.
The Xbox image supports read-only library identification and measured PS2/Xbox capability comparisons. The owner decides the route. Xbox-derived algorithms are eligible only for renderer, viewport and HD presentation after comparison. GS packet decoding remains interim. Pin both images and establish each function’s addresses, ABI, inputs, outputs, effects, errors, timing, asset identity and consumers. Record the measured dependencies and native services each route requires. The 0406 CPU-image comparison found no substantial Xbox-specific dependency reduction.

Any donor function match with `heuristic` confidence in `research/ledger/ps2_xbox_map.csv`
is strictly barred from adoption until promoted to `exact` or `structural`
through instruction CFG and oracle verification.

## Owner Decision: Online Features and Network Services Out of Scope

Online and network functionality (including the `NETR.ovl` dynamic overlay, online initialization and `NetRuntime`/`NetLobby`/`StatsBackend` classes in the main ELF, and `XONLINE` on Xbox) is formally classified as **Out of Scope (Stubbed / Retail Offline Route)** by explicit owner decision (`research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json`). Retail online services (GameSpy, PS2 DNAS, Xbox Live servers) were permanently decommissioned over 15 years ago.

**Stubbing and Non-Regression Contract**:
1. **Adopt the Retail Offline Path**: On physical hardware without a network adapter, retail Armageddon naturally routed through a verified offline fallback. The native port must strictly adopt this offline path (`read_retail_online_boot_word` returns 0; `initialize_retail_online_stage` initializes offline state).
2. **Never Fabricate Success**: Synthetic network success or silent no-ops are strictly forbidden. Any online invocation that cannot function offline must fail by name (e.g. `RetailStartupGate::source_refused` or `online_unavailable`).
3. **Overlay Manifest Exclusion**: `NETR.ovl` (1,834 functions) is dropped from `recomp/overlays/OVERLAY-MAP-MANIFEST.json` and must never be loaded on the accepted native route.
4. **UI Behavior**: Menu entries for online/network features must match the retail PS2-without-network experience (disabled or inaccessible).
5. **Ledger Accounting**: All excluded online functions are tracked with status `out-of-scope-stubbed` citing `research/decisions/DECISION_ONLINE_OUT_OF_SCOPE.json`. Acceptance checkers skip online scenarios only when this decision file is authenticated. Acceptance frames must originate from external capture paths (OS display compositor, window manager surface capture, or external GPU capture), never in-process OpenGL readbacks (`diagnostic readbacks = 0`).

## Non-regression rule for the entire retail game

**No feature may regress while a PS2 implementation is replaced.** A native
replacement that boots but loses a profile, plays a different effect, drops a
portrait, ignores a controller, skips a movie, changes timing, or breaks a
mode is a failed change. Reaching a later screen never excuses an earlier
loss. Restore the last verified behavior or fix the regression before stacking
another subsystem change. A previous playable build is an oracle for behavior,
not proof that its architecture satisfies this contract. The authored app does not establish retail behavior. The restored R192-lineage package (`external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe`, `8AB43BDA...`, `research/runs/r192-hooks-playable-run-20261125/RUN.md` with Konquest and Motor Kombat verified and persistent slot-1 memory card `game/disc/armageddon.ps2`) remains the verified original gameplay reference; historical `../../milestone_work_verified_playable_20261110/` (`B9EEF7DA...`) and `../../milestone_build_kombat_practice/` (`8B763DCC...`) remain preserved on disk. None of these recomp builds satisfies final zero-emulation native-port acceptance.

For **every** change, capture the same action on the pinned behavioral oracle
and the candidate, and compare the request/response trace, visible output,
audio, state changes, and errors. Record the two binary hashes, commits,
asset hashes, configuration, input sequence, timestamps, and comparison
result. If the oracle lacks a feature, use retail evidence; if neither exists,
mark that behavior **unverified and blocking**. Never guess a result or call a
missing recording a pass. No diagnostic flag, scripted input, or save copy
may silently change between the two runs.

The regression inventory includes **all reached behavior**, not a chosen
sample: startup and exit; legal screens, menu navigation, options, screen
position, profiles, native saves, storage availability/space/errors,
load and overwrite, restart and recovery; keyboard and controller press/release,
analog and remapping; logos, every intro/movie/cutscene and A/V sync; text,
portraits, models, animations, stages, HUD, effects, blending and frame
position; music, announcer, voice, combat/stage/menu effects, volume, pan,
pitch and loop identity; character select and every fighter/stage; combat
inputs, moves, damage, rounds, results and progression; Konquest, Motor
Kombat, Kreate-A-Fighter, Krypt, training/practice, and other retail modes;
file errors, save errors, suspend/resume/focus, repeated entry/exit, and
state reset. Expand this inventory whenever source or a run reveals another
surface. A mode with no oracle/complete observation remains open.

## Exact meanings

- Native execution means retail logic compiled ahead of time to host C++ whose
  platform operations end in native file, input, audio, video, timing,
  threading, memory, and GPU services.
- Native game memory means host-managed objects and buffers addressed with
  native pointers or verified native handles. A fixed 32 MiB `g_ram`, IOP RAM,
  PS2 address masking/mirroring, scratchpad mapping, or MMIO dispatch on the
  accepted route is a retained PS2 memory model. Allocating that array in a
  Windows process does not make the architecture native. Generated retail C++
  may stay, but its memory accesses must be transformed before acceptance.
- Retail file keys, graphics packets, and RPC payloads may be decoded as
  compatibility **data**. A simulated bus, device register file, scheduler,
  software rasterizer, chip mixer, sector reader, or executing guest module
  behind that decoder is forbidden, regardless of its name or counter label.
- A source implementation, clean build, synthetic test, zero census counter,
  visible frame, and complete gameplay are distinct evidence levels. None
  substitutes for the others.
- A status label (`exact`, `ported`, `implemented-via-donor`) is assigned by
  code from stored measurements. If the measurements contradict the label,
  the ledger build fails. Labels are never typed by hand.
- Editing a test label, comment, or evidence string to satisfy a validator is
  a failed change. Show the test exercises the function, or change the status.
- A validator must test correctness, not existence, and ships with a planted
  failure that proves it can fail.

## Required implementation sequence

Current coding priority is the connected route in ROADMAP.md: retail entry, real native initialization, main/frame control, physical input, rendering and sound. Link-only wrappers, empty effects and component tests do not satisfy missing production behavior. Mode changes must preserve original nonlocal main reentry without guest register/stack state. ROADMAP.md gives the phase order. The dated connected-entry record owns implementation evidence; whole-game acceptance requirements below are unchanged.

1. **Pin provenance and preserve data.** Record parent/fork HEAD and dirty
   state, compiler, binary SHA-256, launch flags, and extracted asset manifest.
   Preserve the dated backup and save/state files. Identify and hash the protected original playable recomp reference (`external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe`, `8AB43BDA...`, persistent slot-1 card `game/disc/armageddon.ps2` holding user saves + Konquest checkpoint) as well as the saved-state and parity oracle (`milestone_work_verified_playable_20261110/`, `B9EEF7DA...`) and Kombat Practice (`8B763DCC...`). Never overwrite persistent memory cards, checkpoints, saves, keys, or
   local model/settings. A `dirty` binary cannot be accepted or credited with
   another binary's log.
2. **Enumerate retail behavior offline.** Inventory ELF functions and indirect
   entries, overlays, copied code, VU0/VU1 programs, IOP imports/RPCs, file
   keys, sound banks, movies, saves, and modes. For each boundary record the
   actual caller, request/return layout, side effects, error behavior,
   lifetime, and evidence path. An unidentified entry is a blocker. The owner
   must not play modes to discover missing code.
3. **Lift memory and ABI dependencies.** Inventory every user of `g_ram`,
   `g_spr`, `resolve()`, `physical()`, guest register state, and DMA-like copies,
   including generated code. Move game objects and buffers to native host
   allocation and verified native pointer/handle contracts. Preserve observed
   aliasing and object lifetime. Remove fixed PS2 address-space limits,
   mirrored regions, and MMIO dispatch from the accepted build. A renamed
   guest-memory accessor does not pass this gate.
4. **Finish coverage of selected native game logic.** Inventory the code,
   scripts and assets needed by the accepted route. Implement retained logic
   and selected replacements before launch; a routine replaced by verified
   native Xbox/host code does not also require a PS2 implementation. Remove runtime
   compilation and instruction/microcode interpreters from the accepted path.
   Unknown code or operations must fail closed, never become no-ops.
5. **Replace file and save services.** Map observed retail resource keys to
   extracted files with checked path normalization and exact read/seek/error
   behavior. Remove CDVD/CDVDFSV/`cdvdman` endpoints, sector I/O, ISO
   dependency, and switches selecting a legacy reader. Define a native storage
   contract preserving complete native game state, atomic writes and correct
   host error/space behavior. Choose the simplest suitable native encoding;
   existing codecs can remain but console save-byte identity is not a gate.
   Verify profile discovery, enumeration, create, load, overwrite, delete,
   restart and interrupted-write recovery with disposable native data.
   The actual game/profile UI must recognize the right persisted state.
   Legacy card images/import are optional input adapters, not a simulated
   device or a mandatory release feature. Never overwrite original user saves.
6. **Replace IOP/SIF at the call boundary.** Implement every reached import
   and RPC in native C++ with observed responses and callback order. Decode
   any surviving RPC payload once into a typed host request. No guest IOP
   instructions, resident IRX, guest RPC server, SIF hardware queue, or
   synthetic success reply remains. Unknown methods fail by name.
7. **Verify physical input end to end.** Trace SDL keyboard and controllers
   through the native service to the retail input event and actual menu/combat
   action. Check press/release, analog, both ports, focus, and remapping on the
   same binary. Scripted `--press` only verifies automation, not hardware.
8. **Replace audio with proven sound identities.** Decode bank/handle lifetime,
   sound index, sample records, rate, loop, sequencing, gain, and pan from
   retail requests and asset bytes. Compare selected PCM and heard effects
   with reference menu, combat, stage, Konquest, and Motor Kombat events.
   Remove bank-0 fallbacks and positional-ID guesses; unknown IDs fail by
   name. Use a native host mixer, not an SPU2 register/chip model. ADPCM
   decoding as an asset codec is permitted. For each exercised sound event,
   record the retail request, resolved bank and sound/sample identity,
   expected asset hash, produced PCM hash or bounded waveform comparison,
   timing, and audible result. Verify menu, announcer, character, combat,
   stage, movie, Konquest, and Motor Kombat sounds separately. A nonzero
   waveform, voice count, or familiar-sounding substitute is not a pass.
9. **Replace video and GS.** Decode movies into host video frames with verified
   timing/audio sync. Convert all reached graphics packets and transfers into
   GPU-owned textures, framebuffers, and draws. The GPU is source of truth:
   remove host GS VRAM, software raster fallback, skipped transfers, and
   unimplemented reachable blend/test/depth/CLUT states. Compare same-time
   source and presented frames across legal, menu, character select, movie,
   arena, and combat scenes.
10. **Verify the full retail route.** One clean binary must cover boot, menus,
    saves/reload, movies, physical controls, correct audible sounds, character
    select, complete combat, Konquest, and Motor Kombat. Record complete logs,
    visible frames, audio, exit conditions, and forbidden-path census. A short
    splash-screen run cannot certify the product. Do not ask the owner to
    launch until agent verification closes all known defects.
11. **Run the non-regression matrix after each subsystem replacement.** Keep
    the previous verified build and its evidence immutable. Run matching
    scenarios on a disposable copy of its data and on the candidate. Compare
    frames at matched game states, audio events/PCM, input-to-action, save
    contents, transitions, timing, and error returns. Record each difference,
    root cause, correction, and rerun. A source review or unit test cannot
    waive a failed same-scenario check. If a comparison cannot be performed,
    mark the gate open; do not call the new build better or ready.

## Source-level work map (active fork)

This map names the current seams to replace and the proof required to close
each one. Read actual call sites before editing; the file name alone does not
prove a service is reached. Record additional call sites as they are found.

| Boundary to remove or replace | Start at these source files | Closure evidence |
| --- | --- | --- |
| Fixed guest address space and register ABI | `runtime/src/memory.cpp`, `runtime/include/ps2/memory.hpp`, `runtime/src/dispatch.cpp`, generated retail units | Native allocation/handle trace for every reached object, no fixed guest map, alias/lifetime parity |
| EE and VU instruction fallback | `runtime/src/dispatch.cpp`, `runtime/src/vu_micro.cpp`, `runtime/src/vu_jit.cpp`, VU code generator and linked outputs | Offline image/program inventory equals linked coverage; no interpreter or runtime compiler reachable |
| GS/VRAM and presentation | `runtime/src/gs/gs.cpp`, `runtime/src/gs/gif.cpp`, `runtime/src/gs/native_renderer.cpp`, `runtime/src/gs/native_gl_backend.cpp` | Every reached packet/transfer produces correct GPU resources and matching presented frames; no host VRAM or CPU raster path |
| IOP, SIF, and service imports | `runtime/src/iop/kernel.cpp`, `runtime/src/iop/sif.cpp`, `runtime/src/iop/aot_import_registry.hpp`, `scripts/iop_reference_codegen.py` | Every reached import/RPC resolves to a native typed service; no guest module execution, hardware queue, or fabricated success |
| Files and saves | `runtime/src/native/content_vfs.cpp`, `runtime/src/disc.cpp`, `runtime/src/iop/cdvd_native.cpp`, `runtime/src/iop/mcserv.cpp`, `runtime/src/iop/sio2.cpp` | Exact extracted-file bytes and errors; no ISO/sector path; native profile UI persistence, atomic save/reload and host errors; legacy card import optional |
| Input | `runtime/src/iop/padman.cpp`, `runtime/src/iop/sio2.cpp`, `host/main.cpp` | Physical keyboard and gamepad event reaches correct menu/combat action in both ports |
| Sound | `runtime/src/iop/msl_native.cpp`, `runtime/src/iop/audio_service.cpp`, sound bank decoder and host output | Exact event→bank→sample mapping and audible PCM parity; no bank-0 guess or chip mixer |
| Movies | `runtime/src/media/mpeg2.cpp`, `runtime/src/media/mpeg2_decoder.cpp`, presentation and audio paths | All intro/cutscene frames visible and synchronized with their correct audio |
| Acceptance and packaging | `runtime/src/compliance_census.cpp`, run records, release script/checker | Clean matching binary/manifest and complete regression matrix; any missing/forbidden path fails |

For each boundary, perform the work in this order: (1) cite the retail
caller and byte/field evidence, (2) specify the typed native request and
result including errors, (3) implement with no fallback, (4) test both normal
and error/lifetime cases, (5) run the same-scenario behavioral comparison,
(6) remove the old reachable path, (7) run the forbidden-route audit, and
(8) update the evidence manifest. If a caller, format, or expected effect is
unknown, stop that boundary as **blocked by missing evidence**; do not invent
its behavior or substitute another sound/file/frame.

## Mandatory gate for each change and release candidate

Before admitting saved-state evidence, establish its exact producer/version,
serialized ABI, section order, variable-length fields and checked extents.
The version-8 VU/IPU/GS regression is mandatory: an intervening IPU section
invalidates the old backwards-from-GS extraction. Validate all required bounds
before restoring any state. A valid program key or successful generated
execution cannot authenticate bytes taken from the wrong source region.
Capture provenance must bind original-file hash, exact region, extracted-byte
hash and entry point. Keep generated coverage, restored-field coverage,
synthetic behavior and independently compared register/memory/packet behavior
as separate claims. A missing comparison leaves equivalence OPEN.

At phase publication, authenticate the measured source manifest as well as
the binary hashes. Any later compiled-source change needs its own affected
verification; record unchanged components as reused evidence with that scope.
Regenerate and synchronize the ledgers before publishing. Parent and fork
ledger files, chart and current progress documents must match byte for byte,
including measured-source commit pins and LF bytes. Record a later
documentation checkpoint separately instead of changing only one copy's pin.
Push the verified phase checkpoint before advancing to another phase.

| Evidence | Reject if |
| --- | --- |
| Exact retail call/asset bytes and source or disassembly | Method, bank, ID, format, or callback inferred |
| Source diff and named native destination | Emulator renamed, no-op, or guest fallback |
| Clean commit, compiler, flags, binary hash | Dirty stamp or mismatched log/package |
| Success, error, lifetime, and repeat-call checks | Only one happy-path test |
| Same-build full log, final frame/exit, frames, audio | Old log, script-only input, counters alone |
| Physical input and heard sound check | Host button events or nonzero samples alone |
| Forbidden-route census **and source-path audit** | Zero counter but reachable uncounted device path |
| Behavioral oracle and candidate on matching actions | Any known regression or missing comparison |
| Native profile UI, complete-state save/reload, disposable save directory | Only a success status from the service |
| Retail event to exact sound asset to emitted PCM and listening check | Wrong/random effect or nonzero audio only |
| Real controls and all-mode progress | Script-only `--press` or one mode only |

Each claim links every applicable row in `research/runs/`. Independent review
checks the current code and binary. A missing row leaves status **open**. Fix
the implementation and rerun the gate; do not weaken the checker.

## Known open evidence at this revision

- Generated AOT still retains guest registers, fixed guest memory and ABI.
  Static generation is distinct from the required native transformation.
- Callable coverage is measured from nonzero entry maps and exact switch
  targets. A unit's address range cannot establish translated function coverage.
- Required-effect placeholders remain registered. A successful diagnostic
  link or bounded headless probe does not establish their retail behavior.
- Paired function-boundary register/memory deltas and ordered VU output packet
  comparisons across the oracle states remain OPEN. Existing synthetic cases
  and code-identity checks have narrower scope.
- Continuous movie playback, synchronized sound, real input-to-action,
  connected scene rendering, complete native saves and whole-game parity
  remain OPEN. Historical device/census logs do not close these current gates.
- R192 supplies gameplay and telemetry evidence. The November 10 milestone supplies saved-state and hash parity. Kombat Practice is the historical snapshot. Owner direction on 2026-11-26 keeps the original R192 as the playable oracle with live translation; universal oracle closure is not a prerequisite for the native port. The builds share measured hook lineage. Their distinct
  executable hashes and hook configuration must accompany comparisons.
  Existing oracle files are immutable; only recorded Phase 0 additions are
  authorized. The old restored launcher has a retired path and is historical.

This list is not exhaustive. Missing comparison evidence remains OPEN.

## Enforcement in the repository

Every delivery report must be generated from repository state (counts from
the ledgers, SHAs from `git rev-parse`, links to the correct repository: fork
commits to ps2-native-recomp, parent commits to armageddon-reimplementation).
It must also list: claims not verified, contradictions with earlier
documents, and the result of every investigation run, including negative ones.

An instruction file cannot technically prevent a future agent from making a
false claim. The acceptance checker and release packaging must therefore
reject a candidate unless its machine-readable manifest names the clean
commit and binary hash, every regression-matrix scenario, source and
candidate evidence paths, exact sound and card checks, forbidden-path source
audit, and independent reviewer result. Make missing, stale, dirty, skipped,
or mismatched evidence a nonzero checker exit. Do not allow a diagnostic flag
to change this requirement. Until this checker and its complete evidence
exist, the release state is **open**, even if individual components improve.
`tools/acceptance/check_acceptance.py` now validates a pinned acceptance
manifest, strict evidence identities/coverage and independent review records.
`tools/acceptance/package_release.py` gates isolated release staging on it.
Its synthetic tests verify checker structure, not the truth of gameplay or
reviewer assertions. No real candidate manifest passes at this revision.
`--census-only` remains diagnostic and never authorizes packaging.

## Documentation currency gate

Every code/configuration change, test result, verified source finding,
decision, blocker, and rollback must be reflected in the affected active
documents before the next implementation step begins. Live state has one
owner (`PORT_STATUS.md`), implementation order one owner (`ROADMAP.md`), and
acceptance this contract. Dated run records preserve measurements. Update
dependent guides and links together; fix conflicting current statements in
place. A candidate with conflicting or stale current documentation fails
handoff/release review, even when its individual test passes.

PS2CAP01 store records contain an address and low32 value, with no recorded width. Compare the ordered captured trace and final byte effects separately; derive wider copy widths from original instructions without claiming transient upper lanes were observed. Counter labels must be joined through the exact measured wrapper symbol/source map; colliding labels stay unassigned and zero observations never prove unreachability.
