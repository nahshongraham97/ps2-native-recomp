#include "mcserv.hpp"

#include "kernel.hpp"
#include "sif.hpp"
#include "ps2/memory.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ps2::iop::mcserv {

namespace {

constexpr std::uint32_t kSifSid = 0x80000400u;
constexpr std::uint32_t kMcDeniedPermit = static_cast<std::uint32_t>(-5);
constexpr std::uint32_t kMcDescParamBytes = 48u;
constexpr std::uint32_t kMcRpcStatBytes = 12u;

std::uint32_t ee_physical(std::uint32_t address) { return address & 0x1FFFFFFFu; }

bool valid_ee_range(std::uint32_t address, std::uint32_t size) {
    const std::uint32_t physical = ee_physical(address);
    return physical < ps2::kRamSize && size <= ps2::kRamSize - physical;
}

[[noreturn]] void strict_stop(const char *reason, std::uint32_t method,
                              std::uint32_t args, std::uint32_t args_size,
                              std::uint32_t ret_size) {
    std::fprintf(stderr,
                 "[iop-native-stop] MCSERV SIF 0x%08X RPC method 0x%08X args 0x%08X size %u reply size %u: %s",
                 kSifSid, method, args, args_size, ret_size, reason);
    if (args_size && valid_ee_range(args, std::min(args_size, 48u))) {
        const std::uint8_t *data = ps2::fast<std::uint8_t>(ee_physical(args));
        if (data) {
            const std::uint32_t words = std::min<std::uint32_t>(args_size / 4u, 12u);
            std::fprintf(stderr, " request");
            for (std::uint32_t i = 0; i < words; ++i) {
                std::uint32_t word = 0;
                std::memcpy(&word, data + 4u * i, sizeof word);
                std::fprintf(stderr, " %08X", word);
            }
        }
    }
    std::fprintf(stderr, "\n");
    std::fflush(stderr);
    std::abort();
}

bool mcsrv_rpc(std::uint32_t method, std::uint32_t args,
               std::uint32_t args_size, std::uint32_t ret,
               std::uint32_t ret_size) {
    if (!valid_ee_range(args, args_size) || !valid_ee_range(ret, ret_size))
        strict_stop("RPC buffer outside EE RAM", method, args, args_size, ret_size);

    // PS2SDK libmc probes XMCMAN first and treats a completed -5 init result as
    // an unhandled protocol before trying legacy MCMAN. That behavior is a
    // reference expectation; the retail game's use of this exact libmc path has
    // not yet been established. This deliberately does not claim card support.
    if (method == 0xFEu) {
        if (args_size != kMcDescParamBytes || ret_size != kMcRpcStatBytes)
            strict_stop("unexpected XMCMAN init buffer sizes", method, args, args_size, ret_size);

        std::uint8_t *reply = ps2::fast<std::uint8_t>(ee_physical(ret));
        if (!reply) strict_stop("XMCMAN init reply is outside EE RAM", method, args, args_size, ret_size);
        const std::uint32_t stat[3] = {kMcDeniedPermit, 0u, 0u};
        std::memcpy(reply, stat, sizeof stat);
        std::fprintf(stderr,
                     "[iop-native] MCSERV returned -5 for XMCMAN init (0xFE), EE reply buffer 0x%08X; legacy fallback remains to be observed\n",
                     ee_physical(ret));
        return true;
    }

    strict_stop("legacy/card RPC has no host implementation yet", method,
                args, args_size, ret_size);
}

} // namespace

void reset() { sif::unregister_hle_override(kSifSid); }

void activate() {
    sif::register_hle_override(kSifSid, mcsrv_rpc);
    std::fprintf(stderr,
                 "[iop-native] MCSERV host SIF replacement active at 0x%08X; card operations fail closed until implemented\n",
                 kSifSid);
}

void restore_sif_override() {
    const auto &loaded = kernel::modules();
    const bool module_loaded = std::any_of(loaded.begin(), loaded.end(), [](const auto &entry) {
        return entry.second.name == "MCSERV";
    });
    if (module_loaded) {
        sif::register_hle_override(kSifSid, mcsrv_rpc);
    } else {
        sif::unregister_hle_override(kSifSid);
    }
}

} // namespace ps2::iop::mcserv
