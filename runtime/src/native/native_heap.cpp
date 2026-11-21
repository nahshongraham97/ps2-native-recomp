#include "mka/native_heap.hpp"

#include <atomic>
#include <limits>
#include <list>
#include <mutex>
#include <new>
#include <utility>

namespace mka::memory {
namespace {
// Serializes registry publication/retirement, never a host allocation or callout.
// Heap observations keep their existing independent state mutexes.
std::recursive_mutex ownership_registry;
}
namespace detail {
using AllocationLinks = std::list<std::weak_ptr<NativeAllocationRecord>>;
struct NativeHeapState {
    std::mutex mutex;
    std::string name;
    NativeHeapIdentity identity;
    bool active{true};
    std::size_t live{}, inflight{}, children{};
    std::size_t requested_live{}, actual_live{}, requests{};
    bool overflow_attempted{};
    bool resetting{};
    std::shared_ptr<NativeHeapState> parent;
    std::list<std::weak_ptr<NativeHeapState>> child_states;
    AllocationLinks allocations;
    ~NativeHeapState() {
        if (parent) {
            std::lock_guard lock(parent->mutex);
            --parent->children;
        }
    }
};
struct NativeAllocationRecord {
    void* pointer{};
    std::size_t alignment{};
    std::shared_ptr<NativeHeapState> requested, actual;
    AllocationLinks::iterator requested_link, actual_link;
    bool tracked{}; // ownership_registry protects links/counter retirement.
    bool deleting{};
    std::atomic<bool> valid{false};
    ~NativeAllocationRecord() {
        // The move-only handle drops the last record outside every owner lock.
        if (pointer) ::operator delete(pointer, std::align_val_t{alignment});
    }
};
} // namespace detail
namespace {
using State = detail::NativeHeapState;
std::atomic<std::uint64_t> next_identity{1};
bool fail(NativeHeapFailure& f, NativeHeapError kind) noexcept {
    f.kind = kind;
    return false;
}
std::uint64_t identity() noexcept {
    auto current = next_identity.load(std::memory_order_relaxed);
    while (current != std::numeric_limits<std::uint64_t>::max()) {
        if (next_identity.compare_exchange_weak(current, current + 1,
                                               std::memory_order_relaxed)) return current;
    }
    return 0;
}
bool admit(const std::shared_ptr<State>& s, NativeHeapFailure& f) {
    std::lock_guard registry(ownership_registry);
    std::lock_guard lock(s->mutex);
    if (!s->active) return fail(f, NativeHeapError::inactive_owner);
    if (s->resetting) return fail(f, NativeHeapError::owner_busy);
    const auto max = std::numeric_limits<std::size_t>::max();
    if (s->live == max || s->inflight >= max - s->live)
        return fail(f, NativeHeapError::counter_overflow);
    ++s->inflight;
    return true;
}
void complete(const std::shared_ptr<State>& s, bool success,
              bool requested = false, bool actual = false) noexcept {
    std::lock_guard lock(s->mutex);
    --s->inflight;
    if (success) {
        // Each role is a subset of unique live ownership, whose sum with inflight
        // was checked at admission. Neither role increment can overflow.
        ++s->live;
        if (requested) ++s->requested_live;
        if (actual) ++s->actual_live;
    }
}
void released(const std::shared_ptr<State>& s, bool requested, bool actual) noexcept {
    std::lock_guard lock(s->mutex);
    --s->live;
    if (requested) --s->requested_live;
    if (actual) --s->actual_live;
}
// Caller owns ownership_registry. Retirement only revokes native ownership;
// it cannot delete storage still held by a move-only allocation handle.
void retire(detail::NativeAllocationRecord& record) noexcept {
    if (!record.tracked) return;
    record.valid.store(false, std::memory_order_release);
    record.requested->allocations.erase(record.requested_link);
    if (record.actual != record.requested)
        record.actual->allocations.erase(record.actual_link);
    released(record.requested, true, record.actual == record.requested);
    if (record.actual != record.requested) released(record.actual, false, true);
    record.tracked = false;
}
// The registry gate prevents child registration and fresh admission during the
// walk. No snapshots or list nodes are allocated under this gate.
bool freeze_subtree(const std::shared_ptr<State>& state, NativeHeapFailure& f) {
    {
        std::lock_guard lock(state->mutex);
        if (!state->active) return fail(f, NativeHeapError::inactive_owner);
        if (state->resetting || state->inflight || state->requests)
            return fail(f, NativeHeapError::owner_busy);
        state->resetting = true;
    }
    for (const auto& weak : state->child_states) {
        auto child = weak.lock();
        if (child && child->parent == state && !freeze_subtree(child, f)) return false;
    }
    for (const auto& weak : state->allocations) {
        auto record = weak.lock();
        if (!record || record->actual != state) continue;
        if (record->deleting) return fail(f, NativeHeapError::owner_busy);
        if (record->requested == state) continue;
        std::lock_guard lock(record->requested->mutex);
        if (record->requested->inflight || record->requested->requests)
            return fail(f, NativeHeapError::owner_busy);
    }
    return true;
}
void thaw_subtree(const std::shared_ptr<State>& state) noexcept {
    { std::lock_guard lock(state->mutex); state->resetting = false; }
    for (const auto& weak : state->child_states) {
        auto child = weak.lock();
        if (child && child->parent == state) thaw_subtree(child);
    }
}
void retire_subtree(const std::shared_ptr<State>& state) noexcept {
    for (const auto& weak : state->child_states) {
        auto child = weak.lock();
        if (child && child->parent == state) retire_subtree(child);
    }
    for (auto it = state->allocations.begin(); it != state->allocations.end();) {
        auto record = it->lock();
        ++it;
        if (record && record->actual == state) retire(*record);
    }
}
} // namespace

NativeHeapOwner::NativeHeapOwner(std::shared_ptr<State> state) noexcept
    : state_(std::move(state)) {}

std::shared_ptr<NativeHeapOwner> NativeHeapOwner::create(
    std::string name, std::shared_ptr<NativeHeapOwner> parent,
    NativeHeapFailure& f) noexcept {
    f = {};
    try {
        auto state = std::make_shared<State>();
        state->name = std::move(name);
        state->identity = {identity(), 1};
        if (!state->identity.value) {
            fail(f, NativeHeapError::identity_exhausted);
            return {};
        }
        // Allocate the public wrapper before registering a child; no failure follows registration.
        auto result = std::shared_ptr<NativeHeapOwner>(new NativeHeapOwner(state));
        if (parent) {
            // Stage the weak list node before entering any owner mutex.
            std::list<std::weak_ptr<State>> staged;
            staged.emplace_back(state);
            std::lock_guard registry(ownership_registry);
            std::lock_guard lock(parent->state_->mutex);
            if (!parent->state_->active) {
                fail(f, NativeHeapError::inactive_owner);
                return {};
            }
            if (parent->state_->resetting) {
                fail(f, NativeHeapError::owner_busy);
                return {};
            }
            if (parent->state_->children == std::numeric_limits<std::size_t>::max()) {
                fail(f, NativeHeapError::counter_overflow);
                return {};
            }
            ++parent->state_->children;
            state->parent = parent->state_;
            parent->state_->child_states.remove_if([](const auto& w) { return w.expired(); });
            parent->state_->child_states.splice(parent->state_->child_states.end(), staged);
        }
        return result;
    } catch (const std::bad_alloc&) { fail(f, NativeHeapError::allocation_failed); }
      catch (...) { fail(f, NativeHeapError::native_exception); }
    return {};
}

std::optional<NativeAllocation> NativeHeapOwner::allocate(
    std::size_t bytes, NativeHeapFailure& f, std::size_t alignment) noexcept {
    return allocate_on(state_, bytes, f, alignment);
}

std::optional<NativeAllocation> NativeHeapOwner::allocate_from(
    const std::shared_ptr<NativeHeapOwner>& actual, std::size_t bytes,
    NativeHeapFailure& f, std::size_t alignment) noexcept {
    f = {};
    if (!actual) {
        fail(f, NativeHeapError::invalid_owner);
        return {};
    }
    return allocate_on(actual->state_, bytes, f, alignment);
}

std::optional<NativeAllocation> NativeHeapOwner::allocate_on(
    const std::shared_ptr<State>& target, std::size_t bytes,
    NativeHeapFailure& f, std::size_t alignment) noexcept {
    f = {};
    if (alignment < 16 || (alignment & (alignment - 1)) != 0) {
        fail(f, NativeHeapError::invalid_alignment);
        return {};
    }
    const auto effective = bytes == 0 ? std::size_t{16} : bytes;
    if (effective > std::numeric_limits<std::size_t>::max() - (alignment - 1)) {
        fail(f, NativeHeapError::size_overflow);
        return {};
    }
    const auto rounded = (effective + alignment - 1) & ~(alignment - 1);
    bool requested_admitted = false, actual_admitted = false;
    void* pointer = nullptr;
    try {
        auto record = std::make_shared<detail::NativeAllocationRecord>();
        record->requested = state_;
        record->actual = target;
        record->alignment = alignment;
        detail::AllocationLinks requested_link, actual_link;
        requested_link.emplace_back(record);
        if (target != state_) actual_link.emplace_back(record);
        if (!admit(state_, f)) return {};
        requested_admitted = true;
        if (target != state_) {
            if (!admit(target, f)) {
                complete(state_, false);
                return {};
            }
            actual_admitted = true;
        }
        // Host work is outside every owner mutex. Reservations prevent shutdown.
        pointer = ::operator new(rounded, std::align_val_t{alignment});
        record->pointer = pointer;
        pointer = nullptr; // Record now owns the physical host deleter.
        NativeAllocation result;
        result.record_ = record;
        result.requested_size_ = bytes;
        result.rounded_size_ = rounded;
        result.alignment_ = alignment;
        // Shutdown cannot change the generation while admitted.
        result.requested_identity_ = state_->identity;
        result.actual_identity_ = target->identity;
        {
            std::lock_guard registry(ownership_registry);
            record->requested_link = requested_link.begin();
            state_->allocations.splice(state_->allocations.end(), requested_link);
            if (target != state_) {
                record->actual_link = actual_link.begin();
                target->allocations.splice(target->allocations.end(), actual_link);
            }
            complete(state_, true, true, target == state_);
            if (actual_admitted) complete(target, true, false, true);
            record->tracked = true;
            record->valid.store(true, std::memory_order_release);
        }
        return result;
    } catch (const std::bad_alloc&) { fail(f, NativeHeapError::allocation_failed); }
      catch (...) { fail(f, NativeHeapError::native_exception); }
    if (pointer) ::operator delete(pointer, std::align_val_t{alignment});
    if (actual_admitted) complete(target, false);
    if (requested_admitted) complete(state_, false);
    return {};
}

std::optional<NativeHeapRequest> NativeHeapOwner::retain_request(NativeHeapFailure& f) noexcept {
    f = {};
    try {
        std::lock_guard registry(ownership_registry);
        std::lock_guard lock(state_->mutex);
        if (!state_->active) {
            fail(f, NativeHeapError::inactive_owner);
            return {};
        }
        if (state_->resetting) {
            fail(f, NativeHeapError::owner_busy);
            return {};
        }
        if (state_->requests == std::numeric_limits<std::size_t>::max()) {
            fail(f, NativeHeapError::counter_overflow);
            return {};
        }
        NativeHeapRequest result;
        result.state_ = state_;
        ++state_->requests;
        return result;
    } catch (...) { fail(f, NativeHeapError::native_exception); }
    return {};
}

NativeHeapRequest::~NativeHeapRequest() { release(); }
NativeHeapRequest::NativeHeapRequest(NativeHeapRequest&& other) noexcept
    : state_(std::move(other.state_)) {}
NativeHeapRequest& NativeHeapRequest::operator=(NativeHeapRequest&& other) noexcept {
    if (this != &other) {
        release();
        state_ = std::move(other.state_);
    }
    return *this;
}
bool NativeHeapRequest::release() noexcept {
    if (!state_) return false;
    // Drop the retained state/parent only after releasing its mutex.
    auto retained = std::move(state_);
    {
        std::lock_guard lock(retained->mutex);
        --retained->requests;
    }
    return true;
}

NativeHeapObservation NativeHeapOwner::observe() const {
    std::lock_guard lock(state_->mutex);
    // Parent identity is immutable while its registered child exists.
    return {state_->identity, state_->parent ? std::optional{state_->parent->identity}
                                            : std::nullopt,
            state_->active, state_->live, state_->inflight, state_->children,
            state_->requested_live, state_->actual_live, state_->requests, state_->overflow_attempted};
}
void NativeHeapOwner::mark_overflow_attempted() noexcept {
    std::lock_guard lock(state_->mutex);
    state_->overflow_attempted = true;
}
const std::string& NativeHeapOwner::name() const noexcept { return state_->name; }
bool NativeHeapOwner::reset_actual_subtree(NativeHeapFailure& f) noexcept {
    f = {};
    try {
        std::lock_guard registry(ownership_registry);
        if (!freeze_subtree(state_, f)) {
            thaw_subtree(state_);
            return false;
        }
        retire_subtree(state_);
        thaw_subtree(state_);
        return true;
    } catch (...) {
        // All effected ownership is explicit; never report a completed reset.
        std::lock_guard registry(ownership_registry);
        thaw_subtree(state_);
        return fail(f, NativeHeapError::native_exception);
    }
}
bool NativeHeapOwner::shutdown(NativeHeapFailure& f) noexcept {
    f = {};
    std::shared_ptr<State> parent;
    try {
        std::lock_guard registry(ownership_registry);
        {
            std::lock_guard lock(state_->mutex);
            if (!state_->active) return fail(f, NativeHeapError::inactive_owner);
            if (state_->resetting || state_->live || state_->inflight || state_->children || state_->requests)
                return fail(f, NativeHeapError::owner_busy);
            state_->active = false;
            ++state_->identity.generation;
            parent = std::move(state_->parent);
        }
        if (parent) {
            std::lock_guard lock(parent->mutex);
            --parent->children;
        }
        return true;
    } catch (...) { return fail(f, NativeHeapError::native_exception); }
}

NativeAllocation::~NativeAllocation() { reset(); }
NativeAllocation::NativeAllocation(NativeAllocation&& other) noexcept { *this = std::move(other); }
NativeAllocation& NativeAllocation::operator=(NativeAllocation&& other) noexcept {
    if (this != &other) {
        reset();
        record_ = std::move(other.record_);
        requested_size_ = std::exchange(other.requested_size_, 0);
        rounded_size_ = std::exchange(other.rounded_size_, 0);
        alignment_ = std::exchange(other.alignment_, 0);
        requested_identity_ = std::exchange(other.requested_identity_, {});
        actual_identity_ = std::exchange(other.actual_identity_, {});
    }
    return *this;
}
void NativeAllocation::reset() noexcept {
    auto retained = std::move(record_);
    if (!retained) return;
    {
        std::lock_guard registry(ownership_registry);
        retained->deleting = true;
        retained->valid.store(false, std::memory_order_release);
    }
    // Normal release keeps both live counters until real host deletion finishes.
    // A prior bulk reset has already retired those logical counters; retire()
    // then does nothing. Both routes perform the physical delete exactly once.
    auto* pointer = std::exchange(retained->pointer, nullptr);
    if (pointer) ::operator delete(pointer, std::align_val_t{retained->alignment});
    { std::lock_guard registry(ownership_registry); retire(*retained); }
    requested_size_ = rounded_size_ = alignment_ = 0;
    requested_identity_ = actual_identity_ = {};
}
void* NativeAllocation::data() const noexcept {
    return record_ && record_->valid.load(std::memory_order_acquire) ? record_->pointer : nullptr;
}
bool NativeAllocation::release(NativeHeapFailure& f) noexcept {
    f = {};
    if (!data()) return fail(f, NativeHeapError::already_released);
    reset();
    return true;
}
bool NativeAllocation::release_from(const NativeHeapOwner& owner, NativeHeapFailure& f) noexcept {
    f = {};
    if (!data()) return fail(f, NativeHeapError::already_released);
    if (record_->actual != owner.state_) return fail(f, NativeHeapError::foreign_owner);
    reset();
    return true;
}
} // namespace mka::memory
