#include "mka/retail_random.hpp"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

struct OracleRow {
    std::string case_name;
    std::string action;
    std::uint32_t argument{};
    std::uint32_t result{};
    std::uint32_t reference_cursor{};
    std::uint32_t reference_consumed{};
};

std::uint32_t decimal_word(const std::string& text) {
    if (text.empty()) throw std::runtime_error("empty oracle integer");
    std::size_t used = 0;
    const auto value = std::stoull(text, &used, 10);
    if (used != text.size() || value > std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("invalid oracle integer");
    return static_cast<std::uint32_t>(value);
}

std::vector<OracleRow> read_oracle(const char* path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open independent random oracle");
    std::string line;
    if (!std::getline(input, line) || line != "# native-retail-rng-oracle-v1" ||
        !std::getline(input, line) ||
        line != "case\taction\targument\tresult\tcursor\tconsumed")
        throw std::runtime_error("random oracle header mismatch");
    std::vector<OracleRow> rows;
    while (std::getline(input, line)) {
        if (line.empty()) throw std::runtime_error("empty random oracle row");
        std::istringstream stream(line);
        std::vector<std::string> fields;
        std::string field;
        while (std::getline(stream, field, '\t')) fields.push_back(field);
        if (fields.size() != 6 || fields[0].empty())
            throw std::runtime_error("random oracle row width/name mismatch");
        if (fields[1] != "SEED" && fields[1] != "NEXT" && fields[1] != "RANGE16")
            throw std::runtime_error("unknown random oracle action");
        OracleRow row{fields[0], fields[1], decimal_word(fields[2]),
            decimal_word(fields[3]), decimal_word(fields[4]), decimal_word(fields[5])};
        if (row.action == "SEED" && row.result != 0)
            throw std::runtime_error("seed oracle sentinel mismatch");
        if (row.action == "RANGE16" && row.result > 0xffffu)
            throw std::runtime_error("range oracle result width mismatch");
        rows.push_back(std::move(row));
    }
    if (!input.eof() || rows.empty())
        throw std::runtime_error("random oracle missing or unreadable rows");
    return rows;
}

void compare_oracle(const std::vector<OracleRow>& rows) {
    std::unique_ptr<mka::retail::NativeRetailRandom> random;
    std::string current_case;
    std::size_t checked = 0;
    std::size_t cases = 0, seeds = 0, draws = 0, ranges = 0;
    for (const auto& row : rows) {
        if (row.case_name != current_case) {
            if (row.case_name.empty()) throw std::runtime_error("empty random case");
            current_case = row.case_name;
            random = std::make_unique<mka::retail::NativeRetailRandom>();
            ++cases;
        }
        mka::retail::RetailRandomFailure failure;
        if (row.action == "SEED") {
            ++seeds;
            if (!random->seed(row.argument, failure))
                throw std::runtime_error("native random seed failed in " + current_case);
        } else if (row.action == "NEXT") {
            ++draws;
            const auto actual = random->next(failure);
            if (!actual || *actual != row.result)
                throw std::runtime_error("native random next mismatch in " + current_case +
                    " row " + std::to_string(checked));
        } else {
            ++ranges;
            const auto actual = random->low16_range(row.argument, failure);
            if (!actual || *actual != row.result)
                throw std::runtime_error("native random range mismatch in " + current_case +
                    " row " + std::to_string(checked));
        }
        if (failure.kind != mka::retail::RetailRandomError::none)
            throw std::runtime_error("native random left a failure after success");
        ++checked;
    }
    if (cases < 2 || seeds < 2 || draws <= 624 || ranges < 2)
        throw std::runtime_error("independent random oracle lacks boundary coverage");
    std::cout << "independent random oracle rows: " << checked << '\n';
}

void check_wrong_thread() {
    mka::retail::NativeRetailRandom random;
    bool rejected_seed = false;
    bool rejected_next = false;
    bool rejected_range = false;
    std::thread other([&] {
        mka::retail::RetailRandomFailure failure;
        rejected_seed = !random.seed(1, failure) &&
            failure.kind == mka::retail::RetailRandomError::wrong_thread;
        rejected_next = !random.next(failure).has_value() &&
            failure.kind == mka::retail::RetailRandomError::wrong_thread;
        rejected_range = !random.low16_range(56, failure).has_value() &&
            failure.kind == mka::retail::RetailRandomError::wrong_thread;
    });
    other.join();
    if (!rejected_seed || !rejected_next || !rejected_range)
        throw std::runtime_error("foreign random access accepted");
    mka::retail::RetailRandomFailure failure{
        mka::retail::RetailRandomError::wrong_thread, "stale"};
    const auto first = random.next(failure);
    if (!first || *first != 0 || failure.kind != mka::retail::RetailRandomError::none)
        throw std::runtime_error("foreign random access changed zero-born state");
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("expected frozen oracle TSV path");
        compare_oracle(read_oracle(argv[1]));
        check_wrong_thread();
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
