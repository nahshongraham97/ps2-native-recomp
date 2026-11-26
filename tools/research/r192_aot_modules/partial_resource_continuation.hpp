#pragma once
// Typed continuation of the source-capacity resource algorithm. It owns
// prepared logical values, not a machine frame or executable operation stream.
#include "partial_resource_pipeline.hpp"
#include "resource_pipeline_continuation.hpp"

namespace mka::emitted::partial_resources {
class Continuation {
    Prepared prepared_;Streams streams_;
    resources::Phase phase_=resources::Phase::initial;
    std::uint32_t consumed_{},accepted_{},unstarted_{};bool busy_{};
    resources::Result effects_;
    void merge(resources::Result current){
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
    Continuation(Streams streams,const Descriptor&descriptor,const Bindings&bindings):
        prepared_(prepare(descriptor,streams,bindings)),streams_(streams),unstarted_(streams.count){
        effects_.scalar_flags=prepared_.body.prelude_flags;
        effects_.vector=prepared_.body.vector;effects_.vector_written=prepared_.body.vector_written;
    }
    Continuation(const Continuation&)=delete;Continuation&operator=(const Continuation&)=delete;
    Continuation(Continuation&&)=delete;Continuation&operator=(Continuation&&)=delete;
    const Prepared&prepared()const{return prepared_;}
    resources::Boundary advance(std::int64_t budget,resources::GuardService service,Writes writes={}){
        using resources::Phase;
        if(busy_)refuse("partial resource continuation: recursive advance refused");
        if(phase_==Phase::complete||phase_==Phase::failed)refuse("partial resource continuation: retired or failed context refused");
        if(!service.invoke)refuse("partial resource continuation: guard service absent");
        busy_=true;struct Reset{bool&busy;~Reset(){busy=false;}}reset{busy_};
        try{
            if(phase_==Phase::initial){
                if(budget<INT64_MIN+static_cast<std::int64_t>(prepared_.prelude+4))
                    refuse("partial resource continuation: budget arithmetic overflow");
                budget-=prepared_.prelude;
            }else goto process_element;
            for(;;){
                if(budget<INT64_MIN+4)refuse("partial resource continuation: budget arithmetic overflow");
                budget-=2;
                if(!unstarted_){budget-=2;phase_=Phase::complete;return {phase_,consumed_,accepted_,unstarted_,budget,effects_};}
                --unstarted_;
                if(budget<0&&service.invoke(service.context,budget)){
                    phase_=Phase::suspended;return {phase_,consumed_,accepted_,unstarted_,budget,effects_};
                }
process_element:
                {
                    const auto worst=prepared_.main+3*prepared_.descriptor.filter_count+prepared_.accepted+2+prepared_.remaps;
                    if(budget<INT64_MIN+static_cast<std::int64_t>(worst))
                        refuse("partial resource continuation: budget arithmetic overflow");
                    Streams one=streams_;one.count=1;
                    for(auto&stream:one.streams){
                        stream.input=stream.input.subspan(std::size_t(consumed_)*stream.stride);
                        stream.output=stream.output.subspan(std::size_t(accepted_)*stream.stride);
                    }
                    auto current=run(one,prepared_,writes);
                    // Initial prelude/return/guard costs from the isolated
                    // one-element helper are not executed again on resume.
                    budget-=static_cast<std::int64_t>(current.cost-prepared_.prelude-6);
                    accepted_+=current.effects.accepted;++consumed_;merge(current.effects);effects_.accepted=accepted_;
                }
            }
        }catch(...){phase_=Phase::failed;throw;}
    }
};
}
