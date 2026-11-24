#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_textures.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mka::retail {

class NativeRetailRendererMaterials;
struct NativeRetailMaterialRecord;

enum class RetailRendererMaterialError {
    none,
    owner,
    wrong_thread,
    retired,
    foreign_owner,
    invalid_handle,
    retired_material,
    reentrant,
    allocator,
    allocation,
    invalid_texture,
    ref_count_underflow,
    after_effect_unknown,
    native_exception
};

struct RetailRendererMaterialFailure {
    RetailRendererMaterialError kind{RetailRendererMaterialError::none};
    const char* operation{""};
    std::uint32_t source_code{0};
    RetailRendererAllocatorFailure allocator{};
};

struct RetailRendererRgbaColor {
    std::uint8_t r{255};
    std::uint8_t g{255};
    std::uint8_t b{255};
    std::uint8_t a{255};

    constexpr bool operator==(const RetailRendererRgbaColor& other) const noexcept {
        return r == other.r && g == other.g && b == other.b && a == other.a;
    }
    constexpr bool operator!=(const RetailRendererRgbaColor& other) const noexcept {
        return !(*this == other);
    }
};

struct RetailRendererSurfaceProperties {
    float ambient{1.0f};
    float specular{1.0f};
    float diffuse{1.0f};

    constexpr bool operator==(const RetailRendererSurfaceProperties& other) const noexcept {
        return ambient == other.ambient && specular == other.specular && diffuse == other.diffuse;
    }
    constexpr bool operator!=(const RetailRendererSurfaceProperties& other) const noexcept {
        return !(*this == other);
    }
};

// Generational typed handle: provides fail-closed lifecycle tracking and prevents
// dangling references during rollback/unwind without raw pointer retention.
class NativeRetailMaterialHandle final {
public:
    NativeRetailMaterialHandle() noexcept = default;
    bool empty() const noexcept { return !assigned_; }
    bool same_identity(const NativeRetailMaterialHandle& x) const noexcept {
        return assigned_ && x.assigned_ && !record_.owner_before(x.record_) && !x.record_.owner_before(record_);
    }

private:
    friend class NativeRetailRendererMaterials;
    NativeRetailMaterialHandle(
        std::weak_ptr<NativeRetailRendererMaterials> o,
        std::weak_ptr<NativeRetailMaterialRecord> r)
        : owner_(std::move(o)), record_(std::move(r)), assigned_(true) {}
    std::weak_ptr<NativeRetailRendererMaterials> owner_;
    std::weak_ptr<NativeRetailMaterialRecord> record_;
    bool assigned_{false};
};

struct RetailRendererMaterialDescription {
    RetailRendererRgbaColor color{255, 255, 255, 255};
    RetailRendererSurfaceProperties surface{};
    NativeRetailTextureHandle texture{};
};

struct RetailRendererMaterialObservation {
    RetailRendererRgbaColor color{255, 255, 255, 255};
    RetailRendererSurfaceProperties surface{};
    std::int32_t ref_count{1};
    bool has_texture{false};
    bool destroying{false};
};

class NativeRetailRendererMaterials final
    : public std::enable_shared_from_this<NativeRetailRendererMaterials> {
public:
    ~NativeRetailRendererMaterials();
    NativeRetailRendererMaterials(const NativeRetailRendererMaterials&) = delete;
    NativeRetailRendererMaterials& operator=(const NativeRetailRendererMaterials&) = delete;
    NativeRetailRendererMaterials(NativeRetailRendererMaterials&&) = delete;
    NativeRetailRendererMaterials& operator=(NativeRetailRendererMaterials&&) = delete;

    static std::shared_ptr<NativeRetailRendererMaterials> create(
        NativeRetailRendererCore& core,
        RetailRendererMaterialFailure& f) noexcept;

    std::optional<NativeRetailMaterialHandle> create_material(
        RetailRendererMaterialFailure& f) noexcept;

    std::optional<NativeRetailMaterialHandle> create_material(
        RetailRendererMaterialDescription desc,
        RetailRendererMaterialFailure& f) noexcept;

    bool destroy_material(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) noexcept;

    std::optional<std::int32_t> add_ref(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) noexcept;

    bool release(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) noexcept;

    bool set_color(
        NativeRetailMaterialHandle handle,
        RetailRendererRgbaColor color,
        RetailRendererMaterialFailure& f) noexcept;

    std::optional<RetailRendererRgbaColor> get_color(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) const noexcept;

    bool set_surface_properties(
        NativeRetailMaterialHandle handle,
        RetailRendererSurfaceProperties surface,
        RetailRendererMaterialFailure& f) noexcept;

    std::optional<RetailRendererSurfaceProperties> get_surface_properties(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) const noexcept;

    bool set_texture(
        NativeRetailMaterialHandle handle,
        NativeRetailTextureHandle texture,
        RetailRendererMaterialFailure& f) noexcept;

    std::optional<NativeRetailTextureHandle> get_texture(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) const noexcept;

    std::optional<RetailRendererMaterialObservation> observe(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) const noexcept;

    std::size_t active_count() const noexcept;

    void retire() noexcept;

private:
    friend class NativeRetailRendererCore;
    NativeRetailRendererMaterials(
        std::weak_ptr<NativeRetailGameSession> session,
        std::shared_ptr<NativeRetailRendererAllocatorService> allocator,
        std::shared_ptr<NativeRetailRendererTextures> textures);

    bool access(RetailRendererMaterialFailure& f) const noexcept;
    std::shared_ptr<NativeRetailMaterialRecord> validate(
        NativeRetailMaterialHandle handle,
        RetailRendererMaterialFailure& f) const noexcept;

    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
    std::shared_ptr<NativeRetailRendererTextures> textures_;
    std::vector<std::shared_ptr<NativeRetailMaterialRecord>> records_;
    const std::thread::id thread_{std::this_thread::get_id()};
    bool retired_{false};
};

} // namespace mka::retail
