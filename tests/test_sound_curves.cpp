#include "mka/playback_owner.hpp"
#include "fixtures/retail_sound_curve_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
using namespace mka::sound;
namespace {
void check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
constexpr auto initial=AudioControlCurve::initial_nonlinear;
constexpr auto linear=AudioControlCurve::startup_linear;
const NativeEnvelopeProfile profile{{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048};
}
int main(int argc,char** argv)try{
    check(argc==2,"original MS2 argument required");
    ArchiveFailure archive_failure;
    auto archive=SoundArchive::open(argv[1],archive_failure);
    auto raw=archive->read_bank("announcr",archive_failure);
    BankPreparationReport report;PreparationFailure preparation;
    auto bank=PreparedBank::prepare(std::move(*raw),report,preparation);
    auto program=bank->prepare_deferred_normal(0,preparation);
    ProgramFailure program_failure;
    auto executor=ProgramExecutor::create(program,{0},program_failure);
    auto original=executor->step({0},program_failure);
    check(original&&original->kind==ProgramStepKind::sample_start&&original->start.parameter_44==16383,
        "pinned original announcr gain operand");
    auto event=*original;
    // Explicit coefficient-domain test inputs; sample identity remains real.
    event.start.parameter_44=16384;event.start.parameter_46=0;event.start.parameter_48=4096;
    const auto neutral=initial_program_control_factors();AudioControlFailure failure;
    for(const auto curve:{initial,linear}){
        const auto& expected=curve==linear?sound_curve_fixture::linear_gain:sound_curve_fixture::initial_gain;
        const auto& pan=curve==linear?sound_curve_fixture::linear_pan:sound_curve_fixture::initial_pan;
        for(unsigned index=0;index<200;++index){
            auto context=neutral;context.gain_q14=16384-static_cast<int>((index*16384u+198u)/199u);
            auto result=decode_audio_controls(event,context,neutral,3,failure,AudioControlUse::fresh_start,curve);
            check(result&&result->curve==curve&&result->effective_gain_q14==context.gain_q14&&
                result->gains_q15[0][0]==2*expected[index]&&result->gains_q15[0][1]==2*expected[index],
                "all200 original/static or startup-generated gain rows");
        }
        for(unsigned index=0;index<100;++index)for(int sign:{-1,1}){
            auto context=neutral;context.pan_q14=sign*static_cast<int>((index*8192u+98u)/99u);
            auto result=decode_audio_controls(event,context,neutral,3,failure,AudioControlUse::fresh_start,curve);
            const auto side=context.pan_q14<0?1u:0u;
            check(result&&result->gains_q15[0][side]==2*expected[std::min<unsigned>(199,pan[index])]&&
                result->gains_q15[0][1-side]==32766,"all100 source pan rows in both directions");
        }
    }
    auto half=neutral;half.gain_q14=8192;
    const auto old_half=decode_audio_controls(event,half,neutral,3,failure,AudioControlUse::fresh_start,initial);
    const auto retail_half=decode_audio_controls(event,half,neutral,3,failure,AudioControlUse::fresh_start,linear);
    check(old_half&&retail_half&&old_half->gains_q15[0][0]==108&&retail_half->gains_q15[0][0]==16464,
        "actual initialization changes half-gain coefficient rather than using nonlinear fallback");
    for(int position:{-16384,-12288,-8192,-4096,0,4096,8192,12288,16384}){
        auto context=neutral;context.pan_q14=position;
        auto result=decode_audio_controls(event,context,neutral,3,failure,AudioControlUse::fresh_start,linear);
        const auto magnitude=std::abs(position);
        const int expected=magnitude==16384?-32766:magnitude==12288?-16300:magnitude==8192?164:magnitude==4096?16628:32766;
        check(result&&result->gains_q15[0][position<0?1:0]==expected,"linear ordinary/phase pan source goldens");
        auto folded=decode_audio_controls(event,context,neutral,1,failure,AudioControlUse::fresh_start,linear);
        check(folded&&folded->gains_q15[0][0]>=0&&folded->gains_q15[0][1]>=0,"mode1 fold remains independent of curve policy");
    }
    check(!decode_audio_controls(event,neutral,neutral,3,failure,AudioControlUse::fresh_start,static_cast<AudioControlCurve>(99))&&
        failure.kind==AudioControlError::unsupported_curve,"unknown policy is refused, never a table fallback");

    AudioBanksFailure bf;
    auto session=AudioBankSession::open(argv[1],std::nullopt,bf);
    auto handle=session->load_bank("announcr",bf);
    check(handle&&session->prepare_bank(*handle,report,preparation,bf),"actual native shared bank");
    auto a=session->share_native_services(bf),b=session->share_native_services(bf);
    PlaybackFailure pf;
    check(!NativePlaybackOwner::create(std::move(*a),48000,static_cast<AudioControlCurve>(99),pf)&&
        pf.kind==PlaybackError::invalid_control_curve,"native owner requires a supported explicit curve before consuming Session");
    auto nonlinear=NativePlaybackOwner::create(std::move(*a),48000,initial,pf);
    auto retail=NativePlaybackOwner::create(std::move(*b),48000,linear,pf);
    check(nonlinear&&retail,"both source policies own the same actual native bank registry");
    auto nc=nonlinear->create_sound_context({3},pf),rc=retail->create_sound_context({3},pf);
    NativePlayParameters params{half,neutral,profile,{1000},3,0,{-1,0}};
    auto np=nonlinear->play(*nc,*handle,0,params,pf),rp=retail->play(*rc,*handle,0,params,pf);
    check(np&&rp&&nonlinear->tick(*np,{1000},pf)&&retail->tick(*rp,{1000},pf),"actual native private copies/voices start");
    auto n=nonlinear->mix(8192,pf),l=retail->mix(8192,pf);
    check(n&&l&&n->size()==l->size(),"actual PCM from both policy paths");
    const auto peak=static_cast<std::size_t>(std::max_element(n->begin(),n->end(),[](float x,float y){return std::abs(x)<std::abs(y);})-n->begin());
    const double nq=std::round((*n)[peak]*32768.0),lq=std::round((*l)[peak]*32768.0);
    const double ratio=8232.0/54.0;
    // For identical integer pre-gain sample v, floor(v*G/32768) differs
    // from the scaled other result by at most ratio+1 quantization units.
    check(std::abs(nq)>=4&&std::abs(lq-ratio*nq)<=ratio+1,
        "native owner policy reaches Copy/HostVoice and real PCM, including integer quantization");
    check(nonlinear->stop(*np,pf)&&retail->stop(*rp,pf)&&nonlinear->mix(32,pf)&&retail->mix(32,pf)&&
        nonlinear->release(*np,pf)&&retail->release(*rp,pf)&&
        nonlinear->release_sound_context(*nc,pf)&&retail->release_sound_context(*rc,pf),"actual policy test references retire");
    check(session->unload_bank(*handle,bf)&&session->close(bf),"shared source Session closes after both native copies retire");
    std::puts("PASS original600 curve coefficients/directional pan/phase/unknown policy and real native PCM propagation; no device/game");
}catch(const std::exception& e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}
