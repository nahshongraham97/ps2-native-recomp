#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_rasters.hpp"
#include <atomic>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace mka::retail {
class NativeRetailRendererCameraModule;
struct NativeRetailRendererBaseCameraRecord;
enum class RetailRendererCameraError {
 none,owner,wrong_thread,retired,reentrant,invalid_handle,foreign_owner,retired_camera,
 allocator,allocation,native_exception,device_depth_required,unverified_numeric_domain,render_services_required
};
struct RetailRendererCameraFailure {
 RetailRendererCameraError kind{RetailRendererCameraError::none};const char* operation{""};
 RetailRendererAllocatorFailure allocator;
};
struct RetailRendererCameraDepthInputs {float near_value{},far_value{};};
struct RetailRendererCameraWindow {float x{},y{};};
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
};
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
 // Source view-window path with no attached frame. It is not frame syncing.
 bool set_view_window(const NativeRetailRendererCameraHandle&,RetailRendererCameraWindow,RetailRendererCameraFailure&)noexcept;
 bool destroy_base_camera(const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)noexcept;
 // A capability refusal, not an implementation of source Begin/EndUpdate.
 bool require_render_ready(const NativeRetailRendererCameraHandle&,RetailRendererCameraFailure&)const noexcept;

 // Native OpenGL FBO scaffolding: raster-to-camera attachment (recovered camera framebuffer/zbuffer pairing)
 bool set_frame_buffer_raster(const NativeRetailRendererCameraHandle&, const NativeRetailRasterHandle&, RetailRendererCameraFailure&) noexcept;
 std::optional<NativeRetailRasterHandle> frame_buffer_raster(const NativeRetailRendererCameraHandle&, RetailRendererCameraFailure&) const noexcept;
 bool set_z_buffer_raster(const NativeRetailRendererCameraHandle&, const NativeRetailRasterHandle&, RetailRendererCameraFailure&) noexcept;
 std::optional<NativeRetailRasterHandle> z_buffer_raster(const NativeRetailRendererCameraHandle&, RetailRendererCameraFailure&) const noexcept;

 // Native OpenGL FBO lifecycle: bind, clear, unbind (native scaffolding; no recovered retail draw call)
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
 void retire()noexcept;
 std::weak_ptr<NativeRetailGameSession> session_;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 std::vector<std::shared_ptr<NativeRetailRendererBaseCameraRecord>> records_;
 std::atomic<bool> active_{true};
 bool busy_{};
 const std::thread::id thread_{std::this_thread::get_id()};
};
}
