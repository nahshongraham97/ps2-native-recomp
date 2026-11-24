#include "mka/retail_renderer_geometry.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <cstring>
#include <new>

namespace mka::retail {

namespace {

struct NativeRetailGeometryObject {
    alignas(16) std::uint8_t container[40]{};
    RetailRendererGeometryDescription desc{};
    std::int32_t ref_count{1};
};

template<class T, class Create>
std::shared_ptr<T> heap_object(
    const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,
    RetailRendererGeometryFailure& f,
    Create create) noexcept {
    static_assert(alignof(T) <= 16);
    auto allocation = heap->allocate(sizeof(T), f.allocator);
    if (!allocation) {
        f.kind = RetailRendererGeometryError::allocator;
        f.operation = "040b actual typed Permanent allocation";
        return {};
    }
    try {
        auto* object = create(allocation->data());
        return std::shared_ptr<T>(object, [heap, alloc = std::move(*allocation)](T* p) mutable {
            p->~T();
            alloc = memory::NativeAllocation{};
        });
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererGeometryError::allocation;
        f.operation = "040b actual native ownership";
    } catch (...) {
        f.kind = RetailRendererGeometryError::native_exception;
        f.operation = "040b native construction";
    }
    return {};
}

} // namespace

struct NativeRetailGeometryRecord final {
    std::shared_ptr<NativeRetailGeometryObject> geometry;
    bool destroying{false};
    bool retired{false};

    NativeRetailGeometryRecord() = default;
};

NativeRetailRendererGeometry::NativeRetailRendererGeometry(
    std::weak_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator)
    : session_(std::move(session)),
      allocator_(std::move(allocator)) {}

NativeRetailRendererGeometry::~NativeRetailRendererGeometry() {
    retire();
}

bool NativeRetailRendererGeometry::access(RetailRendererGeometryFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != thread_) {
        f.kind = RetailRendererGeometryError::wrong_thread;
        f.operation = "040b geometry thread affinity check";
        return false;
    }
    const auto owner = session_.lock();
    RetailGameSessionFailure sf;
    if (!owner || !owner->validate_current_thread(sf)) {
        f.kind = RetailRendererGeometryError::owner;
        f.operation = "040b game session lifetime";
        return false;
    }
    if (retired_) {
        f.kind = RetailRendererGeometryError::retired;
        f.operation = "040b geometry module retired";
        return false;
    }
    return true;
}

std::shared_ptr<NativeRetailGeometryRecord> NativeRetailRendererGeometry::validate(
    NativeRetailGeometryHandle handle,
    RetailRendererGeometryFailure& f) const noexcept {
    if (!access(f)) return {};
    if (handle.empty()) {
        f.kind = RetailRendererGeometryError::invalid_handle;
        f.operation = "040b empty geometry handle";
        return {};
    }
    if (handle.owner_.lock().get() != this) {
        f.kind = RetailRendererGeometryError::foreign_owner;
        f.operation = "040b foreign geometry owner";
        return {};
    }
    auto rec = handle.record_.lock();
    if (!rec || rec->retired || !rec->geometry) {
        f.kind = RetailRendererGeometryError::retired_geometry;
        f.operation = "040b retired geometry handle";
        return {};
    }
    return rec;
}

std::shared_ptr<NativeRetailRendererGeometry> NativeRetailRendererGeometry::create(
    NativeRetailRendererCore& core,
    RetailRendererGeometryFailure& f) noexcept {
    RetailRendererCoreFailure cf;
    if (!core.access(cf)) {
        f.kind = RetailRendererGeometryError::owner;
        f.operation = "040b core access refusal";
        return {};
    }
    if (!core.allocator_) {
        f.kind = RetailRendererGeometryError::allocator;
        f.operation = "040b allocator missing";
        return {};
    }
    auto module = heap_object<NativeRetailRendererGeometry>(
        core.allocator_, f,
        [&](void* p) {
            return new (p) NativeRetailRendererGeometry(core.session_, core.allocator_);
        });
    if (!module) {
        f.operation = "040b geometry module allocation";
        return {};
    }
    return module;
}

std::optional<NativeRetailGeometryHandle> NativeRetailRendererGeometry::create_geometry(
    RetailRendererGeometryFailure& f) noexcept {
    return create_geometry(RetailRendererGeometryDescription{}, f);
}

