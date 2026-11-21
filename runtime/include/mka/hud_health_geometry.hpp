#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mka::retail {
enum class HudHealthGeometryError {
    none, allocation_failed, native_exception, invalid_geometry, invalid_owner,
    inactive_owner, unbound_reference, invalid_configuration, insufficient_vertices, generation_exhausted
};
struct HudHealthGeometryFailure { HudHealthGeometryError kind{HudHealthGeometryError::none}; };
struct HudPositionVertex {
    float x{}, y{};
    // Native opaque channels: this leaf assigns no UV/color/device meaning.
    std::vector<std::byte> untouched_channels;
    bool operator==(const HudPositionVertex&) const = default;
};
struct HudPositionSnapshot { std::vector<HudPositionVertex> vertices; bool dirty{}; };
namespace detail { struct HudPositionState; struct HudObjectState; struct HudHealthAccess; }
class NativeHudPositionGeometry final {
public:
    static std::shared_ptr<NativeHudPositionGeometry> create(std::vector<HudPositionVertex>, HudHealthGeometryFailure&) noexcept;
    std::optional<HudPositionSnapshot> snapshot(HudHealthGeometryFailure&) const noexcept;
    NativeHudPositionGeometry(const NativeHudPositionGeometry&) = delete;
    NativeHudPositionGeometry& operator=(const NativeHudPositionGeometry&) = delete;
    NativeHudPositionGeometry(NativeHudPositionGeometry&&) = delete;
    NativeHudPositionGeometry& operator=(NativeHudPositionGeometry&&) = delete;
private:
    friend struct detail::HudHealthAccess;
    explicit NativeHudPositionGeometry(std::shared_ptr<detail::HudPositionState>) noexcept;
    std::shared_ptr<detail::HudPositionState> state_;
};
class HudHealthObjectRef final {
public:
    HudHealthObjectRef() noexcept = default; // Unknown producer, distinct from known absence.
    static HudHealthObjectRef known_absent() noexcept;
private:
    friend class NativeHudHealthObject;
    friend struct detail::HudHealthAccess;
    enum class Kind { unbound, absent, bound };
    Kind kind_{Kind::unbound};
    std::weak_ptr<detail::HudObjectState> owner_;
    std::uint64_t generation_{};
};
class NativeHudHealthObject final {
public:
    static std::optional<NativeHudHealthObject> create(std::shared_ptr<NativeHudPositionGeometry>, HudHealthGeometryFailure&) noexcept;
    ~NativeHudHealthObject();
    NativeHudHealthObject(NativeHudHealthObject&&) noexcept;
    NativeHudHealthObject& operator=(NativeHudHealthObject&&) noexcept;
    NativeHudHealthObject(const NativeHudHealthObject&) = delete;
    NativeHudHealthObject& operator=(const NativeHudHealthObject&) = delete;
    std::optional<HudHealthObjectRef> reference(HudHealthGeometryFailure&) const noexcept;
    bool retire(HudHealthGeometryFailure&) noexcept;
private:
    explicit NativeHudHealthObject(std::shared_ptr<detail::HudObjectState>) noexcept;
    void invalidate() noexcept;
    std::shared_ptr<detail::HudObjectState> state_;
};
struct HudHealthGeometryConfig {
    std::int64_t design_origin, logical_width;
    float displayed_left, displayed_right;
};
enum class HudHealthUpdate { updated, skipped };
std::optional<HudHealthUpdate> update_hud_health_geometry(const HudHealthObjectRef&, const HudHealthObjectRef&,
    const HudHealthGeometryConfig&, HudHealthGeometryFailure&) noexcept;
} // namespace mka::retail
