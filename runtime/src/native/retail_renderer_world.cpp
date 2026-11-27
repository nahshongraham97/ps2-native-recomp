#include "mka/retail_renderer_world.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

namespace mka::retail {

namespace {

template<class T, class Create>
std::shared_ptr<T> heap_object(
    const std::shared_ptr<NativeRetailRendererAllocatorService>& heap,
    RetailRendererWorldFailure& f,
    Create create) noexcept {
    static_assert(alignof(T) <= 16);
    auto allocation = heap->allocate(sizeof(T), f.allocator);
    if (!allocation) {
        f.kind = RetailRendererWorldError::allocator;
        f.operation = "0412 actual typed Permanent allocation";
        return {};
    }
    try {
        auto* object = create(allocation->data());
        return std::shared_ptr<T>(object, [heap, alloc = std::move(*allocation)](T* p) mutable {
            p->~T();
            alloc = memory::NativeAllocation{};
        });
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 actual native ownership";
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 native construction";
    }
    return {};
}

} // namespace

NativeRetailWorld::NativeRetailWorld(
    const std::shared_ptr<NativeRetailGameSession>& session) noexcept
    : NativeRetailRenderWorld(session) {}

NativeRetailWorld::~NativeRetailWorld() = default;

bool NativeRetailWorld::add_scene_object(RetailWorldSceneObject obj, RetailRendererWorldFailure& f) noexcept {
    f = {};
    if (obj.id == 0 || obj.frame.empty() || obj.triangles.empty() || (obj.triangles.size() % 3) != 0 ||
        !std::isfinite(obj.bounding_radius) || obj.bounding_radius < 0.0f) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 valid scene object id, frame, non-empty triangles, and finite radius required";
        return false;
    }
    for (const auto& existing : scene_objects_) {
        if (existing.id == obj.id) {
            f.kind = RetailRendererWorldError::reentrant;
            f.operation = "0412 duplicate scene object id";
            return false;
        }
    }
    try {
        scene_objects_.push_back(std::move(obj));
        return true;
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 scene object allocation";
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 scene object insertion";
    }
    return false;
}

bool NativeRetailWorld::remove_scene_object(std::uint64_t id, RetailRendererWorldFailure& f) noexcept {
    f = {};
    const auto it = std::find_if(scene_objects_.begin(), scene_objects_.end(),
        [id](const RetailWorldSceneObject& o) { return o.id == id; });
    if (it == scene_objects_.end()) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 scene object id not found";
        return false;
    }
    scene_objects_.erase(it);
    return true;
}

void NativeRetailWorld::clear_scene_objects() noexcept {
    scene_objects_.clear();
}

struct NativeRetailWorldRecord final {
    std::shared_ptr<NativeRetailWorld> world;
    bool destroying{false};
    bool retired{false};

    NativeRetailWorldRecord() = default;
};

NativeRetailRendererWorldModule::NativeRetailRendererWorldModule(
    std::weak_ptr<NativeRetailGameSession> session,
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator)
    : session_(std::move(session)),
      allocator_(std::move(allocator)) {}

NativeRetailRendererWorldModule::~NativeRetailRendererWorldModule() {
    retire();
}

bool NativeRetailRendererWorldModule::access(RetailRendererWorldFailure& f) const noexcept {
    f = {};
    if (std::this_thread::get_id() != thread_) {
        f.kind = RetailRendererWorldError::wrong_thread;
        f.operation = "0412 thread affinity check";
        return false;
    }
    if (retired_) {
        f.kind = RetailRendererWorldError::retired;
        f.operation = "0412 world module retired";
        return false;
    }
    if (session_.expired()) {
        f.kind = RetailRendererWorldError::owner;
        f.operation = "0412 session lifetime expired";
        return false;
    }
    return true;
}

std::shared_ptr<NativeRetailWorldRecord> NativeRetailRendererWorldModule::validate(
    NativeRetailWorldHandle handle,
    RetailRendererWorldFailure& f) const noexcept {
    if (handle.empty()) {
        f.kind = RetailRendererWorldError::invalid_handle;
        f.operation = "0412 empty world handle";
        return {};
    }
    auto owner = handle.owner_.lock();
    if (!owner || owner.get() != this) {
        f.kind = RetailRendererWorldError::foreign_owner;
        f.operation = "0412 foreign world owner";
        return {};
    }
    auto rec = handle.record_.lock();
    if (!rec || rec->retired) {
        f.kind = RetailRendererWorldError::retired_world;
        f.operation = "0412 retired world handle";
        return {};
    }
    return rec;
}

