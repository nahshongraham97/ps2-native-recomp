#pragma once
// Typed source guard events for the intact resource pipeline. No Cpu, source
// PC/register bank, opcode sequence, compiler or executable cache is present.
// The caller owns and authenticates Prepared inputs and external services.
#include "resource_pipeline_continuation.hpp"

namespace mka::emitted::resources {
struct GuardEvents {
    void*context{};
    bool(*invoke)(void*,const Boundary&before,std::int64_t&budget){};
};
class EventContinuation {
    Prepared prepared_;Streams streams_;Timing timing_;
    Phase phase_=Phase::initial;std::uint32_t consumed_{},accepted_{},unstarted_{};
    bool busy_{};Result effects_;
    void merge(Result current){
        current.scalar_flags|=effects_.scalar_flags;
        current.vector.raised_sticky|=effects_.vector.raised_sticky;
        if(!current.vector_written&&effects_.vector_written){current.vector=effects_.vector;current.vector_written=true;}
        if(!current.scalar_accumulator_written&&effects_.scalar_accumulator_written){current.scalar_accumulator_written=true;current.scalar_accumulator=effects_.scalar_accumulator;}
        if(!current.vector_accumulator_written&&effects_.vector_accumulator_written){current.vector_accumulator_written=true;current.vector_accumulator=effects_.vector_accumulator;}
        if(!current.remap_written&&effects_.remap_written){current.remap_written=true;current.remap_word=effects_.remap_word;}
        effects_=current;
    }
public:
    EventContinuation(Streams streams,const Prepared&prepared):prepared_(prepared),streams_(streams),timing_(timing(prepared.descriptor)),unstarted_(streams.count){
        validate(prepared_.descriptor,streams_);
        effects_.scalar_flags=prepared_.prelude_flags;effects_.vector=prepared_.vector;effects_.vector_written=prepared_.vector_written;
    }
    EventContinuation(const EventContinuation&)=delete;EventContinuation&operator=(const EventContinuation&)=delete;
    EventContinuation(EventContinuation&&)=delete;EventContinuation&operator=(EventContinuation&&)=delete;
    Boundary advance(std::int64_t budget,GuardEvents service,Writes writes={}){
        if(busy_)refuse("resource event continuation: recursive advance refused");
        if(phase_==Phase::complete||phase_==Phase::failed)refuse("resource event continuation: retired or failed context refused");
        if(!service.invoke)refuse("resource event continuation: guard service absent");
        busy_=true;struct Reset{bool&flag;~Reset(){flag=false;}}reset{busy_};
        try{
            if(phase_==Phase::initial){if(budget<INT64_MIN+static_cast<std::int64_t>(timing_.prelude+4))refuse("resource event continuation: budget arithmetic overflow");budget-=static_cast<std::int64_t>(timing_.prelude);}
            else goto process_element;
            for(;;){
                if(budget<INT64_MIN+4)refuse("resource event continuation: budget arithmetic overflow");
                budget-=2;
                if(!unstarted_){budget-=2;phase_=Phase::complete;return {phase_,consumed_,accepted_,unstarted_,budget,effects_};}
                --unstarted_;
                if(budget<0){const Boundary before{Phase::suspended,consumed_,accepted_,unstarted_,budget,effects_};
                    if(service.invoke(service.context,before,budget)){phase_=Phase::suspended;return {phase_,consumed_,accepted_,unstarted_,budget,effects_};}}
process_element:
                {
                    if(budget<INT64_MIN+static_cast<std::int64_t>(timing_.before_filter+timing_.accepted))refuse("resource event continuation: budget arithmetic overflow");
                    Streams one=streams_;one.count=1;
                    for(auto&stream:one.streams){stream.input=stream.input.subspan(std::size_t(consumed_)*stream.stride);stream.output=stream.output.subspan(std::size_t(accepted_)*stream.stride);}
                    auto current=run(one,prepared_,writes);
                    budget-=static_cast<std::int64_t>(timing_.before_filter+current.accepted*timing_.accepted);
                    accepted_+=current.accepted;++consumed_;merge(current);effects_.accepted=accepted_;
                }
            }
        }catch(...){phase_=Phase::failed;throw;}
    }
};
}
