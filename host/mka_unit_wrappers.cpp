#include "mka_combat_hooks.hpp"

extern "C" {

// Real symbols in libps2game.a provided by the linker under --wrap
void __real__ZN3gen9unit_0018ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void __real__ZN3gen9unit_0020ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void __real__ZN3gen9unit_0025ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);

// Linker Wrapper for gen::unit_0018: Intercepts PAD Scanner (328u) and Input Events (309u)
void __wrap__ZN3gen9unit_0018ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth) {
    if (mka::combat::is_interception_enabled()) {
        if (entry == 328u) {
            // Scanner entry point (guest PC 0x001637D0) called by unit_0008
            static std::uint64_t s_scanner_calls = 0;
            if (++s_scanner_calls <= 5) {
                std::fprintf(stderr, "[mka-scanner] frame scan #%llu (entry=328u)\n",
                             static_cast<unsigned long long>(s_scanner_calls));
            }
            __real__ZN3gen9unit_0018ERN3ps23CpuEjj(c, entry, depth);
            return;
        }

        if (entry == 309u) {
            // Button event handler entry point (guest PC 0x00162E70)
            mka::combat::handle_pad_event(c, depth, __real__ZN3gen9unit_0018ERN3ps23CpuEjj);
            return;
        }
    }

    __real__ZN3gen9unit_0018ERN3ps23CpuEjj(c, entry, depth);
}

// Linker Wrapper for gen::unit_0020: Intercepts Video Stream Dispatcher (126u)
void __wrap__ZN3gen9unit_0020ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth) {
    if (entry == 126u && mka::combat::is_interception_enabled()) {
        // Video stream dispatcher (guest PC 0x00181F90)
        mka::combat::handle_video_stream(c, depth, __real__ZN3gen9unit_0020ERN3ps23CpuEjj);
        return;
    }

    __real__ZN3gen9unit_0020ERN3ps23CpuEjj(c, entry, depth);
}

// Linker Wrapper for gen::unit_0025: Intercepts Health Scalar Deduction (204u)
void __wrap__ZN3gen9unit_0025ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth) {
    if (entry == 204u && mka::combat::is_interception_enabled()) {
        // Health deduction scalar entry point (guest PC 0x00197BC0) called by unit_0041/unit_0042
        mka::combat::handle_subzero_health(c, depth, __real__ZN3gen9unit_0025ERN3ps23CpuEjj);
        return;
    }

    __real__ZN3gen9unit_0025ERN3ps23CpuEjj(c, entry, depth);
}

} // extern "C"
