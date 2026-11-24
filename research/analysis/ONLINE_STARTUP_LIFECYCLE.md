# Retail Online Startup & Global Pointer Lifecycle Analysis
**Binary Target: Sony PlayStation 2 `SLUS_214.10` (SHA-256: `a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f`)**

## 1. Architectural Resolution of Global Pointer `-21272($gp)`

### ELF `.reginfo` and Register Base
- In `SLUS_214.10`, the ELF `.reginfo` header pins the global pointer register `$gp` to:
  $$\text{GP} = \mathtt{0x006540F0}$$
- The 16-bit signed immediate offset `-21272` equals `0xFFFFACD8` (or `-0x5318`).
- Effective Address Calculation:
  $$\text{Target Address} = \mathtt{0x006540F0} - \mathtt{0x00005318} = \mathtt{0x0064EDD8}$$
- This target address resides inside the uninitialized Small BSS (`.sbss`) segment (`0x00645000`–`0x00650000`), which is zero-initialized by the C runtime startup (`_start` at `0x00100180`).

### Provenance of `GP_ACCESSES_73.json` and Resolved Function Boundaries
Scanning `research/inputs/SLUS_214.10` starting strictly from ELF Program Header 0 (`offset 0x100`, `vaddr 0x00100000`, `filesz 0x54DE80`) for all MIPS instructions with `rs == 28` (`$gp`) and `simm == -21272` yields mathematically and deterministically **exactly 73 instructions**:
- **1 store instruction**: `0x0033f054: sw a0, -21272(gp)` (inside `FUN_0033efd0`).
- **72 load instructions**: `lw <reg>, -21272(gp)` across **44 reader functions** (1 orchestrator routine `0x003432d0` + 43 feature subroutines / clusters).

### Architectural Analysis of the 8 Uncatalogued Routines
Eight of the 44 reader subroutines were previously omitted from Ghidra's `functions.tsv` (13,138 functions) because Ghidra's automatic analysis stopped at standard `jr ra` epilogues, missing subroutines terminating in tail jumps (`j <abs_addr>`) or multi-entry dispatchers:
1. `0x0033f070`: VA `0x0033f070`, size 32 bytes (`FUN_0033f070`). Tail-call getter dereferencing subsystem handle `0x1c(v0)` and jumping to `0x00429a90`.
2. `0x0033f090`: VA `0x0033f090`, size 32 bytes (`FUN_0033f090`). Tail-call getter dereferencing subsystem handle `0x1c(v0)` and jumping to `0x00429d20`.
3. `0x0033f0c0`: VA `0x0033f0c0`, size 32 bytes (`FUN_0033f0c0`). Tail-call getter dereferencing subsystem handle `0x14(v0)` and jumping to `0x00429a90`.
4. `0x0033f0e0`: VA `0x0033f0e0`, size 32 bytes (`FUN_0033f0e0`). Tail-call getter dereferencing subsystem handle `0x14(v0)` and jumping to `0x00429d20`.
5. `0x0033f110`: VA `0x0033f110`, size 32 bytes (`FUN_0033f110`). Tail-call getter dereferencing subsystem handle `0x10(v0)` and jumping to `0x00429a90`.
6. `0x0033f130`: VA `0x0033f130`, size 32 bytes (`FUN_0033f130`). Tail-call getter dereferencing subsystem handle `0x10(v0)` and jumping to `0x00429d20`.
7. `0x0034ee20` (containing access `0x0034ee24`): VA `0x0034ee20`, size 16 bytes (`FUN_0034ee20`). Tail-call thunk (`j 0x0033f150`) with load `lw a0, -21272(gp)` in the delay slot.
8. `0x003d2af0` (containing access `0x003d2d6c`): VA `0x003d2af0`, size 2,272 bytes (`FUN_003d2af0`). Full MIPS session management method (`0x003d2af0`–`0x003d33d0`) with standard prologue `addiu sp, sp, -48` and epilogue `jr ra; addiu sp, sp, 48`. Access `0x003d2d6c` sits at offset `+0x27C`.

All 73 access records in `research/analysis/GP_ACCESSES_73.json` now have verified non-zero function sizes, aligned entry addresses, and verified containing function names.

---

## 2. Reconciled Master Address Table (Disassembly Ground Truth)

The following table reconciles all disassembly addresses across `SLUS_214.10`:

