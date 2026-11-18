#pragma once

// Native host MPEG-2 VLC decoding tables, built from the ISO/IEC 13818-2
// standard. These lookup structures are used by the native decoder to
// efficiently decode MPEG-2 video streams with zero IPU emulation.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "mpeg2_tables.hpp"

namespace ps2::media::vlc {

struct Entry {
    std::uint8_t length = 0;
    std::uint32_t value = 0;
};

struct Lookup {
    unsigned bits = 0;
    std::vector<Entry> entries;
};

struct DctEntry {
    std::uint8_t length = 0;
    std::uint8_t run = 0;
    std::int16_t level = 0;
};

struct DctLookup {
    unsigned bits = 0;
    std::vector<DctEntry> entries;
};

template <std::size_t N>
Lookup build(const tables::Vlc (&table)[N]) {
    Lookup l;
    for (const tables::Vlc &e : table) l.bits = std::max<unsigned>(l.bits, e.length);
    l.entries.resize(std::size_t{1} << l.bits);
    for (const tables::Vlc &e : table) {
        const unsigned shift = l.bits - e.length;
        for (std::uint32_t i = 0; i < (1u << shift); ++i)
            l.entries[(static_cast<std::uint32_t>(e.code) << shift) | i] = {e.length, e.value};
    }
    return l;
}

template <std::size_t N>
DctLookup build(const tables::Dct (&table)[N]) {
    DctLookup l;
    for (const tables::Dct &e : table) l.bits = std::max<unsigned>(l.bits, e.length);
    l.entries.resize(std::size_t{1} << l.bits);
    for (const tables::Dct &e : table) {
        const unsigned shift = l.bits - e.length;
        for (std::uint32_t i = 0; i < (1u << shift); ++i)
            l.entries[(static_cast<std::uint32_t>(e.code) << shift) | i] = {e.length, e.run, e.level};
    }
    return l;
}

// Every table the decoder needs, built once.
struct Tables {
    Lookup mbai = build(tables::kMbAddressIncrement);
    Lookup mb_i = build(tables::kMbTypeI);
    Lookup mb_p = build(tables::kMbTypeP);
    Lookup mb_b = build(tables::kMbTypeB);
    Lookup motion = build(tables::kMotionCode);
    Lookup dmv = build(tables::kDmVector);
    Lookup dc_luma = build(tables::kDcSizeLuma);
    Lookup dc_chroma = build(tables::kDcSizeChroma);
    Lookup cbp = build(tables::kCodedBlockPattern);
    DctLookup dct0 = build(tables::kDctTable0);
    DctLookup dct1 = build(tables::kDctTable1);
    double cosines[8][8];

    Tables() {
        for (int x = 0; x < 8; ++x)
            for (int u = 0; u < 8; ++u)
                cosines[x][u] = (u ? 0.5 : 0.5 / std::sqrt(2.0)) *
                                std::cos((2 * x + 1) * u * 3.14159265358979323846 / 16.0);
    }
};

inline const Tables &tables() {
    static const Tables t;
    return t;
}

}  // namespace ps2::media::vlc
