#include "vurecomp_gen.hpp"

#include "ps2/os.hpp"
#include "ps2/vu_ops.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <regex>
#include <exception>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

int main(int argc, char **argv) {
#ifdef _WIN32
    // argv is in the ANSI code page and loses every character outside it: take the UTF-16 command line and use it as UTF-8
    std::vector<std::string> utf8_args;
    std::vector<char *> utf8_argv;
    int wide_argc = 0;
    if (wchar_t **wide = CommandLineToArgvW(GetCommandLineW(), &wide_argc)) {
        for (int i = 0; i < wide_argc; ++i) utf8_args.push_back(ps2::os::utf8(wide[i]));
        LocalFree(wide);
        for (std::string &a : utf8_args) utf8_argv.push_back(a.data());
        utf8_argv.push_back(nullptr);
        argc = wide_argc;
        argv = utf8_argv.data();
    }
#endif
    try {
    // Offline identity inspection for provenance-pinned capture bytes. This
    // decodes reachability and hashes instructions; it never executes a program.
    if (argc > 1 && std::string_view(argv[1]) == "--inspect-program") {
        if (argc != 4) throw std::runtime_error("usage: vurecomp --inspect-program <code.bin> <entry-hex>");
        std::size_t consumed = 0;
        const auto value = std::stoul(argv[3], &consumed, 16);
        if (consumed != std::string_view(argv[3]).size() || value >= 16384 || (value & 7u))
            throw std::runtime_error("invalid VU capture entry");
        const auto path = ps2::os::path(argv[2]);
        if (fs::file_size(path) != 16384) throw std::runtime_error("VU capture must contain exactly 16384 bytes");
        std::vector<std::uint8_t> code(16384);
        std::ifstream input(path, std::ios::binary);
        input.read(reinterpret_cast<char*>(code.data()), static_cast<std::streamsize>(code.size()));
        if (!input || input.gcount() != 16384 || input.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("VU capture changed or could not be read");
        const auto entry = static_cast<std::uint32_t>(value);
        const auto pcs = ps2::vu::reachable(code.data(), 16383u, entry);
        const auto key = ps2::vu::program_key(code.data(), pcs, entry);
        std::printf("{\"entry\":%u,\"key\":\"%016llX\",\"reachable_instructions\":%zu,\"executed\":false}\n",
                    entry, static_cast<unsigned long long>(key), pcs.size());
        return 0;
    }
    bool require_complete = true;
    for (int i = 3; i < argc; ++i) {
        if (std::string_view(argv[i]) == "--require-complete") require_complete = true;
        else {
            std::fprintf(stderr, "usage: vurecomp <recorded programs dir> <out dir> [--require-complete]\n");
            return 1;
        }
    }
    if (argc < 3) {
        std::fprintf(stderr, "usage: vurecomp <recorded programs dir> <out dir> [--require-complete]\n");
        return 1;
    }
    const fs::path in_dir = ps2::os::path(argv[1]), out_dir = ps2::os::path(argv[2]);
    std::vector<ps2::vurecomp::Input> inputs;
    for (const auto &e : fs::directory_iterator(in_dir)) {
        if (e.path().extension() != ".bin") continue;
        unsigned long long key = 0;
        unsigned entry = 0;
        const std::string stem = ps2::os::utf8(e.path().stem());
        if (!std::regex_match(stem, std::regex("[0-9A-Fa-f]{16}_[0-9A-Fa-f]{4}")) ||
            std::sscanf(stem.c_str(), "%llx_%x", &key, &entry) != 2 || (entry & 7u) || entry >= 16384) {
            std::fprintf(stderr, "[vurecomp] malformed capture name/entry: %s\n", ps2::os::utf8(e.path()).c_str());
            return 2;
        }
        if (fs::file_size(e.path()) != 16384) {
            std::fprintf(stderr, "[vurecomp] capture must contain exactly 16384 bytes: %s\n", ps2::os::utf8(e.path()).c_str());
            return 2;
        }
        std::ifstream f(e.path(), std::ios::binary);
        ps2::vurecomp::Input in{key, entry, std::vector<std::uint8_t>(16384)};
        f.read(reinterpret_cast<char *>(in.code.data()), static_cast<std::streamsize>(in.code.size()));
        if (!f || f.gcount() != 16384 || f.peek() != std::char_traits<char>::eof()) {
            std::fprintf(stderr, "[vurecomp] capture read failed or size changed: %s\n", ps2::os::utf8(e.path()).c_str());
            return 2;
        }
        inputs.push_back(std::move(in));
    }
    if (require_complete && inputs.empty()) {
        std::fprintf(stderr, "[vurecomp] strict static output requires at least one recorded VU1 program\n");
        return 2;
    }
    const ps2::vurecomp::Result result = ps2::vurecomp::generate_files(std::move(inputs), out_dir, 0, 16, true, require_complete);
    std::printf("%zu VU1 programs, %zu jump targets, %zu omitted from generated C++\n",
                result.programs, result.jump_targets, result.skipped);
    if (require_complete && result.skipped != 0) return 2;
    return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[vurecomp] capture/output error: %s\n", e.what());
        return 2;
    }
}
