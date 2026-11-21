#include "mka/retail_profile_entry_decision.hpp"
#include "mka/retail_main.hpp"
#include "retail_main_control.hpp"
#include "native_retail_startup.hpp"

#include <new>
#include <stdexcept>

namespace mka::retail {
namespace {
constexpr std::uint64_t baseline_64c438 = 0x5ffffffbffffff77ull;

std::optional<std::uint32_t> fail(RetailProfileMenuFailure& f,
    RetailProfileMenuError kind, const char* operation) noexcept {
    f.kind = kind;
    f.operation = operation;
    return std::nullopt;
}
} // namespace

std::optional<std::uint32_t> decide_retail_profile_entry_from_1892d0(
    NativeRetailMainState& main, std::uint32_t key, std::uint32_t selector,
    std::uint32_t role, RetailProfileMenuFailure& f) {
    f = {};
    if (!main.lifetime || !main.lifetime->active_on_owner_thread())
        return fail(f, RetailProfileMenuError::main_lifetime,
            "1892D0 Main owner thread/lifetime");
    // Original DSLL32/DSRA32 sign-extends A0 before unsigned SLTIU 0x40.
    // Every negative signed-32 key is already >=64 as this unsigned input.
    if (key >= 64u) return 1u;

    if (key == 62u) {
        if (role != 5u) {
            try {
                const auto permission = observe_retail_profile_permission_from_4eee60(
                    main, role, f);
                if (!permission) {
                    if (f.kind == RetailProfileMenuError::none)
                        return fail(f, RetailProfileMenuError::required_child,
                            "1892D0 required 4EEE60 profile/task child");
                    return std::nullopt;
                }
                if (*permission != 0u) return 1u;
            } catch (const detail::NativeMainReentry&) {
                throw;
            } catch (const std::bad_alloc&) {
                return fail(f, RetailProfileMenuError::allocation,
                    "1892D0 allocation after 4EEE60 effect");
            } catch (const std::length_error&) {
                return fail(f, RetailProfileMenuError::allocation,
                    "1892D0 capacity after 4EEE60 effect");
            } catch (...) {
                return fail(f, RetailProfileMenuError::native_exception_after_effect,
                    "1892D0 exception after 4EEE60 effect");
            }
        }
        if (selector != 0u) return 1u;
    }

    // A retained Session is the sole owner of both the 2432A0 aggregate and
    // the independently live 64E318 override. No baseline is cached here.
    const auto session = main.startup.game_session;
    if (!session)
        return fail(f, RetailProfileMenuError::session,
            "1892D0 retained game Session missing");
    auto* aggregate = session->profile_aggregate(f.session);
    if (!aggregate)
        return fail(f, RetailProfileMenuError::session,
            "1892D0 profile aggregate owner");

    std::uint64_t mask{};
    if (selector == 0u) {
        RetailProfileAggregateFailure aggregate_failure;
        const auto data = aggregate->observe(aggregate_failure);
        if (!data)
            return fail(f, RetailProfileMenuError::required_child,
                "1892D0 source 660508 aggregate not ready");
        mask = data->or_words_660508_510_518_520[0] | baseline_64c438;
    } else {
        // Original LD [GP-5DD8] precedes LD 660510. Read the live value
        // after the optional 4EEE60 child, even if 2432A0 ran earlier.
        const auto* words = session->game_records_values(f.session);
        if (!words)
            return fail(f, RetailProfileMenuError::session,
                "1892D0 live 64E318 game-record owner");
        const std::uint64_t override_word = words->word_64e318;
        RetailProfileAggregateFailure aggregate_failure;
        const auto data = aggregate->observe(aggregate_failure);
        if (!data)
            return fail(f, RetailProfileMenuError::required_child,
                "1892D0 source 660510 aggregate not ready");
        mask = data->or_words_660508_510_518_520[1] | override_word;
    }
    return (mask & (std::uint64_t{1} << key)) == 0u ? 1u : 0u;
}
} // namespace mka::retail
