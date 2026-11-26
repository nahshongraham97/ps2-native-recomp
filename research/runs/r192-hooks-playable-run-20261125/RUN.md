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

Source/tool pins and immutable raw results distinguish the invalid experiment
from this correction. The candidate builder's sole publication style change
removes an extra blank EOF; its measured copy and Python AST identity certificate
are preserved in build/oracle-telemetry/r192-vu-correction-publication-01/.
The clean-input and source-bound generator scripts match their measured receipt
hashes exactly. Native ownership, 2 transformed and 238 credited totals stay unchanged.

### Original-source VU stop/resume closure in progress

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
source-backed variants than the previous 1,033-key candidate. Compilation
and connected replay of this newer set are pending. Unknown external roots,
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
worst2736 admission); typed continuation/event scheduling is required. Exact
VU MAC/status and VF2/3/4 results still need side-output projection or proved
consumer liveness; this family preserves the accumulator. The whole caller
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
Service#42 verification/connection is the next independent phase. Other
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
