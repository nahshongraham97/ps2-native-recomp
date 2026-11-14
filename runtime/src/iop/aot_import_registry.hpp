#pragma once

#include "iop.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ps2::iop::aot_imports {
struct Module {
    const char *name;
    std::uint32_t base, text_size;
    std::uint64_t text_hash;
    bool (*dispatch)(Cpu &);
};

// Defined by the generated registry, which is linked only when AOT output is
// explicitly selected. The loader must validate every identity field first.
const Module *modules(std::size_t &count);

// A duplicate identity is ambiguous and must not select either bridge.
inline const Module *find(const Module *entries, std::size_t count, const char *name,
                          std::uint32_t base, std::uint32_t text_size,
                          std::uint64_t text_hash) {
    if (!entries || !name || !text_size) return nullptr;
    const Module *match = nullptr;
    for (std::size_t i = 0; i < count; ++i) {
        const Module &entry = entries[i];
        if (!entry.name || !entry.dispatch || std::strcmp(entry.name, name) != 0 ||
            entry.base != base || entry.text_size != text_size || entry.text_hash != text_hash)
            continue;
        if (match) return nullptr;
        match = &entry;
    }
    return match;
}
}
