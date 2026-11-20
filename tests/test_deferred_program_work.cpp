#include "mka/bank_preparation.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
using namespace mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::abort();}}
template<class P> void await(P p,const char* why){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!p()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
struct Fixture {
    std::shared_ptr<AudioResourceDomain> domain;
    std::optional<PreparedBank> bank;
    std::weak_ptr<SoundArchiveBacking> backing;
    explicit Fixture(const char* path){
        AudioResourceFailure rf;domain=AudioResourceDomain::create(rf);check(bool(domain),"domain");
        ArchiveFailure af;auto archive=SoundArchive::open(path,domain,af);check(bool(archive),"real archive");
        auto source=archive->read_bank("announcr",af);check(bool(source),"actual ANNOUNCR source");
        check(source->backing->resource_domain()==domain,"same backing domain");backing=source->backing;
        BankPreparationReport report;PreparationFailure f;bank=PreparedBank::prepare(std::move(*source),report,f);
        check(bool(bank)&&report.resource_precreation_skipped,"real deferred bank");
    }
    std::shared_ptr<PreparedProgramWork> admit(){PreparationFailure f;auto w=bank->admit_deferred_normal(0,f);check(bool(w),"exact real selector0 admission");return w;}
    bool occupied(){PreparationFailure f;auto c=bank->sample_cache(f);check(bool(c),"cache inspection");for(auto x:*c)if(x.occupied)return true;return false;}
};
void queued(const char* path){
    Fixture x(path);auto before=x.domain->observe();auto w=x.admit();auto after=x.domain->observe();
    for(unsigned i=0;i<4;++i)check(before[i].depth==after[i].depth&&before[i].waiting==after[i].waiting,"queue admission starts no resource work");
    check(!x.occupied()&&w->observe().state==ProgramWorkState::queued,"queue owns no samples");
    check(w->cancel()==ProgramWorkCancellation::queued_removed,"queued cancellation receipt");
    PreparationFailure f;check(!w->build(f)&&f.kind==PreparationError::cancelled,"cancelled queue never builds");
    check(w->observe().state==ProgramWorkState::cancelled&&!x.occupied(),"cancel differs from retirement");
    check(w->retire(f)&&w->observe().state==ProgramWorkState::retired,"queued retirement");
    check(!w->build(f)&&f.kind==PreparationError::invalid_work_state,"retired job cannot rerun");
    check(w->cancel()==ProgramWorkCancellation::not_cancellable,"retired cancellation rejected");
}
void running_cancel(const char* path){
    Fixture x(path);auto w=x.admit();AudioResourceFailure rf;auto movie=x.domain->surround_movie(rf);check(bool(movie),"actual movie publication exclusion");
    std::promise<void> built,finish;auto finish_gate=finish.get_future();
    std::thread worker([&]{PreparationFailure f;check(!w->build(f)&&f.kind==PreparationError::cancelled,"actual running build cancelled");built.set_value();finish_gate.wait();check(!w->take_result(f)&&f.kind==PreparationError::cancelled,"cancelled work has no result");check(w->retire(f),"cancelled work retires on actual worker");});
    await([&]{return x.domain->observe()[3].waiting!=0;},"actual publication waiter");
    check(w->observe().state==ProgramWorkState::building&&w->observe().cancellation_open,"running cancellation window");
    check(w->cancel()==ProgramWorkCancellation::running_requested,"request is running not queue removal");
    PreparationFailure f;check(!w->retire(f)&&f.kind==PreparationError::wrong_thread,"foreign running retirement rejects");
    check(w->observe().state==ProgramWorkState::building,"request and foreign cleanup do not retire");
    check(movie->release(rf),"actual movie release");
    check(built.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"actual worker finishes after publication exclusion");
    check(w->observe().state==ProgramWorkState::cancelled&&!x.occupied(),"cancel cleans sole flag40-clear association");
    check(x.domain->observe()[0].depth!=0,"cancelled worker still owns program barrier until retirement");
    finish.set_value();worker.join();check(w->observe().state==ProgramWorkState::retired&&x.domain->observe()[0].depth==0,"actual retirement releases program barrier");
}
void ready_and_unload(const char* path,bool unload){
    Fixture x(path);auto w=x.admit();std::promise<void> ready,proceed;auto gate=proceed.get_future();
    std::shared_ptr<const PreparedProgram> result;
    std::thread worker([&]{PreparationFailure f;check(w->build(f),"actual ready build");ready.set_value();gate.wait();result=w->take_result(f);if(unload)check(!result&&f.kind==PreparationError::unloaded,"unload revokes ready publication");else check(bool(result),"same-worker take publishes");check(w->retire(f),"same-worker terminal retirement");});
    check(ready.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"ready handoff");
    PreparationFailure f;check(w->observe().state==ProgramWorkState::ready,"actual ready phase");
    check(!w->take_result(f)&&f.kind==PreparationError::wrong_thread,"foreign take rejects");
    check(!w->retire(f)&&f.kind==PreparationError::wrong_thread&&w->observe().state==ProgramWorkState::ready,"foreign retirement preserves ready");
    check(w->cancel()==ProgramWorkCancellation::not_cancellable,"closed cancellation window");
    if(unload){x.bank->unload();check(!x.backing.expired(),"work pins actual backing after unload");}
    proceed.set_value();worker.join();check(w->observe().state==ProgramWorkState::retired,"ready path retired");
    if(unload){check(x.backing.expired(),"explicit retirement releases final backing");return;}
    check(result->normal_instances.size()==1&&!result->normal_instances[0].source_preserve_resource_after_instance,"real selector0 flag40-clear instance");
    auto pcm=result->normal_instances[0].sample;check(bool(pcm)&&!pcm->pcm.empty()&&x.occupied(),"real PCM and independent cache association");
    AudioResourceFailure rf;auto sample_gate=x.domain->acquire(AudioResourceWork::sample,rf);check(bool(sample_gate),"actual sample-operation gate");
    auto cancelled=x.admit();std::thread cancelling_worker([&]{PreparationFailure cf;check(!cancelled->build(cf)&&cf.kind==PreparationError::cancelled,"second shared-cache program cancelled");check(cancelled->retire(cf),"shared-cache cancellation retires on worker");});
    await([&]{return x.domain->observe()[1].waiting!=0;},"second program waits on actual sample operation");
    check(cancelled->cancel()==ProgramWorkCancellation::running_requested,"shared-cache running cancellation");
    check(sample_gate->release(rf),"release real sample gate");cancelling_worker.join();
    check(x.occupied()&&result->normal_instances[0].sample==pcm,"cancelled other program does not clear live instance association");
    auto second=x.admit();check(second->build(f),"second actual program build");auto other=second->take_result(f);check(other&&other->normal_instances[0].sample==pcm,"actual instances share cached PCM");check(second->retire(f),"second work retirement");
    result.reset();check(x.occupied(),"first release does not clear another actual instance");
    other.reset();check(!x.occupied()&&!pcm->pcm[0].samples.empty(),"last instance expires association while exported PCM survives");
}
void unload_before_build(const char* path){
    Fixture x(path);auto w=x.admit();x.bank->unload();check(!x.backing.expired(),"queued admitted work pins backing");
    PreparationFailure f;check(!w->build(f)&&f.kind==PreparationError::unloaded,"unloaded bank cannot build admitted work");
    check(w->observe().state==ProgramWorkState::failed&&!w->take_result(f),"unloaded job never publishes success");
    check(w->retire(f)&&x.backing.expired(),"failed work explicit retirement releases backing");
}
}
int main(int argc,char** argv){
    if(argc!=2){std::fprintf(stderr,"usage: test_deferred_program_work <pinned_original_MS2>\n");return 2;}
    queued(argv[1]);running_cancel(argv[1]);ready_and_unload(argv[1],false);ready_and_unload(argv[1],true);unload_before_build(argv[1]);
    std::puts("PASS_NATIVE_DEFERRED_PROGRAM_WORK: 5 actual ANNOUNCR lifecycle cases; CPU-only, no runtime callback or hardware fidelity claim");
}
