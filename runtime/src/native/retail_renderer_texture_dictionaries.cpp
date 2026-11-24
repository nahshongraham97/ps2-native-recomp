#include "mka/retail_renderer_texture_dictionaries.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <cstring>
#include <new>

namespace mka::retail {

namespace {

template<class T, class Create>
std::shared_ptr<T> heap_object(
    const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,
    RetailRendererTextureDictionaryFailure& f,
    Create create) noexcept {
    static_assert(alignof(T) <= 16);
    auto allocation = heap->allocate(sizeof(T), f.allocator);
    if (!allocation) {
        f.kind = RetailRendererTextureDictionaryError::allocator;
        f.operation = "0409 actual typed Permanent allocation";
        return {};
    }
    try {
        auto* object = create(allocation->data());
        return std::shared_ptr<T>(object, [heap, alloc = std::move(*allocation)](T* p) mutable {
            p->~T();
            alloc = memory::NativeAllocation{};
        });
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererTextureDictionaryError::allocation;
        f.operation = "0409 actual native ownership";
    } catch (...) {
        f.kind = RetailRendererTextureDictionaryError::native_exception;
        f.operation = "0409 native construction";
    }
    return {};
}

} // namespace

struct NativeRetailTextureDictionaryRecord final {
    std::string name;
    bool is_current{false};
    bool destroying{false};
    bool retired{false};

    NativeRetailTextureDictionaryRecord() = default;
};

NativeRetailRendererTextureDictionaries::NativeRetailRendererTextureDictionaries(
    std::weak_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator)
    : session_(std::move(session)),
      allocator_(std::move(allocator)) {}

NativeRetailRendererTextureDictionaries::~NativeRetailRendererTextureDictionaries() {
    retire();
}

bool NativeRetailRendererTextureDictionaries::access(RetailRendererTextureDictionaryFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != thread_) {
        f.kind = RetailRendererTextureDictionaryError::wrong_thread;
        f.operation = "0409 thread affinity check";
        return false;
    }
    if (retired_) {
        f.kind = RetailRendererTextureDictionaryError::retired;
        f.operation = "0409 dictionary service retired";
        return false;
    }
    if (session_.expired()) {
        f.kind = RetailRendererTextureDictionaryError::owner;
        f.operation = "0409 session lifetime expired";
        return false;
    }
    return true;
}

std::shared_ptr<NativeRetailTextureDictionaryRecord> NativeRetailRendererTextureDictionaries::validate(
    NativeRetailTextureDictionaryHandle handle,
    RetailRendererTextureDictionaryFailure& f) const noexcept {
    if (handle.empty()) {
        f.kind = RetailRendererTextureDictionaryError::invalid_handle;
        f.operation = "0409 empty dictionary handle";
        return {};
    }
    auto owner = handle.owner_.lock();
    if (!owner || owner.get() != this) {
        f.kind = RetailRendererTextureDictionaryError::foreign_owner;
        f.operation = "0409 foreign dictionary owner";
        return {};
    }
    auto rec = handle.record_.lock();
    if (!rec || rec->retired) {
        f.kind = RetailRendererTextureDictionaryError::retired_dictionary;
        f.operation = "0409 retired dictionary handle";
        return {};
    }
    return rec;
}

std::shared_ptr<NativeRetailRendererTextureDictionaries> NativeRetailRendererTextureDictionaries::create(
    NativeRetailRendererCore& core,
    RetailRendererTextureDictionaryFailure& f) noexcept {
    RetailRendererCoreFailure cf;
    if (!core.access(cf)) {
        f.kind = RetailRendererTextureDictionaryError::owner;
        f.operation = "0409 core access refusal";
        return {};
    }
    auto allocator = core.allocator_;
    if (!allocator) {
        f.kind = RetailRendererTextureDictionaryError::allocator;
        f.operation = "0409 allocator missing";
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailRendererTextureDictionaries>(
            new NativeRetailRendererTextureDictionaries(core.session_, std::move(allocator)));
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererTextureDictionaryError::allocation;
        f.operation = "0409 dictionary service allocation";
        return {};
    } catch (...) {
        f.kind = RetailRendererTextureDictionaryError::native_exception;
        f.operation = "0409 dictionary service constructor";
        return {};
    }
}

