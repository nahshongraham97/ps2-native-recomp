#include "mka/host_voice.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <cmath>
using namespace mka::sound;
namespace {
void check(bool v,const char* s){if(!v){std::fprintf(stderr,"FAIL: %s\n",s);std::exit(1);}}
const NativeEnvelopeProfile observed_profile{{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048};
// Explicit source-init profile, not a claim it was installed on the saved voices.
const NativeEnvelopeProfile init_profile{{1,14336,false},{1,-16384,true},{1048576,4,false},{1,-256,false},32768};
std::int64_t floor_div(std::int64_t v,std::int64_t d){auto q=v/d;return q-(v<0&&v%d!=0);}
}
int main(int argc,char** argv){
 if(argc!=2){std::fprintf(stderr,"usage: test_host_voice <pinnedMS2>\n");return 2;}
 ArchiveFailure af;auto archive=SoundArchive::open(argv[1],af);check(bool(archive),"explicitMS2");auto raw=archive->read_bank("announcr",af);check(bool(raw),"sourceactualannouncrname");
 BankPreparationReport report;PreparationFailure pf;auto bank=PreparedBank::prepare(std::move(*raw),report,pf);check(bool(bank),"prepareactualbank");auto p=bank->prepare_deferred_normal(0,pf);check(bool(p),"actualdeferredselector0");
 ProgramFailure ef;auto executor=ProgramExecutor::create(p,{0},ef);check(bool(executor),"executor");auto emitted=executor->step({0},ef);check(emitted&&emitted->kind==ProgramStepKind::sample_start,"actualstart");const ProgramStep event=*emitted;
 const auto neutral=initial_program_control_factors();auto actual=neutral;actual.gain_q14=1866;
 AudioControlFailure cf;auto c=decode_audio_controls(event,neutral,neutral,3,cf);check(c&&c->effective_gain_q14==16383&&c->effective_pitch_q12==4096&&c->gains_q15[0][0]==32766&&c->gains_q15[0][1]==32766,"sourceLUTfirstindexandcenter");
 auto low=decode_audio_controls(event,neutral,actual,3,cf);check(low&&low->effective_gain_q14==1865&&low->gains_q15[0][0]==0,"actualperplayfactorisnotunity");
 auto half=neutral;half.gain_q14=8192;auto nonlinear=decode_audio_controls(event,neutral,half,3,cf);check(nonlinear&&nonlinear->effective_gain_q14==8191&&nonlinear->gains_q15[0][0]==108,"exactcurveindex99notlineargain");
 auto negative=neutral;negative.gain_q14=-3;auto trunc=decode_audio_controls(event,neutral,negative,3,cf);check(trunc&&trunc->effective_gain_q14==0,"signednegativegainclampedaftersourcetruncation");
 auto invalid=neutral;invalid.pitch_q12=32768;check(!decode_audio_controls(event,neutral,invalid,3,cf)&&cf.kind==AudioControlError::invalid_program_factor_encoding,"programfieldsareoriginalsigned16");
 ProgramStep changed=event;changed.start.repeat_operand=2;check(!decode_audio_controls(changed,neutral,neutral,3,cf)&&cf.kind==AudioControlError::unsupported_repeat,"repeatnotguessed");changed=event;changed.start.start_argument=1;check(!decode_audio_controls(changed,neutral,neutral,3,cf)&&cf.kind==AudioControlError::unsupported_start_argument,"startargnotignored");changed=event;changed.start.descriptor_word_28=1;check(!decode_audio_controls(changed,neutral,neutral,3,cf)&&cf.kind==AudioControlError::unsupported_descriptor_word,"descriptorwordnotignored");
 auto pan=neutral;
 for(const auto value:{-16384,-12288,-8192,-4096,0,4096,8192,12288,16384}){
  pan.pan_q14=value;auto mapped=decode_audio_controls(event,pan,neutral,3,cf);check(bool(mapped),"freshnoncenterstart");
  const auto magnitude=value<0?-value:value;const auto atten=magnitude==4096?10360:magnitude==8192?0:magnitude==12288?-10360:magnitude==16384?-32766:32766;
  const auto side=value<0?1:0;check(mapped->gains_q15[0][side]==atten&&mapped->gains_q15[0][1-side]==32766,"fixedoriginalpanLUTandphasegoldens");
  auto folded=decode_audio_controls(event,pan,neutral,1,cf);check(bool(folded),"mode1fold");const auto expected_fold=value>8192?16384-value:value<-8192?-16384-value:value;check(folded->routed_pan_q14==expected_fold&&folded->gains_q15[0][0]>=0&&folded->gains_q15[0][1]>=0,"mode1hasnofreshphaseinversion");
  auto mono=decode_audio_controls(event,pan,neutral,0,cf);check(mono&&mono->gains_q15[0][0]==32766&&mono->gains_q15[0][1]==32766,"mode0ignoresallpan");
 }
 for(int magnitude=0;magnitude<=16384;++magnitude){
  pan.pan_q14=magnitude;auto positive=decode_audio_controls(event,pan,neutral,3,cf);pan.pan_q14=-magnitude;auto negative_pan=decode_audio_controls(event,pan,neutral,3,cf);
  check(positive&&negative_pan&&positive->gains_q15[0][0]==negative_pan->gains_q15[0][1]&&positive->gains_q15[0][1]==negative_pan->gains_q15[0][0],"allpanvaluesmirrorwithouttableoverrun");
  check(positive->gains_q15[0][1]==32766&&(magnitude<=8192?positive->gains_q15[0][0]>=0:positive->gains_q15[0][0]<=0),"sourcefreshphasequadrant");
 }
 check(!decode_audio_controls(event,neutral,neutral,3,cf,AudioControlUse::dynamic_update)&&cf.kind==AudioControlError::unsupported_dynamic_pan_update,"dynamichistorycannotpretendfresh");
 auto flags=std::make_shared<PreparedProgram>(*p);flags->effective_flags|=4;changed=event;changed.owner=flags;check(!decode_audio_controls(changed,neutral,neutral,3,cf)&&cf.kind==AudioControlError::unsupported_spatial_mode,"spatialnotunityfallback");flags->effective_flags=8;check(!decode_audio_controls(changed,neutral,neutral,3,cf)&&cf.kind==AudioControlError::unsupported_deferred_key_on,"deferredkeyonnotimmediate");
 auto wide=neutral;check(event.clip->source.rate_value()>8192,"actualratepermitswidepitchtestwithinprovedMULTdomain");wide.pitch_q12=static_cast<std::int32_t>(0xffffffffu/event.clip->source.rate_value());changed=event;changed.start.set_descriptor_flag_4=false;auto wc=decode_audio_controls(changed,wide,neutral,3,cf);check(wc&&wc->sample_step_q12>65535&&wc->sample_step_q12==std::uint64_t(event.clip->source.rate_value())*wide.pitch_q12/48000&&wc->reference_word_truncated&&wc->reference_packed_pitch_word==(wc->sample_step_q12&65535),"widepitchisnotdevicewordtruncation");
 auto overflow=neutral;overflow.pitch_q12=2147483647;check(!decode_audio_controls(event,overflow,neutral,3,cf)&&cf.kind==AudioControlError::unsupported_arithmetic_overflow,"sourcearithmeticoverflowneverwraps");
 HostVoiceFailure f;auto make=[&](std::uint32_t rate=48000,const NativeEnvelopeProfile& profile=observed_profile){auto v=HostVoice::start(event,neutral,neutral,3,profile,rate,f);check(bool(v),"realcliphostvoicestart");return std::move(*v);};
 auto badprofile=observed_profile;badprofile.attack.exponential=true;check(!HostVoice::start(event,neutral,neutral,3,badprofile,48000,f)&&f.kind==HostVoiceError::unsupported_envelope,"unprovedexponentialattackrejected");check(!HostVoice::start(event,neutral,neutral,3,observed_profile,0,f)&&f.kind==HostVoiceError::invalid_output_rate,"explicithostrate");
 auto a=make(),b=make();auto together=a.render(512,f);auto first=b.render(137,f),second=b.render(375,f);check(together&&first&&second,"renders");first->insert(first->end(),second->begin(),second->end());check(*first==*together&&a.source_index()==b.source_index(),"blockpartitioninvariant");
 check(std::any_of(together->begin(),together->end(),[](float x){return x!=0;}),"actualPCMproducesnonzeros");
 // Independent scalar arithmetic for the first 512 frames: ordinary reference
 // interpolation, explicit saved profile, and original full-gain LUT value.
 auto oracle=make();auto out=oracle.render(512,f);check(bool(out),"oraclebatch");std::uint64_t phase=0;int level=0,stage=0;const auto step=c->sample_step_q12;bool saw_negative=false;
 for(std::size_t i=0;i<512;++i){auto ix=phase/4096;auto frac=phase%4096;const auto& pcm=event.clip->pcm[0].samples;auto sample=floor_div(std::int64_t(pcm[ix])*(4096-static_cast<std::int64_t>(frac))+std::int64_t(pcm[ix+1])*static_cast<std::int64_t>(frac),4096);phase+=step;
  if(stage==0){level=std::min(32767,level+14336);if(level==32767)stage=1;}else if(stage==1){level=std::max(0,level+(-16384*level)/32768);if(level<=2048)stage=2;}else level=std::min(32767,level+14336);
  auto gain=floor_div(floor_div(sample*level,32768)*32766,32768);float expected=static_cast<float>(gain)/32768.0f;check((*out)[2*i]==expected&&(*out)[2*i+1]==expected,"independentsignedscalarreference");saw_negative|=sample<0;
 }
 check(saw_negative,"referenceexercisesnegativesamples");
 auto rear=neutral;rear.pan_q14=16384;auto phase_voice=HostVoice::start(event,rear,neutral,3,observed_profile,48000,f);check(bool(phase_voice),"hostsignedgainstart");auto phase_output=phase_voice->render(512,f);check(bool(phase_output),"hostsignedgainrender");
 for(std::size_t i=0;i<512;++i){check((*phase_output)[2*i+1]==(*out)[2*i+1],"phaseinversionpreservesright");const auto pair_sum=(*phase_output)[2*i]+(*out)[2*i];check(pair_sum<=0&&pair_sum>=-1.0f/32768.0f,"signedQ15leftinversionroundsdown");}
 auto resampled=make(44100);check(bool(resampled.render(100,f))&&resampled.source_index()==(100*c->sample_step_q12*48000)/(44100ull*4096),"explicitnon48000hostrate");
 auto stopping=make();check(bool(stopping.render(40,f)),"prestop");ProgramStep stop;stop.kind=ProgramStepKind::program_stop;stop.owner=p;auto index=stopping.source_index();check(stopping.stop_from_program(stop,f),"actualnormalstop");check(bool(stopping.render(32,f))&&!stopping.active()&&stopping.source_index()==index,"stopfreezesphaseandreleases");auto silence=stopping.render(20,f);check(silence&&std::all_of(silence->begin(),silence->end(),[](float x){return x==0;}),"offisexplicitsilence");
 auto foreign=stop;foreign.owner=std::make_shared<PreparedProgram>(*p);check(!a.stop_from_program(foreign,f)&&f.kind==HostVoiceError::foreign_program,"stopownernotaliased");auto before=a.source_index();const auto source_remaining=event.clip->pcm[0].samples.size()-before;const auto host_frames_to_exceed_eof=(source_remaining*4096+c->sample_step_q12-1)/c->sample_step_q12+2;check(!a.render(static_cast<std::size_t>(host_frames_to_exceed_eof),f)&&f.kind==HostVoiceError::unsupported_sample_end_control&&a.source_index()==before,"EOFnotzerofilledandtransactional");
 auto moved=std::move(a);check(!a.render(1,f)&&f.kind==HostVoiceError::moved_voice,"movedvoicefails");
 std::vector<HostVoice> many;many.reserve(64);for(unsigned i=0;i<64;++i)many.push_back(make());std::vector<HostVoice*> pointers;for(auto& v:many)pointers.push_back(&v);auto mix=mix_host_voices(pointers.data(),pointers.size(),128,f);auto single=make();auto solo=single.render(128,f);check(mix&&solo,"64hostvoiceswithout24voicecap");for(std::size_t i=0;i<mix->size();++i)check(std::abs((*mix)[i]-(*solo)[i]*64)<0.0001f,"additiveunclippedfloatmix");
 HostVoice* duplicate[]{&many[0],&many[0]};before=many[0].source_index();check(!mix_host_voices(duplicate,2,2,f)&&f.kind==HostVoiceError::duplicate_voice&&many[0].source_index()==before,"duplicateatomicfailure");HostVoice* rates[]{&many[0],&resampled};check(!mix_host_voices(rates,2,2,f)&&f.kind==HostVoiceError::output_rate_mismatch&&many[0].source_index()==before,"ratemismatchrollback");
 auto fresh=make();auto nearend=make();const auto length=event.clip->pcm[0].samples.size();const auto safe=std::size_t((length-2)*4096/c->sample_step_q12);check(bool(nearend.render(safe,f)),"positionneartheactualEOF");auto oldfresh=fresh.source_index(),oldend=nearend.source_index();HostVoice* transaction[]{&fresh,&nearend};check(!mix_host_voices(transaction,2,512,f)&&f.kind==HostVoiceError::unsupported_sample_end_control&&fresh.source_index()==oldfresh&&nearend.source_index()==oldend,"laterfailuredoesnotadvancevalidprefix");
 auto init=make(48000,init_profile);check(bool(init.render(128,f)),"explicitdistinctinitprofileacceptedwithoutdefaultclaim");bank->unload();check(bool(moved.render(10,f)),"retainedPCMsurvivesunload");
 std::puts("PASS realannouncr freshpan/LUT/phase/explicitprofiles/signedDSP/hostresample/64voices/transactionalstop; no device/output/fidelity claim");
}
