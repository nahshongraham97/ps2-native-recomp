# Armageddon strict boot observations, 2026-11-12

The `build_native_gl_full` runner was launched headlessly with a disposable
card, strict VU1 AOT, strict native IOP, strict GPU mode, and the extracted
disc filesystem. The experimental OpenGL backend was disabled.

* With EE live compilation disabled, execution stopped after 8 frames at
  untranslated EE address `0x00648680`. That address is present in the
  game's `entries.txt`; its presence there alone does not prove that the
  currently linked generated code implements it.
* With EE live compilation enabled, using a separate copy of the ELF,
  entries and overlays and a separate live compiler cache, execution reached
  28 frames. Native SIO2MAN, MWFILEFR, PADMAN, MCMAN and MCSERV registered.
  The game exited following MCSERV method `0xFE`, for which the current host
  service deliberately replies `{-5, 0, 0}`. Temporal proximity does not by
  itself prove that this reply caused the exit.
* No VU1 execution trace was emitted in either short boot. The 490 registered
  AOT entries therefore have not been demonstrated to cover gameplay here.
* MCSERV currently has no card-operation implementation. The local PS2SDK
  libmc reference treats an init result of `-5` as an unhandled XMCSERV
  protocol and attempts legacy initialization; this run did not show a legacy
  initialization call. A separate local `ps2-port-engine` implementation
  demonstrates a directory-backed service, but its protocol and storage
  assumptions must be verified against this game before adaptation.
* A second isolated run with syscall tracing reached the same exit at frame
  28. The EE invoked `Exit(0x04)` with first argument `0`, from syscall stub
  `0x0010C404`. The saved return address `0x001D8F30` is the instruction
  following a call from the shutdown wrapper at `0x001D8F28`; it is not
  itself the initiating failure. The initiating caller remains unknown.
* Static startup code gives a strong lead: `entry` calls `FUN_00122740` and
  forwards its return value to the shutdown wrapper. The recovered
  `FUN_00122740` returns zero if one of the initialization gates
  `FUN_00225db0`, `FUN_00221c90`, `FUN_00171b10`, or `FUN_00127260` fails.
  The observed zero exit is consistent with this route, but does not prove
  which gate returned zero or that card initialization caused it. Capture
  the gate return values in a live run before changing their behavior.
* An isolated diagnostic build inserted probes into a copied generated unit
  at the four return sites. In the 30-frame strict boot, `FUN_00225db0` and
  `FUN_00221c90` returned 1; `FUN_00171b10` returned 0 immediately after the
  MCSERV initialization exchange. The fourth gate was not reached. This
  proves the third gate failed in this run, but its internal reason still
  needs to be traced. `probe-stderr.log` records the values. The scratch
  `build_native_gl_full` is configured against `probe-generated`, so its
  runner is diagnostic; the original generated sources were not modified.
* A second probe build measured the nested card initializer: both module
  loads returned nonnegative values (4 and 5), and `FUN_0011bfa0` returned
  `0xFFFFFF88` (-120). The recovered retail code checks the second word of
  the `0xFE` reply against `0x20A` before inspecting the result; the current
  host reply has zero in that word. Thus the observed failure is the retail
  client's MCSERV version check. The retail client does not follow the
  fallback behavior assumed by the host shim. Implementing real card services
  and valid version reporting is required; changing version words alone
  would not supply card operations.
* An explicitly opt-in diagnostic (`PS2_MCSERV_PROBE_INIT=1`) returned
  success and minimum versions solely to discover subsequent requirements.
  The card initializer returned zero and the third startup gate returned one.
  The game then requested `DEV9.IRX`; strict IOP mode stopped because no
  native DEV9 replacement is registered. No card-operation RPC was observed
  before that stop. The diagnostic response is not card support and is not
  enabled by default. See `probe-init-stderr.log` in the diagnostic folder.
* Static IRX inventory reports that the retail `ENT_SMAP.IRX` imports DEV9
  library version `0x0109`, ordinals 4, 5, 7, 8, 9, 12 and 13. The local
  PS2SDK DEV9 v1.9 export table identifies them as interrupt handler
  registration, DMA transfer, interrupt enable/disable, Ethernet ID, and
  pre/post DMA callback registration respectively. This is a static import
  contract, not evidence that these imports were executed. The machine-readable
  inventory is `dev9-imports.json` in the diagnostic folder. Qwen's first
  source inventory shifted ordinals 12 and 13 by one; the table was checked
  directly and the mapping above uses the corrected indices.
