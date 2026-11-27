#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_rasters.hpp"
#include "mka/retail_renderer_frames.hpp"
#include "mka/retail_display_service.hpp"
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace mka::retail {
class NativeRetailRendererCameraModule;
struct NativeRetailRendererBaseCameraRecord;
enum class RetailRendererCameraError {
 none,owner,wrong_thread,retired,reentrant,invalid_handle,foreign_owner,retired_camera,
 allocator,allocation,native_exception,device_depth_required,unverified_numeric_domain,render_services_required,frame_binding,
 raster_binding,native_target
};
struct RetailRendererCameraFailure {
 RetailRendererCameraError kind{RetailRendererCameraError::none};const char* operation{""};
 RetailRendererAllocatorFailure allocator;
 RetailRendererFrameFailure frame;
 RetailRendererRasterFailure raster;
 DisplayServiceFailure display;
};
struct RetailRendererCameraDepthInputs {float near_value{},far_value{};};
struct RetailRendererCameraWindow {float x{},y{};};
struct RetailRendererCameraTargetPolicy {
 DisplayViewportRect physical_viewport;
 std::optional<float> clear_depth; // Explicit native projection policy, never inferred from source Z=0.
};
struct RetailRendererCameraMatrix4x4 {
 std::array<float,16> elements{}; // Column-major 4x4 matrix.
 bool operator==(const RetailRendererCameraMatrix4x4&)const=default;
};
struct RetailRendererCameraPlane {
 float a{},b{},c{},d{};
 bool operator==(const RetailRendererCameraPlane&)const=default;
};
class NativeRetailRendererCameraHandle final {
public:
 NativeRetailRendererCameraHandle()noexcept=default;
 bool empty()const noexcept{return !assigned_;}
 bool same_identity(const NativeRetailRendererCameraHandle& other)const noexcept{
  return assigned_&&other.assigned_&&!record_.owner_before(other.record_)&&!other.record_.owner_before(record_);
 }
private:
 friend class NativeRetailRendererCameraModule;
 NativeRetailRendererCameraHandle(std::weak_ptr<NativeRetailRendererCameraModule> owner,
     std::weak_ptr<NativeRetailRendererBaseCameraRecord> record):owner_(std::move(owner)),record_(std::move(record)),assigned_(true){}
 std::weak_ptr<NativeRetailRendererCameraModule> owner_;
 std::weak_ptr<NativeRetailRendererBaseCameraRecord> record_;
 bool assigned_{};
};
// Only source-initialized logical values. No400-byte source layout, invented
// identity view/frustum matrices or source callback addresses are represented.
struct RetailRendererBaseCameraObservation {
 RetailRendererCameraWindow view_window,inverse_window,view_offset;
 float near_clip{},far_clip{},fog_plane{},depth_scale{},depth_shift{};
 std::uint32_t projection_type{};
 RetailRendererCameraDepthInputs explicit_device_depth;
 bool synchronized_view_available{};
 RetailRendererCameraMatrix4x4 view_matrix;
 RetailRendererCameraMatrix4x4 projection_matrix;
 std::array<RetailRendererCameraPlane,6> frustum_planes{};
};
using RetailRendererCameraSyncCallback=bool(*)(
    const NativeRetailRendererCameraHandle&,
    const RetailRendererBaseCameraObservation&,
    void* user_data)noexcept;
class NativeRetailRendererCameraModule final:public std::enable_shared_from_this<NativeRetailRendererCameraModule> {
public:
 ~NativeRetailRendererCameraModule();
 NativeRetailRendererCameraModule(const NativeRetailRendererCameraModule&)=delete;
 NativeRetailRendererCameraModule& operator=(const NativeRetailRendererCameraModule&)=delete;
 // Explicit base/initial-empty-registry creation. Actual EngineOpen depth
 // inputs are mandatory; no BSS/default-device or runtime-flag fallback.
 std::optional<NativeRetailRendererCameraHandle> create_base_camera(
     const std::optional<RetailRendererCameraDepthInputs>&,RetailRendererCameraFailure&)noexcept;
 std::optional<RetailRendererBaseCameraObservation> observe(
     const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)const noexcept;
 std::optional<std::size_t> live_base_cameras(RetailRendererCameraFailure&)const noexcept;
 // Source1AA0B0 association and attached1A64A0 dirtying. These do not execute
 // camera/frame synchronization or supply missing renderer/plugin services.
 // set_frame's bool is native admission status; source1AA0B0 has a void ABI.
 bool set_frame(const NativeRetailRendererCameraHandle&,const NativeRetailRendererFrameHandle&,
     RetailRendererCameraFailure&)noexcept;
 std::optional<NativeRetailRendererFrameHandle> frame(
     const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)const noexcept;
 std::optional<NativeRetailRendererFrameObjectHandle> frame_object_identity(
     const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)const noexcept;
 bool set_view_window(const NativeRetailRendererCameraHandle&,RetailRendererCameraWindow,RetailRendererCameraFailure&)noexcept;
 bool set_near_clip(const NativeRetailRendererCameraHandle&,float near_clip,RetailRendererCameraFailure&)noexcept;
 bool set_far_clip(const NativeRetailRendererCameraHandle&,float far_clip,RetailRendererCameraFailure&)noexcept;
 bool set_projection(const NativeRetailRendererCameraHandle&,std::uint32_t projection_type,RetailRendererCameraFailure&)noexcept;
 bool set_fov(const NativeRetailRendererCameraHandle&,float fov_degrees,float aspect_ratio,RetailRendererCameraFailure&)noexcept;
 std::optional<std::uint64_t> register_sync_callback(
     RetailRendererCameraSyncCallback,void* user_data,RetailRendererCameraFailure&)noexcept;
 bool unregister_sync_callback(std::uint64_t token,RetailRendererCameraFailure&)noexcept;
 bool synchronize(const NativeRetailRendererCameraHandle&,NativeRetailRendererFrames&,RetailRendererCameraFailure&)noexcept;
 bool destroy_base_camera(const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)noexcept;
 // A capability refusal, not an implementation of source Begin/EndUpdate.
 bool require_render_ready(const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)const noexcept;

 // Typed raster fields; empty handles detach. Nonempty inputs must be live,
 // of the appropriate role and belong to the camera's actual Session.
 bool set_frame_buffer_raster(const NativeRetailRendererCameraHandle&, const NativeRetailRasterHandle&, RetailRendererCameraFailure&) noexcept;
 std::optional<NativeRetailRasterHandle> frame_buffer_raster(const NativeRetailRendererCameraHandle&, RetailRendererCameraFailure&) const noexcept;
 bool set_z_buffer_raster(const NativeRetailRendererCameraHandle&, const NativeRetailRasterHandle&, RetailRendererCameraFailure&) noexcept;
 std::optional<NativeRetailRasterHandle> z_buffer_raster(const NativeRetailRendererCameraHandle&, RetailRendererCameraFailure&) const noexcept;

 bool set_render_target_policy(const NativeRetailRendererCameraHandle&,
     RetailRendererCameraTargetPolicy,RetailRendererCameraFailure&)noexcept;
 // Native default-display draw boundary. Requires real same-display raster
 // backing and explicit physical viewport/depth policy. It does not implement
 // the still-unbound retail view/frustum synchronization or scene draw.
 bool begin_update(const NativeRetailRendererCameraHandle&, RetailRendererCameraFailure&) noexcept;
 bool end_update(const NativeRetailRendererCameraHandle&, RetailRendererCameraFailure&) noexcept;
 bool clear(const NativeRetailRendererCameraHandle&, float r, float g, float b, float a, std::uint32_t clear_flags, RetailRendererCameraFailure&) noexcept;
 bool is_updating(const NativeRetailRendererCameraHandle&, RetailRendererCameraFailure&) const noexcept;
private:
 friend bool renderer_required::construct_core_plugin_0405(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0405(NativeRetailRendererCore&)noexcept;
 NativeRetailRendererCameraModule(std::weak_ptr<NativeRetailGameSession>,
     std::shared_ptr<NativeRetailRendererAllocatorService>)noexcept;
 static std::shared_ptr<NativeRetailRendererCameraModule> create(
     std::weak_ptr<NativeRetailGameSession>,std::shared_ptr<NativeRetailRendererAllocatorService>,RetailRendererCameraFailure&)noexcept;
 std::shared_ptr<NativeRetailGameSession> admit(RetailRendererCameraFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererBaseCameraRecord> record(
     const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererRasters> raster_owner(const NativeRetailRasterHandle&,
     const std::shared_ptr<NativeRetailGameSession>&,bool depth,RetailRendererCameraFailure&)const noexcept;
 void retire()noexcept;
 std::weak_ptr<NativeRetailGameSession> session_;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 struct SyncCallbackEntry {
  std::uint64_t token{};
  RetailRendererCameraSyncCallback callback{};
  void* user_data{};
 };
 std::vector<std::shared_ptr<NativeRetailRendererBaseCameraRecord>> records_;
 std::vector<SyncCallbackEntry> sync_callbacks_;
 std::uint64_t next_callback_token_{1};
 std::atomic<bool> active_{true};
 bool busy_{};
 const std::thread::id thread_{std::this_thread::get_id()};
};
}
