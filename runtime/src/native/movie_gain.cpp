#include "mka/movie_gain.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <limits>

namespace mka::video {
namespace {
struct GainRow {
    std::uint32_t category, base_bits;
    std::array<std::int32_t, 5> tenth_db;
};

// Mandatory generated input. No empty/default/conditional dataset is valid.
// Schema: category0..100 = UI percent;101 = source literal default0.75.
// Five values = initial,fade1,fade2,fade3,fade4 (total4).
#define MKA_MOVIE_GAIN_ROW(category, bits, initial, one, two, three, four) \
    GainRow{category, bits, {initial, one, two, three, four}},
constexpr GainRow rows[] = {
#include "generated/movie_gain_rows.inc"
};
#undef MKA_MOVIE_GAIN_ROW

struct GainInterval {
    std::uint32_t first, last;
    std::array<std::int32_t, 5> tenth_db;
};
// Mandatory full finite-magnitude dataset. Its final value includes the
// reviewed native-wide cancellation extension, not legacy overflow behavior.
#define MKA_MOVIE_GAIN_INTERVAL(first, last, initial, one, two, three, four) \
    GainInterval{first, last, {initial, one, two, three, four}},
constexpr GainInterval intervals[] = {
#include "generated/movie_gain_intervals.inc"
};
#undef MKA_MOVIE_GAIN_INTERVAL

constexpr bool valid_intervals() {
    if (std::size(intervals) == 0) return false;
    std::uint32_t next = 0;
    for (const auto& interval : intervals) {
        if (interval.first != next || interval.first > interval.last ||
            interval.last > 0x7f7fffffu) return false;
        for (auto value : interval.tenth_db)
            if (value < -960 || value > 0) return false;
        if (interval.tenth_db[4] != -960) return false;
        next = interval.last + 1u;
    }
    return next == 0x7f800000u;
}
static_assert(valid_intervals(),
              "Movie gain requires full contiguous finite magnitude coverage and native final mute");

constexpr const GainInterval* magnitude_interval(std::uint32_t magnitude) {
    std::size_t begin = 0, end = std::size(intervals);
    while (begin < end) {
        const auto middle = begin + (end - begin) / 2;
        const auto& interval = intervals[middle];
        if (magnitude < interval.first) end = middle;
        else if (magnitude > interval.last) begin = middle + 1;
        else return &interval;
    }
    return nullptr;
}

constexpr bool valid_rows() {
    if (std::size(rows) != 102) return false;
    std::array<bool, 102> seen{};
    for (const auto& row : rows) {
        if (row.category >= seen.size() || seen[row.category]) return false;
        if ((row.base_bits & 0x80000000u) ||
            (row.base_bits & 0x7f800000u) == 0x7f800000u) return false;
        seen[row.category] = true;
        for (auto value : row.tenth_db)
            if (value < -960 || value > 0) return false;
        for (const auto& other : rows)
            if (row.base_bits == other.base_bits && row.tenth_db != other.tenth_db)
                return false;
    }
    for (bool present : seen) if (!present) return false;
    return true;
}
static_assert(sizeof(float) == sizeof(std::uint32_t) &&
              std::numeric_limits<float>::is_iec559,
              "Movie gain requires exact binary32 base identity");
static_assert(valid_rows(),
              "Movie gain requires102 unique categories0..101, valid tenths-dB and agreeing duplicate bases");
constexpr bool source_categories_agree() {
    for (const auto& row : rows) {
        const auto* interval = magnitude_interval(row.base_bits);
        if (!interval || row.tenth_db != interval->tenth_db) return false;
    }
    return true;
}
static_assert(source_categories_agree(),
              "Full native gain intervals must preserve all five source UI/default values");

constexpr const GainRow* category(std::uint32_t key) {
    for (const auto& row : rows) if (row.category == key) return &row;
    // Unreachable after the compile-time complete-key validation.
    return nullptr;
}
static_assert(category(101)->base_bits == 0x3f400000u,
              "Source default must retain literal0.75 bits");
static_assert(category(0)->base_bits == 0u,
              "Source zero category must retain literal positive zero");

const GainInterval* finite(float base) noexcept {
    const auto magnitude = std::bit_cast<std::uint32_t>(base) & 0x7fffffffu;
    if (magnitude >= 0x7f800000u) return nullptr;
    return magnitude_interval(magnitude);
}
} // namespace

float movie_gain_ui_base(std::int64_t percent) noexcept {
    const auto key = percent < 0 ? 0u : percent > 100 ? 100u :
                     static_cast<std::uint32_t>(percent);
    return std::bit_cast<float>(category(key)->base_bits);
}
float movie_gain_default_base() noexcept {
    return std::bit_cast<float>(category(101)->base_bits);
}
std::optional<std::int32_t> movie_gain_initial(float base,
                                             MovieGainFailure& failure) noexcept {
    failure = {};
    const auto* row = finite(base);
    if (!row) {
        failure = {MovieGainError::unsupported_base, "movie initial nonfinite base"};
        return std::nullopt;
    }
    return row->tenth_db[0];
}
std::optional<std::int32_t> movie_gain_fade(const MovieFadeStep& step,
                                          MovieGainFailure& failure) noexcept {
    failure = {};
    if (step.total != 4) {
        failure = {MovieGainError::unsupported_fade_total, "movie source fade total4"};
        return std::nullopt;
    }
    if (step.step < 1 || step.step > 4) {
        failure = {MovieGainError::unsupported_fade_step, "movie source fade steps1..4"};
        return std::nullopt;
    }
    const auto* row = finite(step.current_base_volume);
    if (!row) {
        failure = {MovieGainError::unsupported_base, "movie fade nonfinite base"};
        return std::nullopt;
    }
    return row->tenth_db[static_cast<std::size_t>(step.step)];
}
const char* movie_gain_error_name(MovieGainError error) noexcept {
    switch (error) {
    case MovieGainError::none: return "none";
    case MovieGainError::unsupported_base: return "unsupported_base";
    case MovieGainError::unsupported_fade_total: return "unsupported_fade_total";
    case MovieGainError::unsupported_fade_step: return "unsupported_fade_step";
    }
    return "unknown_movie_gain_error";
}
} // namespace mka::video
