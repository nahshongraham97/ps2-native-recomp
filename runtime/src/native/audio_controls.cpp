#include "mka/audio_controls.hpp"
#include <algorithm>
#include <bit>
#include <limits>
namespace mka::sound {
namespace {
// Exact source gain-curve data at originalMSL object102AC/file1033C.
constexpr std::uint16_t gain_curve[200]={
    16383,15466,14601,13784,13013,12285,11598,10949,10336,9758,
    9212,8697,8210,7751,7318,6908,6522,6157,5812,5487,
    5180,4890,4617,4359,4115,3885,3667,3462,3268,3085,
    2913,2750,2596,2451,2314,2184,2062,1947,1838,1735,
    1638,1546,1460,1378,1301,1228,1159,1094,1033,975,
    921,869,821,775,731,690,652,615,581,548,
    518,489,461,435,411,388,366,346,326,308,
    291,275,259,245,231,218,206,194,183,173,
    163,154,146,137,130,122,115,109,103,97,
    92,86,82,77,73,69,65,61,58,54,
    51,48,46,43,41,38,36,34,32,30,
    29,27,25,24,23,21,20,19,18,17,
    16,15,14,13,13,12,11,10,10,9,
    9,8,8,7,7,6,6,6,5,5,
    5,4,4,4,4,3,3,3,3,3,
    2,2,2,2,2,2,2,1,1,1,
    1,1,1,1,1,1,1,1,1,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,
};
// Exact source pan attenuation bytes1043C..1049F (one100byte table).
// Outerphase address1049F uses signednegative offsets into this same table.
constexpr std::uint8_t pan_attenuation[100]={
    0,0,0,1,1,1,2,2,2,3,
    3,3,4,4,4,5,5,5,6,6,
    6,7,7,7,8,8,9,9,9,10,
    10,11,11,11,12,12,13,13,14,14,
    15,15,16,16,17,17,18,18,19,20,
    20,21,21,22,23,23,24,25,25,26,
    27,27,28,29,30,31,31,32,33,34,
    35,36,37,38,40,41,42,43,45,46,
    47,49,51,52,54,56,58,61,63,66,
    69,72,76,81,86,92,101,112,132,200,
};
constexpr auto linear_gain_curve=[] {
    std::array<std::uint16_t,200> result{};
    for(std::uint32_t i=0;i<200;++i)result[199-i]=static_cast<std::uint16_t>(i*16383u/199u);
    return result;
}();
constexpr auto linear_pan_attenuation=[] {
    std::array<std::uint8_t,100> result{};
    for(std::size_t i=0;i<100;++i)result[i]=static_cast<std::uint8_t>(2*i);
    return result;
}();
bool product(std::int64_t a,std::int64_t b,unsigned shift,std::int64_t& out){
    const auto wide=a*b; // Inputs are bounded source32/16 values at each stage.
    if(wide<std::numeric_limits<std::int32_t>::min()||wide>std::numeric_limits<std::int32_t>::max())return false;
    out=wide/(std::int64_t(1)<<shift);return true; // Source's signed truncation toward zero.
}
}
AudioControlFactors initial_program_control_factors()noexcept{return {0x4000,0,0x1000,0x4000,0,0x1000};}
static std::optional<NativeAudioControls> decode_controls(const ProgramStep& event,
    const AudioControlFactors& context,const AudioControlFactors& program,std::uint32_t mode,AudioControlFailure& failure,AudioControlHistory& history,AudioControlCurve curve){
    failure={};auto fail=[&](AudioControlError e)->std::optional<NativeAudioControls>{failure.kind=e;return std::nullopt;};
    if(!valid_audio_control_curve(curve))return fail(AudioControlError::unsupported_curve);
    if(event.kind!=ProgramStepKind::sample_start)return fail(AudioControlError::not_sample_start);
    if(!event.clip||!event.owner)return fail(AudioControlError::missing_clip);
    for(const auto value:{program.gain_q14,program.pan_q14,program.pitch_q12,program.secondary_gain_q14,program.secondary_pan_q14,program.secondary_pitch_q12})
        if(value<-32768||value>32767)return fail(AudioControlError::invalid_program_factor_encoding);
    if(event.owner->effective_flags&4u)return fail(AudioControlError::unsupported_spatial_mode);
    if(event.owner->effective_flags&8u)return fail(AudioControlError::unsupported_deferred_key_on);
    if(event.start.repeat_operand!=1)return fail(AudioControlError::unsupported_repeat);
    if(event.start.start_argument)return fail(AudioControlError::unsupported_start_argument);
    if(event.start.descriptor_word_28)return fail(AudioControlError::unsupported_descriptor_word);
    NativeAudioControls result;result.source_output_mode=mode;result.planes=event.clip->pcm.size();result.curve=curve;
    if(result.planes<1||result.planes>2)return fail(AudioControlError::invalid_plane_count);
    std::int64_t gain=0,pitch=0,temp=0;
    if(!product(context.gain_q14,program.gain_q14,14,temp)||!product(event.start.parameter_44,temp,14,gain)||
       !product(context.pitch_q12,program.pitch_q12,12,temp)||!product(event.start.parameter_48,temp,12,pitch))return fail(AudioControlError::unsupported_arithmetic_overflow);
    std::int64_t pan=std::int64_t(context.pan_q14)+program.pan_q14+event.start.parameter_46;
    if(event.start.set_descriptor_flag_4){
        if(!product(gain,context.secondary_gain_q14,14,temp)||!product(temp,program.secondary_gain_q14,14,gain)||
           !product(pitch,context.secondary_pitch_q12,12,temp)||!product(program.secondary_pitch_q12,temp,12,pitch))return fail(AudioControlError::unsupported_arithmetic_overflow);
        pan+=std::int64_t(context.secondary_pan_q14)+program.secondary_pan_q14;
    }
    if(pan<std::numeric_limits<std::int32_t>::min()||pan>std::numeric_limits<std::int32_t>::max())return fail(AudioControlError::unsupported_arithmetic_overflow);
    result.effective_gain_q14=static_cast<std::int32_t>(std::clamp<std::int64_t>(gain,0,0x4000));
    result.effective_pan_q14=static_cast<std::int32_t>(std::clamp<std::int64_t>(pan,-0x4000,0x4000));
    result.effective_pitch_q12=pitch;
    if(pitch<=0)return fail(AudioControlError::unsupported_zero_pitch);
    const auto rate=event.clip->source.rate_value();
    result.desired_frequency_numerator=std::uint64_t(rate)*static_cast<std::uint64_t>(pitch);
    failure.desired_frequency_numerator=result.desired_frequency_numerator;
    // Beyond this domain original low32 MULT overflow changes meaning, which is
    // not a verified game-parameter rule. Fail explicitly, never wrap the host rate.
    if(result.desired_frequency_numerator>0xffffffffu)return fail(AudioControlError::unsupported_arithmetic_overflow);
    result.sample_step_q12=result.desired_frequency_numerator/48000u;
    if(!result.sample_step_q12)return fail(AudioControlError::unsupported_zero_pitch);
    result.reference_packed_pitch_word=static_cast<std::uint16_t>(result.sample_step_q12&0xffffu);
    result.reference_word_truncated=result.sample_step_q12>0xffffu;
    const auto curve_index=std::size_t((0x4000-result.effective_gain_q14)*199/0x4000);
    auto routed_pan=result.effective_pan_q14;
    // Mode1 folds the rear/phase region into the ordinary positive-gain region.
    if(mode==1){if(routed_pan>8192)routed_pan=16384-routed_pan;
                else if(routed_pan<-8192)routed_pan=-16384-routed_pan;}
    result.routed_pan_q14=routed_pan;
    const auto gain_at=[curve](std::size_t index){return curve==AudioControlCurve::startup_linear?linear_gain_curve[index]:gain_curve[index];};
    const auto pan_at=[curve](std::size_t index){return curve==AudioControlCurve::startup_linear?linear_pan_attenuation[index]:pan_attenuation[index];};
    auto next=history;
    const auto narrow=[](std::int32_t value){return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));};
    // The source emits a signed15-bit constant. Convert that game operand
    // to native Q15 gain; no volume-register or sweep engine is retained.
    const auto q15=[](std::int16_t value){
        const auto word=static_cast<std::uint16_t>(value)&0x7fffu;
        return 2*(word&0x4000u?static_cast<std::int32_t>(word)-32768:static_cast<std::int32_t>(word));
    };
    for(std::size_t plane=0;plane<result.planes;++plane){
        const auto base=static_cast<std::int32_t>(gain_at(curve_index));
        std::int16_t left=static_cast<std::int16_t>(base),right=left;
        if(mode){
            const bool positive=routed_pan>=0;
            const bool rear=routed_pan>8192||routed_pan<-8192;
            const auto magnitude=static_cast<unsigned>(positive?routed_pan:-routed_pan);
            const auto index=rear?99u-(99u*(magnitude-8192u))/8192u:(99u*magnitude)/8192u;
            const auto attenuated=static_cast<std::int32_t>(gain_at(std::min<std::size_t>(199,curve_index+pan_at(index))));
            const bool negative=positive?
                ((rear?next.right<=0:next.right<0)&&next.right!=-4096):
                (next.left<=0&&next.right!=-4096);
            const auto sign=negative?-1:1;
            if(positive){right=narrow(sign*base);left=narrow(sign*(rear?-attenuated:attenuated));}
            else {left=narrow(sign*base);right=narrow(sign*(rear?-attenuated:attenuated));}
            const std::int16_t region=positive?(rear?4:2):(rear?3:1);
            if(next.right==-4096){next.left=left;next.right=right;}
            const auto abs=[](std::int32_t value){return value<0?-value:value;};
            if(next.region!=region&&
               (abs(std::int32_t(next.left)-left)>=10001||abs(std::int32_t(next.right)-right)>=10001)){
                const auto delta=narrow(positive?std::int32_t(next.left)-left:std::int32_t(next.right)-right);
                const auto divisor=static_cast<std::int16_t>(abs(delta/2500));
                next.transition=AudioControlTransition{delta,divisor};
                next.remaining=divisor;
            }
            if(next.remaining>0){
                if(!next.transition||next.transition->divisor<=0)return fail(AudioControlError::invalid_history);
                const auto change=next.transition->delta/next.transition->divisor;
                if(positive)left=narrow(std::int32_t(next.left)-change);
                else right=narrow(std::int32_t(next.right)-change);
                --next.remaining;
            }
            next.region=region;next.left=left;next.right=right;
        }
        //9F1C calls959C separately for each plane before its routing mask.
        result.gains_q15[plane]={q15(left),q15(right)};
        if(mode&&result.planes==2)result.gains_q15[plane][plane==0?1:0]=0;
    }
    history=next;
    failure={};return result;
}
std::optional<NativeAudioControls> decode_audio_controls(const ProgramStep& event,
    const AudioControlFactors& context,const AudioControlFactors& program,
    std::uint32_t mode,AudioControlFailure& failure,AudioControlUse use,AudioControlCurve curve){
    if(!valid_audio_control_curve(curve)){failure={};failure.kind=AudioControlError::unsupported_curve;return {};}
    if(use!=AudioControlUse::fresh_start){failure={};failure.kind=AudioControlError::unsupported_dynamic_pan_update;return {};}
    AudioControlHistory history;
    return decode_controls(event,context,program,mode,failure,history,curve);
}
std::optional<NativeAudioControls> decode_audio_controls_with_history(const ProgramStep& event,
    const AudioControlFactors& context,const AudioControlFactors& program,
    std::uint32_t mode,AudioControlHistory& history,AudioControlFailure& failure,AudioControlCurve curve){
    return decode_controls(event,context,program,mode,failure,history,curve);
}
} // namespace mka::sound
