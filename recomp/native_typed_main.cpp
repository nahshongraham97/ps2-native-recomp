#include "native_typed_codegen.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace {
std::string read(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot open native compiler input");
    std::string bytes{std::istreambuf_iterator<char>(file), {}};
    if (file.bad()) throw std::runtime_error("cannot read native compiler input");
    return bytes;
}
void publish(const fs::path& path, const std::string& bytes) {
    // A content-keyed output may already exist after an incremental rebuild.
    // Authenticate it instead of overwriting an unexpected generated file.
    if (fs::exists(path)) {
        if (read(path) != bytes)
            throw std::runtime_error("existing generated native source differs");
        return;
    }
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("cannot create generated native source");
    file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    file.close();
    if (!file) throw std::runtime_error("cannot finish generated native source");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 3) throw std::runtime_error("usage: native_typed_recomp ORIGINAL_ELF OUTPUT_DIRECTORY");
        const auto bytes = read(fs::path(argv[1]));
        const std::vector<std::uint8_t> elf(bytes.begin(), bytes.end());
        recomp::NativeTypedFailure failure;
        auto source = recomp::emit_native_typed_functions(
            elf, recomp::retail_scalar_mode_manifest(), failure);
        if (!source) {
            std::cerr << "native typed compilation refused: " << failure.operation << '\n';
            return 1;
        }
        const fs::path output(argv[2]);
        fs::create_directories(output);
        publish(output / "native_typed_retail.hpp", source->header);
        publish(output / "native_typed_retail.cpp", source->implementation);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "native typed compiler: " << error.what() << '\n';
        return 1;
    }
}
