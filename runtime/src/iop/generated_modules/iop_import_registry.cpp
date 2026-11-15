#include "iop/aot_import_registry.hpp"

namespace ps2::iop::aot_imports {
bool iop_imports_LIBSD_B00020500_HBDCCE405D7757993_20500(Cpu &);
bool iop_imports_SDRDRV_B00026300_HD70539C3E82D9283_26300(Cpu &);
bool iop_imports_MSL_B00028100_H7308BB2C3E647540_28100(Cpu &);
bool iop_imports_CRI_ADXI_B00071300_H990112CDFA87D073_71300(Cpu &);
namespace {
const Module kModules[] = {
    {"LIBSD", 0x00020500u, 0x4BB0u, 0xBDCCE405D7757993ull, &iop_imports_LIBSD_B00020500_HBDCCE405D7757993_20500},
    {"SDRDRV", 0x00026300u, 0x1330u, 0xD70539C3E82D9283ull, &iop_imports_SDRDRV_B00026300_HD70539C3E82D9283_26300},
    {"MSL", 0x00028100u, 0x10010u, 0x7308BB2C3E647540ull, &iop_imports_MSL_B00028100_H7308BB2C3E647540_28100},
    {"CRI_ADXI", 0x00071300u, 0x9520u, 0x990112CDFA87D073ull, &iop_imports_CRI_ADXI_B00071300_H990112CDFA87D073_71300},
};
}
const Module *modules(std::size_t &count) { count = 4; return kModules; }
}
