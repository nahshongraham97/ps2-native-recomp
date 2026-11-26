# Restored R192-Lineage Playable Recomp Build & Active Oracle — 2026-11-25 Session

**Date:** 2026-11-25 (`00:07:58` – `00:29:01`)  
**Status:** **Protected original gameplay reference; current compiled R192 integration is in section6** (owner-verified gameplay: **Konquest** and **Motor Kombat** work as intended alongside VS/Practice combat).
**Important Scope Boundary:** This build is the working AOT recompilation + live EE/VU + D3D12 GS + guest IOP/SPU2 build and serves as the project's **playable behavioral oracle**. The separate zero-emulation shipped native port acceptance contract (`NATIVE_PORT_EXECUTION_CONTRACT.md`) remains **OPEN** (`238 / 15,670` credited functions).

---

## 1. Pinned Package & Persistent Memory Card

- **Executable:** `external/forks/ps2-native-recomp/build/r192-hooks/out/ps2run.exe`
  - **Size:** `61,706,240` bytes
  - **SHA-256:** `8AB43BDA05E7A621B946883F3195A583F0C2455F0351BF8608282CD3E97B4617`
- **Launchers:**
  - Colocated launcher beside the build: `external/forks/ps2-native-recomp/build/r192-hooks/out/launch_r192_hooks.bat`
  - Tracked shortcut: `launchers/launch_r192_hooks.bat`
  - Instant launch (no disposable 4.3 GiB ISO copy; reads `external/forks/ps2-native-recomp-backup-20261110/games/mkarmageddon/disc.iso` read-only and streams live `stdout`/`stderr` to the `cmd` console window and `logs/last_run_stderr.log`).
- **Source Lineage:** Built from `external/forks/ps2-native-recomp` commit `67612aa` (direct D3D12 WebGPU + guest audio/IOP descendant of R192 `7c998b2`) + `host/mka_unit_wrappers.cpp` + all 7 Kombat Practice overlay hooks restored into `build/r192-hooks/src/host/mka_combat_hooks.hpp`.
- **Persistent Slot-1 Memory Card (DO NOT OVERWRITE OR RESET):**
  - **Path:** `external/forks/ps2-native-recomp/build/r192-hooks/game/disc/armageddon.ps2`
  - **Size:** `8,650,752` bytes (8 MiB PS2 card image)
  - **SHA-256 (after 2026-11-25 session):** `f38dc412d579669363c23e8aca8113b7dd592c516089b0e25b1b964612e05b3e`
  - **Write Activity Logged:** `5` card write flushes (frames `39240`, `39480`, `58320`, `58680`, `62520`), totaling `2,409` page reads, `4,590` page writes, and `363` block erases. Holds substantial user save progress including a **Konquest checkpoint**. Always mount directly via `--memcard` — never swap in a disposable card or overwrite this file.

---

## 2. Log Summary (`ps2run_2026-11-25_00-07-58.log` / `last_run_stderr.log`)

- **Captured Logs:** `external/forks/ps2-native-recomp/build/r192-hooks/game/disc/logs/ps2run_2026-11-25_00-07-58.log` and `external/forks/ps2-native-recomp/build/r192-hooks/logs/last_run_stderr.log` (`33,073` lines, `2,316,232` bytes).
- **Duration & Exit:** `68,426` frames (`336,329,334,297` EE cycles), cleanly terminated on `window closed`.
- **Host Environment Observed:**
  - **CPU:** AMD Ryzen 7 7800X3D (8 cores / 16 logical processors)
  - **GPU / Present:** AMD Radeon RX 6800 XT on Direct3D 12 (DXC shader compiler, `9` shaders compiled in `450 ms`, Mailbox present via WebGPU)
  - **Audio:** WASAPI `48000 Hz` stereo; `1,139.72 s` of game audio delivered across `1,258.37 s` wall clock (`73` brief dry events totaling `~31.00 s` during cold JIT compile stalls, `155.88 s` stretched, `0` dropped samples).
- **Framerate & Live Code Cache Warmup (`570` 120-frame profiler windows):**
  - **Median:** `60.0 FPS` | **Mean:** `57.1 FPS` | **Peak:** `73.3 FPS` | **Min:** `16.4 FPS`
  - **Stability:** `493 / 570` windows (`86.5%`) ran at `>= 59.0 FPS`; `503 / 570` (`88.2%`) ran at `>= 55.0 FPS`.
  - **Cache Growth:** `ee-live` started with `2,212` pre-translated code windows, ran `167` synchronous live compilations as new modes/areas were entered, and expanded the persistent cache to `3,022` files. `vu1-live` loaded `210` pre-compiled programs (`44` files) and compiled `2` new vector programs (`1.1 s` at frame `40320`, `0.8 s` at frame `55200`), growing to `47` files.
- **GPU & GS Counters:** `68,603` GPU submits (`9,133,766` decodes, `754,868` rasters, `68,417` displays, `3,096,189` uploads, `2,979,182` CLUT loads, `0` copy reads/writes) and `1,928,325,228` primitives drawn.
- **Modes & Owner Gameplay Verification:**
  - Owner verified **Konquest** and **Motor Kombat** work cleanly end-to-end alongside menu/VS/Practice play.
  - Observed `raw_mode` trace transitions across `0x0000000C` (front-end/menus, `60.0–60.6 FPS` avg), `0x00000004` (`frames 1440..6240` @ `58.6 FPS` avg; `63120..67560` @ `57.1 FPS` avg), `0x00000000` (`6480..38280`, `265` windows @ `55.2 FPS` avg), `0x00000003` (`38520..39600` @ `60.0 FPS` avg), `0x00000008` (`39960..58920`, `158` windows @ `59.2 FPS` avg around the Konquest/save sequence), `0x0000000F` (`59160..59400` @ `60.0 FPS` avg), and `0x00000007` (`59400..61080`, `14` windows @ `51.4 FPS` avg).

---

## 3. Hook & Telemetry Firing Summary

- **Registered `register_hook` Hooks (`8 / 8`):** `0x00197BC0` (`MKA_HealthDeduction`), `0x0084A100` (`MOVIER_ctor`), `0x007DC1F8` (`movier_007DC1F8`), `0x00812750` (`movier_00812750`), `0x00812760` (`movier_00812760`), `0x007C4980` (`GAMER_ctor`), `0x00856700` (`NETR_ctor`), `0x00856780` (`NETR_alt_ctor`).
- **Linker `--wrap` Wrappers:** Armed (`[mka-hooks] Combat telemetry armed for guest PC 0x00197BC0 (Linker Wrappers Active)`), along with `PS2_OBSERVER_HOOKS=1`, `PS2_PROFILE_SUBSYSTEMS=1`, and `PS2_NATIVE_VERTEX=1`.
- **Observed Runtime Hook Log Counts:**
  - `[mka-overlay]`: `7` intercepts (`4x` `GAMER.OVL` at `0x007C4980`, `3x` `MOVIER.OVL` at `0x0084A100`)
  - `[mka-mode-trace]`: `1,936`
  - `[mka-pad]`: `1,402`
  - `[mka-actor]`: `1,401`
  - `[mka-camera]`: `1,292`
  - `[mka-combo]`: `1,238`
  - `[mka-aggressor]`: `791`
  - `[mka-special]`: `285`
  - `[mka-health]`: `274`
  - `[mka-render]`: `268`
  - `[mka-telemetry]`: `268`
  - `[mka-practice-snap]`: `73`
  - `[mka-air]`: `50`
  - `[mka-stage]`: `8` (`Freefall Tier 2` perimeter transitions)

---

## 4. Verification Boundaries

- **Not verified in this run:** Offline-excluded `NETR.ovl` hooks (`0x00856700`, `0x00856780`) were registered but not entered in offline play; `misses.txt` was not generated; standalone native port zero-emulation acceptance remains **OPEN**.
- **Contradictions resolved with earlier docs:** Earlier status docs treated the `11/10` B9 binary (`B9EEF7DA...`) or `11/18` Kombat Practice `native-gl` binary (`8B763DCC...`, which ran at 4–27 FPS with `x5.00` stretched audio) as the working gameplay oracle. The restored `r192-hooks` package (`8AB43BDA...`) is confirmed as the active playable recomp build and behavioral oracle; older `milestone_work_verified_playable_20261110` (`B9EEF7DA...`) and `milestone_build_kombat_practice` (`8B763DCC...`) remain preserved historical snapshots.

---

## 5. Historical audit checkpoint before the compiled integration in section6

The owner authorized a separate fork branch for R192 work. Branch
**r192-aot-closure-proof**, in sibling worktree
**external/forks/ps2-native-recomp-r192-closure**, starts at fork main
7244e8da527e8a3a7400aa53bca64b41b5ce63d4. It contains authored audit,
inspection and disposable diagnostic tools, not a replacement playable package.
The native study stays on phase6-abi-leaf-cohort in both repositories, unmerged.
Both main refs remain unchanged. The project target remains compiled retail
logic with native memory/ABI/services and zero emulation or runtime compilation.
EE Live closure alone does not remove the remaining console device/memory models.

The original playable executable remains 8ab43bda05e7a621b946883f3195a583f0c2455f0351bf8608282cd3e97b4617.
No original executable, hook, source snapshot, persistent card, save or protected
corpus was written, restored or reset by this work. The owner continues playing.
F5 saves; F8 loads the newest quick state in R192, leaving the current card intact
(source: actual build/r192-hooks/src/runtime/src/quick_keys.inc).

### Source lineage and historical closure limits

The actual R192 and closure build caches name their respective build/.../src
trees. Both serialize version **8**. Current main/worktree sources serialize
version **14**. The audit pins both actual roots, Cpu headers, serializers and
CMake HOME paths; the tooling worktree must not be assumed to reproduce R192.
The earlier 67612aa + wrappers/seven restored hooks lineage is a recorded
statement, not a newly reproduced historical build.

tools/research/audit_r192_aot_closure.py SHA-256
**6c6ed8295cebb117f4956a057582948d637094abe6b75aef5de5ecd66c94394b** completed its read-only
audit with status OPEN; --require-closed exits 2. Its eight negative controls
and three positive controls pass. Final receipt, relative to this sibling
worktree: build/oracle-telemetry/r192-closure-audit-07/RESULT.json,
SHA-256 **eab0e4e35ef59cabc7a1185807b5e1242197e70c09c83d861afa511122af6402**.

- The historical 00B17700/RA002E6180 miss is not bound to the newer candidate
  PE (0ad85a65845ba9d772b83bf82eb9a87d6d7b01531799bb2ce9cf8d9d4ab13b48).
- All **61** existing state logs advance **one** frame. They do not establish
  90 new frames: the old runner passed absolute --frames 90 after restoring a
  much later g_frames counter. Historical command/binary binding remains unknown.
  This corrects the closure-run duration claim only, not the separate native
  function-boundary comparisons across 61 snapshots.
- Preserve all **43** retained B14000 image variants. Same address/range and
  sparse checked words are insufficient to identify the failing executable image.
  The recorded B885 page has the exact generated B17700 entry and all 48 check
  words match; its historical runtime identity remains unproved.
- Finite authenticated emitter templates, the historical 55-dump manifest/fit,
  full-suite identities, held-out execution and image parity remain OPEN.
  Do not use the old subset/empty-suite convergence loop as acceptance.

### Preserved owner capture and read-only resource evidence

Completed owner F5 state quick_2026-11-25_18-18-55_f080225.state is preserved
with its quick-save card companion under the sibling worktree's
build/oracle-telemetry/r192-owner-konquest-capture-01/. State SHA-256:
7802920f2bcd2114721abb3c017e774a67b86e8e04922c2a12d01eef81ab4984.
Companion SHA-256: a42816d6710af396bbd344536ee48bf2d84e329976e18c2c86fd317fea7423b7.
Copy receipt SHA-256: **f113e12689401d8238207fa25e90cbc000c36d9df7e44082abcf5ba32b57688a**. The persistent card was not opened
or copied for this capture preservation. Input recording closure is unproved;
the original study denominator stays **61 states / 72 sidecars**. A later F5
state f105764 exists separately and is not conflated with this capture.

tools/research/inspect_r192_konquest_state.py SHA-256
**1dd14616b7897a36228774b8e6c39df0aa80c767948d0ddc52cc801768b951c1** verifies the pinned
version-8 fixed EE prefix from actual source, Clang Cpu layout and producer PE
byte-count witnesses. RAM begins at file offset 0x5C4; saved frame 80225 is at
0x020045D4. This is not a full variable-subsystem parser or a full restore proof.
Seven refusal controls pass. Final receipt:
build/oracle-telemetry/r192-owner-konquest-capture-01/resource-source-audit-01/inspection-04-root-guard/RESULT.json,
SHA-256 **5909ee68c1aee0817453aad8de929888cbb29042ab1a21b1de8a7eac74deb376**. Its adjacent command-argv.json records replay inputs.

The linked GP 006540F0 resolves pointer word 0064F250 to resource context
00BBFD00 in this capture. Original 00466B10 subtracts F12 from context+A0,
clamps against zero/context+9C, then refills to maximum when context byte+0D
mask01 is set. Observed current/max power is 1/1 and byte+0D is 43 hex, so the
retail refill mechanism is enabled. Life uses different fields +68/+64.
The original dispatch table and selected instructions are joined; the recovered
mega_power script supplies supporting naming evidence but was not rejoined to
its original MKO container here. UI magic identity, the setter record's producer
and the current cause of that flag remain unproved. No host-assist cause is
established. The actual arena hook predicate excludes the mode8/P2-null capture.
No new Konquest or arcade hook was implemented. Future hooks need their own
mode, ABI, reader/writer and firing evidence.

The captured B14000 page matches **0/43** retained pages; B17700 is zero.
This is the current owner state, not the historical failed runtime image.

### Fresh copied-PE probe: authenticated missing code, not a pass

tools/research/probe_r192_aot_copy.py SHA-256
**f63abd6d76da541400f8efb533a738298b3b7c98cf83cc1bebde9e91e91b1081** copies the fixed candidate PE,
ELF, DLLs and quick-save card companion into fresh owned telemetry. Copies must
match their initial pins before launch. The ISO is read-only and hash-pinned;
EE/VU Live are disabled, there is no window flag or audio setup, and all output
paths/card writes are disposable. Seven planted false-pass conditions reject.

The one requested diagnostic was saved frame **80225 + 16**, absolute limit
80241. It loaded 80225 and stopped at 80225 on **008A5140**, RA **002E6180**:
**zero** new frames, hence OPEN_DIAGNOSTIC_FAILURE. The runtime itself returned
0 despite the miss; the tool correctly rejected both stderr and the nonempty
misses.txt independently. No replay/image parity or suite closure is claimed.
Receipt: build/oracle-telemetry/r192-owned-aot-probe-01/RESULT.json,
SHA-256 **adf1afecb481ab9b490e1decd513acd3ee659c2a0ca2035c8da010068bd46e44**. All pinned inputs remained unchanged.

The exact fresh 16 KiB miss dump 008A4000_D8F8253E.bin has SHA-256
5cf739b2a2257c95429ea9b1828eb6e1e66b1a580befa40375771f7faca36192;
entry word D9640000. It differs from that page in the starting snapshot:
runtime writes occurred before the missing call even though no new frame ended.
A whole-page hash includes data and does not prove an executable extent.

Corrected source/runtime join:
build/oracle-telemetry/r192-owned-miss-join-02/RESULT.json,
SHA-256 **4bd5e5ea33162a9a3d898b26557f2c7dabc10ad166147bbfb2a391766a5fc096**. JALR at 002E6178 reads its target through object+4C;
the delay slot computes t3 using the value loaded from object+394 at 002E6164.
RA002E6180 is the caller continuation in 002E60E0, not an authenticated emitter.
The fresh dump's object008A5250+4C equals008A5140; field+394=34hex produces
observed t3=008A5210. The first derived receipt incorrectly named field+8;
arithmetic against the logged registers refuted it. That failed receipt is
preserved and invalidated; no runtime/code generation used its interpretation.

**Exact next dependency:** prove the writer, executable extent and complete
generated body behind object+4C for this bound fresh miss; retain code/data
distinctions and every image variant. Reproduce the version-8 candidate from
pinned sources in disposable output before any full/held-out comparison.
Do not retire the working EE Live route or replace the active oracle yet.

### Native-study review, protection exception and verification scope

The 30-leaf refusal follow-up rejoins all **417** original words and exact AST
fragments. Raw error counts and all 30 annotations remain in the existing native
work item; primary first-statement causes are typed memory13, incoming ABI6,
branch/control6, return interface/bundle5. These are diagnostic denominators,
not predicted unlocks. Runtime-valued SSA, typed memory effects, branches and
general return lowering are separate gaps. Stop serial binding rounds. Keep
001A3BE0 as a deferred, bounded Stage A candidate; no broad handwritten retail
rewrite follows from this study.

The earlier protection exception concerned the persistent card itself:
baseline f38dc412... changed to a42816d6... after the completed baseline.
The owner confirmed concurrent R192 use; the exact writer was not independently
attributed. Only the ad-hoc source publication helper's obsolete card-hash
equality check was excepted for source-only publication. No project checker,
manifest, expected pin, card or process was changed by the agent. The unchanged
hash statement applies to the completed controlled baseline, not all later
owner activity.

Existing native baseline **59 targets / 65 variants / 16 gates** is carried
forward for unchanged native sources. The current canonical parent/fork ledger
sync passes14 artifacts/3 docs. This oracle branch retains its own main-base
ledger bytes; the native study's additional ownership admission is not imported.
Transformed/native credit remains **2/238**. No native implementation, gameplay
acceptance, physical audio, new visible frames or whole-game completion is
established by these tools. The protected build remains available to the owner.

### Not verified

EE Live closure, full/held-out replay and image parity, authenticated template
coverage, writer/type/executable-extent recovery, new hook firing and full native
acceptance remain OPEN. Resource-flag cause and UI magic identity are unproved.
The fresh diagnostic is a failure, not a whole-function or gameplay pass.

### Contradictions with earlier documents

The old closure-state duration is one new frame, not90. Current main's version14
serializer is not R192's version8. RA002E6180 identifies a continuation, not an
emitter. Exit0 did not establish success. The protected card changed after the
controlled baseline; only its earlier source-publication hash check was excepted,
with no agent card write or project-checker change. The failed field+8 derivation
is preserved and superseded by the original field+394 instruction/register join.

### Falsification attempts

Eight audit, seven prefix-inspection and seven copied-probe refusal controls
pass. Wrong image/entry/binary, subset/empty convergence, inadequate frame
duration, wrong ABI/version/GP/pointer/pin, protected output, ambiguous reload,
nonzero-exit/timeout and miss-file false passes are rejected within their stated
scopes. The real exit0/missing-code run was also rejected. Earlier native plants
and the full unchanged baseline are retained as prior evidence, not newly rerun.

---

## 6. Compiled R192 integration without EE/VU Live — 2026-11-25

**Current compiled R192 executable:** external/forks/ps2-native-recomp-r192-closure/build/r192-compiled-konquest/out/ps2run.exe,
SHA-256 **8823021a23e2a3fdd3b2dc4ca0c8eac9e7359beb3e5bce489b965587a0c2479b**.
The familiar parent launchers/launch_r192_hooks.bat selects this build;
launch_ps2recomp.bat is an alias of that same entry. The old native-GL scripts
were moved byte-for-byte from active fork roots to the existing
research/history/native-port-docs-2026-11-18/launchers/ archive. Protected
backup/oracle copies were not changed. The original 8AB43BDA... executable and
its original colocated launcher remain the preserved comparison reference.
The fork has only main and r192-aot-closure-proof branches locally/on origin;
parent has main. Reviewed phase6 work was fast-forwarded to main, and unmerged
historical tips were preserved as published archive tags before branch removal.

### What was actually built

The owner's diagnosis was confirmed in the original R192 CMakeCache:
PS2_GENERATED was empty and PS2_PREBUILT_GAME_LIB pointed to the protected
backup's old libps2game.a. Later generated AOT sources had not been linked into
that R192 executable.

The isolated owned build copies the actual version-8 closure source and
generated inputs, sets PS2_PREBUILT_GAME_LIB=OFF and PS2_GENERATED to its own
generated_ee directory, and compiles/links its own game/runtime archives.
The initial 2,393 generated C++ sources linked successfully (PE 62ec792d...).
The final source set has **2,455 C++ files**, including the expanded VU set.
The LLVM-Mingw Clang 23.1.3/Ninja toolchain, Release, -mavx2 and
-ffp-contract=off match the actual lane configuration. Source/build manifests,
exact configure/build argv and linker map are retained inside the owned build.
The source manifest SHA is 1b7ce9032a1a643ebfd83f8ba6abd5f305caed35240e89f923e3c0c6aabc1a6a.

The latest cache harvest preserved **3,569 EE DLL variants**, **48 VU DLLs**,
217 raw VU inputs, 88 closed recordings, 14 new states and their companions.
No persistent card was copied or written. The completed 20:31:49 owner log ends
at 204009; the later 20:50:24 startup fragment is not called a completed session.
That completed session loaded 3,521 EE windows and added 48 DLLs; startup and
translation stalls are recorded in SESSION-SUMMARY.json.

