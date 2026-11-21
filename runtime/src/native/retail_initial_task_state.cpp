#include "mka/retail_initial_task_state.hpp"

namespace mka::retail {
bool NativeRetailInitialTaskState::owner(RetailInitialTaskStateFailure& f) const noexcept {
    f={};
    if(std::this_thread::get_id()==owner_thread_) return true;
    f={RetailInitialTaskStateError::wrong_thread,"initial task source-word owner thread"};
    return false;
}
std::optional<RetailInitialTaskWords> NativeRetailInitialTaskState::observe(
    RetailInitialTaskStateFailure& f) const noexcept {
    if(!owner(f)) return {};
    return words_;
}
bool NativeRetailInitialTaskState::store_start_words_from_18e100(
    RetailInitialTaskStateFailure& f) noexcept {
    if(!owner(f)) return false;
    words_.cycle_word_6602ec=0; // 18E118.
    words_.game_word_660094=0x38; // 18E120.
    return true;
}
bool NativeRetailInitialTaskState::store_first_run_from_18e100(
    RetailInitialTaskStateFailure& f) noexcept {
    if(!owner(f)) return false;
    words_.first_run=1; // 18E154, after both child procedures.
    return true;
}
bool NativeRetailInitialTaskState::store_menu_index_from_source(
    std::int32_t index,RetailInitialTaskStateFailure& f) noexcept {
    if(!owner(f)) return false;
    words_.menu_index=index;
    return true;
}
std::optional<unsigned> NativeRetailInitialTaskState::select_menu_body_from_18db60(
    RetailInitialTaskStateFailure& f) noexcept {
    if(!owner(f)) return {};
    auto index=words_.menu_index;
    if(index<0) {
        f={RetailInitialTaskStateError::invalid_menu_index,
           "negative source menu index would address before 22-entry table"};
        return {};
    }
    if(index>=22) {
        index=1;
        ++words_.cycle_word_6602ec; // 18DBB8/BC, unsigned source word wraps.
    }
    // 18DBE4 is the selected-body call delay slot: publish next index first.
    words_.menu_index=static_cast<std::int32_t>(static_cast<std::uint32_t>(index)+1u);
    return static_cast<unsigned>(index);
}
} // namespace mka::retail