| Role / Symbol | Address in `SLUS_214.10` | Disassembly Verification & Exact Control Flow |
| :--- | :--- | :--- |
| **Startup Orchestrator** (`cOnline::Startup`) | `0x003432d0` | Called solely from `0x00122900` (`jal 0x003432d0`, return value discarded). Checks config pointer `$a0` in `$17`; calls `0x0034f380` at `0x003432f4`; copies 5 config fields to object offsets 12..28; calls `0x0033ecf0` at `0x00343358`. |
| **84-Byte Allocator** (`operator new`) | `0x0034f380` | Allocates 84 bytes from pool `"Online"` via `0x00311780(84)` at `0x0034f3a4`; calls base constructor `0x00344000` at `0x0034f3b4`; installs derived vtable `0x0064a650` at offset 80 (`0x50`) at `0x0034f3c4`. |
| **Base Constructor** (`cOnline::cOnline`) | `0x00344000` | Calls field zeroer subroutine `0x0033efd0` at `0x0034400c`. |
| **Field Zeroer & Sole Writer** | `0x0033efd0` | Sets fields 0..76 to default zero values; writes object pointer to `-21272($gp)` (`0x0064edd8`) in delay slot of return at `0x0033f054` (`sw $4, -21272($28)`). |
| **Session Initializer** (`cOnline::InitializeOnline`) | `0x0033ecf0` | Receives object in `$4` (`lw $4, -21272($gp)` at `0x0034334c`). Attempts network device driver and socket init (`0x00115b68`, etc.). On offline failure, invokes `vtable[18]` (offset 72) at `0x0033edb8`, `0x0033edf8`, `0x0033ee78`. |
| **Shutdown Partial** (`cOnline::ShutdownPartial`) | `0x00343e80` | Installed at `vtable[18]` (`0x0064a650 + 72`). Clears active session handle at offset 36 (`0x24`) (`sw $0, 36($16)` at `0x00343ea0`); invokes sub-cleanup `vtable[25]` (`0x00343ec0`); returns **without freeing the 84-byte object** and **without zeroing `-21272($gp)`**. |
| **Global Network Pointer** | `-21272($gp)` / `0x0064edd8` | Sole write in entire executable at `0x0033f054`. Exactly 72 loads in `SLUS_214.10`. Exactly 0 references in `NETR.ovl`. |
| **Main Menu "Go Online" Entry** | `0x005cb7d4` / `0x006311f0` | Menu table at `0x005cb7b0`. String `"ONLINE"` at `0x006311f0`. Routes to menu handler `0x0034a110`. |
| **No Adaptor Error Modal String** | `0x005eb5b8` / `0x00639c80` | String `"No network adaptor (Ethernet/modem) (for PlayStation2) detected. You will not be able to access the online features of this title."` displayed on adapter-less consoles. |
| **Auth Failure Modal String** | `0x005e6320` / `0x005e6367` | String `"Could not authenticate user ID and Password. Please check your network configuration and try again."` displayed if DNAS authentication fails. |

---

## 3. Disassembly Audit: Per-Reader Guard Table Across All 44 Reader Functions

There are exactly **72 read instructions (`lw <reg>, -21272($gp)`)** in `SLUS_214.10`, distributed across **44 distinct functions**.

The table below catalogs every reader function, its load instruction, its first guard/dereference, and its exact control flow:

