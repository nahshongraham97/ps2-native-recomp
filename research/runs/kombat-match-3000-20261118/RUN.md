# Historical 3,000-frame scripted Kombat run — 2026-11-18

**Superseded acceptance wording:** This run records scripted combat and its
own counters/frames. It does not prove a physical keyboard or controller,
correct sound effects, card profile save/reload, every mode, or elimination
of every reachable PS2 device/memory path. Preserve the measurements below
as dated evidence; full acceptance is OPEN under the parent execution
contract.

## 1. Executive Summary & Project Milestones

- **Zero Emulation Enforced**: Strictly 0 guest EE/VU/IOP interpreter instructions, 0 emulated GS device calls, 0 emulated SPU2 chip ops, and strictly **0 IPU emulation** anywhere on the accepted route.
- **Zero Required Flags**: Out-of-the-box native retail execution of *Mortal Kombat: Armageddon* (`SLUS_214.10`) with zero command-line flags and zero required environment variables.
- **3,000-Frame Offline Combat Match Verified**: Full retail execution traversing legal screen, direct memory card verification, front-end navigation, Practice Mode selection, character selection (Scorpion vs. Sub-Zero), arena loading, and 3,000 frames of active combat (punches, kicks, spear combo, jump/crouch attacks, weapon switches) with registered damage on opponent health bars.
- **Character Select Rendering Perfected**:
  - `clut_hash` caching in `native_gl_backend.cpp` eliminates portrait palette collision. Row 4 accurately renders all 10 distinct portraits: Meat, Locked (Daegon), single Zombie Liu Kang, Kung Lao, Shang Tsung, Quan Chi, Shinnok, Raiden, Locked (Taven), single KAK Blue Statue.
  - Restricted clamp-extent expansion in `raster.cpp` strictly to region clamp mode (`s.wms == 2`, `s.wmt == 2`), fixing UV coordinate stretching and eliminating black facial clipping on Scorpion's mask and hood.
- **Host-Native Vector Particle Evaluator**:
  - Midway's internal dynamic heap particle generator call (`ra == 0x002E6180`) is intercepted and evaluated directly in native C++ in `dispatch.cpp`. Zero dynamic JIT compilation required.
- **Host-Native CRI_ADXI SIF Service (SID 0x90000200)**: Native C++ implementation of all 12 RPC functions without guest IOP execution or IPU emulation.
- **Direct Directory Memory Card Mapping (SID 0x80000400)**: Direct filesystem mapping to native Windows `saves/` directory, eliminating raw 8 MB memory card dependencies.
- **Subtractive & Reverse Subtractive Alpha Blending**: Support for `GL_FUNC_SUBTRACT` and `GL_FUNC_REVERSE_SUBTRACT` in OpenGL backend for complex GS ALPHA equations.

---

## 2. Pinned Build & Binary Identification

- **Branch**: `main` (commit `89b5999`)
- **Compiler**: Clang 23.1.3 (LLVM MinGW UCRT x86_64)
- **Runner Executable**: `external/forks/ps2-native-recomp/build_native_gl_full/ps2run.exe`
- **Milestone Package**: `milestone_build_kombat_practice/`
- **Target Game**: *Mortal Kombat: Armageddon* (`SLUS_214.10`, Entry: `0x00100008`)

---

## 3. Run Telemetry & Acceptance Gate Verification

Execution command:
```bash
ps2run.exe SLUS_214.10 --frames 3000 \
  --press 450:cross:20 --press 550:start:20 --press 600:cross:20 \
  --press 650:start:20 --press 750:start:20 --press 850:cross:20 \
  --press 950:down:15 --press 1020:cross:20 --press 1420:cross:20 \
  --press 1680:right:15 --press 1730:right:15 --press 1800:cross:20 \
  --press 1900:cross:20 --press 2000:cross:20 --press 2450:right:30 \
  --press 2500:square:15 --press 2540:triangle:15 --press 2580:circle:15 \
  --press 2620:cross:15 --press 2660:left:10 --press 2680:left:10 \
  --press 2700:square:15 --press 2760:up:15 --press 2775:square:15 \
  --press 2820:down+circle:20 --press 2880:l1:15 --press 2920:square:15 \
  --frames-dir shots --frame-at 2500 --frame-at 2600 --frame-at 2700 --frame-at 2800 --frame-at 2900 --frame-at 3000
```

### Accepted-Route PS2 API Census:
- **EE Live Translations**: 0 (PASS)
- **VU1 Programs Translated while Running**: 0 (PASS)
- **IOP Guest Modules Loaded and Able to Run**: 0 (PASS)
- **IOP Guest Instructions**: 0 (PASS)
- **VU Interpreter Fallbacks**: 0 (PASS)
- **SIF RPCs Dispatched Natively**: 697,227 (PASS)
- **GS Primitives Drawn**: 58,577,398 (PASS)
- **SPU2 Chip-Model Operations at Device Boundary**: 0 (PASS)
- **Native Audio Sample Triggers**: 2,966 (PASS)
- **CDVD Device Sector Reads**: 0 (PASS)
- **Final Exit Code**: 0 (PASS)
