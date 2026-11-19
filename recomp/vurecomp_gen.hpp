#pragma once

// Offline-only vector unit code generator. No runtime compilation or interpreter fallback.

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ps2::vurecomp {

// One program to compile: its key and entry (the file name of a recording is %016llX_%04X.bin) and the whole 16 KiB of
// vector unit code memory it was recorded from.
struct Input {
    std::uint64_t key = 0;
    std::uint32_t entry = 0;
    std::vector<std::uint8_t> code;
};

struct Result {
    std::size_t programs = 0;      // programs written, jump targets included
    std::size_t jump_targets = 0;  // of them, programs that no input named but an input jumps to
    std::size_t skipped = 0;       // omitted from the static C++ table (unsupported op or pipeline analysis failure)
    std::vector<std::uint64_t> keys;
};

// Writes static C++ and its registry only when all inputs and discovered targets translate.
// On rejection existing output is untouched; Result::skipped is nonzero.
// live_stamp retains the historical loadable-table output ABI for offline callers only.
// with_targets adds statically discoverable continuation entries.
// require_complete is compatibility-only: passing false does not permit partial output.
Result generate_files(std::vector<Input> inputs, const std::filesystem::path &out_dir, std::uint64_t live_stamp, std::size_t per_file = 16,
                      bool with_targets = true, bool require_complete = true);

// The fingerprint a loadable library carries with its programs, so a library made by another version of the generator, the
// headers or the compiler flags is not loaded again (vu_live.cpp mixes it into the stamp).
extern const char *const kGeneratorVersion;

}  // namespace ps2::vurecomp