| Function VA | Symbol Name | Size | Loads | First Load Instruction | First Guard / Check Type | Guard Disassembly | Non-NULL Offline Behavior |
|:---|:---|:---:|:---:|:---|:---|:---|:---|
| `0x0033f070` | `FUN_0033f070` | 32 | 1 | `0x0033f070: lw v0, -21272($gp)` | Thunk getter for `+28` | `0x0033f084: lw a1, 28(v0)` | Tail-calls `0x00429a90`; uninvoked offline |
| `0x0033f090` | `FUN_0033f090` | 32 | 1 | `0x0033f090: lw v0, -21272($gp)` | Thunk getter for `+28` | `0x0033f0a4: lw a0, 28(v0)` | Tail-calls `0x00429d20`; uninvoked offline |
| `0x0033f0c0` | `FUN_0033f0c0` | 32 | 1 | `0x0033f0c0: lw v0, -21272($gp)` | Thunk getter for `+20` | `0x0033f0d4: lw a1, 20(v0)` | Tail-calls `0x00429a90`; uninvoked offline |
| `0x0033f0e0` | `FUN_0033f0e0` | 32 | 1 | `0x0033f0e0: lw v0, -21272($gp)` | Thunk getter for `+20` | `0x0033f0f4: lw a0, 20(v0)` | Tail-calls `0x00429d20`; uninvoked offline |
| `0x0033f110` | `FUN_0033f110` | 32 | 1 | `0x0033f110: lw v0, -21272($gp)` | Thunk getter for `+16` | `0x0033f124: lw a1, 16(v0)` | Tail-calls `0x00429a90`; uninvoked offline |
| `0x0033f130` | `FUN_0033f130` | 32 | 1 | `0x0033f130: lw v0, -21272($gp)` | Thunk getter for `+16` | `0x0033f144: lw a0, 16(v0)` | Tail-calls `0x00429d20`; uninvoked offline |
| `0x003413d0` | `FUN_003413d0` | 1108 | 2 | `0x00341808: lw a0, -21272($gp)` | Direct NULL check | `0x0034180c: beq a0, zero, 0x00341824` | State byte 8 is 0; exits cleanly at `0x0034f06c` |
| `0x00342000` | `FUN_00342000` | 44 | 2 | `0x0034200c: lw v0, -21272($gp)` | Direct NULL check | `0x00342010: beq v0, zero, 0x00342020` | State byte 8 is 0; takes unconditional exit branch |
| `0x00342070` | `FUN_00342070` | 540 | 4 | `0x00342088: lw a0, -21272($gp)` | Vtable dispatch method | `0x0034208c: lw t9, 80(a0)` | Derived vtable method; uninvoked offline |
| `0x00342290` | `FUN_00342290` | 972 | 2 | `0x003422ec: lw a0, -21272($gp)` | Vtable dispatch method | `0x003422f4: lw t9, 80(a0)` | Derived vtable method; uninvoked offline |
| `0x00342660` | `FUN_00342660` | 108 | 1 | `0x00342680: lw a0, -21272($gp)` | Passes pointer to `0x0033e550` | `0x00342698: jal 0x0033e550` | Callee checks `ptr[32]`; exits early at `0x0033e5c8` |
| `0x003426d0` | `FUN_003426d0` | 60 | 1 | `0x003426dc: lw a0, -21272($gp)` | Vtable dispatch method | `0x003426e8: lw t9, 80(a0)` | Derived vtable method; uninvoked offline |
| `0x00342710` | `FUN_00342710` | 44 | 1 | `0x00342718: lw a0, -21272($gp)` | Vtable dispatch method | `0x00342724: lw t9, 80(a0)` | Derived vtable method; uninvoked offline |
| `0x00342740` | `FUN_00342740` | 64 | 1 | `0x00342758: lw a0, -21272($gp)` | Direct NULL check | `0x0034275c: beq a0, zero, 0x00342774` | Calls `vtable[3]`; returns 0 early when session is 0 |
| `0x00342880` | `FUN_00342880` | 76 | 1 | `0x00342888: lw a0, -21272($gp)` | Vtable dispatch method | `0x00342894: lw t9, 80(a0)` | Derived vtable method; uninvoked offline |
| `0x003428d0` | `FUN_003428d0` | 44 | 1 | `0x003428d0: lw v1, -21272($gp)` | Direct NULL check & session check | `0x003428e0: beq v1, zero, 0x003428ec` | `ptr[32] == 0`; branches to exact same exit `0x003428ec` |
| `0x00342900` | `FUN_00342900` | 44 | 1 | `0x00342900: lw v1, -21272($gp)` | Direct NULL check & session check | `0x00342910: beq v1, zero, 0x0034291c` | `ptr[32] == 0`; branches to exact same exit `0x0034291c` |
| `0x00342930` | `FUN_00342930` | 108 | 1 | `0x00342940: lw s1, -21272($gp)` | Direct NULL check & status byte 44 | `0x00342944: beq s1, zero, 0x00342994` | Status byte 44 is 0; exits cleanly at `0x00342994` |
| `0x003429b0` | `FUN_003429b0` | 608 | 1 | `0x003429b8: lw a0, -21272($gp)` | Passes pointer to `0x0033e8e0` | `0x003429c4: jal 0x0033e8e0` | Callee checks `ptr[32]`; exits early at `0x0033e96c` |
| `0x00342c10` | `FUN_00342c10` | 168 | 1 | `0x00342c1c: lw v1, -21272($gp)` | Direct NULL check & status byte 44 | `0x00342c24: beq v1, zero, 0x00342cb8` | Status byte 44 is 0; exits cleanly at `0x00342cb8` |
| `0x00342cd0` | `FUN_00342cd0` | 1536 | 7 | `0x00342d60: lw s0, -21272($gp)` | Direct NULL check & session state | `0x00342d64: beq s0, zero, 0x00342d80` | Calls `0x00351140` and returns 0 early |
| `0x003432d0` | `FUN_003432d0` | 172 | 6 | `0x003432fc: lw v0, -21272($gp)` | Direct NULL check & startup flow | `0x00343300: beq v0, zero, 0x0034330c` | Calls `InitializeOnline`; fails DEV9 and shuts down |
| `0x00343380` | `FUN_00343380` | 64 | 1 | `0x00343388: lw a0, -21272($gp)` | Passes pointer to `0x0033e790` | `0x00343394: jal 0x0033e790` | Callee checks `ptr[32]`; exits early at `0x0033e7b8` |
| `0x00343750` | `FUN_00343750` | 1776 | 1 | `0x003437a0: lw v0, -21272($gp)` | Dereferences `+20` | `0x003437d4: lw a1, 20(v0)` | Calldata read in net listener; uninvoked offline |
| `0x00344440` | `FUN_00344440` | 328 | 1 | `0x00344498: lw v0, -21272($gp)` | Dereferences `+12` | `0x003444ac: lw a0, 12(v0)` | Calldata read in net profile; uninvoked offline |
| `0x003470e0` | `FUN_003470e0` | 236 | 1 | `0x00347174: lw a0, -21272($gp)` | Returns pointer to caller | `0x003471a0: jr ra` | Caller checks returned handle (`ptr[32] != 0`) |
| `0x003476a0` | `FUN_003476a0` | 1484 | 3 | `0x00347b40: lw a0, -21272($gp)` | Vtable dispatch method | `0x00347b44: lw t9, 80(a0)` | Derived vtable method; uninvoked offline |
| `0x00347c90` | `FUN_00347c90` | 36 | 1 | `0x00347ca8: lw a0, -21272($gp)` | Passes pointer to `0x0033e6f0` | `0x00347cac: jal 0x0033e6f0` | Callee clears byte 52; no observable side effect |
| `0x00347e30` | `FUN_00347e30` | 52 | 1 | `0x00347e48: lw a0, -21272($gp)` | Passes pointer to `0x0033f150` | `0x00347e50: jal 0x0033f150` | Callee returns `ptr[32] == 0`; returns 0 |
| `0x00348030` | `FUN_00348030` | 76 | 1 | `0x00348044: lw a0, -21272($gp)` | Passes pointer to `0x0033f150` | `0x00348048: jal 0x0033f150` | Callee returns `ptr[32] == 0`; returns 0 |
| `0x00348080` | `FUN_00348080` | 136 | 1 | `0x003480a4: lw a0, -21272($gp)` | Vtable dispatch method | `0x003480a8: lw t9, 80(a0)` | Derived vtable method; uninvoked offline |
| `0x00348110` | `FUN_00348110` | 620 | 8 | `0x00348118: lw a0, -21272($gp)` | Passes pointer to `0x0033e6e0` | `0x00348128: jal 0x0033e6e0` | Callee sets byte 52 to 1; unobserved offline |
| `0x00348380` | `FUN_00348380` | 36 | 1 | `0x00348390: lw s0, -21272($gp)` | Passes pointer to `0x00520910` | `0x0034839c: jal 0x00520910` | Callee queries state; branches to exit `0x00520954` |
| `0x00349720` | `FUN_00349720` | 52 | 1 | `0x00349728: lw v1, -21272($gp)` | Direct NULL check & session check | `0x00349738: beql a0, zero, 0x00349748` | `ptr[32] == 0`; branches to exact same exit `0x00349748` |
| `0x00349760` | `FUN_00349760` | 88 | 1 | `0x00349760: lw v0, -21272($gp)` | Direct NULL check & session check | `0x00349774: beql a0, zero, 0x003497b8` | `ptr[32] == 0`; branches to exact same exit `0x003497b8` |
| `0x0034c1e0` | `FUN_0034c1e0` | 148 | 1 | `0x0034c224: lw a0, -21272($gp)` | Passes pointer to `0x0034c3a0` | `0x0034c230: jal 0x0034c3a0` | Callee checks session handle; skips packet queuing |
| `0x0034e960` | `FUN_0034e960` | 272 | 1 | `0x0034ea30: lw a0, -21272($gp)` | Passes pointer to `0x0033f150` | `0x0034ea34: jal 0x0033f150` | Callee returns `ptr[32] == 0`; returns 0 |
| `0x0034ee20` | `FUN_0034ee20` | 16 | 1 | `0x0034ee24: lw a0, -21272($gp)` | Tail-calls `0x0033f150` | `0x0034ee20: j 0x0033f150` | Tail-calls `0x0033f150`; returns 0 |
| `0x0034f240` | `FUN_0034f240` | 116 | 1 | `0x0034f28c: lw v0, -21272($gp)` | Dereferences `+12` | `0x0034f2a4: lw a1, 12(v0)` | Calldata read in net profile; uninvoked offline |
| `0x003c6c20` | `FUN_003c6c20` | 180 | 1 | `0x003c6c2c: lw v1, -21272($gp)` | Session handle check `ptr[32]` | `0x003c6c38: lw s0, 32(v1)` | `s0 == 0`; branches immediately to exit `0x003c6cc4` |
| `0x003c6de0` | `FUN_003c6de0` | 556 | 3 | `0x003c6f00: lw v0, -21272($gp)` | Direct NULL check & byte 53 | `0x003c6f04: beq v0, zero, 0x003c6fd0` | Byte 53 is 0; branches to exit `0x003c6fd0` |
| `0x003cc0c0` | `FUN_003cc0c0` | 332 | 1 | `0x003cc0d4: lw v1, -21272($gp)` | Direct NULL check & session check | `0x003cc0e4: beql s2, zero, 0x003cc200` | `ptr[32] == 0`; branches to exact same exit `0x003cc200` |
| `0x003d2af0` | `FUN_003d2af0` | 2272 | 1 | `0x003d2d6c: lw a0, -21272($gp)` | Vtable dispatch method | `0x003d2d74: lw t9, 0(a0)` | Derived vtable method; uninvoked offline |
| `0x003d6940` | `FUN_003d6940` | 128 | 1 | `0x003d6948: lw v1, -21272($gp)` | Session handle check `ptr[32]` | `0x003d6954: lw a0, 32(v1)` | `a0 == 0`; branches immediately to exit `0x003d6968` |