std::optional<NativeRetailGeometryHandle> NativeRetailRendererGeometry::create_geometry(
    RetailRendererGeometryDescription desc,
    RetailRendererGeometryFailure& f) noexcept {
    if (!access(f)) return {};

    auto geom_obj = heap_object<NativeRetailGeometryObject>(
        allocator_, f,
        [&](void* p) {
            auto* obj = new (p) NativeRetailGeometryObject();
            obj->desc = desc;
            obj->ref_count = 1;
            return obj;
        });
    if (!geom_obj) {
        f.operation = "040b native geometry object allocation";
        return {};
    }

    auto rec = heap_object<NativeRetailGeometryRecord>(
        allocator_, f,
        [&](void* p) {
            auto* r = new (p) NativeRetailGeometryRecord();
            r->geometry = std::move(geom_obj);
            return r;
        });
    if (!rec) {
        f.operation = "040b geometry record allocation";
        return {};
    }

    try {
        records_.push_back(rec);
    } catch (...) {
        f.kind = RetailRendererGeometryError::allocation;
        f.operation = "040b geometry record registration";
        return {};
    }

    return NativeRetailGeometryHandle(shared_from_this(), rec);
}

bool NativeRetailRendererGeometry::destroy_geometry(
    NativeRetailGeometryHandle handle,
    RetailRendererGeometryFailure& f) noexcept {
    auto rec = validate(handle, f);
    if (!rec) return false;

    rec->destroying = true;
    rec->retired = true;
    rec->geometry.reset();

    records_.erase(
        std::remove_if(records_.begin(), records_.end(),
            [&](const std::shared_ptr<NativeRetailGeometryRecord>& r) {
                return !r || r->retired;
            }),
        records_.end());

    return true;
}

std::optional<std::int32_t> NativeRetailRendererGeometry::add_ref(
    NativeRetailGeometryHandle handle,
    RetailRendererGeometryFailure& f) noexcept {
    auto rec = validate(handle, f);
    if (!rec) return {};
    rec->geometry->ref_count += 1;
    return rec->geometry->ref_count;
}

bool NativeRetailRendererGeometry::release(
    NativeRetailGeometryHandle handle,
    RetailRendererGeometryFailure& f) noexcept {
    auto rec = validate(handle, f);
    if (!rec) return false;
    if (rec->geometry->ref_count <= 0) {
        f.kind = RetailRendererGeometryError::ref_count_underflow;
        f.operation = "040b release ref_count underflow";
        return false;
    }
    rec->geometry->ref_count -= 1;
    if (rec->geometry->ref_count == 0) {
        return destroy_geometry(handle, f);
    }
    return true;
}

std::optional<RetailRendererGeometryObservation> NativeRetailRendererGeometry::observe(
    NativeRetailGeometryHandle handle,
    RetailRendererGeometryFailure& f) const noexcept {
    auto rec = validate(handle, f);
    if (!rec) return {};

    RetailRendererGeometryObservation obs;
    obs.flags = rec->geometry->desc.flags;
    obs.num_triangles = rec->geometry->desc.num_triangles;
    obs.num_vertices = rec->geometry->desc.num_vertices;
    obs.ref_count = rec->geometry->ref_count;
    obs.destroying = rec->destroying;
    return obs;
}

std::size_t NativeRetailRendererGeometry::active_count() const noexcept {
    return std::count_if(records_.begin(), records_.end(),
        [](const std::shared_ptr<NativeRetailGeometryRecord>& r) {
            return r && !r->retired && r->geometry;
        });
}

void NativeRetailRendererGeometry::retire() noexcept {
    retired_ = true;
    for (auto& r : records_) {
        if (r) {
            r->retired = true;
            r->geometry.reset();
        }
    }
    records_.clear();
}

// ---------------------------------------------------------------------------
// NativeRetailRendererCore Integration
// ---------------------------------------------------------------------------

std::shared_ptr<NativeRetailRendererGeometry> NativeRetailRendererCore::geometry(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    if (!geometry_) {
        f.kind = RetailRendererCoreError::owner;
        f.operation = "040b geometry module not constructed";
        return {};
    }
    return geometry_;
}

std::optional<std::uint32_t> NativeRetailRendererCore::module_040b_count(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return module_040b_count_.load(std::memory_order_relaxed);
}

namespace renderer_required {

bool construct_core_plugin_040b(NativeRetailRendererCore& core, RetailRendererCoreFailure& f) {
    if (!core.access(f)) return false;
    if (core.geometry_) {
        f.kind = RetailRendererCoreError::reentrant;
        f.operation = "040b duplicate plugin registration";
        return false;
    }
    RetailRendererGeometryFailure gf;
    auto module = NativeRetailRendererGeometry::create(core, gf);
    if (!module) {
        f.kind = RetailRendererCoreError::allocation;
        f.operation = gf.operation ? gf.operation : "040b construction failure";
        return false;
    }
    core.geometry_ = std::move(module);
    core.module_040b_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void destroy_core_plugin_040b(NativeRetailRendererCore& core) noexcept {
    if (core.geometry_) {
        core.geometry_->retire();
        core.geometry_.reset();
        core.module_040b_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

} // namespace renderer_required

} // namespace mka::retail
