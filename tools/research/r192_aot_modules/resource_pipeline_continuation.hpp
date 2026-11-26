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
// Unused stream transport carries no source access obligation. In particular,
// an empty unused span may have an arbitrary nonzero stride. Never perform
// pointer arithmetic or subspan on it. Used offsets are checked before slicing.
inline Streams element_streams(const Streams&streams,const Descriptor&descriptor,std::uint32_t consumed,std::uint32_t accepted){
    if(descriptor.attribute_count>descriptor.attributes.size())refuse("resource continuation: slice descriptor bounds");
    Streams one=streams;one.count=1;
    for(unsigned index=0;index<one.streams.size();++index){
        bool referenced=false;
        for(unsigned i=0;i<descriptor.attribute_count;++i)referenced|=descriptor.attributes[i].stream==index;
        if(!referenced)continue;
        auto&stream=one.streams[index];
        if((consumed&&stream.stride>SIZE_MAX/consumed)||(accepted&&stream.stride>SIZE_MAX/accepted))
            refuse("resource continuation: stream slice arithmetic overflow");
        const auto input=std::size_t(consumed)*stream.stride,output=std::size_t(accepted)*stream.stride;
        if(input>stream.input.size()||output>stream.output.size())
            refuse("resource continuation: stream slice bounds");
        stream.input=stream.input.subspan(input);stream.output=stream.output.subspan(output);
    }
    return one;
}
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
                    Streams one=element_streams(streams_,prepared_.descriptor,consumed_,accepted_);
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
