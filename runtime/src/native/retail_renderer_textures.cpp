#include "mka/retail_renderer_textures.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <new>

namespace mka::retail {

namespace {

template<class T, class Create>
std::shared_ptr<T> heap_object(
    const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,
    RetailRendererTextureFailure& f,
    Create create) noexcept {
    static_assert(alignof(T) <= 16);
    auto allocation = heap->allocate(sizeof(T), f.allocator);
    if (!allocation) {
        f.kind = RetailRendererTextureError::allocator;
        f.operation = "0408 actual typed Permanent allocation";
        return {};
    }
    try {
        auto* object = create(allocation->data());
        return std::shared_ptr<T>(object, [heap, alloc = std::move(*allocation)](T* p) mutable {
            p->~T();
            alloc = memory::NativeAllocation{};
        });
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererTextureError::allocation;
        f.operation = "0408 actual native ownership";
    } catch (...) {
        f.kind = RetailRendererTextureError::native_exception;
        f.operation = "0408 native construction";
    }
    return {};
}

bool iequals(std::string_view a, std::string_view b) noexcept {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace

struct NativeRetailTextureRecord final {
    std::string name;
    std::string mask;
    RetailRendererTextureFilter filter{RetailRendererTextureFilter::linear};
    RetailRendererTextureAddress address_u{RetailRendererTextureAddress::wrap};
    RetailRendererTextureAddress address_v{RetailRendererTextureAddress::wrap};
    NativeRetailRasterHandle raster;
    std::int32_t ref_count{1};
    bool is_default{false};
    bool destroying{false};
    bool retired{false};

    NativeRetailTextureRecord() = default;
};

NativeRetailRendererTextures::NativeRetailRendererTextures(
    std::weak_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
    std::shared_ptr<NativeRetailRendererRasters> rasters)
    : session_(std::move(session)),
      allocator_(std::move(allocator)),
      rasters_(std::move(rasters)) {}

NativeRetailRendererTextures::~NativeRetailRendererTextures() {
    retire();
}

bool NativeRetailRendererTextures::access(RetailRendererTextureFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != thread_) {
        f.kind = RetailRendererTextureError::wrong_thread;
        f.operation = "0408 thread affinity check";
        return false;
    }
    if (retired_) {
        f.kind = RetailRendererTextureError::retired;
        f.operation = "0408 manager retired";
        return false;
    }
    auto session = session_.lock();
    RetailGameSessionFailure sf;
    if (!session || !session->validate_current_thread(sf)) {
        f.kind = RetailRendererTextureError::owner;
        f.operation = "0408 session lease invalid";
        return false;
    }
    return true;
}

std::shared_ptr<NativeRetailTextureRecord> NativeRetailRendererTextures::validate(
    NativeRetailTextureHandle handle,
    RetailRendererTextureFailure& f) const noexcept {
    if (!access(f)) return {};
    if (handle.empty()) {
        f.kind = RetailRendererTextureError::invalid_handle;
        f.operation = "empty texture handle";
        return {};
    }
    auto owner = handle.owner_.lock();
    if (owner.get() != this) {
        f.kind = RetailRendererTextureError::foreign_owner;
        f.operation = "texture foreign owner";
        return {};
    }
    auto rec = handle.record_.lock();
    if (!rec || rec->retired) {
        f.kind = RetailRendererTextureError::retired_texture;
        f.operation = "texture record retired";
        return {};
    }
    return rec;
}

std::shared_ptr<NativeRetailRendererTextures> NativeRetailRendererTextures::create(
    NativeRetailRendererCore& core,
    RetailRendererTextureFailure& f) noexcept {
    RetailRendererCoreFailure cf;
    if (!core.access(cf)) {
        f.kind = RetailRendererTextureError::owner;
        f.operation = "0408 core access failure";
        return {};
    }
    auto rasters = core.rasters(cf);
    if (!rasters) {
        f.kind = RetailRendererTextureError::raster_required;
        f.operation = "0408 requires active 0407 rasters service";
        return {};
    }
    auto allocator = core.allocator_;
    if (!allocator) {
        f.kind = RetailRendererTextureError::allocator;
        f.operation = "0408 core allocator invalid";
        return {};
    }

    auto instance = heap_object<NativeRetailRendererTextures>(allocator, f, [&](void* p) {
        return new (p) NativeRetailRendererTextures(core.session_, allocator, rasters);
    });
    if (!instance) return {};

    // Recovered 0x002b8500: create default dummy texture record
    auto default_rec = heap_object<NativeRetailTextureRecord>(allocator, f, [&](void* p) {
        auto* rec = new (p) NativeRetailTextureRecord();
        rec->name = "";
        rec->mask = "";
        rec->filter = RetailRendererTextureFilter::linear; // rwFILTERLINEAR (mode 6 or 2)
        rec->address_u = RetailRendererTextureAddress::wrap;
        rec->address_v = RetailRendererTextureAddress::wrap;
        rec->ref_count = 1;
        rec->is_default = true;
        return rec;
    });
    if (!default_rec) return {};

    instance->default_record_ = default_rec;
    instance->default_handle_ = NativeRetailTextureHandle(instance, default_rec);
    try {
        instance->records_.push_back(default_rec);
    } catch (...) {
        f.kind = RetailRendererTextureError::allocation;
        f.operation = "default texture record registration";
        return {};
    }

    return instance;
}

std::optional<NativeRetailTextureHandle> NativeRetailRendererTextures::create_texture(
    RetailRendererTextureDescription desc,
    NativeRetailRasterHandle raster,
    RetailRendererTextureFailure& f) noexcept {
    if (!access(f)) return {};
    if (busy_) {
        f.kind = RetailRendererTextureError::reentrant;
        f.operation = "texture creation reentrant";
        return {};
    }
    if (raster.empty()) {
        f.kind = RetailRendererTextureError::raster_required;
        f.operation = "texture creation requires valid raster handle";
        return {};
    }

    // Verify raster handle belongs to our connected rasters service
    RetailRendererRasterFailure rf;
    auto obs = rasters_->observe(raster, rf);
    if (!obs) {
        f.kind = RetailRendererTextureError::invalid_raster;
        f.operation = "texture creation rejected: invalid or retired raster";
        return {};
    }

    auto record = heap_object<NativeRetailTextureRecord>(allocator_, f, [&](void* p) {
        auto* rec = new (p) NativeRetailTextureRecord();
        rec->name = std::move(desc.name);
        rec->mask = std::move(desc.mask);
        rec->filter = desc.filter;
        rec->address_u = desc.address_u;
        rec->address_v = desc.address_v;
        rec->raster = raster;
        rec->ref_count = 1;
        rec->is_default = false;
        return rec;
    });
    if (!record) return {};

    try {
        records_.push_back(record);
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererTextureError::allocation;
        f.operation = "texture registry growth";
        return {};
    } catch (...) {
        f.kind = RetailRendererTextureError::native_exception;
        f.operation = "texture registry growth";
        return {};
    }

    return NativeRetailTextureHandle(shared_from_this(), record);
}

bool NativeRetailRendererTextures::destroy_texture(
    NativeRetailTextureHandle handle,
    RetailRendererTextureFailure& f) noexcept {
    auto rec = validate(handle, f);
    if (!rec) return false;

    if (rec->is_default) {
        // Default sentinel texture is owned by manager lifetime
        return true;
    }

    if (rec->ref_count > 1) {
        --rec->ref_count;
        return true;
    }

    // Ref count reached 0 -> retire record
    rec->ref_count = 0;
    rec->destroying = true;
    rec->raster = NativeRetailRasterHandle{};
    rec->retired = true;

    auto it = std::find(records_.begin(), records_.end(), rec);
    if (it != records_.end()) {
        records_.erase(it);
    }
    return true;
}

std::optional<std::int32_t> NativeRetailRendererTextures::add_ref(
    NativeRetailTextureHandle handle,
    RetailRendererTextureFailure& f) noexcept {
    auto rec = validate(handle, f);
    if (!rec) return {};
    ++rec->ref_count;
    return rec->ref_count;
}

std::optional<NativeRetailRasterHandle> NativeRetailRendererTextures::raster(
    NativeRetailTextureHandle handle,
    RetailRendererTextureFailure& f) const noexcept {
    auto rec = validate(handle, f);
    if (!rec) return {};
    return rec->raster;
}

bool NativeRetailRendererTextures::set_raster(
    NativeRetailTextureHandle handle,
    NativeRetailRasterHandle raster,
    RetailRendererTextureFailure& f) noexcept {
    auto rec = validate(handle, f);
    if (!rec) return false;
    if (!raster.empty()) {
        RetailRendererRasterFailure rf;
        auto obs = rasters_->observe(raster, rf);
        if (!obs) {
            f.kind = RetailRendererTextureError::invalid_raster;
            f.operation = "set_raster: invalid raster handle";
            return false;
        }
    }
    rec->raster = raster;
    return true;
}

bool NativeRetailRendererTextures::set_filter_addressing(
    NativeRetailTextureHandle handle,
    RetailRendererTextureFilter filter,
    RetailRendererTextureAddress address_u,
    RetailRendererTextureAddress address_v,
    RetailRendererTextureFailure& f) noexcept {
    auto rec = validate(handle, f);
    if (!rec) return false;
    rec->filter = filter;
    rec->address_u = address_u;
    rec->address_v = address_v;
    return true;
}

std::optional<RetailRendererTextureObservation> NativeRetailRendererTextures::observe(
    NativeRetailTextureHandle handle,
    RetailRendererTextureFailure& f) const noexcept {
    auto rec = validate(handle, f);
    if (!rec) return {};
    RetailRendererTextureObservation obs;
    obs.name = rec->name;
    obs.mask = rec->mask;
    obs.filter = rec->filter;
    obs.address_u = rec->address_u;
    obs.address_v = rec->address_v;
    obs.ref_count = rec->ref_count;
    obs.has_raster = !rec->raster.empty();
    obs.is_default = rec->is_default;
    obs.destroying = rec->destroying;
    return obs;
}

std::optional<NativeRetailTextureHandle> NativeRetailRendererTextures::find_texture(
    std::string_view name,
    RetailRendererTextureFailure& f) const noexcept {
    if (!access(f)) return {};
    for (const auto& rec : records_) {
        if (!rec->retired && iequals(rec->name, name)) {
            return NativeRetailTextureHandle(
                const_cast<NativeRetailRendererTextures*>(this)->shared_from_this(),
                rec);
        }
    }
    return std::nullopt;
}

NativeRetailTextureHandle NativeRetailRendererTextures::default_texture() const noexcept {
    return default_handle_;
}

std::size_t NativeRetailRendererTextures::active_count() const noexcept {
    return records_.size();
}

void NativeRetailRendererTextures::retire() noexcept {
    if (retired_) return;
    retired_ = true;
    for (auto& rec : records_) {
        if (rec) {
            rec->retired = true;
            rec->raster = NativeRetailRasterHandle{};
        }
    }
    records_.clear();
    default_record_.reset();
    default_handle_ = NativeRetailTextureHandle{};
}

std::shared_ptr<NativeRetailRendererTextures> NativeRetailRendererCore::textures(RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    if (!textures_) {
        f.kind = RetailRendererCoreError::unbound_factory;
        f.operation = "0408 actual texture resource required";
        return {};
    }
    return textures_;
}

std::optional<std::uint32_t> NativeRetailRendererCore::module_0408_count(RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return textures_ ? 1u : 0u;
}

namespace renderer_required {

bool construct_core_plugin_0408(NativeRetailRendererCore& core, RetailRendererCoreFailure& f) {
    if (!core.access(f)) return false;
    if (core.textures_) {
        f.kind = RetailRendererCoreError::reentrant;
        f.operation = "0408 live texture resource already published";
        return false;
    }
    RetailRendererTextureFailure detail;
    auto owner = NativeRetailRendererTextures::create(core, detail);
    if (!owner) {
        f.kind = (detail.kind == RetailRendererTextureError::allocation ||
                  detail.allocator.heap.kind == memory::NativeHeapError::allocation_failed)
            ? RetailRendererCoreError::allocation
            : RetailRendererCoreError::allocator;
        f.operation = detail.operation;
        f.allocator = detail.allocator;
        return false;
    }
    core.textures_ = std::move(owner);
    core.module_0408_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void destroy_core_plugin_0408(NativeRetailRendererCore& core) noexcept {
    if (core.textures_) {
        core.textures_->retire();
        core.textures_.reset();
        core.module_0408_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

} // namespace renderer_required

} // namespace mka::retail
