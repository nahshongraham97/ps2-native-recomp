#pragma once

// The variable length code tables of the MPEG-1/2 video standard, as the
// lookups the decoder reads them with.
//
// The tables themselves live in ipu/tables.hpp (the PS2's IPU used the same
// standard's codes). The lookups are here because two things read them: the
// IPU model (the chip's own decoder) and the native MPEG-2 decoder that
// replaces it on the accepted route.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "ipu/tables.hpp"

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
Lookup build(const ipu::tables::Vlc (&table)[N]) {
    Lookup l;
    for (const ipu::tables::Vlc &e : table) l.bits = std::max<unsigned>(l.bits, e.length);
    l.entries.resize(std::size_t{1} << l.bits);
    for (const ipu::tables::Vlc &e : table) {
        const unsigned shift = l.bits - e.length;
        for (std::uint32_t i = 0; i < (1u << shift); ++i)
            l.entries[(static_cast<std::uint32_t>(e.code) << shift) | i] = {e.length, e.value};
    }
    return l;
}

template <std::size_t N>
DctLookup build(const ipu::tables::Dct (&table)[N]) {
    DctLookup l;
    for (const ipu::tables::Dct &e : table) l.bits = std::max<unsigned>(l.bits, e.length);
    l.entries.resize(std::size_t{1} << l.bits);
    for (const ipu::tables::Dct &e : table) {
        const unsigned shift = l.bits - e.length;
        for (std::uint32_t i = 0; i < (1u << shift); ++i)
            l.entries[(static_cast<std::uint32_t>(e.code) << shift) | i] = {e.length, e.run, e.level};
    }
    return l;
}

// Every table the decoder needs, built once.
struct Tables {
    Lookup mbai = build(ipu::tables::kMbAddressIncrement);
    Lookup mb_i = build(ipu::tables::kMbTypeI);
    Lookup mb_p = build(ipu::tables::kMbTypeP);
    Lookup mb_b = build(ipu::tables::kMbTypeB);
    Lookup motion = build(ipu::tables::kMotionCode);
    Lookup dmv = build(ipu::tables::kDmVector);
    Lookup dc_luma = build(ipu::tables::kDcSizeLuma);
    Lookup dc_chroma = build(ipu::tables::kDcSizeChroma);
    Lookup cbp = build(ipu::tables::kCodedBlockPattern);
    DctLookup dct0 = build(ipu::tables::kDctTable0);
    DctLookup dct1 = build(ipu::tables::kDctTable1);
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
