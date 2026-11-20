#include "mka/geometry_companion_quantizer.hpp"
namespace mka::game {
std::uint8_t quantize_geometry_companion_component(MaterialScalarWord input) noexcept {
    const auto x=input.bits;
    const auto exponent=(x>>23)&255u;
    if(!exponent)return 0; // Source flushes signed zero and denormals.
    const auto magnitude=(x&0x7fffffu)|0x800000u;
    // Literal127 is FIRST; coordinate is SECOND. Preserve ordered multiplier
    // correction before the source24-bit product store.
    std::uint64_t product=std::uint64_t(0xfe0000u)*magnitude;
    product-=(magnitude>>1)&1u;
    int stored_exponent=static_cast<int>(exponent)+6;
    if(product>>47){product>>=24;++stored_exponent;}else product>>=23;
    // Stored overflow saturates the project source word. Conversion of that
    // extended word saturates to signed32, whose LOW byte is required here.
    if(stored_exponent>=158)return (x&0x80000000u)?0u:255u;
    if(stored_exponent<=126)return 0;
    const int shift=stored_exponent-150;
    const auto m=static_cast<std::uint32_t>(product);
    const auto whole=shift>=0?m<<shift:m>>-shift;
    const auto signed_word=(x&0x80000000u)?0u-whole:whole;
    return static_cast<std::uint8_t>(signed_word&255u);
}
}
