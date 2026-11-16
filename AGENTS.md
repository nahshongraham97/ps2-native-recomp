# Project guidance (this fork)

**Read `NATIVE_PORT_ONLY.md` in this directory before changing anything
here.** It carries the owner's absolute rules. The short form:

> **Everything native, and nothing missing.**
> 1. **No interpreter on the accepted route** (EE, VU0/VU1, IOP).
> 2. **No emulated PS2 device on the route** (CDVD, GS raster/state machine,
>    SPU2, SIF hardware) - removed, not "compatibility".
> 3. **Nothing compiles while the game runs.** The EE and the VU1 are already
>    translated and ready; the runtime's translator is a hard stop that names
>    the missing window, never a fallback. Every function the program can
>    enter is an entry point of the generated library, and every code image is
>    enumerated statically and translated whole. **The owner never plays modes
>    to discover missing code, and the game never stops for one.**
> 4. **No guest anything that involves emulation in any capacity.** Guest code
>    that is pre-built and linked in is fine; a guest module left loaded and
>    running is not.
> 5. **The VRAM is the device's VRAM** - the guest's writes land in native GL
>    resources, its reads come from the GPU; no host-side VRAM array is the
>    source of truth.
> 6. **Retail platform calls terminate in native game/Windows services**, and
>    compatibility data may be decoded but the implementation must be native
>    (never a PS2 state machine).
> 7. **No knowingly unfinished fidelity in a release build.** A dropped
>    packet or a wrong blend is fixed, not counted.
> 8. **Every claim carries a run record** (build commit, flags, log, frames,
>    audio). Counters measure; they are not the claim.
> 9. **The tools must always be available** - the Ghidra project is never
>    shared between agents.

## What this fork is

This is the runtime and the recompiler that turn a PS2 disc into a native
Windows program. For **this project** (the retail Armageddon port) the
upstream README's behaviour is **overridden** in one respect:

> Upstream: *"Code translated while you play: game code and graphics programs
> the first build did not see are translated in the background while the game
> runs."*
>
> **Here: nothing is translated while the game runs.** Every function the
> program can enter is an entry point of the generated library, every code
> image is enumerated statically and translated whole, and the runtime's
> translator is a hard stop. The upstream `PS2_EE_LIVE` "translate now" path
> and the VU1's `vu1-live` path are to be removed, not left as fallbacks.

Everything else upstream says about the runtime (the recompiler, the WebGPU
or native GL drawing, the pad, the memory card, the sound) is the direction
this project takes.

## House rules that differ from upstream

- **The census is the gate.** `runtime/src/compliance_census.cpp` counts every
  emulated device on the route; a non-zero counter is a defect, not a
  limitation to report.
- **No fallback paths on the accepted route.** The software rasterizer exists
  for other titles, but the native route must not use it.
- **Instrumentation is added, never removed.** If a counter would hide a
  problem, add a better counter instead of removing the one.
- **The runtime's own naming.** The sound service is `runtime/src/iop/
  audio_service.cpp` (it replaced the emulated SPU2 chip); the GS's native
  backend is `runtime/src/gs/native_gl_backend.cpp`; the census is
  `runtime/src/compliance_census.cpp`.
- **The upstream code is a starting point, not the spec.** Where upstream
  emulates a device and this project requires a native implementation, the
  native implementation wins.

## Where the direction lives

`NATIVE_PORT_ONLY.md` (the rules) and, in the parent repository,
`ROADMAP.md`, `VERIFIED_STATUS.md`, `RUNTIME_BLOCKERS.md` and `HANDOFF.md`
(the current step and the evidence).

<!-- PROJECT-RULES -->
## The project's rules — absolute, and repeated in every document that an agent may read

**How the work is driven**

1. **The next build I tell the user to run must carry REAL fixes.** No
   "please try it again" without a change that the evidence says is a fix.
2. **Never tell the user to launch the game until EVERYTHING is fixed.** The
   user's machine is not a test rig: the game is not opened (no window, no
   device, no sound on the user's machine), and the user is not asked to
   launch it, until the work is verified ready and working.
3. **Do not stop, and do not produce a final output with work outstanding.**
   Every claim carries its run record; the work list is the state of the
   project, not a summary of intentions.
4. **Every claim carries a run record**: build commit, flags, log, frames and
   audio. A counter is not a claim - the frames and the audio are the claim.
5. **Documentation is part of the work.** Every build, every blocker, every
   defect, every fix and every revert is recorded where the next agent will
   read it: `PORT_STATUS.md` (the state), `research/runs/INDEX.md` (the build
   log) and the per-build records under `research/runs/`.

**The acceptance gate (absolute)**

6. **No PS2 syscall, CDVD, GS, VU or IOP API may remain on the accepted
   boot/gameplay path.**
7. **The recompiled retail logic may remain C++, but its platform calls must
   terminate in native game/Windows services** - file, input, audio, video,
   timing and thread services.
8. Operationally, for one pinned build and flags:
   * **no guest instruction or microcode interpreter** (EE, VU0/VU1, IOP);
   * **no emulated PS2 devices on the accepted route** (CDVD, GS raster/state
     machine, SPU2, SIF hardware);
   * the retail game's platform calls terminate in native file/input/audio/
     video/timing/thread services;
   * compatibility data (GS packets, SIF RPC packets) may be decoded, but the
     implementation is native, not a PS2 state machine;
   * every claim has a run record with build commit, flags, log, frames and
     audio.
9. **NO EMULATED ANYTHING AT ALL. NO EMULATED VRAM. NO CPU RASTER. GPU-ONLY
   RASTER IS REQUIRED.** Nothing may be emulated or fall back to emulation.
   **NATIVE ONLY.** Acceptance in one shot - one build.
<!-- /PROJECT-RULES -->
