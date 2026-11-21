#include "mka/hud_health_geometry.hpp"
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <new>
#include <utility>

namespace mka::retail {
namespace detail {
struct HudPositionState { std::mutex mutex; std::vector<HudPositionVertex> vertices; bool dirty{}; };
struct HudObjectState {
    std::mutex mutex;
    bool live{true};
    std::uint64_t generation{1};
    std::shared_ptr<NativeHudPositionGeometry> geometry;
};
struct HudHealthAccess {
    static bool unbound(const HudHealthObjectRef& r) { return r.kind_ == HudHealthObjectRef::Kind::unbound; }
    static bool absent(const HudHealthObjectRef& r) { return r.kind_ == HudHealthObjectRef::Kind::absent; }
    static auto lock(const HudHealthObjectRef& r) { return r.owner_.lock(); }
    static bool valid(const HudHealthObjectRef& r, const HudObjectState& s) { return s.live && r.generation_ == s.generation; }
    static auto geometry(const NativeHudPositionGeometry& g) { return g.state_; }
};
} // namespace detail
namespace {
bool fail(HudHealthGeometryFailure& f, HudHealthGeometryError e) noexcept { f.kind = e; return false; }
// Force separate single-precision operations; preserve source grouping, no FMA/layout rewrite.
float add(float a, float b) noexcept { volatile float result = a + b; return result; }
float sub(float a, float b) noexcept { volatile float result = a - b; return result; }
float mul(float a, float b) noexcept { volatile float result = a * b; return result; }
struct Position { float x, y; };
using Quad = std::array<Position, 4>;
} // namespace
NativeHudPositionGeometry::NativeHudPositionGeometry(std::shared_ptr<detail::HudPositionState> s) noexcept
    : state_(std::move(s)) {}
std::shared_ptr<NativeHudPositionGeometry> NativeHudPositionGeometry::create(
    std::vector<HudPositionVertex> vertices, HudHealthGeometryFailure& f) noexcept {
    f = {};
    try {
        auto state = std::make_shared<detail::HudPositionState>();
        state->vertices = std::move(vertices);
        return std::shared_ptr<NativeHudPositionGeometry>(new NativeHudPositionGeometry(std::move(state)));
    } catch (const std::bad_alloc&) { fail(f, HudHealthGeometryError::allocation_failed); }
      catch (...) { fail(f, HudHealthGeometryError::native_exception); }
    return {};
}
std::optional<HudPositionSnapshot> NativeHudPositionGeometry::snapshot(HudHealthGeometryFailure& f) const noexcept {
    f = {};
    try { std::lock_guard lock(state_->mutex); return HudPositionSnapshot{state_->vertices, state_->dirty}; }
    catch (const std::bad_alloc&) { fail(f, HudHealthGeometryError::allocation_failed); }
    catch (...) { fail(f, HudHealthGeometryError::native_exception); }
    return {};
}
HudHealthObjectRef HudHealthObjectRef::known_absent() noexcept { HudHealthObjectRef r; r.kind_ = Kind::absent; return r; }
NativeHudHealthObject::NativeHudHealthObject(std::shared_ptr<detail::HudObjectState> s) noexcept : state_(std::move(s)) {}
std::optional<NativeHudHealthObject> NativeHudHealthObject::create(
    std::shared_ptr<NativeHudPositionGeometry> geometry, HudHealthGeometryFailure& f) noexcept {
    f = {};
    if (!geometry) { fail(f, HudHealthGeometryError::invalid_geometry); return {}; }
    try {
        auto state = std::make_shared<detail::HudObjectState>();
        state->geometry = std::move(geometry);
        return NativeHudHealthObject(std::move(state));
    } catch (const std::bad_alloc&) { fail(f, HudHealthGeometryError::allocation_failed); }
      catch (...) { fail(f, HudHealthGeometryError::native_exception); }
    return {};
}
NativeHudHealthObject::~NativeHudHealthObject() { invalidate(); }
NativeHudHealthObject::NativeHudHealthObject(NativeHudHealthObject&& other) noexcept : state_(std::move(other.state_)) {}
NativeHudHealthObject& NativeHudHealthObject::operator=(NativeHudHealthObject&& other) noexcept {
    if (this != &other) { invalidate(); state_ = std::move(other.state_); }
    return *this;
}
void NativeHudHealthObject::invalidate() noexcept {
    if (!state_) return;
    std::shared_ptr<NativeHudPositionGeometry> released;
    {
        std::lock_guard lock(state_->mutex);
        if (!state_->live) return;
        state_->live = false;
        if (state_->generation != std::numeric_limits<std::uint64_t>::max()) ++state_->generation;
        released = std::move(state_->geometry);
    }
}
std::optional<HudHealthObjectRef> NativeHudHealthObject::reference(HudHealthGeometryFailure& f) const noexcept {
    f = {};
    if (!state_) { fail(f, HudHealthGeometryError::invalid_owner); return {}; }
    try {
        std::lock_guard lock(state_->mutex);
        if (!state_->live) { fail(f, HudHealthGeometryError::inactive_owner); return {}; }
        HudHealthObjectRef r;
        r.kind_ = HudHealthObjectRef::Kind::bound; r.owner_ = state_; r.generation_ = state_->generation;
        return r;
    } catch (...) { fail(f, HudHealthGeometryError::native_exception); return {}; }
}
bool NativeHudHealthObject::retire(HudHealthGeometryFailure& f) noexcept {
    f = {};
    if (!state_) return fail(f, HudHealthGeometryError::invalid_owner);
    std::shared_ptr<NativeHudPositionGeometry> released;
    try {
        std::lock_guard lock(state_->mutex);
        if (!state_->live) return fail(f, HudHealthGeometryError::inactive_owner);
        if (state_->generation == std::numeric_limits<std::uint64_t>::max())
            return fail(f, HudHealthGeometryError::generation_exhausted);
        state_->live = false; ++state_->generation;
        released = std::move(state_->geometry);
        return true;
    } catch (...) { return fail(f, HudHealthGeometryError::native_exception); }
}
std::optional<HudHealthUpdate> update_hud_health_geometry(const HudHealthObjectRef& left,
    const HudHealthObjectRef& right, const HudHealthGeometryConfig& c, HudHealthGeometryFailure& f) noexcept {
    f = {};
    using Access = detail::HudHealthAccess;
    if (Access::unbound(left) || Access::unbound(right)) { fail(f, HudHealthGeometryError::unbound_reference); return {}; }
    if (Access::absent(left) || Access::absent(right)) return HudHealthUpdate::skipped;
    auto l = Access::lock(left), r = Access::lock(right);
    if (!l || !r) return HudHealthUpdate::skipped;
    try {
        std::unique_lock ll(l->mutex, std::defer_lock), rl(r->mutex, std::defer_lock);
        if (l == r) ll.lock(); else std::lock(ll, rl);
        if (!Access::valid(left, *l) || !Access::valid(right, *r)) return HudHealthUpdate::skipped;
        if (!l->geometry || !r->geometry) { fail(f, HudHealthGeometryError::invalid_geometry); return {}; }
        if (!std::isfinite(c.displayed_left) || !std::isfinite(c.displayed_right)) {
            fail(f, HudHealthGeometryError::invalid_configuration); return {};
        }
        const float origin = static_cast<float>(c.design_origin), width = static_cast<float>(c.logical_width);
        const float base = add(add(origin, 32.0f), 256.0f);
        const float ls = mul(c.displayed_left, 256.0f), rs = mul(c.displayed_right, 256.0f);
        const Quad left_quad{{{sub(add(base, 0.0f), ls), 392.0f},
            {c.displayed_left > 0.0f ? sub(add(sub(base, 7.0f), 0.0f), ls) : base, 410.0f},
            {base, 410.0f}, {base, 392.0f}}};
        const Quad right_quad{{{sub(width, base), 392.0f}, {sub(width, base), 410.0f},
            {c.displayed_right > 0.0f ? sub(width, sub(add(sub(base, 7.0f), 0.0f), rs)) : sub(width, base), 410.0f},
            {add(sub(width, sub(add(base, 0.0f), rs)), 1.0f), 392.0f}}};
        for (const auto& q : {left_quad, right_quad})
            for (const auto& p : q) if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
                fail(f, HudHealthGeometryError::invalid_configuration); return {};
            }
        auto lg = Access::geometry(*l->geometry), rg = Access::geometry(*r->geometry);
        std::unique_lock gl(lg->mutex, std::defer_lock), gr(rg->mutex, std::defer_lock);
        if (lg == rg) gl.lock(); else std::lock(gl, gr);
        if (lg->vertices.size() < 4 || rg->vertices.size() < 4) {
            fail(f, HudHealthGeometryError::insufficient_vertices); return {};
        }
        // All validation is complete. No allocations/callbacks/failures between these edits.
        for (std::size_t i = 0; i < 4; ++i) { lg->vertices[i].x = left_quad[i].x; lg->vertices[i].y = left_quad[i].y; }
        lg->dirty = true;
        for (std::size_t i = 0; i < 4; ++i) { rg->vertices[i].x = right_quad[i].x; rg->vertices[i].y = right_quad[i].y; }
        rg->dirty = true;
        return HudHealthUpdate::updated;
    } catch (...) { fail(f, HudHealthGeometryError::native_exception); return {}; }
}
} // namespace mka::retail
