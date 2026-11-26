#pragma once
// Typed algorithm continuation for the bounded resource pipeline. The guard
// service is explicit. This layer contains no Cpu, source PC, saved register
// bank, opcode operation stream, original compiler or executable-code cache.
#include "semantic_resource_pipeline.hpp"
#include <exception>

namespace mka::emitted::resources {
struct GuardService {void*context{};bool(*invoke)(void*,std::int64_t&){};};
enum class Phase {initial, suspended, complete, failed};
struct Boundary {
    Phase phase{};std::uint32_t consumed{},accepted{},remaining_after_pending{};
    std::int64_t budget{};Result effects;
};
class Continuation {
    Prepared prepared_;Streams streams_;Timing timing_;
    Phase phase_=Phase::initial;std::uint32_t consumed_{},accepted_{},unstarted_{};
    bool busy_{};Result effects_;
    void merge(Result current){
        current.scalar_flags|=effects_.scalar_flags;
        current.vector.raised_sticky|=effects_.vector.raised_sticky;
        if(!current.vector_written&&effects_.vector_written){current.vector=effects_.vector;current.vector_written=true;}
        if(!current.scalar_accumulator_written&&effects_.scalar_accumulator_written){
            current.scalar_accumulator_written=true;current.scalar_accumulator=effects_.scalar_accumulator;}
        if(!current.vector_accumulator_written&&effects_.vector_accumulator_written){
            current.vector_accumulator_written=true;current.vector_accumulator=effects_.vector_accumulator;}
        if(!current.remap_written&&effects_.remap_written){current.remap_written=true;current.remap_word=effects_.remap_word;}
        effects_=current;
    }
public:
    explicit Continuation(Streams streams,const Descriptor&descriptor):
        prepared_(prepare(descriptor,streams)),streams_(streams),timing_(timing(descriptor)),unstarted_(streams.count){
        effects_.scalar_flags=prepared_.prelude_flags;effects_.vector=prepared_.vector;effects_.vector_written=prepared_.vector_written;
    }
    Continuation(const Continuation&)=delete;Continuation&operator=(const Continuation&)=delete;
    Continuation(Continuation&&)=delete;Continuation&operator=(Continuation&&)=delete;
    Boundary advance(std::int64_t budget,GuardService service,Writes writes={}){
        if(busy_)refuse("resource continuation: recursive advance refused");
        if(phase_==Phase::complete||phase_==Phase::failed)refuse("resource continuation: retired or failed context refused");
        if(!service.invoke)refuse("resource continuation: guard service absent");
        busy_=true;
        struct Reset{bool&flag;~Reset(){flag=false;}}reset{busy_};
        try{
            if(phase_==Phase::initial){
                if(budget<INT64_MIN+static_cast<std::int64_t>(timing_.prelude+4))
                    refuse("resource continuation: budget arithmetic overflow");
                budget-=static_cast<std::int64_t>(timing_.prelude);
            }else{
                // The source resumed at the body AFTER its prior true guard.
                // Prepared constants are snapshotted once, never re-read.
                goto process_element;
            }
            for(;;){
                if(budget<INT64_MIN+4)refuse("resource continuation: budget arithmetic overflow");
                budget-=2;
                if(!unstarted_){budget-=2;phase_=Phase::complete;return {phase_,consumed_,accepted_,unstarted_,budget,effects_};}
                --unstarted_;
                if(budget<0&&service.invoke(service.context,budget)){
                    phase_=Phase::suspended;return {phase_,consumed_,accepted_,unstarted_,budget,effects_};
                }
process_element:
                {
                    const auto worst=timing_.before_filter+timing_.accepted;
                    if(budget<INT64_MIN+static_cast<std::int64_t>(worst))
                        refuse("resource continuation: budget arithmetic overflow");
                    Streams one=streams_;one.count=1;
                    for(auto&stream:one.streams){
                        stream.input=stream.input.subspan(std::size_t(consumed_)*stream.stride);
                        stream.output=stream.output.subspan(std::size_t(accepted_)*stream.stride);
                    }
                    // Pure prepared-prefix observations are idempotent. They
                    // are merged, not re-applied to a machine status register.
                    auto current=run(one,prepared_,writes);
                    budget-=static_cast<std::int64_t>(timing_.before_filter+current.accepted*timing_.accepted);
                    accepted_+=current.accepted;++consumed_;merge(current);effects_.accepted=accepted_;
                }
            }
        }catch(...){phase_=Phase::failed;throw;}
    }
};
}
