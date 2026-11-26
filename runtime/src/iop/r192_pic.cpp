#include "r192_pic.hpp"

namespace ps2::iop::r192_aot {
bool BoundPic::bind(const PicTemplate &source, std::uint32_t base) {
    module = {};
    words.clear();
    if (!source.name || !source.body || !source.original_words || !source.text_size ||
        (source.text_size & 3u) || source.text_size > source.total_size ||
        (base & 0xFFu) || base < 0x20000u || base >= kRamSize || source.total_size > kRamSize - base ||
        (source.relocation_count && !source.relocations)) return false;
    words.assign(source.original_words, source.original_words + source.text_size / 4);
    std::vector<bool> seen(words.size());
    for (std::uint32_t i = 0; i < source.relocation_count; ++i) {
        const auto &r = source.relocations[i];
        if (r.offset >= source.text_size) continue; // Data/BSS is relocated by the real loader.
        if ((r.offset & 3u) || seen[r.offset / 4] || words[r.offset / 4] != r.original_word) return false;
        seen[r.offset / 4] = true;
        const auto value = base + r.addend;
        auto &word = words[r.offset / 4];
        switch (r.type) {
        case 2: word += base; break;
        case 4:
            if ((word >> 26) != 2u && (word >> 26) != 3u) return false;
            if (r.addend >= source.text_size || (r.addend & 3u)) return false;
            word = (word & 0xFC000000u) | ((value >> 2) & 0x03FFFFFFu);
            break;
        case 5:
            if ((word >> 26) != 15u) return false;
            word = (word & 0xFFFF0000u) | (((value + 0x8000u) >> 16) & 0xFFFFu);
            break;
        case 6:
            word = (word & 0xFFFF0000u) | (value & 0xFFFFu);
            break;
        default: return false;
        }
    }
    module = {source.name, base, source.text_size, source.total_size, words.data(),
              source.imports, source.import_count, nullptr, source.body};
    return true;
}
} // namespace ps2::iop::r192_aot
