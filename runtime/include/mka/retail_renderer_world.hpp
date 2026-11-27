#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/retail_renderer_frames.hpp"
#include "mka/retail_renderer_materials.hpp"
#include "mka/retail_renderer_textures.hpp"
#include "mka/retail_renderer_rasters.hpp"
#include "mka/content_volume.hpp"
#include "mka/movie_gpu_completion.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include <thread>
#include <utility>

namespace mka::retail {

class NativeRetailRendererWorldModule;
struct NativeRetailWorldRecord;
struct NativeRetailWorldGpuResources;

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
    native_exception,
    gpu_unbound,
    gpu_context,
    gpu_operation,
    invalid_hud
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
    float u{0.0f};
    float v{0.0f};
    std::array<float, 4> color{1.0f, 1.0f, 1.0f, 1.0f};
    RetailRendererPoint3 normal{0.0f, 0.0f, 1.0f};
    std::array<std::uint16_t, 4> bone_indices{0, 0, 0, 0};
    std::array<float, 4> bone_weights{0.0f, 0.0f, 0.0f, 0.0f};
};

struct RetailWorldSceneObject {
    std::uint64_t id{0};
    NativeRetailRendererFrameHandle frame;
    RetailRendererPoint3 local_center{};
    float bounding_radius{1.0f};
    std::vector<RetailWorldTriangleVertex> triangles;
    NativeRetailMaterialHandle material;
    std::vector<NativeRetailRendererFrameHandle> bone_frames;
    std::vector<RetailRendererMatrixValue> inv_bind_matrices;
    std::vector<RetailRendererMatrixValue> bone_matrices;
    bool alpha_blended{false};
    bool visible{true};
};

enum class RetailWorldLightType : std::uint32_t {
    ambient = 1,
    directional = 2
};

struct RetailWorldLight {
    std::uint64_t id{0};
    RetailWorldLightType type{RetailWorldLightType::ambient};
    std::array<float, 3> color{1.0f, 1.0f, 1.0f};
    RetailRendererPoint3 direction{0.0f, 0.0f, -1.0f};
    bool enabled{true};
};

struct RetailWorldGpuTextureEntry {
    NativeRetailTextureHandle texture;
    std::uint32_t gl_texture_id{0};
    bool ready{false};
    NativeRetailRasterHandle raster;
    std::vector<std::byte> source_bytes;
    std::int32_t width{},height{};
    RetailRendererTextureFilter filter{RetailRendererTextureFilter::none};
    RetailRendererTextureAddress address_u{RetailRendererTextureAddress::wrap},address_v{RetailRendererTextureAddress::wrap};
};

// Explicit native drawing inputs, not recovered retail HUD geometry or channels.
// Positions use a top-left logical origin; UVs and straight-alpha RGBA are supplied.
struct RetailHudRect { float x{}, y{}, width{}, height{}; };
struct RetailHudUV { float u0{}, v0{}, u1{1}, v1{1}; };
struct RetailHudQuad {
    RetailHudRect rect;
    RetailHudUV uv;
    std::array<float, 4> color{1, 1, 1, 1};
    std::shared_ptr<const graphics::NativeGlTexture> texture; // Null explicitly means solid.
};
struct RetailHudSubmission {
    float logical_width{}, logical_height{};
    std::vector<RetailHudQuad> quads;
};

struct RetailWorldBspSector {
    std::uint64_t id{0};
    bool is_leaf{true};
    std::uint32_t split_type{0};
    float split_value{0.0f};
    std::int32_t left_child{-1};
    std::int32_t right_child{-1};
    RetailRendererPoint3 bbox_min{};
    RetailRendererPoint3 bbox_max{};
    RetailRendererPoint3 bsphere_center{};
    float bsphere_radius{0.0f};
    std::vector<RetailWorldTriangleVertex> triangles;
    NativeRetailMaterialHandle material;
};

class NativeRetailWorld final : public NativeRetailRenderWorld {
public:
    ~NativeRetailWorld() override;

