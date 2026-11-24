#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_rasters.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mka::retail {

class NativeRetailRendererTextures;
struct NativeRetailTextureRecord;

enum class RetailRendererTextureError {
    none,
    owner,
    wrong_thread,
    retired,
    foreign_owner,
    invalid_handle,
    retired_texture,
    reentrant,
    allocator,
    allocation,
    raster_required,
    invalid_raster,
    raster_owner,
    duplicate_name,
    ref_count_underflow,
    after_effect_unknown,
    native_exception
};

struct RetailRendererTextureFailure {
    RetailRendererTextureError kind{RetailRendererTextureError::none};
    const char* operation{""};
    std::uint32_t source_code{0};
    RetailRendererAllocatorFailure allocator{};
};

enum class RetailRendererTextureFilter : std::uint32_t {
    none = 0,
    nearest = 1,
    linear = 2,
    mip_nearest = 3,
    mip_linear = 4,
    linear_mip_nearest = 5,
    linear_mip_linear = 6
};

enum class RetailRendererTextureAddress : std::uint32_t {
    wrap = 1,
    mirror = 2,
    clamp = 3,
    border = 4
};

class NativeRetailTextureHandle final {
public:
    NativeRetailTextureHandle() noexcept = default;
    bool empty() const noexcept { return !assigned_; }
    bool same_identity(const NativeRetailTextureHandle& x) const noexcept {
        return assigned_ && x.assigned_ && !record_.owner_before(x.record_) && !x.record_.owner_before(record_);
    }

private:
    friend class NativeRetailRendererTextures;
    NativeRetailTextureHandle(std::weak_ptr<NativeRetailRendererTextures> o, std::weak_ptr<NativeRetailTextureRecord> r)
        : owner_(std::move(o)), record_(std::move(r)), assigned_(true) {}
    std::weak_ptr<NativeRetailRendererTextures> owner_;
    std::weak_ptr<NativeRetailTextureRecord> record_;
    bool assigned_{false};
};

struct RetailRendererTextureDescription {
    std::string name;
    std::string mask;
    RetailRendererTextureFilter filter{RetailRendererTextureFilter::linear};
    RetailRendererTextureAddress address_u{RetailRendererTextureAddress::wrap};
    RetailRendererTextureAddress address_v{RetailRendererTextureAddress::wrap};
};

struct RetailRendererTextureObservation {
    std::string name;
    std::string mask;
    RetailRendererTextureFilter filter{RetailRendererTextureFilter::linear};
    RetailRendererTextureAddress address_u{RetailRendererTextureAddress::wrap};
    RetailRendererTextureAddress address_v{RetailRendererTextureAddress::wrap};
    std::int32_t ref_count{0};
    bool has_raster{false};
    bool is_default{false};
    bool destroying{false};
};

class NativeRetailRendererTextures final : public std::enable_shared_from_this<NativeRetailRendererTextures> {
public:
    ~NativeRetailRendererTextures();
    NativeRetailRendererTextures(const NativeRetailRendererTextures&) = delete;
    NativeRetailRendererTextures& operator=(const NativeRetailRendererTextures&) = delete;
    NativeRetailRendererTextures(NativeRetailRendererTextures&&) = delete;
    NativeRetailRendererTextures& operator=(NativeRetailRendererTextures&&) = delete;

    static std::shared_ptr<NativeRetailRendererTextures> create(
        NativeRetailRendererCore& core,
        RetailRendererTextureFailure& f) noexcept;

    std::optional<NativeRetailTextureHandle> create_texture(
        RetailRendererTextureDescription desc,
        NativeRetailRasterHandle raster,
        RetailRendererTextureFailure& f) noexcept;

    bool destroy_texture(
        NativeRetailTextureHandle handle,
        RetailRendererTextureFailure& f) noexcept;

    std::optional<std::int32_t> add_ref(
        NativeRetailTextureHandle handle,
        RetailRendererTextureFailure& f) noexcept;

    std::optional<NativeRetailRasterHandle> raster(
        NativeRetailTextureHandle handle,
        RetailRendererTextureFailure& f) const noexcept;

    bool set_raster(
        NativeRetailTextureHandle handle,
        NativeRetailRasterHandle raster,
        RetailRendererTextureFailure& f) noexcept;

    bool set_filter_addressing(
        NativeRetailTextureHandle handle,
        RetailRendererTextureFilter filter,
        RetailRendererTextureAddress address_u,
        RetailRendererTextureAddress address_v,
        RetailRendererTextureFailure& f) noexcept;

    std::optional<RetailRendererTextureObservation> observe(
        NativeRetailTextureHandle handle,
        RetailRendererTextureFailure& f) const noexcept;

    std::optional<NativeRetailTextureHandle> find_texture(
        std::string_view name,
        RetailRendererTextureFailure& f) const noexcept;

    NativeRetailTextureHandle default_texture() const noexcept;

    std::size_t active_count() const noexcept;

    void retire() noexcept;

private:
    friend class NativeRetailRendererCore;
    NativeRetailRendererTextures(
        std::weak_ptr<NativeRetailGameSession> session,
        std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
        std::shared_ptr<NativeRetailRendererRasters> rasters);

    bool access(RetailRendererTextureFailure& f) const noexcept;
    std::shared_ptr<NativeRetailTextureRecord> validate(
        NativeRetailTextureHandle handle,
        RetailRendererTextureFailure& f) const noexcept;

    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
    std::shared_ptr<NativeRetailRendererRasters> rasters_;

    std::vector<std::shared_ptr<NativeRetailTextureRecord>> records_;
    std::shared_ptr<NativeRetailTextureRecord> default_record_;
    NativeRetailTextureHandle default_handle_;

    bool retired_{false};
    bool busy_{false};
    std::size_t destroy_cursor_{0};
    const std::thread::id thread_{std::this_thread::get_id()};
};

namespace renderer_required {
bool construct_core_plugin_0408(NativeRetailRendererCore&, RetailRendererCoreFailure&);
void destroy_core_plugin_0408(NativeRetailRendererCore&) noexcept;
}

} // namespace mka::retail