* With both diagnostic init and `PS2_DEV9_OFFLINE=1`, the host deliberately
  reports DEV9 module-load failure, matching the retail caller's documented
  failure branch. The game continued and requested `LIBSD.IRX`; strict mode
  then stopped because LIBSD has no registered native replacement. This is
  evidence only for the offline failure path, not a DEV9 or audio replacement.
  See `offline-stderr.log` in the diagnostic folder.
* Static inventory of retail `SDRDRV.IRX` reports 26 imports from `libsd`
  version `0x0105`: ordinals 4–20, 23–27 and 30–33. The local PS2SDK
  `libsd` table is version 1.4 and ends at ordinal 27, so it cannot establish
  the identities or behavior of the four later imports. The existing runtime
  has an SPU2 mixer, but no registered native LIBSD module was found. The
  God of War LIBSD analysis is a useful reference for common operations and
  DMA semantics; its different driver and binary do not prove the Armageddon
  contract. The static inventory is `libsd-imports.json` in the diagnostic
  folder. No LIBSD imports or audio output were observed in this strict run.
  The retail v1.5 export table has 34 entries. Local PCSX2 name metadata
  labels ordinals 30–33 `sceSdStopTrans`, `sceSdCleanEffectWorkArea`,
  `sceSdSetEffectMode`, and `sceSdSetEffectModeParams`; those labels are
  hypotheses for this specific IRX, not proof of behavior. The first 96 bytes
  at each retail entry are in `libsd-extra-exports-disassembly.txt`, decoded
  from the ELF PT_LOAD image using its actual file offset `0xB0`. Qwen
  correctly extracted the names but incorrectly described gaps in the local
  table and inferred transposition semantics; those statements are unsupported.
* The GoW reference's `native_overrides.txt` contains 16 `libsd_irx` entries,
  including the internal-looking `bus_setup` entry; this is not a list of 16
  Armageddon import ordinals. Its `sceSdSetEffectWorkArea` override is recorded
  at module offset `0x0984`. Armageddon's selected `SDRDRV.IRX` imports 26
  `libsd` ordinals, while the PS2SDK v1.5 import header names ordinal 31
  `sceSdCleanEffectWorkArea`; Armageddon's retail export table points ordinal
  31 to `0x097C`. The GoW offset is eight bytes into the Armageddon entry's
  visible prologue, so the offset difference alone cannot identify a different
  routine, but neither it nor the GoW label proves matching behavior. The
  available Armageddon disassembly is only the first 96 bytes; do not map this
  export to a native replacement until its full function and call contract are
  checked.
* A comparison run selected `--disc-files` without an ISO argument. The runner
  reported strict extracted-filesystem mode with raw ISO fallback disabled,
  MWFILEFR opened `MKDA.PAK` from that directory, and execution reached the
  same LIBSD strict stop under the diagnostic card-init and offline DEV9
  settings. This verifies the observed startup files do not require the ISO;
  CDVD-facing service calls and sector semantics remain in the runtime, and
  later game modes and files have not been covered. See `files-only-stderr.log`.
* A separate run loaded a copy of the user's state at frame 108075, with
  `PS2_VU_AOT_STRICT=1`, `PS2_TRACE_VU1RUN=1`, strict IOP and strict GPU
  enabled. It logged 3,261 VU1 entry attempts at 18 distinct PCs before strict
  IOP stopped guest instruction execution at `0x00049094` in thread 9. The
  source state and diagnostic copy have matching SHA-256
  `8D5E72D8C645E8AD1650F9919E37B105D169248F8C268713CFBFE9F9C5410E58`.
  The strict VU dispatcher prevents interpreter fallback; an entry trace is
  emitted before dispatch and does not by itself prove completion or visual
  correctness. The saved state's scene and full gameplay coverage were not
  established. This run used EE live compilation and restored legacy IOP
  state, so it does not establish the requested all-native boot path. See
  `state-aot-stderr.log`.
* The current VU1 capture directory contains 210 16-KiB program snapshots;
  the generated table contains 490 registered AOT entries, including 280
  jump-target entries, split across 31 generated C++ units. The generator's
  `--require-complete` check rejects an empty set of valid `.bin` inputs and
  any supplied or derived program omitted for a key mismatch or incomplete
  translation. The check cannot prove that captures cover all VU1 programs a
  retail mode may execute. In the copied-state run above, strict VU1 mode proceeded through
  3,261 entry attempts at 18 PCs before the strict IOP stop, with no reported
  VU1 AOT miss; that is evidence for this saved-state path only, not all modes.
  Evidence: `build/vu1-aot-audit.log`, `generated/mkarmageddon/vu1_table.cpp`,
  and `games/mkarmageddon/vu1/`.
