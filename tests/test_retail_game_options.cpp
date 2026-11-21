#include "mka/retail_game_session.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
void check(bool ok, const char* what) {
    if (!ok) throw std::runtime_error(what);
}
template <std::size_t N>
std::optional<std::span<const std::uint8_t>> record(
    const std::array<std::uint8_t, N>& bytes) {
    return std::span<const std::uint8_t>{bytes};
}
void check_six(const RetailGameOptionsSnapshot& s, std::uint32_t first) {
    // Independently from the original 164130 words: 660380 is mode==3;
    // the following source writes are 3, 3, 1, 1, and (34EE30's 4)-2.
    check(s.word_660380 == first && s.word_660384 == 3 &&
          s.word_66038c == 3 && s.word_660388 == 1 &&
          s.word_660390 == 1 && s.word_660394 == 2,
          "164130 six distinct words and source delay-slot values");
}
}

int main() { try {
    RetailGameSessionFailure sf;
    auto session = NativeRetailGameSession::create(sf);
    check(session && sf.kind == RetailGameSessionError::none,
          "retained game session birth");
    auto retained = session;
    auto* options = session->game_options(sf);
    check(options && options == retained->game_options(sf),
          "one persistent option owner in the actual game session");
    RetailGameOptionsFailure f;
    auto s = options->observe(f);
    check(s && s->live_mode == 0 && s->word_660380 == 0 &&
          s->word_660384 == 0 && s->word_66038c == 0 &&
          s->word_660388 == 0 && s->word_660390 == 0 &&
          s->word_660394 == 0,
          "64EDF0 and six option words have source BSS-zero birth");
    check(options->publish_six_words_from_164130(f),
          "first 164130 publication from live zero mode");
    check_six(*options->observe(f), 0);

    // 3433C0 reads a signed first byte and at most a second byte; its
    // supported prefix is ASCII digit followed by dot. Tail is unread.
    constexpr std::array<std::uint8_t, 4> four{'4', '.', '9', 0};
    const std::array cases{std::array<std::uint8_t,3>{'1','.','x'},
        std::array<std::uint8_t,3>{'2','.',0xff},
        std::array<std::uint8_t,3>{'3','.',0},
        std::array<std::uint8_t,3>{'4','.','x'}};
    for (std::uint32_t mode = 0; mode != cases.size(); ++mode) {
        check(options->parse_selected_record_from_3433c0(record(cases[mode]), f) &&
              f.kind == RetailGameOptionsError::none &&
              options->observe(f)->live_mode == mode,
              "3433C0 source ASCII prefix selects full-width mode");
        check(options->publish_six_words_from_164130(f),
              "164130 publishes from current mode on every call");
        check_six(*options->observe(f), mode == 3 ? 1u : 0u);
    }
    constexpr std::array<std::uint8_t, 2> no_dot{'4', 'x'};
    constexpr std::array<std::uint8_t, 2> unknown{'8', '.'};
    constexpr std::array<std::uint8_t, 2> signed_byte{0xff, '.'};
    constexpr std::array<std::uint8_t, 1> first_nul{0};
    for (auto bytes : {std::span<const std::uint8_t>{no_dot},
                       std::span<const std::uint8_t>{unknown},
                       std::span<const std::uint8_t>{signed_byte},
                       std::span<const std::uint8_t>{first_nul}}) {
        check(options->parse_selected_record_from_3433c0(bytes, f) &&
              options->observe(f)->live_mode == 0,
              "nonmatching or NUL-first source record resets mode");
    }
    check(options->parse_selected_record_from_3433c0(std::nullopt, f) &&
          options->observe(f)->live_mode == 0,
          "null original record resets mode without byte read");

    // Checked native selected-record spans are a safety boundary: the
    // original has no length argument. Keep its preceding zero store.
    check(options->parse_selected_record_from_3433c0(record(four), f) &&
          options->observe(f)->live_mode == 3,
          "set a nonzero mode before truncated-record refusal");
    constexpr std::array<std::uint8_t, 0> empty{};
    check(!options->parse_selected_record_from_3433c0(record(empty), f) &&
          f.kind == RetailGameOptionsError::truncated_selected_record &&
          options->observe(f)->live_mode == 0,
          "empty selected-record capability fails after source zero store");
    check(options->parse_selected_record_from_3433c0(record(four), f),
          "restore nonzero mode before one-byte refusal");
    constexpr std::array<std::uint8_t, 1> one_byte{'4'};
    check(!options->parse_selected_record_from_3433c0(record(one_byte), f) &&
          f.kind == RetailGameOptionsError::truncated_selected_record &&
          options->observe(f)->live_mode == 0,
          "second-byte absence fails after source zero store");
    check(options->parse_selected_record_from_3433c0(record(first_nul), f),
          "first NUL does not require a second byte");

    bool foreign_refused = false;
    std::thread foreign([&] {
        RetailGameSessionFailure other_session;
        RetailGameOptionsFailure other;
        const auto prior = options->parse_selected_record_from_3433c0(record(four), other);
        const auto published = options->publish_six_words_from_164130(other);
        const auto observed = options->observe(other);
        foreign_refused = !retained->game_options(other_session) &&
            other_session.kind == RetailGameSessionError::wrong_thread &&
            !prior && !published && !observed &&
            other.kind == RetailGameOptionsError::wrong_thread;
    });
    foreign.join();
    check(foreign_refused && options->observe(f)->live_mode == 0 &&
          options->parse_selected_record_from_3433c0(record(four), f) &&
          options->observe(f)->live_mode == 3,
          "foreign borrower cannot mutate retained owner; owner remains usable");
    session.reset();
    check(options == retained->game_options(sf) &&
          options->observe(f)->live_mode == 3,
          "option state persists with same game session across Main borrow");
    std::puts("PASS native retail game options; network producer/consumer join open");
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "FAIL native retail game options: %s\n", e.what());
    return 1;
} }
