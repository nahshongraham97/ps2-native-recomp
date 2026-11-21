#include "mka/retail_game_pool.hpp"

#include <limits>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace mka::retail {
namespace {
bool fail(RetailGamePoolFailure& f, RetailGamePoolError kind,
          const char* operation) noexcept {
    f.kind = kind;
    f.operation = operation;
    return false;
}
}

namespace detail {
struct RetailGamePoolState {
    struct Record {
        std::optional<std::size_t> next_free;
        std::uint64_t generation{1};
        std::uint32_t cached_identity{1};
        RetailGamePoolLinkKind kind{RetailGamePoolLinkKind::task_12ffe0};
        bool active{};
    };
    mutable std::mutex mutex;
    const std::thread::id owner_thread{std::this_thread::get_id()};
    std::vector<Record> records;
    std::optional<std::size_t> free_head;
    std::size_t initial_count{};
    std::size_t available{};
    std::size_t active{};
};
} // namespace detail

NativeRetailGamePoolLink::NativeRetailGamePoolLink(
    std::shared_ptr<detail::RetailGamePoolState> state,
    std::size_t ordinal, std::uint64_t generation,
    std::uint32_t cached_identity, RetailGamePoolLinkKind kind) noexcept
    : state_(std::move(state)), ordinal_(ordinal), generation_(generation),
      cached_identity_(cached_identity), kind_(kind) {}

NativeRetailGamePoolLink::~NativeRetailGamePoolLink() {
    release_unchecked();
}
NativeRetailGamePoolLink::NativeRetailGamePoolLink(
    NativeRetailGamePoolLink&& other) noexcept {
    *this = std::move(other);
}
NativeRetailGamePoolLink& NativeRetailGamePoolLink::operator=(
    NativeRetailGamePoolLink&& other) noexcept {
    if (this != &other) {
        release_unchecked();
        state_ = std::move(other.state_);
        ordinal_ = std::exchange(other.ordinal_, 0);
        generation_ = std::exchange(other.generation_, 0);
        cached_identity_ = std::exchange(other.cached_identity_, 0);
        kind_ = other.kind_;
    }
    return *this;
}

void NativeRetailGamePoolLink::release_unchecked() noexcept {
    if (!state_) return;
    try {
        std::lock_guard lock(state_->mutex);
        if (ordinal_ < state_->records.size()) {
            auto& record = state_->records[ordinal_];
            if (record.active && record.generation == generation_) {
                record.active = false;
                record.cached_identity = 1;
                --state_->active;
                if (record.generation != std::numeric_limits<std::uint64_t>::max()) {
                    ++record.generation;
                    record.next_free = state_->free_head;
                    state_->free_head = ordinal_;
                    ++state_->available;
                }
            }
        }
    } catch (...) {
        // The lease still releases its retained host state on destruction.
    }
    state_.reset();
    ordinal_ = 0;
    generation_ = 0;
    cached_identity_ = 0;
}

bool NativeRetailGamePoolLink::release(RetailGamePoolFailure& f) noexcept {
    f = {};
    if (!state_) return fail(f, RetailGamePoolError::invalid_link,
                             "game pool link already released");
    if (std::this_thread::get_id() != state_->owner_thread)
        return fail(f, RetailGamePoolError::wrong_thread,
                    "game pool link release thread");
    try {
        auto retained = state_;
        {
            std::lock_guard lock(retained->mutex);
            if (ordinal_ >= retained->records.size() ||
                !retained->records[ordinal_].active ||
                retained->records[ordinal_].generation != generation_)
                return fail(f, RetailGamePoolError::stale_link,
                            "game pool link generation");
            auto& record = retained->records[ordinal_];
            record.active = false;
            record.cached_identity = 1;
            --retained->active;
            if (record.generation != std::numeric_limits<std::uint64_t>::max()) {
                ++record.generation;
                record.next_free = retained->free_head;
                retained->free_head = ordinal_;
                ++retained->available;
            }
        }
        state_.reset();
        ordinal_ = 0;
        generation_ = 0;
        cached_identity_ = 0;
        return true;
    } catch (...) {
        return fail(f, RetailGamePoolError::native_exception,
                    "game pool link release");
    }
}

std::unique_ptr<NativeRetailGamePool> NativeRetailGamePool::initialize(
    std::uint64_t source_profile_count, RetailGamePoolFailure& f) noexcept {
    f = {};
    if (source_profile_count > std::numeric_limits<std::size_t>::max()) {
        fail(f, RetailGamePoolError::host_capacity,
             "game pool configured count representability");
        return {};
    }
    try {
        auto result = std::unique_ptr<NativeRetailGamePool>(new NativeRetailGamePool);
        result->state_ = std::make_shared<detail::RetailGamePoolState>();
        if (!result->reset_from_12da70(source_profile_count, f)) return {};
        return result;
    } catch (const std::bad_alloc&) {
        fail(f, RetailGamePoolError::allocation, "game pool owner allocation");
    } catch (const std::length_error&) {
        fail(f, RetailGamePoolError::host_capacity, "game pool owner capacity");
    } catch (...) {
        fail(f, RetailGamePoolError::native_exception, "game pool owner creation");
    }
    return {};
}

