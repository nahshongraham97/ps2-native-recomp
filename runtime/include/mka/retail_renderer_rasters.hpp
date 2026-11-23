#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_images.hpp"
#include "mka/native_gl_texture.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace mka::retail {

class NativeRetailRendererRasters;
struct NativeRetailRasterRecord;

enum class RetailRendererRasterError {
 none,
 owner,
 wrong_thread,
 retired,
 foreign_owner,
 invalid_handle,
 retired_raster,
 reentrant,
 allocator,
 allocation,
 extent,
 already_backed,
 missing_backing,
 unsupported_format,
 unsupported_depth,
 context_underflow,
 backend_refusal,
 subview_invalid,
 lock_state,
 image_conversion,
 callback_required,
 callback_result,
 after_effect_unknown,
 native_exception
};

struct RetailRendererRasterFailure {
 RetailRendererRasterError kind{RetailRendererRasterError::none};
 const char* operation{""};
 std::uint32_t source_code{0};
 RetailRendererAllocatorFailure allocator{};
};

class NativeRetailRasterHandle final {
public:
 NativeRetailRasterHandle() noexcept = default;
 bool empty() const noexcept { return !assigned_; }
 bool same_identity(const NativeRetailRasterHandle& x) const noexcept {
  return assigned_ && x.assigned_ && !record_.owner_before(x.record_) && !x.record_.owner_before(record_);
 }
private:
 friend class NativeRetailRendererRasters;
 NativeRetailRasterHandle(std::weak_ptr<NativeRetailRendererRasters> o, std::weak_ptr<NativeRetailRasterRecord> r)
  : owner_(std::move(o)), record_(std::move(r)), assigned_(true) {}
 std::weak_ptr<NativeRetailRendererRasters> owner_;
 std::weak_ptr<NativeRetailRasterRecord> record_;
 bool assigned_{false};
};

struct RetailRendererRasterDescription {
 std::int32_t width{0};
 std::int32_t height{0};
 std::int32_t depth{0};
 std::uint32_t type{0};
 std::uint32_t flags{0};
 std::uint32_t format{0};
};

struct RetailRendererRasterObservation {
 RetailRendererRasterDescription description;
 std::uint32_t format{0};
 std::uint16_t offset_x{0};
 std::uint16_t offset_y{0};
 std::size_t stride{0};
 std::size_t pixel_bytes{0};
 std::size_t palette_bytes{0};
 std::uint32_t flags{0};
 std::uint32_t private_flags{0};
 bool is_subview{false};
 bool is_sentinel{false};
 bool has_gpu_backing{false};
 bool locked_pixels{false};
 bool locked_palette{false};
 bool destroying{false};
 bool extension_chain_completed{false};
};

class NativeRetailRasterExtensionState {
public:
 virtual ~NativeRetailRasterExtensionState() = default;
};

struct RetailRendererRasterExtension {
 std::uint32_t id{0};
 std::function<bool(NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&, RetailRendererRasterFailure&)> construct;
 std::function<void(NativeRetailRendererRasters&, NativeRetailRasterHandle, std::shared_ptr<NativeRetailRasterExtensionState>&)> destroy;
};

struct RetailRendererRasterBackendCallbacks {
 std::function<bool(NativeRetailRendererRasters&, NativeRetailRasterHandle, std::uint32_t flags, RetailRendererRasterFailure&)> create;
 std::function<void(NativeRetailRendererRasters&, NativeRetailRasterHandle)> destroy;
 std::function<bool(NativeRetailRendererRasters&, const std::optional<NativeRetailRasterHandle>&, RetailRendererRasterFailure&)> set_context;
 std::function<bool(NativeRetailRendererRasters&, NativeRetailRasterHandle subview, NativeRetailRasterHandle parent, RetailRendererRasterFailure&)> subraster;
 std::function<std::optional<std::int32_t>(NativeRetailRendererRasters&, NativeRetailRasterHandle, RetailRendererRasterFailure&)> query_mips;
};

class NativeRetailRendererRasters final : public std::enable_shared_from_this<NativeRetailRendererRasters> {
public:
 ~NativeRetailRendererRasters();
 NativeRetailRendererRasters(const NativeRetailRendererRasters&) = delete;
 NativeRetailRendererRasters& operator=(const NativeRetailRendererRasters&) = delete;