std::optional<NativeRetailTextureDictionaryHandle> NativeRetailRendererTextureDictionaries::create_dictionary(
    RetailRendererTextureDictionaryDescription desc,
    RetailRendererTextureDictionaryFailure& f) noexcept {
    if (!access(f)) return {};
    if (busy_) {
        f.kind = RetailRendererTextureDictionaryError::reentrant;
        f.operation = "0409 reentrant dictionary creation";
        return {};
    }
    struct Guard {
        bool& b;
        Guard(bool& b) : b(b) { b = true; }
        ~Guard() { b = false; }
    } guard(busy_);

    auto rec = heap_object<NativeRetailTextureDictionaryRecord>(
        allocator_, f, [&](void* ptr) {
            return ::new (ptr) NativeRetailTextureDictionaryRecord();
        });
    if (!rec) return {};

    rec->name = std::move(desc.name);

    NativeRetailTextureDictionaryHandle handle(
        shared_from_this(),
        std::weak_ptr<NativeRetailTextureDictionaryRecord>(rec));

    try {
        records_.push_back(rec);
    } catch (...) {
        f.kind = RetailRendererTextureDictionaryError::allocation;
        f.operation = "0409 dictionary record registration";
        return {};
    }

    return handle;
}

bool NativeRetailRendererTextureDictionaries::destroy_dictionary(
    NativeRetailTextureDictionaryHandle handle,
    RetailRendererTextureDictionaryFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;

    if (rec->destroying) return true;
    rec->destroying = true;

    if (current_record_ == rec) {
        current_record_.reset();
        current_handle_ = {};
    }

    rec->retired = true;
    auto it = std::find(records_.begin(), records_.end(), rec);
    if (it != records_.end()) {
        records_.erase(it);
    }
    return true;
}

bool NativeRetailRendererTextureDictionaries::set_current_dictionary(
    NativeRetailTextureDictionaryHandle handle,
    RetailRendererTextureDictionaryFailure& f) noexcept {
    if (!access(f)) return false;
    if (handle.empty()) {
        if (current_record_) current_record_->is_current = false;
        current_record_.reset();
        current_handle_ = {};
        return true;
    }
    auto rec = validate(handle, f);
    if (!rec) return false;
    if (current_record_) current_record_->is_current = false;
    current_record_ = rec;
    current_record_->is_current = true;
    current_handle_ = handle;
    return true;
}

NativeRetailTextureDictionaryHandle NativeRetailRendererTextureDictionaries::current_dictionary() const noexcept {
    if (retired_ || !current_record_ || current_record_->retired) {
        return {};
    }
    return current_handle_;
}

std::optional<RetailRendererTextureDictionaryObservation> NativeRetailRendererTextureDictionaries::observe(
    NativeRetailTextureDictionaryHandle dict_handle,
    RetailRendererTextureDictionaryFailure& f) const noexcept {
    if (!access(f)) return {};
    auto rec = validate(dict_handle, f);
    if (!rec) return {};

    RetailRendererTextureDictionaryObservation obs;
    obs.name = rec->name;
    obs.is_current = (rec == current_record_);
    obs.destroying = rec->destroying;
    return obs;
}

std::size_t NativeRetailRendererTextureDictionaries::active_count() const noexcept {
    return records_.size();
}

void NativeRetailRendererTextureDictionaries::retire() noexcept {
    if (retired_) return;
    retired_ = true;
    current_record_.reset();
    current_handle_ = {};
    for (auto& rec : records_) {
        if (rec && !rec->retired) {
            rec->destroying = true;
            rec->retired = true;
        }
    }
    records_.clear();
}

std::shared_ptr<NativeRetailRendererTextureDictionaries> NativeRetailRendererCore::texture_dictionaries(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return texture_dictionaries_;
}

std::optional<std::uint32_t> NativeRetailRendererCore::module_0409_count(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return texture_dictionaries_ ? 1u : 0u;
}

namespace renderer_required {

bool construct_core_plugin_0409(NativeRetailRendererCore& core, RetailRendererCoreFailure& f) {
    if (!core.access(f)) return false;
    if (core.texture_dictionaries_) {
        f.kind = RetailRendererCoreError::reentrant;
        f.operation = "0409 duplicate plugin registration";
        return false;
    }
    RetailRendererTextureDictionaryFailure df;
    auto owner = NativeRetailRendererTextureDictionaries::create(core, df);
    if (!owner) {
        f.kind = RetailRendererCoreError::construction;
        f.operation = df.operation ? df.operation : "0409 construction failure";
        return false;
    }
    core.texture_dictionaries_ = std::move(owner);
    core.module_0409_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void destroy_core_plugin_0409(NativeRetailRendererCore& core) noexcept {
    if (core.texture_dictionaries_) {
        core.texture_dictionaries_->retire();
        core.texture_dictionaries_.reset();
        core.module_0409_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

} // namespace renderer_required

} // namespace mka::retail
