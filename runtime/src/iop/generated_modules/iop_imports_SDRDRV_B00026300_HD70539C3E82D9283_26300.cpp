#include "iop/kernel.hpp"
#include <cstdio>

namespace ps2::iop::aot_imports {
bool iop_imports_SDRDRV_B00026300_HD70539C3E82D9283_26300(Cpu &c) {
    switch (phys(c.pc)) {
    case 0x000273D4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000273DCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000273E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000273ECu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000273F4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000273FCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 9u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 9u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027404u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 10u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 10u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002740Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 11u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 11u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027414u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 12u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 12u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002741Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 13u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 13u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027424u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002742Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 15u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 15u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027434u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 16u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 16u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002743Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027444u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 18u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 18u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002744Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 19u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027454u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 20u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 20u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002745Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 23u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 23u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027464u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 24u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 24u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002746Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 25u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 25u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027474u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 26u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 26u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002747Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 27u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 27u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027484u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 30u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 30u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002748Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 31u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 31u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027494u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 32u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 32u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002749Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 33u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("libsd", 0x0105u, 33u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000274C0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysmem", 0x0101u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysmem", 0x0101u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000274E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000274ECu:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 18u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 18u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027510u:
      if (!kernel::compiled_import_stub_matches(c.pc, "loadcore", 0x0103u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("loadcore", 0x0103u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027518u:
      if (!kernel::compiled_import_stub_matches(c.pc, "loadcore", 0x0103u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("loadcore", 0x0103u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002753Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027544u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 15u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 15u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002754Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 16u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 16u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027554u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002755Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 19u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027564u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 22u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 22u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002756Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 24u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 24u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027574u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 25u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 25u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027598u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0103u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0103u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275A0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0103u, 29u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0103u, 29u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275A8u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0103u, 36u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0103u, 36u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275CCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275D4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275DCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 10u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 10u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275ECu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275F4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 20u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 20u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000275FCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 22u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 22u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027604u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 24u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 24u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002760Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 26u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 26u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00027614u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 28u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 28u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    default: return false;
    }
}
}
