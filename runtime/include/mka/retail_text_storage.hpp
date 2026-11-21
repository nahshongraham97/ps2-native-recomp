#pragma once

#include "mka/retail_game_pool.hpp"
#include "mka/wave_allocation.hpp"
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {
enum class RetailTextStorageError {
    none, wrong_thread, missing_owner, foreign_owner, stale_allocation,
    wave, heap, pool, allocation, native_exception
};
struct RetailTextStorageFailure {
    RetailTextStorageError kind{RetailTextStorageError::none};
    const char* operation{""};
    memory::WaveAllocationFailure wave;
    memory::NativeHeapFailure heap;
    RetailGamePoolFailure pool;
};
enum class RetailStorageReleaseRoute { immediate_phase, deferred, immediate_record_exhaustion };
struct RetailTextStorageObservation {
    std::size_t pending{}, immediate_releases{}, deferred_releases{}, record_exhaustion_releases{};
};

// Real native allocation/free destinations of16FD80, not stored guest callback
// addresses. 12D9C0 defers only in source phase0;16FDA0 supplies count4.
// Pending frees retain their actual host allocation and the SAME game pool.
class NativeRetailTextStorage final {
public:
    static std::shared_ptr<NativeRetailTextStorage> create(
        std::shared_ptr<memory::NativeWaveAllocator>,
        std::shared_ptr<NativeRetailGamePool>, RetailTextStorageFailure&) noexcept;
    bool same_bindings(const std::shared_ptr<memory::NativeWaveAllocator>&,
        const std::shared_ptr<NativeRetailGamePool>&, RetailTextStorageFailure&) const noexcept;
    std::optional<memory::NativeAllocation> allocate_text(
        std::size_t, RetailTextStorageFailure&) noexcept;
    std::optional<RetailStorageReleaseRoute> retire_source_storage(
        memory::NativeAllocation&, std::uint32_t source_phase,
        std::uint32_t source_count, RetailTextStorageFailure&) noexcept;
    std::optional<RetailStorageReleaseRoute> retire_text(
        memory::NativeAllocation& allocation, std::uint32_t phase,
        RetailTextStorageFailure& failure) noexcept {
        return retire_source_storage(allocation, phase, 4, failure);
    }
    bool advance_frame_from_12d930(RetailTextStorageFailure&) noexcept;
    std::optional<RetailTextStorageObservation> observe(RetailTextStorageFailure&) const noexcept;
private:
    NativeRetailTextStorage(std::shared_ptr<memory::NativeWaveAllocator>,
        std::shared_ptr<NativeRetailGamePool>) noexcept;
    bool access(RetailTextStorageFailure&) const noexcept;
    struct Entry {
        NativeRetailGamePoolLink pool_link;
        // Destruction also frees actual backing before recycling its record.
        memory::NativeAllocation allocation;
        std::uint32_t remaining{};
    };
    std::shared_ptr<memory::NativeWaveAllocator> wave_;
    std::shared_ptr<NativeRetailGamePool> pool_;
    std::list<Entry> pending_;
    RetailTextStorageObservation counters_;
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
