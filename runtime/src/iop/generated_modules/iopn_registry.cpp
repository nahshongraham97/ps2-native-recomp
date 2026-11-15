#include "native/iop_native.h"

namespace ps2x::iop::detail {
std::uint32_t iopn_LIBSD_B00020500_HBDCCE405D7757993_20500(IopCpuState &, IopMemory &, std::uint32_t);
std::uint32_t iopn_SDRDRV_B00026300_HD70539C3E82D9283_26300(IopCpuState &, IopMemory &, std::uint32_t);
std::uint32_t iopn_MSL_B00028100_H7308BB2C3E647540_28100(IopCpuState &, IopMemory &, std::uint32_t);
std::uint32_t iopn_CRI_ADXI_B00071300_H990112CDFA87D073_71300(IopCpuState &, IopMemory &, std::uint32_t);
namespace {
const IopNativeModule kModules[] = {
    {"LIBSD", 0x00020500u, 0x4BB0u, 0xBDCCE405D7757993ull, &iopn_LIBSD_B00020500_HBDCCE405D7757993_20500},
    {"SDRDRV", 0x00026300u, 0x1330u, 0xD70539C3E82D9283ull, &iopn_SDRDRV_B00026300_HD70539C3E82D9283_26300},
    {"MSL", 0x00028100u, 0x10010u, 0x7308BB2C3E647540ull, &iopn_MSL_B00028100_H7308BB2C3E647540_28100},
    {"CRI_ADXI", 0x00071300u, 0x9520u, 0x990112CDFA87D073ull, &iopn_CRI_ADXI_B00071300_H990112CDFA87D073_71300},
};
}
const IopNativeModule *iopNativeModules(std::size_t &count) {
    count = 4;
    return kModules;
}
}
