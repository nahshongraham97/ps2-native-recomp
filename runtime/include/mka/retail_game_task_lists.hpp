#pragma once

#include "mka/retail_observer_list.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

namespace mka::retail {
enum class RetailGameTaskListRoot : std::uint8_t {
    e1a8, e024, e020, e7ec, e7e8, dfe4, e02c, dfe0, e1ac
};
enum class RetailGameTaskListError {
    none, wrong_thread, missing_pool, foreign_pool, invalid_root,
    observer_list, allocation, native_exception
};
struct RetailGameTaskListFailure {
    RetailGameTaskListError kind{RetailGameTaskListError::none};
    const char* operation{ "" };
    RetailObserverListFailure list;
};

// Nine distinct source global callback heads, retained across Main reentry.
// The source constructors zero them at separate ordered callpoints after
// 12DA70; no task body manufactures a fresh local head on each frame.
class NativeRetailGameTaskLists final {
public:
    static std::shared_ptr<NativeRetailGameTaskLists> create(
        std::shared_ptr<NativeRetailGamePool>,RetailGameTaskListFailure&) noexcept;
    NativeRetailGameTaskLists(const NativeRetailGameTaskLists&)=delete;
    NativeRetailGameTaskLists& operator=(const NativeRetailGameTaskLists&)=delete;
    bool same_pool(const std::shared_ptr<NativeRetailGamePool>&,
        RetailGameTaskListFailure&) const noexcept;
    std::shared_ptr<NativeRetailObserverList> root(
        RetailGameTaskListRoot,RetailGameTaskListFailure&) const noexcept;
    bool clear_from_228b90(RetailGameTaskListFailure&) noexcept;
    bool clear_first_pair_from_13c180(RetailGameTaskListFailure&) noexcept;
    bool clear_later_pair_from_13c180(RetailGameTaskListFailure&) noexcept;
    bool clear_from_139a30(RetailGameTaskListFailure&) noexcept;
    bool clear_from_16c1b0(RetailGameTaskListFailure&) noexcept;
private:
    explicit NativeRetailGameTaskLists(
        std::shared_ptr<NativeRetailGamePool>) noexcept;
    bool access(RetailGameTaskListFailure&) const noexcept;
    bool clear(const RetailGameTaskListRoot*,std::size_t,
        RetailGameTaskListFailure&) noexcept;
    std::shared_ptr<NativeRetailGamePool> pool_;
    std::array<std::shared_ptr<NativeRetailObserverList>,9> roots_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};
} // namespace mka::retail
