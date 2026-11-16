# Build delivery — 2026-11-16 (nothing in the log is sampled or truncated)

- Commit: `c9cfbb7` on `main` (`external/forks/ps2-native-recomp`), built from a **clean** tree
  (stamp: `commit c9cfbb7 clean, built 2026-11-16T03:37:52Z, Release, Clang-23.1.3`)
- Runner: `build_native_gl_full/ps2run.exe`, 2026-11-16 22:37, 75,976,704 bytes
- SHA-256: `47075bb124fffab3225287a39df56cfc941f428d22ea02cc1efdcf4fbc28d0bf`
- What it adds: every trace line is logged by default, and the log file is never truncated.
  `PS2_LOG_MAX_BYTES` (default 0 = no ceiling) rotates the file to `<name>.1` instead of stopping
  at a fixed size; `PS2_TRACE_MSL_LIMIT` (default unlimited), `PS2_TRACE_MOVIE_PRESENT` (default
  every present), `PS2_TRACE_SWEEP` (default every sweep write), `PS2_TRACE_VU` (default every
  unimplemented VU "lower" operation), and the `[frame]` heartbeat now prints every frame.
  The 600-line in-memory ring is only the in-game window's tail/head view — the file is complete
  regardless. The census report, the `[iop-aot]`/`[iop-aot-import]`/`[iop-native-hle-call]` traces
  and the cumulative `[vu1]` summary were already complete and are untouched.
- Replaced runner: `E536E22CB107C669C9BFFDEFF6AF643304ABC37E6743B6C5408E7A62081CF17C`
  (`6e0fb9b clean`, 2026-11-16 03:10 — native voice engine, Slice 2–3 step b)
- Delivery history: `435ce55ae7de51a7dcbc4c93f16e27aa572a76fb46bce7d3209f75ed43062aa7`
  → `8600a796…` → `6b2335a1…` → `bd7479bd…` → `13424cdb…` → `9141cd27…` → `eaf36b36…`
  → `2c16e7af…` → `cd686675…` → `E536E22C…` → `47075bb1…`
- The `13424cdb…` runner was run live once (`ps2run_2026-11-15_17-57-43.log`): card fixed, all
  four AOT bases loaded, crashed at the movie's `stdio v0103` import — record in
  `research/runs/aot-layout-crash-20261115/RUN.md`.
- Earlier the same day the `6b2335a1…` runner was run once (`ps2run_2026-11-15_16-54-43.log`),
  crashed at `LIBSD` (fixed in `7f2f3d9`), and verified the shutdown flush: `census.txt` was
  written with the complete report including the `VERDICT:` line.

## Why the census was lost, and what actually limits a log

Run 2 (`games/mkarmageddon/disc/logs/ps2run_2026-11-15_14-21-00.log`) ends mid-table at line
44571. The cause is not size:

- the **log file ceiling is 64 MB** (`LogTee::kFileLimit`, `host/main.cpp:238`, enforced at
  `:277` — the file stops and appends "[log] this file reached 64 MB and stops here");
- the **1 MB pipe** (`_pipe(fds, 1 << 20, ...)`) is backpressure only: a writer blocks when it is
  full, it never drops bytes;
- what loses data is **process death with undrained bytes in the pipe**: the census is printed from
  the SIGABRT handler, `raise(SIGABRT)` ends the process without the SEH filter, so
  `LogTee::flush()` never runs and the pump thread never moves those bytes into the file.

## What changed

- `compliance_census`: `set_exit_flush_hook()`, called from `report()` and from every shutdown
  path (atexit, SIGABRT, SIGTERM); the verbose GS by-base table is kept **complete** (the earlier
  12-base cap was reverted: it removed detail for a problem it could not cause — the table is
  ~1.5 KB).
- `host/main.cpp`: registers the hook, which now (a) flushes the log pipe and (b) copies the
  `[census]` lines out of `LogTee`'s in-memory ring into `census.txt` next to the log, so the
  report survives even a death no handler can catch.
- `kernel.cpp` / `iop/cpu.cpp`: `report()` is called before the strict-mode abort at the AOT
  placement stop and the IOP stop, so the report is produced in normal control flow.

## Gates

1. A run that aborts under `PS2_IOP_NATIVE_STRICT=1` must leave a log containing the whole
   `[census]` block down to `VERDICT:`, and `census.txt` must contain the same block.
   **Met live** by `ps2run_2026-11-15_16-54-43.log` + `census.txt` (891 bytes): the report
   survives the abort; the log's final line is the GS by-base table, which `report()` prints
   after the verdict by design.
2. A normal run must leave both the log and `census.txt` complete (VERDICT line present).

## Suites against this binary

`test_spu2_mix` PASS · `test_gl_vs_raster` 18/18 PASS · `test_native_gl_alpha` PASS ·
`test_gl_lazy_readback` 10/10 PASS · `test_iop_native_module` PASS (all exit 0).

Verified live: the shutdown flush and `census.txt` (see the 16:54 run), and the AOT footprint
protection — which was exercised live and found a real bug (the reservation rejected its own
footprint; fixed in `7f2f3d9`, record in `research/runs/aot-layout-crash-20261115/RUN.md`).
Still unverified live: the shape-based XMCSERV 0x0C/0x0D dispatch, the named `libsd` tracing,
and the mode-2 reservation fix itself. Newest live evidence before this build: the Run 2 census
(fork `c25fef6`) plus the 16:54 crash run.
