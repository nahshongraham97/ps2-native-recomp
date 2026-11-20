#include "mka/audio_bank_service.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
using namespace mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::abort();}}
template<class P>void await(P p,const char* why){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!p()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
AudioBankCommand command(AudioBankOpcode op,std::uint32_t token){AudioBankCommand c;c.opcode=op;c.bank_token={token};return c;}
constexpr std::uint32_t token=0x91ab7321,other_token=0xe5827321;
struct Fixture {
    std::shared_ptr<AudioResourceDomain> domain;
    std::optional<AudioBankService> service;
    explicit Fixture(const char* path){
        AudioResourceFailure rf;domain=AudioResourceDomain::create(rf);check(bool(domain),"domain");
        AudioBanksFailure bf;auto session=AudioBankSession::open(path,std::nullopt,domain,bf);check(bool(session),"actual session");
        AudioProtocolFailure pf;auto protocol=AudioBankProtocol::create(std::move(*session),pf);check(bool(protocol),"actual protocol");
        BankServiceFailure sf;service=AudioBankService::create(std::move(*protocol),sf);check(bool(service)&&service->resource_domain()==domain,"same-domain actual Service");
        load(token);
    }
    void load(std::uint32_t t,const char* name="announcr"){AudioBankCommand c;c.opcode=AudioBankOpcode::load_sync;c.pathname=name;BankServiceFailure f;auto r=service->load_sync(c,{t},f);check(r&&r->bank.value==t,"actual named bank load");}
    std::shared_ptr<BankProgramWork> admit(){PreparationFailure p;BankServiceFailure f;auto w=service->admit_deferred_program(command(AudioBankOpcode::play,token),p,f);check(bool(w),"actual Service admission");return w;}
    void blocked(){BankServiceFailure f;check(!service->unload(command(AudioBankOpcode::unload,token),f)&&f.kind==BankServiceError::program_work_active,"same-bank unload blocked by actual admission");check(!service->close_when_idle(f)&&f.kind==BankServiceError::worker_active,"close blocked by actual admission");}
    void finish(){BankServiceFailure f;check(service->unload(command(AudioBankOpcode::unload,token),f),"bank unload after cleanup");check(service->close_when_idle(f),"Service close after cleanup");}
};
void queued(const char* path){
    Fixture x(path);auto c=command(AudioBankOpcode::play_extended,token);c.callback_token=0xdef12345;c.context_word=0x91345678;c.opaque_load_argument=0xfed81234;c.opaque_load_tag=17;c.pathname="owned-request";c.payload={0,1,255,37};c.frame_bytes=88;
    PreparationFailure p;BankServiceFailure f;auto w=x.service->admit_deferred_program(c,p,f);check(bool(w),"owned extended request admission");c.bank_token={1};c.selector=99;c.payload.clear();c.pathname.clear();c.context_word=0;
    const auto& r=w->request();check(r.opcode==AudioBankOpcode::play_extended&&r.bank_token.value==token&&r.selector==0&&r.payload==std::vector<std::uint8_t>({0,1,255,37})&&r.pathname=="owned-request"&&r.context_word==0x91345678&&r.callback_token==0xdef12345&&r.opaque_load_argument==0xfed81234&&r.opaque_load_tag==17&&r.frame_bytes==88,"all owned original request fields/full token survive caller mutation");
    x.blocked();check(w->cancel()==ProgramWorkCancellation::queued_removed,"queued removal receipt");x.blocked();check(w->observe().admission_retained,"queued cancellation retains reservation");check(w->retire(p)&&!w->observe().admission_retained,"explicit queued retirement releases reservation");x.finish();
}
void rollback(const char* path){
    Fixture x(path);PreparationFailure p;BankServiceFailure f;auto bad=command(AudioBankOpcode::play,token);bad.selector=0x7fffffff;
    check(!x.service->admit_deferred_program(bad,p,f)&&p.kind==PreparationError::absent_selector,"absent selector rolls back admission");
    check(!x.service->admit_deferred_program(command(AudioBankOpcode::unload,token),p,f)&&f.kind==BankServiceError::wrong_command,"wrong command rejected");
    check(!x.service->admit_deferred_program(command(AudioBankOpcode::play,0x7321),p,f)&&f.kind==BankServiceError::unknown_bank_token,"truncated token cannot resolve");
    x.load(other_token,"bg_armory");check(!x.service->admit_deferred_program(command(AudioBankOpcode::play,other_token),p,f)&&p.kind==PreparationError::already_prepared,"actual eager program cannot impersonate fresh admission");
    check(x.service->unload(command(AudioBankOpcode::unload,other_token),f),"failed eager admission leaves no reservation");x.finish();
}
void running(const char* path){
    Fixture x(path);x.load(other_token);auto w=x.admit();AudioResourceFailure rf;auto movie=x.domain->surround_movie(rf);check(bool(movie),"actual publication exclusion");
    std::promise<void> built,finish;auto gate=finish.get_future();
    std::thread worker([&]{PreparationFailure p;check(!w->build(p)&&p.kind==PreparationError::cancelled,"running actual data build cancelled");built.set_value();gate.wait();check(!w->take_result(p)&&p.kind==PreparationError::cancelled,"cancel has no published result");check(w->retire(p),"actual worker retires cancellation");});
    await([&]{return x.domain->observe()[3].waiting!=0;},"actual publication wait");
    check(w->cancel()==ProgramWorkCancellation::running_requested,"running cancellation distinct receipt");x.blocked();
    PreparationFailure p;check(!w->take_result(p)&&p.kind==PreparationError::wrong_thread,"foreign take rejected");check(!w->retire(p)&&p.kind==PreparationError::wrong_thread&&w->observe().admission_retained,"foreign retirement retains reservation");
    BankServiceFailure f;auto unrelated=x.service->admit_deferred_program(command(AudioBankOpcode::play,other_token),p,f);check(bool(unrelated),"unrelated bank admission usable while first worker waits");check(unrelated->cancel()==ProgramWorkCancellation::queued_removed&&unrelated->retire(p),"unrelated actual admission cleanup");
    check(x.service->unload(command(AudioBankOpcode::unload,other_token),f),"unrelated bank unload independently usable");
    check(movie->release(rf),"release actual publication exclusion");check(built.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"cancelled build completes");x.blocked();
    check(w->observe().preparation.state==ProgramWorkState::cancelled&&w->observe().admission_retained,"completed cancellation still needs actual retirement");finish.set_value();worker.join();check(!w->observe().admission_retained,"worker retirement releases Service reservation");x.finish();
}
void ready(const char* path){
    Fixture x(path);auto w=x.admit();std::promise<void> built,take,published,finish;auto tg=take.get_future(),fg=finish.get_future();std::shared_ptr<const PreparedProgram> result;
    std::thread worker([&]{PreparationFailure p;check(w->build(p),"actual ready build");built.set_value();tg.wait();result=w->take_result(p);check(bool(result),"actual worker publishes");published.set_value();fg.wait();check(w->retire(p),"published work retirement");});
    check(built.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"ready handoff");x.blocked();PreparationFailure p;check(!w->take_result(p)&&p.kind==PreparationError::wrong_thread,"foreign ready take rejects");check(!w->retire(p)&&p.kind==PreparationError::wrong_thread,"foreign ready retirement rejects");
    take.set_value();check(published.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"published handoff");x.blocked();check(w->observe().preparation.result_published&&w->observe().admission_retained,"publication does not release Service reservation");
    finish.set_value();worker.join();x.finish();check(result->normal_instances.size()==1&&!result->normal_instances[0].sample->pcm[0].samples.empty(),"actual immutable PCM survives retirement and unload");
}
void lifetime(const char* path){
    Fixture x(path);auto w=x.admit();auto moved=std::move(*x.service);BankServiceFailure f;check(!x.service->close_when_idle(f)&&f.kind==BankServiceError::moved_service,"moved Service rejects");x.service.reset();x.service.emplace(std::move(moved));x.blocked();x.service.reset();
    PreparationFailure p;check(w->build(p),"retained work builds after last Service object destruction");auto result=w->take_result(p);check(result&&w->retire(p)&&!w->observe().admission_retained,"retained actual Service state outlives public owner");
}
void dropped(const char* path){
    Fixture x(path);{auto w=x.admit();x.blocked();}PreparationFailure p;BankServiceFailure f;auto ready=x.admit();check(ready->build(p)&&x.domain->observe()[0].depth!=0,"abandoned ready work owns actual barrier");x.blocked();ready.reset();check(x.domain->observe()[0].depth==0,"wrapper abandonment cleans actual work barrier");x.finish();
}
void sync_revocation(const char* path){
    Fixture x(path);AudioResourceFailure rf;auto movie=x.domain->surround_movie(rf);check(bool(movie),"sync publication exclusion");std::promise<void> done;
    std::thread worker([&]{PreparationFailure p;BankServiceFailure f;check(!x.service->prepare_program(command(AudioBankOpcode::play,token),p,f)&&p.kind==PreparationError::unloaded,"existing sync operation is revoked by unload");done.set_value();});
    await([&]{return x.domain->observe()[3].waiting!=0;},"actual sync publication wait");BankServiceFailure f;check(x.service->unload(command(AudioBankOpcode::unload,token),f),"sync stack activity does not become owned admission");check(movie->release(rf),"release sync exclusion");check(done.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"revoked sync operation finishes");worker.join();check(x.service->close_when_idle(f),"sync operation leaves no admission leak");
}
}
int main(int argc,char** argv){if(argc!=2){std::fprintf(stderr,"usage: test_program_admission <pinned_original_MS2>\n");return 2;}queued(argv[1]);rollback(argv[1]);running(argv[1]);ready(argv[1]);lifetime(argv[1]);dropped(argv[1]);sync_revocation(argv[1]);std::puts("PASS_NATIVE_PROGRAM_ADMISSION: 7 actual Service admission lifecycle cases; no scheduler/callback/context/hardware fidelity claim");return std::fflush(stdout)||std::ferror(stdout)?1:0;}