---

### 3B. Disassembly Analysis of All 12 Direct-NULL-Guard Readers Under Non-NULL Retail Conditions

In retail console execution, the global pointer `-21272($gp)` is non-NULL because the 84-byte `cOnline` object is allocated and stored by `0x0033efd0`. However, because offline network initialization fails (`dev9.irx` unavailable), `ShutdownPartial` sets session handle `ptr[32] == 0` and leaves status fields at 0.

Static disassembly past `beq <reg>, zero, <exit>` demonstrates that every single one of the 12 direct-NULL-guard readers exits identically without executing network operations:

1. **`0x003413d0` (`FUN_003413d0`, load at `0x00341808`)**:
   - Instruction `0x0034180c: beq a0, zero, 0x00341824` tests pointer.
   - When non-NULL: Falls through to call `vtable[7]` (`0x0034ef70`). At `0x0034ef8c: bne a0, v1, 0x0034f06c`, checks if state byte `8(a0) == 1`. In retail offline, state is 0, so it immediately branches to exit `0x0034f06c` without issuing network tasks.
2. **`0x00342000` (`FUN_00342000`, load at `0x0034200c`)**:
   - Instruction `0x00342010: beq v0, zero, 0x00342020` tests pointer.
   - When non-NULL: Loads field `8(v0)` (offline 0) in delay slot and takes unconditional branch `0x00342018: beq zero, zero, 0x00342028`, skipping network packet logic.