* A repeat of that copied-state run with strict-stop ownership diagnostics
  identifies thread 9 as guest `PADMAN`: module base `0x00049000`, size
  `0x9F00`, stopped offset `0x94`, thread entry `0x00049A10`. The copied
  save is state version 8. The native PADMAN state reader resets its host
  state for versions before 12 and restores SIF overrides only if that host
  state is active. Therefore this old state does not supply the native
  PADMAN open-pad records. Migrating it requires recovering the guest pad
  state and pending work; deleting the thread alone would not establish
  equivalent controller behavior. See `owner-stderr.log`.
* In a repeat run the stopped thread's saved `s0` and `a0` were both `14`,
  with `ra=0x00049094`. In the selected PADMAN IRX, the instruction at
  module offset `0x8C` calls the import stub at `0x7E5C` and returns to
  `0x94`; the IRX import table identifies that stub as `thevent` v1.1
  ordinal 10. The local PS2SDK table names ordinal 10 `WaitEventFlag`.
  The adjacent code tests the result at `sp+0x58` and later calls thevent
  ordinals 6 and 8, named `SetEventFlag` and `ClearEventFlag` in that table.
  These observations identify a synchronization boundary; they do not yet
  identify all pad buffers or prove a safe guest-to-host state migration.
  See `owner2-stderr.log` and `padman-irx-inventory.json`.
* A strict-stop probe of the copied state captured the wait's registers:
  `a0=14`, `a1=0x1001`, `a2=1`, `a3=0x007F17C0`, `v0=0`; the result word
  at the mirrored IOP address was `1`. The SDK declaration describes these
  arguments as event ID, requested bits, mode and result pointer; mode 1 is
  `WEF_OR`. At the resumed PC the module would test the result against
  `0x1000`, so the observed bit does not take that branch. This is a single
  captured boundary, not a complete mapping of PADMAN's state machine.
  The strict-stop probe runs before any guest instruction and leaves strict
  rejection intact. See `event-stderr.log`.
* The retail thread entry at PADMAN offset `0xA10` loops through the event
  wait and then calls offset `0x5194`. Static disassembly of that routine
  reads two arrays starting at module offsets `0x92B0` and `0x9450`, with
  `0x68` bytes between slots. At the captured strict stop, the word at
  `base+0x92B0` was `1`. The meaning and complete layout are not yet proven;
  in particular, this is not yet evidence of an EE pad buffer address.
  `PS2_IOP_PROBE_ADDRESS` supplies an opt-in, read-only 64-byte IOP snapshot
  at a specified hex address when a requested probe is hit. The diagnostic
  run and its saved values are in `pad-globals-stderr.log`.
* Retail PADMAN code calls the `sifman` ordinal 7 import with a descriptor
  array at module offset `0x8330`. At the strict stop, the first two saved
  descriptors contained `(IOP source 0x00051440, EE destination 0x006AA7C0,
  size 128)` and `(0x00051B50, 0x006AA8C0, 128)`. In separate copied-state
  probes, the first 64 bytes of each source matched its EE destination.
  EE address `0x006AA840` (128 bytes after the first destination) held a
  similar record; the words at offset 88 were `0x1A5FE` and `0x1A5FD`,
  consistent with adjacent alternating records described by the local SDK.
  This is evidence for candidate buffer locations, not proof of port/slot
  identity, current DMA pending status, or a complete migration contract.
  See `dma-snapshot-stderr.log`, `dma-data-stderr.log`,
  `dma-data2-stderr.log` and `dma-neighbor-stderr.log`.
* Retail DMA construction uses records at module offset `0x8440`, indexed
  by outer port stride `0x710` and inner slot stride `0x1C4`. The saved
  port 0/slot 0 and port 1/slot 0 records contain EE base addresses at
  record offset `0x184`: `0x006AA7C0` and `0x006AA8C0`, respectively.
  Their saved counters at `0x180` are both `0x1A5FF`. The addresses and
  counters were captured from the copied state and agree with the observed
  DMA descriptors and the two adjacent EE frame values. This identifies two
  candidate host open-pad records for this one save; it does not recover all
  controller state or pending work.
  Retail instructions at offsets `0x4F4–0x508` copy the old counter into
  the outgoing record and increment the saved counter; `0x544` derives
  selection parity from the old counter, and `0x634–0x648` selects base or
  base+128. The local SDK reference uses a different update ordering, so it
  cannot be substituted without checking the selected retail implementation.
