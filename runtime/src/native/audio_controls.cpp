#include "mka/audio_controls.hpp"
#include <algorithm>
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
bool product(std::int64_t a,std::int64_t b,unsigned shift,std::int64_t& out){
    const auto wide=a*b; // Inputs are bounded source32/16 values at each stage.
    if(wide<std::numeric_limits<std::int32_t>::min()||wide>std::numeric_limits<std::int32_t>::max())return false;
    out=wide/(std::int64_t(1)<<shift);return true; // Source's signed truncation toward zero.
}
}
AudioControlFactors initial_program_control_factors()noexcept{return {0x4000,0,0x1000,0x4000,0,0x1000};}
std::optional<NativeAudioControls> decode_audio_controls(const ProgramStep& event,
    const AudioControlFactors& context,const AudioControlFactors& program,std::uint32_t mode,AudioControlFailure& failure,AudioControlUse use){
    failure={};auto fail=[&](AudioControlError e)->std::optional<NativeAudioControls>{failure.kind=e;return std::nullopt;};
    if(use!=AudioControlUse::fresh_start)return fail(AudioControlError::unsupported_dynamic_pan_update);
    if(event.kind!=ProgramStepKind::sample_start)return fail(AudioControlError::not_sample_start);
    if(!event.clip||!event.owner)return fail(AudioControlError::missing_clip);
    for(const auto value:{program.gain_q14,program.pan_q14,program.pitch_q12,program.secondary_gain_q14,program.secondary_pan_q14,program.secondary_pitch_q12})
        if(value<-32768||value>32767)return fail(AudioControlError::invalid_program_factor_encoding);
    if(event.owner->effective_flags&4u)return fail(AudioControlError::unsupported_spatial_mode);
    if(event.owner->effective_flags&8u)return fail(AudioControlError::unsupported_deferred_key_on);
    if(event.start.repeat_operand!=1)return fail(AudioControlError::unsupported_repeat);
    if(event.start.start_argument)return fail(AudioControlError::unsupported_start_argument);
    if(event.start.descriptor_word_28)return fail(AudioControlError::unsupported_descriptor_word);
    NativeAudioControls result;result.source_output_mode=mode;result.planes=event.clip->pcm.size();
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
    const auto q15=std::int32_t(gain_curve[curve_index])*2;
    std::array<std::int32_t,2> gains{q15,q15};
    if(mode){
        const auto magnitude=static_cast<unsigned>(routed_pan<0?-routed_pan:routed_pan);
        const bool phase_inverted=magnitude>8192;
        const auto table_index=phase_inverted ? 99u-(99u*(magnitude-8192u))/8192u : (99u*magnitude)/8192u;
        const auto attenuated_index=std::min<std::size_t>(199,curve_index+pan_attenuation[table_index]);
        auto attenuated=std::int32_t(gain_curve[attenuated_index])*2;
        if(phase_inverted)attenuated=-attenuated;
        // Positivepan attenuates/inverts left; negativepan right. Fresh3259C
        // sets50/52=F000,56=0; eachbranch publishes initial50/52 BEFORE
        // 31634 compareshistory, making bothdifferenceszero. No smoothingstate
        // is fabricated here; dynamicupdates have a separate namedfailure.
        gains[routed_pan<0?1:0]=attenuated;
    }
    for(std::size_t plane=0;plane<result.planes;++plane){
        result.gains_q15[plane]=gains;
        if(mode && result.planes==2)result.gains_q15[plane][plane==0?1:0]=0;
    }
    failure={};return result;
}
} // namespace mka::sound
