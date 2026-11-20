#include "mka/allocation_class.hpp"
#include <limits>
#include <new>
#include <utility>

namespace mka::memory {
namespace detail {
struct AllocationClassState {
    AllocationClassConfig config;
    std::size_t rounded_payload{};
};
} // namespace detail
namespace {
bool fail(AllocationClassFailure& f, AllocationClassError kind) noexcept { f.kind = kind; return false; }
bool aligned(std::size_t a) noexcept { return a != 0 && (a & (a - 1)) == 0; }
bool role_known(AllocationClassRole r) noexcept {
    switch (r) {
    case AllocationClassRole::pfx: case AllocationClassRole::mkobj: case AllocationClassRole::mkbone:
    case AllocationClassRole::mksobj: case AllocationClassRole::mkproc: case AllocationClassRole::size_64:
    case AllocationClassRole::size_128: case AllocationClassRole::size_256: case AllocationClassRole::size_512:
    case AllocationClassRole::size_2048: case AllocationClassRole::size_4096: case AllocationClassRole::tiny_stack:
    case AllocationClassRole::big_stack: return true;
    default: return false;
    }
}
std::optional<std::size_t> round16(std::size_t bytes, AllocationClassFailure& f) noexcept {
    if (bytes > std::numeric_limits<std::size_t>::max() - 15) {
        fail(f, AllocationClassError::size_overflow); return {};
    }
    return (bytes + 15) & ~std::size_t{15};
}
} // namespace
NativeAllocationClass::NativeAllocationClass(std::shared_ptr<const detail::AllocationClassState> s) noexcept
    : state_(std::move(s)) {}
std::unique_ptr<NativeAllocationClass> NativeAllocationClass::create(
    AllocationClassConfig config, AllocationClassFailure& f) noexcept {
    f = {};
    if (!config.owner) { fail(f, AllocationClassError::invalid_owner); return {}; }
    if (!role_known(config.role)) { fail(f, AllocationClassError::unbound_role); return {}; }
    if (config.schema != AllocationClassSchema::source_kind2_payload) {
        fail(f, AllocationClassError::unbound_schema); return {};
    }
    if (!config.payload_bytes) { fail(f, AllocationClassError::invalid_payload); return {}; }
    if (!aligned(config.alignment)) { fail(f, AllocationClassError::invalid_alignment); return {}; }
    auto rounded = round16(config.payload_bytes, f);
    if (!rounded) return {};
    try {
        auto admission = config.owner->retain_request(f.heap);
        if (!admission) { fail(f, AllocationClassError::heap); return {}; }
        auto state = std::make_shared<detail::AllocationClassState>();
        state->config = std::move(config);
        state->rounded_payload = *rounded;
        return std::unique_ptr<NativeAllocationClass>(new NativeAllocationClass(std::move(state)));
    } catch (const std::bad_alloc&) { fail(f, AllocationClassError::allocation_failed); }
      catch (...) { fail(f, AllocationClassError::native_exception); }
    return {};
}
std::optional<ClassAllocation> NativeAllocationClass::allocate(
    const std::shared_ptr<NativeHeapOwner>& requested, std::size_t bytes,
    std::size_t requested_alignment, AllocationClassFailure& f) noexcept {
    f = {};
    if (!requested) { fail(f, AllocationClassError::invalid_owner); return {}; }
    if (!aligned(requested_alignment)) { fail(f, AllocationClassError::invalid_alignment); return {}; }
    if (requested_alignment > state_->config.alignment) { fail(f, AllocationClassError::request_alignment); return {}; }
    auto normalized = round16(bytes ? bytes : std::size_t{16}, f);
    if (!normalized) return {};
    if (*normalized > state_->rounded_payload) { fail(f, AllocationClassError::class_fit); return {}; }
    // The actual class is retained and live throughout the entire payload transaction.
    auto admission = state_->config.owner->retain_request(f.heap);
    if (!admission) { fail(f, AllocationClassError::heap); return {}; }
    auto storage = requested->allocate_from(state_->config.owner, state_->rounded_payload, f.heap, state_->config.alignment < 16 ? 16 : state_->config.alignment);
    if (!storage) { fail(f, AllocationClassError::heap); return {}; }
    ClassAllocation result;
    result.state_ = state_;
    result.storage_ = std::move(*storage);
    result.requested_bytes_ = bytes;
    result.normalized_bytes_ = *normalized;
    result.requested_alignment_ = requested_alignment;
    return result;
}
std::optional<AllocationClassObservation> NativeAllocationClass::observe(AllocationClassFailure& f) const noexcept {
    f = {};
    try {
        return AllocationClassObservation{state_->config.role, state_->config.schema, state_->config.payload_bytes,
            state_->rounded_payload, state_->config.alignment, state_->config.owner->observe()};
    } catch (...) { fail(f, AllocationClassError::native_exception); return {}; }
}
std::optional<std::int64_t> NativeAllocationClass::pfx_headroom(
    std::uint64_t count, AllocationClassFailure& f) const noexcept {
    f = {};
    if (state_->config.role != AllocationClassRole::pfx) { fail(f, AllocationClassError::wrong_role); return {}; }
    auto snapshot = observe(f);
    if (!snapshot) return {};
    if (!snapshot->owner.active) {
        f.heap.kind = NativeHeapError::inactive_owner;
        fail(f, AllocationClassError::heap);
        return {};
    }
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
    return checked_pfx_headroom(count, static_cast<std::uint64_t>(snapshot->owner.actual_live_allocations), f);
}
ClassAllocation::ClassAllocation(ClassAllocation&& other) noexcept { *this = std::move(other); }
ClassAllocation& ClassAllocation::operator=(ClassAllocation&& other) noexcept {
    if (this != &other) {
        // Free old backing while its old class State is still retained.
        storage_ = std::move(other.storage_);
        state_ = std::move(other.state_);
        requested_bytes_ = std::exchange(other.requested_bytes_, 0);
        normalized_bytes_ = std::exchange(other.normalized_bytes_, 0);
        requested_alignment_ = std::exchange(other.requested_alignment_, 0);
    }
    return *this;
}
std::size_t ClassAllocation::payload_size() const noexcept { return state_ ? state_->rounded_payload : 0; }
bool ClassAllocation::release(NativeHeapFailure& f) noexcept {
    if (!storage_.release(f)) return false;
    state_.reset();
    requested_bytes_ = normalized_bytes_ = requested_alignment_ = 0;
    return true;
}
std::optional<std::int64_t> checked_pfx_headroom(std::uint64_t count, std::uint64_t actual,
    AllocationClassFailure& f) noexcept {
    f = {};
    constexpr auto positive = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    if (count >= actual) {
        const auto difference = count - actual;
        if (difference > positive) { fail(f, AllocationClassError::host_range); return {}; }
        return static_cast<std::int64_t>(difference);
    }
    const auto magnitude = actual - count;
    if (magnitude > positive + 1) { fail(f, AllocationClassError::host_range); return {}; }
    if (magnitude == positive + 1) return std::numeric_limits<std::int64_t>::min();
    return -static_cast<std::int64_t>(magnitude);
}
} // namespace mka::memory
