#include "mka/adpcm_codec.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
namespace mka::sound {
namespace {
// Portable arithmetic right shift: floor division also for negative values.
std::int32_t floor_div(std::int32_t n, std::int32_t d) noexcept {
    return n >= 0 ? n/d : -((-n+d-1)/d);
}
}
AdpcmError decode_adpcm_block(const std::uint8_t* bytes, std::size_t size, AdpcmHistory& history, AdpcmBlock& out) noexcept {
    if (size != 16) return AdpcmError::truncated_block;
    if (!bytes) return AdpcmError::null_input;
    const unsigned filter = bytes[0] >> 4, shift = bytes[0] & 15;
    if (filter > 4) return AdpcmError::unsupported_filter;
    if (shift > 12) return AdpcmError::unsupported_shift;
    // Read-only playable backup spu2.cpp164..178; only codec arithmetic.
    static constexpr int coefficients[5][2] = {{0,0},{60,0},{115,-52},{98,-55},{122,-60}};
    auto next = history; AdpcmBlock result; result.flags = bytes[1];
    for (unsigned i=0;i<28;++i) {
        const int bits = (bytes[2+i/2] >> ((i&1)*4)) & 15;
        const int nibble = bits < 8 ? bits : bits-16;
        const auto predicted = floor_div(int(next.s1)*coefficients[filter][0] + int(next.s2)*coefficients[filter][1] + 32,64);
        const auto value = std::clamp(floor_div(nibble*4096,1<<shift)+predicted,-32768,32767);
        next.s2 = next.s1; next.s1 = static_cast<std::int16_t>(value); result.samples[i] = next.s1;
    }
    out = result; history = next; return AdpcmError::none;
}
std::optional<AdpcmMono> decode_adpcm_mono(const std::uint8_t* bytes, std::size_t size, AdpcmHistory& history, AdpcmError& error) {
    error = AdpcmError::none;
    if (size%16) { error=AdpcmError::truncated_block; return std::nullopt; }
    if (size && !bytes) { error=AdpcmError::null_input; return std::nullopt; }
    AdpcmMono result; const auto blocks = size/16;
    if (blocks>result.samples.max_size()/28 || blocks>result.block_flags.max_size()) { error=AdpcmError::host_capacity; return std::nullopt; }
    try { result.samples.resize(blocks*28); result.block_flags.resize(blocks); }
    catch (const std::bad_alloc&) { error=AdpcmError::allocation_failed; return std::nullopt; }
    catch (const std::length_error&) { error=AdpcmError::host_capacity; return std::nullopt; }
    auto next=history;
    for (std::size_t i=0;i<blocks;++i) {
        AdpcmBlock block; error=decode_adpcm_block(bytes+i*16,16,next,block);
        if (error!=AdpcmError::none) return std::nullopt;
        std::copy(block.samples.begin(),block.samples.end(),result.samples.begin()+i*28);
        result.block_flags[i]=block.flags;
    }
    history=next; return result;
}
}
