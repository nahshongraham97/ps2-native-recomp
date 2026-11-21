#pragma once

#include <atomic>
#include <thread>

namespace mka::retail {

// A lifetime witness for native procedures that borrow the stack Main state.
// It carries no Main pointer or game data. Main retires it before fields die.
class NativeRetailMainLifetime final {
public:
    NativeRetailMainLifetime() noexcept = default;
    NativeRetailMainLifetime(const NativeRetailMainLifetime&) = delete;
    NativeRetailMainLifetime& operator=(const NativeRetailMainLifetime&) = delete;

    bool active_on_owner_thread() const noexcept {
        return active_.load(std::memory_order_acquire) &&
               std::this_thread::get_id() == owner_thread_;
    }
    void retire() noexcept { active_.store(false, std::memory_order_release); }

private:
    const std::thread::id owner_thread_{std::this_thread::get_id()};
    std::atomic<bool> active_{true};
};

} // namespace mka::retail
