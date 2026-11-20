#include "mka/runtime_preparation.hpp"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>
using namespace mka::sound;
namespace {
void check(bool ok,const char* why){if(!ok){std::fprintf(stderr,"FAIL: %s\n",why);std::abort();}}
template<class P>void await(P p,const char* why){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!p()){check(std::chrono::steady_clock::now()<end,why);std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
struct Fixture {
 std::shared_ptr<AudioResourceDomain> domain;
 std::optional<PreparedBank> bank;
 Fixture(const char* path,const char* name){AudioResourceFailure rf;domain=AudioResourceDomain::create(rf);check(bool(domain),"domain");ArchiveFailure af;auto archive=SoundArchive::open(path,domain,af);check(bool(archive),"actual archive");auto source=archive->read_bank(name,af);check(bool(source)&&source->backing->resource_domain()==domain,"actual same-domain named source");BankPreparationReport report;PreparationFailure f;bank=PreparedBank::prepare(std::move(*source),report,f);check(bool(bank),"actual bank preparation");}
 RuntimeCacheObservation record(){PreparationFailure f;auto c=bank->runtime_cache(f);check(c&&!c->empty()&&(*c)[0].program_index==0,"runtime record inspection");return (*c)[0];}
 std::unique_ptr<RuntimeProgramReference> acquire(){PreparationFailure f;auto r=bank->acquire_runtime_program(0,f);check(bool(r)&&r->active()&&r->preparation(),"actual runtime acquisition");return r;}
};
void constructor(const std::shared_ptr<const NativeRuntimePreparation>& r,const std::shared_ptr<AudioResourceDomain>& d){
 check(r&&r->data()&&r->resource_domain()==d&&r->initial_command_index()==0,"typed runtime owns actual data and domain");
 const auto& c=r->initial_controls();check(c.gain_q14==0x4000&&c.pan_q14==0&&c.pitch_q12==0x1000&&c.secondary_gain_q14==0x4000&&c.secondary_pan_q14==0&&c.secondary_pitch_q12==0x1000,"exact known constructor factors only");
 check(r->runtime_flags()==static_cast<std::uint16_t>(r->data()->effective_flags),"runtime low16 flags preserve actual data flags");
}
void eager(const char* path){
 Fixture x(path,"bg_armory");auto initial=x.record();check(initial.cached&&!initial.deferred&&initial.references==0&&!initial.building,"actual eager cache starts retained with zero acquisitions");
 auto a=x.acquire(),b=x.acquire();auto runtime=a->preparation();constructor(runtime,x.domain);check(runtime==b->preparation()&&x.record().references==2,"eager acquisitions share exact runtime");
 PreparationFailure f;check(a->release(f)&&!a->active()&&!a->preparation()&&x.record().references==1,"first semantic release");check(!a->release(f)&&f.kind==PreparationError::invalid_work_state&&x.record().references==1,"duplicate release cannot decrement another acquisition");
 b.reset();check(x.record().references==0&&x.record().cached,"destructor release retains eager cache");auto c=x.acquire();check(c->preparation()==runtime,"eager cache identity survives zero references");
}
void deferred(const char* path){
 Fixture x(path,"announcr");check(x.record().deferred&&!x.record().cached&&x.record().references==0,"actual deferred starts absent");
 auto a=x.acquire(),b=x.acquire();auto old=a->preparation();auto data=old->data();constructor(old,x.domain);check(old==b->preparation()&&x.record().references==2,"deferred same-record sharing");
 PreparationFailure f;check(a->release(f)&&x.record().references==1&&x.record().cached,"one deferred acquisition remains");b.reset();check(!x.record().cached&&x.record().references==0,"last destructor detaches deferred runtime despite exported ownership");
 check(old->data()==data&&!data->normal_instances.empty(),"old immutable runtime and data survive detachment");auto next=x.acquire();check(next->preparation()!=old&&next->preparation()->same_bank(*old),"new runtime same bank identity while old runtime survives");
 Fixture other(path,"announcr");auto foreign=other.acquire();check(!foreign->preparation()->same_bank(*old),"different native bank instance never aliases bank identity");
 check(next->release(f)&&!x.record().cached,"explicit deferred last release detaches");
 auto bad=x.bank->acquire_runtime_program(0x7fffffff,f);check(!bad&&f.kind==PreparationError::absent_selector&&!x.record().cached&&x.record().references==0&&!x.record().building,"invalid selector leaves no acquisition or building state");
}
void concurrent(const char* path){
 Fixture x(path,"announcr");AudioResourceFailure rf;auto barrier=x.domain->acquire(AudioResourceWork::program,rf);check(bool(barrier),"actual program operation barrier");
 std::unique_ptr<RuntimeProgramReference> first,second;std::promise<void> start;auto go=start.get_future().share();
 auto worker=[&](std::unique_ptr<RuntimeProgramReference>& out){go.wait();PreparationFailure f;out=x.bank->acquire_runtime_program(0,f);check(bool(out),"concurrent actual runtime acquisition");};
 std::thread a(worker,std::ref(first)),b(worker,std::ref(second));start.set_value();await([&]{return x.domain->observe()[0].waiting==2;},"both native threads wait on real program barrier");check(barrier->release(rf),"release actual program barrier");
 // Completion promises bound native acquisition rather than assuming thread speed.
 std::promise<void> joined;auto joined_future=joined.get_future();std::thread joiner([&]{a.join();b.join();joined.set_value();});check(joined_future.wait_for(std::chrono::seconds(10))==std::future_status::ready,"concurrent acquisitions finish");joiner.join();
 check(first->preparation()==second->preparation()&&x.record().references==2&&!x.record().building,"concurrent workers share one committed runtime");first.reset();second.reset();check(!x.record().cached&&x.record().references==0,"concurrent reference destructors detach exactly once");
}
void unload_race(const char* path){
 Fixture x(path,"announcr");AudioResourceFailure rf;auto movie=x.domain->surround_movie(rf);check(bool(movie),"actual movie publication exclusion");
 std::promise<void> done;std::thread worker([&]{PreparationFailure f;auto r=x.bank->acquire_runtime_program(0,f);check(!r&&f.kind==PreparationError::unloaded,"unload rejects actual unpublished runtime");done.set_value();});
 await([&]{return x.domain->observe()[3].waiting!=0;},"runtime builder actual publication wait");x.bank->unload();check(movie->release(rf),"release actual movie exclusion");check(done.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"revoked runtime builder finishes");worker.join();
 PreparationFailure f;check(!x.bank->runtime_cache(f)&&f.kind==PreparationError::unloaded,"unloaded record cannot expose stale commit");check(!x.bank->acquire_runtime_program(0,f)&&f.kind==PreparationError::unloaded,"unloaded bank cannot acquire runtime");
}
void movie_owner_conflict(const char* path){
 Fixture x(path,"announcr");AudioResourceFailure rf;auto movie=x.domain->surround_movie(rf);check(bool(movie),"movie-owning native thread");
 std::unique_ptr<RuntimeProgramReference> foreign;std::promise<void> done;
 std::thread worker([&]{PreparationFailure f;foreign=x.bank->acquire_runtime_program(0,f);check(bool(foreign),"foreign runtime worker succeeds after actual exclusion release");done.set_value();});
 await([&]{return x.domain->observe()[3].waiting!=0;},"actual foreign runtime worker publication wait");
 auto before=x.record();check(before.building&&!before.cached&&before.references==0,"actual foreign builder owns incomplete runtime record");
 PreparationFailure f;auto r=x.bank->acquire_runtime_program(0,f);
 check(!r&&f.kind==PreparationError::resource_access&&f.resource.kind==AudioResourceError::recursive_work_conflict,"movie owner cannot wait for actual foreign program owner");
 auto after=x.record();check(after.program_index==before.program_index&&after.building==before.building&&after.cached==before.cached&&after.references==before.references&&after.deferred==before.deferred,"conflicting admission leaves foreign builder record unchanged");
 check(movie->release(rf),"movie owner releases for actual foreign worker");check(done.get_future().wait_for(std::chrono::seconds(10))==std::future_status::ready,"foreign runtime worker finishes");worker.join();
 check(x.record().cached&&x.record().references==1&&!x.record().building,"foreign worker alone commits one runtime reference");foreign.reset();check(!x.record().cached&&x.record().references==0,"foreign reference cleanup detaches runtime");
 auto retry=x.acquire();check(bool(retry),"actual retry after foreign owner and exclusion retire");
}
}
int main(int argc,char** argv){if(argc!=2){std::fprintf(stderr,"usage: test_runtime_preparation <pinned_original_MS2>\n");return 2;}eager(argv[1]);deferred(argv[1]);concurrent(argv[1]);unload_race(argv[1]);movie_owner_conflict(argv[1]);std::puts("PASS_NATIVE_RUNTIME_PREPARATION: 5 actual bank runtime ownership cases; no callback/clone/output/voice/fidelity claim");return std::fflush(stdout)||std::ferror(stdout)?1:0;}