The EE libraries are already compiled native code. They are copied into
out/aot-modules/ee and linked/registered once at startup by an authored native
Windows loader. No code is translated or compiled on a missing address.
The runtime does not link ee_live.cpp, vu_live.cpp, live_build.cpp or the EE
generator. The CMake/linker map authenticates the owned AOT library rather than
the old backup archive. Missing code returns a named nonzero failure rather
than invoking a compiler; unknown VU1 programs do not fall back to an interpreter.
The legacy metadata export names/ABI header are retained for binary compatibility,
not a runtime compiler implementation.

The package seal is manifest SHA-256
**20e7a347164805453fe608d7226d3cb87f318dc0c3e285ea4ba8856fc4b4f86d**,
exact EE stamp A2659FA9BD846661, Cpu 1456 and EeLiveHost 328.
All 3,569 DLL hashes, exports, stamps, module counts, metadata/entry bounds and
original sorted quick/normal ranks are verified before any registry mutation.
Files remain read-locked and libraries resident. All variants remain distinct;
no near-duplicate merging, zero-filled reconstruction or same-name aggregation.
Metadata has 589,356 overlapping entry records; this is not a function-census count.

### Hook and VU integration corrections

Regeneration changed numeric entry IDs. Both wrapper conditions AND the
real-unit calls in the copied hook header are rebound from actual generated
case-to-PC maps, preserving the intended retail PCs:

| PC | Unit | Old ID | Verified current ID |
|---|---|---:|---:|
|00162E70|0018|309|503|
|001637D0|0018|328|544|
|00181F90|0020|126|154|
|00197BC0|0025|204|318|

All eight original replacement hooks and all three linker wrappers remain.
Actual packaged fresh boot logs show eight registrations, scanner entry544
firing five logged times, the MOVIER overlay hook, observers and input trace.
Registration is separate from firing: NETR/offline-excluded hooks and every
combat/mode-specific path are not claimed to have fired in these tests.
No new Konquest/arcade assist was invented or implemented.

Offline VU generation used all 278 baseline and 217 captured aliases, preserving
both conflicting raw images. Both actual compilers produced 986/501 program keys;
the merged table has **987 keys**, with 500 identical common function bodies and
one added key BAC8608B3F8BC8AD over that baseline. For 9700A8B92357617A,
all 605 reachable instructions and the emitted function match; raw image
differences begin at 12F8 after the reachable region. Both images' additional
jump targets are retained. The 125 emitted C++ files reproduce byte-for-byte
through the reusable expansion tool. These are reference-build
AOT keys, not additions to the native ledger or the separate 493-key native study.

### Executed checks and runtime packaging

- **Owner's latest complete Konquest recording:** 181224 to 204009,
  **22,785 new frames**, all **1,519 recorded checkpoints equal**,
  exit 0, no EE/VU miss, runtime compilation or input/binary changes.
  The raw run and independent strict-driver assessment both pass.
- **Frozen 14-state cohort:** all 14 completed exactly 90 new frames, exit 0,
  zero misses/compiler events and unchanged inputs/package/PE. None was omitted.
  This replaces the old absolute-limit mistake for this cohort; it is coverage,
  not full-recording parity for those14 cases.
- **Actual packaged launcher:** fresh boot to 900 frames, exit 0, no misses,
  from an unrelated working directory with system-only PATH and its LOCAL
  ISO/ELF. A disposable explicit test-card override was used. No GUI/audio
  endpoint was opened. This is not a headed visual/audio acceptance run.
- Real package initialization validates/registers all3,569 modules without guest
  callbacks. Two positive/eight negative loader cases, seven package controls,
  eight replay-driver controls and two VU union controls pass in their stated
  scopes. Failed preliminary metadata/layout/overstrict comparisons remain
  preserved and are not counted as passes.
- Original native 59-target/65-variant/16-gate baseline carries forward for
  unchanged native sources. Parent/fork main ledger/doc synchronization is
  checked separately; transformed/native credit remains **2/238**.

Code/modules, ISO, ELF, SYSTEM.CNF and the copied original key mapping are
inside the compiled build. The normal launcher deliberately mounts the
**existing original R192 persistent card**, never a new/blank card, and uses
that same card directory for existing F5/F8 states. These are the intentional
external user-data paths. No original card bytes were read/copied/reset/written
by staging; only disposable quick-state companion copies were used in tests.
Original executable, launcher, keys and original ISO bytes remain unchanged.
Normal settings preserve combat/overlay hooks, observer/input trace, profiler
and native vertex. EE/VU Live are disabled and no compiler folder is added to
the runtime PATH.

The first staged launcher had a trailing-backslash quoting issue in quick-dir;
its pre-validation bytes/receipt are preserved under r192-runtime-stage-01.
The corrected launcher passed the actual 900-frame boot above. A read-only
progress sample of the agent-owned headless candidate was used during the long
replay; no original owner process memory was read or written.

The top-level launch_ps2recomp alias -> familiar R192 shortcut -> packaged launcher also passed verification from an unrelated directory with system-only PATH, before boot/window/audio and with an explicit disposable test-card override. Receipt build/oracle-telemetry/r192-primary-launcher-01/RESULT.json SHA-256 526c7bcdb48b092cb4324547e99b4eed9148ea2e2c6ff45bf80a542eee338607. SOURCE_BUILD_DIGESTS.json in the owned build pins 283 v8 source files, 2457 generated files (2455 C++ plus headers), 46 external native hook headers used at build time, both actual archives and the linker map. Its SHA-256 is cc836b0f24161904f941fd4b26863af621bc26e203b32c1ab79b4286ae0bdfd4. These compile-time headers/toolchain paths are not runtime dependencies.

Evidence paths below are relative to the isolated fork worktree:

| Receipt | Path | SHA-256 |
|---|---|---|
| Latest complete Konquest replay | build/oracle-telemetry/r192-latest-konquest-replay-01/RESULT.json | 03f63e39a8e304bac231e5a50dd6aba72b1495261dfef720fd16e96e81d6ebe2 |
| Strict 1519-checkpoint replay check | build/oracle-telemetry/r192-latest-konquest-replay-01/STRICT_CHECK.json | c7783b563672cde4feca0d5feff9622104ea12d80793199ef3e3c6847bd10bd0 |
| Fourteen-state frame batch | build/oracle-telemetry/r192-new14-frame90-01/RESULT.json | 6e48a3b883aa4d073e6ec71466f02371b072bebffd86f262139b4a1f1f2dcde0 |
| Packaged launcher fresh boot | build/oracle-telemetry/r192-packaged-launcher-01/RESULT.json | 5a22317ed34704f7e295b9f571e332171fd9b69e0db4d34c62fd79cb7a8a2263 |
| Sealed module validation | build/oracle-telemetry/sealed-ee-loader-tests-final-01/RESULT.json | 8402d12702d0031e7f694d485878cea1c06d28c0b90eeecbdccdc233d44753f0 |
| EE package | build/oracle-telemetry/r192-precompiled-package-01/BUILD_PACKAGE.json | f7b967a98fa96c70c95215cd5c43118109f0584df2037844ea6b32729d6e64fb |
| Replayed public cache inspector | build/oracle-telemetry/r192-public-cache-metadata-01/RESULT.json | ea96abf0df2386bd8d5f56e1ddffedd9c4a8617838a5859e5e14ef85245ec45d |
| Replayed public VU expansion | build/oracle-telemetry/r192-public-vu-expansion-01/RESULT.json | 1f2c29ac9ad3cf70351002bffada3d1e40b773df867fa98b58862c3ee7e13d0f |

Authored source identities:

| File | SHA-256 |
|---|---|
| tests/test_r192_precompiled_modules.cpp | b3367864cc3837bcfa994805b828c63d5057fcb6281cafa098622b7331ab3b40 |
| tools/research/inspect_r192_cache_metadata.py | 78327902bbfe0a0726048402ea3f37320207a4ffcd32bc62384384a5be0f21f0 |
| tools/research/expand_r192_vu_sources.py | 0626efb68938341af4d26541bebeb635613652dbed4d739340705cfe909bf806 |
| tools/research/package_r192_precompiled.py | abb4d2c7659d3636813ecfd91f76404d3d893fb311ee78f49ac2be0c4b10a75d |
| tools/research/integrate_r192_aot_candidate.py | 824be86a99e1793f45d35f04090d06dec8857d51a04e4e1e359ce1c57412ae04 |
| tools/research/replay_r192_aot_sessions.py | 9a3921b0c0036052997b20021de9b18baa9c7427787b39e3cb5146d57ac925ed |
| tools/research/stage_r192_playable.py | 4be10f2e14054b2ed93a15a094b62f89cea03f06e9431caa6adb1b57298819e6 |
| tools/research/r192_aot_modules/loader.cpp | f828b628638000a58c0ea12e2f8e4d0e27404d8ee1a691d3648fdecb6d360629 |
| tools/research/r192_aot_modules/loader.hpp | 26bc6856c4d6431831cf72b87d0cd4bad18ed160ec0ab1f702450d82387c6955 |

## 7. Complete precompilation requirement and original-data coverage

The owner requires every possible executable path to be precompiled. A passing
recording, a sealed cache, or a fail-fast missing-code handler does not establish
that result. A completed owner playthrough is not a prerequisite: stored code,
overlays, original code-generating routines and their input domains define the
work. Runtime-generated EE families, VU upload/patch/indirect-launch coverage
and the remaining IOP interpreter are still OPEN. Native memory/ABI acceptance
and the 2 transformed / 238 native credit totals remain separate and unchanged.

The supplied screenshot shows about 9.1 FPS, repeated synchronous EE Live
translation pauses of 0.2–0.3 seconds, and 117 reported audio underruns.
It establishes real compilation stalls in that recording. It does not identify
the renderer backend or apportion all CPU/GPU cost. Source defaults to GPU
primary and can fall back to software when initialization fails; no headed
performance or physical audio claim is made here.

### Static overlay omission corrected in a separate candidate

The copied v8 generator truncated MWo3 images at header +0x0C's text extent.
That excludes the original constructor roots GAMER 0x007C4980 and MOVIER
0x0084A100, whose addresses are present in the original +0x18/+0x1C constructor
tables. The already reviewed main-branch reader retains the complete mapped
image and seeds those authenticated roots. Only that reader function is
substituted into a fresh copy of the v8 generator; the installed source is
unchanged. Module input hashes remain 70714c6f... (GAMER) and 3a34e383... (MOVIER).

tools/research/close_r192_static_overlays.py reproduces the correction, checks
the actual entry-map ID AND matching switch destination, and compiles the
reader against both original modules. Two positives pass. Restoring the
truncation and removing constructor roots are each rejected for both modules
(four actual negative executions). Earlier failed test-build attempts remain
in stages 01–03; stage 04 passed; final published-tool replay stage 05 regenerates identical C++.

The separate candidate build/r192-static-compiled-candidate/out/ps2run.exe
SHA-256 c07082e496fed888a0890a66a811bcb9106e938804541365c2dce5132764e262
contains all 13,137 callable base catalogue starts, all 2,537 MOVIER starts
and the GAMER start. The sole missing base row 0x005BF218 remains authenticated
descriptor data, with zero function credit. There are 2,339 generated EE units,
including 1,948 captured dynamic windows, plus 125 VU C++ files and table code.
Its archive is freshly compiled; its reference runtime archive is the unchanged
owned v8 runtime. This compile-time archive reuse creates no runtime dependency
on an external build.

The complete recorded Konquest comparison passes: frame 181224 to 204009,
22,785 new frames, all 1,519 checkpoints equal, exit 0, zero misses and no
compiler events; 328.126 seconds headless. No normal launcher is switched by
this study. This verifies the recorded execution, not complete path coverage.

A separate offline-only NETR study generated 1,788 of its 1,834 catalogue
starts, leaving 46 explicit refusals/missing entries in
build/r192-catalogue-all-static-01/RESULT.json. It is not integrated and does
not change the standing online feature/service exclusion or any credit count.
Do not use that study to assert all catalogue entries or online behavior pass.

### VU programs recovered from original ELF payloads

tools/research/extract_r192_elf_vu.py parses 99 bounded, MPG-only DMA RET/END
payload streams directly from the pinned retail ELF, at file offsets
0x0042CC00 through 0x004E27B0. Each payload has its actual PT_LOAD mapping,
original command/word ranges and complete copied source bytes recorded.
All entry-zero direct reachable words lie within bytes uploaded by that
payload. This is not a proof that every upload format, launch root, indirect
target or runtime patch has been enumerated.

**Invalidated VU extension experiment; do not promote.** The original
generator emitted 942 keys (843 speculative targets); 929 bodies matched the
987-key cache and 13 appeared additional. A later per-target original-byte
check showed that 142 generated keys read unuploaded, zero-filled bytes.
ALL 13 additional keys are in that unsupported set. The compiler treated
IADDIU constants as possible code targets without proving they were code
pointers. Those 13 are not recovered original programs; the earlier claim is
retracted. The 99 structured source payloads remain real.

The separate 74fc7c9b... binary compiled and passed package startup and a
90-frame probe, but neither check establishes source admission. It was never
selected by the normal launcher. Its files and successful raw observations
remain as rejected-experiment evidence. New generation must enumerate targets
to a fixed point while admitting only complete source-backed code, and report
unproved target hints separately instead of supplying zero-filled tails.

The same origin audit found old cache input 883AC7731B3857A8_0000.bin,
SHA-256 551ad4eb1ada7d39c6867f7395b9d9561b3ef2f1718c8fecc185f67232d604d7.
The existing ledger had already rejected this exact capture because of the
backwards-from-GS extraction across IPU. The copied historical cache baseline
had inherited it. The corrected generator enforces the published rejection
by byte identity, including renamed copies, without deleting or modifying originals. No native credit was ever awarded by this experiment.
Of 283 unique recorded image/entry pairs, 282 match one original ELF payload
at every direct-reachable word and identical VU offset; only this rejected
capture fails that whole-source join.

### Corrected VU source admission and full candidate replay

The source-input manifest is now mandatory and pinned before staging: every
baseline/captured filename and SHA-256 must match exactly. Unknown, missing or
changed inputs refuse. The rejected capture is excluded by byte identity even
when renamed; changing its old filename's bytes requires new provenance.
Known/renamed rejected bytes and a changed-name control are rejected. No original
capture is deleted. Clean inputs are 277 baseline plus 217 captured aliases,
118 unique images, generating an exact 986-key union.

Original-ELF generation now disables the compiler's implicit target expansion.
An explicit worklist checks the complete direct-reachable words against the
actual MPG upload spans before any entry reaches the compiler. Linked returns
and IADDIU constant hints recurse to a fixed point; 157 unuploaded hints refuse.
The 99 original payloads yield 875 source-bounded instances / 861 unique keys;
814 keys match the clean baseline exactly and 47 are additional entry variants,
for 1,033 total keys. These 47 are not claimed as 47 called functions. The
zero-filled-tail hint plant and four malformed-payload plants are rejected.
This worklist does not yet establish every E-bit/MSCNT continuation, indirect
target, upload composition or runtime patch; those remain explicit closure work.

tools/research/build_r192_vu_candidate.py builds fresh local game AND runtime
archives from the corrected sources. Every shared body must remain identical;
only the known rejected key may disappear, and only the source-proved additions
may appear. No existing archive is edited. The separate candidate is
build/r192-vu-source-admitted-candidate/out/ps2run.exe,
SHA-256 81ce40dbd80ba7933a0bf73d5d248dc42f61a401d6ee2602264412e5f5e821fb.
It compiled 2,513 targets in 401.212 seconds. The complete recorded Konquest
replay passed frames 181224–204009: 22,785 new frames, all 1,519 checkpoints equal,
exit 0, zero EE/VU misses and zero compiler events (362.358 seconds headless).
Its stderr SHA-256 is fa60019f362be655c52de85df80f7a0d994eea8801122eee8fd41b2c42667b14.
The installed normal 882302... route is unchanged. This is a bounded reference
candidate, not universal precompilation or native-port acceptance.

An attempted shared-main copy was withdrawn. Main did not previously contain
these VU helpers; the assumption that its older helpers needed repair was
incorrect. The unnecessary new-main addition392cdd35 is reversed without
rewriting pushed history. All four verified source/test files remain on the
R192 branch. The main-root control receiptfd52b253... is historical verification
of that temporary identical copy, not current main source. Its measured files
and WITHDRAWAL.json are preserved under the same telemetry folder. No original
or normal runtime input changed. Before any future cross-branch source edit,
inspect the actual pre-change tree and named diff; a branch's documentation
references do not prove that its tree contains the referenced implementation.

Source/tool pins and immutable raw results distinguish the invalid experiment
from this correction. The candidate builder's sole publication style change
removes an extra blank EOF; its measured copy and Python AST identity certificate
are preserved in build/oracle-telemetry/r192-vu-correction-publication-01/.
The clean-input and source-bound generator scripts match their measured receipt
hashes exactly. Native ownership, 2 transformed and 238 credited totals stay unchanged.

### Original-source VU stop/resume measured subset

The next compiler worklist includes plain E-bit stop continuations. The frozen
v8 reference advances PC/NPC before execution, completes the following pair,
saves TPC=E+16, and VIF MSCNT restarts there. All 173 E-bit pairs in the 99
original payloads have no combined branch and no preceding physical branch.
Conservative branch/E contexts refuse rather than assume this formula.

The test-only tests/test_r192_vu_continuations.cpp links the actual pinned v8
VU implementation. Eight controls cover plain stop/resume, executed delay,
I-bit immediate, wrapping, branch-on-E, E in a branch delay, JR-on-E, and a
branch inside the E delay. Wrong one-pair resume, dropped delay effect and
assumed branch fallthrough are rejected. This proves reference control
semantics for those cases, not hardware arithmetic or complete VU equivalence.
Harness attempt 01 failed link; 02 used mismatched unused test declarations and
is not admitted; including the actual JIT/Live headers exposed the mismatch in
03. Attempt 04 uses those actual declarations and passes. Failed attempts remain.

The source worklist now recursively follows proved plain-E continuations as
well as the prior linked returns and constant hints. Every entry still needs
complete uploaded-byte admission before generation. Current receipt:
build/oracle-telemetry/r192-elf-vu-continuation-01/RESULT.json,
SHA-256 b0e1479bd608a693d59e84390ec90ba225efd1cc2f18285ed3b4e7caa4b1483e.
It emits 1,047 source-bounded instances and a 1,175-key union, 142 more
source-backed variants than the previous 1,033-key candidate. A fresh candidate
compiled in531.004 seconds with1,175 keys, SHA-256
bfced6b44da3faa702e9bc88c0aae1e0a0b9510eb6b20f30ce7cd694fdd3cb31
(build/r192-vu-continuation-candidate/out/ps2run.exe). Its complete recorded
replay passes22,785 new frames, all1,519 checkpoints equal, exit0, zero misses
and compiler events, in322.177 seconds. Build receipt26fe709e599210c3e14ec1c3ea9bbbc72410cc96f63e6cac957a244dc63405e6;
replay build/oracle-telemetry/r192-vu-continuation-replay-01/RESULT.json
SHAbda10b8f03d9c2c7671061af5b59b18393e726dd99af3c1abda352ee8e962f0f.
All shared generated bodies remain exact. This candidate is not the normal
launcher and still contains the reference IOP/VU0 interpreters. Unknown external roots,
indirect target dataflow, code compositions/patches, EE families and IOP/native
services remain OPEN. No launcher, protected input or credit changes.

### Generated EE family: original builder, typed pipeline and no-yield proof

The complete cache audit processes all 3,569 DLL/module records and 589,356
entry records, not just the heap stub capture. It joins the four exact image/PC
pairs visible in the owner's screenshot. Its 881 normalized checked-code groups
are not 881 proved producer families; missing instruction/delay tails remain
explicit. None of these counts establishes unplayed-path closure.

The original builder 0x002ED4B0 regenerates all 244 bytes of the actual captured
program (52 code/padding words plus 9 constants) from its descriptor/attributes.
Fifteen original-built descriptor configurations cover all five effect callbacks
and eleven admitted effect IDs, with one supported filter. The source guard is
13 effect records; the filter append has no local capacity proof, so only the
witnessed zero/one-filter domain is admitted. Unsupported IDs, duplicate17,
unknown compositions and binding/extent mismatches refuse.

The authored semantic_pipeline.hpp uses native spans, resource-tagged typed
values and five high-level effects. It contains no CPU, PC, guest register
bank, instruction decoder, code cache or opcode operation stream. The latest
whole-pipeline check passes 3,976 complete-RAM/ordered-write/accepted-count
comparisons: 1,080 original-builder numeric cases, 2,400 alias cases and 15
zero-count tests among them. Four actual source mutations and six refusal
controls pass. Constants, transient writes and overlap snapshots retain their
original order. These are bounded component results, not transformed/native credit.

The separate no_yield adapter admits exactly the captured [1,5,6,17] effects
plus filter2, with proved resource bindings and adjacent constant block.
It charges 16+18*N+16*accepted instructions; the checked worst bound16+34*N
must fit signed budget and be no greater than the incoming budget BEFORE writes.
Literal scalar control/status preserves existing bits, merges original raised
flags and replaces only the last executed comparison condition. Original-builder
DIV flags are excluded from the callee. The connected constructed frame matches
578 ordered stores, literal FCR and budget13791. The adapter passes 432 complete
RAM/order/count/FCR/budget comparisons, 432 prewrite insufficient-budget refusals,
11 arithmetic boundaries, 36 skipped-remap-overflow controls and five source plants.
The 48 reachable original instructions are authenticated separately from padding
and constants on every numeric builder replay. The other14 configurations have
pipeline-output proof only, no new timing contract.

