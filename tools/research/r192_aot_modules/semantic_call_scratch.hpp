#pragma once
// Algorithm intermediates needed by the R192 reference scheduler boundary.
// Fixed typed meanings, not an indexed register file or instruction stream.
#include <array>
#include <cstdint>

namespace mka::emitted::semantic {
struct CallScratch {
    using Bits=std::uint32_t;
    using Block=std::array<Bits,4>;
    struct Prepared {
        Bits scalar_product{},zero{},remap_upper{},remap_scale{},remap_start{},remap_extent{},filter_limit{};
        bool operator==(const Prepared&)const=default;
    } prepared;
    bool element_observed{},accepted_observed{};
    Block retained_middle{};
    Bits resource259{},resource258{},resource769{},remap_word{},remap_accumulator{};
    void element(const Block& retained,Bits loaded259,Bits updated258,Bits updated769){
        retained_middle=retained;resource259=loaded259;resource258=updated258;resource769=updated769;element_observed=true;
    }
    void accepted(Bits converted_word,Bits accumulator){
        remap_word=converted_word;remap_accumulator=accumulator;accepted_observed=true;
    }
    bool merge_element(const CallScratch& next){
        if(prepared!=next.prepared||!next.element_observed)return false;
        element(next.retained_middle,next.resource259,next.resource258,next.resource769);
        if(next.accepted_observed)accepted(next.remap_word,next.remap_accumulator);
        return true;
    }
};
}
