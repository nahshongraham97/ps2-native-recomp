# The native CRI ADX service — the contract (2026-11-16)

What the native service has to replace, measured from the game's own files and
from the 13:21 run's log (`ps2run_2026-11-16_13-21-09.log`, the build that
reached the intro movie).

## The guest module

- The file: `cdrom0:\MODULES\CRI_ADXI.IRX;1` (57,917 bytes), the module's
  own name: **CRI_ADX_Driver**.
- It registers **one SIF server, sid `0x90000200`** (`[sif:target-rpc] register
  sid 0x90000200 func 0x00071A20 buf 0x0007C840 queue 0x0007CE00 sd
  0x0007CDB0`), and the game talks to it **only over SIF RPC**: the module
  publishes **no export table** (the IRX scan in `scripts/iop_irx_imports.py`
  finds none), so there is no by-address entry point to intercept.
- The function numbers the game calls (from the `[sif:target-rpc] finish`
  lines): **2, 3, 1024, 1025, 1027, 1032, 1033, 1034, 1036, 1037, 1058,
  1059**. The call trace prints each one's arguments (`[sif] call endpoint ...
  fn N send ... args ...`), so the protocol can be read off the log.
- Its imports: `libsd` (10 stubs), `sifcmd` (7), `sifman` (3), `sysclib` (8),
  `thbase` (5), `thsemap` (4), `intrman` (2), `loadcore` (1), `stdio` (1),
  `vblank` (1).
- What it does: it plays the movie. The native GS path expects the decoded
  frames in VRAM at FBP 280 and presents that surface
  (`[native-gl] sampled offscreen target FBP=280 ...`).
- It uses the PS2's **IPU** for the intra frames and the **RSP** for the rest.
  Both are emulated PS2 devices; the census does not count either, so the IPU
  emulation is a rule-4 gap of the same kind the AOT guest code was.

## The media

`cdrom0:\MOVIEPS2\LOGON.SFD;1` (3,321,856 bytes) is **not a CRI format**: it
is an **MPEG-2 program stream** (ISO/IEC 13818-1) with CRI's metadata in
private stream 2 and the audio in its own PES stream.

Measured by `tests/test_mpeg2_stream.cpp` (which parses the game's own movie):

| | |
|---|---|
| picture | 640x448, 30 fps, progressive |
| video PES packets | 1,485 |
| pictures | 3,854 (315 intra, 349 predicted) |
| audio PES packets | 136 |
| CRI metadata | private stream 2 (`CRITAGS`, `SFVFTRS`, `CRI_SVE 3.87`) |

The muxer writes MPEG-1 form pack headers (twelve bytes) and MPEG-1 form PES
headers, and the packet lengths run past the pack they sit in, so a payload
really ends at the next program stream start code.

## What is already in the tree

- `runtime/src/media/mpeg2.{hpp,cpp}`: the program stream parser (the
  elementary streams, the PES packets with their time stamps, the sequence
  header, the pictures).
- `runtime/src/media/vlc.hpp`: the variable length code lookups, now shared
  with the IPU model (`runtime/src/ipu/ipu.cpp` uses them).

## What is left

1. **The decoder** (the pixels): the slice layer, the macroblock layer, the
   inverse DCT and the frame assembly. The coefficient reading, the inverse
   DCT and the macroblock layer are in the IPU model
   (`runtime/src/ipu/ipu.cpp`: `read_dc_diff`, `read_coefficient`, `idct`,
   `decode_block`, `decode_macroblock`) and are standard MPEG-1/2 work, so the
   native decoder does the same from an elementary stream instead of from the
   chip's commands. The predicted frames need the motion compensation the RSP
   did.
2. **The oracle**: the IPU model decodes the same stream, so the native
   decoder can be measured against it **without a game launch** - the frame it
   produces and the frame the IPU model produces must match.
3. **The CRI audio**: the audio PES stream's payload is CRI's own; it goes to
   the native sound service (the voices and the ADPCM decoder are already
   native).
4. **The service**: a native SIF server for sid `0x90000200` with the twelve
   function numbers, and the native module registration that stops the guest
   IRX from loading at all (the loader already reserves the captured base
   `0x00071300` for a native replacement).
5. **The IPU out of the route**: once the native decoder decodes the movie,
   nothing uses the IPU model and it can go.

## The acceptance criteria for the movie path

- the game loads the movie and the picture is on screen (the user's launch);
- the census reads **0** for "guest modules loaded and able to run" and **0**
  for guest instructions on that path (the ADX module is a guest module, and
  `f6c190f` counts the compiled guest code, so a run that loads it says so);
- the IPU emulation is not on the route;
- a run record with the build commit, the flags, the log, the frames and the
  audio.
