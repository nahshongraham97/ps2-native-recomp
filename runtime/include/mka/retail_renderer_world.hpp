#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/retail_renderer_frames.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mka::retail {

class NativeRetailRendererWorldModule;
struct NativeRetailWorldRecord;

enum class RetailRendererWorldError {
    none,
    owner,
    wrong_thread,
    retired,
    foreign_owner,
    invalid_handle,
    retired_world,
    reentrant,
    allocator,
    allocation,
    after_effect_unknown,
    native_exception
};

struct RetailRendererWorldFailure {
    RetailRendererWorldError kind{RetailRendererWorldError::none};
    const char* operation{""};
    std::uint32_t source_code{0};
    RetailRendererAllocatorFailure allocator{};
};

// Generational typed handle: provides fail-closed lifecycle tracking and prevents
// dangling references during rollback/unwind without raw pointer retention.
class NativeRetailWorldHandle final {
public:
    NativeRetailWorldHandle() noexcept = default;
    bool empty() const noexcept { return !assigned_; }
    bool same_identity(const NativeRetailWorldHandle& x) const noexcept {
        return assigned_ && x.assigned_ && !record_.owner_before(x.record_) && !x.record_.owner_before(record_);
    }

private:
    friend class NativeRetailRendererWorldModule;
    NativeRetailWorldHandle(
        std::weak_ptr<NativeRetailRendererWorldModule> o,
        std::weak_ptr<NativeRetailWorldRecord> r)
        : owner_(std::move(o)), record_(std::move(r)), assigned_(true) {}
    std::weak_ptr<NativeRetailRendererWorldModule> owner_;
    std::weak_ptr<NativeRetailWorldRecord> record_;
    bool assigned_{false};
};

// Plugin 0412 scope strictly recovered from Xbox default.xbe (0x002cd7d0/0x002cd820)
// and PS2 SLUS_214.10 (0x001a3df0/0x001a3e70):
// - FreeList-backed 4-byte container registration via engine plugin registration table (Xbox 0x002c0194, PS2 0x001a6818)
// - Plugin extension registration and freelist block allocation via shared helper 0x002c4620 (PS2 0x001a0fd0)
//   (plainly named: shared freelist block allocator and plugin extension registry helper used by 0409, 0412, etc.)
// - Global module count tracking: Xbox 0x005c761c, PS2 -23604($gp)
// - Real typed container ownership, thread affinity, and rollback
// Note: downstream bounding box, sector hierarchy, flags, names, and observation queries
// are not part of the 4-byte plugin registration contract and remain unrecovered.
struct RetailWorldTriangleVertex {
    RetailRendererPoint3 position;
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct RetailWorldSceneObject {
    std::uint64_t id{0};
    NativeRetailRendererFrameHandle frame;
    RetailRendererPoint3 local_center{};
    float bounding_radius{1.0f};
    std::vector<RetailWorldTriangleVertex> triangles;
    bool visible{true};
};

class NativeRetailWorld final : public NativeRetailRenderWorld {
public:
    ~NativeRetailWorld() override;

    bool add_scene_object(RetailWorldSceneObject obj, RetailRendererWorldFailure& f) noexcept;
    bool remove_scene_object(std::uint64_t id, RetailRendererWorldFailure& f) noexcept;
    void clear_scene_objects() noexcept;
    const std::vector<RetailWorldSceneObject>& scene_objects() const noexcept { return scene_objects_; }

private:
    friend class NativeRetailRendererWorldModule;
    explicit NativeRetailWorld(const std::shared_ptr<NativeRetailGameSession>& session) noexcept;
    std::vector<RetailWorldSceneObject> scene_objects_;
};

class NativeRetailRendererWorldModule final
    : public std::enable_shared_from_this<NativeRetailRendererWorldModule> {
public:
    ~NativeRetailRendererWorldModule();
    NativeRetailRendererWorldModule(const NativeRetailRendererWorldModule&) = delete;
    NativeRetailRendererWorldModule& operator=(const NativeRetailRendererWorldModule&) = delete;
    NativeRetailRendererWorldModule(NativeRetailRendererWorldModule&&) = delete;
    NativeRetailRendererWorldModule& operator=(NativeRetailRendererWorldModule&&) = delete;

    static std::shared_ptr<NativeRetailRendererWorldModule> create(
        NativeRetailRendererCore& core,
        RetailRendererWorldFailure& f) noexcept;

    std::optional<NativeRetailWorldHandle> create_world(
        RetailRendererWorldFailure& f) noexcept;

    bool destroy_world(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) noexcept;

    std::shared_ptr<NativeRetailWorld> resolve_world(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) const noexcept;

    std::size_t active_count() const noexcept;

    void retire() noexcept;

private:
    friend class NativeRetailRendererCore;
    NativeRetailRendererWorldModule(
        std::weak_ptr<NativeRetailGameSession> session,
        std::shared_ptr<NativeRetailRendererAllocatorService> allocator);

    bool access(RetailRendererWorldFailure& f) const noexcept;
    std::shared_ptr<NativeRetailWorldRecord> validate(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) const noexcept;

    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;

    std::vector<std::shared_ptr<NativeRetailWorldRecord>> records_;

    bool retired_{false};
    bool busy_{false};
    std::size_t destroy_cursor_{0};
    const std::thread::id thread_{std::this_thread::get_id()};
};

namespace renderer_required {
bool construct_core_plugin_0412(NativeRetailRendererCore&, RetailRendererCoreFailure&);
void destroy_core_plugin_0412(NativeRetailRendererCore&) noexcept;
}

struct RetailWorldRenderStats {
    std::size_t total_objects{};
    std::size_t visible_objects{};
    std::size_t culled_objects{};
    std::size_t submitted_triangles{};
};

bool sphere_in_frustum(
    const std::array<RetailRendererCameraPlane, 6>& planes,
    const RetailRendererPoint3& center,
    float radius) noexcept;

bool attach_camera_to_world_1cf300(
    NativeRetailRendererCameraModule& cameras,
    NativeRetailRendererFrames& frames,
    const NativeRetailRendererCameraHandle& camera,
    const std::shared_ptr<NativeRetailCameraWorldLink>& world_link,
    std::shared_ptr<NativeRetailRenderWorld> world,
    RetailCameraProducerFailure& failure) noexcept;

bool detach_camera_from_world_1cf350(
    const std::shared_ptr<NativeRetailCameraWorldLink>& world_link,
    const std::shared_ptr<NativeRetailRenderWorld>& world,
    RetailCameraWorldFailure& failure) noexcept;

std::optional<RetailWorldRenderStats> render_world_scene_frame(
    NativeRetailRendererCameraModule& cameras,
    NativeRetailRendererFrames& frames,
    const NativeRetailRendererCameraHandle& camera,
    const std::shared_ptr<NativeRetailCameraWorldLink>& world_link,
    const std::shared_ptr<NativeRetailWorld>& world,
    const std::array<float, 4>& clear_color,
    std::uint32_t clear_flags,
    bool call_show_raster,
    RetailCameraProducerFailure& failure) noexcept;

} // namespace mka::retail
