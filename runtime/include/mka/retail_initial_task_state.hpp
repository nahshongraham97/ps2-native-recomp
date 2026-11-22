#pragma once

#include <cstdint>
#include <optional>
#include <thread>

namespace mka::retail {

enum class RetailInitialTaskStateError { none, wrong_thread, invalid_menu_index };
struct RetailInitialTaskStateFailure {
    RetailInitialTaskStateError kind{RetailInitialTaskStateError::none};
    const char* operation{""};
};
struct RetailInitialTaskWords {
    std::uint32_t first_run{};       // 64E32C, BSS zero, 18E154 writes one.
    std::int32_t menu_index{};       // 64E350, BSS zero, menu bodies also write it.
    std::uint32_t game_word_660094{};
    std::uint32_t cycle_word_6602ec{};
    std::uint32_t first_transition_latch{};  // Source64E334, full-word latch.
    std::uint32_t second_transition_latch{}; // Source64E330, full-word latch.
    std::uint32_t nonowning_object_reference{}; // Source 64E348, cleared by 18DA30.
    std::uint32_t nonowning_object_cookie{};    // Source 64E34C, cleared by 18DA34.
    std::uint32_t nonowning_task_reference{};   // Source 64E340, cleared by 18DA38.
    std::uint32_t nonowning_task_cookie{};      // Source 64E344, cleared by 18DA40.
};

// Known persistent game values, not guest RAM or a complete game-info
// facade. The containing game session determines their lifetime.
class NativeRetailInitialTaskState final {
public:
    NativeRetailInitialTaskState() = default;
    NativeRetailInitialTaskState(const NativeRetailInitialTaskState&) = delete;
    NativeRetailInitialTaskState& operator=(const NativeRetailInitialTaskState&) = delete;

    std::optional<RetailInitialTaskWords> observe(RetailInitialTaskStateFailure&) const noexcept;
    bool store_start_words_from_18e100(RetailInitialTaskStateFailure&) noexcept;
    bool store_first_run_from_18e100(RetailInitialTaskStateFailure&) noexcept;
    bool store_first_transition_latch(bool, RetailInitialTaskStateFailure&) noexcept;
    bool store_second_transition_latch(bool, RetailInitialTaskStateFailure&) noexcept;
    bool store_menu_index_from_source(std::int32_t, RetailInitialTaskStateFailure&) noexcept;
    // 18DA30 clears the nonowning object pair then task pair in source order.
    bool clear_nonowning_references_from_18da30(RetailInitialTaskStateFailure&) noexcept;
    // 18D430(0) stores the new menu index, then advances 6602EC.
    bool reset_menu_index_and_advance_cycle_from_18d430(
        RetailInitialTaskStateFailure&) noexcept;
    // Source 18DB60 wraps >=22 to one and increments 6602EC, then stores
    // index+1 before invoking the indexed body. Returns the selected slot.
    std::optional<unsigned> select_menu_body_from_18db60(RetailInitialTaskStateFailure&) noexcept;

private:
    bool owner(RetailInitialTaskStateFailure&) const noexcept;
    RetailInitialTaskWords words_{};
    const std::thread::id owner_thread_{std::this_thread::get_id()};
};

} // namespace mka::retail
