#include "mka/retail_renderer_materials.hpp"
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
    RetailRendererMaterialFailure& f,
    Create create) noexcept {
    static_assert(alignof(T) <= 16);
    auto allocation = heap->allocate(sizeof(T), f.allocator);
    if (!allocation) {
        f.kind = RetailRendererMaterialError::allocator;
        f.operation = "040a actual typed Permanent allocation";
        return {};
    }
    try {
        auto* object = create(allocation->data());
        return std::shared_ptr<T>(object, [heap, alloc = std::move(*allocation)](T* p) mutable {
            p->~T();
            alloc = memory::NativeAllocation{};
        });
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererMaterialError::allocation;
        f.operation = "040a actual native ownership";
    } catch (...) {
        f.kind = RetailRendererMaterialError::native_exception;
        f.operation = "040a native construction";
    }
    return {};
}

} // namespace

struct NativeRetailMaterialRecord final {
    RetailRendererRgbaColor color{255, 255, 255, 255};
    RetailRendererSurfaceProperties surface{};
    NativeRetailTextureHandle texture{};
    std::int32_t ref_count{1};
    bool destroying{false};
    bool retired{false};

    NativeRetailMaterialRecord() = default;
};

NativeRetailRendererMaterials::NativeRetailRendererMaterials(
    std::weak_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
    std::shared_ptr<NativeRetailRendererTextures> textures)
    : session_(std::move(session)),
      allocator_(std::move(allocator)),
      textures_(std::move(textures)) {}

NativeRetailRendererMaterials::~NativeRetailRendererMaterials() {
    retire();
}

bool NativeRetailRendererMaterials::access(RetailRendererMaterialFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != thread_) {
        f.kind = RetailRendererMaterialError::wrong_thread;
        f.operation = "040a thread affinity check";
        return false;
    }
    if (retired_) {
        f.kind = RetailRendererMaterialError::retired;
        f.operation = "040a materials manager retired";
        return false;
    }
    auto session = session_.lock();
    RetailGameSessionFailure sf;
    if (!session || !session->validate_current_thread(sf)) {
        f.kind = RetailRendererMaterialError::owner;
        f.operation = "040a session lease invalid";
        return false;
    }
    return true;
}

std::shared_ptr<NativeRetailMaterialRecord> NativeRetailRendererMaterials::validate(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) const noexcept {
    if (!access(f)) return {};
    if (handle.empty()) {
        f.kind = RetailRendererMaterialError::invalid_handle;
        f.operation = "040a empty material handle";
        return {};
    }
    auto owner = handle.owner_.lock();
    if (!owner || owner.get() != this) {
        f.kind = RetailRendererMaterialError::foreign_owner;
        f.operation = "040a foreign material owner";
        return {};
    }
    auto rec = handle.record_.lock();
    if (!rec || rec->retired) {
        f.kind = RetailRendererMaterialError::retired_material;
        f.operation = "040a retired material record";
        return {};
    }
    return rec;
}

std::shared_ptr<NativeRetailRendererMaterials> NativeRetailRendererMaterials::create(
    NativeRetailRendererCore& core,
    RetailRendererMaterialFailure& f) noexcept {
    f = {};
    if (std::this_thread::get_id() != core.thread_) {
        f.kind = RetailRendererMaterialError::wrong_thread;
        f.operation = "040a thread affinity check";
        return {};
    }
    auto session = core.session_.lock();
    if (!session) {
        f.kind = RetailRendererMaterialError::owner;
        f.operation = "040a core session expired";
        return {};
    }
    if (!core.allocator_) {
        f.kind = RetailRendererMaterialError::allocator;
        f.operation = "040a core allocator missing";
        return {};
    }
    return std::shared_ptr<NativeRetailRendererMaterials>(
        new (std::nothrow) NativeRetailRendererMaterials(core.session_, core.allocator_, core.textures_));
}

std::optional<NativeRetailMaterialHandle> NativeRetailRendererMaterials::create_material(
    RetailRendererMaterialFailure& f) noexcept {
    return create_material(RetailRendererMaterialDescription{}, f);
}

std::optional<NativeRetailMaterialHandle> NativeRetailRendererMaterials::create_material(
    RetailRendererMaterialDescription desc,
    RetailRendererMaterialFailure& f) noexcept {
    if (!access(f)) return {};

    auto rec = heap_object<NativeRetailMaterialRecord>(
        allocator_, f, [](void* ptr) {
            return ::new (ptr) NativeRetailMaterialRecord();
        });
    if (!rec) return {};

    rec->color = desc.color;
    rec->surface = desc.surface;
    rec->texture = desc.texture;
    rec->ref_count = 1;

    try {
        records_.push_back(rec);
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererMaterialError::allocation;
        f.operation = "040a record registry retention";
        return {};
    }

    return NativeRetailMaterialHandle(weak_from_this(), rec);
}

