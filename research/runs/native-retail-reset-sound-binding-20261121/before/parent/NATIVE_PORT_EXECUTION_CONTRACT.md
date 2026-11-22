# Retail Armageddon native-port execution contract

This governs the **retail game** in `external/forks/ps2-native-recomp/`.
It describes acceptance, not current success. No agent may call a build
accepted or ready for the owner to run until every gate passes on one clean,
pinned binary. The authored replacement application is outside scope.

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
Preserve audio identity, envelopes, effects, looping, timing, and video sync
while replacing chip control and emulated transport. A simpler native mixer
is not acceptable if it changes the sound.

The pasted proposal's week/month estimates, function counts, and format
assumptions are not implementation evidence. Verify each required field and
call site against the assets/source; do not schedule or replace behavior on
those estimates.

## Verified reuse across retail platforms

The native Windows service architecture and verified PS2 work are retained.
A function or subsystem from another Armageddon version may be adapted when
its verified behavior matches the target and it closes a current blocker
more directly. Pin each version's executable/assets separately and establish
its own addresses, ABI, types, layouts and consumers. Verify equivalent
inputs/outputs, side effects, lifetime, errors, timing and asset identity,
then connect the donor to the existing native interfaces. Reuse requires the
same source, component, behavioral and whole-game gates as any other change.
No donor hardware emulator, guest address model, unapproved gameplay change
or wholesale platform switch is authorized by this reuse policy. The current
PS2 oracle remains the behavioral baseline. Missing comparison evidence is
open, even when the donor implementation appears simpler.

## Non-regression rule for the entire retail game

**No feature may regress while a PS2 implementation is replaced.** A native
replacement that boots but loses a card, plays a different effect, drops a
portrait, ignores a controller, skips a movie, changes timing, or breaks a
mode is a failed change. Reaching a later screen never excuses an earlier
loss. Restore the last verified behavior or fix the regression before stacking
another subsystem change. A previous playable build is an oracle for behavior,
not proof that its architecture satisfies this contract. The authored app and
`milestone_build_kombat_practice/` are not substitutes for the verified
playable recomp checkpoint.

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
position, profiles, saves, card insertion/detection/format/free-space/changed
status, load and overwrite, both ports; keyboard and controller press/release,
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

## Required implementation sequence

Current coding priority is the connected route in ROADMAP.md: retail entry, real native initialization, main/frame control, physical input, rendering and sound. Link-only wrappers, empty effects and component tests do not satisfy missing production behavior. Mode changes must preserve original nonlocal main reentry without guest register/stack state. The dated connected-entry record owns implementation evidence; whole-game acceptance requirements below are unchanged.

1. **Pin provenance and preserve data.** Record parent/fork HEAD and dirty
   state, compiler, binary SHA-256, launch flags, and extracted asset manifest.
   Preserve the dated backup and save/state files. Identify and hash the
   verified playable recomp checkpoint separately from the Kombat Practice
   package. Never overwrite the checkpoint, memory cards, saves, keys, or
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
4. **Finish static EE/VU coverage.** Enumerate all executable images from
   retail media offline; translate every callable EE entry and VU program
   before launch and link them into a deterministic registry. Remove runtime
   compilation and instruction/microcode interpreters from the accepted path.
   Unknown code or operations must fail closed, never become no-ops.
5. **Replace file and save services.** Map observed retail resource keys to
   extracted files with checked path normalization and exact read/seek/error
   behavior. Remove CDVD/CDVDFSV/`cdvdman` endpoints, sector I/O, ISO
   dependency, and switches selecting a legacy reader. Define a native storage
   contract preserving verified retail payload bytes/layout, atomic writes,
   and observed error/space behavior. Do not require a new save encoding. Verify card
   detection and the full save lifecycle with disposable copies of the
   checkpoint card and native save directory: inserted/absent, changed-card,
   type, format, free space, directory enumeration, create, load, overwrite,
   delete, both slots, restart, and recovery from interrupted write. The
   actual in-game UI must recognize the card and load the correct profile;
   a successful `GetInfo2` log line alone is insufficient. Compare file
   requests and returned bytes with retail evidence.
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
| Files and saves | `runtime/src/native/content_vfs.cpp`, `runtime/src/disc.cpp`, `runtime/src/iop/cdvd_native.cpp`, `runtime/src/iop/mcserv.cpp`, `runtime/src/iop/sio2.cpp` | Exact extracted-file bytes and errors; no ISO/sector path; UI card detection and save/reload parity |
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
| Memory-card UI, profile round-trip, disposable card/save copy | Only a success status from the service |
| Retail event to exact sound asset to emitted PCM and listening check | Wrong/random effect or nonzero audio only |
| Real controls and all-mode progress | Script-only `--press` or one mode only |

Each claim links every applicable row in `research/runs/`. Independent review
checks the current code and binary. A missing row leaves status **open**. Fix
the implementation and rerun the gate; do not weaken the checker.

## Known open evidence at this revision

- `runtime/src/memory.cpp` retains fixed `g_ram`/`g_spr` and PS2 address
  resolution. That does not meet the owner's native-memory definition.
- `runtime/src/iop/msl_native.cpp` has inferred bank-handle mapping and a
  fallback to bank 0. Correct sound identity is unproven.
- Current card `GetInfo2` has a directory-backed success path, but its in-game
  detection and profile loading have regressed by the owner's observation.
  Treat the card as failing until a same-build UI and save round-trip pass.
- The owner reports wrong/random sound effects and dead keyboard/controller
  input in the newer milestone. The owner's verified playable 2026-11-10
  backup is present with runner, card, and state data and is the behavioral
  oracle; the Kombat Practice package
  is not that checkpoint. These are release blockers, not acceptable deltas.
- The 2026-11-15 17:57 Run 3 at fork `c34b3fb` records a card-change `-1`
  followed by successful queries and `/BASLUS-21410` file operations; it is
  a **card behavior reference**, not proof of current card behavior or full
  game acceptance. The 2026-11-16 image baseline intentionally had silent
  game voices and cannot be used as an audio oracle. A suitable exact-sound
  event-to-output comparison still needs to be captured from the preserved
  playable oracle and recorded. Current uncommitted
  fork edits to MCSERV and MSL require their own clean matching run.
- The 2026-11-18 14:28 run reports 18,226 GS transfers stopped in emulated
  VRAM. An earlier 300-frame zero counter does not cover that route.
- The 3,000-frame combat record uses scripted input, not a physical keyboard
  and controller validation.
- The delivered milestone binary is stamped `670ca38 dirty`; its cited 14:28
  log is stamped `3e1770a dirty`. A clean matching build/run is still needed.

This list is not exhaustive. Recheck each claim against current source and a
matching run before removing it.

## Enforcement in the repository

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
