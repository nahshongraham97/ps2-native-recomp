#include "mka/retail_saved_profile_defaults.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void require(bool condition, const char* what) {
    if (!condition) {
        std::cerr << "FAIL retail saved profile defaults: " << what << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    using namespace mka::retail;
    RetailSavedProfileDefaults value;
    RetailSavedProfileDefaultFailure failure;
    std::uint32_t shared_word = 0xa5a5a5a5u;

    // Source 263E40 category 1/index 0, at languages 0..4 and special 5.
    const std::array<std::string, 5> original_names{
        "EMPTY", "VAC\xcdO", "LEER", "VIDE", "VUOTO"
    };
    for (int language = 0; language < 5; ++language) {
        shared_word = 0xffffffffu;
        require(initialize_retail_saved_profile_known_defaults_from_245cb0(
            value, language, shared_word, failure), "valid language initializes");
        require(failure.kind == RetailSavedProfileDefaultError::none,
                "valid language failure clear");
        require(value.initialized && value.empty_name == original_names[language],
                "original language bytes");
        require(shared_word == 0, "514D30 shared-word reset");
    }
    require(initialize_retail_saved_profile_known_defaults_from_245cb0(
        value, 5, shared_word, failure) && value.empty_name == "EMPTY",
        "language five folds to zero");
    for (int language : {-1, 6, 100}) {
        require(initialize_retail_saved_profile_known_defaults_from_245cb0(
            value, language, shared_word, failure) && value.empty_name == " ",
            "source invalid-language one-space literal");
    }

    for (unsigned i = 0; i < 16; ++i) {
        const std::uint32_t original_5be100_mask = 1u << i;
        require(value.first_action_masks[i] == original_5be100_mask &&
                value.second_action_masks[i] == original_5be100_mask,
                "sixteen original strided masks in both tables");
    }
    require(value.gameplay.bitword_1164 == 18 &&
            value.gameplay.bitword_1168 == 1 &&
            value.gameplay.bitword_1188 == 4 &&
            value.gameplay.bitword_118c == 1 &&
            value.gameplay.bitword_1190 == 27 &&
            value.gameplay.bitword_1194 == 0xffffffffu &&
            value.gameplay.bitword_11a4 == 64 &&
            value.gameplay.bitword_11ac == 64 &&
            value.gameplay.bitword_11b0 == 64,
            "514D30 source bitword defaults and final overwrite");
    for (const auto& label : value.command.labels)
        require(label == "BUG THIS", "647020 literal is nonempty");
    require(value.command.bitword_1fc == 1 &&
            value.command.bitword_264 == 4 &&
            value.command.bitword_268 == 2 &&
            value.command.bitword_2d0 == 1 &&
            value.command.bitword_2d4 == 1 &&
            value.command.bitword_2e0 == 10 &&
            value.command.flag_bit2 == 4,
            "4F3C80 scalar defaults");

    const std::array<std::int32_t, 6> source_modifiers{0, 2, 1, 3, 3, -2};
    const std::array<std::vector<std::uint32_t>, 6> source_codes{{
        {0x80000004u, 0x80000002u, 0xffffffffu},
        {0x80000001u, 0x80000001u, 0xffffffffu},
        {0x80000002u, 0x80000001u, 0xffffffffu},
        {0x80000004u, 0x80000003u, 0xffffffffu},
        {0x80000004u, 0x80000002u, 0x80000001u, 0xffffffffu},
        {0xffffffffu}
    }};
    for (unsigned i = 0; i < 6; ++i) {
        require(value.command.commands[i].modifier == source_modifiers[i] &&
                value.command.commands[i].input_codes == source_codes[i],
                "six source-encoded commands, modifier and terminator");
    }
    for (auto entry : value.first_unassigned_indices)
        require(entry == -1, "first final -1 span");
    for (auto entry : value.second_unassigned_indices)
        require(entry == -1, "second final -1 span");

    RetailSavedProfileCommandDefault parsed;
    parsed.modifier = 31;
    require(decode_retail_saved_profile_command_from_507f60(
        "unknown_?!", parsed, failure) && parsed.modifier == 31 &&
        parsed.input_codes == std::vector<std::uint32_t>{0xffffffffu},
        "unknown punctuation preserves prior modifier and writes sentinel");
    require(decode_retail_saved_profile_command_from_507f60(
        std::string("@\0&", 3), parsed, failure) && parsed.modifier == 31 &&
        parsed.input_codes == std::vector<std::uint32_t>{
            0x80000002u, 0xffffffffu},
        "source parser stops at first NUL");
    require(decode_retail_saved_profile_command_from_507f60(
        "|+}&A", parsed, failure) && parsed.modifier == 0 &&
        parsed.input_codes == std::vector<std::uint32_t>{
            0x80000001u, 0x80000008u, 0xffffffffu},
        "modifier replacement and A token");

    std::cout << "PASS native retail saved profile default leaves; aggregate/menu and unknown profile consumers open\n";
}
