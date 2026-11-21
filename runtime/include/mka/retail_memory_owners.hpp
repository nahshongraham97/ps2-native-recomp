#pragma once

#include "mka/native_heap.hpp"
#include "mka/wave_allocation.hpp"
#include <array>
#include <memory>
#include <mutex>
#include <optional>

namespace mka::retail {
enum class RetailMemoryRole {
    system, permanent, section, mw_file, wave, mpeg, section_table, overflow,
    disabled_first, disabled_second
};
enum class RetailMemoryPolicy { logical_root, ordinary, wave_callback, mpeg_unbound, fixed_table_unbound };
enum class RetailMemoryOwnerError {
    none, invalid_role, absent_role, heap, allocation_failed, native_exception,
    wave_unbound, wave_binding_active, wave_already_bound, wave_binding,
    unbound_allocator_policy
};
struct RetailMemoryOwnerFailure {
    RetailMemoryOwnerError kind{RetailMemoryOwnerError::none};
    memory::NativeHeapFailure heap;
    memory::WaveAllocationFailure wave;
};
struct RetailMemoryOwnerObservation {
    RetailMemoryRole role;
    const char* name;
    RetailMemoryPolicy policy;
    memory::NativeHeapObservation heap;
    std::optional<bool> overflow_enabled;
};

// The real ordered logical-owner phase only. It does not complete retail main.
class NativeRetailMemoryOwners final {
    // The source class factory consumes this exact group\'s Wave parent.
    friend class NativeRetailProfileOwners;
public:
    static std::unique_ptr<NativeRetailMemoryOwners> create(RetailMemoryOwnerFailure&) noexcept;
    std::optional<RetailMemoryOwnerObservation> observe(
        RetailMemoryRole, RetailMemoryOwnerFailure&) const noexcept;
    std::optional<RetailMemoryOwnerObservation> default_owner(RetailMemoryOwnerFailure&) const noexcept;
    std::optional<RetailMemoryOwnerObservation> overflow_owner(RetailMemoryOwnerFailure&) const noexcept;
    // Native direct storage leaf only; not the outer retail overflow/diagnostic route.
    std::optional<memory::NativeAllocation> allocate_ordinary(
        RetailMemoryRole, std::size_t bytes, RetailMemoryOwnerFailure&,
        std::size_t alignment = 16) noexcept;
    bool bind_wave_initial(memory::WaveAlternateState,
        std::shared_ptr<memory::NativeHeapOwner> actual_alternate,
        std::shared_ptr<memory::WaveAllocationDiagnostics>, RetailMemoryOwnerFailure&) noexcept;
    std::shared_ptr<memory::NativeWaveAllocator> wave_allocator(RetailMemoryOwnerFailure&) const noexcept;
    NativeRetailMemoryOwners(const NativeRetailMemoryOwners&) = delete;
    NativeRetailMemoryOwners& operator=(const NativeRetailMemoryOwners&) = delete;
private:
    NativeRetailMemoryOwners() = default;
    std::array<std::shared_ptr<memory::NativeHeapOwner>, 8> owners_;
    std::shared_ptr<memory::NativeHeapOwner> default_, overflow_;
    mutable std::mutex mutex_;
    bool wave_binding_{};
    std::shared_ptr<memory::NativeWaveAllocator> wave_;
};
} // namespace mka::retail
