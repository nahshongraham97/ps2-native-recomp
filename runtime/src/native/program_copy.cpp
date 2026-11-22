#include "mka/program_copy.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>
#include <type_traits>
namespace mka::sound {
ProgramClockWord ordinary_initial_deadline(const CommandRecord& first,ProgramClockWord clock)noexcept{
    return {clock.milliseconds+static_cast<std::uint32_t>(first.delay_milliseconds())};
}
bool reset_ordinary_command_state(std::vector<CommandRecord>& commands,ProgramCopyFailure& f){
    f={};const auto end=std::find_if(commands.begin(),commands.end(),[](const auto& c){return c.kind()==7;});
    if(end==commands.end()){f.kind=ProgramCopyError::missing_terminator;return false;}
    for(auto i=commands.begin();i!=end;++i)if(i->kind()==6)std::fill_n(i->bytes.begin()+16,4,0);
    return true;
}
struct NativeProgramCopy::Impl {
    struct Instance {std::size_t command_index;NormalInstanceTemplateState state;std::shared_ptr<const PreparedNormal> sample;std::optional<HostVoice> output;};
    std::unique_ptr<RuntimeProgramReference> reference;
    std::shared_ptr<const NativeRuntimePreparation> runtime;
    std::shared_ptr<PreparedProgram> data;
    std::vector<Instance> instances;
    bool scheduled=false,stopped=false,outputs_retired=false;
    Instance* find(std::size_t index){for(auto& i:instances)if(i.command_index==index)return &i;return nullptr;}
    ~Impl(){instances.clear();data.reset();runtime.reset();reference.reset();}
};
NativeProgramCopy::NativeProgramCopy(std::unique_ptr<Impl> s):state_(std::move(s)){}
NativeProgramCopy::~NativeProgramCopy()=default;
std::unique_ptr<NativeProgramCopy> NativeProgramCopy::clone(std::unique_ptr<RuntimeProgramReference>& reference,ProgramCopyFailure& f){
    f={};if(!reference){f.kind=ProgramCopyError::absent_runtime_reference;return {};}
    auto runtime=reference->preparation();if(!runtime){f.kind=ProgramCopyError::absent_runtime_reference;return {};}
    auto work=runtime->resource_domain()->acquire(AudioResourceWork::program,f.resource);
    if(!work){f.kind=ProgramCopyError::resource_access;return {};}
    try {
        const auto& donor=*runtime->data();
        if(donor.commands.empty()){f.kind=ProgramCopyError::empty_program;return {};}
        auto s=std::make_unique<Impl>();s->runtime=runtime;s->data=std::make_shared<PreparedProgram>();
        s->data->program_index=donor.program_index;s->data->effective_flags=donor.effective_flags;s->data->commands=donor.commands;
        s->instances.reserve(donor.normal_instances.size());s->data->normal_instances.reserve(donor.normal_instances.size());
        std::vector<const PreparedSampleInstance*> donors(donor.commands.size(),nullptr);
        for(const auto& i:donor.normal_instances){
            if(i.command_index>=donor.commands.size()||donor.commands[i.command_index].kind()!=1){f.kind=ProgramCopyError::foreign_instance;f.command_index=i.command_index;return {};}
            if(donors[i.command_index]){f.kind=ProgramCopyError::duplicate_instance;f.command_index=i.command_index;return {};}
            donors[i.command_index]=&i;
        }
        for(std::size_t n=0;n<donor.commands.size();++n){const auto& c=donor.commands[n];if(c.kind()!=1)continue;f.command_index=n;
            const auto* instance=donors[n];
            if(!instance||!instance->sample){f.kind=ProgramCopyError::missing_instance;return {};}
            if(instance->native_template.flags&2u){f.kind=ProgramCopyError::alternate_instance;return {};}
            s->instances.push_back({n,instance->native_template,instance->sample,{}});
            // Payload is shared. The template reference owns the single cache
            // acquisition; this clone creates no new preparation occurrence.
            s->data->normal_instances.push_back({n,c,instance->repeat_operand,instance->source_preserve_resource_after_instance,instance->sample,{},instance->native_template});
        }
        auto result=std::unique_ptr<NativeProgramCopy>(new NativeProgramCopy(std::move(s)));
        result->state_->reference=std::move(reference);f={};return result;
    }catch(const std::bad_alloc&){f.kind=ProgramCopyError::allocation_failed;}
    catch(const std::length_error&){f.kind=ProgramCopyError::host_capacity;}
    return {};
}
std::shared_ptr<const void> NativeProgramCopy::bank_identity() const noexcept{
    return state_->runtime?state_->runtime->bank_:nullptr;
}
bool NativeProgramCopy::validate_forced_output_transfer(const std::vector<HostVoice>& destination,ProgramCopyFailure& f) const noexcept{
    f={};
    if(!state_->scheduled){f.kind=ProgramCopyError::not_scheduled;return false;}
    if(state_->outputs_retired){f.kind=ProgramCopyError::outputs_retired;return false;}
    if(!destination.empty()){f.kind=ProgramCopyError::invalid_transfer_destination;return false;}
    if(!state_->reference||!state_->runtime||!state_->data){f.kind=ProgramCopyError::invalid_transfer_provenance;return false;}
    for(const auto& occurrence:state_->data->normal_instances){
        if(occurrence.cache_reference){f.kind=ProgramCopyError::invalid_transfer_provenance;f.command_index=occurrence.command_index;return false;}
        if(occurrence.native_template.flags&2u){f.kind=ProgramCopyError::alternate_instance;f.command_index=occurrence.command_index;return false;}
    }
    std::size_t count=0;
    for(const auto& instance:state_->instances)if(instance.output){
        if(!instance.output->validate_program_owner(state_->data,f.voice)){f.kind=ProgramCopyError::voice_failed;f.command_index=instance.command_index;return false;}
        ++count;
    }
    if(destination.capacity()<count){f.kind=ProgramCopyError::transfer_capacity;return false;}
    return true;
}
bool NativeProgramCopy::transfer_forced_outputs(std::vector<HostVoice>& destination,ProgramCopyFailure& f) noexcept{
    if(!validate_forced_output_transfer(destination,f))return false;
    // Owner serializes all copy/output mutation. Every fallible condition and
    // destination allocation precedes the first actual release-state change.
    for(auto& instance:state_->instances)if(instance.output){
        if(!instance.output->stop_for_resource_retirement(state_->data,f.voice)){f.kind=ProgramCopyError::voice_failed;f.command_index=instance.command_index;return false;}
    }
    static_assert(std::is_nothrow_move_constructible_v<HostVoice>);
    for(auto& instance:state_->instances)if(instance.output){
        destination.push_back(std::move(*instance.output));instance.output.reset();
    }
    state_->stopped=true;state_->outputs_retired=true;return true;
}
const std::vector<CommandRecord>& NativeProgramCopy::commands()const noexcept{return state_->data->commands;}
std::optional<NormalPlayInstanceObservation> NativeProgramCopy::instance(std::size_t index,ProgramCopyFailure& f)const{
    f={};auto* i=state_->find(index);if(!i){f.kind=ProgramCopyError::missing_instance;f.command_index=index;return {};}
    NormalPlayInstanceObservation result;result.state=i->state;result.output_bound=bool(i->output);
    if(i->output){result.output_active=i->output->active();result.output_source_index=i->output->source_index();}
    return result;
}
std::optional<OrdinaryProgramSchedule> NativeProgramCopy::prepare_ordinary_schedule(ProgramClockWord clock,std::uint16_t flags,ProgramCopyFailure& f){
    return bind_preactivated_schedule(ordinary_initial_deadline(state_->data->commands.front(),clock),flags,f);
}
std::optional<OrdinaryProgramSchedule> NativeProgramCopy::bind_preactivated_schedule(ProgramClockWord deadline,std::uint16_t flags,ProgramCopyFailure& f){
    f={};if(state_->scheduled){f.kind=ProgramCopyError::already_scheduled;return {};}
    if(flags&8u){f.kind=ProgramCopyError::alternate_schedule_unbound;return {};}
    if(!reset_ordinary_command_state(state_->data->commands,f))return {};
    state_->data->effective_flags=flags;state_->scheduled=true;
    return OrdinaryProgramSchedule{state_->data,deadline,flags};
}
NativeProgramOutputObservation NativeProgramCopy::output_observation()const noexcept{
    NativeProgramOutputObservation result;
    for(const auto& i:state_->instances)if(i.output){
        if(result.voices++==0)result.first_voice_source_index=i.output->source_index();
        if(i.output->active())++result.active_voices;
    }
    return result;
}
void NativeProgramCopy::append_outputs(std::vector<HostVoice*>& voices){
    for(auto& i:state_->instances)if(i.output)voices.push_back(&*i.output);
}
bool NativeProgramCopy::start_output(const ProgramStep& event,const NormalOutputParameters& p,ProgramCopyFailure& f){
    f={};if(!state_->scheduled){f.kind=ProgramCopyError::not_scheduled;return false;}
    if(state_->outputs_retired){f.kind=ProgramCopyError::outputs_retired;return false;}
    if(state_->stopped){f.kind=ProgramCopyError::stopped_copy;return false;}
    if(event.kind!=ProgramStepKind::sample_start||event.owner!=state_->data){f.kind=ProgramCopyError::foreign_step;return false;}
    auto* i=state_->find(event.command_index);
    if(!i){f.kind=ProgramCopyError::missing_instance;return false;}
    const auto& command=state_->data->commands[event.command_index];
    if(command.kind()!=1||event.command.bytes!=command.bytes||event.clip!=i->sample){f.kind=ProgramCopyError::wrong_command;return false;}
    if(i->output){f.kind=ProgramCopyError::output_already_bound;return false;}
    const auto operands=sample_start_operands(command);
    i->state.gain_q14=operands.parameter_44;i->state.pan_q14=operands.parameter_46;i->state.pitch_q12=operands.parameter_48;
    i->state.descriptor_word=operands.descriptor_word_28;i->state.repeat_operand=operands.repeat_operand;
    if(operands.set_descriptor_flag_1)i->state.flags|=1u;
    if(operands.set_descriptor_flag_4)i->state.flags|=4u;
    ProgramStep actual=event;actual.start=operands;
    auto voice=HostVoice::start(actual,p.context,p.program,p.source_output_mode,p.envelope,p.host_output_rate,f.voice,p.curve);
    if(!voice){f.kind=ProgramCopyError::voice_failed;return false;}
    i->output=std::move(voice);i->state.start_argument=operands.start_argument;return true;
}
std::optional<std::vector<float>> NativeProgramCopy::mix(std::size_t frames,ProgramCopyFailure& f){
    f={};if(!state_->scheduled){f.kind=ProgramCopyError::not_scheduled;return {};}
    try {std::vector<HostVoice*> voices;voices.reserve(state_->instances.size());for(auto& i:state_->instances)if(i.output)voices.push_back(&*i.output);
        auto result=mix_host_voices(voices.data(),voices.size(),frames,f.voice);if(!result)f.kind=ProgramCopyError::voice_failed;return result;
    }catch(const std::bad_alloc&){f.kind=ProgramCopyError::allocation_failed;}
    catch(const std::length_error&){f.kind=ProgramCopyError::host_capacity;}
    return {};
}
bool NativeProgramCopy::stop_outputs(const ProgramStep& event,ProgramCopyFailure& f){
    f={};if(!state_->scheduled){f.kind=ProgramCopyError::not_scheduled;return false;}
    if(event.kind!=ProgramStepKind::program_stop||event.owner!=state_->data){f.kind=ProgramCopyError::foreign_step;return false;}
    const auto mode=static_cast<std::uint16_t>(state_->data->effective_flags&1u);
    if(event.stop_mode_bit!=mode){f.kind=ProgramCopyError::wrong_stop_metadata;return false;}
    if(mode){f.kind=ProgramCopyError::unsupported_stop_mode;return false;}
    if(event.stop_reason==ProgramStopReason::command_kind7&&
       (event.command_index>=state_->data->commands.size()||state_->data->commands[event.command_index].kind()!=7||
        event.command.bytes!=state_->data->commands[event.command_index].bytes)){f.kind=ProgramCopyError::wrong_command;return false;}
    if(state_->stopped){f.kind=ProgramCopyError::stopped_copy;return false;}
    for(auto& i:state_->instances)if(i.output&&!i.output->stop_from_program(event,f.voice)){f.kind=ProgramCopyError::voice_failed;return false;}
    state_->stopped=true;return true;
}
} // namespace mka::sound