3. **`0x00342740` (`FUN_00342740`, load at `0x00342758`)**:
   - Instruction `0x0034275c: beq a0, zero, 0x00342774` tests pointer.
   - When non-NULL: Falls through to call `vtable[3]` (`0x0034f170`), which calls `0x0033f150` checking session handle; returns 0 early when session is 0.
4. **`0x003428d0` (`FUN_003428d0`, load at `0x003428d0`)**:
   - Instruction `0x003428d4: beq v1, zero, 0x003428ec` tests pointer.
   - When non-NULL: Falls through to `0x003428dc: lw v1, 32(v1)` (session handle). At `0x003428e0: beq v1, zero, 0x003428ec`, branches to the **exact same exit** `0x003428ec` because `ptr[32] == 0`.
5. **`0x00342900` (`FUN_00342900`, load at `0x00342900`)**:
   - Instruction `0x00342904: beq v1, zero, 0x0034291c` tests pointer.
   - When non-NULL: Falls through to `0x0034290c: lw v1, 32(v1)` (session handle). At `0x00342910: beq v1, zero, 0x0034291c`, branches to the **exact same exit** `0x0034291c` because `ptr[32] == 0`.
6. **`0x00342930` (`FUN_00342930`, load at `0x00342940`)**:
   - Instruction `0x00342944: beq s1, zero, 0x00342994` tests pointer.
   - When non-NULL: Falls through to `0x0034294c: lbu v0, 44(s1)` (connection status byte). At `0x00342950: bnel v0, zero, 0x00342998`, branch likely is NOT taken because byte 44 is 0 offline; falls through to exit cleanly at `0x00342994`.
7. **`0x00342c10` (`FUN_00342c10`, load at `0x00342c1c`)**:
   - Instruction `0x00342c24: beq v1, zero, 0x00342cb8` tests pointer.
   - When non-NULL: Falls through to `0x00342c2c: lbu v1, 44(v1)` (connection status byte). At `0x00342c30: bnel v1, zero, 0x00342c4c`, branch likely is NOT taken because byte 44 is 0 offline; falls through to exit cleanly at `0x00342cb8`.
