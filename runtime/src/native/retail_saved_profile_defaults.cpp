#include "mka/retail_saved_profile_defaults.hpp"

#include <algorithm>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {

// The first word of each of the sixteen 12-byte records at 5BE100.
constexpr std::array<std::uint32_t, 16> action_masks{
    0x0001, 0x0002, 0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080,
    0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000
};

constexpr const char* empty_names[5]{
    "EMPTY", "VAC\xcdO", "LEER", "VIDE", "VUOTO"
};
constexpr const char* encoded_commands[6]{
    ">,@_}", "&,&_{", "@,&_+", ">,$_=", ">,@,&_=", "|"
};

void fail(RetailSavedProfileDefaultFailure& failure,
          RetailSavedProfileDefaultError kind, const char* operation) noexcept {
    failure.kind = kind;
    failure.operation = operation;
}

} // namespace

bool decode_retail_saved_profile_command_from_507f60(
    std::string_view encoded, RetailSavedProfileCommandDefault& out,
    RetailSavedProfileDefaultFailure& failure) noexcept {
    failure = {};
    out.input_codes.clear();
    try {
        for (const unsigned char c : encoded) {
            if (c == 0) break;
            switch (c) {
            case '|': out.modifier = -2; break;
            case '=': out.modifier = 3; break;
            case '{': out.modifier = 2; break;
            case '+': out.modifier = 1; break;
            case '}': out.modifier = 0; break;
            case 'A': out.input_codes.push_back(0x80000008u); break;
            case '$': out.input_codes.push_back(0x80000003u); break;
            case '&': out.input_codes.push_back(0x80000001u); break;
            case '>': out.input_codes.push_back(0x80000004u); break;
            case '@': out.input_codes.push_back(0x80000002u); break;
            default: break;
            }
        }
        out.input_codes.push_back(0xffffffffu);
        return true;
    } catch (const std::bad_alloc&) {
        fail(failure, RetailSavedProfileDefaultError::allocation, "507f60_codes");
    } catch (const std::length_error&) {
        fail(failure, RetailSavedProfileDefaultError::allocation, "507f60_codes");
    } catch (...) {
        fail(failure, RetailSavedProfileDefaultError::native_exception, "507f60_codes");
    }
    return false;
}

bool initialize_retail_saved_profile_known_defaults_from_245cb0(
    RetailSavedProfileDefaults& out, std::int32_t language,
    std::uint32_t& shared_7bfc80,
    RetailSavedProfileDefaultFailure& failure) noexcept {
    failure = {};
    // 245CB0 clears the child first and sets its first byte before the name.
    out = RetailSavedProfileDefaults{};
    out.initialized = true;
    if (language == 5) language = 0;
    const char* name = language >= 0 && language < 5
        ? empty_names[language] : " ";
    try {
        out.empty_name = name;
    } catch (const std::bad_alloc&) {
        fail(failure, RetailSavedProfileDefaultError::allocation, "245cb0_name");
        return false;
    } catch (...) {
        fail(failure, RetailSavedProfileDefaultError::native_exception, "245cb0_name");
        return false;
    }

    // Each source mask row is copied into two tables before 514D30.
    out.first_action_masks = action_masks;
    out.second_action_masks = action_masks;

    // 514D30 zeroes its gameplay region, writes its scalar defaults and then
    // resets this retained global word before 4F3C80 begins.
    out.gameplay = RetailSavedProfileGameplayDefaults{};
    shared_7bfc80 = 0;

    // 4F3C80 clears the command region, publishes four copies of the exact
    // 647020 literal, writes its scalar defaults, then decodes all six rows.
    out.command = RetailSavedProfileCommandDefaults{};
    try {
        for (auto& label : out.command.labels) label = "BUG THIS";
        for (std::size_t i = 0; i < out.command.commands.size(); ++i) {
            if (!decode_retail_saved_profile_command_from_507f60(
                    encoded_commands[i], out.command.commands[i], failure)) {
                return false;
            }
        }
    } catch (const std::bad_alloc&) {
        fail(failure, RetailSavedProfileDefaultError::allocation, "4f3c80_labels");
        return false;
    } catch (...) {
        fail(failure, RetailSavedProfileDefaultError::native_exception, "4f3c80_labels");
        return false;
    }

    // Final two source byte spans are 40 and 30 signed 32-bit -1 entries.
    out.first_unassigned_indices.fill(-1);
    out.second_unassigned_indices.fill(-1);
    return true;
}

} // namespace mka::retail
