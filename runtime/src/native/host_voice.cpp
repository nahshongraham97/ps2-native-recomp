#include "mka/host_voice.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
namespace mka::sound {
namespace {
std::int64_t floor_div(std::int64_t value,std::int64_t divisor){auto q=value/divisor;return q-(value<0&&value%divisor!=0);}
bool valid_rate(const NativeEnvelopeRate& r){return r.period!=0 && (!r.exponential||r.step<=0);}
bool valid_profile(const NativeEnvelopeProfile& p){return valid_rate(p.attack)&&valid_rate(p.decay)&&valid_rate(p.sustain)&&valid_rate(p.release)&&p.attack.step>0&&p.decay.step<=0&&p.release.step<0&&p.sustain_threshold_q15>=0&&p.sustain_threshold_q15<=32768;}
}
struct HostVoice::Data {
    std::shared_ptr<const PreparedProgram> owner;
    std::shared_ptr<const PreparedNormal> clip;
    NativeAudioControls controls;
    NativeEnvelopeProfile profile;
    std::uint32_t output_rate=0;
    std::uint64_t index=0,fraction=0,envelope_fraction=0,counter=0;
    std::int32_t level=0;
    enum class Phase { attack, decay, sustain, release, off } phase=Phase::attack;
    bool frozen=false;
    void envelope_tick(){
        if(phase==Phase::off)return;
        const auto& r=phase==Phase::attack?profile.attack:phase==Phase::decay?profile.decay:phase==Phase::sustain?profile.sustain:profile.release;
        if(counter==r.period-1){counter=0;std::int64_t delta=r.step;
            if(r.exponential)delta=delta*level/32768; // Source signed truncation toward zero.
            level=static_cast<std::int32_t>(std::clamp<std::int64_t>(std::int64_t(level)+delta,0,32767));
        }else ++counter;
        if(phase==Phase::attack&&level>=32767)phase=Phase::decay;
        else if(phase==Phase::decay&&level<=profile.sustain_threshold_q15)phase=Phase::sustain;
        else if(phase==Phase::release&&level<=0)phase=Phase::off;
    }
    bool render(std::size_t frames,std::vector<float>& output,HostVoiceFailure& failure){
        const std::uint64_t denominator=std::uint64_t(output_rate)*4096;
        if(controls.sample_step_q12>std::numeric_limits<std::uint64_t>::max()/48000){failure.kind=HostVoiceError::arithmetic_overflow;return false;}
        const auto increment=controls.sample_step_q12*48000;
        for(std::size_t frame=0;frame<frames;++frame){
            if(phase==Phase::off)break;
            const auto samples=clip->pcm[0].samples.size();
            // Encoded stop/repeat flags are not guessed from reaching raw EOF.
            if(index>=samples || samples-index<2){failure.kind=HostVoiceError::unsupported_sample_end_control;failure.frame_index=frame;return false;}
            const auto fraction_q12=static_cast<std::int64_t>(fraction*4096/denominator);
            std::array<std::int64_t,2> interpolated{};
            for(std::size_t p=0;p<controls.planes;++p){const auto& pcm=clip->pcm[p].samples;
                interpolated[p]=floor_div(std::int64_t(pcm[static_cast<std::size_t>(index)])*(4096-fraction_q12)+std::int64_t(pcm[static_cast<std::size_t>(index)+1])*fraction_q12,4096);
            }
            if(!frozen){
                if(increment>std::numeric_limits<std::uint64_t>::max()-fraction){failure.kind=HostVoiceError::arithmetic_overflow;return false;}
                const auto phase_value=fraction+increment;const auto advance=phase_value/denominator;
                if(advance>std::numeric_limits<std::uint64_t>::max()-index){failure.kind=HostVoiceError::arithmetic_overflow;return false;}
                index+=advance;fraction=phase_value%denominator;
            }
            envelope_fraction+=48000;
            const auto ticks=envelope_fraction/output_rate;envelope_fraction%=output_rate;
            for(std::uint64_t k=0;k<ticks;++k)envelope_tick();
            for(std::size_t p=0;p<controls.planes;++p){
                const auto value=floor_div(interpolated[p]*level,32768);
                for(std::size_t channel=0;channel<2;++channel){const auto gained=floor_div(value*controls.gains_q15[p][channel],32768);output[frame*2+channel]+=static_cast<float>(gained)/32768.0f;}
            }
        }
        return true;
    }
};
HostVoice::HostVoice(std::unique_ptr<Data> data):data_(std::move(data)){}
HostVoice::~HostVoice()=default;
HostVoice::HostVoice(HostVoice&&)noexcept=default;
HostVoice& HostVoice::operator=(HostVoice&&)noexcept=default;
std::optional<HostVoice> HostVoice::start(const ProgramStep& event,const AudioControlFactors& context,
    const AudioControlFactors& program,std::uint32_t mode,const NativeEnvelopeProfile& profile,std::uint32_t rate,HostVoiceFailure& failure){
    failure={};if(!rate){failure.kind=HostVoiceError::invalid_output_rate;return std::nullopt;}
    if(!valid_profile(profile)){failure.kind=HostVoiceError::unsupported_envelope;return std::nullopt;}
    auto controls=decode_audio_controls(event,context,program,mode,failure.control);
    if(!controls){failure.kind=HostVoiceError::control_failed;return std::nullopt;}
    for(const auto& pcm:event.clip->pcm)if(pcm.samples.size()<2||pcm.samples.size()!=event.clip->pcm[0].samples.size()){
        failure.kind=HostVoiceError::invalid_clip;return std::nullopt;
    }
    try{auto d=std::make_unique<Data>();d->owner=event.owner;d->clip=event.clip;d->controls=*controls;d->profile=profile;d->output_rate=rate;return HostVoice(std::move(d));}
    catch(const std::bad_alloc&){failure.kind=HostVoiceError::allocation_failed;}
    catch(const std::length_error&){failure.kind=HostVoiceError::host_capacity;}return std::nullopt;
}
std::optional<std::vector<float>> HostVoice::render(std::size_t frames,HostVoiceFailure& failure){
    failure={};if(!data_){failure.kind=HostVoiceError::moved_voice;return std::nullopt;}
    try{Data next=*data_;std::vector<float> output;if(frames>output.max_size()/2){failure.kind=HostVoiceError::host_capacity;return std::nullopt;}output.resize(frames*2);
        if(!next.render(frames,output,failure))return std::nullopt;
        *data_=std::move(next);return output;
    }catch(const std::bad_alloc&){failure.kind=HostVoiceError::allocation_failed;}
     catch(const std::length_error&){failure.kind=HostVoiceError::host_capacity;}return std::nullopt;
}
bool HostVoice::validate_program_owner(const std::shared_ptr<const PreparedProgram>& owner,HostVoiceFailure& failure) const noexcept{
    failure={};if(!data_){failure.kind=HostVoiceError::moved_voice;return false;}
    if(!owner||owner!=data_->owner){failure.kind=HostVoiceError::foreign_program;return false;}
    return true;
}
bool HostVoice::stop_for_resource_retirement(const std::shared_ptr<const PreparedProgram>& owner,HostVoiceFailure& failure) noexcept{
    if(!validate_program_owner(owner,failure))return false;
    // Normal2E654->3403C/342E8 freezes source position before release.
    data_->frozen=true;if(data_->phase!=Data::Phase::off)data_->phase=Data::Phase::release;return true;
}
bool HostVoice::stop_from_program(const ProgramStep& event,HostVoiceFailure& failure){
    failure={};if(!data_){failure.kind=HostVoiceError::moved_voice;return false;}
    if(event.kind!=ProgramStepKind::program_stop){failure.kind=HostVoiceError::wrong_stop_event;return false;}
    return stop_for_resource_retirement(event.owner,failure);
}
bool HostVoice::active()const noexcept{return data_&&data_->phase!=Data::Phase::off;}
std::uint64_t HostVoice::source_index()const noexcept{return data_?data_->index:0;}
std::uint32_t HostVoice::output_rate()const noexcept{return data_?data_->output_rate:0;}
std::optional<std::vector<float>> mix_host_voices(HostVoice*const* voices,std::size_t count,std::size_t frames,HostVoiceFailure& failure){
    failure={};if(count&&!voices){failure.kind=HostVoiceError::null_voice;return std::nullopt;}
    try{std::vector<float> output;if(frames>output.max_size()/2){failure.kind=HostVoiceError::host_capacity;return std::nullopt;}output.resize(frames*2);
        std::vector<HostVoice::Data> next;if(count>next.max_size()){failure.kind=HostVoiceError::host_capacity;return std::nullopt;}next.reserve(count);std::uint32_t rate=0;
        for(std::size_t i=0;i<count;++i){failure.voice_index=i;
            if(!voices[i]||!voices[i]->data_){failure.kind=HostVoiceError::null_voice;return std::nullopt;}
            for(std::size_t j=0;j<i;++j)if(voices[j]==voices[i]){failure.kind=HostVoiceError::duplicate_voice;return std::nullopt;}
            if(i&&voices[i]->output_rate()!=rate){failure.kind=HostVoiceError::output_rate_mismatch;return std::nullopt;}rate=voices[i]->output_rate();
            next.push_back(*voices[i]->data_);
            if(!next.back().render(frames,output,failure))return std::nullopt;
        }
        for(std::size_t i=0;i<count;++i)*voices[i]->data_=std::move(next[i]);
        failure={};return output;
    }catch(const std::bad_alloc&){failure.kind=HostVoiceError::allocation_failed;}
     catch(const std::length_error&){failure.kind=HostVoiceError::host_capacity;}return std::nullopt;
}
} // namespace mka::sound
