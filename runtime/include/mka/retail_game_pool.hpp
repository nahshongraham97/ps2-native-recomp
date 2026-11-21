#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>

namespace mka::retail {

enum class RetailGamePoolError {
    none, wrong_thread, active_links, invalid_link, stale_link,
    foreign_link, allocation, host_capacity, native_exception
};
struct RetailGamePoolFailure {
    RetailGamePoolError kind{RetailGamePoolError::none};
    const char* operation{""};
};
enum class RetailGamePoolLinkKind { task_12ffe0, observer_12ff70 };
struct RetailGamePoolObservation {
    std::size_t configured_initial_records{};
    std::size_t native_records{};
    std::size_t available_records{};
    std::size_t active_records{};
    std::optional<std::size_t> first_free_ordinal;
};

namespace detail { struct RetailGamePoolState; }

// A native storage lease for one source link. Its ordinal/generation are host
// identity, never an original pointer or a game cookie. Actual list insertion
// and virtual cleanup remain with the source-proved consumer (task registry,
// scene owner, etc.); this lease owns the shared free-list slot lifecycle.
class NativeRetailGamePoolLink final {
public:
    NativeRetailGamePoolLink() noexcept = default;
    ~NativeRetailGamePoolLink();
    NativeRetailGamePoolLink(NativeRetailGamePoolLink&&) noexcept;
    NativeRetailGamePoolLink& operator=(NativeRetailGamePoolLink&&) noexcept;
    NativeRetailGamePoolLink(const NativeRetailGamePoolLink&) = delete;
    NativeRetailGamePoolLink& operator=(const NativeRetailGamePoolLink&) = delete;

    bool valid() const noexcept { return bool(state_); }
    std::uint32_t cached_object_identity() const noexcept { return cached_identity_; }
    RetailGamePoolLinkKind kind() const noexcept { return kind_; }
    std::size_t native_ordinal() const noexcept { return ordinal_; }
    bool release(RetailGamePoolFailure&) noexcept;

private:
    friend class NativeRetailGamePool;
    NativeRetailGamePoolLink(std::shared_ptr<detail::RetailGamePoolState>,
        std::size_t ordinal, std::uint64_t generation,
        std::uint32_t cached_identity, RetailGamePoolLinkKind) noexcept;
    void release_unchecked() noexcept;
    std::shared_ptr<detail::RetailGamePoolState> state_;
    std::size_t ordinal_{};
    std::uint64_t generation_{};
    std::uint32_t cached_identity_{};
    RetailGamePoolLinkKind kind_{RetailGamePoolLinkKind::task_12ffe0};
};

// 12DA70/130030's real native record storage. The source profile count is
// initial reserve only; later allocation can grow beyond it without a PS2 cap.
class NativeRetailGamePool final {
public:
    NativeRetailGamePool(const NativeRetailGamePool&) = delete;
    NativeRetailGamePool& operator=(const NativeRetailGamePool&) = delete;

    static std::unique_ptr<NativeRetailGamePool> initialize(
        std::uint64_t source_profile_count, RetailGamePoolFailure&) noexcept;
    bool reset_from_12da70(std::uint64_t source_profile_count,
        RetailGamePoolFailure&) noexcept;
    std::optional<NativeRetailGamePoolLink> acquire_task_link(
        std::uint32_t object_identity, RetailGamePoolFailure&) noexcept;
    std::optional<NativeRetailGamePoolLink> acquire_observer_link(
        std::uint32_t object_identity, RetailGamePoolFailure&) noexcept;
    std::optional<RetailGamePoolObservation> observe(
        RetailGamePoolFailure&) const noexcept;

private:
    NativeRetailGamePool() = default;
    std::optional<NativeRetailGamePoolLink> acquire(
        std::uint32_t, RetailGamePoolLinkKind, RetailGamePoolFailure&) noexcept;
    std::shared_ptr<detail::RetailGamePoolState> state_;
};

} // namespace mka::retail