* `PS2_IOP_DUMP_ON_STOP` provides an opt-in module/thread inventory before
  strict rejection. The copied version-8 state contains 15 legacy module
  records, including PADMAN, MCMAN, MCSERV, LIBSD, SDRDRV and CRI_ADXI.
  Multiple guest threads remain, including several in PADMAN's range. Thus
  replacing only thread 9 would not make this save a native IOP state. See
  `threads-stderr.log` and `owner-515c0-err.log` / `owner-51cd0-err.log`.
* Native PADMAN now handles the blocking `padPortClose` RPC for a host-owned
  open pad, stops future record updates, and returns failure for an already
  closed pad. The local SDK reference describes command `0x0E` and its
  arguments; asynchronous close remains unsupported. The synthetic host RPC
  test covers opening, updates and closing, but the retail game has not yet
  exercised this path and old-state migration is not implemented.
* Native PADMAN reports two host controller ports and one supported slot per
  port through the new `0x0C`/`0x0D` RPCs. The host rejects opening other
  slots instead of mirroring one controller into nonexistent multitap slots.
  Synthetic RPC checks passed for the capacity queries and invalid slot;
  the retail caller and multitap behavior are not yet verified.
* Native PADMAN implements the SDK-referenced `padEnd` RPC (`0x0F`) by
  closing all host-owned pad records and stopping updates until `padInit`.
  A synthetic test covers shutdown with both ports open and reinitialization
  without reopening the second port. The scratch `ps2run` and test targets
  built and the test passed; retail shutdown and save-state migration remain
  unverified. This does not replace the missing LIBSD or memory-card services.
* The opt-in experimental GL packet backend now subtracts the selected GS
  context's XYOFFSET from raw vertex coordinates and selects that context's
  FRAME fields for readback. The scratch runner builds with this change.
  This is not a verified retail rendering path: its readback is time-based,
  can mix draws targeting different framebuffers, and lacks textured rendering
  and required GS state handling. No visual or gameplay test was performed.
  It also writes CPU-side GS memory while primary WebGPU presents its own GPU
  copy, so the experimental hook now declines primitives in primary mode and
  emits a warning when requested there. The primary renderer then receives the
  draws. In a headless scratch-runner test with a copied frame-108075 state,
  strict GPU mode and `PS2_NATIVE_GL=1`, the guard warning was observed after
  the WebGPU shaders initialized. The run then stopped at the known legacy
  PADMAN guest thread (`0x00049094`). This confirms the guard was reached,
  not that a visible frame or correct gameplay was produced. Evidence:
  `gl-guard-err.log` in the diagnostic directory.
* With strict extracted-file mode, validated indexed files now take the direct
  host-file path for ioman reads. An isolated retail startup trace recorded
  direct reads of `LOADING.RAW` (98,304 bytes) and `GAMER.OVL` (384 bytes),
  and the native MWFILEFR service opened `MKDA.PAK` from the extracted tree.
  The run used diagnostic MCSERV init and offline DEV9 and stopped at missing
  native LIBSD; it does not prove all CDVD-facing calls have been eliminated.
  Evidence: `vfs-direct-retail-err.log` (its older overlay label predates the
  log-label correction).
* The checked-out `recomp` directory contains EE and VU generators but no IOP
  IRX-to-C++ generator; `scripts/iop_irx_imports.py` only inventories IRX
  metadata. `llvm-readobj --sections --relocations` on the selected retail
  `LIBSD.IRX` reports a 19,376-byte `.text` section and 897 relocation entries:
  298 `R_MIPS_26`, 93 `R_MIPS_32`, 253 `R_MIPS_HI16` and 253
  `R_MIPS_LO16`. An IOP AOT path must resolve these relocations and the
  module's kernel imports in addition to translating instruction semantics;
  registering a successful empty LIBSD module would not supply audio.
