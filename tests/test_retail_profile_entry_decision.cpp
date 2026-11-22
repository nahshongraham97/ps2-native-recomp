#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_profile_aggregate.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>

#define NOMINMAX
#include <windows.h>

namespace {
void check(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}
struct ChildFixture {
    std::uint32_t result{};
    bool refuse{};
    bool transfer{};
    std::uint32_t calls{};
    std::uint32_t last_role{};
} child;
} // namespace

// This isolated1892D0 fixture replaces the production4EEE60 child.
// The actual child has a separate actual-owner permission client.
namespace mka::retail {
std::optional<std::uint32_t> observe_retail_profile_permission_from_4eee60(
    NativeRetailMainState& main, std::uint32_t role, RetailProfileMenuFailure&) {
    ++child.calls;
    child.last_role = role;
    if (child.transfer) throw detail::NativeMainReentry{&main, 23};
    if (child.refuse) return std::nullopt;
    return child.result;
}
} // namespace mka::retail

int main() {
    using namespace mka::retail;
    const auto temp = std::filesystem::temp_directory_path();
    std::filesystem::path root;
    for (unsigned i = 0; ; ++i) {
        root = temp / ("mka-profile-entry-owned-" + std::to_string(GetCurrentProcessId()) +
            "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directory(root, error)) break;
        check(i < 100u, "create exclusively owned fixture directory");
    }
    {
        mka::content::ContentVolumeFailure cf;
        auto volume = mka::content::NativeContentVolume::open(root, cf);
        check(bool(volume), "open owned native content root");
        NativeRetailStartupState startup(std::move(*volume), {});
        StartupFailure startup_failure;
        check(initialize_native_retail_game_session(startup, startup_failure),
            "real retained Session birth");
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup, samples);
        RetailProfileMenuFailure failure;

        // Native lifetime preflight is separate from source key admission.
        check(decide_retail_profile_entry_from_1892d0(main, 64u, 0u, 0u, failure) == 1u &&
              decide_retail_profile_entry_from_1892d0(main, 0xffffffffu, 0u, 0u, failure) == 1u &&
              child.calls == 0u, "signed and unsigned out-of-range keys return one");
        check(!decide_retail_profile_entry_from_1892d0(main, 3u, 0u, 0u, failure) &&
              failure.kind == RetailProfileMenuError::required_child,
              "uninitialized actual aggregate cannot act as zero masks");

        RetailGameSessionFailure session_failure;
        auto* aggregate = startup.game_session->profile_aggregate(session_failure);
        auto* words = startup.game_session->game_records_values(session_failure);
        check(aggregate && words, "one actual aggregate and live override owner");
        std::array<RetailProfileAggregateParentView, 2> parents{};
        parents[0].status_word = 5;
        parents[1].status_word = 5;
        RetailProfileAggregateFailure aggregate_failure;
        check(aggregate->refresh_data_from_2432a0(parents, words->word_64e318,
              aggregate_failure), "source baseline aggregate before menu child");

        check(decide_retail_profile_entry_from_1892d0(main, 0u, 0u, 0u, failure) == 0u &&
              decide_retail_profile_entry_from_1892d0(main, 3u, 0u, 0u, failure) == 1u &&
              decide_retail_profile_entry_from_1892d0(main, 63u, 0u, 0u, failure) == 1u,
              "file-backed first mask bit polarity and word63");
        check(decide_retail_profile_entry_from_1892d0(main, 1u, 9u, 0u, failure) == 1u,
              "nonzero selector uses second mask even when not one");
        words->word_64e318 = std::uint64_t{1} << 63;
        check(decide_retail_profile_entry_from_1892d0(main, 63u, 1u, 0u, failure) == 0u,
              "live 64E318 reread after aggregate refresh");

        child = {};
        check(decide_retail_profile_entry_from_1892d0(main, 62u, 1u, 5u, failure) == 1u &&
              child.calls == 0u, "role five skips 4EEE60 and selector short-circuits");
        child = {.result = 1u};
        check(decide_retail_profile_entry_from_1892d0(main, 62u, 0u, 1u, failure) == 1u &&
              child.calls == 1u && child.last_role == 1u,
              "nonzero 4EEE60 result returns one before masks");
        child = {};
        check(decide_retail_profile_entry_from_1892d0(main, 62u, 0u, 0u, failure) == 0u &&
              child.calls == 1u, "zero 4EEE60 result reaches first-mask bit62");
        child = {.refuse = true};
        check(!decide_retail_profile_entry_from_1892d0(main, 62u, 0u, 0u, failure) &&
              failure.kind == RetailProfileMenuError::required_child,
              "missing real child is named, never retail zero");
        child = {.transfer = true};
        bool transferred = false;
        try { (void)decide_retail_profile_entry_from_1892d0(main, 62u, 0u, 0u, failure); }
        catch (const detail::NativeMainReentry&) { transferred = true; }
        check(transferred, "actual nonlocal Main transfer propagates");
    }
    std::error_code error;
    std::filesystem::remove(root, error);
    check(!error, "remove only exclusively owned disposable fixture directory");
    std::cout << "native retail profile entry decision source fixture passed\n";
}
