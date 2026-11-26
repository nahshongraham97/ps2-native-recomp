#pragma once
// Side effects of the source-proved captured vector multiply/add family.
// Data results only: no Cpu, register bank, opcode stream or scheduler.
#include <array>
#include <cstdint>

namespace mka::emitted::semantic {
struct VectorEffects {
    using Block=std::array<std::uint32_t,4>;
    Block constant_product{},last_resource256{},last_resource257{};
    bool element_observed{};
    std::uint32_t last_mac{},last_flags{},raised_sticky{};

    // Both original operations cover all four lanes. These predicates are
    // derived from the result bits, including signed zero and saturated max.
    void arithmetic_result(const Block& value){
        std::uint32_t zeros{},signs{},overflows{};
        for(unsigned lane=0;lane<4;++lane){
            const auto word=value[lane],bit=1u<<(3u-lane);
            if(!(word&0x7f800000u))zeros|=bit;
            if(word&0x80000000u)signs|=bit;
            if((word&0x7fffffffu)==0x7fffffffu)overflows|=bit;
        }
        last_mac=zeros|(signs<<4)|(overflows<<12);
        last_flags=(zeros?1u:0u)|(signs?2u:0u)|(overflows?8u:0u);
        raised_sticky|=last_flags<<6;
    }
    // The original constant-product prelude executes even for count zero.
    void prepared(const Block& value){constant_product=value;arithmetic_result(value);}
    // Called before filtering or output stores, including rejected elements.
    void element(const Block& loaded257,const Block& transformed256){
        last_resource257=loaded257;last_resource256=transformed256;
        element_observed=true;arithmetic_result(transformed256);
    }
    std::uint32_t project_status(std::uint32_t previous)const{
        // This is the original helper's mask, not an assumed preservation of
        // every incoming status bit. Bits >=12 are cleared by the prelude.
        return (previous&0xff0u)|raised_sticky|last_flags;
    }
};
}
