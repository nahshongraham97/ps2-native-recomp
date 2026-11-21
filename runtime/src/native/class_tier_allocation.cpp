#include "mka/class_tier_allocation.hpp"
#include <limits>
#include <new>
#include <utility>

namespace mka::memory {
namespace {
constexpr std::array<std::size_t, 6> payloads{64, 128, 256, 512, 2048, 4096};
constexpr std::array<AllocationClassRole, 6> roles{AllocationClassRole::size_64, AllocationClassRole::size_128,
    AllocationClassRole::size_256, AllocationClassRole::size_512, AllocationClassRole::size_2048, AllocationClassRole::size_4096};
bool fail(TierAllocationFailure& f, TierAllocationError e) noexcept { f.kind = e; return false; }
TierTarget target(std::size_t i) noexcept { return static_cast<TierTarget>(i); }
} // namespace
struct NativeTierAllocator::Call {
    NativeTierAllocator& owner;
    bool active{}, callback{};
    std::uint64_t request{};
    bool begin(bool is_callback, TierAllocationFailure& f) {
        std::lock_guard lock(owner.mutex_);
        if (owner.next_request_ == std::numeric_limits<std::uint64_t>::max() ||
            owner.active_requests_ == std::numeric_limits<std::size_t>::max() ||
            (is_callback && owner.active_callbacks_ == std::numeric_limits<std::size_t>::max()))
            return fail(f, TierAllocationError::counter_overflow);
        request = owner.next_request_++;
        ++owner.active_requests_;
        if (is_callback) ++owner.active_callbacks_;
        active = true; callback = is_callback; return true;
    }
    void end_callback() noexcept {
        if (callback) { std::lock_guard lock(owner.mutex_); --owner.active_callbacks_; callback = false; }
    }
    ~Call() {
        end_callback();
        if (active) { std::lock_guard lock(owner.mutex_); --owner.active_requests_; }
    }
};
NativeTierAllocator::NativeTierAllocator(TierAllocationBindings b,
    std::array<std::shared_ptr<NativeHeapOwner>, 9> owners) noexcept
    : bindings_(std::move(b)), owners_(std::move(owners)) {}
std::unique_ptr<NativeTierAllocator> NativeTierAllocator::create(TierAllocationBindings b, TierAllocationFailure& f) noexcept {
    f = {};
    if (!b.wave_policy) { fail(f, TierAllocationError::unbound_wave); return {}; }
    if (!b.diagnostics) { fail(f, TierAllocationError::unbound_diagnostics); return {}; }
    // Friend access to the actual complete immutable Wave context, no separately supplied aliases.
    const auto& wave = b.wave_policy->bindings_;
    if (wave.alternate_state != WaveAlternateState::ordinary_owner || !wave.alternate) {
        fail(f, TierAllocationError::unsupported_overlay); return {};
    }
    if (wave.overflow_state != WaveOverflowState::ordinary_owner || !wave.overflow) {
        fail(f, TierAllocationError::unsupported_overflow); return {};
    }
    if (!wave.wave) { fail(f, TierAllocationError::invalid_owner); return {}; }
    try {
        std::array<std::shared_ptr<NativeHeapOwner>, 9> owners;
        const auto wave_identity = wave.wave->observe().identity;
        for (std::size_t i = 0; i < 6; ++i) {
            f.target = target(i);
            if (!b.classes[i].capability || !b.classes[i].owner) { fail(f, TierAllocationError::unbound_class); return {}; }
            auto c = b.classes[i].capability->observe(f.class_leaf);
            if (!c) { fail(f, TierAllocationError::class_leaf); return {}; }
            if (!c->owner.active) {
                f.heap.kind = NativeHeapError::inactive_owner;
                fail(f, TierAllocationError::heap); return {};
            }
            if (c->owner.parent != wave_identity || c->schema != AllocationClassSchema::source_kind2_payload || c->role != roles[i] ||
                c->configured_payload_bytes != payloads[i] || c->payload_bytes != payloads[i] ||
                c->alignment != 16 || c->owner.identity != b.classes[i].owner->observe().identity) {
                fail(f, TierAllocationError::invalid_class); return {};
            }
            owners[i] = b.classes[i].owner;
        }
        owners[6] = wave.wave; owners[7] = wave.alternate; owners[8] = wave.overflow;
        for (std::size_t i = 0; i < owners.size(); ++i)
            for (std::size_t j = 0; j < i; ++j)
                if (owners[i]->observe().identity == owners[j]->observe().identity) {
                    fail(f, TierAllocationError::invalid_owner); return {};
                }
        std::array<NativeHeapRequest, 9> admission;
        for (std::size_t i = 0; i < owners.size(); ++i) {
            f.target = target(i);
            auto request = owners[i]->retain_request(f.heap);
            if (!request) { fail(f, TierAllocationError::heap); return {}; }
            admission[i] = std::move(*request);
        }
        auto result = std::unique_ptr<NativeTierAllocator>(new NativeTierAllocator(std::move(b), std::move(owners)));
        f = {}; return result;
    } catch (const std::bad_alloc&) { fail(f, TierAllocationError::allocation_failed); }
      catch (...) { fail(f, TierAllocationError::native_exception); }
    return {};
}
TierAllocationObservation NativeTierAllocator::observe() const {
    std::lock_guard lock(mutex_); return {active_requests_, active_callbacks_};
}
bool NativeTierAllocator::report(TierDiagnosticKind kind, TierTarget from, TierTarget next,
    std::uint64_t request, std::size_t bytes, unsigned attempts, TierAllocationFailure& f) noexcept {
    try {
        const auto id = owners_[static_cast<std::size_t>(from)]->observe();
        if (!bindings_.diagnostics->report({kind, request, from, next, id.identity, bytes, attempts, id.overflow_attempted}))
            return fail(f, TierAllocationError::diagnostic_rejected);
        return true;
    } catch (...) { return fail(f, TierAllocationError::diagnostic_exception); }
}
std::optional<TierAllocation> NativeTierAllocator::allocate(std::size_t bytes, TierAllocationFailure& f) noexcept {
    f = {};
    try {
        std::size_t selected = 0;
        while (selected < payloads.size() && bytes > payloads[selected]) ++selected;
        if (selected == 6) {
            Call call{*this};
            f.target = TierTarget::wave;
            if (!call.begin(false, f)) return {};
            // Initial large requests take the COMPLETE outer Wave route, including its own diagnostics.
            auto result = bindings_.wave_policy->allocate(bytes, f.wave);
            if (!result) { fail(f, TierAllocationError::wave_policy); return {}; }
            return TierAllocation(std::move(*result), bytes);
        }
        std::array<NativeHeapRequest, 9> admission;
        for (std::size_t i = 0; i < owners_.size(); ++i) {
            const bool needed = i == selected || (selected < 5 && i == selected + 1) ||
                                i == 6 || (selected < 5 && i == 7) || i == 8;
            if (!needed) continue;
            f.target = target(i);
            auto request = owners_[i]->retain_request(f.heap);
            if (!request) { fail(f, TierAllocationError::heap); return {}; }
            admission[i] = std::move(*request);
        }
        // Scope counters retire before the older route owner admissions.
        Call call{*this};
        if (!call.begin(true, f)) return {};
        unsigned attempts = 0;
        bool fatal = false;
        auto attempt = [&](std::size_t actual) -> std::optional<TierAllocation> {
            f.target = target(actual); ++attempts;
            if (actual < 6) {
                auto result = bindings_.classes[actual].capability->allocate(owners_[selected], bytes, 16, f.class_leaf);
                if (result) return TierAllocation(std::move(*result), bytes);
                const auto e = f.class_leaf.kind;
                if (e != AllocationClassError::class_fit && e != AllocationClassError::request_alignment &&
                    !(e == AllocationClassError::heap && f.class_leaf.heap.kind == NativeHeapError::allocation_failed)) {
                    fatal = true; fail(f, TierAllocationError::class_leaf);
                }
            } else {
                auto result = owners_[selected]->allocate_from(owners_[actual], bytes, f.heap, 16);
                if (result) return TierAllocation(std::move(*result), bytes);
                if (f.heap.kind != NativeHeapError::allocation_failed) { fatal = true; fail(f, TierAllocationError::heap); }
            }
            return {};
        };
        auto result = attempt(selected);
        if (!result && !fatal && selected < 5) {
            if (!report(TierDiagnosticKind::inner_fallback, target(selected), target(selected + 1), call.request, bytes, attempts, f)) return {};
            result = attempt(selected + 1);
            if (!result && !fatal) result = attempt(6);
            if (!result && !fatal && selected != 0) {
                if (!report(TierDiagnosticKind::inner_fallback, target(selected), target(selected + 1), call.request, bytes, attempts, f)) return {};
                result = attempt(selected + 1);
            }
            if (!result && !fatal) result = attempt(7);
        } else if (!result && !fatal) {
            result = attempt(6);
            if (!result && !fatal) result = attempt(6);
        }
        if (result) { f = {}; return result; }
        if (fatal) return {};
        call.end_callback();
        if (!report(TierDiagnosticKind::overflow_attempt, target(selected), TierTarget::overflow, call.request, bytes, attempts, f)) return {};
        owners_[selected]->mark_overflow_attempted();
        result = attempt(8);
        if (result) { f = {}; return result; }
        if (fatal) return {};
        if (!report(TierDiagnosticKind::terminal_failure, target(selected), TierTarget::overflow, call.request, bytes, attempts, f)) return {};
        fail(f, TierAllocationError::heap); return {};
    } catch (...) { fail(f, TierAllocationError::native_exception); return {}; }
}
TierAllocation::TierAllocation(ClassAllocation&& a, std::size_t n) noexcept : storage_(std::move(a)), logical_bytes_(n) {}
TierAllocation::TierAllocation(NativeAllocation&& a, std::size_t n) noexcept : storage_(std::move(a)), logical_bytes_(n) {}
TierAllocation::TierAllocation(TierAllocation&& other) noexcept
    : storage_(std::move(other.storage_)), logical_bytes_(std::exchange(other.logical_bytes_, 0)) {}
TierAllocation& TierAllocation::operator=(TierAllocation&& other) noexcept {
    if (this != &other) {
        // Variant assignment retires old owned storage before taking the successor.
        storage_ = std::move(other.storage_);
        logical_bytes_ = std::exchange(other.logical_bytes_, 0);
    }
    return *this;
}
TierAllocation::operator bool() const noexcept { return std::visit([](const auto& a) { return static_cast<bool>(a); }, storage_); }
void* TierAllocation::data() const noexcept { return std::visit([](const auto& a) { return a.data(); }, storage_); }
NativeHeapIdentity TierAllocation::requested_owner() const noexcept { return std::visit([](const auto& a) { return a.requested_owner(); }, storage_); }
NativeHeapIdentity TierAllocation::actual_owner() const noexcept { return std::visit([](const auto& a) { return a.actual_owner(); }, storage_); }
bool TierAllocation::release(NativeHeapFailure& f) noexcept {
    const bool result = std::visit([&](auto& a) { return a.release(f); }, storage_);
    if (result) logical_bytes_ = 0;
    return result;
}
} // namespace mka::memory