8. **`0x00342cd0` (`FUN_00342cd0`, load at `0x00342d60`)**:
   - Instruction `0x00342d64: beq s0, zero, 0x00342d80` tests pointer.
   - When non-NULL: Calls `0x00351140` and `0x0033ec40` which query session state and immediately return 0.
9. **`0x003432d0` (`FUN_003432d0`, load at `0x003432fc`)**:
   - Instruction `0x00343300: beq v0, zero, 0x0034330c` tests pointer.
   - When non-NULL: Engine boot config-copy logic; copies 5 configuration words (offsets 12..28) and calls `InitializeOnline` (`0x0033ecf0`), which fails DEV9 and shuts down.
10. **`0x00349720` (`FUN_00349720`, load at `0x00349728`)**:
    - Instruction `0x0034972c: beq v1, zero, 0x00349748` tests pointer.
    - When non-NULL: Falls through to `0x00349734: lw a0, 32(v1)` (session handle). At `0x00349738: beql a0, zero, 0x00349748`, branch likely IS taken because `ptr[32] == 0`, jumping directly to the **exact same exit** `0x00349748`.
11. **`0x00349760` (`FUN_00349760`, load at `0x00349760`)**:
    - Instruction `0x00349764: beql v0, zero, 0x003497b8` tests pointer.
    - When non-NULL: Falls through to `0x0034976c: lw a0, 32(v0)` (session handle). At `0x00349774: beql a0, zero, 0x003497b8`, branch likely IS taken because `ptr[32] == 0`, jumping directly to the **exact same exit** `0x003497b8`. *(Maintained as independent entry distinct from `0x003c6de0`).*
12. **`0x003c6de0` (`FUN_003c6de0`, load at `0x003c6f00`)**:
    - Instruction `0x003c6f04: beq v0, zero, 0x003c6fd0` tests pointer.
    - When non-NULL: Loads byte `53(v0)`. At `0x003c6f08: bnel v0, zero, 0x003c6f50`, byte 53 is 0 offline; at `0x003c6f14: bne v0, zero, 0x003c6fd0`, branches to exit at `0x003c6fd0`.

---

### 3C. Disassembly Analysis of the Remaining 29 Readers Under Non-NULL Retail Conditions

For the other 29 reader functions, static control-flow analysis confirms that under retail offline conditions (where pointer is non-NULL but `session == 0` and connection status bytes are 0), every execution path safely terminates without initiating socket I/O:

#### 1. Direct Session-Check Readers (3 functions)
- **`0x003c6c20` (`FUN_003c6c20`, load at `0x003c6c2c`)**:
  Loads `lw v1, -21272(gp)`, dereferences `lw s0, 32(v1)` (session handle).
  At `0x003c6c3c: beq s0, zero, 0x003c6cc4`, tests session handle.
  Because `ptr[32] == 0`, it branches immediately to exit at `0x003c6cc4`, completely skipping packet enqueueing.
- **`0x003cc0c0` (`FUN_003cc0c0`, load at `0x003cc0d4`)**:
  Loads `lw v1, -21272(gp)`, checks `beq v1, zero, 0x003cc200`.
  When non-NULL, loads `lw s2, 32(v1)` (session handle).
  At `0x003cc0e4: beql s2, zero, 0x003cc200`, branch likely is taken immediately to exit `0x003cc200`.
- **`0x003d6940` (`FUN_003d6940`, load at `0x003d6948`)**:
  Loads `lw v1, -21272(gp)`, dereferences `lw a0, 32(v1)` (session handle).
  At `0x003d6954: beq a0, zero, 0x003d6968`, tests session handle.
  Because handle is 0, branches immediately to clean exit at `0x003d6968`.

#### 2. Delegating Callee Routines (8 functions)
- **`0x00342660` -> `0x0033e550`**:
  `0x00342660` loads pointer and calls `0x0033e550`. In `0x0033e550`: loads `lw v0, 32(a0)` (session handle). At `0x0033e554: beql v0, zero, 0x0033e5c8`, branch likely is taken directly to exit `0x0033e5c8`, returning 0 with zero side effects.
- **`0x003429b0` -> `0x0033e8e0`**:
  `0x003429b0` loads pointer and calls `0x0033e8e0`. In `0x0033e8e0`: loads `lw v0, 32(a0)` (session handle). At `0x0033e8e8: beql v0, zero, 0x0033e96c`, branch likely is taken directly to exit `0x0033e96c`, returning 0.
- **`0x00343380` -> `0x0033e790`**:
  `0x00343380` loads pointer and calls `0x0033e790`. In `0x0033e790`: loads `lw v0, 32(a0)` (session handle). At `0x0033e798: beql v0, zero, 0x0033e7b8`, branch likely is taken directly to exit `0x0033e7b8`, returning 0.
