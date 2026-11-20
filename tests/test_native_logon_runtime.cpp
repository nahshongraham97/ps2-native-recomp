#include "native_movie_audio_ports.hpp"
#include "native_logon.hpp"
#include <vector>
using namespace mka::native_logon;
int main(int argc,char** argv){try{
 check(argc==5,"usage: test_native_logon_runtime DLLdir asset_root LOGON_relative MOVIER.ovl");
 MovieAssetFailure af;auto store=NativeMovieAssetStore::create(std::filesystem::u8path(argv[2]),{{"LOGON.SFD",argv[3]}},af);check(bool(store),"actual LOGON association");
 std::ifstream original(std::filesystem::u8path(argv[4]),std::ios::binary);std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(original)),{});check(bytes.size()>=0x77dd8+6144,"actual color source extent");MovieColorError ce;auto color=NativeMovieColorTable::decode(MovieColorRole::helper_default,bytes.data()+0x77dd8,6144,ce);check(bool(color),"source color table");
 enum class Case{normal,skip,missing_effect,missing_mapping,prepare_failure,surround_failure,start_failure,poll_failure,dual_failure,cancel,destructor,foreign,foreign_poll,reentry,positive_gain,negative_gain};
 for(auto scenario:{Case::normal,Case::skip,Case::missing_effect,Case::missing_mapping,Case::prepare_failure,Case::surround_failure,Case::start_failure,Case::poll_failure,Case::dual_failure,Case::cancel,Case::destructor,Case::foreign,Case::foreign_poll,Case::reentry,Case::positive_gain,Case::negative_gain}){
 auto data=std::make_shared<Data>();auto gpu=std::make_shared<GPU>();std::int64_t ticks=0;unsigned profiles=0;MovieServiceFailure failure;bool start_error=scenario==Case::start_failure;
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
 auto actions=mka::input::NativeGameActionInput::create({{0,maps},{1,scenario==Case::missing_mapping?std::map<std::uint32_t,std::uint32_t>{{6,0x40}}:maps}},action_failure);check(bool(actions),"owned actual action mappings");
 if(scenario!=Case::missing_mapping){check(host.publish(0,mka::input::digital_state(0x800,true)),"prior intro skip press");auto edge=actions->poll_logon_skip(host,action_failure);check(edge&&*edge&&actions->intro_skip(),"prior retained skip genuinely set");check(host.publish(0,mka::input::digital_state(0,true)),"prior release");check(actions->poll(host,action_failure),"release previous intro");}
 std::vector<int> effects;unsigned restores=0;NativeLogonState* current=nullptr;bool reentry_rejected=false;
 auto effect=[&](int id,bool bad){return [&,id,bad](EffectFailure& f){effects.push_back(id);if(id==3)++restores;if(id==1&&scenario==Case::reentry){Failure inner;reentry_rejected=current&&!current->native_logon_enter(inner)&&inner.kind==Error::invalid_state;}if(bad){f={"fixture required effect failure",90+id};return false;}f={};return true;};};
 Effects ports{effect(1,scenario==Case::prepare_failure),effect(2,scenario==Case::surround_failure),effect(3,scenario==Case::dual_failure),effect(4,false),effect(5,false)};
 if(scenario==Case::missing_effect)ports.transition_task={};
 float volume=scenario==Case::positive_gain?2.0f:scenario==Case::negative_gain?-0.5f:1.0f;
 Failure f;auto state=std::make_unique<NativeLogonState>(*service,*actions,host,Settings{MovieViewSelection::normal,0,volume},ports);current=state.get();
 const bool entered=state->native_logon_enter(f);
 if(scenario==Case::missing_effect||scenario==Case::missing_mapping){check(!entered&&profiles==0&&effects.empty(),"required effects/mappings reject before acquisition/game effects");continue;}
 if(scenario==Case::prepare_failure||scenario==Case::surround_failure){check(!entered&&profiles==0&&f.kind==Error::effect&&restores==0,"failed preparation/surround owns no successful surround");check(effects==(scenario==Case::prepare_failure?std::vector<int>{1}:std::vector<int>{1,2}),"preparation failure source order");continue;}
 check(!actions->intro_skip(),"successful preparation clears SAME retained intro flag");
 if(scenario==Case::start_failure){check(!entered&&f.kind==Error::service&&effects==std::vector<int>({1,2,3})&&restores==1,"start failure restores successful surround once");continue;}
 check(entered&&profiles==1,"actual generated LOGON chain acquires one native session");
 if(scenario==Case::reentry)check(reentry_rejected,"effect callback cannot reenter state");
 auto owned=service->request_view(failure);check(owned&&owned->source==data->source,"actual generation/token captured");
 if(scenario==Case::foreign||scenario==Case::foreign_poll){check(service->close(failure),"external close owned request");check(service->start(0,MovieViewSelection::normal,0,4,failure),"external starts distinct source request");auto foreign=service->request_view(failure);check(foreign&&foreign->generation!=owned->generation&&foreign->source!=owned->source,"real new generation/token");if(scenario==Case::foreign_poll)check(!state->poll(f)&&f.kind==Error::foreign_request&&restores==1,"foreign poll rejects before pumping or cancelling replacement");
 else check(!state->cancel(f)&&f.kind==Error::foreign_request&&restores==1,"foreign request never cancelled by state");auto remains=service->request_view(failure);check(remains&&remains->source==foreign->source,"foreign owner retained");state.reset();check(service->request_view(failure).has_value()&&restores==1,"destructor never closes foreign request or restores twice");check(service->close(failure),"fixture owner closes foreign");continue;}
 if(scenario==Case::destructor){state.reset();check(restores==1&&!service->request_view(failure),"destructor closes same request/restores once");continue;}
 if(scenario==Case::cancel||scenario==Case::reentry||scenario==Case::positive_gain||scenario==Case::negative_gain){check(state->cancel(f)&&restores==1&&effects==std::vector<int>({1,2,3}),"owned cancellation restoration source order");state.reset();check(restores==1,"cancel destructor no repeated restore");continue;}
 if(scenario==Case::poll_failure||scenario==Case::dual_failure){data->clock_error=true;auto result=state->poll(f);check(result&&result->progress==MovieControlProgress::error&&result->failure.native_code==72&&f.kind==Error::service&&restores==1,"actual clock failure restores after terminal Service teardown");check(bool(f.restoration_failure)==(scenario==Case::dual_failure),"independent restoration failure retained alongside primary");if(f.restoration_failure)check(f.restoration_failure->code==93,"exact secondary effect failure");continue;}
 bool complete=false,skip_sent=false;unsigned polls=0;for(;polls<20000&&!complete;++polls){data->advance();ticks+=10;
 if(scenario==Case::skip&&!skip_sent&&data->started){check(host.publish(0,mka::input::digital_state(0x40,true)),"mapped native skip edge");skip_sent=true;}
 auto result=state->poll(f);check(result&&result->progress!=MovieControlProgress::error&&f.kind==Error::none,"actual emitted runtime poll succeeds");complete=result->progress==MovieControlProgress::complete;}
 check(complete&&state->phase()==Phase::complete&&effects==std::vector<int>({1,2,3,4,5})&&restores==1,"generated full continuation order");
 if(scenario==Case::normal)check(data->total_accepted==243201&&gpu->presents==152&&data->starts==1&&data->resets==1&&gpu->retired==152,"normal actual LOGON control/audio/work/drain counts");
 else check(skip_sent&&actions->intro_skip()&&data->total_accepted<243201,"actual mapped skip initiates source fade/early finish");
 state.reset();check(restores==1,"successful destruction no duplicate restore");
 }
 check(std::puts("PASS generated native LOGON actual Service chain/actions/restore/identity; deterministic ports not full game effects")>=0&&std::fflush(stdout)==0,"checked footer");return 0;
 }catch(const std::exception& e){std::fprintf(stderr,"FAIL native LOGON runtime %s\n",e.what());return 1;}}