* The local GoW reference does include `tools/iop/iopn_gen.py`: it compiles a
  captured, relocated IRX image into C++ chunks (512 instructions per chunk)
  and emits a registry keyed by module name, load base, text size and hash.
  Its generated functions return to the dispatcher at import stubs, exceptions,
  external targets and instruction-budget boundaries. This is the right IOP
  AOT reference, but it is not drop-in: it expects `IopCpuState` and
  `IopMemory`, models delayed loads and its own exception/branch flow, while
  this runtime has `ps2::iop::Cpu`, global IOP RAM accessors, `pc`/`npc`, and a
  strict path that aborts before any compiled-code lookup. The first integration
  must adapt the execution/memory contract and route a matched compiled module
  before strict rejection; the GoW files alone do not prove any Armageddon IRX
  has been compiled or executed as C++.
* The capture naming contract also differs: this fork writes a manifest name
  containing `_B<base>_H<hash>` and uses that name for the `.bin` file. The
  donor generator writes the manifest name into its generated registry. A
  direct run of that generator would therefore register the capture identifier
  rather than the retail module name. The adapter must retain both identities
  and verify base, text size and hash before dispatch; renaming files alone
  does not implement the module or its imports.
* Source review through Qwen's filesystem MCP and direct checkout inspection
  confirm that `NativeIopModule::start` is only loader-time host activation
  (`load_module_buffer` invokes it before registering exports and skipping the
  guest module). The direct imported-function seam is
  `invoke_native_import`; no production generated IOP caller exists. The
  mixed `link_imports` HLE-stub route still requires guest IOP instruction
  execution and cannot satisfy strict native replacement on its own.

Logs and the disposable card are under
`build_native_gl_full/diagnostics/strict-boot-resume-20261112/` and
`build_native_gl_full/diagnostics/strict-ee-live-resume-20261112/`. These
observations do not establish a playable game or a complete native port.

## Retail IOP capture and reference generation

An isolated non-strict, headless diagnostic with MCSERV probe initialization
and DEV9 offline produced relocated LIBSD, SDRDRV and MSL captures under
`build_native_gl_full/diagnostics/iop-capture-20261112/captures`. Their
reference-generated import bridges and registry passed Clang syntax checking.
The reference module bodies still target the donor ABI and are not linked.
The output log for that diagnostic is empty, so it does not establish a boot
milestone. The LIBSD branch-in-delay candidate at relocated `0x20588` is
offset `0x88` within its image; static inspection of the original MODULES
LIBSD ELF places that offset in the `libsd` export table at ordinal 29. The
candidate is therefore table data, not evidence of an executed branch.

## GS ALPHA=0x44 compositor update

The earlier diagnostic retail trace
`build_native_gl_full/diagnostics/native-gl-alpha-20261112/run-alpha-identity.stderr.log`
reached an actual triangle packet with `prim=4`, `attr=0x5C`, `FPSM=PSMCT32`,
`ALPHA=0x44`, `TCC=1`, `TFX=0`, and `COLCLAMP=1`. The native GL backend rejected
the packet because its ordinary blend path did not reproduce the GS
`(Cs-Cd)*As/128 + Cd` equation. The ALPHA fields decode as
`A=Cs, B=Cd, C=As, D=Cd`.

`runtime/src/gs/native_gl_backend.cpp` now routes only this supported PSMCT32,
`COLCLAMP=1`, untextured or TFX=0 source-over case through a GLSL/FBO
compositor. It renders the primitive with GS texture/depth/scissor state into
an offscreen source image, snapshots the destination, then composites using
`As/128`. Unsupported alpha tuples remain fail-closed. The compositor also
saves and restores texture-unit bindings/enables so the renderer's GS texture
cache remains consistent after the composite.

The focused Windows GL test target was rebuilt with
`..\\..\\..\\..\\dependencies\\cmake\\cmake-4.4.5-windows-x86_64\\bin\\cmake.exe --build build_native_gl_full --target ps2run test_native_gl_alpha --parallel 2`
and exited 0 on an AMD Radeon RX 6800 XT / OpenGL 4.6 context. It verified the
captured `As=0x80` case (`0x800000FF`) and two repeated cached-texture draws
(`0x6F4000BF`, within one channel code of the expected quantized result). Full
output is in
`build_native_gl_full/diagnostics/native-gl-alpha-compositor-test-20261112/stderr.log`;
the recorded exit code is `0`.

This is a synthetic renderer integration check, not a post-change retail packet
run. The latest normal strict boot still exits at frame 28 after the MCSERV
`-5` exchange and never reaches gameplay. The prior retail diagnostic packet
trace predates the compositor, so live acceptance of `ALPHA=0x44` remains
unverified.
