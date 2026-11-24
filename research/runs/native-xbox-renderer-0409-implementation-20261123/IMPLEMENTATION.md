# Native 0409 shared texture dictionary service -- implementation scope

Stage: compiled and tested component through actual Core/Main prefix.
Not a complete whole-scene renderer, graphics driver, oracle, or whole-game acceptance result.
Acceptance status: **OPEN** (0409 remains component-verified awaiting subsequent plugins 0412, connected render-target, first-scene, and whole-engine integration).

## Authority and binary identities

Xbox default.xbe and PS2 SLUS_214.10 establish the texture dictionary contracts and plugin registration sequence:
- Xbox default.xbe: SHA-256 78f99fca447fddc5c5116c09d27e6f1c04af8572dace94f559966588c6f5e393
- PS2 SLUS_214.10: SHA-256 a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f

Native source is runtime/include/mka/retail_renderer_texture_dictionaries.hpp and
runtime/src/native/retail_renderer_texture_dictionaries.cpp, connected through the existing
Core header/ordered 0409 factory and standard CMake source/test staging.

No guest console emulation (g_ram, g_spr, g_memory, emulated SPU2/IOP, JITs) is used.
No console dictionary headers, raw pointer arrays, or console freelist blocks are imported into host memory.

## Binary source words and disassembly

### Xbox default.xbe source words
- 0x002c9840 (constructor construct_core_plugin_0409):
  0x0824448b 0x8343f8a3 0x00c2e800 0x4c8b0000 0xd8f70424 0xc123c01b 0xccccccc3 0xcccccccc
  Disassembly reads argument from [esp+8], writes global plugin offset 0x008343f8, calls 0x002c9910 (module open), and conditionally returns dictionary object pointer from [esp+4].
- 0x002c9860 (destructor destroy_core_plugin_0409):
  0x00002be8 0x24448b00 0xccccc304 0xcccccccc 0x000001b8 0xccccc300 0xcccccccc 0xcccccccc
  Disassembly calls 0x002c9890 (module close), loads return pointer from [esp+4], and returns.
- 0x002c9910 (module open):
  0x5c75e4a1 0x7563c085 0x5a6a74a1 0xe850005a 0x0000473c 0x8304c483 ...
  Checks module count at global 0x005c75e4. If zero, initializes dictionary freelist, sets count to 1, and registers dictionary stream chunk handlers. If greater than zero, increments module count.
- 0x002c9890 (module close):
  0x5c75e4a1 0x74c08500 0x43f8a143 0x888b0083 0x00835340 0xa716e851 ...
  Decrements module count at global 0x005c75e4. If zero, flushes freelist memory and unregisters chunk handlers.
- 0x002c9bf0 (RwTexDictionaryGetCurrent):
  0x5c75e8a1 0xccccc300 0xcccccccc 0xcccccccc
  Reads global pointer at 0x005c75e8 (active current dictionary pointer).

### PS2 SLUS_214.10 cross-check
- 0x001a4a80 (constructor):
  0x8f828390 0x1040000a 0x00000000 0x8e820000 0x00031880 0x00431021 0x8c440000 0x0c069da4
  Disassembly checks module count, sets up plugin data, and invokes jal 0x001a7690 (module open).
- 0x001a4ac0 (destructor):
  0x0c069d74 0x0240282d 0x8fa300cc 0x26730001 0x0263102a 0x1440ff8d 0x26100004 0x1000000c
  Disassembly invokes jal 0x001a75d0 (module close).
- 0x001a7690 (module open):
  0x27bdffa0 0xffbf0050 0x7fb40040 0x7fb30030 0x7fb20020 0x7fb10010 0x7fb00000 0x0080882d
- 0x001a75d0 (module close):
  0x27bdffb0 0xffbf0040 0x7fb30030 0x7fb20020 0x0080982d 0x7fb10010 0x7fb00000 0x8ca20004

## Plugin registration site and object sizes

The core plugin registration functions are:
- Xbox default.xbe: 0x002c0020 through 0x002c024f (calling RwEngineRegisterPlugin at 0x002c2990).
- PS2 SLUS_214.10: 0x001a66a0 through 0x001a688c (calling RwEngineRegisterPlugin at 0x001a6a20).

Registration sequence and exact object sizes derived directly from binary calls:
1. Plugin 0408 (RwTexture):
   - Xbox: size 0x34 (52 bytes) at 0x002c0152 (construct 0x002b8c70, destroy 0x002b8ba0).
   - PS2: size 0x34 (52 bytes) at 0x001a67c8 (construct 0x001a9e80, destroy 0x001a9cc0).
2. Plugin 0409 (RwTexDictionary):
   - Xbox: size 0x60 (96 bytes) at 0x002c0173 (construct 0x002c9840, destroy 0x002c9860).
   - PS2: size 0x60 (96 bytes) at 0x001a67e8 (construct 0x001a4a80, destroy 0x001a4ac0).
3. Plugin 0412 (RwWorld):
   - Xbox: size 0x04 (4 bytes) at 0x002c0194 (construct 0x002cd7d0, destroy 0x002cd820).
   - PS2: size 0x04 (4 bytes) at 0x001a6808 (construct 0x001a3df0, destroy 0x001a3e70).
4. Plugin 040a (RwMaterial):
   - Xbox: size 0x74 (**116 bytes**) at 0x002c020a (push 0x74).
   - PS2: size 0x7c (**124 bytes**) at 0x001a6838 (li $a0, 0x7c; 8 additional bytes for PS2 GS extension structure).
5. Plugin 040b (RwAtomic / RwClump):
   - Xbox: size 0x28 (40 bytes) at 0x002c021a (push 0x28).
   - PS2: size 0x28 (40 bytes) at 0x001a6858 (li $a0, 0x28).

## Scope of implementation

The native 0409 implementation is strictly scoped to behavior authenticated from the binary source:
- Freelist-backed 96-byte container allocation and destruction.
- Module open/close reference counting and rollback on failed initialization.
- Circular doubly-linked list tracking for dictionary containers.
- Global tracking and queries for the active current dictionary pointer (RwTexDictionaryGetCurrent).
- Thread affinity and isolation against cross-thread mutation.

Explicitly omitted from 0409 scope:
- Downstream texture container methods (`add_texture`, `remove_texture`, `find_texture`) and mutual exclusion are not part of the 0409 core registration contract and remain unrecovered.
- No locale-dependent string comparison (`std::tolower`) is used.
