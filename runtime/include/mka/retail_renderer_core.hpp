#pragma once
#include "mka/retail_renderer_allocator.hpp"
#include "mka/content_volume.hpp"
#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>
namespace mka::retail {
class NativeRetailGameSession;
class NativeRetailRendererCore;
class NativeRetailRendererErrorDomain;
class NativeRetailRendererTransforms;
class NativeRetailRendererMatrixDomain;
class NativeRetailRendererFrames;
class NativeRetailRendererStreams;
class NativeRetailRendererCameraModule;
class NativeRetailRendererImages;
struct RetailRendererImageExtension;
class NativeRetailRendererRasters;
struct RetailRendererRasterExtension;
enum class RetailRendererCoreError {none,owner,wrong_thread,allocation,native_exception,allocator,content,unbound_factory,registration_closed,reentrant,construction,after_effect_unknown};
struct RetailRendererCoreFailure {
 RetailRendererCoreError kind{RetailRendererCoreError::none};const char* operation{""};
 RetailRendererAllocatorFailure allocator;content::ContentVolumeFailure content,cleanup_content;
};
struct RetailRendererErrorRecord {std::uint32_t plugin{},code{};bool operator==(const RetailRendererErrorRecord&)const=default;};
struct RetailRendererPluginSpec {std::uint32_t id{},source_payload_bytes{};bool operator==(const RetailRendererPluginSpec&)const=default;};
// Selected Xbox2C84D0 registers metadata only. The source's24-byte device
// extension and registry offset are NOT a native object layout or GPU owner.
struct RetailRendererRasterExtensionSpec {
 std::uint32_t id{};
 bool construct_callback{},destroy_callback{},copy_callback{};
 bool operator==(const RetailRendererRasterExtensionSpec&)const=default;
};
class NativeRetailRendererRasterExtensionRegistration final {
public:
 NativeRetailRendererRasterExtensionRegistration(const NativeRetailRendererRasterExtensionRegistration&)=delete;
 NativeRetailRendererRasterExtensionRegistration& operator=(const NativeRetailRendererRasterExtensionRegistration&)=delete;
 std::optional<RetailRendererRasterExtensionSpec> observe(RetailRendererCoreFailure&)const noexcept;
private:
 friend class NativeRetailRendererCore;
 explicit NativeRetailRendererRasterExtensionRegistration(std::weak_ptr<NativeRetailGameSession> session):session_(std::move(session)){}
 std::weak_ptr<NativeRetailGameSession> session_;
 std::atomic<bool> active_{true};
 const std::thread::id thread_{std::this_thread::get_id()};
};
// Native code references, never source addresses or a numeric dispatch table.
// Constructors own typed native data in the actual Core; no guest extensions.
struct RetailRendererPluginCallbacks {
 bool(*construct)(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 void(*destroy)(NativeRetailRendererCore&) noexcept;
};
namespace renderer_required {
bool construct_core_plugin_0401(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0401(NativeRetailRendererCore&)noexcept;
bool construct_core_plugin_0402(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0402(NativeRetailRendererCore&)noexcept;
bool construct_core_plugin_0403(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0403(NativeRetailRendererCore&)noexcept;
bool construct_core_plugin_0404(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0404(NativeRetailRendererCore&)noexcept;
bool construct_core_plugin_0405(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0405(NativeRetailRendererCore&)noexcept;
bool construct_core_plugin_0406(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0406(NativeRetailRendererCore&)noexcept;
bool construct_core_plugin_0407(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0407(NativeRetailRendererCore&)noexcept;
bool construct_core_plugin_040d(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_040d(NativeRetailRendererCore&)noexcept;
std::optional<bool> register_object_extensions_1b72b0(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void discard_object_extensions_after_core_init(NativeRetailRendererCore&)noexcept;
}
class NativeRetailRendererPluginRegistration final {
public:
 RetailRendererPluginSpec spec()const noexcept{return spec_;}
private:
 friend class NativeRetailRendererCore;
 NativeRetailRendererPluginRegistration(RetailRendererPluginSpec s,RetailRendererPluginCallbacks c):spec_(s),callbacks_(c){}
 const RetailRendererPluginSpec spec_;const RetailRendererPluginCallbacks callbacks_;
};
class NativeRetailRendererErrorDomain final {
public:
 ~NativeRetailRendererErrorDomain();
 NativeRetailRendererErrorDomain(const NativeRetailRendererErrorDomain&)=delete;
 NativeRetailRendererErrorDomain& operator=(const NativeRetailRendererErrorDomain&)=delete;
 bool store(RetailRendererErrorRecord,RetailRendererCoreFailure&) noexcept;
 std::optional<RetailRendererErrorRecord> take(RetailRendererCoreFailure&) noexcept;
private:
 friend class NativeRetailRendererCore;
 struct State;
 explicit NativeRetailRendererErrorDomain(std::shared_ptr<State> s):state_(std::move(s)){}
 std::shared_ptr<State> state_;
 bool counted_{};
};
class NativeRetailRendererCore final {
public:
 ~NativeRetailRendererCore();
 NativeRetailRendererCore(const NativeRetailRendererCore&)=delete;
 NativeRetailRendererCore& operator=(const NativeRetailRendererCore&)=delete;
 NativeRetailRendererCore(NativeRetailRendererCore&&)=delete;
 NativeRetailRendererCore& operator=(NativeRetailRendererCore&&)=delete;
 // Persistent actual Session owner; weak Session identity prevents a cycle.
 static std::shared_ptr<NativeRetailRendererCore> for_session(
  std::shared_ptr<NativeRetailGameSession>,std::shared_ptr<NativeRetailRendererAllocatorService>,
  const content::NativeContentVolume&,RetailRendererCoreFailure&) noexcept;
 std::optional<std::shared_ptr<const NativeRetailRendererPluginRegistration>> register_factory(
  RetailRendererPluginSpec,RetailRendererPluginCallbacks,RetailRendererCoreFailure&) noexcept;
 // Complete source1A6DD0 core-init phase only; no platformopen/start/GPU success.
 std::optional<bool> initialize_core(RetailRendererCoreFailure&);
 std::optional<std::vector<RetailRendererPluginSpec>> registered(RetailRendererCoreFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererErrorDomain> construct_error_domain(RetailRendererCoreFailure&) noexcept;
 std::optional<std::uint32_t> error_domain_count(RetailRendererCoreFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererMatrixDomain> matrix_domain(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> matrix_domain_count(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> module_040d_count(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> module_0401_count(RetailRendererCoreFailure&)const noexcept;
  std::shared_ptr<NativeRetailRendererTransforms> transforms(RetailRendererCoreFailure&)const noexcept;
  std::shared_ptr<const NativeRetailRendererRasterExtensionRegistration> raster_extension(
      RetailRendererCoreFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererFrames> frames(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> module_0403_count(RetailRendererCoreFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererStreams> streams(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> module_0404_count(RetailRendererCoreFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererCameraModule> camera_module(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> module_0405_count(RetailRendererCoreFailure&)const noexcept;
 std::shared_ptr<NativeRetailRendererImages> images(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> module_0406_count(RetailRendererCoreFailure&)const noexcept;
 bool register_image_extension(std::shared_ptr<const RetailRendererImageExtension>,RetailRendererCoreFailure&)noexcept;
 std::shared_ptr<NativeRetailRendererRasters> rasters(RetailRendererCoreFailure&)const noexcept;
 std::optional<std::uint32_t> module_0407_count(RetailRendererCoreFailure&)const noexcept;
 bool register_raster_extension(std::shared_ptr<const RetailRendererRasterExtension>,RetailRendererCoreFailure&)noexcept;
 //1A3480 forward construction/reverse prior-success unwind. Explicitly later
 // than CoreInit; production EngineOpen/Start producers remain required.
 bool construct_registered(RetailRendererCoreFailure&);
 bool destroy_constructed(RetailRendererCoreFailure&) noexcept;
 std::optional<content::NativeContentFile> open_resource(std::string_view,RetailRendererCoreFailure&)const;
 memory::NativeHeapIdentity allocator_identity()const noexcept{return allocator_->owner_identity();}
private:
 NativeRetailRendererCore(std::weak_ptr<NativeRetailGameSession>,std::shared_ptr<NativeRetailRendererAllocatorService>,content::NativeContentVolume&&);
 bool access(RetailRendererCoreFailure&)const noexcept;
 void store_error(RetailRendererErrorRecord)noexcept;
 bool register_core_plugins(RetailRendererCoreFailure&)noexcept;
 void discard_object_extensions()noexcept;
 bool construct_transforms(RetailRendererCoreFailure&)noexcept;
  void destroy_transforms()noexcept;
  std::optional<bool> register_native_raster_extension(RetailRendererCoreFailure&)noexcept;
  void discard_native_raster_extension()noexcept;
 std::weak_ptr<NativeRetailGameSession> session_;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 content::NativeContentVolume content_;
 std::shared_ptr<NativeRetailRendererErrorDomain::State> errors_;
 std::shared_ptr<NativeRetailRendererErrorDomain> error_domain_;
 std::shared_ptr<NativeRetailRendererTransforms> transforms_;
 std::vector<std::shared_ptr<NativeRetailRendererPluginRegistration>> entries_;
 std::vector<std::shared_ptr<NativeRetailRendererPluginRegistration>> constructed_;
  std::shared_ptr<NativeRetailRendererMatrixDomain> matrix_domain_;
  std::shared_ptr<NativeRetailRendererRasterExtensionRegistration> raster_extension_;
 std::shared_ptr<NativeRetailRendererFrames> frames_;
 std::atomic<std::uint32_t> module_0403_count_{};
 std::shared_ptr<NativeRetailRendererStreams> streams_;
 std::atomic<std::uint32_t> module_0404_count_{};
 std::shared_ptr<NativeRetailRendererCameraModule> camera_module_;
 std::atomic<std::uint32_t> module_0405_count_{};
 std::shared_ptr<NativeRetailRendererImages> images_;
 std::vector<std::shared_ptr<const RetailRendererImageExtension>> image_extensions_;
 std::shared_ptr<NativeRetailRendererRasters> rasters_;
 std::vector<std::shared_ptr<const RetailRendererRasterExtension>> raster_extensions_;
 std::atomic<std::uint32_t> module_0407_count_{};
 content::ContentVolumeFailure cleanup_content_failure_;
 std::atomic<std::uint32_t> matrix_domain_count_{};
 std::atomic<std::uint32_t> module_040d_count_{};
 std::atomic<std::uint32_t> module_0401_count_{};
 bool object_extensions_prepared_{},initialized_{},initializing_{},busy_{},sealed_{},after_effect_unknown_{};
 const std::thread::id thread_{std::this_thread::get_id()};
 friend bool renderer_required::construct_core_plugin_0402(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0402(NativeRetailRendererCore&)noexcept;
 friend bool renderer_required::construct_core_plugin_040d(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend bool renderer_required::construct_core_plugin_0401(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0401(NativeRetailRendererCore&)noexcept;
 friend void renderer_required::destroy_core_plugin_040d(NativeRetailRendererCore&)noexcept;
 friend bool construct_native_renderer_error(NativeRetailRendererCore&,RetailRendererCoreFailure&);
  friend void destroy_native_renderer_error(NativeRetailRendererCore&)noexcept;
  friend std::optional<bool> renderer_required::register_object_extensions_1b72b0(NativeRetailRendererCore&,RetailRendererCoreFailure&);
  friend void renderer_required::discard_object_extensions_after_core_init(NativeRetailRendererCore&)noexcept;
 friend bool renderer_required::construct_core_plugin_0403(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0403(NativeRetailRendererCore&)noexcept;
 friend bool renderer_required::construct_core_plugin_0404(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0404(NativeRetailRendererCore&)noexcept;
 friend bool renderer_required::construct_core_plugin_0405(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0405(NativeRetailRendererCore&)noexcept;
 friend class NativeRetailRendererImages;
 friend bool renderer_required::construct_core_plugin_0406(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0406(NativeRetailRendererCore&)noexcept;
 friend class NativeRetailRendererRasters;
 friend bool renderer_required::construct_core_plugin_0407(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0407(NativeRetailRendererCore&)noexcept;
};
bool construct_native_renderer_error(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_native_renderer_error(NativeRetailRendererCore&)noexcept;
namespace renderer_required {
bool construct_core_plugin_0401(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0401(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_040d(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_040d(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0402(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0402(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0403(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0403(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0404(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0404(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0405(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0405(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0406(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0406(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0407(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0407(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0408(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0408(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0409(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0409(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_0412(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_0412(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_040a(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_040a(NativeRetailRendererCore&) noexcept;
bool construct_core_plugin_040b(NativeRetailRendererCore&,RetailRendererCoreFailure&);
void destroy_core_plugin_040b(NativeRetailRendererCore&) noexcept;
// Historical native integration name; selected Xbox2C84D0 now registers the
// real native raster-extension definition. PS2 device/stream layouts are not
// adopted. No raster construction, device open/start or GPU effect is implied.
std::optional<bool> register_object_extensions_1b72b0(NativeRetailRendererCore&,RetailRendererCoreFailure&);
// Retire actual typed metadata on partial CoreInit failure or Core retirement.
void discard_object_extensions_after_core_init(NativeRetailRendererCore&)noexcept;
}
}
