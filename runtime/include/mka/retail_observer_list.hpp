#pragma once

#include "mka/retail_game_pool.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

namespace mka::retail {

enum class RetailObserverListError {
    none, wrong_thread, missing_object, foreign_link, already_retired,
    pending_transfer, reentrant_retirement, active_links, cleanup_no_progress,
    after_effect_unknown, pool, allocation, native_exception
};
struct RetailObserverListFailure {
    RetailObserverListError kind{RetailObserverListError::none};
    const char* operation{ "" };
    RetailGamePoolFailure pool;
};

// A real source object supplies its cookie and virtual +0xC retirement.
// Implementations are the native actor/resource producers, not raw pointers.
class NativeRetailObservedObject {
public:
    virtual ~NativeRetailObservedObject() = default;
    virtual std::uint32_t source_cookie() const noexcept = 0;
    virtual void cleanup_from_12fa00() = 0;
};
class NativeRetailObserverCallback {
public:
    virtual ~NativeRetailObserverCallback() = default;
    virtual void invoke(const std::shared_ptr<NativeRetailObservedObject>&) = 0;
};

namespace detail { struct RetailObserverLinkState; }
class NativeRetailObserverLink final {
public:
    NativeRetailObserverLink(const NativeRetailObserverLink&)=delete;
    NativeRetailObserverLink& operator=(const NativeRetailObserverLink&)=delete;
private:
    friend class NativeRetailObserverList;
    explicit NativeRetailObserverLink(
        std::shared_ptr<detail::RetailObserverLinkState> state) noexcept
        : state_(std::move(state)) {}
    std::shared_ptr<detail::RetailObserverLinkState> state_;
};

struct RetailObserverListObservation {
    std::size_t linked{}, pending{}, retired{};
    bool after_effect_unknown{};
};

// 12FF70 + 12F870 observer birth and 12F4E0 traversal on one real game pool.
// Callback producers must supply executable native object and callback code.
class NativeRetailObserverList final {
public:
    explicit NativeRetailObserverList(
        std::shared_ptr<NativeRetailGamePool>) noexcept;
    NativeRetailObserverList(const NativeRetailObserverList&)=delete;
    NativeRetailObserverList& operator=(const NativeRetailObserverList&)=delete;
    std::shared_ptr<NativeRetailObserverLink> insert_head_from_12f870(
        std::shared_ptr<NativeRetailObservedObject>,RetailObserverListFailure&);
    bool retire_link_from_12fa00(
        const std::shared_ptr<NativeRetailObserverLink>&,
        RetailObserverListFailure&);
    bool visit_from_12f4e0(NativeRetailObserverCallback&,
        RetailObserverListFailure&);
    // The 13A7A0 actor head has the same cookie/stale-link traversal but no
    // temporary bit-1 callback guard; its callback is source 13A870.
    bool visit_from_13a7a0(NativeRetailObserverCallback&,
        RetailObserverListFailure&);
    // Source 130AC0 retires every reachable link, invoking live object
    // cleanup with the temporary bit-1 guard. Used on task +E8 then +EC.
    bool drain_from_130ac0(RetailObserverListFailure&);
    std::shared_ptr<NativeRetailGamePool> retained_pool(
        RetailObserverListFailure&) const noexcept;
    // A source global-head zero store after pool reset. Refuses to discard
    // retained native links or a pending nonlocal cleanup transfer.
    bool reset_empty_head_from_source(RetailObserverListFailure&) noexcept;
    std::optional<RetailObserverListObservation> observe(
        RetailObserverListFailure&) const noexcept;
private:
    bool access(RetailObserverListFailure&) const noexcept;
    bool retire(const std::shared_ptr<detail::RetailObserverLinkState>&,
        RetailObserverListFailure&);
    std::shared_ptr<NativeRetailGamePool> pool_;
    std::shared_ptr<detail::RetailObserverLinkState> head_;
    std::vector<std::shared_ptr<detail::RetailObserverLinkState>> pending_;
    std::size_t retired_{};
    bool after_effect_unknown_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