Runtime adoption stays OPEN. Real low budgets occur (1893/count80 fails the
worst2736 admission); typed continuation/event scheduling is required. At that checkpoint,
VU MAC/status and VF2/3/4 projection was open; the bounded vector follow-up
below now proves those effects for the exact captured family. This family
preserves the accumulator. The whole caller
comparison is a constructed replay seeded from an observed frame, not an
observed whole-caller execution. No preferred-family route is installed yet.
Unknown families must not become a generic VM, JIT or fabricated success.
All16 authored EE source files match at least one final measured source pin.

| EE evidence under build/oracle-telemetry | RESULT.json SHA-256 |
|---|---|
| emitted-cache-entire-grouping-03 | 283ed5f9743f6dc3c0e4936bdd92e3a9e946804242829662bc6ffad232cd47bc |
| emitted-builder-domain-06 | e3f46531c6026be4331a6b7f558a3cafad66ef3d35f0691d4a51c661f40fddf5 |
| emitted-builder-actual-descriptor-06 | ffa154fa526bd0af42c3404f9454ee45a28a196b9936f82281fc61fa062a603c |
| emitted-builder-whole-references-02 | a7b1464c1c472611f3b537471473f6831af268cb6fcf1c6c4fb05835e4c29275 |
| emitted-clip48-actual-boundary-03 | b646c42b31147f1c4a4c0cb5e0ab4db4620a356cd71f24893d725772b3a53e4c |
| emitted-clip48-actual-resume-pair-02 | 4007829b55d01c0652914350efa78e34c8314cc66f104fe8926aa08e2c87c4fc |
| emitted-clip48-dispatch-actual-03 | 289ae8a3bb42e494ca9becb322a06cc787838def391d59af44c902d9656798d8 |
| emitted-semantic-pipeline-06 | 78cffaeac5b755c37ffd52e66f9525c6ec965c1abde96654dc5c7324722fb9f6 |
| emitted-semantic-caller-03 | 2487f7347bcf2cb2a3b5a1efc359ca321c987ac3c014195a382b0c43932e840d |
| emitted-semantic-no-yield-02 | e9287944fc2b50f706d70410a58b084df2c78079d05eab79a99febca7eeec571 |

Reproduction uses tools/research/{replay_r192_builder,
compile_r192_builder_references,verify_r192_semantic_pipeline,
verify_r192_semantic_caller,verify_r192_semantic_no_yield}.py in that dependency
order, with fresh owned --output directories. Each receipt pins the actual
ELF/listing, v8 headers, compiler, boundary captures, fixtures and generated
original programs, and records its compile/generator/execution command.
The no-yield verifier intentionally consumes immutable pipeline05/caller02
fixture sources; pipeline06/caller03 separately verify the final frozen core.
Original R192, N, cards, saves, launcher and all credit rules are unchanged.

### Disc-derived IOP compiler/dispatcher source phase

Read-only original-disc enumeration finds41 IRX plus two IOPRP images containing
32 ELF aliases:73 source aliases /70 distinct original payloads. All70 PIC bodies
are generated and compiled in both a separately labelled queued-load model and
a frozen-R192-v8 compatibility model. There is no default module selection.
The v8 mode preserves its immediate-load and diagnostic SYS/BREAK behavior;
it is not advertised as hardware-correct R3000 behavior or a native service port.
The earlier fixed-placement study's29 CFG/branch refusals and the initial v8 PIC
batch's8 refused delay joins remain recorded; the final PIC compile is a distinct
measured scope, not a waiver of all control-flow/runtime obligations.
The final v8 domain reuses93 object compilations whose source/object identities
were checked, rather than calling them93 newly compiled payloads.

The compatibility scalar test matches20,000 actual N interpreter operations
over190 source shapes plus48 unaligned-pair steps. Representative SDRDRV
whole-export tests cover448 calls at six physical placements and KSEG0/KSEG1
execution aliases80027000/A0027000, with physical relocationbase27000 retained.
The private actual-loader dispatcher compares five placements,86 source
instructions and seven known native service calls each, with per-step CPU/RAM,
ordered writes and original IOP serializer bytes equal. Relocation carry,
wrong ordinary immediate, forged relocation/site and unknown-service plants
refuse. A known loader import J/NOP retains two actual instruction steps.
Unknown callbacks never synthesize V0=0.

The disposable adapter uses the actual v8 Cpu156 ABI with explicit pc136/status144
assertions and field copies to a separate generated state type, never a layout
cast across v8/v14. Private-kernel include rewriting names the original IOP
headers explicitly; an earlier wrong include lookup failed compilation and is
preserved. Every executed instruction is reauthenticated; native service calls
invalidate cached module checks. The disposable candidate
build/r192-iop-v8-candidate-03/out/ps2run.exe,
SHA-256 880e269645bd9e3b4b5e041d642b313159050eb850916c436fe2ab257bb19b4b,
links no original IOP interpreter object.

Actual saved-state integration is REFUSED, not passed: snapshot02 loaded
frame181224 but measured zero new frames, stopping at thbase#42,
IOP PC000A1B98 / RA000AC4A0 in MSL. This exposes an original N silent-success
placeholder, not an admitted replacement service. The first attempt's SEH
report obscured the exception text; the next attempt adds the named refusal.
The subsequent service#42 phase below connects and tests this service. Other
imports, indirect destinations, exceptions, scheduler lifecycle and complete
IOP execution remain OPEN. No normal launcher/N/card/protected input changed;
all actual tests use a disposable companion card. These reference AOT results
award zero native or transformed credit.

All20 frozen authored source paths, exact hashes, raw result identities and
literal CLI argv arrays are in the existing ignored telemetry record
build/oracle-telemetry/r192-iop-source-freeze-01/RESULT.json,
SHA-256 d8c758d8bb43801adb3ef7904803b4627ce722a8d3a03ab9c86d93464c9ad13d.

| IOP evidence under build/oracle-telemetry | RESULT.json SHA-256 |
|---|---|
| r192-iop-domain-02 | ef311498267533dd6d2e7ca9175d568255bded60ea97fcef81f89bc4de2c29b6 |
| r192-iop-pic-domain-01 | 51140ca0422e8a4c93584deb11564598878e767e18416127dd7ece4c62dfa9d8 |
| r192-iop-pic-v8-domain-02 | 489516027f81bd629222d609ff7fbd4f16ed82365d0954b490e8e48e282e9535 |
| r192-iop-pic-differential-03 | 11f649563109e03de50b5bc5fdd082f7c2e96f38825cc6bfb4dcb4ae0ee7a170 |
| r192-iop-v8-semantics-03 | f8b8b45bf2ae829afa12b5fe24db4c0744cefdca693a4a2a4ec25b013129e045 |
| r192-iop-v8-private-dispatch-04 | 9b11dde7b69e126545aba7120c34b6cacc0fbf6019ffe4f614b329c5980287a4 |
| r192-iop-v8-snapshot-02 (REFUSED) | b40f40c2ef9b654feba59a8041fbb98d830d7cd9f27c9e221121f2c6ef21c326 |

### Whole-disc code-input boundary

tools/research/inventory_r192_code_inputs.py reads the pinned disc.iso read-only,
including all105 physical files,245 PAK members and200,460 bounded SEC sections.
There are no unparsed local-container limits in this measured set. Known code
magics identify43 ELF inputs (41 IRX, SLUS and NTGUI) plus3 MWo3 overlays.
This is a container census, not a claim that absence of a magic proves absence
of executable producers or script semantics. The IOPRP ROMDIR descendants have
the separate70-payload IOP census above; no code/image count earns native credit.
The early physical-file-only census is historical and is superseded by the
PAK/SEC-aware replay.

NTGUI.ELF is separately identified as4813336 bytes, SHA-256
3ba914401c9be9e36921e605f42c5909ac4a3eb8d93ad743286050621c08e5ef.
Its original strings name network-configuration operations and NETCNF/SMAP
modules; the main image references its disc path at00633560. This external
utility is retained in the inventory, not silently counted as compiled SLUS
gameplay or added to the current15,670 denominator. No online integration or
owner decision is changed.

Do not treat a historical .mko label or naive concatenation recipe as an EE
code image. The original2170E0 loader and216C90 linker have102+57 listed words
joined exactly to the ELF's actual PT_LOAD mapping; the latter's three omitted
words are original zero/NOPs at216CCC/216D1C/216D34. The records distinguish
function/variable/name/type/value regions and a command region whose references
are resolved through the original high-byte table. Complete MKO command/caller
semantics and their possible generated effects remain open; this inspection
does not classify every record as machine code or promote any native function.
This avoids adopting the older unverified container claim as current proof.

Raw census build/oracle-telemetry/r192-disc-code-inputs-02/RESULT.json
SHA-256 bf0908b9dbeba133bb026e623ea3ef8d2d5fea0e0d11a2a533a692e5e3db2ea9
pins the original ISO and parser. GHIDRA.json in that folder is
476e229277cf971ff3c0bad51c936f4bb2bfd56b8799da862eaa8102adb61983;
LOADER-WORD-JOIN.json and NTGUI-ROLE-STRINGS.json keep the bounded joins and
original string offsets. Ghidra API inspection was read-only, with no GUI use,
project mutation or new lock removal.

### Typed vector effects and actual EE-call observations

The exact captured family now has typed vector side outputs: constant-product
prelude, last transformed/loaded values, MAC and status/sticky effects.
The prelude executes at zero count; rejected elements still update the vector
results before filtering. The original status mask preserves0xFF0, merges
raised sticky bits and the final low flags, and clears incoming bits>=12.
The accumulator and all other defined VU fields remain unchanged. No guest
CPU, opcode stream or register-bank runtime enters this semantic helper.

The vector check compares the entire Vu0Regs on432 numeric/count/alias/status
cases and the constructed caller. Four plants reject skipped zero prelude,
skipped rejected-element effects, false high-status preservation and wrong MAC
lane sign. The final core reruns all3,976 pipeline comparisons and the prior
literal-FCR/no-yield checks. Scope is the exact4-effects+one-filter family;
the other14 descriptor configurations retain their prior output-only boundary.

A privately relinked original observer executed the real frame80225→80315
(90 new frames) on a disposable card, with zero misses and unchanged original
dispatch. It counted50 known-family and34 other-family calls, capturing two
real high-budget calls and one real low-budget call. Offline native shadows
of both actual high-budget N80/A80 calls match every RAM byte, the entire VU
state, literal FCR, budget and source-defined CPU fields. Each actual return
also joins an independent compiled-original replay with560 ordered stores.
Final budgets are13852 and12761. This is an actual JALR before/after observation,
not observation of the whole2E60E0 caller entry and not a native runtime replacement.

The actual budget1893/count80 call unwinds to008A5171. It remains explicitly
REFUSED by the no-yield admission; typed continuation/event handling is now the
next step. Unknown shapes, code/constant/descriptor changes and lifetime/output
alias domains remain refused. Seven planted errors per admitted actual call
(return, budget, VU, code, constant, order and transient write) are rejected.
The live observer makes no claim to recording each store; ordered writes are
the separate re-executed-original comparison. No normal runtime was modified.

The observer's final replay reuses all18 immutable actual before/after files
and the same PE from actual01; it reconstructs the instrumented original unit
identically. Actual01's label was too broad and is narrowed to JALR boundary;
replay02's relative/absolute path source guard failed and remains saved.
Replay03 fixes path normalization and passes offline without another launch.
All seven authored vector/observer source files are pinned by their final receipts.

| Vector/actual-call evidence under build/oracle-telemetry | RESULT.json SHA-256 |
|---|---|
| emitted-semantic-vector-02 | a57fd9d7fe9c7d363bc3f256bca5b9af6b953e0847490da2244a1a08f7603397 |
| emitted-semantic-no-yield-vector-03 | f411bcba23c1011e6b841a5ead3fe694b79d59607be11441086d58e924dba0f9 |
| emitted-semantic-pipeline-vector-07 | 98e524bbfbfad3f129278fdf78ef04b2a0e8b48568f542c8a6218ffd9fdd9379 |
| emitted-semantic-caller-vector-04 | 30f936840ffba3c6f9c3d6fdf1ade82e37eb33133005195c4918a2a982da90db |
| emitted-semantic-actual-observer-03 | 4af49188cb170fdae3f1e5c2d4ef78d35bd3fddd86bb99d72ae047fdfba515d5 |

Reproduce the final observer comparison with
tools/research/observe_r192_semantic_family.py, --vector-proof pointing to
emitted-semantic-vector-02, --replay-observations pointing to actual-observer-01,
the pinned candidate-build/source-fork/state/card/compiler inputs, and a fresh
--output. The receipt retains literal argv and dependency hashes. The original
still executes in the observer; shadow agreement does not retire EE families,
prove all descriptors, or award native/transform credit.

### Current baseline and next independent VU coverage step

The post-publication native foundation baseline passes59 targets,65 variants
and16 gates (81 recorded checks including variants), with protected hashes
unchanged and expected acceptance/strict-census refusals retained.
build/oracle-telemetry/complete-baseline-20261126-001941-39ab9287/RESULT.json
in the canonical fork has SHA-256
0921c98e3119aa258c063ebbd146241e272d2a3f220b8e0a1c4eefee6113bb6f.
The2 transformed/238 native totals remain unchanged. This baseline covers the
native foundation; the separately cited R192 candidate/component tests cover
the closure experiments, not accepted gameplay.

The99 original MPG payloads contain66,036 uploaded instruction pairs.
The next independent offline step enumerates every aligned uploaded entry
and checks its entire canonical reachable-word set against original upload
spans, including entries not named by existing captures/hints. This is a
conservative code-entry superset, not66,036 proved game functions or launch
roots. Generated instruction semantics remain those of the pinned v8 compiler.
Complete original-entry generation/compilation, indirect/code-patch coverage,
VU0 use, EE family continuations and IOP services remain OPEN.
Only35.4GB disk was free at this checkpoint: new work must reserve space,
avoid duplicate input images and never delete backups to make room.

### Typed continuation prefix and actual scheduler attribution

The new semantic_continuation.hpp retains typed consumed/accepted cursors,
snapshotted constants, vector/scalar side effects and a native resume phase.
It executes no CPU/register-bank/opcode stream. Original prelude, count guards,
per-element costs, accepted-write costs and final return are charged in their
verified order. A suspended true guard resumes the next element directly.

The final component run passes1,296 constructed whole calls /2,292 stop-or-return
boundaries with fullRAM, ordered writes, fullVU, literalFCR, budget and cursor
comparisons,1,788 guard events,216 between-stop constant/descriptor mutations,
and1,304 named reuse/domain/lifetime/scheduler refusals. Five compiled-source
plants reject incorrect cost, equality, resume charge, consumed cursor and
sticky flags. This does not yet prove full native Cpu projection at a real
scheduler call: source-known scalar scratch/volatile values are the next ABI
binding; original ThreadContext serializes those values.

The first strict actual low-budget comparison FAILED and remains saved.
A narrow actual pre/post-preempt capture now proves why: the typed56-element
prefix (392stores, budget-25) matches every pre-preempt RAM byte. The original
scheduler alone changes00650340(00→30) and00650358(2D→2E), refills budget20000,
and returns true. Its post-event RAM equals actual call-return RAM exactly;
Cpu differs only in budget. The later genuine resume enters with33, not20000.
The prefix/native core does not whitelist the two bytes or implement those
external writes. Specific inner writers, widths/order and accepted native
scheduler ownership remain OPEN. A future reference-only adapter must invoke
the actual compiled scheduler and preserve its effects.

The observed natural resume matches the next independently replayed7-store
segment. Current constructed callbacks and captured callback outcomes stay
separate. SharedVU interleaving/lifetime must be validated on resumption or
refused; one retained pair does not prove all-thread exclusivity. Normal N,
original R192, cards, saves and native/transform credit are unchanged.

| Continuation evidence under build/oracle-telemetry | RESULT.json SHA-256 |
|---|---|
| emitted-semantic-continuation-01 (FAILED) | 74b6422ab343897ceb4fad3c7cf95aaff2d7d1d392fc60c210e17560f3617796 |
| emitted-semantic-preempt-observer-02 | 6c97ec2c4c4e28bea7a0188ea7f10ec3f4450ee0cb8d2029178331dcf10668d3 |
| emitted-semantic-continuation-02 | c073a76072641fc7fa2fc363d5f84cf32d6b8357dca31f1e260ef407a0b58836 |

The first preempt-observer replay incorrectly globbed preempt files as call
captures and refused; final replay uses an exact case-number pattern and
preserves the original files. Exact reproduction argv and all dependencies
are retained in the final receipts; no second observation is inferred from
offline replay.

### All-uploaded-entry VU generation, native compilation pending

The exhaustive original-entry scan classifies all66,036 uploaded pairs:
65,924 source-bounded entry instances /65,026 unique keys,112 entries refused
because their direct reachable sets leave uploaded original bytes. Every
refusal is recorded with its first missing PC and count. This is an entry
superset, not65,026 identified functions or proved launch roots.

All99 payload batches now generate complete C++ with the pinned original v8
compiler, implicit target synthesis disabled. The complete generated-body audit
finds zero differing bodies under the same key, zero unsupported-lower fallback
sites, and zero changed bodies among1,041 keys shared with the prior candidate.
The remaining134 prior keys are outside this original-source entry domain;
their capture/hint provenance is separate and is not promoted by this study.
Native-object/DLL compilation and integration of the larger set are pending.

Generation build/oracle-telemetry/r192-vu-all-uploaded-generation-01/RESULT.json
SHA1fd01dd7233560172ca68f97b5b91cbfa87dfd25107387f56c983a66e1c48ec8;
body audit r192-vu-all-entry-body-audit-01/RESULT.json
SHA06160cd5e0bf85e9cb3ffa94adc1ee6c30d19dd16845de427e5c219cad2aabf4.
The initial enumeration-only receipt reported a vacuously true generation flag
while generation_requested was false; it is not generation evidence. The
producer now reports null when generation was not requested. The full generation
receipt is the actual positive evidence. Disk reserve remains12GiB; original
inputs and backups are not cleaned to make space.

### Native thbase42 service connection in the disposable IOP build

Original THREADMAN variants and PS2SDK commit
d4b9943bd4a58dbe69800bf4ec9d81640df5bc5b establish thbase1.2 ordinal42 as
GetThreadCurrentPriority. The original export relocation, complete body,
QueryIntrContext import, signed LH TCB+0E and interrupt error-100 are pinned.
The original MSL consumer calls DelayThread, then42, then passes the returned
priority to RotateThreadReadyQueue in the actual call delay slot. No-thread
states refuse rather than invent zero; interrupt error is verified before
requiring a running-thread value. Four downloaded SDK inputs retain their
hashes and commit-pinned source URLs in the evidence receipt.

The typed getter comparison passes287 cases, including constructed signed/error
cases and the14 actual snapshot inputs; only one snapshot supplies a running
thread(priority50), the other states remain explicit None inputs. Original
method frame stores are measured separately from native query effects.
Four compiled source mutations reject signedness, interrupt-zero, no-thread-zero
and wrong-field changes. The old export scanner stops at null slot3; ordinal42
was independently bounded/joined, not credited from that truncated summary.

The new disposable PE is
b124b2c3a23ff14e5f604a620229ad91e22e5dfe5ca66ef7af39502eb7f99c90.
It retains the strict IOP AOT dispatcher and has no original interpreter object.
Full same-recording replay passes181224→204009,22,785 new frames, all1,519
checkpoints equal, exit0, zero misses/compilation/unknown fallback and unchanged
inputs. All14 affected state probes pass90 frames each(1,260 total).
The installed N/original R192/card/launcher are unchanged. These reference
domain results award zero native or transformed credit.

Elapsed headless replay is489.7587seconds versus328.1258 for the earlier
full-static candidate and362.3583 for the source-admitted VU candidate.
These are confounded build comparisons(about1.49x/1.35x elapsed), not an isolated
IOP cost attribution or headed FPS result. Later batching must preserve code
identity and actual scheduler/service boundaries.

| Service evidence under build/oracle-telemetry | RESULT.json SHA-256 |
|---|---|
| r192-iop-thbase42-evidence-02 | 54c6926f7f0678dab9d25002a5cdaf1c1c1d77e8b5ac24419ed9f11d25a7268d |
| r192-iop-thbase42-full-replay-01 | 68f3787d4cebdc1c778d23cdd92cad622b83c7aa5b91f3eed44c3e51ebc2f221 |
| r192-iop-thbase42-probes-01 | f54a2252e4758091ae748ed17ac2c25e082e8f0e7a3b5abc6a882112099275f5 |
| r192-iop-thbase42-freeze-01 | 7cd70744e9a523fda32b963fa4035b23f90e011d966801a558523b871fc3d7db |

The freeze receipt lists all five source files, final component/evidence/run
receipts and literal build/replay argv. Buffer-loader header-name aliases are
still open; filename identity alone is insufficient. No arbitrary alias or
success fallback is added by this phase.

