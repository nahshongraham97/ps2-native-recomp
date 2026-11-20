#include "native_movie_audio_ports.hpp"
#include "native_logon.hpp"
#include "logon_surround_probe.hpp"
#include <vector>
using namespace mka::native_logon;
namespace {
struct TimedSurround final:NativeLogonSurround {
 Effect enter,finish;unsigned begin_left,end_left,end_reset;bool poll_error;
 TimedSurround(Effect a,Effect b,unsigned begins,unsigned ends,bool bad):enter(std::move(a)),finish(std::move(b)),begin_left(begins),end_left(ends),end_reset(ends),poll_error(bad){}
 EffectProgress begin(SurroundFailure& f)override {if(!enter(f.primary))return EffectProgress::error;return begin_left?EffectProgress::pending:EffectProgress::complete;}
 EffectProgress poll_begin(SurroundFailure& f)override {
  if(begin_left&&--begin_left)return EffectProgress::pending;
  if(poll_error){f.primary={"fixture delayed begin failure",96};return EffectProgress::error;}return EffectProgress::complete;
 }
 EffectProgress begin_finish(SurroundFailure& f)override {if(!finish(f.primary)){f.graphics={"fixture graphics restoration failure",111};f.audio={"fixture audio release failure",112};return EffectProgress::error;}return end_left?EffectProgress::pending:EffectProgress::complete;}
 EffectProgress poll_finish(SurroundFailure&)override {return end_left&&--end_left?EffectProgress::pending:EffectProgress::complete;}
 EffectProgress retry_finish(SurroundFailure& f)override {end_left=end_reset;return begin_finish(f);}
};
}
int main(int argc,char** argv){try{
 check(argc==5,"usage: test_native_logon_runtime DLLdir asset_root LOGON_relative MOVIER.ovl");
 MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{"LOGON.SFD",argv[3]}},af);check(bool(store),"actual LOGON association");
 std::ifstream original(std::filesystem::u8path(argv[4]),std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(original)),{});check(bytes.size()>=0x77dd8+6144,"actual color source extent");MovieColorError ce;auto color=NativeMovieColorTable::decode(MovieColorRole::helper_default,bytes.data()+0x77dd8,6144,ce);check(bool(color),"source color table");
 enum class Case{normal,skip,missing_effect,missing_mapping,prepare_failure,surround_failure,start_failure,poll_failure,dual_failure,cancel,destructor,foreign,foreign_poll,reentry,positive_gain,negative_gain,inhibit,missing_target,extra_player,wrong_task_owner,pending_begin,pending_begin_error,pending_start_error,pending_cancel,pending_begin_cancel,pending_finish,pending_retry,pending_foreign};
 for(auto scenario:{Case::normal,Case::skip,Case::missing_effect,Case::missing_mapping,Case::prepare_failure,Case::surround_failure,Case::start_failure,Case::poll_failure,Case::dual_failure,Case::cancel,Case::destructor,Case::foreign,Case::foreign_poll,Case::reentry,Case::positive_gain,Case::negative_gain,Case::inhibit,Case::missing_target,Case::extra_player,Case::wrong_task_owner,Case::pending_begin,Case::pending_begin_error,Case::pending_start_error,Case::pending_cancel,Case::pending_begin_cancel,Case::pending_finish,Case::pending_retry,Case::pending_foreign}){
 auto data=std::make_shared<Data>();auto gpu=std::make_shared<GPU>();std::int64_t ticks=0;unsigned profiles=0;MovieServiceFailure failure;bool start_error=scenario==Case::start_failure||scenario==Case::pending_start_error;
        auto service=NativeMovieService::create_with_native_clock(1.0f,
            [data,&profiles](const RetailMovieRequest& request,std::shared_ptr<const MovieSourceObservationIdentity> id,const std::optional<sound::AudioStreamFormat>& actual,MovieSessionFailure& f)->std::optional<MovieServicePolicyConfiguration>{f={};++profiles;check(request.id==0&&request.logical_key=="LOGON.SFD"&&id==data->source&&actual&&*actual==sound::AudioStreamFormat{data->rate,data->channels,3},"actual same-source data determines client/profile join");return profile(request,*data);},
            [&,data,gpu](const RetailMovieRequest& request,std::int64_t language,MovieSessionPolicyFactory policies,NativeMovieControlContext& control,MovieSessionFailure& f)->std::unique_ptr<NativeMoviePlaybackSession>{
                if(start_error){f.kind=MovieSessionError::source;f.operation="fixture acquisition failure";return {};}
                MovieSourceFailure sf;auto source=NativeRetailMovieSource::open(*store,std::filesystem::u8path(argv[1]),request.id,request.requested_view,language,sf);if(!source){f.kind=MovieSessionError::source;f.source=std::move(sf);return {};}
                auto present=source->audio_present(sf);if(!present||!*present){f.kind=MovieSessionError::source;f.operation="fixture requires actual audio source";f.source=std::move(sf);return {};}
                data->source=source->observation_identity();MovieControlFailure cf;auto input=own_movie_playback_source(std::move(source),cf);if(!input){f.kind=MovieSessionError::control;f.control=cf;return {};}
                std::deque<MovieReadResult> buffered;
                for(;;){auto r=input->next(cf);if(r.status!=MovieReadStatus::frame||cf.kind!=MovieControlError::none){f.kind=MovieSessionError::control;f.operation="fixture actual source ended/failed before audio metadata";f.control=cf;return {};}
                    const auto* a=std::get_if<MovieAudioFrame>(&r.frame);if(a){check(a->sample_rate>0&&a->channels.size()==2&&a->channel_layout.channel_count==a->channels.size(),"explicit two-plane fixture profile admits actual metadata");data->rate=a->sample_rate;data->channels=static_cast<std::uint32_t>(a->channels.size());data->layout=a->channel_layout;data->capacity=std::max(1u,data->rate/20);}
                    const bool found_audio=a!=nullptr;buffered.push_back(std::move(r));++data->prefetched;if(found_audio)break;
                }
                auto concrete=policies(request,data->source,sound::AudioStreamFormat{data->rate,data->channels,3},f);if(!concrete)return {};
                MoviePlaybackConfiguration config;config.audio=MoviePlaybackAudioPresence::required;config.client_sample_rate=data->rate;config.client_channels=data->channels;config.client_channel_mask=3;config.layout={1,640,448,MovieAspectMode::explicit_display_aspect,4,3,MovieWindowPolicy::centered_fit};config.filter=MovieFilter::nearest;config.queued_byte_budget=8*1024*1024;
                return NativeMoviePlaybackSession::create_from_ports(std::make_unique<BufferedInput>(std::move(input),std::move(buffered),data),std::move(concrete),std::make_unique<Audio>(data),gpu,*color,config,control,f);
            },[&](std::string_view key,std::shared_ptr<const MovieSourceObservationIdentity> id,MovieHostClockFailure& f){return NativeMovieHostClock::create_with_reader(key,std::move(id),1000,[&]{return std::optional<std::int64_t>{ticks};},f);},failure);

 check(bool(service),"actual Service construction");
 mka::input::ControllerInput host;check(host.publish(0,mka::input::digital_state(0,true))&&host.publish(1,mka::input::digital_state(0,true)),"explicit actual host participants");
 mka::input::GameActionFailure action_failure;std::map<std::uint32_t,std::uint32_t> maps{{11,0x800},{6,0x40}};
 std::vector<mka::input::GameActionParticipant> participants{{0,maps,true,false},{1,scenario==Case::missing_mapping?std::map<std::uint32_t,std::uint32_t>{{6,0x40}}:maps,false,true}};
 if(scenario==Case::extra_player){participants.emplace_back(5,std::map<std::uint32_t,std::uint32_t>{{99,0x40}},true,false);check(host.publish(5,mka::input::digital_state(0,true)),"extra host participant with distinct mapping");}
 auto actions=mka::input::NativeGameActionInput::create(std::move(participants),scenario==Case::inhibit,action_failure);check(bool(actions),"owned actual action mappings and explicit inhibition");
 if(scenario!=Case::missing_target)check(actions->add_unassigned_record(2,action_failure),"explicit unassigned third game input record without host sample");
 mka::input::GameInputTaskFailure task_failure;std::unique_ptr<mka::input::NativeGameInputTasks> tasks;
 if(scenario==Case::wrong_task_owner){std::thread worker([&]{mka::input::GameInputTaskFailure own;tasks=mka::input::NativeGameInputTasks::create(own);});worker.join();}
 else tasks=mka::input::NativeGameInputTasks::create(task_failure);
 check(bool(tasks),"actual native task restore-state owner");
 std::optional<mka::input::GameInputTaskToken> task_primary,task_no_payload,task_later,task_modal;
 if(scenario!=Case::wrong_task_owner){task_primary=tasks->create_task(mka::input::GameInputTaskKind::kind2065,false,task_failure);task_no_payload=tasks->create_task(mka::input::GameInputTaskKind::kind2066,std::nullopt,task_failure);task_later=tasks->create_task(mka::input::GameInputTaskKind::kind2066,false,task_failure);task_modal=tasks->create_task(mka::input::GameInputTaskKind::kind208b,false,task_failure);check(task_primary&&task_no_payload&&task_later&&task_modal,"owned existing task records");}
 if(scenario!=Case::missing_mapping){check(host.publish(0,mka::input::digital_state(0x800,true)),"prior intro skip press");auto edge=actions->poll_logon_skip(host,action_failure);check(edge&&*edge&&actions->intro_skip(),"prior retained skip genuinely set");check(host.publish(0,mka::input::digital_state(0,true)),"prior release");check(actions->poll(host,action_failure),"release previous intro");}
 std::vector<int> effects;unsigned restores=0;NativeLogonState* current=nullptr;bool reentry_rejected=false,post_checked=false;
 auto effect=[&](int id,bool bad){return [&,id,bad](EffectFailure& f){effects.push_back(id);if(id==3)++restores;if(id==1&&scenario==Case::reentry){Failure inner;reentry_rejected=current&&!current->native_logon_enter(inner)&&inner.kind==Error::invalid_state;}
 if(id==5){auto masks=actions->snapshot(action_failure);check(bool(masks),"observe SAME native post-movie mask owner");for(const auto& s:*masks){if(s.player<=2&&scenario!=Case::inhibit)check(s.previous==0&&s.current==0&&s.rising==0,"source post-movie clears all selected native masks");if(s.player==0&&scenario==Case::inhibit)check(s.current==0x40,"inhibition preserves held input masks");if(s.player==5)check(s.current==0x40,"source clear set does not erase additional host role");}
 const bool expected=scenario!=Case::inhibit;auto primary=tasks->get(*task_primary,task_failure);auto modal=tasks->get(*task_modal,task_failure);auto absent=tasks->get(*task_no_payload,task_failure);auto later=tasks->get(*task_later,task_failure);check(primary&&modal&&absent&&later&&primary->saved_restore==expected&&modal->saved_restore==expected&&!absent->saved_restore&&later->saved_restore==false,"actual direct suspension updates first live typed payloads before transition; absent payload stops that kind search");post_checked=true;}
 if(bad&&(id!=3||restores==1)){f={"fixture required effect failure",90+id};return false;}f={};return true;};};
 Effects ports{effect(1,scenario==Case::prepare_failure),logon_surround_probe(effect(2,scenario==Case::surround_failure),effect(3,scenario==Case::dual_failure||scenario==Case::pending_retry)),effect(5,false)};
 const bool delayed_begin=scenario==Case::pending_begin||scenario==Case::pending_begin_error||scenario==Case::pending_start_error||scenario==Case::pending_begin_cancel||scenario==Case::pending_foreign;
 const bool delayed_end=delayed_begin||scenario==Case::pending_cancel||scenario==Case::pending_finish||scenario==Case::pending_retry;
 if(delayed_end){
  ports.surround_movie=[&](EffectFailure& ef)->std::unique_ptr<NativeLogonSurround>{ef={};return std::make_unique<TimedSurround>(effect(2,false),effect(3,scenario==Case::pending_retry),delayed_begin?2:0,2,scenario==Case::pending_begin_error);};
 }
 if(scenario==Case::missing_effect)ports.transition_task={};
 float volume=scenario==Case::positive_gain?2.0f:scenario==Case::negative_gain?-0.5f:1.0f;
 Failure f;auto state=std::make_unique<NativeLogonState>(*service,*actions,host,*tasks,Settings{MovieViewSelection::normal,0,volume},ports);current=state.get();
 const bool entered=state->native_logon_enter(f);
 if(scenario==Case::missing_effect||scenario==Case::missing_mapping||scenario==Case::missing_target||scenario==Case::wrong_task_owner){check(!entered&&profiles==0&&effects.empty(),"required effects/mappings/reset roles/task ownership reject before acquisition/game effects");check(f.kind==(scenario==Case::missing_effect?Error::missing_effect:Error::input),"typed pre-acquisition failure");continue;}
 if(scenario==Case::prepare_failure||scenario==Case::surround_failure){check(!entered&&profiles==0&&f.kind==Error::effect&&restores==(scenario==Case::prepare_failure?0:1),"failed surround still finishes partial owned operation");check(effects==(scenario==Case::prepare_failure?std::vector<int>{1}:std::vector<int>{1,2,3}),"preparation failure source order");continue;}
 check(!actions->intro_skip(),"successful preparation clears SAME retained intro flag");
 if(delayed_begin){
  check(entered&&state->phase()==Phase::beginning&&profiles==0&&effects==std::vector<int>({1,2}),"pending begin admits ownership without creating movie or repeating preparation");
  if(scenario==Case::pending_begin_cancel){
   check(!state->cancel(f)&&f.kind==Error::none&&state->phase()==Phase::finishing&&profiles==0,"cancel retains pending partial begin cleanup");
   check(!state->poll(f)&&f.kind==Error::invalid_state,"poll cannot turn pending cancellation into successful movie handoff");
   check(!state->cancel(f)&&f.kind==Error::none&&state->cancel(f)&&state->phase()==Phase::cancelled&&restores==1,"only cancel joins pending cancellation once");continue;
  }
  if(scenario==Case::pending_foreign)check(service->start(0,MovieViewSelection::normal,0,4,failure),"foreign actual Service starts during pending begin");
  for(unsigned n=0;n<6&&state->phase()!=Phase::playing&&state->phase()!=Phase::failed;++n){
   auto step=state->poll(f);
   if(state->phase()==Phase::beginning||state->phase()==Phase::finishing)check(step&&step->progress==MovieControlProgress::ongoing&&f.kind==Error::none,"pending setup or failure rollback remains blocked with owned failure");
  }
  if(scenario==Case::pending_begin_error||scenario==Case::pending_start_error||scenario==Case::pending_foreign){
   check(state->phase()==Phase::failed&&restores==1&&effects==std::vector<int>({1,2,3}),"partial setup failure waits for unconditional cleanup and never transitions");
   check(f.kind==(scenario==Case::pending_begin_error?Error::effect:scenario==Case::pending_foreign?Error::foreign_request:Error::service),"exact delayed primary failure survives cleanup polls");
   if(scenario==Case::pending_foreign){check(service->request_view(failure).has_value(),"pending begin revalidates and preserves foreign movie");check(service->close(failure),"foreign fixture closes actual owner");}continue;
  }
  check(state->phase()==Phase::playing&&profiles==1,"pending begin starts same-source movie exactly once after completion");
 }
 if(scenario==Case::pending_cancel){
  check(!state->cancel(f)&&f.kind==Error::none&&state->phase()==Phase::finishing&&!service->request_view(failure),"movie closes but native surrounding cleanup remains pending");
  check(!state->cancel(f)&&f.kind==Error::none&&state->cancel(f)&&restores==1&&effects==std::vector<int>({1,2,3}),"repeated cancellation joins exact cleanup without transition");continue;
 }
 if(scenario==Case::pending_retry){
  data->clock_error=true;auto failed=state->poll(f);
  check(failed&&failed->progress==MovieControlProgress::error&&state->phase()==Phase::cleanup_failed&&f.kind==Error::service&&f.restoration_failure,"primary and cleanup failure retained with operation");
  check(!state->poll(f)&&state->phase()==Phase::cleanup_failed&&restores==1,"ordinary polling never retries failed cleanup");
  check(!state->retry_cleanup(f)&&f.kind==Error::none&&state->phase()==Phase::finishing&&restores==2,"explicit retry can remain pending");
  for(unsigned n=0;n<3&&state->phase()==Phase::finishing;++n)state->poll(f);
  check(state->phase()==Phase::failed&&f.kind==Error::service&&f.restoration_failure->code==93&&f.surround_cleanup_failure&&f.surround_cleanup_failure->graphics.code==111&&f.surround_cleanup_failure->audio.code==112&&effects==std::vector<int>({1,2,3,3}),"successful delayed cleanup retry never upgrades failed movie or drops old cleanup failure");continue;
 }

 if(scenario==Case::start_failure){check(!entered&&f.kind==Error::service&&effects==std::vector<int>({1,2,3})&&restores==1,"start failure restores successful surround once");continue;}
 check(entered&&profiles==1,"actual generated LOGON chain acquires one native session");
 if(scenario==Case::reentry)check(reentry_rejected,"effect callback cannot reenter state");
 auto owned=service->request_view(failure);check(owned&&owned->source==data->source,"actual generation/token captured");
 if(scenario==Case::foreign||scenario==Case::foreign_poll){check(service->close(failure),"external close owned request");check(service->start(0,MovieViewSelection::normal,0,4,failure),"external starts distinct source request");auto foreign=service->request_view(failure);check(foreign&&foreign->generation!=owned->generation&&foreign->source!=owned->source,"real new generation/token");if(scenario==Case::foreign_poll)check(!state->poll(f)&&f.kind==Error::foreign_request&&restores==1,"foreign poll rejects before pumping or cancelling replacement");
 else check(!state->cancel(f)&&f.kind==Error::foreign_request&&restores==1,"foreign request never cancelled by state");auto remains=service->request_view(failure);check(remains&&remains->source==foreign->source,"foreign owner retained");state.reset();check(service->request_view(failure).has_value()&&restores==1,"destructor never closes foreign request or restores twice");check(service->close(failure),"fixture owner closes foreign");continue;}
 if(scenario==Case::destructor){state.reset();check(restores==1&&!service->request_view(failure),"destructor closes same request/restores once");continue;}
 if(scenario==Case::cancel||scenario==Case::reentry||scenario==Case::positive_gain||scenario==Case::negative_gain){check(state->cancel(f)&&restores==1&&effects==std::vector<int>({1,2,3}),"owned cancellation restoration source order");state.reset();check(restores==1,"cancel destructor no repeated restore");continue;}
 if(scenario==Case::poll_failure||scenario==Case::dual_failure){data->clock_error=true;auto result=state->poll(f);check(result&&result->progress==MovieControlProgress::error&&result->failure.native_code==72&&f.kind==Error::service&&restores==1,"actual clock failure restores after terminal Service teardown");check(bool(f.restoration_failure)==(scenario==Case::dual_failure),"independent restoration failure retained alongside primary");if(f.restoration_failure){check(f.restoration_failure->code==93,"exact secondary effect failure");check(!state->retry_cleanup(f)&&state->phase()==Phase::failed&&f.kind==Error::service&&f.restoration_failure->code==93&&restores==2,"explicit successful cleanup retry preserves primary and prior cleanup failure");}continue;}
 bool complete=false,skip_sent=false;unsigned polls=0;for(;polls<20000&&!complete;++polls){data->advance();ticks+=10;
 if((scenario==Case::skip||scenario==Case::inhibit)&&!skip_sent&&data->started){check(host.publish(0,mka::input::digital_state(0x40,true)),"mapped native skip edge");skip_sent=true;}
 if(scenario==Case::extra_player&&!skip_sent&&data->started){check(host.publish(5,mka::input::digital_state(0x40,true)),"additional host role edge must not trigger LOGON skip");skip_sent=true;}
 auto result=state->poll(f);check(result&&result->progress!=MovieControlProgress::error&&f.kind==Error::none,"actual emitted runtime poll succeeds");complete=result->progress==MovieControlProgress::complete;if(state->phase()==Phase::finishing)check(effects==std::vector<int>({1,2,3})&&!post_checked,"pending finish never admits post-movie input suspension/transition");}
 check(complete&&state->phase()==Phase::complete&&effects==std::vector<int>({1,2,3,5})&&restores==1&&post_checked,"generated continuation order with concrete native input/task operation");
 if(scenario==Case::normal||scenario==Case::extra_player||scenario==Case::pending_begin||scenario==Case::pending_finish)check(data->total_accepted==243201&&gpu->presents==152&&data->starts==1&&data->resets==1&&gpu->retired==152&&!actions->intro_skip(),"normal actual LOGON counts; unrelated host role never skips");
 else check(skip_sent&&actions->intro_skip()&&data->total_accepted<243201,"actual mapped skip initiates source fade/early finish");
 state.reset();check(restores==1,"successful destruction no duplicate restore");
 }
 check(std::puts("PASS generated native LOGON actual Service/input-task suspension/restore/identity; deterministic ports not full game effects")>=0&&std::fflush(stdout)==0,"checked footer");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL native LOGON runtime %s\n",e.what());return 1;}}
