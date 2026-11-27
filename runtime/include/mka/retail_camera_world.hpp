#pragma once
#include "mka/retail_renderer_cameras.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>
namespace mka::retail {
class NativeRetailGameSession;
enum class RetailCameraWorldError { none, session, wrong_thread, foreign_owner, missing_producer, missing_record, expired_camera, expired_world, allocation, native_exception };
struct RetailCameraWorldFailure { RetailCameraWorldError kind{};const char* operation{""}; };
// The real native1CD6E0 producer supplies a concrete world. No world, actor,
// frame, raster or renderer implementation is manufactured by this interface.
class NativeRetailRenderWorld {
public:
 virtual ~NativeRetailRenderWorld()=0;
 const NativeRetailGameSession* owner_session() const noexcept {const auto owner=session_.lock();return owner.get();}
protected:
 explicit NativeRetailRenderWorld(const std::shared_ptr<NativeRetailGameSession>& session) noexcept:session_(session){}
private:
 std::weak_ptr<NativeRetailGameSession> session_;
};
inline NativeRetailRenderWorld::~NativeRetailRenderWorld()=default;
// Only the proved camera world-plugin fields. This is not a camera factory.
// The actual17EBB0/1A65A0 producer retains this subowner with its camera.
class NativeRetailCameraWorldLink final {
public:
 static std::shared_ptr<NativeRetailCameraWorldLink> create_initial(
     const std::shared_ptr<NativeRetailGameSession>&,RetailCameraWorldFailure&) noexcept;
 // Final1CF300 store ONLY: actual producer must first perform the required
 // nonnull-frame1A7510 update. This method does not implement that call.
 bool attach_after_frame_update_from_1cf300(std::shared_ptr<NativeRetailRenderWorld>,RetailCameraWorldFailure&) noexcept;
 std::optional<std::shared_ptr<NativeRetailRenderWorld>> world_from_1cf380(RetailCameraWorldFailure&) const noexcept;
 std::optional<std::shared_ptr<NativeRetailRenderWorld>> remove_from_1cf350(
     const std::shared_ptr<NativeRetailRenderWorld>& argument,RetailCameraWorldFailure&) noexcept;
 std::uint32_t observed_update_word() const noexcept {return update_word_;}
private:
 friend class NativeRetailRenderCameras;
 explicit NativeRetailCameraWorldLink(const std::shared_ptr<NativeRetailGameSession>&) noexcept;
 bool access(RetailCameraWorldFailure&) const noexcept;
 std::weak_ptr<NativeRetailGameSession> session_;
 const NativeRetailGameSession* identity_;
 std::weak_ptr<NativeRetailRenderWorld> world_;
 bool world_published_{};
 std::uint32_t update_word_{}; //1CDFFC zero birth;1CF36C clears on detach.
};
struct RetailRenderCameraObservation {std::int32_t count{};bool count_producer_admitted{},render_suspended{},has_world{};std::size_t retained_records{};};
// One persistent Session domain for64DEFC,64E0FC,64DED4 and69EAE0
// camera-record projections. Historical addresses are provenance only.
class NativeRetailRenderCameras final {
public:
 NativeRetailRenderCameras(const NativeRetailRenderCameras&)=delete;
 NativeRetailRenderCameras& operator=(const NativeRetailRenderCameras&)=delete;
 NativeRetailRenderCameras(NativeRetailRenderCameras&&)=delete;
 NativeRetailRenderCameras& operator=(NativeRetailRenderCameras&&)=delete;
 //27270 source count store in220900 JAL delay slot; admits actual count
 // producer, not successful renderer/first-scene initialization.
 bool store_initial_count_from_127260(RetailCameraWorldFailure&) noexcept;
 bool store_count_from_127110(std::int32_t,RetailCameraWorldFailure&) noexcept;
 // Real180ED0 producer calls only after constructing its actual camera,
 // frame/raster/task state. Null slots are born source BSS, not dummy objects.
 bool publish_camera_link_after_180ed0(std::size_t,std::shared_ptr<NativeRetailCameraWorldLink>,RetailCameraWorldFailure&) noexcept;
 // Actual186404 producer publishes the world returned by1CD6E0.
 bool publish_world_after_186404(std::shared_ptr<NativeRetailRenderWorld>,RetailCameraWorldFailure&) noexcept;
 bool detach_cameras_from_181a30(RetailCameraWorldFailure&) noexcept;
 std::optional<RetailRenderCameraObservation> observe(RetailCameraWorldFailure&) const noexcept;
private:
 friend class NativeRetailGameSession;
 explicit NativeRetailRenderCameras(NativeRetailGameSession& session) noexcept:session_(&session){}
 bool access(RetailCameraWorldFailure&) const noexcept;
 NativeRetailGameSession* session_; // borrowed from enclosing Session
 struct Record {std::weak_ptr<NativeRetailCameraWorldLink> camera;bool published{};};
 std::vector<Record> cameras_;
 std::shared_ptr<NativeRetailRenderWorld> world_;
 std::int32_t count_{};
 bool admitted_{},suspended_{};
};

struct RetailCameraProducerFailure {
 const char* operation{""};
 RetailCameraWorldFailure world;
 RetailRendererCameraFailure camera;
 RetailRendererFrameFailure frame;
 RetailRendererRasterFailure raster;
};

struct RetailCameraProducer17EBB0Config {
 std::int32_t width{};
 std::int32_t height{};
 bool has_color{false};
 bool has_depth{false};
 std::uint32_t color_flags{0};
 std::uint32_t depth_flags{0};
 RetailRendererCameraDepthInputs depth_inputs{};
 std::shared_ptr<NativeProductionDisplayService> display;
 std::optional<RetailRendererCameraTargetPolicy> target_policy;
};

struct RetailCameraProducer17EBB0Result {
 NativeRetailRendererCameraHandle camera;
 NativeRetailRendererFrameHandle frame;
 NativeRetailRasterHandle color_raster;
 NativeRetailRasterHandle depth_raster;
 std::shared_ptr<NativeRetailCameraWorldLink> world_link;
};

struct RetailCameraInit180ED0Config {
 std::int32_t width{};
 std::int32_t height{};
 float fov_degrees{60.0f};
 float near_clip{0.25f};
 float far_clip{125.0f};
 bool create_scene_camera{false};
 bool primary_has_color{false};
 bool primary_has_depth{false};
 bool scene_has_color{true};
 bool scene_has_depth{true};
 RetailRendererCameraDepthInputs depth_inputs{};
 std::shared_ptr<NativeProductionDisplayService> display;
 std::optional<RetailRendererCameraTargetPolicy> target_policy;
};

struct RetailCameraInit180ED0Result {
 RetailCameraProducer17EBB0Result primary;
 std::optional<RetailCameraProducer17EBB0Result> scene;
};

// Native 0x0017EBB0 camera producer: allocates camera, frame, requested
// depth/color rasters, display/target bindings, and world link with full
// rollback if any sub-object creation or binding fails.
std::optional<RetailCameraProducer17EBB0Result> create_camera_producer_17ebb0(
 const std::shared_ptr<NativeRetailGameSession>& session,
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererRasters& rasters,
 const RetailCameraProducer17EBB0Config& config,
 RetailCameraProducerFailure& failure) noexcept;

void destroy_camera_producer_17ebb0(
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererRasters& rasters,
 RetailCameraProducer17EBB0Result& produced) noexcept;

// Native 0x00180ED0 camera initializer: builds primary (and optional scene)
// camera via 17EBB0, applies near/far/FOV/projection, and publishes links.
std::optional<RetailCameraInit180ED0Result> initialize_cameras_180ed0(
 const std::shared_ptr<NativeRetailGameSession>& session,
 NativeRetailRendererCameraModule& cameras,
 NativeRetailRendererFrames& frames,
 NativeRetailRendererRasters& rasters,
 NativeRetailRenderCameras& world_cameras,
 const RetailCameraInit180ED0Config& config,
 RetailCameraProducerFailure& failure) noexcept;
}