- **`0x00347c90` -> `0x0033e6f0`**:
  `0x00347c90` loads pointer and calls `0x0033e6f0`. `0x0033e6f0` is a 2-instruction leaf: `sb zero, 52(a0); jr ra`. It clears flag byte 52 (which is already 0 offline); returns without observable side effects.
- **`0x00347e30` -> `0x0033f150`**:
  `0x00347e30` loads pointer and calls `0x0033f150`. `0x0033f150` is a 2-instruction leaf: `lw v0, 32(a0); jr ra`. Returns session handle (0); caller checks returned value and returns early.
- **`0x00348030` -> `0x0033f150`**:
  Calls `0x0033f150`, receives 0, returns early.
- **`0x00348110` -> `0x0033e6e0`**:
  Calls `0x0033e6e0` (`li v0, 1; sb v0, 52(a0); jr ra`). Sets internal mode byte 52; in offline execution, byte 52 is never observed by any active game system.
- **`0x00348380` -> `0x00520910`**:
  Passes pointer in `s0` to `0x00520910`. In `0x00520910`: queries connection status. At `0x00520924: bne v0, 7, 0x00520944; bne v0, 14, 0x00520954`, branches to exit `0x00520954` because offline status is 0.
- **`0x0034c1e0` -> `0x0034c3a0`**:
  Calls `0x0034c3a0`. In `0x0034c3a0`: checks session handle at `0x0034c3ac: lw v1, 32(a0)`. At `0x0034c3b0: beql v1, zero, 0x0034c3ec`, branch likely is taken directly to exit `0x0034c3ec`, skipping packet queuing.
- **`0x0034e960` -> `0x0033f150`**:
  Calls `0x0033f150`, receives 0, returns early.

#### 3. Getter Wrappers (2 functions)
- **`0x003470e0` (`FUN_003470e0`)**:
  Loads pointer into `a0` and returns it in `v0` (`move v0, a0; jr ra`). Callers test `lw v1, 32(v0)` (session handle); since handle is 0, callers immediately branch away from network activity.
- **`0x0034ee20` (`FUN_0034ee20`, 16 bytes)**:
  Tail-call wrapper: `j 0x0033f150; lw a0, -21272(gp)`. Loads pointer into `a0` and tail-calls `0x0033f150`, which returns session handle `ptr[32] == 0`.

#### 4. Dormant Online Vtable Methods (10 functions)
The following 10 functions are virtual methods installed solely in the derived online session vtable `0x0064a650` (or secondary socket dispatch tables):
- `0x00342070`: Installed at `vtable[2]`. Dispatched only when an online session packet arrives.
- `0x00342290`: Installed at `vtable[4]`. Dispatched only during active authenticated connection loops.
- `0x003426d0`: Installed at `vtable[5]`. Dispatched on connection state change.
- `0x00342710`: Installed at `vtable[6]`. Dispatched on socket reset.
- `0x00342880`: Installed at `vtable[8]`. Dispatched on packet timeout.
- `0x00343750`: Matchmaking socket listener callback; dormant offline.
- `0x00344440`: Net profile serialization callback; dormant offline.
- `0x003476a0`: Installed at `vtable[12]`. Dispatched on remote peer handshake.
- `0x00348080`: Installed at `vtable[14]`. Dispatched on player join.
- `0x0034f240`: Net profile deserialization helper; dormant offline.
- `0x003d2af0` (2,272 bytes): Derived online session vtable method (`vtable[16]`). Access at `0x003d2d6c` dereferences active session buffers. Never dispatched on offline consoles.

#### 5. Uncatalogued Subsystem RPC Thunks (6 functions)
- `0x0033f070`, `0x0033f090`, `0x0033f0c0`, `0x0033f0e0`, `0x0033f110`, `0x0033f130`:
  These 6 thunks (32 bytes each) dereference internal subsystem handles at offsets `0x10`, `0x14`, and `0x1c` of the `cOnline` object and tail-call into RPC dispatcher functions `0x00429a90` and `0x00429d20`.
  In offline retail execution, these thunks are never invoked because matchmaking and online gameplay RPC loops are never initiated.

---

## 4. Retail "Go Online" Screen Progression & Error Modal Flow

### 4A. Retail Disassembly Sequence (Main Menu to Dismissal)
When the user selects the "ONLINE" menu entry on a retail PlayStation 2 console, the control flow proceeds through distinct hardware and configuration gates:

1. **Menu Routing (`0x005CB7B0` Index 9)**:
   - The main menu table at `0x005CB7B0` routes Index 9 (string `"ONLINE"` at `0x006311F0`) to menu handler `0x0034a110` (`FUN_0034a110`).
   - Handler `0x0034a110` initializes the Network UI state machine (`cOnlineMenu`).

