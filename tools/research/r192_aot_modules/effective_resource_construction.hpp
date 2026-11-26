#pragma once
// Original constructor outcomes, not a runtime Plan/owner admission API.
// Inputs must come from the producer's original sentinel list and records.
// Later owner resource lists and relocated references are distinct evidence.
#include "reference_resource_layout.hpp"
#include <optional>
#include <vector>

namespace mka::emitted::reference_resources::construction {
struct RawFilter {
    std::int16_t id{};
    Bits source_tag{},linked_descriptor{},limit{};
};
enum class FilterBreak { none,unknown_id,linked_descriptor,non_scalar_bank };
struct FilterOutcome {
    std::vector<Filter> effective;
    unsigned attempted{};
    std::optional<unsigned> break_index;
    FilterBreak reason=FilterBreak::none;
};
inline FilterOutcome source_filter_outcome(std::span<const Attribute> attributes,
        std::span<const RawFilter> supplied,std::int32_t declared_count){
    FilterOutcome result;
    // Original signed BLEZ skips the loop. Do not reinterpret negative count
    // as a huge unsigned extent or invent a universal four-record bound.
    for(std::int32_t i=0;i<declared_count;++i){
        if(std::size_t(i)>=supplied.size())refuse("construction: original filter record unavailable");
        const auto&raw=supplied[unsigned(i)];++result.attempted;
        auto stop=[&](FilterBreak reason){result.reason=reason;result.break_index=unsigned(i);};
        // The original table lookup occurs before callback2ED300; unknown ID
        // stops the entire filter loop without examining this record's data.
        if(raw.id!=2){stop(FilterBreak::unknown_id);break;}
        if(raw.linked_descriptor){stop(FilterBreak::linked_descriptor);break;}
        const Attribute*resource=nullptr;
        for(const auto&a:attributes)if(a.tag==raw.source_tag){resource=&a;break;}
        if(!resource)refuse("construction: original filter resource binding unavailable");
        // Unknown format remains unproved. It must not be converted to a
        // guessed failed callback, since original lookup can dereference RAM.
        if(format(resource->tag)!=3){stop(FilterBreak::non_scalar_bank);break;}
        result.effective.push_back({2,raw.source_tag,raw.limit});
    }
    return result;
}

// Original2E7A30 searches temporary descriptor pointers in order and replaces
// a matching reference with the corresponding final descriptor pointer.
// This is a metadata equation only; it does not write memory or create a lease.
inline Bits relocated_filter_reference(Bits previous,
        std::span<const Bits> temporary,std::span<const Bits> final){
    if(temporary.size()!=final.size())refuse("construction: descriptor relocation table extent mismatch");
    for(unsigned i=0;i<temporary.size();++i)if(previous==temporary[i])return final[i];
    return previous;
}

struct CapturedPlanInput {
    std::vector<Attribute> original_attributes;
    std::vector<Effect> original_effects;
    std::vector<RawFilter> original_filters;
    std::int32_t original_filter_count{};
    FilterOutcome outcome;
    // No fabricated event sequence can make this component an owner oracle.
    // Actual producer/copy/publication/revocation hooks are still required.
    static constexpr bool runtime_lifetime_admitted=false;
};
inline CapturedPlanInput capture_plan_input(std::span<const Attribute> attributes,
        std::span<const Effect> effects,std::span<const RawFilter> filters,
        std::int32_t count){
    CapturedPlanInput result;
    result.original_attributes.assign(attributes.begin(),attributes.end());
    result.original_effects.assign(effects.begin(),effects.end());
    result.original_filters.assign(filters.begin(),filters.end());
    result.original_filter_count=count;
    result.outcome=source_filter_outcome(result.original_attributes,result.original_filters,count);
    return result;
}
}