### Native VU module compiler pilot and complete batch

The first61-entry payload now compiles to a native DLL and passes exact exported
table keys/count, source/ABI stamp, register/host sizes, table-image bounds and
executable-function-image/page checks. Null/incomplete host bindings refuse.
Five planted stamp/ABI/count/key/function-bound errors are rejected before
any game registration. No VU program is executed by this metadata probe.

Final pilot03 receipt is
build/oracle-telemetry/r192-vu-entry-module-pilot-03/RESULT.json,
SHA2858404b9a000caee3f0b2dd19aa3c15da93f38dd1298fa22296eeda102b96aa.
Explicit completed-build resume verifies existing receipts/binaries without
changing them. A planted interrupted owned output is preserved under a checked
sibling path before its successful rebuild; receipt
r192-vu-entry-resume-control-01/RESULT.json
SHAd440ffb854dd10c0d164d80281f37dd30dd59109ced9c99c912f772504a543c6.
All move targets are resolved inside the owned output; no originals are cleaned.

The full99-payload native compilation batch is running, with two compiler
jobs and a12GiB disk reserve. This emits precompiled native DLLs using a sealed
metadata interface; no runtime compiler or game loader is enabled. Native code
build, runtime registration and execution equivalence remain distinct gates.

### EE full-Cpu scratch, literal IOP loader names and exhaustive VU audit

The reference scheduler saves volatile fields as well as return values. Typed
algorithm intermediates now preserve those observable values across native
continuations. The full-Cpu comparison excludes only dispatcher-owned PC/base;
callback budget stages are compared separately. All 2,295 boundaries pass,
including 1,296 constructed whole calls / 2,292 continuation boundaries and
65,544 original EE same-input subtraction cancellation tests. Five targeted
scratch mutations are rejected. Prior pipeline, vector, no-yield and caller
checks were refreshed against the actual authored headers. The root reran the
positive full-Cpu binary independently. This is source-derived algorithm scratch,
not a guest register bank in the native core. No runtime adapter is admitted by
these component checks; actual compiled scheduler effects, shared-VU interleaving,
thread lifecycle and state-load contexts remain separate gates.

IOP filename and buffer loading now share an exact literal registry for all 70
original payloads: 70 existing filename pairs plus 70 original IOPMOD header-name
pairs. The actual frozen loader reads PT_IOPMOD+26, bounded to 56 bytes. Literal
empty MSL is preserved without a default. The actual-loader/selector test passes
140 filename/buffer cases and rejects 210 unknown-name, changed-header and
changed-code plants (350 total). No module entry is executed in that test.
Thirteen payloads have executable-equivalent variants: 26 path cases select an
identical relocated code/import body rather than uniquely identifying the raw
original file. No recovered original-file identity is claimed for those cases.
All 140 body/descriptor objects are reused unchanged; only the exact registry
object is newly compiled. Native filename bypasses observed in this fixture: 0.
Unplayed imports, indirect targets and exception domains remain open.

The exhaustive VU generation auditor was falsified with owned constructed copies
of the first authenticated 61-entry payload. Positive duplicated bodies pass;
a conflicting same-key body, unsupported lower emission, missing table entry
and unrepinned source change all refuse. The four authored enumeration, audit,
native module compiler and falsification tools belong on the R192 branch only.
All 99 original payloads have already generated C++; the 99-DLL native batch is
running with two compiler jobs and a 12 GiB disk reserve. Completion, sealed
runtime registration and execution equivalence are separate gates. These changes
are not installed into the normal 882302... route and add no native credit.

| Completed evidence under R192 build/oracle-telemetry | RESULT.json SHA-256 |
|---|---|
| emitted-semantic-full-cpu-02 | 42dfbf045c06a9206eb64ac5cb839823f9b72a8754bf1a457630af034da73ffe |
| emitted-semantic-continuation-scratch-03 | 4215e42e5f078e494948db75e66210e95346860d310f9ce1b727b77194b98040 |
| emitted-semantic-pipeline-scratch-08 | a57d895f350f56a3603d6d576dea6305ff9e677632969beef8b71fb919c7230a |
| emitted-semantic-no-yield-scratch-04 | 7e9ac51eaffab6120cc0faa038f5063e25850871269f616b5ea06ec343502d38 |
| emitted-semantic-vector-scratch-03 | bc7a3ee356b94d31af237fbe27c6044281876fd3ab0f4dd2ba2958769ee117ac |
| emitted-semantic-caller-scratch-05 | 2d972f565bbfa7fbd1bd24ae93d7ed0cb199cdbaec98ad148119eb5fdaef1994 |
| emitted-semantic-preempt-observer-scratch-03 | 3715318321c5667bbb91ca70435cb942eed356aeb21644b0f8d89ab2a8e36efb |
| r192-iop-loader-names-freeze-01 | 47262fe8a6fb9406ff781d03218f8fff1dbe5e434f143774e46b138f6b596b99 |
| r192-iop-loader-names-domain-02 | e59ab1201dfe99da405255cfa442da426ea94e771bb97fce551e71c46d21ef91 |
| r192-iop-loader-names-test-01 | 48e78c338811141fa38165ec111e2f8ca8249c24b4105b8a9930e1fb6b18faf3 |
| r192-vu-entry-audit-falsification-01 | b6852801c1589e32ddfef0f5a810566d18528a6bc551ae25427b10dfa389a424 |

The fresh full native-foundation baseline remains PASS 59 targets / 65 variants /
16 gates (complete-baseline-20261126-001941-39ab9287, SHA
0921c98e3119aa258c063ebbd146241e272d2a3f220b8e0a1c4eefee6113bb6f).
This R192-only batch changes none of that native foundation. Its affected
component checks above are separate. Ledgers remain 2 transformed / 238 native.
Original executable/launcher, installed build, original inputs and persistent
card are untouched. The earlier claim that buffer-loader aliases were still
open is superseded by these 350 component tests, not by full IOP acceptance.

### Sealed VU loader controls and original IOP export frontier

The actual authored VU loader passes eight headless component cases using an
original precompiled 61-entry pilot DLL. Normal registration is exactly once;
altered manifest seals, DLL seals, table counts, key-set hashes, unsafe names,
and a failure in the second DLL all refuse before registration. A second
initializer call refuses after the first successful registration. Admitted DLLs
remain locked against writes/deletes for their execution lifetime. All pending
DLLs are released on validation failure. Registration happens only after every
package member passes. Generated chained calls select compiled VU1 code through
the frozen dispatcher (unknown entry halts); the original PC masking is retained.
This does not remove the separate VU0 micro interpreter. No program or game is
executed by these loader controls. A pilot package cannot enter the full-game
connector; the complete 99-payload package is required there.

The package producer's first resource check used a not-yet-created parent path
and failed before any output creation. It now checks the existing owned build
volume. The root provenance join also corrected its denominator: 282 accepted
recorded image/entry instances represent 281 unique keys. All 281 occur in the
65,026-key original-source domain; none is among the 134 legacy-only derived
entries. Those 134 entries remain separately recorded, not promoted by this
join. The inherited rejected 883AC7731B3857A8 capture remains excluded. This
root join does not prove partial-upload combinations or indirect-target closure.

The literal IOP registry plus source-proved thbase42 service is connected in a
separate candidate (8e7311fc0a68842afd37859ea6393abd8865ab6948e39632a04bca5ac8960a7c).
It passes 181224 -> 181314 (90 new frames). A whole original SDRDRV start proof
also passes filename/buffer loading across physical, KSEG0 and KSEG1 aliases:
six routes, 86 original instructions and seven known HLE boundaries per route.
Full CPU/RAM, ordered per-step effects, service transitions and original IOP
serialization agree with the pinned reference fixture. The two libsd#4 plants
refuse by name with zero synthetic callbacks. The 90-frame test and six original
function comparisons are distinct evidence; neither closes unplayed imports.

A whole-source audit of all 70 original IOP payloads finds 499 import requirements
including version, or 454 distinct library/ordinal pairs, over 35 libraries.
The actual boot/service registry contains 196 of those pairs; 258 are absent.
Original export-slot candidates exist for 247 absent pairs, but their actual
registration, provider start, version, load order and lifetime still need proof.
Eleven pairs have no bounded candidate in this audit: dmacman 4/5/6/8/9/14/15,
heaplib 8, intrman 14/28/30. No guessed callback was added. The original rooted
frontier still has 544 register-destination and 431 exception-boundary obligations.
Text-aligned JALR/JR shapes are candidate counts, not extra reachable functions.
The next IOP work proves the original module/export route using already compiled
provider bodies instead of inventing 247 replacements.

| Evidence under R192 build/oracle-telemetry | File | SHA-256 |
|---|---|---|
| r192-vu-sealed-loader-control-01/pilot | PACKAGE.json | 30d240eb9be0c7003c2b971679c6a0d8b3f255b58943a2055d852226fb6152e1 |
| r192-vu-sealed-loader-control-01/checks-01 | RESULT.json | 0eafc26ebc3a9d9396165f25649176f88e34a4e32031bdef829f0b31c4691d2b |
| r192-vu-sealed-loader-control-01 | ROOT-KEY-DOMAIN.json | f0c71897670670a7da15c1c320a24771d07f0be280658906f1cf74f9ab4f9d56 |
| r192-iop-loader-closure-freeze-01 | RESULT.json | 0838b536b86bae2a3b14430ec63016647b10aa6567d05f2ba2a65d7f354ff2fa |
| r192-iop-closure-audit-02 | RESULT.json | 34b7b05a7d4a24a17f3ddb6605e264d2fd4f6649c91bd99fafe83a576362e5e3 |
| r192-iop-loader-execution-01 | RESULT.json | 72a480e6b922edcc821f00159fa9faeb9e11ce0f4e6bb20bb51be7e183d03d16 |
| r192-iop-loader-alias-snapshot-01 | RESULT.json | 9ea6231b32226fb5f7a6bcb2d379346ed2d1b4bdfbd93bc2d442739a37ce64f1 |

Native foundation source and 2/238 credit remain unchanged; the previously pinned
59-target / 65-variant / 16-gate full baseline still applies to that unchanged
source. These R192 changes have their affected checks above. The original R192
executable/launcher, installed 882302... route, original assets, card/saves and
model settings are unchanged. Full VU native compilation and actual integration
are continuing, not claimed complete by the pilot. Complete offline game closure
and final native-only acceptance remain OPEN.

### Actual preferred typed EE family with original scheduler

A disposable reference candidate now actually selects the typed algorithm for
the authenticated emitted family, rather than just replaying its inputs offline.
The native continuation lives outside the guest register bank. Reference-only
projection preserves full original Cpu boundaries before the unchanged compiled
scheduler runs. The adapter checks original code/caller/descriptor identity,
proved input relationships, shared-VU state and active owner/frame lifetime.
Reset/start/dormant/delete and machine-state replacement hooks retire typed
contexts; an orphan loaded resume or changed parked state refuses. The original
state serializer cannot save a live native continuation in this experiment and
refuses that operation explicitly. Other known families retain their sealed
compiled AOT path. No interpreter/JIT route is added by this adapter.

Projection tests pass all 2,295 original full-Cpu boundaries and parked-state
mutation checks. The adapter passes four whole original body segments, including
a low-budget stop and two resumes, comparing full Cpu/RAM/ordered effects.
Nineteen named admission/lifecycle failures and five source mutations are
rejected. The real scheduler retains arbitrary callback stores; no two-byte
whitelist or fabricated scheduler implementation is used.

The actual owned 90-frame run (80225 -> 80315) completes 50 typed invocations,
including nine suspensions and nine resumes, with 18 original preempt calls and
zero pending contexts at exit. Another 34 family invocations use their existing
compiled originals. Zero code misses or harness objections are reported. Final
frame/cycle (80315, 394766299044) equals the separately pinned original observer
run. That frame/cycle equality is not a frame-image comparison or full-memory
proof for the entire game run; the four segment comparisons are separate.

Candidate SHA-256 is
61f51f2263b671e3d2a9ac5dc8f6224a38c34ebe23434f5719609b209cf92640.
The first package-check argv omitted the required program argument and refused
before game/state/card execution. Its log remains preserved. The corrected
package check passes on the same PE, followed by the actual run. The authored
builder now includes the required pinned ELF argument; no rebuild was used to
hide a failed game comparison.

| Evidence under R192 build/oracle-telemetry | RESULT.json SHA-256 |
|---|---|
| emitted-reference-projection-01 | 7aa6a86e05c0b151d16742deba5fcc62568fc74346c67145eb5ec5c325f84a56 |
| emitted-reference-adapter-04 | a0e6f68d2b48e43fd07a62d7be6011ca2a7bff9ebc4d6105d2250e9da79bc820 |
| emitted-reference-preferred-probe-01 | 47acb854ce5fba66c91898d342e863155610d3b3e617060370c2a239a0ee916b |
| emitted-reference-preferred-probe-01/probe | f569c3d46368140e1d7e996c2f5f6357c9b1a581bf6ebd819b853648a67c341e |

Only this exact producer/configuration is runtime-admitted here. It has no native
ownership credit, no full-recording/held-out acceptance or headed performance
measurement, and no native scheduler adoption. The full original emitter table
(five callbacks / eleven IDs plus filters/composition bounds) is the next source
domain; 34 observed families are validation inputs, not that domain's definition.
Normal N882302..., original R192 executable/card, launchers, saves and settings
remain unchanged; native credit remains 2 transformed / 238 native.

The original-source VU native batch has completed 60 DLLs. Its own authenticated
Python worker and clang children were stopped to increase only this offline
build from two to four compiler jobs. The completed receipts/binaries are reused
under the same source/flags/ABI stamp; the partial next module is preserved under
an owned interrupted sibling before rebuilding. No game process was stopped.
WORKER-RESTART.json in the owned batch directory records the exact process
identities. The 12 GiB reserve remains enforced. Full compilation, runtime
registration and execution checks still remain separate, open gates.

### Original IOP provider/export execution and remaining lifetime gap

The existing frozen loader/dispatcher can route a consumer import to an actual
compiled original provider export. With original LIBSD and SDRDRV, both filename
and buffer routes start the real modules through the scheduler: LIBSD returns
0 after 74 ticks, SDRDRV returns 2 after 87, and both real on_exit callbacks are
consumed. Twenty-six original libsd import stubs join the exact registered
version0105 export slots via J/NOP. This is registration/binding evidence for
26 slots, not 26 executed or differentially verified methods.

