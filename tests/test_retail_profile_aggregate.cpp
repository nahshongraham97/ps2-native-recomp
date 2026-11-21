#include "mka/retail_profile_aggregate.hpp"

#include <array>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using namespace mka::retail;

namespace {
void require(bool value, const char* message) {
    if (!value) { std::cerr << message << '\n'; std::abort(); }
}

RetailProfileAggregateChildView active_child() {
    RetailProfileAggregateChildView child;
    child.gate_word_3c28 = 1;
    child.and_words_120.fill(0xffffffffu);
    child.and_words_1c0.fill(0xffffffffu);
    return child;
}
}

int main() {
    RetailProfileAggregateFailure failure;
    std::vector<RetailProfileAggregateChildView> first(9), second(9);
    std::array<RetailProfileAggregateParentView, 3> parents{{
        {0, 0, first}, {0, 0, second}, {0, 0, {}}
    }};
    const auto classify = [&](std::int32_t status, std::uint32_t version = 0) {
        parents[0].status_word = status;
        parents[0].version_word = version;
        const auto result = classify_retail_profile_parent_from_245410(0, parents, failure);
        require(result && failure.kind == RetailProfileAggregateError::none,
                "status classification should be defined");
        return *result;
    };
    require(classify(-1) == 1 && classify(0) == 0 && classify(1) == 1 &&
        classify(2, 173) == 2 && classify(2, 174) == 3 &&
        classify(3) == 4 && classify(4) == 4 && classify(5) == 2 &&
        classify(6) == 1 && classify(7) == 5 && classify(8) == 1,
        "source status sets and unsigned version threshold");
    require(classify(2, 0xffffffffu) == 3, "unsigned version comparison");
    const auto invalid_low = classify_retail_profile_parent_from_245410(-1, {}, failure);
    const auto invalid_high = classify_retail_profile_parent_from_245410(2, {}, failure);
    require(invalid_low && *invalid_low == -1 && invalid_high && *invalid_high == -1,
        "source signed ordinal bounds before parent access");

    NativeRetailProfileAggregate aggregate;
    require(!aggregate.observe(failure) &&
        failure.kind == RetailProfileAggregateError::uninitialized,
        "uninitialized aggregate is not an implicit baseline");
    first[0] = active_child();
    first[0].or_word_fc = 1;
    first[0].or_words_100_108_110_118 = {
        0x2000000000000000ull, 0x40ull, 0x8000000000000000ull, 0x04ull
    };
    first[0].and_words_1c0[0] = 0xf0f0f0f0u;
    first[0].and_words_120[39] = 0xff00ff00u;
    first[1].or_word_fc = 0x40000000u; // Nonzero data cannot bypass zero gate.
    first[1].or_words_100_108_110_118[3] = 0x1000u;
    second[0] = active_child();
    second[0].or_word_fc = 2;
    second[0].or_words_100_108_110_118[1] = 0x80;
    second[0].or_words_100_108_110_118[3] = 0x10;
    second[0].and_words_1c0[0] = 0x0ff00ff0u;
    second[0].and_words_120[39] = 0x0f0f0f0fu;
    first[8] = active_child();
    first[8].or_word_fc = 0x80000000u; // Source scans eight, not host capacity.

    parents[0].status_word = 0;
    parents[1].status_word = 0;
    require(aggregate.refresh_data_from_2432a0(parents, 0x12ull, failure),
        "both admitted parents with live children");
    const auto both = aggregate.observe(failure);
    require(both && both->or_word_660500 == 3 &&
        both->or_words_660508_510_518_520[0] == 0x7ffffffbffffff77ull &&
        both->or_words_660508_510_518_520[1] == 0xd2ull &&
        both->or_words_660508_510_518_520[2] == 0x80000000001f07ffull &&
        both->or_words_660508_510_518_520[3] == 0x14ull,
        "source OR baselines, live word and child masks");
    require(both->and_words_660528[0] == 0x00f000f0u &&
        both->and_words_660528[29] == 0xffffffffu &&
        both->and_words_6605a0[39] == 0x0f000f00u &&
        both->and_words_6605a0[0] == 0xffffffffu,
        "source 30/40 AND spans and opposite endpoints");

    // First 171B10 call: first parent is -1, second may retain prior state.
    parents[0].status_word = -1;
    require(aggregate.refresh_data_from_2432a0(parents, 0x100ull, failure),
        "first partial-parent aggregate");
    const auto partial = aggregate.observe(failure);
    require(partial && partial->or_word_660500 == 2 &&
        partial->or_words_660508_510_518_520[1] == 0x180ull,
        "reentry observes prior second parent and rereads live baseline");
    // Second call: both new parents are -1; only baseline data survives.
    parents[1].status_word = -1;
    require(aggregate.refresh_data_from_2432a0(parents, 0x5ull, failure),
        "second partial-parent aggregate");
    const auto cleared = aggregate.observe(failure);
    require(cleared && cleared->or_word_660500 == 0 &&
        cleared->or_words_660508_510_518_520[1] == 5 &&
        cleared->and_words_660528[0] == 0xffffffffu,
        "repeat refresh replaces prior child aggregate");

    parents[0].status_word = 0;
    parents[0].children = std::span<const RetailProfileAggregateChildView>(first).first(7);
    require(!aggregate.refresh_data_from_2432a0(parents, 1, failure) &&
        failure.kind == RetailProfileAggregateError::missing_children,
        "admitted parent needs eight source children");
    require(aggregate.observe(failure)->or_words_660508_510_518_520[1] == 5,
        "malformed native view cannot publish partial data");
    parents[0].children = first;
    bool foreign_refused = false;
    std::thread other([&] {
        RetailProfileAggregateFailure other_failure;
        foreign_refused = !aggregate.refresh_data_from_2432a0(parents, 7, other_failure) &&
            other_failure.kind == RetailProfileAggregateError::wrong_thread &&
            !aggregate.observe(other_failure) &&
            other_failure.kind == RetailProfileAggregateError::wrong_thread;
    });
    other.join();
    require(foreign_refused && aggregate.observe(failure)->or_words_660508_510_518_520[1] == 5,
        "foreign thread cannot mutate aggregate");
    std::cout << "retail_profile_aggregate: passed\n";
}