bool NativeRetailGamePool::reset_from_12da70(
    std::uint64_t source_profile_count, RetailGamePoolFailure& f) noexcept {
    f = {};
    if (!state_) return fail(f, RetailGamePoolError::invalid_link,
                             "game pool owner absent");
    if (std::this_thread::get_id() != state_->owner_thread)
        return fail(f, RetailGamePoolError::wrong_thread,
                    "game pool reset thread");
    if (source_profile_count > std::numeric_limits<std::size_t>::max())
        return fail(f, RetailGamePoolError::host_capacity,
                    "game pool configured count representability");
    try {
        const auto count = static_cast<std::size_t>(source_profile_count);
        std::vector<detail::RetailGamePoolState::Record> staged;
        staged.resize(count);
        std::optional<std::size_t> head;
        // 130030 iterates ordinal 0..N-1 and prepends each free node.
        for (std::size_t i = 0; i < count; ++i) {
            staged[i].next_free = head;
            head = i;
        }
        std::lock_guard lock(state_->mutex);
        if (state_->active)
            return fail(f, RetailGamePoolError::active_links,
                        "12DA70 live pool link retirement unbound");
        state_->records.swap(staged);
        state_->free_head = head;
        state_->initial_count = count;
        state_->available = count;
        return true;
    } catch (const std::bad_alloc&) {
        return fail(f, RetailGamePoolError::allocation,
                    "game pool native initial reserve");
    } catch (const std::length_error&) {
        return fail(f, RetailGamePoolError::host_capacity,
                    "game pool native initial reserve");
    } catch (...) {
        return fail(f, RetailGamePoolError::native_exception,
                    "game pool native reset");
    }
}

std::optional<NativeRetailGamePoolLink> NativeRetailGamePool::acquire(
    std::uint32_t object_identity, RetailGamePoolLinkKind kind,
    RetailGamePoolFailure& f) noexcept {
    f = {};
    if (!state_) {
        fail(f, RetailGamePoolError::invalid_link, "game pool owner absent");
        return {};
    }
    if (std::this_thread::get_id() != state_->owner_thread) {
        fail(f, RetailGamePoolError::wrong_thread, "game pool acquire thread");
        return {};
    }
    try {
        std::lock_guard lock(state_->mutex);
        if (!state_->free_head) {
            // The source count is an initial reserve, never a native ceiling.
            state_->records.emplace_back();
            const auto ordinal = state_->records.size() - 1;
            state_->records[ordinal].next_free = state_->free_head;
            state_->free_head = ordinal;
            ++state_->available;
        }
        const auto ordinal = *state_->free_head;
        auto& record = state_->records[ordinal];
        state_->free_head = record.next_free;
        record.next_free.reset();
        record.cached_identity = object_identity;
        record.kind = kind;
        record.active = true;
        --state_->available;
        ++state_->active;
        return NativeRetailGamePoolLink(state_, ordinal, record.generation,
                                        object_identity, kind);
    } catch (const std::bad_alloc&) {
        fail(f, RetailGamePoolError::allocation, "game pool link allocation");
    } catch (const std::length_error&) {
        fail(f, RetailGamePoolError::host_capacity, "game pool link growth");
    } catch (...) {
        fail(f, RetailGamePoolError::native_exception, "game pool link acquire");
    }
    return {};
}

std::optional<NativeRetailGamePoolLink>
NativeRetailGamePool::acquire_task_link(
    std::uint32_t identity, RetailGamePoolFailure& f) noexcept {
    return acquire(identity, RetailGamePoolLinkKind::task_12ffe0, f);
}
std::optional<NativeRetailGamePoolLink>
NativeRetailGamePool::acquire_observer_link(
    std::uint32_t identity, RetailGamePoolFailure& f) noexcept {
    return acquire(identity, RetailGamePoolLinkKind::observer_12ff70, f);
}

std::optional<RetailGamePoolObservation> NativeRetailGamePool::observe(
    RetailGamePoolFailure& f) const noexcept {
    f = {};
    if (!state_) {
        fail(f, RetailGamePoolError::invalid_link, "game pool owner absent");
        return {};
    }
    if (std::this_thread::get_id() != state_->owner_thread) {
        fail(f, RetailGamePoolError::wrong_thread, "game pool observe thread");
        return {};
    }
    try {
        std::lock_guard lock(state_->mutex);
        return RetailGamePoolObservation{state_->initial_count,
            state_->records.size(), state_->available, state_->active,
            state_->free_head};
    } catch (...) {
        fail(f, RetailGamePoolError::native_exception, "game pool observation");
        return {};
    }
}

} // namespace mka::retail
