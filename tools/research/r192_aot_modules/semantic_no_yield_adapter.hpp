#pragma once
// Checked compatibility contract for ONE source-proved whole semantic family.
// Pure typed values; no Cpu, PC, code decoder, module registry or runtime JIT.
#include "semantic_pipeline.hpp"
#include <cstdint>
#include <limits>

namespace mka::emitted::semantic::no_yield {
struct Admission {std::uint64_t count,worst_cost;std::int32_t incoming_budget;};
struct CheckedResult {Result pipeline;std::int32_t remaining_budget;Bits scalar_control_status;std::uint64_t charged_cost;};
inline Admission admit(std::uint64_t count,std::int32_t budget){
    if(count>(std::numeric_limits<std::uint64_t>::max()-16u)/34u)refuse("no-yield adapter: worst-cost arithmetic overflow");
    if(count>0x7fffffffu)refuse("no-yield adapter: source count domain unproved");
    const std::uint64_t worst=16u+34u*count;
    if(worst>std::uint64_t(std::numeric_limits<std::int32_t>::max()))refuse("no-yield adapter: worst cost exceeds signed budget domain");
    if(budget<0||std::uint64_t(budget)<worst)refuse("no-yield adapter: insufficient worst-case budget; continuation OPEN");
    return {count,worst,budget};
}
inline std::uint64_t actual_cost(std::uint64_t count,std::uint64_t accepted){
    if(accepted>count)refuse("no-yield adapter: accepted count exceeds input count");
    if(count>(std::numeric_limits<std::uint64_t>::max()-16u)/34u)refuse("no-yield adapter: exact-cost arithmetic overflow");
    return 16u+18u*count+16u*accepted;
}
inline Bits project_scalar(Bits original,ScalarEffects effects){
    // Original callee uses ADD/SUB/MUL/MADD/MSUB and CMP only. Builder DIV
    // flags are deliberately excluded. All untouched control/sticky bits stay.
    if(effects.raised_flags&~0x8010u)refuse("no-yield adapter: unproved scalar status bits");
    Bits result=original|effects.raised_flags;
    if(effects.condition_written)result=(result&~0x00800000u)|(effects.condition?0x00800000u:0u);
    return result;
}
inline CheckedResult run(Streams streams,Pipeline pipeline,std::int32_t budget,Bits scalar_control_status,Writes writes={}){
    validate(pipeline);
    if(pipeline.effect_count!=4||pipeline.effects[0].id!=1||pipeline.effects[1].id!=5||pipeline.effects[2].id!=6||pipeline.effects[3].id!=17||
       pipeline.filter_count!=1||pipeline.filter_id!=2||pipeline.filter_tag!=769)
        refuse("no-yield adapter: family has no proved timing contract");
    if(pipeline.effects[1].constant[3]!=pipeline.effects[2].constant[0])refuse("no-yield adapter: original constant-block adjacency unproved");
    const auto admission=admit(streams.count,budget); // BEFORE any output/store.
    auto result=semantic::run(streams,pipeline,writes);
    const auto cost=actual_cost(streams.count,result.accepted);
    if(cost>admission.worst_cost)refuse("no-yield adapter: source cost invariant failed");
    const auto remaining=std::int64_t(budget)-std::int64_t(cost);
    if(remaining<0||remaining>std::numeric_limits<std::int32_t>::max())refuse("no-yield adapter: projected budget outside proved domain");
    return {result,static_cast<std::int32_t>(remaining),project_scalar(scalar_control_status,result.scalar),cost};
}
}
