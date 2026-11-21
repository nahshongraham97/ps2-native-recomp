#include "mka/retail_game_records.hpp"

namespace mka::retail {
namespace {
bool fail(RetailGameRecordsFailure& f,RetailGameRecordsError kind,
          const char* operation) noexcept {
    f.kind=kind;
    f.operation=operation;
    return false;
}
RetailGameRecordsValues* values(NativeRetailGameSession& session,
                                 RetailGameRecordsFailure& f) noexcept {
    auto* result=session.game_records_values(f.session);
    if(!result)
        fail(f,RetailGameRecordsError::session,
             "1303F0 retained game-record values");
    return result;
}
}

bool apply_retail_game_records_prefix_1303f0(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    auto* random=session.random(f.session);
    if(!random)
        return fail(f,RetailGameRecordsError::session,
                    "1303F0 same-session source PRNG");
    // 165780(-1), 185CC0(12), two game words, 185C30(0x38).
    state->word_660098=0xffffffffu;
    if(!session.store_scalar_mode_from_source(12,f.session))
        return fail(f,RetailGameRecordsError::session,
                    "1303F0 scalar mode 12");
    state->word_6602e4=0;
    state->word_6602e8=0;
    const auto draw=random->low16_range(0x38,f.random);
    if(!draw)
        return fail(f,RetailGameRecordsError::random,
                    "1303F0 source 185C30 range draw");
    state->word_6600a4=*draw; // Original caller retains the low 16 bits.
    return true;
}

bool clear_retail_game_records_pair_1303f0(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    state->profile_preferences.word_34=0;
    state->profile_preferences.word_38=0;
    return true;
}

bool reset_retail_game_records_tail_163fd0(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    state->word_6602ac=0;
    state->word_6602c4=0;
    state->word_6602b0=0;
    state->word_6602b4=0;
    state->word_6602b8=0;
    state->word_6602bc=1;
    state->word_6602c0=0;
    return true;
}

bool reset_retail_game_records_word_173570(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    state->word_64e1f0=0;
    return true;
}
bool store_retail_game_records_bytes_127370(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    // The source calls 127370(0,0,0,0xFF), writing four adjacent bytes.
    state->word_64debc=0xff000000u;
    return true;
}
bool clear_retail_game_records_13c180_early_words(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    state->word_64e01c=0;
    state->word_64e000=0;
    state->word_64dff4=0;
    state->word_64dff0=0;
    state->word_64dffc=0;
    state->word_64dff8=0;
    state->word_64e018=0;
    state->word_64e014=0;
    state->word_64e010=0;
    state->word_64e00c=0;
    state->word_64e008=0;
    state->word_64e004=0;
    return true;
}
bool clear_retail_game_records_13c180_late_words(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    state->word_64dfec=0;
    state->word_64dfe8=0;
    return true;
}
bool clear_retail_game_records_16c1b0_word(
    NativeRetailGameSession& session,RetailGameRecordsFailure& f) noexcept {
    f={};
    auto* state=values(session,f);
    if(!state) return false;
    state->word_64e180=0;
    return true;
}
} // namespace mka::retail