std::shared_ptr<NativeRetailRendererWorldModule> NativeRetailRendererWorldModule::create(
    NativeRetailRendererCore& core,
    RetailRendererWorldFailure& f) noexcept {
    RetailRendererCoreFailure cf;
    if (!core.access(cf)) {
        f.kind = RetailRendererWorldError::owner;
        f.operation = "0412 core access refusal";
        return {};
    }
    auto allocator = core.allocator_;
    if (!allocator) {
        f.kind = RetailRendererWorldError::allocator;
        f.operation = "0412 allocator missing";
        return {};
    }
    try {
        return std::shared_ptr<NativeRetailRendererWorldModule>(
            new NativeRetailRendererWorldModule(core.session_, std::move(allocator)));
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 world module allocation";
        return {};
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 world module constructor";
        return {};
    }
}

std::optional<NativeRetailWorldHandle> NativeRetailRendererWorldModule::create_world(
    RetailRendererWorldFailure& f) noexcept {
    if (!access(f)) return {};
    if (busy_) {
        f.kind = RetailRendererWorldError::reentrant;
        f.operation = "0412 reentrant world creation";
        return {};
    }
    struct Guard {
        bool& b;
        Guard(bool& b) : b(b) { b = true; }
        ~Guard() { b = false; }
    } guard(busy_);

    auto session = session_.lock();
    if (!session) {
        f.kind = RetailRendererWorldError::owner;
        f.operation = "0412 session expired";
        return {};
    }

    auto rec = heap_object<NativeRetailWorldRecord>(
        allocator_, f, [&](void* ptr) {
            return ::new (ptr) NativeRetailWorldRecord();
        });
    if (!rec) return {};

    try {
        rec->world = std::shared_ptr<NativeRetailWorld>(
            new NativeRetailWorld(session));
    } catch (const std::bad_alloc&) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 native world object allocation";
        return {};
    } catch (...) {
        f.kind = RetailRendererWorldError::native_exception;
        f.operation = "0412 native world object constructor";
        return {};
    }

    NativeRetailWorldHandle handle(
        shared_from_this(),
        std::weak_ptr<NativeRetailWorldRecord>(rec));

    try {
        records_.push_back(rec);
    } catch (...) {
        f.kind = RetailRendererWorldError::allocation;
        f.operation = "0412 world record registration";
        return {};
    }

    return handle;
}

bool NativeRetailRendererWorldModule::destroy_world(
    NativeRetailWorldHandle handle,
    RetailRendererWorldFailure& f) noexcept {
    if (!access(f)) return false;
    auto rec = validate(handle, f);
    if (!rec) return false;

    if (rec->destroying) return true;
    rec->destroying = true;

    auto it = std::find(records_.begin(), records_.end(), rec);
    if (it != records_.end()) {
        records_.erase(it);
    }

    rec->world.reset();
    rec->retired = true;
    return true;
}

std::shared_ptr<NativeRetailWorld> NativeRetailRendererWorldModule::resolve_world(
    NativeRetailWorldHandle handle,
    RetailRendererWorldFailure& f) const noexcept {
    if (!access(f)) return {};
    auto rec = validate(handle, f);
    if (!rec) return {};
    return rec->world;
}

std::size_t NativeRetailRendererWorldModule::active_count() const noexcept {
    return records_.size();
}

void NativeRetailRendererWorldModule::retire() noexcept {
    if (retired_) return;
    retired_ = true;
    for (auto& r : records_) {
        if (r) {
            r->retired = true;
            r->world.reset();
        }
    }
    records_.clear();
}

std::shared_ptr<NativeRetailRendererWorldModule> NativeRetailRendererCore::world_module(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return world_module_;
}

std::optional<std::uint32_t> NativeRetailRendererCore::module_0412_count(
    RetailRendererCoreFailure& f) const noexcept {
    if (!access(f)) return {};
    return module_0412_count_.load(std::memory_order_relaxed);
}

namespace renderer_required {

bool construct_core_plugin_0412(NativeRetailRendererCore& core, RetailRendererCoreFailure& f) {
    if (!core.access(f)) return false;
    if (core.world_module_) {
        f.kind = RetailRendererCoreError::reentrant;
        f.operation = "0412 duplicate plugin registration";
        return false;
    }
    RetailRendererWorldFailure wf;
    auto owner = NativeRetailRendererWorldModule::create(core, wf);
    if (!owner) {
        f.kind = RetailRendererCoreError::construction;
        f.operation = wf.operation ? wf.operation : "0412 construction failure";
        return false;
    }
    core.world_module_ = std::move(owner);
    core.module_0412_count_.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void destroy_core_plugin_0412(NativeRetailRendererCore& core) noexcept {
    if (core.world_module_) {
        core.world_module_->retire();
        core.world_module_.reset();
        core.module_0412_count_.fetch_sub(1, std::memory_order_relaxed);
    }
}

} // namespace renderer_required

} // namespace mka::retail
