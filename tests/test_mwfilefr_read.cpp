#include "iop/mwfilefr.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

int main() {
    const auto name = "mwfilefr-read-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / name;
    std::vector<std::uint8_t> expected(90000), actual(100000);
    for (std::size_t i = 0; i < expected.size(); ++i) expected[i] = static_cast<std::uint8_t>(i * 17u);
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char *>(expected.data()), expected.size());
        if (!file) return 1;
    }
    std::uint32_t got = 0;
    using ps2::iop::kernel::mwfilefr_read_host_range;
    bool ok = mwfilefr_read_host_range(path.string(), expected.size(), 0, actual.data(), actual.size(), got) &&
              got == expected.size() && std::equal(expected.begin(), expected.end(), actual.begin());
    ok = ok && mwfilefr_read_host_range(path.string(), expected.size(), expected.size(), actual.data(), 1, got) && got == 0;
    std::filesystem::resize_file(path, 40000);
    ok = ok && !mwfilefr_read_host_range(path.string(), expected.size(), 0, actual.data(), expected.size(), got) && got == 40000;
    std::error_code ec;
    std::filesystem::remove(path, ec);
    if (!ok || ec) std::cerr << "MWFILEFR host range read failed\n";
    return ok && !ec ? 0 : 1;
}
