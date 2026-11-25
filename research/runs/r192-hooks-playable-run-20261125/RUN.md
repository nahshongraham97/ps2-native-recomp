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
