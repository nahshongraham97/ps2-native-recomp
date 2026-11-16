# Movie sound, GATE line, and the movie-2 reload crash — 2026-11-15 19:13

- Log: `games/mkarmageddon/disc/logs/ps2run_2026-11-15_19-13-39.log` (407,466 lines)
- Binary: SHA-256 `eaf36b36ae4025f3e03b0d519011f51237d758a6c6e133e7e562976c473fbffb`, build stamp
  `commit 9c94385 clean` — the GATE-line build, launched through `launch_game.bat` with
  `--trace-iop` (first run with module/HLE/RPC traces on).
- Census: `games/mkarmageddon/disc/logs/census.txt` (complete, VERDICT present).

## User-observed result

The intro movie (`movieps2/LOGON.sfd`) **plays and has sound**; it **flickers rapidly** (the user
rated it epilepsy-warning level). The memory-card screen **has no sound at all** — retail plays a
sound on every button press; none was heard until the intro movie's audio started.

## GATE line — the scoreboard (first run with it)

```
[census] GATE  : emulated PS2 devices on the accepted route (each must reach 0):
                GS-stopped-in-VRAM=34592 SPU2-chip-model-ops=2438
                SIF-guest-registered=1555 CDVD-device-reads=1622
[census] VERDICT: 5 acceptance-gate violations remain: EE live translation,
                SIF RPCs through guest-registered servers, CDVD device reads,
                GS transfers stopped in emulated VRAM, SPU2 chip-model operations
```

- **CDVD-device-reads=1622 is exactly `LOGON.SFD`'s sector count** (3,321,856 / 2048 = 1622):
  the movie stream is read through the CDVD sector path (`[cdvd-native] search
  \MOVIEPS2\LOGON.SFD;1 -> sector 1787393`), not only through the native file open.
  Gate violation: movie reads must terminate in native file services.
- **EE live translation=1**: one on-demand translation at `+38.2s`
  (`no translated code at 0x0080E780 … translating it now, the game waits`).
- GS: `reached native surface = 6081 (6080 at base=8960, the movie target), stopped in emulated
  VRAM = 34592; primitives = 558333`.

## Traces — what they proved

- **stdio fix verified**: `[iop-tty] CRI ADX Driver Ver.9.16b(May  7 2004 20:20:44)` — the
  version banner printed through the new `stdio v0103` binding; CRI_ADXI ran.
- **`--trace-iop` works**: `[iop-aot-layout]`, `[iop-aot-import]` (MSL 56,599 dispatches),
  `[msl-libsd]` named calls, `[sif:guest-request]` payloads (capped at 20 for the MSL sid),
  `[iop-native-hle-call]` all appear.
- **All four AOT modules at captured bases** (`SDRDRV 0x26300`, `MSL 0x28100`,
  `CRI_ADXI 0x71300`).

## Audio — corrected picture (a prior claim of mine was wrong)

Earlier I claimed "`sceSdVoiceTrans` never called" from a `First 40` capped grep. **Full counts
contradict it** and the records must say so:

| Claim | Full-count truth |
|---|---|
| VoiceTrans never called | **5 calls** — all CRI_ADXI at 405045–405089, right after `sceSdBlockTrans(STOP)` (405031): movie-1 **shutdown flush**, not game SFX |
| all 508 DMA writes = movie ADMA | 503 ADMA + 5 voice-path DMAs (same flush) |
| "MSL received 1,537 commands" | 1,537 **arrived**; trace cap = **20 payloads logged** (1.3%); the other 1,517 unseen |

Verified audio facts:

