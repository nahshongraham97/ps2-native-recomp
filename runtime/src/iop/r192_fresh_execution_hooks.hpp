#pragma once
#include <cstdint>
namespace ps2::iop::r192_aot { struct Import; }
// Only source-pinned disposable v8 kernel/system integrations call these.
// Restored code is NOT converted to fresh-load/SDK registration history.
namespace ps2::iop::kernel {
void r192_fresh_before_machine_restore();
void r192_fresh_after_machine_restore(bool complete_machine_success);
namespace fresh_bridge {
void reset_before_boot();
void allocation_erased(std::uint32_t base,std::uint32_t size,bool erased);
void thread_created(int id,std::uint32_t entry);
void irq_registered(unsigned line,std::uint32_t handler);
void irq_released(unsigned line);
void before_library_link();
void after_library_link();
void note_open_release_service();
void instruction_boundary();
void dispatch_refused() noexcept;
bool validate_dispatch_import(std::uint32_t address,const r192_aot::Import &entry);
struct CallScope {
    explicit CallScope(std::uint32_t address);
    ~CallScope() noexcept;
    bool active{};int exceptions{};
};
struct IrqScope {
    IrqScope(unsigned line,std::uint32_t handler);
    ~IrqScope() noexcept;
    bool active{},prior_valid{};unsigned prior_line{};
};
struct MutationScope {
    MutationScope() noexcept;
    ~MutationScope() noexcept;
    int exceptions{};
};
}
}
