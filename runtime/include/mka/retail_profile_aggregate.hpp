#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <thread>

namespace mka::retail {

enum class RetailProfileAggregateError {
    none, wrong_thread, missing_parent, missing_children, uninitialized
};
struct RetailProfileAggregateFailure {
    RetailProfileAggregateError kind{RetailProfileAggregateError::none};
};

// Typed fields read by 2432A0. These are game values, not a 3C30-byte guest
// record. A caller can retain more native profiles; this source loop reads
// the first eight children of each of its first two parent groups.
struct RetailProfileAggregateChildView {
    std::uint32_t gate_word_3c28{};
    std::uint32_t or_word_fc{};
    std::array<std::uint64_t, 4> or_words_100_108_110_118{};
    std::array<std::uint32_t, 40> and_words_120{};
    std::array<std::uint32_t, 30> and_words_1c0{};
};
struct RetailProfileAggregateParentView {
    std::int32_t status_word{};
    std::uint32_t version_word{};
    std::span<const RetailProfileAggregateChildView> children;
};

struct RetailProfileAggregateData {
    std::uint32_t or_word_660500{};
    std::array<std::uint64_t, 4> or_words_660508_510_518_520{};
    std::array<std::uint32_t, 30> and_words_660528{};
    std::array<std::uint32_t, 40> and_words_6605a0{};
};

// Source 245410 accepts signed ordinal -1/2+ and returns -1 for both.
// Missing native parent storage is a separate named failure.
std::optional<std::int32_t> classify_retail_profile_parent_from_245410(
    std::int32_t ordinal,
    std::span<const RetailProfileAggregateParentView> parents,
    RetailProfileAggregateFailure&) noexcept;

// Data-only owner for 2432A0 up to, but excluding, its 1888C0 call.
// The caller supplies the current 64E318 word every time; process BSS zero
// at birth is not a permanent constant or a discovered producer.
class NativeRetailProfileAggregate final {
public:
    NativeRetailProfileAggregate() noexcept : owner_(std::this_thread::get_id()) {}
    NativeRetailProfileAggregate(const NativeRetailProfileAggregate&) = delete;
    NativeRetailProfileAggregate& operator=(const NativeRetailProfileAggregate&) = delete;

    bool refresh_data_from_2432a0(
        std::span<const RetailProfileAggregateParentView> parents,
        std::uint64_t live_word_64e318,
        RetailProfileAggregateFailure&) noexcept;
    std::optional<RetailProfileAggregateData> observe(
        RetailProfileAggregateFailure&) const noexcept;

private:
    std::thread::id owner_;
    RetailProfileAggregateData data_{};
    bool has_data_{};
};

} // namespace mka::retail
