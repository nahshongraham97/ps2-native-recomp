#include "mka/retail_profile_preferences.hpp"
#include "mka/retail_game_records.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

#include <array>
#include <bit>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <thread>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
constexpr mka::retail::RetailProfilePreferenceImage original{
    0,0,0x40,0x3f, 0,0,0x40,0x3f, 0,0,0x40,0x3f, 0,0,0x40,0x3f,
    0,0,0x40,0x3f, 0,0,0x80,0x3f, 2,0,0,0, 2,0,0,0,
    2,0,0,0, 90,0,0,0, 3,0,0,0, 1,0,0,0,
    0,0,0,0, 0,0,0,0, 0,0,0,0, 50,0,0,0,
    50,0,0,0, 50,0,0,0, 50,0,0,0, 50,0,0,0,
    85,0,0,0, 0xff,0xff,0xff,0xff
};
}
int main(int argc, char** argv) {
    try {
        using namespace mka::retail;
        check(argc == 2, "existing read-only content root");
        const auto defaults = retail_profile_preference_defaults_from_2955d0();
        check(encode_retail_profile_preferences(defaults) == original,
            "all88 encoded default bytes match the original5BE650 fixture");
        check(defaults.levels == std::array<float,5>{.75f,.75f,.75f,.75f,.75f} &&
            defaults.word_14 == 0x3f800000u, "five proved float values and unguessed sixth bitword");
        auto encoded = original;
        constexpr std::array<std::uint32_t,5> float_bits{
            0x80000000, 0x7fc01234, 0xff800000, 0x00000001, 0x3f800000};
        for (std::size_t i = 0; i < 5; ++i)
            for (std::size_t byte = 0; byte < 4; ++byte)
                encoded[i*4+byte] = static_cast<std::uint8_t>(float_bits[i] >> (byte*8));
        for (std::size_t i = 20; i < encoded.size(); ++i)
            encoded[i] = static_cast<std::uint8_t>(i*17u + 3u);
        const auto decoded = decode_retail_profile_preferences(encoded);
        check(encode_retail_profile_preferences(decoded) == encoded,
            "external encoding preserves negative zero, NaN payload, infinity, denormal and all scalar bits");
        mka::content::ContentVolumeFailure cf;
        auto volume = mka::content::NativeContentVolume::open(std::filesystem::u8path(argv[1]), cf);
        check(bool(volume), "real native content volume");
        NativeRetailStartupState startup(std::move(*volume), {});
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup, samples);
        StartupFailure failure;
        check(!main_binding::initialize_retail_current_profile_preferences(main, failure) &&
            failure.kind == StartupError::game_session, "missing real Session refuses initialization");
        check(initialize_native_retail_game_session(startup, failure), "actual persistent Session");
        RetailGameSessionFailure sf;
        auto* words = startup.game_session->game_records_values(sf);
        check(words && encode_retail_profile_preferences(words->profile_preferences) ==
            RetailProfilePreferenceImage{}, "current native preference values have source BSS zero birth");
        words->profile_preferences = decoded;
        words->word_64eac4 = 73;
        words->word_64eac0 = 21;
        check(main_binding::initialize_retail_current_profile_preferences(main, failure) &&
            encode_retail_profile_preferences(words->profile_preferences) == original &&
            words->word_64eac4 == 0 && words->word_64eac0 == 0xffffffffu,
            "actual Main295660 publishes defaults and both ordered source words");
        words->profile_preferences = decoded;
        const auto before = encode_retail_profile_preferences(words->profile_preferences);
        RetailGameRecordsFailure rf;
        check(clear_retail_game_records_pair_1303f0(*startup.game_session, rf),
            "earlier1303F0 clear on the same current preference owner");
        const auto after = encode_retail_profile_preferences(words->profile_preferences);
        for (std::size_t i = 0; i < after.size(); ++i)
            check(after[i] == (i >= 0x34 && i < 0x3c ? 0 : before[i]),
                "clear updates only real6B6F94/98 aliases, no duplicated fields");
        bool foreign_refused = false;
        std::thread other([&] {
            StartupFailure f;
            foreign_refused = !main_binding::initialize_retail_current_profile_preferences(main, f) &&
                f.kind == StartupError::game_session && f.game.kind == RetailGameSessionError::wrong_thread;
        });
        other.join();
        check(foreign_refused && encode_retail_profile_preferences(words->profile_preferences) == after,
            "foreign thread cannot reset actual native current settings");
        check(main_binding::initialize_retail_current_profile_preferences(main, failure) &&
            encode_retail_profile_preferences(words->profile_preferences) == original,
            "native Main reinitialization resets the same retained preference object");
        std::puts("PASS actual Main current profile preferences, native alias lifetime and external encoding; full game open");
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "FAIL native profile preferences: %s\n", e.what());
        return 1;
    }
}