One export (libsd#4) is executed. Five original caller/stub instructions establish
its mode0 argument and target. A separate unchanged kernel::call_guest invocation
then runs that original export to its real return stub, result0. The context is
constructed through the original loader/scheduler APIs; it is not a historical
RPC entry or a contiguous complete SDR caller. Both routes match the original
N instruction reference in raw return Cpu, restored Cpu, full RAM, ordered writes,
native service transitions and final IOP serializer. Per-step fingerprints apply
to startup/caller prefix/service boundaries, not every internal call_guest step.
All eight plants reject, including a zero result without observing the actual
return. No synthetic callback or new runtime service was installed by this proof.

The earlier contiguous scheduled attempt hit its explicit 10,000-tick diagnostic
bound and remains preserved. That limit does not establish an infinite loop or
missing service. The observed inner original delay loop increments a stack local
and compares against120; the larger call can repeat it. The later successful
callback-adapter test is recorded with its different context, rather than being
substituted for that refused contiguous case.

Lifetime is still open. The frozen reference RegisterLibraryEntries path stores
a host map, and its ReleaseLibraryEntries (loadcore#7) returns a bare zero.
Official SDK comparison exposes linked-client, exact table pointer, version and
header-field behavior absent from that reference implementation. The original
LOADCORE word join and a source-proved ownership/retirement model are required;
a map.erase or successful zero would not prove them. Some export metadata lies
inside .text, so its proved mutable data must be distinguished from instruction
identity without a broad code-check exemption. This is the next IOP boundary.

| Evidence under R192 build/oracle-telemetry | SHA-256 |
|---|---|
| r192-iop-export-route-freeze-01/RESULT.json | c64d034b724b7aae5c7232244a16bbf1e490f5c587c07a0b59c0073c7aea13dc |
| r192-iop-export-route-05/RESULT.json | 8282370d6f5734cf6fa0c3570dd78d0308a249f5c216bc57429867dff5150eec |
| emitted-reference-preferred-probe-01/FREEZE_BUILD_LINK.json | 63d9a0bbf6a54e922323ea11dab9af4cccf36df0bf895b5fbf638f082ba913fb |

The preferred EE companion now pins its measured four private sources/objects,
contract, originalN sources/archives/ABI and runtime witnesses. Build/link argv
are explicitly reconstructed from the immutable source factory and original CLI,
not claimed as captured commands. No unchanged object was rebuilt to fill that
receipt gap. The separate merged EE/IOP/static-AOT candidate passes both targeted
90-frame cases; its complete recorded replay is currently running and is not
claimed passed here. Original builds/cards/settings and all 2/238 native credit
remain unchanged. Prior native-foundation source/baseline is unchanged; these
new original-export tests are capability evidence, not native acceptance.

### Full producer resource schema and bounded replay deadline

The original compiled classifier/load/store builders now establish the resource
schema beyond the earlier one-configuration examples: 20 classified tags plus
one unknown->0 case, 264 exact emitted transfer-word cases and 40 whole original
builder single-resource descriptors pass. All 1,947 selected listing words join
the original ELF. Classes1/2 select 128-bit VF transfers; class3 selects FPR32;
class4 selects GPR32; class5 selects GPR64, including the source's aligned LD/SD
and unaligned LDL/LDR + SDL/SDR branches. Emitted offsets truncate to16 bits;
the resulting signed address domain is a required binding proof, not an implicit
native pointer offset. Four constructed format/word/unaligned/sign plants reject.
The root independently reran the positive original-compiled schema fixture.

IDs alone do not choose scalar/vector semantics: IDs25/2/3 share the callback
that branches on SOURCE resource format; IDs26/5/6 share the callback that branches
on DESTINATION format. Existing semantic examples and the actual preferred
adapter stay narrowly admitted. Fifteen constructed filter descriptors (counts
0..4) show valid comparisons are emitted in order, while an unknown first ID or
nonzero first record+16 stops filter construction even though the whole builder
returns nonzero code. Those finite examples do not prove a universal filter
capacity or all caller-valid records. Aliases, valid compositions, final-source
remap ordering, register allocation, VU/FCR scratch, budgets and continuation
projection remain explicit gates before general runtime admission.

The combined disposable EE/IOP/static-reference build passes both targeted
90-frame cases. Its first full-recording attempt hit the runner's600-second
limit before a clean stop. It remains REFUSED: no complete 1,519-checkpoint pass
is claimed. That attempt reported no EE/VU misses, replay-difference diagnostics
or input changes before timeout; those absences do not turn it into a pass.
A driver-only --timeout-seconds option now retains the600-second default and
admits1..1800 seconds with explicit validation. This changes the deadline only,
not required frames, checkpoint counts, miss/exit checks or input ownership.
Default and1200-second self-test modes pass the earlier plants; zero, negative,
excessive and shell-like noninteger CLI values refuse before output or game
launch. The exact same PE/inputs are rerunning with a1200-second limit in a new
owned directory. The first failure is preserved unchanged.

| Evidence under R192 build/oracle-telemetry | SHA-256 |
|---|---|
| emitted-effect-schema-04/RESULT.json | 753b724ed3317f35a70b85eb562a51efe347d043219156349ece075723cc76ff |
| r192-merged-reference-preferred-90-01/RESULT.json | dc4ab4eaa1a128e477884affe458555060a90b7067d63fbf9c85bc040f924ca1 |
| r192-merged-reference-iop-90-01/RESULT.json | 74dd3d9d8fc0b19b21aed58bd3846e2293c91bcb5e40ccc2ffc0185113352cb5 |
| r192-merged-reference-full-replay-01/RESULT.json (REFUSED) | 6d3b6159d4efc8c9f931199185b299e67de2b48ed3393f519384d0bbf530a5dc |
| r192-replay-timeout-controls-01/RESULT.json | 3b21b82a0af9b1fde974b758e9ec4eb46cc2bde0996743337edc00a2dfcc187e |

The schema tool and diagnostic driver change belong only to the R192 branch.
No wider semantic backend, production route, native credit or physical FPS claim
is introduced. Full native-foundation code/baseline remains unchanged. The full
99-payload VU native batch, complete original producer semantics and IOP export
lifetime work continue from original sources. Normal launcher/build, original
R192 executable, original inputs, persistent card, saves and settings are intact.

### Complete recorded merged replay and wider typed resource model

The merged disposable candidate a221251e7e2a8806095ba14f88c5d3cffe2ad16c834b51942dceb3d7093dfd53
links the measured typed EE objects, source-derived static EE/VU archive and
strict original IOP compiler/loader/service42. Both targeted90-frame cases pass.
Its second full recording run completes 181224 -> 204009:22,785 frames, all1,519
checkpoints equal, zero EE/VU misses, no compiler/window/audio or replay-difference
diagnostics, exit0 and no input changes. Elapsed headless time is650.5017608s
under the1200s limit. The earlier600s refusal is preserved. Different candidates
and concurrent compiler load prevent treating elapsed time as headed FPS proof.

The long recording reports ZERO preferred typed-family entries/resumes/completions
and15,421 unadmitted invocations using existing compiled EE bodies. That counter
counts invocations, not distinct code families. The earlier80225-state90 test
is the evidence for50 actual preferred typed calls; the full recording does not
add typed-call coverage. It does exercise the merged IOP/static-AOT connection.
No normal executable/launcher is replaced by this test build.

The next isolated native resource pipeline now passes276 whole original-builder-
generated programs and4,968 comparisons across three numeric/prior-flag domains.
These compare all RAM, ordered address/width/value/write order, accepted return,
scalar FCR/accumulator, full VU through OFFLINE source-assigned bindings and exact
no-yield budget. Cases include all20 tag identities/five classes, all120 ordered
ID pairs with/without one filter, class2 destinations, reversed scalar bindings,
all-five-callback compositions,13 updates, reversed attributes and an early17
followed by changes to its source. The latter proves remap reads the final source
value. All4,968 insufficient-budget cases refuse BEFORE writes;11 invalid-domain
controls and six semantic/output/VU/budget plants reject.

The source-derived cost is P+4+N*(B+2)+A*C, with coefficients from original loads,
stores, budgeted PREFs, prelude/callback operations, filter branches/delay slots
and accepted remap/stores. The earlier16+18N+16A is one configuration. This core
has native spans/logical resources, no Cpu/PC/register bank or opcode execution.
It is still bounded: unique tags, two streams, nonnegative nonwrapping signed16
offsets/alignment, aligned64, one filter, one tag257 remap and proved register
allocation capacity. Per-producer-block capacity remains a separate admission gate. Full-Cpu mapping, true continuation/scheduler/lifetime and actual wider
runtime admission remain open; the installed narrow adapter is unchanged.

The IOP Register6/Release7 ownership MODEL also passes80 complete original-method
comparisons (20 cases x two LOADCORE variants x actual interpreter/AOT), with288
ordered metadata writes. All eight compiled semantic mutants reject. Original
Register6's584 bytes and Release7's192 bytes are pinned; release returns-213 for
not-found and-215 for in-use, unlinks the exact table and restores canonicalmagic.
The typed model owns Table/Client lifetimes and callable identities; guest record
projection is test-only. Existing/pending clients are explicit seeded conditions.
This does NOT admit Link/UnLink/unload, IRQ atomicity, arbitrary table pointers,
code-owner ABI, metadata permissions or a runtime install. The next work proves
exact client/header/stub transitions and prevents executing metadata as code.
No broad .text exclusion is permitted.

| Evidence under R192 build/oracle-telemetry | SHA-256 |
|---|---|
| r192-merged-reference-full-replay-02/RESULT.json | f38ae697298660a97d32179b96a04b1afa9cef47b8b80b1c4aed6aa619d7f7bd |
| emitted-resource-pipeline-03/RESULT.json | 6a22a60c45bd46e78b00a737f519015809de187055fb2aac00bd8025099769ab |
| r192-iop-registry-model-02/RESULT.json | 60d6aba576ee01fbab57317372afdecfaaf313c0749901dd584012719fc6818d |
| r192-iop-registry-model-freeze-01/RESULT.json | 539b9a515055d5612c5ad719afab5f63949d7b6fe72ad1fc5acfcd33a268d51b |

The merged build receipt is build/r192-merged-compiled-reference-01/BUILD.json,
SHA7324166f0551d1feef7bdb0ad0dcdd82a099ec66a3c380e6aebfe033bb59b7f9.
These authored source files stay on the R192 branch; main receives the evidence
record only. Native foundation/baseline and2/238 credit are unchanged. All original
executables, inputs, cards, saves, launchers and model settings remain intact.
Full VU native compilation and unseen-producer/lifetime closure continue.

### NETR seed correction and standalone NTGUI native body compilation

The old catalogue-all-static study generated NETR automatically but left46 of
1,834 catalogue entries uncovered. Controlled generation now isolates the cause:
its seed file contained ZERO NETR seeds. Either old or new authenticated generator
with the new1,834 seeds maps all1,834; the new generator with the old seed file
still maps1,788 and misses46. This is a seed-policy omission, not evidence of a
new decoder/reader fix. Native-ledger kind 'excluded' meant feature scope; it did
not mean non-executable data. The roadmap now explicitly separates offline code
inventory from feature/credit policy and requires actual entry-map checks.

The first new study accidentally reused that in-scope kind filter. Both generators
succeeded, but its NETR measurement lookup then failed because the excluded rows
were absent. That initial source/log/failure snapshot is preserved in generation-01.
The corrected study includes callable AND excluded rows and leaves the one known
descriptor row separate. No ledger row, denominator or online policy is changed.

Read-only original-disc extraction authenticates GAMER/MOVIER/NETR and NTGUI by
file offset, size and SHA. The corrected source study generates NETR with complete
catalogue entry mapping and standalone NTGUI with59 units and28,858 active PC
entry slots, including its actual ELF entry001056A8. PC entry slots are NOT a
function-ownership count. Every nonzero slot is joined to its actual generated
C++ label. The committed replayable map falsifier admits the authentic map and
rejects an unmapped entry, wrong extent, wrong target and dropped case label.

Both extra images now have offline native body archives: NETR37 units and
NTGUI59 units (96 compiled objects total). Every source/object hash is retained;
archive member lists match exactly. The source/native archive study is separate
from linking/registering these images into the game, proving dynamic targets or
executing them. No NTGUI/game/network run occurs in this phase, online feature
activation remains excluded, and no additional native credit is earned.

| Evidence under R192 build/oracle-telemetry | SHA-256 |
|---|---|
| r192-remaining-disc-images-generation-02/RESULT.json | 9d2970cc18c1c601d715634ec70f8677967036e660605be7a582529784f3e403 |
| r192-remaining-disc-images-native-01/RESULT.json | ab3c904ee1fca1ab9269d2149dd0d225a5faaeee6b407087e9758e8a607ad252 |
| r192-NETR-generation-factor-controls-01/RESULT.json | a6ca5123651d7d0d56f20bd55ecfd4acafd167b3153bde9bcf61ddef2415ab55 |
| r192-disc-entry-map-controls-02/RESULT.json | 478a3ec1ddd6db5f38aee0eef09ac5c53843d0143a56954d49241df64949d3d0 |

A separate producer finding narrows the previous resource-core admission wording:
its proved allocation check covered REGISTER capacity. Original2EDA70 allocates
seven separate blocks with initial word capacities16/8/20/64/12/24/16; the final
2000-word assembly buffer is a different allocation. Per-block append/growth and
length bounds need their own source proof. The measured276 programs/4,968 output
checks remain valid; their logical count limits do not admit all descriptors at
runtime. True continuation/projection/lifetime and producer-capacity work continue.

The full original VU native batch is still running. NormalN882302..., original
R192 executable/launcher, original inputs, persistent card/saves and local model
settings are unchanged. Native foundation/baseline and2/238 credit are unchanged;
the new source/object inventory never promotes catalogue or native tiers.

### Wider typed continuation, producer layout and IOP client lifecycle

The wider resource continuation passes276 original-generated programs,4,416
constructed sessions and9,936 suspend/return boundaries. All5,520 true suspensions
and guard callbacks match;4,416 completed-context reuse attempts refuse. At EVERY
boundary it compares all RAM, ordered writes, FCR, scalar accumulator, full VU
and budget. Eleven overflow/service/reentry/failed-context/zero-count/snapshot
controls and five semantic plants reject. Guards are constructed budget-refilling
services here. Full Cpu mapping, actual scheduler/global Cpu, saved-state
rehydration, code-buffer ownership and wider runtime/lifetime adoption stay open.
The current exact-family runtime adapter remains unchanged.

The separate source-only producer/allocator audit matches281 original constructions:
277 intact-block layouts and four positive-return partial emissions. Exact6,425
original node rows/physical slots/stream references/offsets match the source-derived
model;1,069 fresh instruction words join the original ELF. Independent banks have
ascending VF/FPR allocation; GPR32/GPR64 share the original GPR pool. Internal
retention node tag358 uses bank2 but explicitly performs128-bit transfers. Bank
number alone does not establish width.

In [constant-load32,multiply36,data40,main28,filter44,accepted48,tail52] order,
word capacities are[16,8,20,64,12,24,16]. The matching length formulas are
[V+S+4R+F+Z,V+S,aligned constant data,6+L+Q+effect_ops,3F,1+L+Q+6R,8+R].
L is attribute count, Q tag259 retention, V/S vector/scalar constants, R remaps,
F filters and Z the shared scalar zero. Vector data aligns4 then appends3 words;
scalar/remap/filter data append1/4/1 words. Constructor node count is
13+L+Q+V+S+Z+5R+F. These are authenticated original-layout observations and a
bounded reference model, not a runtime opcode decoder or native register bank.

Four source-supported partial cases remain required open domains: multiply9->8,
data23->20, constant-load17->16 and accepted/store28->24. Every original whole
builder still returns positive. They are NOT impossible/invalid descriptors.
The native core does not implement their omitted operations/ambient-register
behavior. A large source case uses53 main words; no fictitious main64 overflow
is claimed. Observed append helpers drop rather than grow and capacities stay
fixed in these cases; complete reachable-instruction no-growth proof is kept
separate. A failed first layout model used the wrong failed-append offset sentinel;
its exact old source/failure is preserved, and final04 matches the original
FFFFFFFF sentinel.

All99 vector fourth fetches in this cohort are classified:45 following appends,
14 unwritten pool alignment,39 outside copied-data allocation words and one failed-
append partial case. Ambient words are explicit inputs/unadmitted producer
bindings. Neither zero nor descriptor+28 is assumed. Stage A may bind a value
from a proved original read/state once address/owner/bounds/preparation lifetime
are established; Stage B ownership is a separate requirement.

The native IOP registry lifecycle model now compares29 source cases against two
LOADCORE variants in interpreter/AOT modes:116 complete method comparisons and
448 exact ordered metadata writes. Genuine original Link8/UnLink9 cases cover
inline import records, active skip, second client, interior/head unlink, relink,
missing provider(-1) and pending unlink. Epoch/stale-client and observer allocation-
failure poison/rebuild controls pass; eight compiled semantic mutants still reject.
The original SDK C is supporting evidence: its import-record indirection and
clean-stub condition differ from verified original instructions, which remain
the authority. Runtime metadata/stub permissions, IRQ/ModuleOwner restoration
and installation remain open; the new static guard is not included in this phase.

| Evidence under R192 build/oracle-telemetry | SHA-256 |
|---|---|
| emitted-resource-continuation-02/RESULT.json | a4f3166fe1812d298977f68bb5dfc7808476bdcb54a0a98d9122f8ca15c1edc2 |
| emitted-producer-layout-04/RESULT.json | a4fa1d05c3f8136d17c799cc4b2debc5593ec5691264bd691b1c7696ed6b13c3 |
| r192-iop-registry-lifecycle-01/RESULT.json | b9deecbcf9b390ff7e305aa719076082d2c04bc3ebbc7f98e5c32cf3162d6785 |
| r192-iop-registry-lifecycle-freeze-01/RESULT.json | 7ce4277486ddd2495dc89e49a82cc5ed607d135d74a7fa8de1666494ade9023b |
| r192-merged-reference-probes-01/RESULT.json | 598ea69f236acbc0029c1ac5abf32cc95cd0877b69879c1c4fc2f7778d44d90c |

The merged PE also passes the14-state diagnostic suite:12 fresh90-frame cases
and two explicitly reused exact-PE/input90-frame results,1,260 measured frames
across the suite. This adds saved-state coverage, not whole-game/image acceptance.
The full22,785-frame/all1,519-checkpoint replay remains separately proved.
All authored code stays on the R192 branch. No model is silently installed into
the normal game. Native foundation/baseline,2/238 credit, original assets/builds,
launcher, persistent card/saves and model settings remain unchanged.

### All original-upload VU entry modules connected and replayed

The complete99-payload offline native batch finishes successfully:65,924 native
entry instances /65,026 unique keys. Every DLL passes source/ABI stamp, exact
key/count/table bounds and executable-image checks; source bodies remain joined
to the pinned original upload data. This is the conservative uploaded-pair entry
superset, not65,026 identified game functions. The112 source-unwritten reachable
candidate refusals,134 legacy-only derived keys, provenance limits and unproved
upload/patch contexts remain separate. No original input was changed.

The full package is sealed by manifest SHA-256 and exclusive verified hardlinks,
never opened for writing. The producer and connector reuse the same immutable
native files without making multi-gigabyte duplicate copies. Pilot mutation
fixtures still use independent copies. The native startup locks every DLL,
authenticates all files/ABI/keys/functions, deduplicates identical source keys and
registers one65,026-entry table only after validation. Old static VU function
bodies are absent from the new PE's symbol table; VU Live is not its loader.

Separate candidate25d6ae51cdbb8e88b95118ba5a4bdfbf466b50f0ef7ed7e6d3bd3c417419e761
combines that VU set with the previously measured static EE, strict compiled
IOP and exact-family typed EE objects. Actual startup registers65026/99 VU and
3569/3569 EE. Its complete181224 ->204009 recording passes22,785 frames/all1,519
checkpoints, exit0, zero code-miss/compiler/window/audio/difference diagnostics
and no input changes. All14 saved-state cases are FRESH90-frame runs on this
PE (1,260 additional frames); no old-PE result is reused. The earlier merged
1175-key result and its600s timeout stay preserved, not overwritten.

The driver now pins the VU manifest/all99 DLLs before and after each run, requires
the exact single startup registration summary and refuses an untracked/missing/
duplicate/wrong-count registration. A VU miss tag cannot be hidden behind a clean
stop. Earlier seven plants/checkpoint plant still reject; five registration/miss
plants and four malformed-manifest controls reject. Native ABI/table checks are
the executable initializer's gate, separate from the driver's file/summary proof.
This build is not installed by the normal launcher. VU0 micro interpretation,
other upload sources, unseen generated EE domains/partial emission, actual IOP
lifetime/IRQ and final native memory/services remain open. The test is not a
physical input/audio/image capture or whole-game/native acceptance.

The separate allocator-derived reference projection now matches all1456 Cpu
bytes at9,936 boundaries over276 constructed programs/4,416 sessions/5,520
suspensions. Source-derived resume PC and exact allocator slots replace borrowed
reference outputs. Eight entry/data/step/stride/count64/budget32 refusals, eleven
prior lifetime controls and four projection mutants reject. Native int64 budget
must fit original int32 before projection; memcpy preserves padding. This API
copies entry state for ISOLATED bodies/budget-only constructed guards. A real
interleaved adapter must patch only source-defined effects onto CURRENT state:
ThreadContext does not restore base/VU/cop0/PCR/timestamps/budget. Actual chosen
transport, shared-VU/private scratch, global scheduler and code-buffer ownership
need separate proof. No generic runtime replacement is admitted by this test.

The new isolated IOP metadata guard authenticates70 original images/598 static
records (73 export tables/525 client tables), including relocation-based function
pointers. Original6/7/8/9 execute on actual LIBSD/SDR records in both reference
modes. Exact expected mutable words update only through checked native-model
writes; immutable names/versions/function slots and code remain exact. Canonical
import schema words stay separate from live SDK J/original-ordinal-ADDIU words.
PC-as-data, stale owners/records and eight explicit field/pointer/slot/version/
name/function/code/live-thread plants refuse; seven guard-policy mutants reject.
Twelve full-Cpu stub comparisons per mode cover physical/KSEG and zero/split
budgets. Init threads are cancelled for this finite metadata-method fixture;
this is NOT module boot/unload/RPC/IRQ/restore proof or runtime installation.
An initial failed guard copied live words over canonical import schema; it is
preserved. The corrected guard does not weaken existing valid/matches.

| Evidence under R192 build/oracle-telemetry | SHA-256 |
|---|---|
| r192-vu-all-entry-native-modules-01/RESULT.json | 1de954e567e21f6f41d5d7aa430d89103ac42c48072d517dad53cc1d575693fd |
| r192-vu-complete-sealed-package-01/PACKAGE.json | ba3e998745f9f4f8098863b95d63fbef67a4e4c57eb5e0154d630f92cff8dc16 |
| r192-complete-source-vu-startup-01/RESULT.json | 1f48490008d1d954f2c665ab7a5c70f61e46bad02f0831a5d3c30c99da8bbaef |
| r192-complete-source-vu-full-replay-01/RESULT.json | 6f0360fd78e579005d7a39d8a313385f1a66cbeac62a82a3d4821e96a4a3ba84 |
| r192-complete-source-vu-probes-01/RESULT.json | a0a2af44042403da9b87ce02c657212b9407ba17dbf205d42291fea0a88164e6 |
| r192-replay-sealed-vu-controls-01/RESULT.json | 7a2f129636b001d72bfe8b2e3be42ee2ab7b331c7b6021b0cb9929f7a9628038 |
| emitted-resource-full-projection-03/RESULT.json | 39cb6e2801d9a17f69d0485a33cf962b8c1b015d00548be8df6314713d4e5219 |
| r192-iop-static-guard-02/RESULT.json | 81be4629234ab5fc967aee16091833143db1694c9300f4f63effd8610ced90e0 |
| r192-iop-static-guard-freeze-01/RESULT.json | b0505b1d82fb9fdc5d653c1162fb0c3d4bb7cfbbe330ceb153e24c599f62a6f6 |

The compiled candidate build receipt (build/r192-complete-source-vu-candidate-01/
BUILD.json) is188612e8f4f17ce49af807db288eb191037ec8efcb5ccbcca3cf3784221afc36.
Native foundation source and its59-target/65-variant/16-gate baseline are unchanged;
affected reference/component checks above are separate. Source experiments stay
on R192; main receives synchronized status/evidence. Native credit remains2/238.
NormalN882302..., originalR192 executable/launcher, original assets, persistent
card/saves and local model settings are unchanged. Universal closure stays OPEN.

### Verification and continuing work

The full native baseline was freshly rerun: 59 targets, 65 variants and 16
gates pass, including expected negative exits. All protected hashes checked
by that runner remain unchanged. Whole-game acceptance and strict ownership
census retain their expected open/refusal results. Native ledgers remain
unchanged; these R192 reference-domain counts do not alter native tiers.

The EE family and IOP implementation work continues independently. Completed
component receipts must retain their actual whole-body scope and limitations;
one emitted stub family or one IOP export is not complete domain closure.
No original executable, launcher, asset, persistent card, save, key or local
model setting is modified. All tests use owned companion-card copies, with
no GUI or audio endpoint.

| Evidence | Path from its repository | SHA-256 |
|---|---|---|
| Static overlay generation | build/r192-static-overlay-stage-05/RESULT.json | 215e9586c08523e3082822e68736beb745e665fe8aded8e4b805a9feb70efeca |
| Static candidate compile | build/r192-static-compiled-candidate/RESULT.json | 1cfb0b2dbe662da6d2425b369a20888702f31faa356d30addc31f2cf0b0cea39 |
| Static full Konquest replay | build/oracle-telemetry/r192-full-static-replay-01/RESULT.json | 3cbd5a3a47f8b6095bedf53ea2ff7c8a0867cc6d886d6dbf4fa29f8caca30889 |
| INVALIDATED speculative VU generation | build/oracle-telemetry/r192-elf-vu-04/RESULT.json | 8c05157c65de120cbb3fe0eecb266b9336638d15bba85422ee27a3a91aa69079 |
| INVALIDATED speculative VU compile | build/r192-elf-vu-candidate/RESULT.json | 6e2c270b2be37820bb5c611bb87c210f39ede6c231bd4dfb087a9d41eda10a08 |
| INVALIDATED speculative VU probe | build/oracle-telemetry/r192-elf-vu-frame90-01/RESULT.json | 0ebfcafc5ef56d2ce038ed0dad385b40b188e3c9faaf73c2edb73347b8dd06c4 |
| Clean pinned VU input generation | build/oracle-telemetry/r192-clean-vu-02/RESULT.json | ebeedd67ecc4f6e8fcf97ae54730aef715f8105b6ff8a6099d89ba7eb0db3368 |
| Source-bounded original VU generation | build/oracle-telemetry/r192-elf-vu-source-bounded-01/RESULT.json | 559a33df4f952bd8d39ebe128ab92c2c83ba76c46604d1243dfaaaf7e300432c |
| Corrected fresh candidate compile | build/r192-vu-source-admitted-candidate/RESULT.json | 057c413e0b279e8e947a421a01b920efad8131af945e08e24712bbe49305f749 |
| Corrected full Konquest replay | build/oracle-telemetry/r192-source-admitted-vu-replay-01/RESULT.json | ed48abe5f7c9c49f58dee5a9c20a4924aee7cc923e86409ba427c8e4844e98b9 |
| Whole original-word join | build/oracle-telemetry/r192-vu-origin-01/WHOLE-SOURCE.json | 69dfea6d6cfb69964370b3b1d5a89f08d81a1dd1f9564c1793e40544182a1b07 |
| Fresh native baseline | build/oracle-telemetry/complete-baseline-20261125-223027-30e47a1a/RESULT.json | 0958032277e1fb6c538f7494834146b87f1a893481b1fa391b45d9a0c28bcaed |
| Read-only Ghidra provenance | build/oracle-telemetry/r192-vu-static-payloads-01/GHIDRA.json | 77ddda49d2481d6c55c48e589c14661b434e6f68e8d39620afdf6751d2b721bc |

Historical final-tool replay receipt build/oracle-telemetry/r192-root-source-final-01.json (5ff592028f2c965951f95ef631e03cc543f97fcaeb690083327748b3901c0cbb) established reproducible generation for the earlier experiment. It did not prove target provenance; its speculative extension was subsequently invalidated above.

### Not verified

No physical headed gameplay, audio-device run or independent held-out session
was produced by this integration. Unrecorded future code paths, complete mode
coverage and the old finite-template/55-dump questions remain unproved.
The installed R192 route has no EE/VU live compiler, but complete precompilation
remains OPEN. The separate static/VU candidates do not prove all emitted EE
families, VU payload variants or IOP paths, and are not deployed by this study.
VU0 has no VCALLMS in the inspected generated corpus/listing; VIF0 and dynamic
emission prevent inferring universal absence from that observation.
The corrected 1,033-key candidate still contains the reference IOP interpreter
and VU0 micro interpreter. IPU is compiled host decoding with a device command
model; it is not uncompiled EE code. These are separate remaining native-service
obligations. The recomp retains console memory/device models and is not final native-port
acceptance. Ownership admission and native credit rules are unchanged.

### Contradictions with earlier documents

The old R192 executable used the old prebuilt backup library despite later AOT
generation; the current build links its own generated library. Old numeric hook
IDs no longer selected the intended routines after regeneration; current IDs
are rebound from actual maps. No replacement memory card is needed or deployed.
The old 14-versus-61 duration findings remain distinct: the historical 61 closure
logs each ran one frame, while every new14 probe here ran 90. The original 8ab
reference remains preserved, but the familiar R192 shortcut selects the compiled
882302... executable. Archived native-GL scripts are historical diagnostics.
The 13 speculative VU additions and inherited rejected capture were invalidated
by original-byte provenance, despite compilation/replay checks. The correction
requires input pins and per-entry uploaded-byte bounds before code admission.

### Falsification attempts

Named refusals cover forged manifest/DLL hashes, wrong exported/source stamps,
wrong ABI, out-of-DLL metadata, active padded entries, second-DLL failure before
any registration, malformed package metadata, wrong frame duration, nonzero exit,
timeout, ambiguous state loads, miss-file-only failures, missing checkpoints and
inconsistent VU function/table unions. Legitimate unsigned wrapped/aligned unit
padding is source-proved and tested; it does not admit active out-of-image entries.
All raw failures remain saved. No general transformer/native credit was inferred.

### Whole-disc structured upload search and actual IOP ownership inputs

The source search now covers all 105 disc files (3,633,049,333 file bytes), including the packed resources, rather than only the retail ELF or played states. Exact aligned DMA RET/END plus MPG recognition found 99 instances / 99 image-and-source-bitmap pairs: all 99 match the original ELF inputs already compiled. No new pair or malformed matching header was found. The five parser/coverage controls pass. This establishes the result for that exact format; mixed command chains, raw bus uploads, compressed payloads, patches and generated uploads remain separate open domains. No launch unit is inferred from an MPG record. Scan01 was the same successful search before replacing the ISO hash read with a bounded 4 MiB streaming hash; scan02 replays the final source.

`r192-whole-disc-vu-scan-02/RESULT.json` is `c6501ec3b2df030eb31b86fc1d25c7ef4d7f90b277e271994f743d2e695523fd`. The replayable tool is `tools/research/scan_r192_disc_vu_uploads.py`; its receipt pins the original ISO, prior structural census, complete original-upload generation and parser bytes. `r192-complete-source-vu-candidate-publication-01/SOURCE_PUBLICATION.json` (`fd5cebb304c38fb7272277d9ed39dc657612154e9368ae8d55aa3cfa2855c09c`) joins the previously measured candidate tools to identical blobs in published fork commit `e4668d8c721b03740063a70ddf839e5630f78a8e`; it does not claim a new clean rebuild.

The new IOP helper links the actual pinned v8 serializer and original kernel, library and SIF implementation in separate translation units. It reads 14 owned state copies through the byte-loading overload; it does not boot, execute callbacks, mount media/cards or call the path-loading overload. Every state retains 15 module records, 78 allocations, 52 threads, 5 IRQ records, 2 vblank callbacks, 1 hard timer, 12 guest RPC servers and 9 provider-map entries. All 126 provider headers have canonical magic and zero SDK client links; DEV9/USBD module ranges overlap in all 14 states. Range membership and the restored host map therefore cannot prove a unique owner or reconstruct SDK registration history.

`r192-iop-lifetime-inventory-02/RESULT.json` is `e7fbbfc90756ec6b9d26804a63d388b886dc709534a0425cae88d5ced53fdf87`; the exact 14-state CLI and frozen two-source hashes are in `r192-iop-lifetime-freeze-01/RESULT.json` (`7288f8751cfd6090f702bcac980074d5515e13fcaad70ec934e781e547abf14c`). Three compiled format/truncation refusals and four analysis invariants pass. Inventory01 is an earlier one-state PASS, superseded for population by02; it was not a failed run.

The next source boundary (`r192-iop-owner-generation-source-01/RESULT.json`, `7b022bb11cc2df1557956e82895dbd75a2501de4141019febbd79f277123eb6c`) establishes that fresh loading has raw bytes before relocation, while state restoration mutates progressively without rollback. A native owner connector must authenticate the original input and loaded identity, invalidate generations before restore/reset, and poison failed restoration. Allocation erasure alone is not safe module retirement. No SDK root is invented from the host provider map; IRQ/context, boot, unload, opaque references and runtime transition admission remain OPEN.

These are affected discovery/ownership checks. The unchanged native foundation retains its59-target/65-variant/16-gate baseline, and the complete-VU candidate retains its22785-frame/all1519-checkpoint replay plus14 fresh90-frame probes. No original executable, normal launcher, persistent card/save, ownership rule or credit changed (2 transformed /238 native). Continue the typed EE partial-emission paths and actual scheduler shadow, and the bounded source-authenticated IOP owner generation connector.

### Source-positive partial EE emission has a typed output implementation

`partial_resource_pipeline.hpp` implements the observed original sub-block capacity policies without a CPU/register bank, executable word stream or runtime compiler. The original measured node order selects the constant-load prefix. Only the first8 constants are multiplied; a ninth loaded constant stays raw. Failed vector appends use an explicit, proved full128-bit fetch (the tested sixth vector fetch is the aligned data-base-minus16 block), never nominal descriptor words or assumed zero. Omitted filter loads require an explicit caller input or refuse before writes. Accepted output preserves the source-truncated attribute-store prefix and the separate tail remap store. Timing counts only filters actually visited before short-circuit rejection.

The replayable verifier regenerates all four original positive-return partial programs (multiply9, data23, load17 and accepted28) plus the intact53-word/four-filter control with the pinned original generator. Across zero/one/eight elements, separate/overlapping streams, three numeric patterns and three initial omitted-limit values, all270 whole-output comparisons pass: all32MiB RAM, ordered write address/width/value, return count, FCR31, scalar accumulator, fullVU and exact no-yield budget. There are324 prewrite budget or missing-filter-input refusals. Seven planted errors are rejected: extra stores, an extra multiply, an invented fourth filter constant, wrong failed-append vector input, charging unvisited filters, reordered writes and changed flags.

Final receipt `emitted-partial-resource-pipeline-01/RESULT.json` is `15ec24a6dbb3479c1aafda6d476d60080e43f764a44fa00f0b3562b6bfaffac4`; source hashes and complete CLI are frozen in `emitted-partial-resource-freeze-01/RESULT.json` (`70c2872054fd31cd7565e0a666f9f13bc410d5d2d1bc426ccc7b43604b696083`). These are constructed descriptor cases; the reference is the original builder output, not expected values copied from the new algorithm. Actual caller data ownership, ambient input provenance, suspension/continuation and current-Cpu projection remain unadmitted for this partial extension. No runtime path or protected file changed. The earlier record that these four policies were unimplemented is superseded only for the tested typed no-yield outputs; universal generated-family coverage remains OPEN.

Native credit stays2 transformed /238 native. The unchanged native foundation and last complete-VU replay/14-probe checkpoint retain their distinct scopes; this isolated component does not trigger a new normal-game acceptance claim. Next connect typed partial continuations to explicit guard/lifetime contracts and verify actual EE scheduler shadows, while the separate fresh-load IOP owner connector proceeds.

### Partial EE algorithms retain typed continuations across source guards

The separate partial continuation owns the typed prepared values and cumulative algorithm results. It resumes after the pending true guard, preserving the original load/multiply truncation and snapshotted ambient inputs rather than replaying the prelude. Source filter short-circuit cost, accepted-store truncation, prior remap results, scalar flags and vector sticky flags survive suspension. The continuation has no Cpu/register frame, source PC, emitted operation stream or runtime compiler. Recursive use, missing guard service, completed/failed reuse and budget underflow refuse by name. A separate reference-only projection expresses the original machine widths/untouched upper halves and the clamped source layout. That projection is for isolated bodies with budget-only guards; it must never restore unrelated global state after actual scheduling.

All five original programs (four positive partial shapes plus the intact four-filter control) pass1620 constructed sessions:4410 suspension/return boundaries compare all1456 Cpu bytes, all32MiB RAM and ordered writes. The sessions include2790 actual suspensions under constructed guard services,5400 guard callbacks, true/false/alternating callback outcomes, guard equality, zero/three/eight elements, aliases, numeric patterns and omitted-input values. All1620 completed-context reuse attempts refuse. Four compiled mutants reject: guard equality changed to<=0, repeated prelude after resume, loss of the earlier remap value and loss of vector sticky flags. These are constructed guard calls, not claims about the native game scheduler.

`emitted-partial-resource-continuation-01/RESULT.json` is `3e462d407a7e52cc4f289cc36d708d565fbd685011cef5aa20caba8cec73f00a`; complete CLI and three frozen source hashes are in `emitted-partial-continuation-freeze-01/RESULT.json` (`ff4db82c597a67368ad9be7fa19374459865a3db4c39ca4eff912e82f35f1400`). The new files are `partial_resource_continuation.hpp`, `reference_partial_resource_projection.hpp` and `verify_r192_partial_resource_continuation.py`. The previous typed partial output sources and existing intact core are unchanged. Actual caller/code/data owner lifetimes, scheduler side effects and current-context projection remain OPEN for partial policies; no normal runtime is changed or accepted. Native credit remains2 transformed /238 native.

### Actual EE guard shadows and fresh IOP code-owner generations

A new observation-only disposable host captures AFTER actual EE module selection and BEFORE the original compiled callee, preserving selected Cpu.base. Two90-frame original probes yielded four completed captures but only THREE distinct logical plans (the fourth is a relocated repeat). The80225 probe also records22 orphan-capture refusals/81 duplicate-plan invocations; the181224 probe records450 caller/object/frame refusals. These are invocation diagnostics, not counts of distinct code families. Three naturally low-budget calls were then selected without changing their budget, and each suspended, naturally resumed and completed with two actual preempt calls.

Original compiled builder regeneration matches all155 executable words in the four no-guard captures. All four typed shadows match full1456-byte Cpu and32MiB RAM, with6 planted errors rejected. The actual guard study matches three logical plans across6 segments and6 actual service boundaries; one consumed48 elements and accepted40. The original service changes6 RAM bytes across the three true callbacks and none in the three false callbacks. These are explicit observed service boundary outputs, not an ordered store trace or a native scheduler implementation. Algorithm output/state is calculated independently; reference post-Cpu never fills algorithm state. Untouched CURRENT base/cop0/time fields remain current. Private/sharedVU or owned stream/frame/code/descriptor interference refuses.

The final event verifier retains JRRA plus its delay NOP inside each executable-word comparison and proves all9 static branch targets remain inside. Only the two original after-return NOPs (and two vector alignment words where present) are excluded. The tag259 retained transfer has its own aligned16-byte ownership extent, including an independent offset12/count1 control; its declared scalar resource width does not bound the retained quad. Eight final event plants reject, including the missing retention gate. The new pure typed event-continuation header also passes the prior276-program/4416-session corpus at9936 full-state boundaries/5520 constructed suspensions, with5 compiled/callback plants rejected. The earlier event01 compile refusal and event02 result are preserved;03 supersedes02 for tail/retention gates.

Final EE identities: no-guard shadow `emitted-resource-actual-shadow-01/RESULT.json` = `937a4bcfe06ddac027f0caa82fde3554f43e4139164e8d97c5d86d489fb3b81e`; actual event shadow03 = `545e3f67e0dd9a13bf3501d4cec1a4a3e7728c5a10852c630a29a558788ca496`; whole event regression01 = `aacabd77fd2b45d80aa7c4229afa7f22ee4425a2e038ee55be417cc4d5a6c1df`. The five source hashes and exact CLIs/receipt pins are frozen in `emitted-resource-actual-event-shadow-03/FREEZE.json` = `c031c2c60bf1e829808cb419b5940cc40a1a8099d8426f535b6aab94164cdf76`. Actual store order, generic scheduler effects and runtime admission remain OPEN.

The separate IOP owner connector hashes complete original IRX bytes with native SHA256 before the actual loader; it joins the exact PIC/schema, loaded id/base/size/entry/gp/name and executable export slot. It invalidates generations BEFORE progressive state restore and poisons failures. Fresh admission and every revalidation refuse overlapping other nonzero module ranges, including the actual free-then-same-bytes-loader-reuse case where old module facts survive. Entry validation returns owned identity-proof values, not a borrowed Module or source-word reference that could dangle at restore. Restored maps admit zero owners; their overlaps remain ambiguous observations and no SDK root/history is invented.

All7 IOP comparisons match21 complete CPU/RAM/serializer artifacts: LIBSD/SDRDRV each via actual filename and buffer loading, two actual byte-restores and a deliberately truncated final-tag restore. Ten refusal scenarios contain11 exact checks;4 compiled mutants reject (retained old generation, missing failed-restore poison, bypassed complete raw hash, ignored overlap). Fresh images expose34 and6 authenticated export identities with98 repeated slot checks; no guest instructions or module entry are executed. Static catalogue coverage of70 images/598 records does not promote the actual two-image result to all-module behavior. Generic retirement, IRQ/registry integration, callable ABI and runtime adoption remain unproved. The earlier owner01 compile problem (literal empty MSL name),02 truthful test-name mismatch and superseded03/04 safety revisions are preserved.

Final IOP `r192-iop-code-owner-05/RESULT.json` = `6bb83e2f228e13e6d85ce1f03d475a825a8ba506bf0da7177d8f37f9a5b90900`; three-source freeze and exact CLI `r192-iop-code-owner-freeze-02/RESULT.json` = `5599bb35dad7d7ef0abb0444662fb8d35df6e6bf8322ce16a89aa30ad0c8b99f`. Scope includes only observed context mutations routed through this connector, not unobserved external resets/frees.

To preserve the12GiB workspace reserve,32 completed owned observation RAM files were transparently NTFS-compressed. All paths/logical bytes/SHA256 hashes are unchanged; no raw artifact was deleted. `r192-owned-ram-storage-01/RESULT.json` = `7510a4228ef895ade2644e073c8bcb5ff28f7dcb53c747971d814cdc44860be2`. Original R192/N/card/saves/settings remain untouched. No native/transform credit changes (2/238). Next build a bounded actual preferred EE adapter for precisely the three source-authenticated plans, and extend fresh IOP owner validation across the finite70-image domain; neither study closes universal code coverage.

### Whole original IOP owner corpus and remaining static-image link closure

Fresh source authentication now covers ALL70 original IOP image bodies via both actual filename and literal buffer routes:140/140 loading pairs match420 full CPU/RAM/actual serializer artifacts. All140 owners are admitted under the fresh-source identity policy, with no ambiguous fresh-source selection or missing slot. The1960 original export slots yield3920 route observations; each exact original target is joined to its compiled PIC identity and actual generated entry-map/case label. They represent1332 distinct source-target entries and123 alias groups, not3920 functions. Twelve bodies have no export table and correctly retain zero exports; MSL uses its literal empty buffer name.

The final corpus driver independently regenerates schemas from original ELF inputs and copies its reference prefix from pinned public source. Four source/entry-map plants reject. Source01 was an earlier full-corpus PASS;02 supersedes it with stronger producer pins and exact final target equality. Final `r192-iop-owner-corpus-02/RESULT.json` = `b51e4cc111b09067300ebc54202164869107be083d697d879b186cb6602ec720`; exact CLI/two source hashes are frozen in `r192-iop-owner-corpus-freeze-01/RESULT.json` = `cf0957bb242ffaef5a585a53dd6b42690f309ff86a318f95ff18d97566220e8a`. All840 raw artifacts are retained,5,289,729,832 logical bytes compressed to13,109,920 NTFS physical bytes with every pre/postSHA equal. This does not boot a module, run a guest instruction, prove callable ABI/IRQ/retirement or infer restored ownership; those remain OPEN.

NETR and standalone NTGUI now have concrete offline native link probes, beyond their earlier96 body objects/archives. NETR requires its matching full generated main/overlay graph:244 of346 common body files differ from the installed S4 generation, including shifted entry IDs. Blindly appending its37 objects to S4 would therefore be unsafe. The new tool compiles all346 matching common bodies and both image tables, reuses only the exact measured37 NETR/59 NTGUI bodies, and links both complete images against the pinned actual runtime. In total444 generated bodies/tables plus2 metadata-test hosts are linked. No installed image is replaced.

The probes invoke only actual registry metadata registration/reading, never boot, call, dispatch or game entry. Compiled metadata exactly matches source denominators: NETR graph has311 main units/121746 active entry slots, GAMER1/3, MOVIER34/84799 and NETR37/105935; NTGUI59/28858. These slots are generated instruction/continuation entries, NOT function or native-credit counts. Both count-error controls reject. NETR feature activation remains excluded and NTGUI is not run. There are no unresolved native link symbols in these two probes; execution equivalence and game integration remain separate gates.

`r192-remaining-disc-images-linked-01/RESULT.json` = `2269db91567724335c35fb6e56f6d821078fe725f7d3aa1ee12beb9c465b126f`; authored tool/source identity, full invocation and a post-measurement recheck of every original generated source/reused/new object are in `r192-remaining-disc-link-freeze-01/RESULT.json` (`9da45f4ea832bf5a6a67329eacad24835067e1dcfec256fe209dae5e2e5e0bbc`). Linked metadata-probe PEs: NETR `434b4bd94045147d6c21d652d63dbb3cfc59d5ce0bb768e6f936a869c6f22bdc`, NTGUI `721b4cf8294c73d7cfb21d5bedcb99b384d4c1a0399cc448b201a33bee78632b`. Source-generation, body compilation, link closure and execution coverage retain separate meanings.

Historical publication point: candidate5196e815... had passed both actual90-frame probes while the full replay and14-state gates were still running. Those gates later passed, then corrected candidate6bab8b9c... superseded it as recorded below. Neither candidate is installed or accepted. Normal882302..., originalR192/card/saves/settings, ownership/credit rules and the2transformed/238native totals are unchanged.

### Review limits found before broader helper admission

Independent review found an unused-stream slicing precondition bug: continuation helpers subspan both streams even when a valid descriptor never references one, whose span may be empty with a nonzero stride. The current constructed fixtures bind both spans and the three actual runtime plans use both streams; their measured comparisons remain valid, but the general helper input domain needs correction. The partial reference projection also needs to require that an omitted-filter binding equals its original entry FPR, rather than inventing an omitted-load write. Its current vectors supply the correct entry value. Root is correcting these partial helpers and adding controls; inherited intact/event helper generalization remains open while the immutable5196 game gates run.

The static-image linker validates generated/reused source/object pins upfront but its automatic final pass currently rehashes only support pins. Publication independently rechecked every generated/new/reused source and object, so the linked01 receipt remains valid. The reusable tool needs that complete final rehash built in.

The connected-adapter control verifier also needs to require its supplied BUILD header/dependency/compiler pins. Controls01 compiled the actual03 header but referenced the older02 build contract; this is a provenance guard weakness, not a demonstrated native behavior difference. The author will snapshot the old verifier, tighten its pins, rerun against actual03 BUILD and issue a corrected test-only freeze. No runtime header/object/PE change is allowed during the full replay/14-state gates. These findings earn no credit or broader admission; raw prior evidence remains preserved.

### Corrected connected EE candidate and completed gates

Candidate04-final PE `6bab8b9cfa9f0ae28bc027841825f1ce0ebd89fd75e046fb7e189f88de7715b5` replaces only the private dispatch/resource-adapter objects in the complete-source VU/static IOP candidate. Its BUILD is `57b220ba8816b596b44f04ee467b7d90a546bd58d95e1e7dc315ee06b6917694`; its seven-source/private-object/receipt freeze is `16e155c0a2410e7efc70d82f0f4c92d8692f20dfb56761fecf7b5c14f0ebb6c0`. The package retains3569 sealed EE DLLs and65026 unique original-upload VU keys in99 DLLs. These are registry/code identities, not functions or native credit. The replayable builder is `tools/research/build_r192_resource_event_adapter.py`; its BUILD records the full compiler/link arguments, source and package pins.

Full recording02 passes all22785 new frames (181224 through204009) and all1519 recorded checkpoints, exit0,585.089 seconds. The exact PE completed2166 typed-plan invocations,126 resumes/suspensions and231 actual original preempt calls, with13255 OTHER COMPILED AOT invocations, zero orphan dispatches, failures or pending continuations. These are invocation counts, not distinct families or functions. Full replay `r192-resource-events-full-replay-02/RESULT.json` = `1d50e7f2b9256d19edb507aa8eea809c510abe6cd940abffebc2acef9ebcdd38`. All14 fresh90-frame probes pass1260 frames in `r192-resource-events-probes-02/RESULT.json` = `65fd68e98bb547563869c41d436a59972f2ad147d08f6c5e9cadd088110b24c2`. Both pin this exact PE/package, reject compiler/miss/endpoint activity and retain commands, copied cards, logs and input hashes. No protected input changed. This is connected headless recorded-corpus evidence, not universal coverage, images, physical audio or accepted gameplay.

The adapter authenticates only THREE original logical plans. It keeps typed continuation state and a shared lease through actual preemption. Revalidation checks current thread/stack/object/code/data and epoch membership before publishing; callback revocation cannot publish. It preserves CURRENT unrelated Cpu fields rather than restoring a stale entry frame; unexpected private/sharedVU changes refuse. A retained known context at an unexpected PC now refuses instead of falling through to AOT. Truly unowned/unknown code remains on the already-compiled AOT route and is counted. No runtime decoder/compiler or instruction VM was introduced into the typed kernels. This reference-only adapter still uses the existing guest-memory/CPU boundary and is not final native memory/ABI or a native scheduler.

The unused-stream review finding is fixed: intact/event helpers skip unreferenced streams and check counts, overflow and bounds before slicing; partial continuations skip unreferenced streams and check extents. Omitted partial filter inputs must equal the actual entry FPR when the source omitted its load. The isolated partial projection remains unsuitable for overwriting current state after actual scheduling. Whole event regression03 (`d1352112719c5133be15786e96fc1362193fbe93dbec825326e9a1b17da5a04f`) passes276 original programs/4416 sessions/9936 fullCPU+RAM+ordered-write boundaries/5520 constructed suspensions. Four empty-unused-stream positives cover both stream indices and continuation classes; eight bound/overflow refusals and the undo-skip mutant reject. Partial continuation04 (`37a983d4ff09a248526f73a041a35749dc57054a0e9a45eb5d711c0b73d0dace`) passes1620 sessions/4410 boundaries/2790 suspensions/5400 constructed guards,1620 completed-reuse refusals and six plants, including invented omitted input and undo-unused-stream changes. These partial cases remain constructed component evidence; actual partial caller/scheduler admission is OPEN. Output-extent validation remains conservative for truncated stores.

Actual no-guard shadow02 = `607109e84f593881f05bc538dffa97e0ffbda9129dea2115996b9409399c09d7`; actual event shadow05 = `f0ae6b5d4959156dd6763cecfde5657945e3f1ac61da2d58bc89d386b80527c2`; connected controls03 = `2c38e43a5dda99505218c35c2b15a7984f1b2de3be6c0e6ec8ec91a7448d2aa8`. Actual90-frame receipts are frozen with summaries in candidate04-final/FREEZE.json. The verifier now joins expected BUILD SHA, actual adapter/dependencies/compiler/ABI; older BUILD02 fails before compile. Historical controls01 compiled the actual03 header against a stale02 contract: their behavior observations remain preserved, but03 supersedes their provenance claim. Candidate5196 had itself passed the full recording and14 states before the unused-stream correction; preserve its PE/results and do not promote it over04. No frozen PE/header/object changed during either candidate's long gates.

`tools/research/probe_r192_fresh_boot.py` now starts the original ELF with NO saved state, an owned copy of the existing companion card, no window/audio and no Live compiler. Corrected candidate04 passes120 frames (`r192-resource-events-fresh-boot-03/RESULT.json`, `c5844539bf687fba669e585b88689c5bf4939643b7c029b0f27eddc49dec80ed`) and600 frames (`r192-resource-events-fresh-boot-04/RESULT.json`, `794eb34243726f8261473c5cf07b475b9c25e4511120bb4833327facb61516fc`). Twelve parser/input/endpoint/timeout plants reject. Both pin original and copied ELF/card inputs and all sealed DLLs before/after. This is a bounded boot diagnostic; startup completion, menus, gameplay and VU0 execution are not established. The first fresh01 measurement was a truthful harness REFUSED because it incorrectly expected the extra word "unique" in the actual VU registration text. Preserve that failed result and pre-correction source; corrected02 passed on5196, then03/04 measured the final tool/PE. The parser change did not repair a game-code failure.

### Final static-image guards and bounded original VU0 routes

The remaining-image linker now checks every original generated source, reused object and new source/object at the end, rejects source-identity rebinding, and joins optional reuse to the exact prior compiler flags/support/source/object identities. Final linked03 = `66747d311e36456330314cb506a981e4952f37a40f4a0b6e6cb449ac14db44fb`:348 prior compilations verified, two new hosts compiled, original96 bodies reused; both NETR/NTGUI links and exact metadata/count plants pass. Three final-hash substitutions and the source-identity overwrite plant reject. Original linked01 remains valid under its separately recorded complete manual recheck;03 supersedes the reusable-tool guard weakness. Neither image is booted or integrated and NETR activation stays excluded. Exact CLI and source pins are in each RESULT. Linked03 PEs: NETR `c5e1eb8b473adab7281b0b07e5fae4bb0b564af3f0c1467e870d72ea8a3a9975`, NTGUI `3ac2ddb740ea1eab7f5930adc3f592f4241697eac064c0ce3ed306bfcee3db92`.

`tools/research/audit_r192_vu0_routes.py` reconstructs actual original words in the five generated images. The final source is `fc654acb23866b558e459b816d5776ad0b5f5ee2a140c90c355f06e87ea1bc6e`; route audit04 = `923827f5054336f46e52e6eba2c309ee90bbd1a8037ce0322d7b4c95d700cb11`. SLUS/MOVIER/GAMER/NETR/NTGUI have1082243/107377/3/126865/235475 reconstructed words and zero direct VCALLMS/R in that domain;127448/12300/1/13790/32482 stores and6343/494/0/776/1242 indirect transfers remain unresolved. Do not infer whole-game absence from this. Main13 getter calls select DMA1/2; MOVIER selects8/9 twice but forwards UNKNOWN channel inputs at007FB3F4/007FB43C; NTGUI's four known calls select1/2. Channel0 remains possible. Main7/NTGUI5 constant VIF0 stores include reset and FIFO writes; their identical eight-word initial setup packets contain no MPG/MSCAL/MSCNT. No direct constant-address VU0 code-bus writer is proved.0031CC48 passes11000000 as packet DATA through an unknown GP cursor; NTGUI001C9408 appendsA1 and51000001 is ordinary data. Producer grammar remains OPEN; a planted VCALL template correctly remains possible. Five semantic controls plus three pin/bounds refusals pass (`CONTROL_TESTS.json`, `86746e52f32647d50dd147834f67210ef2671440578e1fcc75e45f66c8c6cdee`). This is source-derived route evidence, zero new callable/native credit or game execution.

### Next source boundary; corrections to over-broad descriptor assumptions

Replace the three-shape whitelist with a producer-bound typed Plan only after proving its whole lifetime:002ED458/002ED460 producer return ->002FAF44 final code copy ->002FAF88 pointer patch ->002FB070/002E8158 copied928-byte descriptor -> final callable owner/cache reuse/free/restore. A token captured only at producer return would become stale across these copies and omit restored caches. Numerical constants must be snapshotted on first invocation from the FINAL allocator-relative pool. Restored/imported cache admission needs a source-template word EQUALITY predicate, not an executable emitted-word interpreter. No removable-invocation count is forecast.

The original builder consumes the A0 SENTINEL LIST, not the owner's+536 count.002FAE60 supplies A2=0;002E8368 can supply ownerbyte+69 and append stream2 resources including tag300. Ordered duplicates are legal:002F0350 returns the FIRST matching tag. The existing unique-tag/two-stream/four-filter/nonempty restrictions are bounded helper admission choices, not universal source invariants.002E7C20 has no four-filter bound; overlap/caller/bounds semantics remain to prove. Zero attributes, silent no-word effects, positive partial appends, signed16/unaligned transfers, allocation failures and ambient full128-bit values remain required until implemented or proved unreachable.002E2528 acts on the parent's+516/index*896 records; it is NOT established as the928-byte descriptor pointer relocation. The actual pointer patch is002FAF88 before002E8158's copy.8000-byte (2000-word) output-arena zeroing (002FAE50 call,002FAE54 delay A2=1F40) and final four-word rounding do not prove every temporary-block hole initialized. These findings correct possible over-generalization; the earlier three-plan/corpus comparisons remain valid within their explicit domains.

The unchanged native foundation retains59 targets/65 variants/16 gates (`0921c98e3119aa258c063ebbd146241e272d2a3f220b8e0a1c4eefee6113bb6f`); do not report it as a rebuild of this reference-only candidate. Ledgers remain2 transformed/238 native and synchronized. OriginalR192, installed normal882302..., persistent card, protected saves/assets/settings remain unchanged; no accepted-game build, physical output or universal all-path claim is made. Continue independent source-plan/cache proof, VU0 forwarded-route proof and IOP callable/IRQ/retirement integration. Full native-only acceptance remains OPEN.

### Descriptor-derived executable-word equality, separate from Plan admission

A new pure reference predicate compares original packing EQUATIONS to a supplied word span. It does not manufacture an executable stream, decode opcodes, execute words or install a runtime route. `reference_resource_word_equality.hpp` derives source allocator slots and individual block clamps from the ordered descriptor, compares each appended word immediately, resolves branch offsets against final clamped block positions and checks source data-start/total metadata. It includes original JRRA/delay and the two after-return NOPs. Pool values and unwritten alignment/ambient bytes remain outside executable identity; they require separate first-invocation input bindings.

The first compiled verifier passes ALL281 source-built programs (276 intact and four positive partial policies plus their intact control), comparing10478 source code words including after-return padding. All336139 executable-bit/metadata plants reject;2003 noncode changes remain outside identity as intended. Code/data input vectors are unchanged. `emitted-resource-word-equality-01/RESULT.json` = `78bbfc99ff7f2413ce68c05ea6081b0e9244b5209585e0303f833ee7f7e65f30`; exact CLI, source/compiler/input pins and full per-program results are recorded there. The reference bytes are the independently executed ORIGINAL builder outputs in producer-layout04, not bytes generated by this predicate. The pinned original65-function word/listing audit supplies the packing helper bodies. Read-only Ghidra C was used for orientation only, on the original SLUS hash/r5900 language; it is not treated as ABI or instruction-semantics proof.

This removes the need for an enumerated observed-shape table for CODE IDENTITY within the existing bounded layout domain. It does not prove the general producer domain, numerical equivalence of new descriptors, allocation extent, owner/sentinel-list identity or a surviving native Plan lease. Unique tags, nonempty attributes, two streams, aligned nonnegative offsets,20/13/4 attribute/effect/filter limits and compatible bank combinations remain explicit restrictions. Zero-attribute, duplicate, stream2, unaligned, silent-incompatible and broader failure policies remain OPEN. No normal executable/card/save or runtime dispatch changed and credit stays2/238. Next measure candidate descriptors using this predicate without execution admission, while closing actual cache lifetime edges from original callers; never infer a producer list from owner count merely to make it pass.

### Cache lifetime and VU0 source proof boundaries

Source lifecycle03 (`ad4e372b83a49e797a5796c6e91c399a1f43cb3c1a1fff3a90f252095bb173b5`) joins4589 original ELF/listing words across31 bounded bodies/ranges and rejects eight operand mutants. It confirms the final code pool and copied928-byte descriptors share the backing allocation at cache+60. Copying is followed by owner/tag rebinding before final cache publication. Installed table0059FD90 slot12 reaches210920->210780, which clears identity, drains the owner+640 auxiliary list and frees cache+60 at2108AC. Other896-byte records and their reset are distinct, not descriptor lease proof. `audit_r192_producer_lifecycle.py` records the exact call/copy/free/indirect paths and unresolved base-typed alias inventory.

The follow-up `prove_r192_cache_lease_edges.py` joins2207 words across13 bodies and rejects eight operand/table/caller mutations (`cache-lease-edges-03/RESULT.json`, `8a71f3acbd9857aeb85f0ac06eb4075082d418879be3b720d09da305eb2d96e9`). Both actual211C50 builder callers set r8/T0=2FEE20 and r9/T1=r10/T2=0:211DA8 resolves to2FEE20, and the task-construction branch is skipped. Failure virtualcall211E34 resolves conditionally through the just-installed cache table; optional task slot12 at2108F4 resolves to12EFB0 only under its separately proved task constructor/table.210810 uses a SEPARATE child receiver at cache+24 and remains OPEN. Constructor zero does not prove later immutability. Pointer+source identity is not a host epoch:130AB0 explicitly overrides identity, with caller31ECB8, and restore/finite reuse can invalidate that assumption. A pending host Plan would need allocation epoch, copy/patch/rebind transaction, success-only commit after2FB2E4, and revoke before cleanup/restore/identity override. No lease is installed or admitted by these source checks.

MOVIER source proof05 (`92f72f16f36a8cd8189c61d845f2fb553b612069f23a32270bbc04533b5c83f8`) resolves all six represented callers of7FB3E8 to channels8/9, with five JAL delay assignments and one tail jump preserving9.7FB430 still has no represented direct/aligned-pointer caller; computed/indirect/interior provenance remains OPEN. A numeric NETR branch target is NETR's OWN code, not a MOVIER call, and the earlier raw03 count is superseded. Original PPACW instructions2E1E20..40 store[unknown,unknown,14000000,11000000] through a GP-loaded cursor: MSCAL0/FLUSH COMMAND DATA is established, destination bus/VIF unit/submission is not. Six controls and the forged-pin preflight refusal pass. The replayable source tool is `prove_r192_movier_vif_routes.py`; no runtime executes.

Packing-domain proof04 (`877bd1ad511c4a6a6b5172aafb492840f93c43bd7ebd611095a9771e779d4822`, `prove_r192_builder_packing_domains.py`) authenticates all65 original spans and generated excerpts.37 of42 inline context-buffer stores exclude VCALLMS/R even over overapproximated loaded fields; five opaque parameter slots remain open. Six of15 raw append sites exclude it; nine need actual selected-block/constant provenance. The site populations share append implementation and are not57 dynamic instructions. Nine static controls and a forged-proof refusal pass. Normal EF700/EF990 transfer helpers OR loaded offsets as full low32 values; unaligned EFC20/EFDA0 explicitly maskFFFF. The bounded word predicate is safe for its nonnegative<=32767 attribute domain and tested masked failed-append constant path; this does NOT license arbitrary negative/large offsets. Register indices0..31 do not bound runtime pointer VALUES, so no universal no-MMIO/VU0 code-bus claim follows. The source-level AC410000 with hypothetical R2=11000000 counterexample is conditional, not an observed invocation or admitted descriptor.

### Read-only descriptor census: selection error preserved before correction

Observer candidate01 (`007b44172a602e4fdbf4f193fa0cf5b413f8aef9b7a59f9508816b25d7c7ebe2`, BUILD `8ff1267241be55fdf80ef2986e8cee0f23e69b31d062648deeb6952fa77d96fc`) adds observation AFTER the unchanged three-plan adapter declines, never replacement dispatch. Its281 constructed non-mutation cases match every Cpu byte and32MiB RAM; the compiled unrelated-RAM-write mutant rejects (`emitted-resource-code-observer-controls-02`, `46163889e21f3ab87a7e2b02fd5b6002d173de838486004b7d1b1b0bc31694e0`). Initial test preflight01 compared a relative fixture path with an absolute pinned path and falsely refused before compile; preserve its source/failure record. Test02 resolves the path without changing runtime bytes.

The first actual saved probes expose an INVALID DIAGNOSTIC SELECTION GATE: candidate01 additionally required Cpu.base==descriptor code entry, although the selected image window and function entry are distinct. It consequently reports zero observations despite other-AOT invocations. Do not use those counts as coverage. No dispatch was changed by the observer. The owned full-replay child was stopped for this exact diagnostic failure; its honest REFUSED result `c4b86f662b16aba1c03d314383854679d2cfbdc95a962be4758d086ec5fe532b` and OWNED_STOP identity are preserved. Its14-state game gates finishedPASS1260frames on immutable01 bytes (`9e9a035e522efe9874862bca242ff1fea83da8cbc10435ed931a4c92a18f0643`); those are game diagnostics only and its observation counts remain invalid. The corrected observer is recorded below. Normal/protected executables/card remain untouched. The useful original281 code-identity result is independent of this observer error.

### Corrected observer03 under connected verification

Candidate03 PE `7d678e6b115253446f1113f09287fdefd40034d72f6367803e3412f6536916d5`, BUILD `61a8a12535b4566b2215a3e6b54716d55a4e2a677dcd45ddf359b2ed46aba2c4`, removes only the unproved diagnostic image-base gate and fixes the builder's include-root/compiler provenance guard. The actual normal include root is now required and its frozen headers are rehashed; wrong-root input rejects BEFORE output (`438ac940e84529dbf4b0e14895687189d7e9212caf03405cd914b0482e70d54c`). Observer controls03 (`f9fd1875cde6fb00976d496b6f9491570de2c810b9ec6741dcbe763af5b50bbc`) make562 calls across281 constructed inputs, each with equal and DISTINCT image-window bases. Complete Cpu/32MiBRAM remain equal. Both compiled plants reject: an unrelated RAM write and reintroduction of the wrong base gate. Four expected candidate-count refusals (two four-filter descriptors, each twice) stay visible;558 calls match code identity. They are not execution admissions.

Full recording02 and14 fresh probes02 finishedPASS on immutable03 PE/header/object bytes; exact completed receipts and counts follow below. Early probes show additional code identities. The old adapter's16-byte object alignment condition skips legitimate4/8-aligned source descriptors before its other-AOT counter; therefore13255 is a bounded diagnostic, NOT the total remaining helper-call denominator. The observer does not impose that16-byte condition, but still reports candidate resource lists, bounded code equality/refusal and no runtime admission. An unparsed candidate is not proof its actual original function is unsupported; its producer sentinel list may differ from owner+536. No code is dispatched from these diagnostics.

Observer build02 correctly refused before creating a PE because the12GiB workspace reserve was unavailable; the refusal is preserved. Transparent NTFS compression of completed independent disposable PEs and completed owned probe ELF/card copies restored space without changing logical bytes, paths or hashes. Every compressed file's single-link ownership was checked; no active PE, shared DLL, normal/R192 protected file or original payload was compressed/deleted. Storage receipts remain in the existing telemetry tree. A later source-proof reserve refusal and completed derived-file compression are separate; unrelated filesystem free-space changes are not attributed to a code failure. The13 original source files published in the preceding phase remain unchanged. All new evidence remains reference/source/component scope with credit2/238 and full native acceptance OPEN.

### Completed observation phase; truthful denominators and source erratum

Corrected observer03 passes the full22785-frame recording/all1519 checkpoints (`r192-resource-code-observer-full-replay-02/RESULT.json`, `5f621f5ff9f67d913db67da29cfa7104f2d5c1313fdbbe7ae40e52fbd8be4d6f`) and all14 fresh90-frame probes/1260 frames (`r192-resource-code-observer-probes-02/RESULT.json`, `29bde5c09ff3f6df0e00f5288cf2b2de26d8157c626e8ea7deb58985cc74e875`). No compiler/code miss/window/audio/difference/input change is reported. The original three-plan adapter still completes2166 invocations with126 resumes/suspensions and231 original preempt calls; the observer adds NO replacement execution. All earlier plants remain in their unchanged affected-component receipts; new observer tests cover full state and the original bad selection gate. The native foundation's59/65/16 baseline remains distinct, not a claim of a new full native build.

The full recording contains75406 declined-entry observations:46712 match bounded original code equations across15 logical candidate signatures;26642 refuse the observer's constructor-control assumption and2052 refuse candidate descriptor bounds. Another40224 non-entry invocations are counted separately, with zero diagnostic exceptions. The old13255 other-AOT count excluded source descriptors before its16-byte guard, so it was never a complete denominator. Across the full recording plus14 probes there are20 distinct matched CANDIDATE DESCRIPTOR SIGNATURES. They may share executable words; unparsed candidates coalesce into refusal buckets. These are not unique functions/code families, newly compiled functions, runtime admissions or native credit. The overlapping workloads are not combined into unique gameplay counts.

`summarize_r192_resource_code_observations.py` joins all15 completed case receipts, exact PE/source/object/package/log pins and every observation row to its summary denominators. Six forged/missing/duplicate/type/admission/error controls reject. Final `resource-code-observation-census-01/RESULT.json` = `827d55e09d3707c4eb333f4a2b25a115793daaef1ccf20611a1652af53934e97`. The26642 constructor-control refusals are an open INVESTIGATION: original2ED300 checks that field during construction, while current descriptors may reflect later state or an omitted callback prefix. Do not silently remove the gate, assume it irrelevant or invent the producer input list. Source-bound Plan capture/effective callback outcomes and whole-function/shadow comparisons are required before native admission. Alignment/code identity alone does not prove ownership or lifetime.

Final block provenance04 (`12f23ba9d41cc418f4710aa1e885e141e7acc88382f253a5fe6f55269124d461`) proves nine raw-append selector/restore chains and two pop chains. The nine literals select builder field+40 only when that selection is NON-NULL and source layout intact: original2EE3C0 treats null setter/restore as no-op. Pop2EFF80 has zero->0, otherwise modulo32 decrement and buffer read, without count/range guards. Fresh successful intact appends conditionally yield0000102D and214AFFFF; failed/refused appends can expose a prior word and retain an open invariant. Ten controls plus independent validation pass. The replayable tool is `prove_r192_builder_block_provenance.py`; exact CLI and original packing words are in its receipt.

An independent root check caught the agent's wrong manually typed summary word254AFFFF in block-provenance03. Its underlying equation and VCALL exclusions already used the correct20000000 base; only that summary field/prose was wrong. Preserve03 and its erratum. Corrected04 DERIVES214AFFFF from original002EF41C:3C052000, shifts21/16 andORIFFFF, checks all32 allocated-index equations as ADDI opcode8 and rejects both the ADDIU summary claim and a substitutedLUI2400. No runtime/observer/earlier packing04 bytes changed. Future summary words must be calculated from pinned source equations, not manually entered examples. This correction and the observer's distinct-image-base plant make both discovered mistakes replayably rejectable.

The preceding13-source phase is published in R192 fork commitc1d52b45854e688c35d5b737bf49757bf9ae633c, main fork262aaa197371605bb619f60d2fd0841aa3e0081a and parent8617a04b8f1ebb415c3b7a836b541f84238c4ae0; its measured-source join is `99ec73ac659cc811ecf0b703dc6f6faa4c8777ac99b16464ebe952c6e0ada697`. This new phase keeps all prior files immutable and publishes the source proofs, bounded identity predicate and read-only observer separately. No ordinary launcher/executable, persistent card/save, original asset/settings, ownership/credit rule or accepted native tier changed. Ledgers stay2 transformed/238 native. Complete precompilation from original source/data, host Plan leases, unadmitted descriptor/partial domains, VU0/other uploads, IOP service ownership/callable ABI/retirement and final native Windows rendering/audio/game integration remain OPEN.

### Next checkpoint in progress: parameterized family dispatch and source lifecycle

The owner requested another playable checkpoint and explicit coverage of unplayed paths from original generators/disc/cold-boot logs. Normal882302... and originalR192 remain protected; implementation is in disposable R192 candidates. The new `reference_general_resource_adapter.hpp` derives the complete executable-word witness from descriptor parameters instead of an observed-shape table. It considers possible emitted filter prefixes and accepts only an exact full-body/data-offset match; later+16 linkage does not by itself select native filtering. Current bounds remain explicit and unknowns retain compiled AOT. This is reference-memory/ABI work, not final native ownership or credit.

General adapter controls02 (`0bc930448ade9ae08cf1656888b85bad0252f77113497abc7991eaa89dd09507`) pass the three original actual guard sessions and all13 positive/lifetime/private-state/unknown-code controls. Initial01 correctly rejected the changed-key revision: the new generic lookup had treated a changed thread key as an orphan despite a live same-object/base context. Its header/result are preserved;02 restores named retained-key rejection before orphan fallback. This is a discovered adapter defect, not a failed game/capture or a weakened checker. Whole newly admitted patterns, connected gameplay, source cache epoch binding and IOP integration remain unverified at this point.

Original effective-construction proof03 (`b4f2c4a664c29e28b9916072e834d59cd6261bc4322ee6c0c2c1f6184627bf36`) passes11 whole8000-byte original program pairs,18 filter flags,two actual928-byte copies,two actual2E7A30 relocations and three planted errors. Filter+16 is a descriptor LINK; nonzero at construction stops the filter loop while final program assembly can still succeed. Relocation later replaces temporary references with final descriptor pointers. The original sentinel list, declared inputs and effective prefix remain distinct. Initial fixture01 omitted the original GP and is preserved; final03 authenticates ELF.REGINFO GP006540F0.

Four exact-archive generated-unit override objects now contain15 source-label hooks (`resource-lifecycle-hooks-01`, `75095ee966aa4e6169b15d9e103ad58783b59e80eae9f3a546ed6c9ac94029a7`). Hook removal reproduces original unit bodies/entry arrays, and15 missing-site plants reject. This avoids relying on dispatch-only hooks which direct AOT unit calls bypass. No runtime epoch/lease guarantee follows until the handler, allocation/publication/cleanup/restore paths and connected controls pass. Restored snapshots have no proven allocation-size witness; RAM bounds/owner flags/derived code length cannot invent one. Fresh IOP execution-owner integration is separately in progress; a pre-registered HLE stub is not a completed native service.

The workspace12GiB reserve initially fell below threshold. To restore it,99 COMPLETED generated VU DLLs were transparently NTFS-compressed only after verifying EVERY hardlink shares the same file identity inside owned W/build and excludes normalN/protected paths; no disposable candidate was active. All99 before/after byte hashes and paths remain identical (`r192-owned-complete-vu-storage-01`, `33e7967d06e1db9cd80fc5d154ed8b60c31876763f64ee010b0e32f7ffc52ed6`). No original asset, shared normal DLL, save, card or source byte was changed or deleted. This storage operation earns no code or coverage credit.
### Source-led coverage gates and complete captured-cache audit

The owner clarified that replay execution is regression evidence, never the code-coverage selection rule. Already captured translations must be packaged once; no further playthrough is required to recover their entries. The new source gates require (1) original-generator/offline-translator/bound-function parameter-space comparisons for all15 observed patterns, exhaustive for small parameter domains and explicit boundary/seeded sets for larger domains, and (2) an original executable/overlay FlushCache/iFlushCache caller census, including inline cache operations and unresolved indirect routes. No universal closure follows from a finite recording or an observed-image whitelist.

The read-only `tools/research/audit_r192_all_caches.py` audits ALL4524 files in the cache/source roots selected by original host/main.cpp and every retained startup log, without a replay filter or DLL execution. Final03 (`build/oracle-telemetry/all-configured-cache-audit-03/RESULT.json`, SHA-256 `050eefb711e5c13f43778a342e054df597cecfbb4a8d9b5d268f73f3a75d203f`) finds3569 EE DLLs,3506 distinct module names and589356 exported entry records. ALL1357 historical missing-code observations join exported entries in that sealed EE package; ZERO original EE DLLs are absent. The installed EE manifest is byte-identical to the audited sealed package. The48 old VU Live DLLs differ in ABI/bytes from the new offline package; all217 original keys are present in its65026-key domain. Key inclusion is coverage/identity evidence, not numerical equivalence or proof of all possible uploads.

There are9 retained original R192 session logs and1 current-launcher stderr log. Ten files are NOT ten distinct original runs, and overwritten/absent logs cannot establish the owner's lifetime launch count. An initial conversational count called all10 R192 runs; this was corrected before publication. The audit's02 attempt completed scanning but failed report assembly because a local log-key list shadowed the VU key set. Its pre-correction source and finding remain in02; final03 renames the local variable and passes. No input was modified, deleted or recaptured.

General adapter controls07 (`94957641b89234354d97875ecd4ef5ca740075bebbe6c3425f3f1b7895b22ef6`) pass18 checks, including missing source-witness refusal and preservation of the pending-save barrier after callback revocation. Source-owned final-pool reads now require the whole authenticated allocation region and current generation. A3600-frame cold baseline passes (`3c6ff04616e50d178cc395c01da1cb1395ceff6ac24ff66061df7aae4eda5165`); this is a regression check, not source coverage.

The combined disposable candidate01 builds with fresh source lifecycle hooks, paired Plan checkpoints and fresh IOP execution ownership, but cold01 FAILS (`44858f115d0c96207dd827621dc769edd095d37c223900fe7e3c19435ebd73be`). Diagnostic-only candidate02 exposes the exact refusal `reused_code_range_requires_generation_bound_entry` after LGAUD startup (`b70200c1b53ebc57f2adac98df8394d66b81b096f64645b5dd87b543ea130036`). The old crash handler intercepted the GCC exception before normal catching, so the first result lacked its named cause; pre-throw logging corrected that diagnostic gap without relaxing any guard. No failed candidate is installed. A source-bound import/registration generation handoff, actual paired save/load, full combined regression, parameter-space results and completed generator census remain gates. Normal882302..., originalR192, persistent card, saves/assets/settings and credit2/238 remain unchanged.

### Frozen closure checkpoint — owner direction 2026-11-26

The owner explicitly directed: keep original `r192-hooks` as the playable oracle with live translation, freeze `r192-aot-closure-proof` at this checkpoint, record closure OPEN and NOT required for the native port, and redirect work to native subsystems without new oracle infrastructure. No familiar launcher or executable is replaced. The installed normal882302... remains a distinct bounded intermediate build; originalR192/its launcher remain8AB43BDA.../7460ACD3..., and no failed candidate is installed. The final native product still requires zero emulation/runtime compilation; this oracle decision does not relax that acceptance contract.

The final parameter-space harness executes the original builder for ALL2202 descriptors across the15 patterns, covering ALL1328 stream assignments, aligned signed16-domain offset boundaries,16 numeric boundary sets and16 seeded joint layouts per pattern (seed26368533). Its1924 distinct executable/placement variants pass through the actual offline live-image translator. All26424 comparisons pass with ZERO differences: complete1456-byte Cpu,32MiB RAM, ordered write address/width/value and budget, count0/1/7 and both disjoint/overlapping stream cases. Five plants reject altered value, write order, actual descriptor offset, preserved upper lane and budget. Larger domains are sampled, NOT universally proved. Unknown tags/IDs, duplicate/zero-attribute layouts, negative/unaligned offsets, positive partial construction, actual scheduling/lifetime and arbitrary unseen topologies are not promoted. Literal fourth words are read from the original final-pool input, not copied from reference post-state.

`tools/research/verify_r192_resource_parameter_space.py` source SHA `e3bae9656b5ee3807f0519cd8850c37f62f364dbb65ec345c6607e9c807a866e`; `resource-parameter-space-04/RESULT.json` = `89e9d965d781a8a60b84d58e083bc3469d5d545b3225ec56519a14c6af2aa471`; FREEZE = `b00a4a930da25d200746d0aad357c2c1d21c0fbd34d2c5e849f6d292f6dc20c6`. Earlier import/extractor/include failures are retained. The offline frontend can discover apparent entries in random literal/alignment bytes; the corrected harness bounds the reference using the original terminal JR/delay/padding semantics and rejects every outgoing branch outside that body. It does not silently execute data or skip a reachable edge.

The source FlushCache/iFlushCache census covers pinned SLUS, MOVIER, GAMER, NETR and NTGUI. Original main wrappers0010CA60/0010CA90 and NTGUI001CAE20/001CAE50 join SDK syscall100 and NEGATIVE104; positive104 is not silently accepted as iFlushCache. There are63 direct JAL boundaries (26SLUS,5NETR,32NTGUI):54 selected other roles, including12 fixed original-code installers,2 overlay-loader sites and7 specifically unresolved sites. There are34 source-confirmed cache-maintenance loop instructions and95 additional instruction/data candidates, plus8855 unresolved indirect sites. All1062521 main-listing words join original ELF bytes; missing overlay/NTGUI Ghidra listings remain explicit. Eleven source/argument/mutation controls pass. `audit_r192_cache_flush_callers.py` source SHA `d6b68cf5d735554f386ba28730145629b5489aa21259f1f647cd0f4af71dc4c5`; `r192-cache-flush-census-06/RESULT.json` = `8c9806719af5e2ba67518ba9159b7ad94afac724533491a0591e4864fc5fcaa2`. This is a bounded source census, NOT proof every generator is found.

The65 pinned resource-builder/helper spans and direct graphs from2ED3E0/2FA9D0 (158/225 visited functions) do not establish a direct flush path; their10/17 unresolved indirect frontiers prevent a global no-flush conclusion. The008A5140 body/caller relation is exact, but its fresh historical writer/allocation transaction remains unproven. Do not assert every generator must have its own direct flush caller, or that a data/DMA classification closes downstream payload consumers.

| Completed bounded proof | RESULT SHA-256 |
| --- | --- |
| effective-construction-03 | b4f2c4a664c29e28b9916072e834d59cd6261bc4322ee6c0c2c1f6184627bf36 |
| resource-lifecycle-hooks-01 | 75095ee966aa4e6169b15d9e103ad58783b59e80eae9f3a546ed6c9ac94029a7 |
| owner-epochs-04 | 7e40d7a2f7149c0e37aadfaab21d62c60ff33fdfe91bc713ef66ee336d2c2c65 |
| plan-checkpoint-components-04 | 2c69aa443e27a9741c02c49fa6fd11376f43643caef0629b2b6b73b172ae5b39 |
| plan-checkpoint-source-02 | 05bb0cb6fd07d81726c7d61c56d6584c7bbc224e78cf822ee6126e5f3b5629fa |
| r192-fresh-iop-execution-05 | 31ede959a15bc4247d4e9fdd52dd6610568e58e963a8e8b6ecbd04e01227b3c9 |
| r192-fresh-iop-connector-04 | 87ac34e4f38b1cde3639d65e36d96e2cf7168a7cd05b0cbe48185c6b0942aba1 |
| general-resource-controls-08 (19 checks) | 3f63fa4929f48d9ebeda2b55884209506b14e1fbdbd795f55cab782d3d1cb49c |
| r192-general-resource-ee-compile-04/BUILD_ONLY.json | 011f5a3de924f2ba6f9e0395d6c92d98cdee0fcacbb71237276c5afd1d7f71b9 |

These proofs remain separate. The paired checkpoint retains the original v8 payload with a32-byte required-pair trailer and DPAPI-sealed native Plan companion; preflight checks the saved RAM, complete payload identity/ABI and required pair before mutation, and imports new host epochs only after full parse. Component checks are constructed/mocked parser controls, NOT actual connected game save/load. No active continuation is serialized; the save barrier includes active leases even after callback revocation. A root review caught that native refusal alone could fall through to AOT after failed restoration: the final adapter now refuses poison by name and the generated dispatcher checks poison before any dispatch route. That final global guard is compiled; integrated whole-machine rejection is still unverified. The earlier component receipt must not be cited as that integrated proof.

The fresh IOP connector passes both filename-naming/buffer scheduler routes, six full artifact comparisons and22 production controls; this is not disc-filesystem or SDK-complete closure. Failed cold integration exposes `reused_code_range_requires_generation_bound_entry`; a correct fix needs a context-bound authenticated import-J→NOP→provider-generation transition, caller-return and stale-callback/free controls. No such semantic fix was implemented. Restored state remains the explicitly legacy/unproved AOT identity route; loadcore#7's old zero stub is OPEN, not a completed native release service.

All23 authored source/test/tool files are experimental and unused by the normal production/native build. Their complete implementation and earlier helper dependencies are published on the frozen R192 branch; main carries the synchronized records and native-port source. A publication check caught an incomplete23-file mirror onto main whose older R192-only helpers were absent; that mirror is removed in a follow-up commit, preserving its history and all complete R192 sources. No native production source is removed. The final factory requires a passing15-pattern parameter receipt before linking generalized bindings; compile-only receipts do not admit execution or install a candidate. Normal executable, originalR192/launcher, persistent card, protected inputs and model/settings remain preserved. The preexisting parent inventory remains SHA `d9bc544615a65ffe3701c9f22690f9e51a818264ba3fa9acab5461eab2325542`. No accepted native implementation or transformed unit is added; credit stays2/238. The unchanged native foundation retains its prior59-target/65-variant/16-gate evidence; no new full-baseline pass is claimed for the failed experimental candidate.

Additional storage receipts preserve byte hashes and file identities: completed single-link metadata/COFF objects (`89f38b5f5b257f5b579dae074c80e603f39668f8cf9677c8d0df2a4553ec36fe`), completed standalone component clients (`fa188ebb1b7385dd693c5e98d29a985353b40e403024a1df5065af28e76baa6e`), and the disposable candidate01 PE's local PE_STORAGE.json. Only transparent NTFS compression was used, no deletion; protected/normal/shared-input paths were excluded. Later available-space changes have no established cause and are not attributed to these bounded outputs.

### Phase6 restart and restored playable route — 2026-11-26

The owner requested the parent forwarding launcher point back to originalR192 with EE Live, keeping all AOT code, card and saves. Only launchers/launch_r192_hooks.bat was changed; originalR192 launcher/executable remain7460ACD3.../8AB43BDA.... A static review verifies all3569 archived precompiled EE DLLs are present byte-identically in original game/ee-live, and confirms the original launcher enables EE/VU Live and selects the existing armageddon.ps2 and quick-save directory. No game was launched and no protected file was written. Receipt: the native Phase6 checkout's build/oracle-telemetry/phase6-launcher-restore-01/RESULT.json. Original base AOT plus cached translations are used before Live; this does not claim complete generated-code coverage.

Parent and fork implementation use the new phase6-native-memory-abi branch; main and frozen r192-aot-closure-proof stay unchanged. The explicit new branch request supersedes the earlier two-branch-only preference. Phase6 resumes the exact old30 capability probes (13memory/6inputABI/6control/5return-interface), plus an independently seeded held-out30. N1 typed guest memory is an intermediate StageA boundary, not accepted native memory ownership. No invented field, receiver, input or ordinary-return contract is admitted. N3 execution is gated on a measured held-out StageA pass; if the N1–N3 cohort remains0, stop the general-transform route. Native subsystem/service work is independent, with source/state equivalence and all ownership/credit rules retained. No oracle-closure work is resumed.

### Native Phase6 measured checkpoint — 2026-11-26

Original R192 remains selected with linked/cached AOT and Live fallback; all3569 archived EE DLLs match, and the original executable/launcher/card/saves are preserved. Native-only acceptance remains OPEN. Full native results live in the existing fork renderer-scope.json, native_phase6_memory_abi_20261126; no new narrative document was created.

| Measured constructed capability stage | Original30 | Independently seeded30 | Exact full-state/write-order comparisons |
| --- | --- | --- | --- |
| Previous closed initializer-shaped transformer | 0/30 | Not measured | Not run: 30 refusals |
| Typed memory and mechanical scalar boundary | 16/30 | 16/30 | 675 |
| Block-parameter SSA, control and return | 29/30 | 25/30 | 1070 |

The new sample seed is Phase6:N4:holdout:20261126, with SHA256(seed UTF8 + NUL + module:address UTF8) ranking, excluding every original30 entry. Membership was fixed before testing; generic parser and conservative loop-refusal fixes were diagnosed during pilot attempts. This is exploratory capability validation, not an untouched blind benchmark or production ABI admission. All new function bodies come from actual pinned generated C++ ASTs, with explicit typed scalar parameters and the one StageA memory layer; none executes an opcode stream or a guest register bank. A same-source memory heldout pass gates the control stage. Valid constructed inputs execute complete functions; declined inputs are separately counted.

The final1070 comparisons have zero mismatches. Of226 declined fixtures,140 are reference unmapped loads,40 reference unmapped stores,46 native natural-alignment refusals. Six whole functions remain refused: five unknown input/memory loop-trip proofs and one128-depth static-analysis cap. Sixty-one saved EE prefixes were inspected; neither raw nor unwind-cleared PC matched a selected entry. Execution is constructed-only; no fabricated historical call inputs or new gameplay verification. Source no-yield debit bounds are2..68, with scheduler/continuation handling still unbound.

The128-check memory component rejects actual width/order mutations. The160-case ABI component preserves the real two JAL/LW-delay bundles, all Cpu bytes/high lanes/SP/RA, with7 structural and8 planted refusals. The101 contract classes are31 leaf,45 direct nonleaf,12 potential tail,12 indirect and1 context-restoring. The largest class retains1 rederived existing machine-return proof and44 refusals; no invented typed signatures or context-restoring ordinary return. Camera verification compares existing native methods:230 standard field matches plus1 edge match/11 explicit SSE-vs-EE edge differences,5 separate frame-queue steps, zero historical entries. It does not implement the unbound camera-to-frame producer/list association or add a new generator-native subsystem. Native save-store/pure audio controls pass in owned data without an endpoint; legacy IOP/SIF compatibility is not accepted native ownership.

The unchanged native foundation passes59 exact-hash reused targets/65 fresh effective variants,13 fresh read-only gates and20 supplementary compiled controls, with958 unchanged compiled-source joins. Three mutating ledger/sync/validation-planted gates retain their unchanged-source historical receipts; they were NOT freshly rerun on protected/shared canonical files. The validation mutator was blocked before its first write. New Phase6 tests were separately compiled. No fresh full rebuild or16-fresh-gate claim is made. Failed parser, recursion, fixture path, reference tail, compiler-DLL and harness-only attempts are preserved; earlier failed lowerer bytes were reconstructed only against their already recorded exact SHA.

No new ownership/admission/native implementation credit is awarded. New scalar/input structs are mechanical test boundaries, not proved gameplay receiver layouts. The new measured pass rate supports another bounded StageA slice; it does not prove catalogue-wide scaling, StageB layout,44 remaining call contracts, actual camera-frame integration, physical input/audio or native whole-game completion. Main and the frozen oracle-closure branch remain unchanged.
