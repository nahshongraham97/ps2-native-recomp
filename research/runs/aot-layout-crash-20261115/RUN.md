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

- Rebuilt in place: `build_native_gl_full/ps2run.exe`, 2026-11-15 17:15,
  75,948,544 bytes, **SHA-256
  `bd7479bd9f18dc0217bf4ccff5c206b422b7f08ada50a6a9f5c6cb7f1ba90149`**
- All five suites pass against that binary: `test_spu2_mix`, `test_gl_vs_raster`
  18/18, `test_native_gl_alpha`, `test_gl_lazy_readback` 10/10,
  `test_iop_native_module`.

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
