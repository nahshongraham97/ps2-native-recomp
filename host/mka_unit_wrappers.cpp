#include "mka_combat_hooks.hpp"

namespace {

struct PadTraceSnapshot {
    std::uint32_t mode = 0;
    std::uint32_t flags80 = 0;
    std::uint32_t flags84 = 0;
    std::uint32_t write_index = 0;
    std::uint32_t last_tick = 0;
};

bool mode_input_trace_enabled() {
    static const bool enabled = [] {
        const std::string value = ps2::os::env("MKA_TRACE_MODE_INPUT");
        return !value.empty() && value != "0" && value != "off";
    }();
    return enabled;
}

PadTraceSnapshot read_pad_trace_snapshot() {
    return {
        ps2::ld32(mka::combat::kModeWordAddr),
        ps2::ld32(mka::combat::kGatekeeperFlagsAddr),
        ps2::ld32(mka::combat::kGatekeeperFlagsAddr + 4u),
        ps2::ld32(mka::combat::kWriteIndexAddr),
        ps2::ld32(mka::combat::kLastTickAddr)
    };
}

} // namespace

extern "C" {

// Real symbols in libps2game.a provided by the linker under --wrap
void __real__ZN3gen9unit_0018ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void __real__ZN3gen9unit_0020ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void __real__ZN3gen9unit_0025ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);

// Linker Wrapper for gen::unit_0018: Intercepts PAD Scanner (328u) and Input Events (309u)
void __wrap__ZN3gen9unit_0018ERN3ps23CpuEjj(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth) {
    if (entry == 328u) {
        static std::uint64_t s_scanner_calls = 0;
        const std::uint64_t scan = ++s_scanner_calls;
        if (mka::combat::is_interception_enabled() && scan <= 5) {
            // Scanner entry point (guest PC 0x001637D0) called by unit_0008.
            std::fprintf(stderr, "[mka-scanner] frame scan #%llu (entry=328u)\n",
                         static_cast<unsigned long long>(scan));
        }

        const bool trace = mode_input_trace_enabled();
        PadTraceSnapshot before{};
        bool state_changed = false;
        if (trace) {
            before = read_pad_trace_snapshot();
            static bool have_previous = false;
            static PadTraceSnapshot previous{};
            state_changed = !have_previous || before.mode != previous.mode ||
                            before.flags80 != previous.flags80 || before.flags84 != previous.flags84;
            if (state_changed) {
                std::fprintf(stderr,
                             "[mka-mode-trace] scan=%llu state raw_mode=0x%08X flags80=0x%08X flags84=0x%08X ring_index=%u last_tick=0x%08X\n",
                             static_cast<unsigned long long>(scan), before.mode, before.flags80, before.flags84,
                             before.write_index, before.last_tick);
            }
            previous = before;
            have_previous = true;
        }

        // Keep the original guest scan on its existing path, exactly once.
        __real__ZN3gen9unit_0018ERN3ps23CpuEjj(c, entry, depth);

        if (trace) {
            const PadTraceSnapshot after = read_pad_trace_snapshot();
            if (after.mode != before.mode || after.flags80 != before.flags80 || after.flags84 != before.flags84) {
                std::fprintf(stderr,
                             "[mka-mode-trace] scan=%llu state-after raw_mode=0x%08X flags80=0x%08X flags84=0x%08X\n",
                             static_cast<unsigned long long>(scan), after.mode, after.flags80, after.flags84);
            }

            if (after.write_index != before.write_index || after.last_tick != before.last_tick) {
                if (after.write_index < 30u) {
                    const std::uint32_t at = mka::combat::kRingBufferAddr + after.write_index * 20u;
                    std::fprintf(stderr,
                                 "[mka-mode-trace] scan=%llu pad-ring index=%u last_tick=0x%08X record=[%08X,%08X,%08X,%08X,%08X]\n",
                                 static_cast<unsigned long long>(scan), after.write_index, after.last_tick,
                                 ps2::ld32(at), ps2::ld32(at + 4u), ps2::ld32(at + 8u),
                                 ps2::ld32(at + 12u), ps2::ld32(at + 16u));
                } else {
                    std::fprintf(stderr,
                                 "[mka-mode-trace] scan=%llu pad-ring raw_index=%u last_tick=0x%08X (record index out of range)\n",
                                 static_cast<unsigned long long>(scan), after.write_index, after.last_tick);
                }
            }
        }
        return;
    }

    if (mka::combat::is_interception_enabled()) {
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

// Native Phase 4 SPU2 Audio Processing Tap
void mka_process_audio(const std::int16_t *samples, std::size_t frames) {
    mka::combat::process_audio(samples, frames);
}

} // extern "C"
