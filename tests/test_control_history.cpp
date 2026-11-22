#include "mka/program_copy.hpp"
#include "mka/audio_banks.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <stdexcept>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
using namespace mka::sound;
namespace {
void check(bool b,const char* label){if(!b)throw std::runtime_error(label);}
void history(const AudioControlHistory& h,const int* r){
 check(h.left==r[0]&&h.right==r[1]&&h.remaining==r[3],"source signed cache/countdown");
 check(h.region.has_value()==bool(r[6]),"source region writer provenance");
 if(h.region)check(*h.region==r[2],"source region value");
 check(h.transition.has_value()==bool(r[7]),"source transition writer provenance");
 if(h.transition)check(h.transition->delta==r[4]&&h.transition->divisor==r[5],"source transition delta/divisor");
}
void corpus(const char* path,const ProgramStep& original){
 std::ifstream input(path);std::size_t count;check(bool(input>>count)&&count==26576,"pinned independent source corpus header");
 auto stereo=std::make_shared<PreparedNormal>(*original.clip);stereo->pcm.push_back(stereo->pcm.front());
 AudioControlHistory state;AudioControlFailure f;
 for(std::size_t n=0;n<count;++n){
  int row[26];for(int& x:row)check(bool(input>>x),"complete independent reference row");
  if(row[0]){state={};if(row[0]==2){state.left=static_cast<std::int16_t>(row[6]);state.right=static_cast<std::int16_t>(row[7]);state.remaining=static_cast<std::int16_t>(row[9]);if(row[12])state.region=static_cast<std::int16_t>(row[8]);if(row[13])state.transition=AudioControlTransition{static_cast<std::int16_t>(row[10]),static_cast<std::int16_t>(row[11])};}}
  history(state,row+6);
  ProgramStep event=original;event.start.parameter_44=16384;event.start.parameter_46=0;event.start.parameter_48=4096;
  if(row[2]==2)event.clip=stereo; // Explicit two-plane coefficient fixture, not a count2 file producer.
  auto context=initial_program_control_factors();context.gain_q14=row[4];context.pan_q14=row[5];
  const auto result=decode_audio_controls_with_history(event,context,initial_program_control_factors(),row[3],state,f,row[1]?AudioControlCurve::startup_linear:AudioControlCurve::initial_nonlinear);
  if(!result){std::fprintf(stderr,"row %zu failure %d\n",n,int(f.kind));check(false,"source corpus controls admitted");}
  for(int plane=0;plane<row[2];++plane)for(int channel=0;channel<2;++channel){
   if(result->gains_q15[plane][channel]!=row[14+plane*2+channel]){std::fprintf(stderr,"row %zu plane%d channel%d actual%d expected%d\n",n,plane,channel,result->gains_q15[plane][channel],row[14+plane*2+channel]);check(false,"source reference output gains");}
  }
  history(state,row+18);
 }
 int extra;check(!(input>>extra),"no ignored reference suffix");
}
const NativeEnvelopeProfile fixture_profile{{2,1024,false},{3,-64,false},{4,0,false},{1,-1024,false},8192};
void live_voice(const ProgramStep& event){
 for(auto curve:{AudioControlCurve::initial_nonlinear,AudioControlCurve::startup_linear}){
  const auto neutral=initial_program_control_factors();HostVoiceFailure f;
  auto a=HostVoice::start(event,neutral,neutral,3,fixture_profile,48000,f,curve);
  auto b=HostVoice::start(event,neutral,neutral,3,fixture_profile,48000,f,curve);check(a&&b,"real normal output owners");
  auto first_a=a->render(17,f),first_b=b->render(17,f);check(first_a&&first_b&&*first_a==*first_b,"same real starting PCM and envelope state");
  auto changed=neutral;changed.pan_q14=16384;
  const auto index=a->source_index();check(a->update_controls(event,changed,neutral,3,f)&&a->source_index()==index,"dynamic update preserves cursor");
  const auto h=*a->control_history();check(h.remaining==12&&h.transition&&h.transition->delta==32766&&h.transition->divisor==13,"actual output retains source13-call crossing");
  check(a->render(19,f)&&b->render(19,f)&&a->source_index()==b->source_index()&&a->control_history()==h,"software frames do not advance invocation countdown");
  check(a->update_controls(event,neutral,neutral,0,f)&&a->control_history()==h,"mode0 does not alter descriptor history");
  auto tail_a=a->render(31,f),tail_b=b->render(31,f);check(tail_a&&tail_b&&*tail_a==*tail_b,"control updates preserve fractional source and envelope progression");
  auto bad=event;bad.start.repeat_operand=2;const auto before=*a->control_history();const auto position=a->source_index();
  check(!a->update_controls(bad,neutral,neutral,3,f)&&f.kind==HostVoiceError::control_failed&&a->control_history()==before&&a->source_index()==position,"unsupported update leaves actual controls/history/cursor");
  bad=event;++bad.command_index;check(!a->update_controls(bad,neutral,neutral,3,f)&&f.kind==HostVoiceError::foreign_control_update,"foreign descriptor update refused");
 }
}
void live_copy(AudioBankSession& session,const AudioBankHandle& bank){
 PreparationFailure p;AudioBanksFailure bf;ProgramCopyFailure f;ProgramFailure pf;
 auto ar=session.acquire_runtime_program(bank,0,p,bf),br=session.acquire_runtime_program(bank,0,p,bf);check(ar&&br,"same real native bank/cache semantic references");
 auto a=NativeProgramCopy::clone(ar,f),b=NativeProgramCopy::clone(br,f);check(a&&b&&!ar&&!br,"actual cloned descriptor owners");
 auto sa=a->prepare_ordinary_schedule({1},0,f),sb=b->prepare_ordinary_schedule({1},0,f);check(sa&&sb,"actual normal schedules");
 auto ea=ProgramExecutor::create(sa->data,sa->deadline,pf),eb=ProgramExecutor::create(sb->data,sb->deadline,pf);check(ea&&eb,"actual kind1/7 executors");
 auto x=ea->step(sa->deadline,pf),y=eb->step(sb->deadline,pf);check(x&&y&&x->kind==ProgramStepKind::sample_start,"actual source kind1 descriptor");
 const auto neutral=initial_program_control_factors();NormalOutputParameters params{neutral,neutral,fixture_profile,3,48000,AudioControlCurve::startup_linear};
 check(a->start_output(*x,params,f)&&b->start_output(*y,params,f)&&ea->acknowledge_start(true,pf)&&eb->acknowledge_start(true,pf),"real cached sample and native outputs bound");
 const auto before=*a->instance(x->command_index,f);const auto other=*b->instance(y->command_index,f);
 auto rear=neutral;rear.pan_q14=16384;
 check(a->update_output_controls(x->command_index,rear,neutral,3,f),"actual clone dynamic descriptor flow");
 auto after=a->instance(x->command_index,f);check(after&&after->control_history&&after->control_history->remaining==12&&after->output_source_index==before.output_source_index&&after->state.gain_q14==before.state.gain_q14,"bound clone updates history without replay/command mutation");
 check(b->instance(y->command_index,f)->control_history==other.control_history,"other clone shares sample but not history");
 check(a->mix(128,f)&&a->instance(x->command_index,f)->control_history==after->control_history,"mix leaves actual clone history count unchanged");
 auto stop_a=ea->stop_from_owner(pf),stop_b=eb->stop_from_owner(pf);check(stop_a&&stop_b&&a->stop_outputs(*stop_a,f)&&b->stop_outputs(*stop_b,f)&&a->mix(64,f)&&b->mix(64,f),"actual output release and semantic disposal");
}
}
int main(int argc,char** argv)try{
 check(argc==3,"requires original MS2 and independent source corpus");
 AudioBanksFailure bf;auto session=AudioBankSession::open(argv[1],std::nullopt,bf);check(bool(session),"actual native bank Session");auto bank=session->load_bank("announcr",bf);check(bool(bank),"original named bank");
 PreparationFailure preparation;BankPreparationReport report;check(bool(session->prepare_bank(*bank,report,preparation,bf)),"actual native bank preparation");
 {
  auto reference=session->acquire_runtime_program(*bank,0,preparation,bf);check(bool(reference),"actual semantic reference");auto data=reference->preparation()->data();ProgramFailure pf;auto executor=ProgramExecutor::create(data,{1},pf);check(bool(executor),"actual normal program");auto event=executor->step({1},pf);check(event&&event->kind==ProgramStepKind::sample_start,"actual source sample start");
  corpus(argv[2],*event);live_voice(*event);live_copy(*session,*bank);
 }
 check(session->unload_bank(*bank,bf)&&session->close(bf),"real Session/cache unload after actual references retire");
 std::puts("PASS_NATIVE_RETAINED_CONTROL_HISTORY_26576_SOURCE_ROWS_AND_ACTUAL_OUTPUTS");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"control history: %s\n",e.what());return 1;}