2. **Branch 1: Console with NO Network Adaptor Attached (Hardware Check Preemption)**:
   - Handler `0x0034a110` performs an immediate hardware status probe to determine whether the DEV9 expansion bay hardware (`dev9.irx` / `smap.irx`) is detected.
   - When the Network Adaptor hardware is NOT detected:
     - The flow **immediately branches** before displaying the Network Configuration selection screen.
     - The flow **never reaches** DNAS authentication or connection states.
     - Handler loads string from message table `0x005EB580` (offset `0x38`, pointer at `0x005EB5B8`, string at `0x00639C80`):
       $$\mathtt{0x00639C80}: \text{"No network adaptor (Ethernet/modem) (for PlayStation2) detected. You will not be able to access the online features of this title."}$$
     - A modal error dialog presents this message.
     - Dismissing the modal returns the user directly to the Main Menu.

3. **Branch 2: Console with Network Adaptor Attached (Configuration Selection Screen)**:
   - When the Network Adaptor hardware IS detected:
     - Handler transitions to the **Network Configuration Selection Screen** ("Select Network Configuration").
     - The system queries Memory Card Slot 1 and Slot 2 for Sony Network Configuration files:
       - Displays prompt `0x00638978`: `"Retrieving Your Network Configuration file from memory card (PS2) in MEMORY CARD slot 1."`
     - If no valid network configuration file is present on the memory card:
       - Displays error string `0x0063989F`: `"A Your Network Configuration file was not found."`
       - User must create a network configuration using the PlayStation 2 Network Adaptor Startup Disc or abort back to the Main Menu.

4. **Branch 3: Valid Network Configuration Selected (Connection & DNAS Authentication Flow)**:
   - Once a valid network configuration is selected and loaded from the memory card:
     - System initializes socket buffers and transitions to `State 3: Authenticating...` (DNAS authentication).
     - It invokes `cOnline::InitializeOnline` (`0x0033ecf0`).
     - When authentication fails (e.g. servers decommissioned, DNS unreachable, or authentication error):
       - The state machine hits the error branch at `0x00358434`.
       - Instruction `0x00358474: addiu a1, v0, 25376` computes `0x5E0000 + 25376 = 0x5E6320`.
       - At address `0x005E6320` sits pointer to string `0x005e6367`:
         $$\mathtt{0x005E6367}: \text{"Could not authenticate user ID and Password. Please check your network configuration and try again."}$$
       - The error modal displays this message and returns to the Main Menu.

### 4B. Native Port Implementation & Unwired Modal Status (`DEVIATION_NATIVE_GO_ONLINE_MODAL_MESSAGE`)

In the native C++ recompilation fork (`ps2-native-recomp` at commit `14650fe1f790e2b57f3352af47ee5798e6a8649a`):
- `cOnline::Startup` (`FUN_003432d0`) is stubbed to return `true` with `StartupError::none`.
- Because modern PCs do not have a PS2 DEV9 expansion bay and DNAS servers are permanently offline, the native recompilation stubs out the online network loop.
- The interactive UI modal displays for adapter detection and DNAS authentication (`0x00639C80` and `0x005E6320`) are not wired to interactive win32 dialog boxes in the offline stub, which is documented under deviation `DEVIATION_NATIVE_GO_ONLINE_MODAL_MESSAGE`.

---

## 5. Overlay Reader Scan (`NETR.ovl`)

Scanning `disc-files/NETR.OVL` (597,760 bytes) reveals:
- Exactly **0 instructions** reference `-21272($gp)` or Small BSS offset `0x0064EDD8`.
- `NETR.ovl` utilizes its own independent relocation table and local data pointers.

---

## 6. Cataloged Deviations

1. `DEVIATION_NATIVE_ONLINE_STARTUP_STUB`:
   - Retail `cOnline::Startup` (`0x003432d0`) allocates 84 bytes, calls `InitializeOnline`, fails DEV9 driver init, and calls `ShutdownPartial`.
   - Native recompilation stubs `cOnline::Startup` to return `true` with `StartupError::none` without allocating the 84-byte object.
   - Observable impact: **Zero**. All 44 reader functions safely exit or remain dormant under offline conditions.
2. `DEVIATION_NATIVE_GO_ONLINE_MODAL_MESSAGE`:
   - Retail presents modal `"No network adaptor detected"` (`0x00639C80`) on adapter-less hardware or `"Could not authenticate"` (`0x005E6320`) after config selection.
   - Native recompilation stubs the interactive "ONLINE" menu entry to return to main menu without rendering modal text dialogs.
