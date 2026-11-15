# AOT layout mismatch at LIBSD — crash, root cause, fix (2026-11-15)

## The run

- Log: `games/mkarmageddon/disc/logs/ps2run_2026-11-15_16-54-43.log` (98 lines, 9,421 bytes)
- Launched by the user through `launch_game.bat`; not agent-launched.
- Binary: `build_native_gl_full/ps2run.exe`, SHA-256
  `6b2335a1b6503960a941b7872bedd2a12dd546a61b95ae8ace3e39341cf1bb99`, whose own
  `[build]` stamp reads `commit dfc4076 clean, built 2026-11-15T21:44:21Z` — the
  delivered census-flush build, so this run exercised the Run 3 staging.
- Outcome: **aborted at `LIBSD.IRX` load, before the memory-card screen.**

```
[loadfile] cdrom0:\MODULES\LIBSD.IRX;1
[iop-native-stop] AOT layout mismatch for native replacement LIBSD: expected base 0x00020500, reserved 0x00085300 size 0x5E00
```

## What the census flush proved (independent of the crash)

`games/mkarmageddon/disc/logs/census.txt` (891 bytes) was written by the new
shutdown hook with the whole report including the `VERDICT:` line, and the log
itself ends complete rather than mid-table. The flush mechanism that Run 2 lacked
is now **verified live**.

The numbers describe an early death, not progress: SIF native = 49 guest = 0
(Run 2 had 2,844 guest RPCs, but only because it ran far enough for MSL to make
them), GS 1 transfer / 3 primitives, SPU2 0, CARD only method `0xFE` ok=1, CDVD 0.
`VERDICT: ALL MANDATE CRITERIA SATISFIED` is technically true of a process that
died at the first AOT module — it must not be read as a milestone.

## Root cause

`runtime/src/iop/kernel.cpp`, the native-replacement reservation:

```cpp
reserved_aot_base = alloc(requested);                 // mode 0 = first-fit
if (!reserved_aot_base || reserved_aot_base != aot_layout->base) -> stop
```

Run 3 added `overlaps_registered_aot()` (kernel.cpp:725-737) to `fits()` so
ordinary allocation cannot be placed over a captured AOT range. But this call
passes no `is_aot_target`, so `fits(..., false)` rejects **the module's own
footprint**, and the mode-0 walk (kernel.cpp:775-807) then jumps every registered
range:

| step | address | rejected because |
|---|---|---|
| start | `0x20000` | overlaps LIBSD `0x20500..0x26300` |
| past the 0x500 block at kUserBase | `0x20500` | **LIBSD's own footprint** |
| jump | `0x2D100` | overlaps MSL `0x28100..0x71300` |
| step 0x100 × 785 | `0x71300` | overlaps CRI_ADXI `0x71300..0x84400` |
| CRI_ADXI hardcoded jump `0x71300+0x14000` | **`0x85300`** | free → chosen |

`0x85300` in the crash line is therefore not random: it is the exact product of
the walk, which confirms the mechanism rather than guessing at it.

Why Run 2 (fork `c25fef6`) survived the same code: it had no overlap check, and
first-fit only reached `0x20500` because the slots below were already full (a
0x500-byte block at `kUserBase = 0x20000`, which ends exactly at `0x20500` and so
does not itself overlap the footprint). The equality check passed by coincidence,
not by design — which is why the new protection exposed it immediately.

Registered footprints are exactly adjacent and non-overlapping:

| module | base | totalSize | ends |
|---|---|---|---|
| LIBSD | `0x20500` | `0x5E00` | `0x26300` |
| SDRDRV | `0x26300` | `0x1E00` | `0x28100` |
| MSL | `0x28100` | `0x49200` | `0x71300` |
| CRI_ADXI | `0x71300` | `0x13100` | `0x84400` |

## Fix — fork `7f2f3d9`

```cpp
reserved_aot_base = alloc(requested, 2, aot_layout->base);
```

Mode 2 asks for the captured address outright; it is the same call the
guest-IRX AOT path already used at `kernel.cpp:1483`. It returns the captured
base or 0, never a wrong address, and 0 arrives with `alloc()`'s own
"range occupied by" diagnostic. No other allocation path is weakened: the
footprint protection for ordinary allocation stays in force.

## Verification

