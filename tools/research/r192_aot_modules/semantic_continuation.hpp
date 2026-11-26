#pragma once
// Native loop continuation for the one proved four-effect/filter family.
// State is typed data/cursors. No Cpu, guest PC, register bank or opcode stream.
#include "semantic_no_yield_adapter.hpp"

namespace mka::emitted::semantic {
enum class ContinuationPhase {initial,ready,suspended,complete};
enum class GuardDecision {keep_running,suspend};
struct GuardObservation {std::uint32_t consumed,accepted,remaining;};
struct GuardScheduler {
    void* context{};
    GuardDecision(*poll)(void*,GuardObservation,std::int32_t&){};
};
class ClipContinuation {
    Streams streams_;
    Pipeline pipeline_; // Constants are snapshotted once, never rebound on resume.
    Result result_{};
    std::uint32_t consumed_{};
    std::int32_t budget_{};
    ContinuationPhase phase_=ContinuationPhase::initial;
    bool busy_{};
    void charge(unsigned amount){
        const auto next=std::int64_t(budget_)-amount;
        if(next<std::numeric_limits<std::int32_t>::min())refuse("semantic continuation: budget arithmetic domain unproved");
        budget_=static_cast<std::int32_t>(next);
    }
    bool guard(GuardScheduler scheduler){
        // Source count decrement has happened before this true-edge callback.
        // Native remaining is the number of elements still to process.
        phase_=ContinuationPhase::ready;
        if(budget_>=0)return false;
        if(!scheduler.poll)refuse("semantic continuation: negative-budget scheduler unbound");
        const auto decision=scheduler.poll(scheduler.context,{consumed_,result_.accepted,streams_.count-consumed_},budget_);
        if(decision==GuardDecision::suspend){phase_=ContinuationPhase::suspended;return true;}
        if(decision!=GuardDecision::keep_running)refuse("semantic continuation: scheduler decision unproved");
        return false;
    }
    void merge(const Result& element){
        if(!element.vector_effects_admitted||element.accepted>1)refuse("semantic continuation: element contract unproved");
        result_.accepted+=element.accepted;
        result_.scalar.raised_flags|=element.scalar.raised_flags;
        result_.scalar.condition_written=element.scalar.condition_written;
        result_.scalar.condition=element.scalar.condition;
        auto&v=result_.vector;const auto&next=element.vector;
        // Per-element evaluation repeats only invariant pure preparation.
        // Its flags have already occurred; OR is idempotent. The final result
        // and source per-element arithmetic determine the current flags.
        if(v.constant_product!=next.constant_product)refuse("semantic continuation: invariant constant product changed");
        v.element_observed=true;v.last_resource256=next.last_resource256;v.last_resource257=next.last_resource257;
        v.last_mac=next.last_mac;v.last_flags=next.last_flags;v.raised_sticky|=next.raised_sticky;
    }
public:
    ClipContinuation(const ClipContinuation&)=delete;
    ClipContinuation& operator=(const ClipContinuation&)=delete;
    ClipContinuation(ClipContinuation&&)=delete;
    ClipContinuation& operator=(ClipContinuation&&)=delete;
    ClipContinuation(Streams streams,Pipeline pipeline):streams_(streams),pipeline_(pipeline){
        validate(pipeline_);
        if(pipeline_.effect_count!=4||pipeline_.effects[0].id!=1||pipeline_.effects[1].id!=5||pipeline_.effects[2].id!=6||pipeline_.effects[3].id!=17||
           pipeline_.filter_count!=1||pipeline_.filter_id!=2||pipeline_.filter_tag!=769||pipeline_.effects[1].constant[3]!=pipeline_.effects[2].constant[0])
            refuse("semantic continuation: family contract unproved");
        if(streams_.count&&(streams_.count>0x7fffffffu||streams_.stride_a>0x7fffffffu||streams_.stride_b>0x7fffffffu||streams_.stride_a<48||(streams_.stride_a&15u)||
            (reinterpret_cast<std::uintptr_t>(streams_.input_a.data())&15u)||(reinterpret_cast<std::uintptr_t>(streams_.output_a.data())&15u)))
            refuse("semantic continuation: count/stride/alignment domain unproved");
        extent(streams_.input_a,streams_.stride_a,streams_.count,48);extent(streams_.output_a,streams_.stride_a,streams_.count,48);
        extent(streams_.input_b,streams_.stride_b,streams_.count,4);extent(streams_.output_b,streams_.stride_b,streams_.count,4);
        result_=semantic::run({},pipeline_); // Original prelude, no stream access.
        if(!result_.vector_effects_admitted)refuse("semantic continuation: vector contract unproved");
    }
    ContinuationPhase run(std::int32_t incoming_budget,GuardScheduler scheduler={},Writes writes={}){
        if(busy_)refuse("semantic continuation: reentrant context");
        if(phase_==ContinuationPhase::complete)refuse("semantic continuation: completed context cannot resume");
        struct Busy {bool&flag;Busy(bool&f):flag(f){flag=true;}~Busy(){flag=false;}} busy(busy_);
        budget_=incoming_budget;
        if(phase_==ContinuationPhase::initial){
            // 12-word prelude including the forward branch/delay, then the
            // two-word first count guard (including its decrement delay).
            charge(12);charge(2);
            if(!streams_.count){charge(2);phase_=ContinuationPhase::complete;return phase_;}
            if(guard(scheduler))return phase_;
        }else if(phase_!=ContinuationPhase::suspended)refuse("semantic continuation: resume phase unproved");
        // A suspended true edge already charged/decremented its guard. Resume
        // starts the next element directly, without preparation or guard.
        phase_=ContinuationPhase::ready;
        for(;;){
            if(budget_<std::numeric_limits<std::int32_t>::min()+36)refuse("semantic continuation: budget arithmetic domain unproved");
            charge(16); // Before first output store, as in the original body.
            auto one=streams_;one.count=1;
            one.input_a=one.input_a.subspan(std::size_t(consumed_)*one.stride_a);
            one.input_b=one.input_b.subspan(std::size_t(consumed_)*one.stride_b);
            one.output_a=one.output_a.subspan(std::size_t(result_.accepted)*one.stride_a);
            one.output_b=one.output_b.subspan(std::size_t(result_.accepted)*one.stride_b);
            const auto element=semantic::run(one,pipeline_,writes);merge(element);
            if(element.accepted)charge(16);
            ++consumed_;charge(2);
            if(consumed_==streams_.count){charge(2);phase_=ContinuationPhase::complete;return phase_;}
            if(guard(scheduler))return phase_;
        }
    }
    const Result& result()const{return result_;}
    std::uint32_t consumed()const{return consumed_;}
    std::uint32_t remaining()const{return streams_.count-consumed_;}
    std::int32_t budget()const{return budget_;}
    ContinuationPhase phase()const{return phase_;}
};
}
