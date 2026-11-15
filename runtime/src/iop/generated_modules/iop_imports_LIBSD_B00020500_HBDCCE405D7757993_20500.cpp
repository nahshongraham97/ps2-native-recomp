#include "iop/kernel.hpp"
#include <cstdio>

namespace ps2::iop::aot_imports {
bool iop_imports_LIBSD_B00020500_HBDCCE405D7757993_20500(Cpu &c) {
    switch (phys(c.pc)) {
    case 0x00024F94u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024F9Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024FA4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024FACu:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024FB4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024FBCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 18u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 18u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024FC4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 23u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 23u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024FE8u:
      if (!kernel::compiled_import_stub_matches(c.pc, "loadcore", 0x0103u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("loadcore", 0x0103u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00024FF0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "loadcore", 0x0103u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("loadcore", 0x0103u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00025014u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifman", 0x0101u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifman", 0x0101u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002501Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifman", 0x0101u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifman", 0x0101u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00025040u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00025064u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thevent", 0x0101u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thevent", 0x0101u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002506Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thevent", 0x0101u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thevent", 0x0101u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00025074u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thevent", 0x0101u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thevent", 0x0101u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002507Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thevent", 0x0101u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thevent", 0x0101u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00025084u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thevent", 0x0101u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thevent", 0x0101u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002508Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thevent", 0x0101u, 9u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thevent", 0x0101u, 9u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00025094u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thevent", 0x0101u, 10u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thevent", 0x0101u, 10u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    default: return false;
    }
}
}