    bool add_scene_object(RetailWorldSceneObject obj, RetailRendererWorldFailure& f) noexcept;
    bool remove_scene_object(std::uint64_t id, RetailRendererWorldFailure& f) noexcept;
    void clear_scene_objects() noexcept;
    const std::vector<RetailWorldSceneObject>& scene_objects() const noexcept { return scene_objects_; }
    std::optional<std::int32_t> add_bsp_sector(RetailWorldBspSector sector, RetailRendererWorldFailure& f) noexcept;
    void set_bsp_root(std::int32_t root_index) noexcept {
        if(std::this_thread::get_id()==thread_&&!retired_)bsp_root_sector_ = root_index;
    }
    std::int32_t bsp_root_sector() const noexcept { return bsp_root_sector_; }
    void clear_bsp_sectors() noexcept;
    const std::vector<RetailWorldBspSector>& bsp_sectors() const noexcept { return bsp_sectors_; }
    bool add_light(RetailWorldLight light, RetailRendererWorldFailure& f) noexcept;
    bool remove_light(std::uint64_t id, RetailRendererWorldFailure& f) noexcept;
    void clear_lights() noexcept;
    const std::vector<RetailWorldLight>& lights() const noexcept { return lights_; }
    const std::vector<RetailWorldGpuTextureEntry>& gpu_texture_cache() const noexcept;
    bool bind_gpu_resources(std::shared_ptr<NativeProductionDisplayService>,
        std::shared_ptr<video::NativeMovieGpuRetirementOwner>, RetailRendererWorldFailure&) noexcept;
    std::shared_ptr<NativeProductionDisplayService> gpu_display(RetailRendererWorldFailure&) const noexcept;
    bool admit_render_modules(NativeRetailRendererMaterials*, NativeRetailRendererTextures*,
        NativeRetailRendererRasters*, RetailRendererWorldFailure&) const noexcept;
    std::optional<std::uint32_t> resolve_gpu_texture(const NativeRetailTextureHandle&,
        NativeRetailRendererTextures&, NativeRetailRendererRasters&, RetailRendererWorldFailure&) noexcept;
    // Successful close retires the binding/task. Rendering again requires an explicit bind.
    // Refusal retains resources for cleanup on their original context; never forget names.
    bool release_gpu_resources(RetailRendererWorldFailure&) noexcept;
    void release_gpu_resources() noexcept;
    bool set_hud_submission(const RetailHudSubmission&, RetailRendererWorldFailure&) noexcept;
    void clear_hud_submission() noexcept;
    const std::optional<RetailHudSubmission>& hud_submission() const noexcept { return hud_; }

private:
    friend class NativeRetailRendererWorldModule;
    explicit NativeRetailWorld(const std::shared_ptr<NativeRetailGameSession>&,
        std::weak_ptr<NativeRetailRendererCore>) noexcept;
    bool access_world(RetailRendererWorldFailure&) const noexcept;
    void retire_world() noexcept;
    std::weak_ptr<NativeRetailGameSession> session_;
    std::weak_ptr<NativeRetailRendererCore> core_;
    const std::thread::id thread_{std::this_thread::get_id()};
    bool retired_{false};
    std::vector<RetailWorldSceneObject> scene_objects_;
    std::vector<RetailWorldBspSector> bsp_sectors_;
    std::int32_t bsp_root_sector_{-1};
    std::vector<RetailWorldLight> lights_;
    std::shared_ptr<NativeRetailWorldGpuResources> gpu_;
    video::MovieGpuRetirementRegistration gpu_registration_;
    std::optional<RetailHudSubmission> hud_;
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
        std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
        std::weak_ptr<NativeRetailRendererCore> core);

    bool access(RetailRendererWorldFailure& f) const noexcept;
    std::shared_ptr<NativeRetailWorldRecord> validate(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) const noexcept;

    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
    std::weak_ptr<NativeRetailRendererCore> core_;

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
    std::size_t opaque_objects{};
    std::size_t alpha_objects{};
    std::size_t total_sectors{};
    std::size_t visible_sectors{};
    std::size_t culled_sectors{};
    std::size_t submitted_triangles{};
    std::size_t bound_textures{};
    std::size_t submitted_hud_quads{};
};

struct RetailSceneStreamLoadResult {
    std::size_t bytes_read{};
    bool txd_ingested{false};
    bool world_ingested{false};
};

class RetailSceneResources;

std::optional<RetailSceneStreamLoadResult> load_retail_scene_streams_from_volume(
    const mka::content::NativeContentVolume& volume,
    std::string_view txd_resource_key,
    std::string_view world_resource_key,
    std::uint64_t base_sector_id,
    NativeRetailRendererRasters& rasters,
    NativeRetailRendererTextures& textures,
    NativeRetailRendererMaterials& materials,
    NativeRetailWorld& world,
    RetailRendererWorldFailure& failure,
    RetailSceneResources* ownership = nullptr) noexcept;

bool ingest_rw_tex_dictionary_stream(
    std::span<const std::byte> stream_bytes,
    NativeRetailRendererRasters& rasters,
    NativeRetailRendererTextures& textures,
    RetailRendererTextureFailure& failure,
    RetailSceneResources* ownership = nullptr) noexcept;

bool ingest_rw_world_bsp_stream(
    std::span<const std::byte> stream_bytes,
    std::uint64_t base_sector_id,
    NativeRetailRendererMaterials& materials,
    NativeRetailRendererTextures& textures,
    NativeRetailWorld& world,
    RetailRendererWorldFailure& failure,
    RetailSceneResources* ownership = nullptr) noexcept;

bool ingest_rw_geometry_stream(
    std::span<const std::byte> stream_bytes,
    std::uint64_t base_object_id,
    NativeRetailRendererFrames& frames,
    NativeRetailRendererMaterials& materials,
    NativeRetailRendererTextures& textures,
    NativeRetailWorld& world,
    const NativeRetailRendererFrameHandle& parent_frame,
    RetailRendererWorldFailure& failure,
    RetailSceneResources* ownership = nullptr) noexcept;

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
    RetailCameraProducerFailure& failure,
    NativeRetailRendererMaterials* materials = nullptr,
    NativeRetailRendererTextures* textures = nullptr,
    NativeRetailRendererRasters* rasters = nullptr) noexcept;

} // namespace mka::retail
