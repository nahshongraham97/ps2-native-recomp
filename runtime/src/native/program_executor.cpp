#include "mka/program_executor.hpp"
namespace mka::sound {
namespace {
std::uint32_t word(const std::array<std::uint8_t,40>& b,std::size_t a){return std::uint32_t(b[a])|(std::uint32_t(b[a+1])<<8)|(std::uint32_t(b[a+2])<<16)|(std::uint32_t(b[a+3])<<24);}
std::int16_t half(const std::array<std::uint8_t,40>& b,std::size_t a){const auto v=std::uint16_t(b[a])|(std::uint16_t(b[a+1])<<8);return v<=32767?static_cast<std::int16_t>(v):static_cast<std::int16_t>(-1-static_cast<int>(65535-v));}
const PreparedSampleInstance* validated_sample(const PreparedProgram& program,std::size_t index,ProgramFailure& failure) noexcept{
    const PreparedSampleInstance* instance=nullptr;
    for(const auto& candidate:program.normal_instances)if(candidate.command_index==index){
        if(instance){failure={ProgramError::ambiguous_prepared_sample,index,program.commands[index].kind()};return nullptr;}instance=&candidate;
    }
    if(!instance || !instance->sample){failure={ProgramError::missing_prepared_sample,index,program.commands[index].kind()};return nullptr;}
    const auto& sample=*instance->sample;const auto count=sample.source.plane_count();
    const auto& r=sample.resource;const auto key=program.commands[index].resource_bits()==0xffffffffu?0:program.commands[index].resource_bits();
    const auto actual_key=std::uint32_t(r.bytes[8])|(std::uint32_t(r.bytes[9])<<8)|(std::uint32_t(r.bytes[10])<<16)|(std::uint32_t(r.bytes[11])<<24);
    bool valid=r.status==ResourceLookupStatus::found && r.key_mode==ResourceKeyMode::numeric && key==actual_key &&
        (count==1||count==2) && sample.pcm.size()==count;
    const std::uint64_t plane_bytes=sample.source.bytes_per_plane();
    valid=valid && plane_bytes!=0 && plane_bytes%16==0;
    for(const auto& pcm:sample.pcm)valid=valid && pcm.block_flags.size()==plane_bytes/16 && pcm.samples.size()==plane_bytes/16*28;
    if(count==2)valid=valid && sample.count2_encoded && sample.count2_encoded->planes[0].size()==plane_bytes && sample.count2_encoded->planes[1].size()==plane_bytes;
    if(!valid){failure={ProgramError::invalid_prepared_sample,index,program.commands[index].kind()};return nullptr;}
    return instance;
}

}
SampleStartOperands sample_start_operands(const CommandRecord& command)noexcept{
    SampleStartOperands value;
    value.parameter_44=half(command.bytes,32);value.parameter_46=half(command.bytes,34);value.parameter_48=half(command.bytes,36);
    value.descriptor_word_28=word(command.bytes,16);value.start_argument=word(command.bytes,12);
    value.repeat_operand=half(command.bytes,10);value.set_descriptor_flag_1=value.repeat_operand!=1;
    value.set_descriptor_flag_4=(command.bytes[9]&1u)!=0;return value;
}
bool validate_program_coverage(const PreparedProgram& program,ProgramFailure& failure) noexcept{
    failure={};
    if(program.commands.empty()){failure.kind=ProgramError::empty_program;return false;}
    // The implemented control flow is linear until kind7 (terminal), or count
    // end. Serialized records beyond that terminal are never dispatched.
    for(std::size_t index=0;index<program.commands.size();++index){
        const auto kind=program.commands[index].kind();
        if(kind==7)break;
        if(kind!=1){failure={ProgramError::unsupported_command_kind,index,kind};return false;}
        if(!validated_sample(program,index,failure))return false;
    }
    return true;
}
std::optional<ProgramExecutor> ProgramExecutor::create(std::shared_ptr<const PreparedProgram> p,ProgramClockWord deadline,ProgramFailure& failure){
    failure={};if(!p){failure.kind=ProgramError::null_program;return std::nullopt;}
    if(p->commands.empty()){failure.kind=ProgramError::empty_program;return std::nullopt;}
    if(!validate_program_coverage(*p,failure))return std::nullopt;
    return ProgramExecutor(std::move(p),deadline);
}
bool ProgramExecutor::fail(ProgramError error,ProgramFailure& failure){
    failure={error,cursor_,cursor_<program_->commands.size()?program_->commands[cursor_].kind():0};
    terminal_failure_=failure;state_=State::failed;return false;
}
void ProgramExecutor::advance(){
    ++cursor_;
    if(cursor_>=program_->commands.size()){state_=State::finished;structural_end_pending_=true;return;}
    // Original2C280 advances40 then schedules the NEXT command's delay.
    const auto& next=program_->commands[cursor_];
    if(next.bytes[8]&0x80u)return;
    const auto delay=next.delay_milliseconds();
    if(delay<0)deadline_=0xffffffffu;
    else deadline_+=static_cast<std::uint32_t>(delay); // Proven source-word arithmetic.
}
std::optional<ProgramStep> ProgramExecutor::step(ProgramClockWord now,ProgramFailure& failure){
    failure={};if(!program_){failure.kind=ProgramError::moved_executor;return std::nullopt;}
    if(state_==State::failed){failure=terminal_failure_;return std::nullopt;}
    ProgramStep event;event.owner=program_;event.command_index=cursor_;event.scheduled={deadline_};event.observed=now;
    if(state_==State::awaiting_start){event.kind=ProgramStepKind::awaiting_start_result;return event;}
    if(state_==State::finished){event.kind=structural_end_pending_?ProgramStepKind::structural_end:ProgramStepKind::finished;structural_end_pending_=false;return event;}
    const auto& command=program_->commands[cursor_];event.command=command;
    if(now.milliseconds<deadline_ && !(command.bytes[8]&0x80u)){event.kind=ProgramStepKind::waiting;return event;}
    switch(command.kind()){
    case 1:{
        const auto* instance=validated_sample(*program_,cursor_,failure);
        if(!instance){terminal_failure_=failure;state_=State::failed;return std::nullopt;}
        event.kind=ProgramStepKind::sample_start;event.clip=instance->sample;
        event.start=sample_start_operands(command);
        state_=State::awaiting_start;return event;
    }
    case 7:
        event.kind=ProgramStepKind::program_stop;event.stop_mode_bit=program_->effective_flags&1u;
        state_=State::finished;return event;
    default:fail(ProgramError::unsupported_command_kind,failure);return std::nullopt;
    }
}
std::optional<ProgramStep> ProgramExecutor::stop_from_owner(ProgramFailure& failure){
    failure={};if(!program_){failure.kind=ProgramError::moved_executor;return std::nullopt;}
    ProgramStep event;event.kind=ProgramStepKind::program_stop;event.owner=program_;event.command_index=cursor_;
    event.scheduled={deadline_};event.stop_reason=ProgramStopReason::owner_request;event.stop_mode_bit=program_->effective_flags&1u;
    if(cursor_<program_->commands.size())event.command=program_->commands[cursor_];
    // External cleanup may stop clips even after an execution failure. It does
    // not erase that named terminal failure or resume past an unknown command.
    if(state_!=State::failed)state_=State::finished;
    structural_end_pending_=false;return event;
}
bool ProgramExecutor::acknowledge_start(bool started,ProgramFailure& failure){
    failure={};if(!program_){failure.kind=ProgramError::moved_executor;return false;}
    if(state_!=State::awaiting_start){failure.kind=ProgramError::invalid_start_acknowledgement;failure.command_index=cursor_;return false;}
    if(!started)return fail(ProgramError::native_start_failed,failure);
    state_=State::running;advance();return true;
}
} // namespace mka::sound
