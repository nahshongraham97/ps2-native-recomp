#pragma once
#include "r192_aot.hpp"
#include <vector>
#include <span>

namespace ps2::iop::r192_aot {
// Compiled from the original ELF relocation section, never inferred by value.
struct Relocation {
    std::uint32_t offset, type, original_word, addend;
};
struct PicTemplate {
    const char *name;
    std::uint32_t text_size, total_size;
    const std::uint32_t *original_words;
    const Import *imports;
    std::uint32_t import_count;
    const Relocation *relocations;
    std::uint32_t relocation_count;
    PicBody body;
};
struct BoundPic {
    BoundPic() = default;
    BoundPic(const BoundPic &) = delete;
    BoundPic &operator=(const BoundPic &) = delete;
    BoundPic(BoundPic &&) = delete;
    BoundPic &operator=(BoundPic &&) = delete;
    std::vector<std::uint32_t> words;
    Module module{};
    // Do not move/copy the bound object while its module descriptor is in use.
    bool bind(const PicTemplate &, std::uint32_t base);
};
const PicTemplate &selected_pic_template();
// Defined only by a separately generated original-image registry. A filename
// alone or an unknown original digest never selects a body.
const PicTemplate *find_pic_template(const char *name, const char *original_sha256);
struct PicRegistryEntry {
    const char *name;
    const char *original_sha256;
    const PicTemplate &(*get)();
};
std::span<const PicRegistryEntry> pic_registry();
} // namespace ps2::iop::r192_aot
