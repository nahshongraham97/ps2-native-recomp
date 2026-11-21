#include "mka/retail_memory_owners.hpp"
#include <cstdio>
#include <new>
#include <utility>

namespace mka::retail {
namespace detail {
class RetailWaveOverlaySource final : public memory::WaveAlternateOwnerSource {
public:
    std::optional<std::shared_ptr<memory::NativeHeapOwner>> current(
        memory::WaveAllocationFailure& f) override {
        f={}; std::lock_guard lock(mutex_); return parent_;
    }
    void publish(std::shared_ptr<memory::NativeHeapOwner> parent) {
        std::shared_ptr<memory::NativeHeapOwner> old;
        { std::lock_guard lock(mutex_); old=std::move(parent_); parent_=std::move(parent); }
    }
private:
    std::mutex mutex_;
    std::shared_ptr<memory::NativeHeapOwner> parent_; // Original64EC50 BSS zero.
};
}
namespace {
class NativeWaveLog final : public memory::WaveAllocationDiagnostics {
public:
    bool report(const memory::WaveAllocationDiagnostic& d) override {
        return std::fprintf(stderr,"[native-memory] Wave %s request=%llu bytes=%zu exhausted=%u\n",
            d.kind==memory::WaveDiagnosticKind::overflow_attempt?"overflow allocation":"allocation failed",
            static_cast<unsigned long long>(d.request),d.requested_bytes,d.exhausted_attempts)>=0 &&
            std::fflush(stderr)==0;
    }
};
constexpr const char* names[] = {"System", "Permanent", "Section", "mwFile", "Wave", "MPEG", "SECTION_TABLE", "OVERFLOW"};
constexpr RetailMemoryPolicy policies[] = {
    RetailMemoryPolicy::logical_root, RetailMemoryPolicy::ordinary, RetailMemoryPolicy::ordinary,
    RetailMemoryPolicy::ordinary, RetailMemoryPolicy::wave_callback, RetailMemoryPolicy::mpeg_unbound,
    RetailMemoryPolicy::fixed_table_unbound, RetailMemoryPolicy::ordinary
};
bool fail(RetailMemoryOwnerFailure& f, RetailMemoryOwnerError e) noexcept { f.kind = e; return false; }
std::optional<std::size_t> index(RetailMemoryRole role, RetailMemoryOwnerFailure& f) noexcept {
    switch (role) {
    case RetailMemoryRole::system: return 0;
    case RetailMemoryRole::permanent: return 1;
    case RetailMemoryRole::section: return 2;
    case RetailMemoryRole::mw_file: return 3;
    case RetailMemoryRole::wave: return 4;
    case RetailMemoryRole::mpeg: return 5;
    case RetailMemoryRole::section_table: return 6;
    case RetailMemoryRole::overflow: return 7;
    case RetailMemoryRole::disabled_first:
    case RetailMemoryRole::disabled_second: fail(f, RetailMemoryOwnerError::absent_role); return {};
    }
    fail(f, RetailMemoryOwnerError::invalid_role); return {};
}
} // namespace
std::unique_ptr<NativeRetailMemoryOwners> NativeRetailMemoryOwners::create(RetailMemoryOwnerFailure& f) noexcept {
    f = {};
    try {
        auto result = std::unique_ptr<NativeRetailMemoryOwners>(new NativeRetailMemoryOwners);
        for (std::size_t i = 0; i < result->owners_.size(); ++i) {
            auto parent = i == 0 ? nullptr : result->owners_[i == 6 ? 1 : 0];
            result->owners_[i] = memory::NativeHeapOwner::create(names[i], std::move(parent), f.heap);
            if (!result->owners_[i]) { fail(f, RetailMemoryOwnerError::heap); return {}; }
            // Typed routing publications are distinct from root hierarchy.
            if (i == 4) result->default_ = result->owners_[1];
            if (i == 7) result->overflow_ = result->owners_[7];
        }
        return result;
    } catch (const std::bad_alloc&) { fail(f, RetailMemoryOwnerError::allocation_failed); }
      catch (...) { fail(f, RetailMemoryOwnerError::native_exception); }
    return {};
}
std::optional<RetailMemoryOwnerObservation> NativeRetailMemoryOwners::observe(
    RetailMemoryRole role, RetailMemoryOwnerFailure& f) const noexcept {
    f = {};
    auto i = index(role, f);
    if (!i) return {};
    try {
        // The role array is immutable after all-or-nothing creation.
        auto heap = owners_[*i]->observe();
        return RetailMemoryOwnerObservation{role, names[*i], policies[*i], heap, *i == 0 ? std::optional<bool>{} : std::optional<bool>{true}};
    } catch (...) { fail(f, RetailMemoryOwnerError::native_exception); return {}; }
}
std::optional<RetailMemoryOwnerObservation> NativeRetailMemoryOwners::default_owner(RetailMemoryOwnerFailure& f) const noexcept {
    f = {};
    try {
        return RetailMemoryOwnerObservation{RetailMemoryRole::permanent, names[1], policies[1], default_->observe(), true};
    } catch (...) { fail(f, RetailMemoryOwnerError::native_exception); return {}; }
}
std::optional<RetailMemoryOwnerObservation> NativeRetailMemoryOwners::overflow_owner(RetailMemoryOwnerFailure& f) const noexcept {
    f = {};
    try {
        return RetailMemoryOwnerObservation{RetailMemoryRole::overflow, names[7], policies[7], overflow_->observe(), true};
    } catch (...) { fail(f, RetailMemoryOwnerError::native_exception); return {}; }
}
std::optional<memory::NativeAllocation> NativeRetailMemoryOwners::allocate_ordinary(
    RetailMemoryRole role, std::size_t bytes, RetailMemoryOwnerFailure& f, std::size_t alignment) noexcept {
    f = {};
    auto i = index(role, f);
    if (!i) return {};
    if (policies[*i] != RetailMemoryPolicy::ordinary) {
        fail(f, RetailMemoryOwnerError::unbound_allocator_policy); return {};
    }
    auto allocation = owners_[*i]->allocate(bytes, f.heap, alignment);
    if (!allocation) fail(f, RetailMemoryOwnerError::heap);
    return allocation;
}
bool NativeRetailMemoryOwners::bind_wave_initial(memory::WaveAlternateState state,
    std::shared_ptr<memory::NativeHeapOwner> alternate,
    std::shared_ptr<memory::WaveAllocationDiagnostics> diagnostics, RetailMemoryOwnerFailure& f) noexcept {
    f = {};
    bool admitted = false;
    try {
        {
            std::lock_guard lock(mutex_);
            if (wave_) return fail(f, RetailMemoryOwnerError::wave_already_bound);
            if (wave_binding_) return fail(f, RetailMemoryOwnerError::wave_binding_active);
            wave_binding_ = admitted = true;
        }
        // A selected real alternate must be live through initial publication.
        // This is native admission, not proof of overlay readiness or later lifetime.
        std::optional<memory::NativeHeapRequest> alternate_admission;
        if (state == memory::WaveAlternateState::ordinary_owner && alternate) {
            alternate_admission = alternate->retain_request(f.heap);
            if (!alternate_admission) {
                f.wave.kind = memory::WaveAllocationError::heap;
                f.wave.stage = memory::WaveAllocationStage::alternate;
                f.wave.heap = f.heap;
                {
                    std::lock_guard lock(mutex_);
                    wave_binding_ = admitted = false;
                }
                return fail(f, RetailMemoryOwnerError::heap);
            }
        }
        memory::WaveAllocationBindings b;
        b.wave = owners_[4];
        b.alternate_state = state;
        b.alternate = std::move(alternate);
        b.overflow_state = memory::WaveOverflowState::ordinary_owner;
        b.overflow = overflow_;
        b.diagnostics = std::move(diagnostics);
        // Factory and shared-owner allocation occur outside the group mutex.
        auto candidate = memory::NativeWaveAllocator::create(std::move(b), f.wave);
        std::shared_ptr<memory::NativeWaveAllocator> shared = std::move(candidate);
        {
            std::lock_guard lock(mutex_);
            wave_binding_ = admitted = false;
            if (!shared) return fail(f, RetailMemoryOwnerError::wave_binding);
            wave_ = std::move(shared);
        }
        return true;
    } catch (const std::bad_alloc&) { fail(f, RetailMemoryOwnerError::allocation_failed); }
      catch (...) { fail(f, RetailMemoryOwnerError::native_exception); }
    if (admitted) {
        std::lock_guard lock(mutex_);
        wave_binding_ = false;
    }
    return false;
}
std::shared_ptr<memory::NativeWaveAllocator> NativeRetailMemoryOwners::wave_allocator(RetailMemoryOwnerFailure& f) const noexcept {
    f = {};
    try {
        std::lock_guard lock(mutex_);
        if (wave_binding_) { fail(f, RetailMemoryOwnerError::wave_binding_active); return {}; }
        if (!wave_) { fail(f, RetailMemoryOwnerError::wave_unbound); return {}; }
        return wave_;
    } catch (...) { fail(f, RetailMemoryOwnerError::native_exception); return {}; }
}
bool NativeRetailMemoryOwners::reset_wave_payloads(RetailMemoryOwnerFailure& f) noexcept {
    f = {};
    try {
        {
            std::lock_guard lock(mutex_);
            if (wave_binding_) return fail(f, RetailMemoryOwnerError::wave_binding_active);
        }
        if (!owners_[4]->reset_actual_subtree(f.heap))
            return fail(f, RetailMemoryOwnerError::heap);
        return true;
    } catch (...) { return fail(f, RetailMemoryOwnerError::native_exception); }
}
bool NativeRetailMemoryOwners::bind_wave_native_source(RetailMemoryOwnerFailure& f) noexcept {
    f={}; bool admitted=false;
    try {
        {
            std::lock_guard lock(mutex_);
            if(wave_) return fail(f,RetailMemoryOwnerError::wave_already_bound);
            if(wave_binding_) return fail(f,RetailMemoryOwnerError::wave_binding_active);
            wave_binding_=admitted=true;
        }
        auto source=std::make_shared<detail::RetailWaveOverlaySource>();
        memory::WaveAllocationBindings b;
        b.wave=owners_[4];
        b.alternate_state=memory::WaveAlternateState::native_source;
        b.alternate_source=source;
        b.overflow_state=memory::WaveOverflowState::ordinary_owner;
        b.overflow=overflow_;
        b.diagnostics=std::make_shared<NativeWaveLog>();
        std::shared_ptr<memory::NativeWaveAllocator> wave=memory::NativeWaveAllocator::create(std::move(b),f.wave);
        {
            std::lock_guard lock(mutex_);
            wave_binding_=admitted=false;
            if(!wave) return fail(f,RetailMemoryOwnerError::wave_binding);
            wave_=std::move(wave); wave_source_=std::move(source);
        }
        return true;
    } catch(const std::bad_alloc&) { fail(f,RetailMemoryOwnerError::allocation_failed); }
    catch(...) { fail(f,RetailMemoryOwnerError::native_exception); }
    if(admitted) { std::lock_guard lock(mutex_); wave_binding_=false; }
    return false;
}
bool NativeRetailMemoryOwners::publish_wave_overlay_parent(
    std::shared_ptr<memory::NativeHeapOwner> parent,RetailMemoryOwnerFailure& f) noexcept {
    f={};
    try {
        std::shared_ptr<detail::RetailWaveOverlaySource> source;
        {
            std::lock_guard lock(mutex_);
            if(wave_binding_) return fail(f,RetailMemoryOwnerError::wave_binding_active);
            source=wave_source_;
        }
        if(!source) return fail(f,RetailMemoryOwnerError::wave_unbound);
        std::optional<memory::NativeHeapRequest> admitted;
        if(parent) {
            admitted=parent->retain_request(f.heap);
            if(!admitted) return fail(f,RetailMemoryOwnerError::heap);
        }
        source->publish(std::move(parent));
        return true;
    } catch(...) { return fail(f,RetailMemoryOwnerError::native_exception); }
}
} // namespace mka::retail
