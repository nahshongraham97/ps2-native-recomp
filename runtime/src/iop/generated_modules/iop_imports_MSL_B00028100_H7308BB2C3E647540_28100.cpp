#include "iop/kernel.hpp"
#include <cstdio>

namespace ps2::iop::aot_imports {
static void trace_msl_libsd_call(std::uint32_t ordinal, const char *name, std::uint32_t a0, std::uint32_t a1, std::uint32_t a2, std::uint32_t a3, Cpu &c, bool ok) {
    if (kernel::g_trace_modules) {
        if (!ok) {
            std::fprintf(stderr, "[msl-libsd] ordinal %u (%s) REJECTED a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X\n",
                         ordinal, name, a0, a1, a2, a3);
        } else {
            std::fprintf(stderr, "[msl-libsd] ordinal %u (%s) a0=0x%08X a1=0x%08X a2=0x%08X a3=0x%08X -> r2=0x%08X\n",
                         ordinal, name, a0, a1, a2, a3, c.r[2]);
        }
    }
}
bool iop_imports_MSL_B00028100_H7308BB2C3E647540_28100(Cpu &c) {
    switch (phys(c.pc)) {
    case 0x00028114u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysmem", 0x0102u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysmem", 0x0102u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002811Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysmem", 0x0102u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysmem", 0x0102u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028124u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysmem", 0x0102u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysmem", 0x0102u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002812Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysmem", 0x0102u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysmem", 0x0102u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028150u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028158u:
      if (!kernel::compiled_import_stub_matches(c.pc, "intrman", 0x0102u, 18u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("intrman", 0x0102u, 18u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002817Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028184u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 17u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002818Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 19u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028194u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifcmd", 0x0101u, 22u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifcmd", 0x0101u, 22u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000281B8u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifman", 0x0101u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifman", 0x0101u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000281C0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sifman", 0x0101u, 29u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sifman", 0x0101u, 29u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000281E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 12u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 12u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000281ECu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000281F4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 19u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000281FCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 20u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 20u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028204u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 21u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 21u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002820Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 23u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 23u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028214u:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 27u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 27u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002821Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "sysclib", 0x0104u, 30u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("sysclib", 0x0104u, 30u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028240u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028248u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028250u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028258u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028260u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 10u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 10u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028268u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 16u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 16u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028270u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 20u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 20u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028278u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 33u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 33u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028280u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 34u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 34u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028288u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 39u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 39u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028290u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 40u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 40u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028298u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thbase", 0x0102u, 42u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thbase", 0x0102u, 42u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000282BCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000282C4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000282CCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000282D4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000282DCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "thsemap", 0x0101u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thsemap", 0x0101u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028300u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thmsgbx", 0x0101u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thmsgbx", 0x0101u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028308u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thmsgbx", 0x0101u, 6u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thmsgbx", 0x0101u, 6u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028310u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thmsgbx", 0x0101u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thmsgbx", 0x0101u, 7u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028318u:
      if (!kernel::compiled_import_stub_matches(c.pc, "thmsgbx", 0x0101u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("thmsgbx", 0x0101u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002833Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "timrman", 0x0103u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("timrman", 0x0103u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028344u:
      if (!kernel::compiled_import_stub_matches(c.pc, "timrman", 0x0103u, 20u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("timrman", 0x0103u, 20u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002834Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "timrman", 0x0103u, 22u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("timrman", 0x0103u, 22u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028354u:
      if (!kernel::compiled_import_stub_matches(c.pc, "timrman", 0x0103u, 23u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("timrman", 0x0103u, 23u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002835Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "timrman", 0x0103u, 24u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("timrman", 0x0103u, 24u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028380u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 4u, c);
      trace_msl_libsd_call(4u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028388u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 5u, c);
      trace_msl_libsd_call(5u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028390u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 7u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 7u, c);
      trace_msl_libsd_call(7u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028398u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 8u, c);
      trace_msl_libsd_call(8u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283A0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 9u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 9u, c);
      trace_msl_libsd_call(9u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283A8u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 10u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 10u, c);
      trace_msl_libsd_call(10u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283B0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 17u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 17u, c);
      trace_msl_libsd_call(17u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283B8u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 19u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 19u, c);
      trace_msl_libsd_call(19u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283C0u:
      if (!kernel::compiled_import_stub_matches(c.pc, "libsd", 0x0105u, 26u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      const auto a0 = c.r[4], a1 = c.r[5], a2 = c.r[6], a3 = c.r[7];
      const bool ok = kernel::invoke_native_import("libsd", 0x0105u, 26u, c);
      trace_msl_libsd_call(26u, "libsd", a0, a1, a2, a3, c, ok);
      if (!ok) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283E4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 4u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 4u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283ECu:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 5u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 5u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283F4u:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 8u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 8u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x000283FCu:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 9u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 9u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028404u:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 10u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 10u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002840Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 13u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 13u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028414u:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 14u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 14u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x0002841Cu:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 15u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 15u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    case 0x00028424u:
      if (!kernel::compiled_import_stub_matches(c.pc, "mwfile", 0x0103u, 16u)) return false;
    { const auto pc = c.pc, npc = c.npc; c.pc = c.r[31]; c.npc = c.pc + 4; c.r[0] = 0;
      if (!kernel::invoke_native_import("mwfile", 0x0103u, 16u, c)) { c.pc = pc; c.npc = npc; return false; }
      c.r[0] = 0; return true; }
    default: return false;
    }
}
}
