#include "iop/kernel.hpp"
#include <cstdio>

namespace ps2::iop::aot_imports {
bool iop_imports_CRI_ADXI_B00071300_H990112CDFA87D073_71300(Cpu &c) {
    switch (phys(c.pc)) {
    case 0x0007A5C4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A5CCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A5D4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A5DCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 11u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 11u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A5E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A5ECu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 18u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 18u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A5F4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 19u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A5FCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 20u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 20u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A604u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 26u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 26u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A60Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 28u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 28u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A630u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A638u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 18u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 18u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A65Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "loadcore", 0x0103u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("loadcore", 0x0103u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A680u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A688u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 16u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 16u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A690u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A698u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 19u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A6A0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 22u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 22u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A6A8u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 24u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 24u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A6B0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 25u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 25u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A6D4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifman", 0x0101u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifman", 0x0101u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A6DCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifman", 0x0101u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifman", 0x0101u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A6E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifman", 0x0101u, 29u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifman", 0x0101u, 29u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A708u:
      if (!kernel::compiled_import_stub_matches(c.pc, "stdio", 0x0103u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("stdio", 0x0103u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A72Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A734u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 11u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 11u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A73Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 12u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 12u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A744u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A74Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 19u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A754u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 29u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 29u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A75Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 30u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 30u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A764u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 36u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 36u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A788u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A790u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A798u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A7A0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 10u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 10u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A7A8u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 20u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 20u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A7CCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A7D4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A7DCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A7E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0007A808u:
      if (!kernel::compiled_import_stub_matches(c.pc, "vblank", 0x0101u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("vblank", 0x0101u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    default: return false;
    }
}
}
