#include "mka/audio_bank_service.hpp"
#include "mka/program_copy.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
using namespace mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::abort();}}
constexpr std::uint32_t bank_token=0xa1b77321,other_token=0x9c427321;
AudioBankCommand command(AudioBankOpcode opcode,std::uint32_t token){AudioBankCommand c;c.opcode=opcode;c.bank_token={token};return c;}
AudioBankCommand load(AudioBankOpcode opcode=AudioBankOpcode::load_sync){AudioBankCommand c;c.opcode=opcode;c.pathname="announcr";c.callback_token=0xd19e7321;return c;}
AudioBankService service(const char* path){AudioBanksFailure bf;auto session=AudioBankSession::open(path,std::nullopt,bf);check(bool(session),"actual session");AudioProtocolFailure pf;auto protocol=AudioBankProtocol::create(std::move(*session),pf);check(bool(protocol),"actual protocol");BankServiceFailure f;auto s=AudioBankService::create(std::move(*protocol),f);check(bool(s),"actual Service");return std::move(*s);}
void loaded(AudioBankService& s,std::uint32_t token){BankServiceFailure f;auto r=s.load_sync(load(),{token},f);check(r&&r->bank.value==token,"actual full-token bank load");}
std::unique_ptr<RuntimeProgramReference> acquire(AudioBankService& s,std::uint32_t token){PreparationFailure p;BankServiceFailure f;auto r=s.acquire_runtime_program(command(AudioBankOpcode::play,token),p,f);check(r&&r->active()&&r->preparation(),"actual Service runtime reference");return r;}
void blocked(AudioBankService& s,std::uint32_t token){BankServiceFailure f;check(!s.unload(command(AudioBankOpcode::unload,token),f)&&f.kind==BankServiceError::runtime_reference_active,"same-bank unload blocked by retained semantic reference");check(!s.close_when_idle(f)&&f.kind==BankServiceError::runtime_reference_active,"Service close blocked by retained semantic reference");}
NormalOutputParameters params(){return {initial_program_control_factors(),initial_program_control_factors(),{{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048},3,48000};}
bool nonzero(const std::vector<float>& v){return std::any_of(v.begin(),v.end(),[](float x){return x!=0;});}
struct Playing {
 std::unique_ptr<NativeProgramCopy> copy;
 std::optional<OrdinaryProgramSchedule> schedule;
 std::optional<ProgramExecutor> executor;
 explicit Playing(std::unique_ptr<RuntimeProgramReference>& r){ProgramCopyFailure f;copy=NativeProgramCopy::clone(r,f);check(copy&&!r,"copy consumes actual Service reference");schedule=copy->prepare_ordinary_schedule({1000},0,f);check(bool(schedule),"actual ordinary schedule");ProgramFailure pf;executor=ProgramExecutor::create(schedule->data,schedule->deadline,pf);check(bool(executor),"actual Executor");auto start=executor->step(schedule->deadline,pf);check(start&&start->kind==ProgramStepKind::sample_start&&copy->start_output(*start,params(),f)&&executor->acknowledge_start(true,pf),"real native output before acknowledgement");auto pcm=copy->mix(512,f);check(pcm&&nonzero(*pcm),"actual nonzero software output");}
};
void references_and_rollback(const char* path){
 auto s=service(path);loaded(s,bank_token);PreparationFailure p;BankServiceFailure f;
 check(!s.acquire_runtime_program(command(AudioBankOpcode::play,0x7321),p,f)&&f.kind==BankServiceError::unknown_bank_token,"no token truncation");check(!s.acquire_runtime_program(command(AudioBankOpcode::unload,bank_token),p,f)&&f.kind==BankServiceError::wrong_command,"wrong command rollback");auto invalid=command(AudioBankOpcode::play,bank_token);invalid.selector=0x7fffffff;check(!s.acquire_runtime_program(invalid,p,f)&&p.kind==PreparationError::absent_selector,"invalid selector rollback");
 auto a=acquire(s,bank_token),b=acquire(s,bank_token);auto runtime=a->preparation();check(runtime==b->preparation()&&runtime->resource_domain()==s.resource_domain()&&runtime->initial_command_index()==0,"Service returns exact shared typed runtime and domain");const auto& c=runtime->initial_controls();check(c.gain_q14==16384&&c.pan_q14==0&&c.pitch_q12==4096&&c.secondary_gain_q14==16384&&c.secondary_pan_q14==0&&c.secondary_pitch_q12==4096,"exact constructor values");blocked(s,bank_token);check(a->release(p)&&!a->active(),"explicit reference release");blocked(s,bank_token);
 loaded(s,other_token);auto other=acquire(s,other_token);check(!other->preparation()->same_bank(*runtime),"unrelated actual bank instance runtime identity");other.reset();check(s.unload(command(AudioBankOpcode::unload,other_token),f),"unrelated bank usable and independently unloadable");b.reset();check(s.unload(command(AudioBankOpcode::unload,bank_token),f)&&s.close_when_idle(f),"all rollback and reference reservations released");
}
void async_publication(const char* path){
 auto s=service(path);PreparationFailure p;BankServiceFailure f;check(s.submit_async(load(AudioBankOpcode::load_async),{bank_token},f)==AsyncBankSubmission::queued,"actual async bank load queued");check(!s.acquire_runtime_program(command(AudioBankOpcode::play,bank_token),p,f)&&f.kind==BankServiceError::unpublished_bank_token,"queued bank has no published runtime admission");check(s.run_one(f),"actual async bank work");check(!s.acquire_runtime_program(command(AudioBankOpcode::play,bank_token),p,f)&&f.kind==BankServiceError::unpublished_bank_token,"prepared bank completion must be delivered first");auto completion=s.poll_completion(f);check(completion&&completion->words[0]==0&&completion->words[1]==bank_token&&completion->callback_token==0xd19e7321,"actual full-token load completion");auto r=acquire(s,bank_token);r.reset();check(s.unload(command(AudioBankOpcode::unload,bank_token),f)&&s.close_when_idle(f),"async publication reference cleanup");
}
void copy_lifetime(const char* path){
 auto s=service(path);loaded(s,bank_token);auto r=acquire(s,bank_token);Playing play(r);blocked(s,bank_token);ProgramFailure pf;auto stop=play.executor->stop_from_owner(pf);ProgramCopyFailure cf;check(stop&&play.copy->stop_outputs(*stop,cf),"actual mode0 stop");blocked(s,bank_token);auto exported=play.schedule->data;auto pcm=exported->normal_instances[0].sample;play.copy.reset();BankServiceFailure f;check(s.unload(command(AudioBankOpcode::unload,bank_token),f)&&s.close_when_idle(f),"copy cleanup releases retained Service admission");check(!exported->normal_instances[0].cache_reference&&!pcm->pcm[0].samples.empty(),"exported clone PCM survives unload without semantic acquisition");
}
void moved_and_destroyed(const char* path){
 std::optional<AudioBankService> s(service(path));loaded(*s,bank_token);auto r=acquire(*s,bank_token);Playing play(r);auto moved=std::move(*s);PreparationFailure p;BankServiceFailure f;check(!s->acquire_runtime_program(command(AudioBankOpcode::play,bank_token),p,f)&&f.kind==BankServiceError::moved_service,"moved Service rejects acquisition");s.reset();s.emplace(std::move(moved));blocked(*s,bank_token);s.reset();ProgramCopyFailure cf;auto pcm=play.copy->mix(512,cf);check(pcm&&nonzero(*pcm),"copy owns actual Service state after last public Service object drops");play.copy.reset();check(!play.schedule->data->normal_instances[0].sample->pcm[0].samples.empty(),"immutable exported data survives final Service keeper cleanup");
}
void protocol_negatives(const char* path){
 AudioBanksFailure bf;auto session=AudioBankSession::open(path,std::nullopt,bf);check(bool(session),"negative-case actual session");AudioProtocolFailure f;auto protocol=AudioBankProtocol::create(std::move(*session),f);check(bool(protocol),"negative-case actual protocol");PreparationFailure p;check(!protocol->acquire_runtime_program(command(AudioBankOpcode::play,0),p,f)&&f.kind==AudioProtocolError::unsupported_null_bank_token,"protocol empty token named");check(!protocol->acquire_runtime_program(command(AudioBankOpcode::unload,bank_token),p,f)&&f.kind==AudioProtocolError::wrong_command,"protocol wrong command named");check(!protocol->acquire_runtime_program(command(AudioBankOpcode::play,bank_token),p,f)&&f.kind==AudioProtocolError::unknown_bank_token,"protocol unknown exact token named");
}
}
int main(int argc,char** argv){if(argc!=2){std::fprintf(stderr,"usage: test_service_runtime <pinned_original_MS2>\n");return 2;}references_and_rollback(argv[1]);async_publication(argv[1]);copy_lifetime(argv[1]);moved_and_destroyed(argv[1]);protocol_negatives(argv[1]);std::puts("PASS_NATIVE_SERVICE_RUNTIME: 5 actual Service runtime-reference/copy/output lifetime cases; no source callback/activation/device/fidelity claim");return std::fflush(stdout)||std::ferror(stdout)?1:0;}