- Rebuilt in place: `build_native_gl_full/ps2run.exe`, 2026-11-15 17:25, 75,948,544 bytes,
  **SHA-256 `13424cdbdac05902cc9cc226e485da204921cb445f6265b4e5a661ca21114bba`**, build stamp
  `commit c34b3fb clean` (the first build of this fix, `bd7479bd…`, carried a `dirty` stamp
  because it preceded the commit and was replaced rather than shipped)
- All five suites pass against that binary: `test_spu2_mix`, `test_gl_vs_raster`
  18/18, `test_native_gl_alpha`, `test_gl_lazy_readback` 10/10,
  `test_iop_native_module`.
- Offline data check: parsing the four real IRX ELFs exactly as `kernel.cpp:1342-1345` does
  gives allocated blocks `0x5E00 / 0x1E00 / 0x49100 / 0x13100`, so every reservation ends at or
  before the next captured base (`0x26300 / 0x28100 / 0x71200 ≤ 0x71300 / 0x84400`).

## Gate for the next run

1. No `AOT layout mismatch`; the log must reach
   `[iop-native] LIBSD v1.5 C++ SPU2 bridge active; guest LIBSD IRX is not loaded`
   (Run 2's line 84), then `MSL.IRX` and the `[libsd] sceSdInit(flag=0) complete`
   sequence.
2. With `PS2_TRACE_MODULES=1`, `[iop-aot-layout] native ... reserved 0x...` lines
   must name `0x00020500`, `0x00026300`, `0x00028100` and `0x00071300`.
3. Then the three standing gates: MSL→`[msl-libsd]` trace (audio), CRI_ADXI body
   at `0x71300` (movie), and the card screen (GetInfo2 readback + `resolved by
   shape to ...`).

Still unfixed and untouched by this change: card detection root cause, audio
(has instrumentation only), movie presentation for the guest-owned surface,
and the census verdict's blindness to early deaths (a process that dies at the
first AOT module reports "ALL MANDATE CRITERIA SATISFIED").

---

# Follow-up run — 2026-11-15 17:57: card fixed, crash moved to the movie

- Log: `games/mkarmageddon/disc/logs/ps2run_2026-11-15_17-57-43.log` (20,913 lines)
- Binary: SHA-256 `13424cdb…` (`c34b3fb clean`, per the log's own `[build]` stamp) — the
  delivered mode-2 fix, run live for the first time.
- Census: `games/mkarmageddon/disc/logs/census.txt` written again (flush still works).

## What this run proved

1. **All four AOT modules loaded at their captured bases.** The census guest-module line reads
   `SDRDRV 0x00026300 MSL 0x00028100 CRI_ADXI 0x00071300` (LIBSD `0x00000000` = native host
   bridge, guest IRX not loaded). The mode-2 reservation fix is **verified live**.
2. **The memory card works.** Receipts from the log:
   - line 1907: first `GetInfo2 status=-1` (the `kMcChangedCard` edge) → subsequent `status=0`;
   - line 4972: `method 0x0C resolved by shape to mc_chdir (maxent=0 … name='/BASLUS-21410')` —
     shape dispatch live, and the game navigated to its save folder;
   - open/read/write/close (`0x02/0x03/0x04/0x05`) all `result 0`;
   - line 14005: `free_clusters` **8000 → 7831** — a save was written to the card;
   - census: `0x01 ok=20 fail=16` (the 16 fails are port 1 absent, `status=-13`, expected),
     `0x02 ok=6 0x03 ok=6 0x04 ok=4 0x05 ok=2 0x06 ok=4 0x0C ok=4 0xFE ok=1`.
3. **The CRI_ADXI AOT body executed**: the stop's `caller=0x000714C8` lies inside
   `0x71300..0x84400`, so the movie driver started from its captured base.

## The new crash — stdio version 0x0103

```
[mka-video] stream request #1: movieps2/LOGON.sfd (3,321,856 bytes opened)
[libsd] sceSdVoiceTransStatus(ch=0, wait=1) -> 1
[libsd] sceSdVoiceTransStatus(ch=1, wait=1) -> 1
[loadfile] cdrom0:\MODULES\CRI_ADXI.IRX;1
[iop-native-stop] unresolved native import stdio v0103 ordinal 4 caller=0x000714C8
  a0=0x0007A8CC a1=0x0007A8F0 a2=0x0007A8FC a3=0x00000000
```

No guess was needed to name the call: the IRX's own strings at those addresses are
`a0="CRI ADX Driver Ver.9.16b(%s %s)."`, `a1="May  7 2004"`, `a2="20:20:44"` — the driver's
version banner. Ordinal 4 of stdio is `printf` (the ps2sdk `0x0102` table has it at the same
slot); the retail driver was built against Sony SDK **stdio version `0x0103`**, which
`install_stdio()` never registered (only `0x0102`). Fix in fork `d9e5097`:
`register_hle_library("stdio", 0x0103)` — `def()` binds ordinals across every registered
version, the pattern `sysclib 0x0103/0x0104` and `loadcore 0x0101/0x0103` already use.
CRI_ADXI's bridge needs exactly one stdio import (ordinal 4); its other imports (libsd,
intrman, loadcore, sifcmd, sifman, sysclib, thbase, thsemap, vblank) are all already bound.

