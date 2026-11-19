#include "mka/audio_bank_service.hpp"
#include "mka/program_executor.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
using namespace mka::sound;
namespace {
void check(bool v,const char* m){if(!v){std::fprintf(stderr,"FAIL: %s\n",m);std::exit(1);}}
void put(std::vector<std::uint8_t>& b,std::size_t a,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i)b[a+i]=v>>(i*8);}
AudioBankCommand load(AudioBankOpcode op,const char* name,std::uint32_t cb){
 std::string text(name);const std::size_t offset=op==AudioBankOpcode::load_async?16:8;const auto words=(offset+text.size()+1+3)/4;std::vector<std::uint8_t>b((words+2)*4);put(b,0,words+2);put(b,4,cb);put(b,8,std::uint32_t(op));
 for(std::size_t i=0;i<text.size();++i)b[8+offset+i]=text[i];AudioProtocolFailure f;auto r=decode_audio_bank_frame(b.data(),b.size(),f);check(bool(r),"sourceframedrequestdecode");return std::move(*r);
}
AudioBankCommand unload(std::uint32_t token){AudioBankCommand c;c.opcode=AudioBankOpcode::unload;c.bank_token={token};return c;}
AudioBankCommand play(std::uint32_t token){AudioBankCommand c;c.opcode=AudioBankOpcode::play;c.bank_token={token};c.selector=0;return c;}
AudioBankService service(const std::filesystem::path& p){AudioBanksFailure b;auto s=AudioBankSession::open(p,std::nullopt,b);check(bool(s),"session");AudioProtocolFailure f;auto protocol=AudioBankProtocol::create(std::move(*s),f);check(bool(protocol),"protocol");BankServiceFailure sf;auto service=AudioBankService::create(std::move(*protocol),sf);check(bool(service),"service");return std::move(*service);}
}
int main(int argc,char** argv){
 if(argc!=3){std::fprintf(stderr,"usage: test_audio_bank_service <pinnedMS2> <NEW diagnostics directory>\n");return 2;}
 const std::filesystem::path out(argv[2]);check(std::filesystem::create_directory(out),"exclusiveowneddiagnostics");auto s=service(argv[1]);BankServiceFailure f;
 auto sync=s.load_sync(load(AudioBankOpcode::load_sync,"bg_armory",0x11111234),{0x89ab1234},f);
 check(sync&&sync->bank.value==0x89ab1234&&sync->preparation.eager_error_count==0&&sync->preparation.programs.size()==11,"syncwordonlyafteractualeagerpreparation");
 check(!s.poll_completion(f)&&f.kind==BankServiceError::none,"syncdoesnotsynthesizecallback");
 auto owned=s.resolve_play_kind1(play(0x89ab1234),0,f);check(bool(owned),"publishedfullwirebanklookup");
 check(!s.resolve_play_kind1(play(0x1234),0,f)&&f.kind==BankServiceError::unknown_bank_token,"nolow16mask");
 auto submit=s.submit_async(load(AudioBankOpcode::load_async,"announcr",0xfedc1234),{0x34cd1234},f);check(submit==AsyncBankSubmission::queued,"asyncadmitted");
 check(!s.poll_completion(f)&&f.kind==BankServiceError::none,"internalpendingstatus1notemitted");
 check(!s.unload(unload(0x34cd1234),f)&&f.kind==BankServiceError::unpublished_bank_token,"unpublishedloadcannotbeguessedunloaded");
 check(!s.cancel_async(f)&&f.kind==BankServiceError::cancellation_unsupported,"unknownasynccancelnotinvented");check(!s.close_when_idle(f)&&f.kind==BankServiceError::cancellation_unsupported,"closecannotdropadmittedrequest");
 check(!s.load_sync(load(AudioBankOpcode::load_sync,"announcr",1),{0x11111111},f)&&f.kind==BankServiceError::sync_during_async_unsupported,"unprovedsyncduringasyncexplicit");
 auto busy=s.submit_async(load(AudioBankOpcode::load_async,"announcr",0x76541234),{0},f);check(busy==AsyncBankSubmission::completed_busy,"sourcebusycompletion");
 auto rejected=s.poll_completion(f);check(rejected&&rejected->callback_token==0x76541234&&rejected->words[0]==2&&rejected->words[1]==7,"busyerror7fullcallbacktoken");
 check(s.run_one(f),"actualhostexecutorpreparesasyncbank");
 PreparationFailure preparation_failure;
 check(!s.prepare_program(play(0x34cd1234),preparation_failure,f)&&f.kind==BankServiceError::unpublished_bank_token,"programcannotstartbeforeloadcompletiondelivery");
 check(!s.resolve_play_kind1(play(0x34cd1234),0,f)&&f.kind==BankServiceError::unpublished_bank_token,"preparedresultnotvisiblebeforecompletiondelivery");
 // Busy cleared BEFORE completion delivery, so a new request may be admitted.
 check(s.submit_async(load(AudioBankOpcode::load_async,"no-such-retail-bank",0xaabbccdd),{0x99887766},f)==AsyncBankSubmission::queued,"busyreleasebeforecallbackdelivery");
 auto completed=s.poll_completion(f);check(completed&&completed->callback_token==0xfedc1234&&completed->words[0]==0&&completed->words[1]==0x34cd1234&&completed->outcome.bank.value==0x34cd1234&&completed->outcome.preparation.eager_error_count==0&&completed->outcome.preparation.programs.size()==192,"actualsuccessstatus0fullbankonlyafterpreparation");
 for(const auto& p:completed->outcome.preparation.programs)check(p.state==ProgramPreparationState::source_deferred,"sourceannouncrdeferredpolicyretained");
 auto resource=s.resolve_play_kind1(play(0x34cd1234),0,f);check(resource&&resource->resolved_key==0x20000002,"callbackbankassociatedexactresource");
 auto prepared_program=s.prepare_program(play(0x34cd1234),preparation_failure,f);
 check(prepared_program&&prepared_program->normal_instances.size()==1&&prepared_program->normal_instances[0].sample->pcm[0].samples.size()==34720,"deliveredfulltokenreachesactualdeferrednormalPCM");
 ProgramFailure program_failure;auto executor=ProgramExecutor::create(prepared_program,{1000},program_failure);
 check(bool(executor),"createactualpreparedprogramtimeline");auto start=executor->step({1000},program_failure);
 check(start&&start->kind==ProgramStepKind::sample_start&&start->clip==prepared_program->normal_instances[0].sample,"requestcompletionthroughpreparationtotypedhoststart");
 check(executor->step({1000},program_failure)->kind==ProgramStepKind::awaiting_start_result,"nophantomhoststartacknowledgement");
 check(s.run_one(f),"actualfailedloadwork");auto failed=s.poll_completion(f);check(failed&&failed->callback_token==0xaabbccdd&&failed->words[0]==2&&failed->words[1]==1&&failed->outcome.bank.value==0&&failed->outcome.protocol_failure.bank.archive.kind==ArchiveError::absent_bank,"loadfailurestatus2error1notfakesuccess");
 check(s.unload(unload(0x34cd1234),f)&&!s.resolve_play_kind1(play(0x34cd1234),0,f)&&f.kind==BankServiceError::unloaded_bank_token,"unloadrevokespublishedexactbank");
 check(!s.prepare_program(play(0x34cd1234),preparation_failure,f)&&f.kind==BankServiceError::unloaded_bank_token,"unloadedfulltokencannotpreparenewprogram");
 auto stop=executor->stop_from_owner(program_failure);
 check(stop&&stop->kind==ProgramStepKind::program_stop&&stop->stop_reason==ProgramStopReason::owner_request&&start->clip->pcm[0].samples.size()==34720,"ownerexplicitlystopsretainedexecutorwithoutlosingownedbytes");
 check(owned->command.kind()==1&&resource->resolved_key==0x20000002,"returnedownedrecords surviveunload");
 check(!s.load_sync(load(AudioBankOpcode::load_sync,"announcr",0),{0x34cd1234},f)&&f.kind==BankServiceError::duplicate_bank_token,"noimplicitstalereuse");
 auto missing=s.load_sync(load(AudioBankOpcode::load_sync,"no-such-retail-bank",0),{0x11223344},f);check(missing&&missing->bank.value==0&&missing->protocol_failure.bank.archive.kind==ArchiveError::absent_bank,"syncfailureiszeroresultnotcallback");
 check(s.close_when_idle(f),"drainedclose");check(!s.submit_async(load(AudioBankOpcode::load_async,"announcr",1),{2},f)&&f.kind==BankServiceError::closed,"closedrejectsnewwork");
 auto moved=std::move(s);check(!s.run_one(f)&&f.kind==BankServiceError::moved_service,"movedservicenamed");
 // Actual eager preparation failure, independent of the pathname-error path.
 std::vector<std::uint8_t>bad(512);put(bad,0,2);put(bad,4,2);put(bad,8,33);put(bad,16,bad.size());put(bad,24,1);put(bad,28,128);put(bad,32,166);const char names[]="\0bad.mst\0";std::copy_n(reinterpret_cast<const std::uint8_t*>(names),9,bad.begin()+36);
 std::vector<std::uint8_t>bank(166);put(bank,0,11);put(bank,4,0x13);put(bank,8,1,2);put(bank,10,1,2);put(bank,12,1,2);put(bank,16,48);put(bank,20,84);put(bank,24,100);put(bank,28,116);put(bank,32,156);put(bank,56,0x20000002);put(bank,60,22050);put(bank,64,4096);put(bank,68,36);put(bank,108,1);put(bank,116,0x20000002);bank[124]=1;put(bank,156,1,2);std::copy(bank.begin(),bank.end(),bad.begin()+128);
 {std::ofstream file(out/"failed-preparation.ms2",std::ios::binary);file.write(reinterpret_cast<const char*>(bad.data()),bad.size());check(bool(file),"ownedbadarchive");}
 auto badservice=service(out/"failed-preparation.ms2");check(badservice.submit_async(load(AudioBankOpcode::load_async,"bad",0xdeadbeef),{0xeeeeffff},f)==AsyncBankSubmission::queued&&badservice.run_one(f),"eagerfailurejobexecuted");auto badcompletion=badservice.poll_completion(f);check(badcompletion&&badcompletion->words[0]==2&&badcompletion->words[1]==1&&badcompletion->outcome.preparation.eager_error_count==1&&badcompletion->outcome.preparation_failure.kind==PreparationError::eager_program_failures,"actualeagererrorcannotpublishsuccess");check(!badservice.resolve_play_kind1(play(0xeeeeffff),0,f)&&f.kind==BankServiceError::unknown_bank_token,"failedcandidatehasnopublishedbinding");check(badservice.close_when_idle(f),"failurecleanupdrained");
 std::puts("PASS nativebank sync/async completion ordering/exacttokens/eagerfailure; no transport/game/playback");
}