 std::optional<NativeRetailRasterHandle> create_raster(RetailRendererRasterDescription, RetailRendererRasterFailure&);
 bool destroy_raster(const NativeRetailRasterHandle&, RetailRendererRasterFailure&);
 std::optional<RetailRendererRasterObservation> observe(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) const noexcept;
 std::optional<std::size_t> live_rasters(RetailRendererRasterFailure&) const noexcept;

 // Context target stack
 std::optional<NativeRetailRasterHandle> current_target(RetailRendererRasterFailure&) const noexcept;
 bool push_context(const std::optional<NativeRetailRasterHandle>&, RetailRendererRasterFailure&);
 std::optional<NativeRetailRasterHandle> pop_context(RetailRendererRasterFailure&);
 std::optional<std::size_t> context_depth(RetailRendererRasterFailure&) const noexcept;

 // Subview creation
 std::optional<NativeRetailRasterHandle> create_subview(
  const NativeRetailRasterHandle& parent, std::int32_t x, std::int32_t y, std::int32_t width, std::int32_t height, RetailRendererRasterFailure&);

 // Mip count query
 std::optional<std::int32_t> mip_count(const NativeRetailRasterHandle&, RetailRendererRasterFailure&);

 // Lock / unlock
 std::optional<std::span<std::byte>> lock_pixels(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) noexcept;
 bool unlock_pixels(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) noexcept;
 std::optional<std::span<std::byte>> lock_palette(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) noexcept;
 bool unlock_palette(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) noexcept;

 // Transfer from 0406 image
 bool set_from_image(const NativeRetailRasterHandle& raster, NativeRetailRendererImages& images, const NativeRetailImageHandle& image, RetailRendererRasterFailure&);

 // GPU device binding
 bool attach_gpu_device(std::shared_ptr<graphics::NativeGlTextureUpload>, RetailRendererRasterFailure&) noexcept;
 std::shared_ptr<const graphics::NativeGlTexture> gpu_texture(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) const noexcept;
 bool set_sampler(const NativeRetailRasterHandle&, graphics::NativeTextureSampler, RetailRendererRasterFailure&) noexcept;
 std::optional<graphics::NativeTextureSampler> sampler(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) const noexcept;

 // Backend callbacks hook (for test fixtures / custom backend)
 void set_backend_callbacks(RetailRendererRasterBackendCallbacks) noexcept;

 // Default sentinel handle
 NativeRetailRasterHandle default_sentinel() const noexcept;

private:
 friend bool renderer_required::construct_core_plugin_0407(NativeRetailRendererCore&, RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0407(NativeRetailRendererCore&) noexcept;

 NativeRetailRendererRasters(std::weak_ptr<NativeRetailGameSession>, std::shared_ptr<NativeRetailRendererAllocatorService>,
  std::weak_ptr<NativeRetailRendererCore>, std::vector<std::shared_ptr<const RetailRendererRasterExtension>>);

 static std::shared_ptr<NativeRetailRendererRasters> create(NativeRetailRendererCore&, RetailRendererRasterFailure&) noexcept;
 std::shared_ptr<NativeRetailGameSession> admit(RetailRendererRasterFailure&) const noexcept;
 std::shared_ptr<NativeRetailRasterRecord> record(const NativeRetailRasterHandle&, RetailRendererRasterFailure&) const noexcept;
 bool reject(RetailRendererRasterFailure&, RetailRendererRasterError, const char*, std::uint32_t = 0) const noexcept;
 void retire() noexcept;

 std::weak_ptr<NativeRetailGameSession> session_;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 std::weak_ptr<NativeRetailRendererCore> core_;
 std::vector<std::shared_ptr<const RetailRendererRasterExtension>> extensions_;
 std::vector<std::shared_ptr<NativeRetailRasterRecord>> records_;
 std::shared_ptr<NativeRetailRasterRecord> sentinel_;
 std::vector<std::shared_ptr<NativeRetailRasterRecord>> context_stack_;
 std::shared_ptr<graphics::NativeGlTextureUpload> gpu_device_;
 RetailRendererRasterBackendCallbacks backend_callbacks_;
 bool active_{true};
 const std::thread::id thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