- **Movie 1 audio** = CRI_ADXI `sceSdBlockTrans(ch=0, mode=0x10, iop=0x7D300, size=0x1000)` at
  229897, double-buffered ADMA (`buf=0/0x7D300` ↔ `buf=1/0x7DB00` status polls) — the only sound
  in the run (matches the user's observation).
- **Game SFX never played**: `key-ons = 0` and **zero `on=1` events** in `[spu2-key]` (all eight
  key lines are `on=0` clears at init) — no voice ever started, on the card screen or anywhere.
- **MSL's complete import surface** (full-counted): libsd ords 4/5/7/8/9/10/17/19/26 plus
  thbase/mwfile/sysclib/timrman/thsemap/thmsgbx/sifcmd/sysmem/sifman/intrman. It imports **no
  sound-driver library**, and it only ever *called* `sceSdInit`, `sceSdSetParam` (102),
  `sceSdSetSwitch` (4), `sceSdVoiceTransStatus` (4, movie setup only) — **no playback call**.
  Its thread is active (56,599 import dispatches), so this is not a stuck thread.
- **SDRDRV**: 12 import dispatches total (init only) — its body never ran a play path.
- `sceSdSetTransCallback`/`SetIRQCallback`: 0 calls.

### Chip-op map (the removal's target)

`[spu2]` register writes total **1,142** of the 2,438 chip ops (the rest are reads + DMA entries):

- **1,020 (89%) are TSA registers `0x1A8`/`0x1AA`** (510 each ≈ one upper/lower pair per
  transfer, matching the 508 transfers) — the **transfer machinery**, not parameters.
- ~122 writes are init/param/switch setup.
- The remaining ~1,300 ops are reads, consistent with the hundreds of transfer-status polls.
- Caller attribution (MSL 728 / CRI_ADXI 350 by nearest-marker) is **unreliable**: the linear
  marker attribution crosses threads (tick-driven writes inherit a stale marker). Not used here.

Removal implication: the bulk of the emulated-SPU2 cost is the **transfer + status machinery**
(TSA setups, status polls, DMA entries), which must become native stream state; parameter writes
are a minority.

## The crash — movie 2 reloads CRI_ADXI while instance 1 lives

```
407052 [mka-video] stream request #2: movieps2/openingn.sfd (190,517,248 bytes opened)
407448 [loadfile] cdrom0:\MODULES\CRI_ADXI.IRX;1
407449 [iop] cannot allocate 0x13100 at fixed base 0x00071300 (range occupied by 0x00071300+0x13100)
407450 [iop-native-stop] captured AOT base 0x00071300 is unavailable for CRI_ADXI size=0x130C0
```

Verified: **no unload event** occurs between the movies; CRI_ADXI's AOT body is still executing at
line 407374; the census at crash time still lists `CRI_ADXI 0x00071300`. The second load's mode-2
reservation correctly reports the **first instance's own block** as occupied. What is **not**
known: which retail semantics the game expects (reuse the loaded module vs. unload-then-reload vs.
true second instance) — the AOT body can only exist at one base, so this needs a decision, not a
guess.

## Flicker — recorded, not diagnosed

Verified: during the movie the present source alternates `DISPFB FBP=0 / scene FBP=140` ↔
`DISPFB FBP=140 / scene FBP=0` every swap while the movie writes land at `base=8960` (fbp 280,
6,080 applied transfers). The code path exists for a conditional movie blit
(`native_gl_backend.cpp:1248-1259`, only when the presented target has `prim_count==0 &&
write_count==0`). **The mechanism causing the visible flicker is a hypothesis, not a diagnosis.**

## Corrections applied to earlier records

The 17:57-run statements in the parent docs ("`sceSdVoiceTrans`/`sceSdBlockTrans` were never
called") were true **of that run** (its census had `DMA writes = 0`; it crashed before CRI_ADXI
ran) and are now superseded by this run's full counts. No previously pushed claim about the 17:57
run is retracted; the "never called" formulation must not be carried forward for this run.

## Open questions (next run / next work)

1. **Why game SFX never play**: commands arrive at MSL (1,537), MSL never calls a playback
   function, SDRDRV never leaves init, no voice keys on. Only 20 payloads seen. Needs the
   uncapped payload view or MSL-side tracing.
2. **Movie-2 reload semantics**: reuse vs reload vs second instance (decision needed for the fix).
3. **Flicker mechanism**: present-source alternation verified; cause undiagnosed.
4. **Audio removal** (gate item 2): replace the transfer/status machinery + params + mixer with
   native stream/voice state; first slice defined in the parent docs' audio blocker.
