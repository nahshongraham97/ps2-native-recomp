# Native port only — the rules of this project

**These rules are absolute. They are not a roadmap, not a preference, and not
something to trade off against a schedule. An agent that breaks one of them
has broken the project, even if the game runs.**

The owner has stated these repeatedly. They are recorded here so that no agent
has to be told again, and so that a change that violates one is visibly a
violation rather than a judgement call.

---

## 1. The acceptance gate (absolute)

> no interpreter on the accepted boot/gameplay route (EE, VU0/VU1, IOP);
> **no emulated PS2 device on that route** (CDVD, GS raster/state machine,
> SPU2, SIF hardware — removed, not "compatibility"); retail platform calls
> terminate in native game/Windows services; compatibility data decoded,
> implementation native (never a PS2 state machine); every claim has a run
> record with build commit, flags, log, frames, audio.

Every clause is a *requirement*, not a goal. A counter that still shows a
violation is a **to-do**, never an acceptable state and never a "known
limitation" to be reported as progress.

## 2. "Everything native" means three things

### 2.1 Nothing compiles while the game runs

The EE and the VU1 are **already translated and ready** before the game
starts. The runtime's translator is **not a fallback**: it is a hard stop that
names the missing window. `PS2_EE_LIVE` must not mean "compile it now while
the game waits", and the VU1's runtime translation path goes the same way.

The coverage is a **static property of the program**, computed from the code:

- **Every function the program can enter is an entry point of the generated
  library** — every function in the ELF's symbol table, not only the ones an
  earlier run happened to reach. A function reached indirectly (a callback, a
  function pointer handed to a menu or a mode) must be callable because it is
  already translated and already exported.
- **Every code image the game can execute is enumerated statically and
  translated whole**: the ELF's own text, the disc's modules, and every blob
  a loader copies into RAM. The images come from the disc, so they are
  captured **offline from the disc**, not from a game run.

Consequence: **the owner never has to play through modes to discover missing
code, and the game never stops for a missing window.** There is nothing to
discover, because everything is already there.

### 2.2 No guest anything that involves emulation in any capacity

No guest module may remain loaded and executing. Guest code that is
**pre-built** (statically recompiled ahead of time, linked into the program)
is fine — that is what this project is. Guest code that runs because it was
left loaded is not. `CRI_ADXI` was the last one: its ADX sound server and
its ADX video player become native services and the module is not loaded at
all.

### 2.3 The VRAM is the device's VRAM

The guest's writes land in **native GL resources**; the guest's reads are
served **from the GPU**. A host-side VRAM array that is the source of truth
is an emulated VRAM and must not exist. Memory the guest owns (its RAM, its
scratchpad) is the game's own memory and is fine.

## 3. Platform calls terminate in native game/Windows services

The file service, the memory card, the CDVD content volume, the sound
service, the pad, the SIF transport, the GPU — all native. Nothing in the
accepted route may call out to an emulated device.

## 4. Compatibility data may be decoded; the implementation must be native

The GS packets, the SIF packets and the sound protocol are *data*: decoding
them is correct and expected. Implementing a **PS2 state machine** to interpret
them is not. A register file, a bus, a timing model or a device model
presented as "the service" is the violation this clause exists to prevent.

## 5. No knowingly unfinished fidelity in a release build

Every known defect is **implemented**. A dropped packet, a wrong blend, a
missing effect, a wrong colour: fixed, not counted. "6 packets remain
rejected and are counted" is a to-do list, never a delivery.

## 6. Every claim carries a run record

Build commit, flags, log, frames, audio. A counter is the *measurement*, not
the claim. If it was not measured in a run that reached the thing, it is not
known.

## 7. The tools must always be available

The Ghidra project is **never shared between agents**: every launch works on
its own copy in its own directory (a private copy has a private lock file,
so no other agent and no crashed session can lock the tools away).
`tools/mcp/unlock_ghidra.ps1` repairs a stale lock and never kills a process.
An agent must never be left without a tool because a dead session held one.

## 8. The forbidden shortcuts

Do not, in any circumstance:

- fall back to the software rasterizer on the native route;
- leave a runtime compiler enabled (the EE's or the VU1's);
- reintroduce a register file or a device model and call it "compatibility";
- count a gap instead of fixing it;
- state a claim without a run record;
- share the Ghidra project between agents;
- ask the owner to play modes to discover missing code.

## 9. The programme's order

The remaining work, in this order. Each step is a build with its own run
record.

| Step | Work | Proof |
|---|---|---|
| 0 | these rules, the acceptance checker, the census counters | the checker runs against the last census |
| 1 | **every function becomes an entry point** of the generated library | the census's live-translation count reaches 0 and the stall disappears |
| 2 | **every code image enumerated statically, captured offline from the disc, translated whole** | the coverage probe (a run with the compilers off) completes |
| 3 | **the runtime compilers are deleted** (EE and VU1) | the census has no "compiled at runtime" counter left to be non-zero |
| 4 | CRI_ADXI native: the ADX sound server, then the ADX video player | `SIF guest-registered = 0`, `guest modules loaded = 0` |
| 5 | the VRAM becomes the device's VRAM | the guest's VRAM reads return the GPU's pixels |
| 6 | the fidelity gaps | the dropped-packet list is empty |
| 7 | the modes, played and fixed | each mode plays to its end |

Steps 1–3 are the "nothing is missing" core. The modes are step 7 and are a
*gameplay* list, not a coverage list: after step 2 the coverage no longer
depends on anyone playing anything.

## 10. Where this is recorded

- This file: `NATIVE_PORT_ONLY.md` (the parent repository, and a copy in
  `external/forks/ps2-native-recomp/`).
- `AGENTS.md` in both repositories points here and repeats the invariants.
- `ROADMAP.md`, `VERIFIED_STATUS.md`, `RUNTIME_BLOCKERS.md` and `HANDOFF.md`
  carry the same wording and the current step.
- The census (`runtime/src/compliance_census.cpp`) measures the gate, and
  `tools/acceptance/check_acceptance.py` fails a run that violates it.

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