## Audio — still silent; the instrumentation gap is now closed

- SPU2 `key-ons = 0`, `peak sample = 0`. The `[spu2-key] … on=0` lines are init clearing KON.
- `[libsd]` received `sceSdInit` + `sceSdSetParam` ×N (startup) and `sceSdVoiceTransStatus` ×2
  (movie setup) — but **never** `sceSdVoiceTrans` or `sceSdBlockTrans`. Those two log
  unconditionally (`libsd_native.cpp:691` and `:704`), so their absence is proof the chain
  never attempted a single sound-buffer transfer.
- The staged `[msl-libsd]` trace could not have fired: it is gated on `g_trace_modules`
  (`iop_imports_MSL_*.cpp:41`), which only the `--trace-iop` **command-line flag** sets
  (`host/main.cpp:610`). The launcher's `PS2_TRACE_MODULES=1` environment variable is read by
  nothing in the runtime — every "trace" line expected from it was silently dead.
- MSL↔driver traffic: 1,425 guest-classified SIF RPCs on `sid 0x00012345 fn 56026`, payloads
  invisible without that flag.
- The census "Guest" label is the **server registration path** (registered through the guest
  SIF API), not interpreter execution: `deliver_request` (`sif.cpp:203`) wakes the AOT module's
  own thread and the handler runs as native code — consistent with `IOP guest instructions = 0`.

## GS — the gate is a display-surface gate, not a texture drop

`applied = 1, skipped = 31,736; primitives = 502,814`. The gate (`gs.cpp:407-411`) applies a
VRAM write to the native surface only when it targets the display base, the active draw base,
or `base == 8960` (the movie target). The skipped transfers are texture uploads to
fbp 301–351: they land in emulated VRAM for sampling, and the native backend has its own
`TextureCache` / `prepare_texture` path (`native_gl_backend.cpp:174,459`) that samples VRAM at
draw time. Whether that sampling path is correct is the open GS question — the census counter
alone does not prove texture loss.

## Verdict

`1 mandate violations remain: SIF guest RPCs` — the 1,425 `sid 0x00012345 fn 56026` calls.
EE live = 0, VU fallbacks = 0, IOP guest = 0, CDVD = 0 (all PASS).

## Delivered for the next run

- **Runner:** `9141cd27e4fc05ca0aa9a0bc70114cd2ce4575b6bf7d6b1b5227fbcdca1cb07b`
  (`d9e5097 clean`, 2026-11-15 18:29, 75,948,544 bytes) — adds the stdio `0x0103` registration.
- **Launcher** (`launch_game.bat`, parent repo): now passes `--trace-iop`, turning on
  `g_trace_modules` — `[iop-aot-layout]`, every `[iop-aot-import]` call with its return,
  named `[msl-libsd]` calls, and `[sif:guest-request]` payloads (first 20 for the MSL sid).
  This is the audio instrumentation the previous runs never had.

### Gates for the next run

1. **Movie:** no stdio stop; CRI_ADXI proceeds past the banner — its libsd calls for the
   movie's ADX audio will appear as `[libsd]` lines.
2. **Audio:** `[msl-libsd]` + `[iop-aot-import]` + `[sif:guest-request]` must show whether the
   MSL RPC thread processes `fn 56026` and why `sceSdVoiceTrans` is never called.
3. **Card:** unchanged (working); confirm a second run repeats the save write.
4. **Census:** the SIF guest-RPC classification stays unless the server registration changes.
