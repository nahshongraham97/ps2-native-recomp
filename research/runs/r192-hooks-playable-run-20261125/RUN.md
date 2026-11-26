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

The original generator emits 942 program keys, including 843 derived jump
targets, with zero skipped inputs. 929 keys have byte-identical generated
bodies to the existing 987-key R192 corpus; 13 keys are additional. Exact-body
union produces 1,000 keys. Truncated DMA, excessive MPG count, out-of-range
destination and wrong command plants are rejected. No captured state is
used to invent these original payloads. The read-only Ghidra connection
identified the matching ELF and joined 145 inspected instructions to original
file bytes; 21 of 99 queried roots have direct recorded references. Unreferenced
roots remain a provenance/control-flow limit, not invented class or call names.

The additional 13 kernels are compiled and registered in separate candidate
build/r192-elf-vu-candidate/out/ps2run.exe, SHA-256
74fc7c9b4bf3d0118c695b9b3cf50c919223f9df26936455713afc007f2ef84f.
Its additional-registration source is byte-identical to the public tool's
generated output. Actual startup registers 987 + 13 VU programs and all 3,569
sealed EE modules. The actual 80225→80315 probe passes 90 new frames with zero
misses and exit 0. This does not establish execution equivalence of all 13
new programs. The normal installed 882302... build still has 987 VU keys.

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
| Original-ELF VU generation | build/oracle-telemetry/r192-elf-vu-04/RESULT.json | 8c05157c65de120cbb3fe0eecb266b9336638d15bba85422ee27a3a91aa69079 |
| VU extension compile | build/r192-elf-vu-candidate/RESULT.json | 6e2c270b2be37820bb5c611bb87c210f39ede6c231bd4dfb087a9d41eda10a08 |
| VU extension 90-frame probe | build/oracle-telemetry/r192-elf-vu-frame90-01/RESULT.json | 0ebfcafc5ef56d2ce038ed0dad385b40b188e3c9faaf73c2edb73347b8dd06c4 |
| Fresh native baseline | build/oracle-telemetry/complete-baseline-20261125-223027-30e47a1a/RESULT.json | 0958032277e1fb6c538f7494834146b87f1a893481b1fa391b45d9a0c28bcaed |
| Read-only Ghidra provenance | build/oracle-telemetry/r192-vu-static-payloads-01/GHIDRA.json | 77ddda49d2481d6c55c48e589c14661b434e6f68e8d39620afdf6751d2b721bc |

Final tool replays regenerate every static/VU C++ file identically to the measured build inputs, and the extra registrar matches the compiled candidate byte-for-byte. Receipt build/oracle-telemetry/r192-root-source-final-01.json SHA-256 5ff592028f2c965951f95ef631e03cc543f97fcaeb690083327748b3901c0cbb. The EOF-only style correction preserves the Python AST; no native behavior or credit changes.

### Not verified

No physical headed gameplay, audio-device run or independent held-out session
was produced by this integration. Unrecorded future code paths, complete mode
coverage and the old finite-template/55-dump questions remain unproved.
The installed R192 route has no EE/VU live compiler, but complete precompilation
remains OPEN. The separate static/VU candidates do not prove all emitted EE
families, VU payload variants or IOP paths, and are not deployed by this study.
VU0 has no VCALLMS in the inspected generated corpus/listing; VIF0 and dynamic
emission prevent inferring universal absence from that observation.
The recomp retains console memory/device models and is not final native-port
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

### Falsification attempts

Named refusals cover forged manifest/DLL hashes, wrong exported/source stamps,
wrong ABI, out-of-DLL metadata, active padded entries, second-DLL failure before
any registration, malformed package metadata, wrong frame duration, nonzero exit,
timeout, ambiguous state loads, miss-file-only failures, missing checkpoints and
inconsistent VU function/table unions. Legitimate unsigned wrapped/aligned unit
padding is source-proved and tested; it does not admit active out-of-image entries.
All raw failures remain saved. No general transformer/native credit was inferred.
