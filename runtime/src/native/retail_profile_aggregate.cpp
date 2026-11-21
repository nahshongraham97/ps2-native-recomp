#include "mka/retail_profile_aggregate.hpp"

#include <algorithm>

namespace mka::retail {

std::optional<std::int32_t> classify_retail_profile_parent_from_245410(
    std::int32_t ordinal,
    std::span<const RetailProfileAggregateParentView> parents,
    RetailProfileAggregateFailure& failure) noexcept {
    failure = {};
    if (ordinal < 0 || ordinal >= 2) return -1;
    if (parents.size() <= static_cast<std::size_t>(ordinal)) {
        failure.kind = RetailProfileAggregateError::missing_parent;
        return {};
    }
    const auto& parent = parents[static_cast<std::size_t>(ordinal)];
    const auto status = parent.status_word;
    if (status == 7) return 5;                    // 1709B0
    if (status != 0 && status != 2 && status != 5 &&
        status != 4 && status != 3) return 1;     // 170B70
    if (status == 3 || status == 4) return 4;     // 170A20
    if (status == 5) return 2;                    // 170B00
    if (status != 2) return 0;
    return parent.version_word < 0xaeu ? 2 : 3; // 170BE0: SLTIU
}

bool NativeRetailProfileAggregate::refresh_data_from_2432a0(
    std::span<const RetailProfileAggregateParentView> parents,
    std::uint64_t live_word_64e318,
    RetailProfileAggregateFailure& failure) noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailProfileAggregateError::wrong_thread;
        return false;
    }
    if (parents.size() < 2) {
        failure.kind = RetailProfileAggregateError::missing_parent;
        return false;
    }

    RetailProfileAggregateData next{};
    // Source 2432D4..243304: three live 64-bit reads/stores, then zero words.
    next.or_words_660508_510_518_520 = {
        0x5ffffffbffffff77ull, live_word_64e318,
        0x00000000001f07ffull, 0ull
    };
    // Source fills 6605A0 first, then overlapping 660528, both with FF.
    next.and_words_6605a0.fill(0xffffffffu);
    next.and_words_660528.fill(0xffffffffu);

    for (std::int32_t ordinal = 0; ordinal < 2; ++ordinal) {
        const auto classified = classify_retail_profile_parent_from_245410(
            ordinal, parents, failure);
        if (!classified) return false;
        if (*classified != 0) continue;
        const auto& children = parents[static_cast<std::size_t>(ordinal)].children;
        if (children.size() < 8) {
            failure.kind = RetailProfileAggregateError::missing_children;
            return false;
        }
        for (std::size_t child_index = 0; child_index < 8; ++child_index) {
            const auto& child = children[child_index];
            if (child.gate_word_3c28 == 0) continue;
            for (std::size_t i = 0; i < 4; ++i)
                next.or_words_660508_510_518_520[i] |=
                    child.or_words_100_108_110_118[i];
            next.or_word_660500 |= child.or_word_fc;
            for (std::size_t i = 0; i < next.and_words_660528.size(); ++i)
                next.and_words_660528[i] &= child.and_words_1c0[i];
            for (std::size_t i = 0; i < next.and_words_6605a0.size(); ++i)
                next.and_words_6605a0[i] &= child.and_words_120[i];
        }
    }
    data_ = next;
    has_data_ = true;
    return true;
}

std::optional<RetailProfileAggregateData> NativeRetailProfileAggregate::observe(
    RetailProfileAggregateFailure& failure) const noexcept {
    failure = {};
    if (std::this_thread::get_id() != owner_) {
        failure.kind = RetailProfileAggregateError::wrong_thread;
        return {};
    }
    if (!has_data_) {
        failure.kind = RetailProfileAggregateError::uninitialized;
        return {};
    }
    return data_;
}

} // namespace mka::retail
