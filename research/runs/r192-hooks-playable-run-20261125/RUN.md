# Restored R192-Lineage Playable Recomp Build & Active Oracle — 2026-11-25 Session

**Date:** 2026-11-25 (`00:07:58` – `00:29:01`)  
**Status:** **Active Verified Playable Recomp Build & Behavioral Oracle** (owner-verified gameplay: **Konquest** and **Motor Kombat** work as intended alongside VS/Practice combat).  
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
  - `[mka-overlay]` (`7` intercepts: `4x` `GAMER.OVL` at `0x007C4980`, `3x` `MOVIER.OVL` at `0x0084A100`)
  - `[mka-mode-trace]` (`1,936`), `[mka-pad]` (`1,402`), `[mka-actor]` (`1,401`), `[mka-camera]` (`1,292`), `[mka-combo]` (`1,238`), `[mka-aggressor]` (`791`), `[mka-special]` (`285`), `[mka-health]` (`274`), `[mka-render]` (`268`), `[mka-telemetry]` (`268`), `[mka-practice-snap]` (`73`), `[mka-air]` (`50`), `[mka-stage]` (`8` Freefall Tier 2 transitions)

---

## 4. Verification Boundaries

- **Not verified in this run:** Offline-excluded `NETR.ovl` hooks (`0x00856700`, `0x00856780`) were registered but not entered in offline play; `misses.txt` was not generated; standalone native port zero-emulation acceptance remains **OPEN**.
- **Contradictions resolved with earlier docs:** Earlier status docs treated the `11/10` B9 binary (`B9EEF7DA...`) or `11/18` Kombat Practice `native-gl` binary (`8B763DCC...`, which ran at 4–27 FPS with `x5.00` stretched audio) as the working gameplay oracle. The restored `r192-hooks` package (`8AB43BDA...`) is confirmed as the active playable recomp build and behavioral oracle; older `milestone_work_verified_playable_20261110` (`B9EEF7DA...`) and `milestone_build_kombat_practice` (`8B763DCC...`) remain preserved historical snapshots.

---

## 5. Isolated closure branch and owner Konquest capture — 2026-11-25 follow-up

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
