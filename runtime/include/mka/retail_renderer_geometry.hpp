#pragma once
#include "mka/retail_renderer_core.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace mka::retail {

class NativeRetailRendererGeometry;
struct NativeRetailGeometryRecord;

enum class RetailRendererGeometryError {
    none,
    owner,
    wrong_thread,
    retired,
    foreign_owner,
    invalid_handle,
    retired_geometry,
    reentrant,
    allocator,
    allocation,
    ref_count_underflow,
    after_effect_unknown,
    native_exception
};

struct RetailRendererGeometryFailure {
    RetailRendererGeometryError kind{RetailRendererGeometryError::none};
    const char* operation{""};
    std::uint32_t source_code{0};
    RetailRendererAllocatorFailure allocator{};
};

// Generational typed handle: provides fail-closed lifecycle tracking and prevents
// dangling references during rollback/unwind without raw pointer retention.
class NativeRetailGeometryHandle final {
public:
    NativeRetailGeometryHandle() noexcept = default;
    bool empty() const noexcept { return !assigned_; }
    bool same_identity(const NativeRetailGeometryHandle& x) const noexcept {
        return assigned_ && x.assigned_ && !record_.owner_before(x.record_) && !x.record_.owner_before(record_);
    }

private:
    friend class NativeRetailRendererGeometry;
    NativeRetailGeometryHandle(
        std::weak_ptr<NativeRetailRendererGeometry> o,
        std::weak_ptr<NativeRetailGeometryRecord> r)
        : owner_(std::move(o)), record_(std::move(r)), assigned_(true) {}
    std::weak_ptr<NativeRetailRendererGeometry> owner_;
    std::weak_ptr<NativeRetailGeometryRecord> record_;
    bool assigned_{false};
};

struct RetailRendererGeometryDescription {
    std::uint32_t flags{0};
    std::int32_t num_triangles{0};
    std::int32_t num_vertices{0};
};

struct RetailRendererGeometryObservation {
    std::uint32_t flags{0};
    std::int32_t num_triangles{0};
    std::int32_t num_vertices{0};
    std::int32_t ref_count{1};
    bool destroying{false};
};

// Plugin 040b scope strictly recovered from Xbox default.xbe (0x002c2d40 / 0x002c30a0)
// and PS2 SLUS_214.10 (0x001a1b30 / 0x001a1b90):
// - 40-byte container registration via engine plugin registry helper
// - Module count tracking: Xbox 0x0083531c, PS2 -23648($gp)
// - Base pointer registration: Xbox 0x00835318, PS2 -23648($gp)
// - Generational typed container ownership, refcounting, and thread affinity
class NativeRetailRendererGeometry final
    : public std::enable_shared_from_this<NativeRetailRendererGeometry> {
public:
    ~NativeRetailRendererGeometry();
    NativeRetailRendererGeometry(const NativeRetailRendererGeometry&) = delete;
    NativeRetailRendererGeometry& operator=(const NativeRetailRendererGeometry&) = delete;
    NativeRetailRendererGeometry(NativeRetailRendererGeometry&&) = delete;
    NativeRetailRendererGeometry& operator=(NativeRetailRendererGeometry&&) = delete;

    static std::shared_ptr<NativeRetailRendererGeometry> create(
        NativeRetailRendererCore& core,
        RetailRendererGeometryFailure& f) noexcept;

    std::optional<NativeRetailGeometryHandle> create_geometry(
        RetailRendererGeometryFailure& f) noexcept;

    std::optional<NativeRetailGeometryHandle> create_geometry(
        RetailRendererGeometryDescription desc,
        RetailRendererGeometryFailure& f) noexcept;

    bool destroy_geometry(
        NativeRetailGeometryHandle handle,
        RetailRendererGeometryFailure& f) noexcept;

    std::optional<std::int32_t> add_ref(
        NativeRetailGeometryHandle handle,
        RetailRendererGeometryFailure& f) noexcept;

    bool release(
        NativeRetailGeometryHandle handle,
        RetailRendererGeometryFailure& f) noexcept;

    std::optional<RetailRendererGeometryObservation> observe(
        NativeRetailGeometryHandle handle,
        RetailRendererGeometryFailure& f) const noexcept;

    std::size_t active_count() const noexcept;

    void retire() noexcept;

private:
    friend class NativeRetailRendererCore;
    NativeRetailRendererGeometry(
        std::weak_ptr<NativeRetailGameSession> session,
        std::shared_ptr<NativeRetailRendererAllocatorService> allocator);

    bool access(RetailRendererGeometryFailure& f) const noexcept;
    std::shared_ptr<NativeRetailGeometryRecord> validate(
        NativeRetailGeometryHandle handle,
        RetailRendererGeometryFailure& f) const noexcept;

    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
    std::vector<std::shared_ptr<NativeRetailGeometryRecord>> records_;
    const std::thread::id thread_{std::this_thread::get_id()};
    bool retired_{false};
};

} // namespace mka::retail