bool NativeRetailRendererMaterials::destroy_material(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;

    if (rec->destroying) return true;
    rec->destroying = true;

    auto it = std::find(records_.begin(), records_.end(), rec);
    if (it != records_.end()) {
        records_.erase(it);
    }

    rec->texture = NativeRetailTextureHandle{};
    rec->retired = true;
    return true;
}

std::optional<std::int32_t> NativeRetailRendererMaterials::add_ref(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) noexcept {
    if (!access(f)) return {};
    auto rec = validate(handle, f);
    if (!rec) return {};
    rec->ref_count++;
    return rec->ref_count;
}

bool NativeRetailRendererMaterials::release(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;
    if (rec->ref_count <= 0) {
        f.kind = RetailRendererMaterialError::ref_count_underflow;
        f.operation = "040a material ref count underflow";
        return false;
    }
    rec->ref_count--;
    if (rec->ref_count == 0) {
        return destroy_material(handle, f);
    }
    return true;
}

bool NativeRetailRendererMaterials::set_color(
    NativeRetailMaterialHandle handle,
    RetailRendererRgbaColor color,
    RetailRendererMaterialFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;
    rec->color = color;
    return true;
}

std::optional<RetailRendererRgbaColor> NativeRetailRendererMaterials::get_color(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) const noexcept {
    if (!access(f)) return {};
    auto rec = validate(handle, f);
    if (!rec) return {};
    return rec->color;
}

bool NativeRetailRendererMaterials::set_surface_properties(
    NativeRetailMaterialHandle handle,
    RetailRendererSurfaceProperties surface,
    RetailRendererMaterialFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;
    rec->surface = surface;
    return true;
}

std::optional<RetailRendererSurfaceProperties> NativeRetailRendererMaterials::get_surface_properties(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) const noexcept {
    if (!access(f)) return {};
    auto rec = validate(handle, f);
    if (!rec) return {};
    return rec->surface;
}

bool NativeRetailRendererMaterials::set_texture(
    NativeRetailMaterialHandle handle,
    NativeRetailTextureHandle texture,
    RetailRendererMaterialFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;
    rec->texture = texture;
    return true;
}

std::optional<NativeRetailTextureHandle> NativeRetailRendererMaterials::get_texture(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) const noexcept {
    if (!access(f)) return {};
    auto rec = validate(handle, f);
    if (!rec) return {};
    return rec->texture;
}

std::optional<RetailRendererMaterialObservation> NativeRetailRendererMaterials::observe(
    NativeRetailMaterialHandle handle,
    RetailRendererMaterialFailure& f) const noexcept {
    if (!access(f)) return {};
    auto rec = validate(handle, f);
    if (!rec) return {};
    RetailRendererMaterialObservation obs;
    obs.color = rec->color;
    obs.surface = rec->surface;
    obs.ref_count = rec->ref_count;
    obs.has_texture = !rec->texture.empty();
    obs.destroying = rec->destroying;
    return obs;
}

std::size_t NativeRetailRendererMaterials::active_count() const noexcept {
    return records_.size();
}

void NativeRetailRendererMaterials::retire() noexcept {
    if (retired_) return;
    retired_ = true;
    for (auto& r : records_) {
        if (r) {
            r->retired = true;
            r->texture = NativeRetailTextureHandle{};
        }
    }
    records_.clear();
}

std::shared_ptr<NativeRetailRendererMaterials> NativeRetailRendererCore::materials(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return materials_;
}

std::optional<std::uint32_t> NativeRetailRendererCore::module_040a_count(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return module_040a_count_.load(std::memory_order_relaxed);
}

namespace renderer_required {

bool construct_core_plugin_040a(NativeRetailRendererCore& core, RetailRendererCoreFailure& f) {
    if (!core.access(f)) return false;
    if (core.materials_) {
        f.kind = RetailRendererCoreError::reentrant;
        f.operation = "040a duplicate plugin registration";
        return false;
    }
    RetailRendererMaterialFailure mf;
    auto owner = NativeRetailRendererMaterials::create(core, mf);
    if (!owner) {
        f.kind = RetailRendererCoreError::construction;
        f.operation = mf.operation ? mf.operation : "040a construction failure";
        return false;
    }
    core.materials_ = std::move(owner);
    core.module_040a_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void destroy_core_plugin_040a(NativeRetailRendererCore& core) noexcept {
    if (core.materials_) {
        core.materials_->retire();
        core.materials_.reset();
        core.module_040a_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

} // namespace renderer_required

} // namespace mka::retail
