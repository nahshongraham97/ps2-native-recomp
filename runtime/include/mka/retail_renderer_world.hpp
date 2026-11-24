#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_camera_world.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace mka::retail {

class NativeRetailRendererWorldModule;
struct NativeRetailWorldRecord;

enum class RetailRendererWorldError {
    none,
    owner,
    wrong_thread,
    retired,
    foreign_owner,
    invalid_handle,
    retired_world,
    reentrant,
    allocator,
    allocation,
    after_effect_unknown,
    native_exception
};

struct RetailRendererWorldFailure {
    RetailRendererWorldError kind{RetailRendererWorldError::none};
    const char* operation{""};
    std::uint32_t source_code{0};
    RetailRendererAllocatorFailure allocator{};
};

class NativeRetailWorldHandle final {
public:
    NativeRetailWorldHandle() noexcept = default;
    bool empty() const noexcept { return !assigned_; }
    bool same_identity(const NativeRetailWorldHandle& x) const noexcept {
        return assigned_ && x.assigned_ && !record_.owner_before(x.record_) && !x.record_.owner_before(record_);
    }

private:
    friend class NativeRetailRendererWorldModule;
    NativeRetailWorldHandle(
        std::weak_ptr<NativeRetailRendererWorldModule> o,
        std::weak_ptr<NativeRetailWorldRecord> r)
        : owner_(std::move(o)), record_(std::move(r)), assigned_(true) {}
    std::weak_ptr<NativeRetailRendererWorldModule> owner_;
    std::weak_ptr<NativeRetailWorldRecord> record_;
    bool assigned_{false};
};

struct RetailRendererWorldDescription {
    std::string name;
    std::array<float, 3> box_min{-10000.0f, -10000.0f, -10000.0f};
    std::array<float, 3> box_max{ 10000.0f,  10000.0f,  10000.0f};
    std::uint32_t flags{0};
};

struct RetailRendererWorldObservation {
    std::string name;
    std::array<float, 3> box_min;
    std::array<float, 3> box_max;
    std::uint32_t flags{0};
    bool destroying{false};
};

// Plugin 0412 scope recovered from Xbox 0x002cd7d0/0x002cd820/0x002c4600/0x002c4590
// and PS2 0x001a3df0/0x001a3e70/0x001a7690/0x001a75d0:
// - FreeList-backed 4-byte container allocation & circular list membership
// - Module open/close reference counting
// - Global module count tracking (Xbox 0x005c761c)
// - Real typed world ownership, thread affinity, and fail-closed validation
class NativeRetailWorld final : public NativeRetailRenderWorld {
public:
    ~NativeRetailWorld() override;
    const RetailRendererWorldDescription& description() const noexcept { return desc_; }

private:
    friend class NativeRetailRendererWorldModule;
    NativeRetailWorld(
        const std::shared_ptr<NativeRetailGameSession>& session,
        RetailRendererWorldDescription desc) noexcept;
    RetailRendererWorldDescription desc_;
};

class NativeRetailRendererWorldModule final
    : public std::enable_shared_from_this<NativeRetailRendererWorldModule> {
public:
    ~NativeRetailRendererWorldModule();
    NativeRetailRendererWorldModule(const NativeRetailRendererWorldModule&) = delete;
    NativeRetailRendererWorldModule& operator=(const NativeRetailRendererWorldModule&) = delete;
    NativeRetailRendererWorldModule(NativeRetailRendererWorldModule&&) = delete;
    NativeRetailRendererWorldModule& operator=(NativeRetailRendererWorldModule&&) = delete;

    static std::shared_ptr<NativeRetailRendererWorldModule> create(
        NativeRetailRendererCore& core,
        RetailRendererWorldFailure& f) noexcept;

    std::optional<NativeRetailWorldHandle> create_world(
        RetailRendererWorldDescription desc,
        RetailRendererWorldFailure& f) noexcept;

    bool destroy_world(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) noexcept;

    std::shared_ptr<NativeRetailWorld> resolve_world(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) const noexcept;

    std::optional<RetailRendererWorldObservation> observe(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) const noexcept;

    std::size_t active_count() const noexcept;

    void retire() noexcept;

private:
    friend class NativeRetailRendererCore;
    NativeRetailRendererWorldModule(
        std::weak_ptr<NativeRetailGameSession> session,
        std::shared_ptr<NativeRetailRendererAllocatorService> allocator);

    bool access(RetailRendererWorldFailure& f) const noexcept;
    std::shared_ptr<NativeRetailWorldRecord> validate(
        NativeRetailWorldHandle handle,
        RetailRendererWorldFailure& f) const noexcept;

    std::weak_ptr<NativeRetailGameSession> session_;
    std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;

    std::vector<std::shared_ptr<NativeRetailWorldRecord>> records_;

    bool retired_{false};
    bool busy_{false};
    std::size_t destroy_cursor_{0};
    const std::thread::id thread_{std::this_thread::get_id()};
};

namespace renderer_required {
bool construct_core_plugin_0412(NativeRetailRendererCore&, RetailRendererCoreFailure&);
void destroy_core_plugin_0412(NativeRetailRendererCore&) noexcept;
}

} // namespace mka::retail
