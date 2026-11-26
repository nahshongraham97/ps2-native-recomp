#pragma once
// Proved thbase1.2 ordinal42 connection for disposable R192-v8 only.
// SDK commit d4b9943bd4a58dbe69800bf4ec9d81640df5bc5b xthbase.h/thbase.c;
// original disc THREADMAN export slot686C -> body1EE8..1F20, LH TCB+0E.
#include "iop/kernel.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <cstdio>

namespace ps2::iop::r192_thbase42 {
[[noreturn]] inline void refuse(const char *name) {
    std::fprintf(stderr, "[iop-thbase42] %s\n", name); std::fflush(stderr);
    throw std::runtime_error(name);
}
inline std::int32_t query(bool interrupt_context, std::optional<std::int32_t> current_priority) {
    if (interrupt_context) return -100; // Original branch delay LI V0,0xFF9C; SDK KE_ILLEGAL_CONTEXT.
    if (!current_priority) refuse("IOP_THBASE42_REFUSED:no_running_thread");
    if (*current_priority < std::numeric_limits<std::int16_t>::min() ||
        *current_priority > std::numeric_limits<std::int16_t>::max())
        refuse("IOP_THBASE42_REFUSED:priority_not_proved_signed16");
    return static_cast<std::int32_t>(static_cast<std::int16_t>(*current_priority));
}
inline void install() {
    static_assert(sizeof(Cpu) == 156, "R192-v8 service connection only; no cross-layout casts");
    register_hle("thbase", 42, [](Cpu &cpu) {
        const bool interrupt = kernel::in_interrupt();
        std::optional<std::int32_t> priority;
        if (!interrupt) if (const auto *thread = kernel::current()) priority = thread->priority;
        cpu.r[2] = static_cast<std::uint32_t>(query(interrupt, priority));
    });
}
} // namespace ps2::iop::r192_thbase42
