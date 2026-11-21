#include "mka/retail_middleware_work.hpp"
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
using namespace mka::retail;
namespace {
bool fail_allocation=false;
void check(bool value,const char* text){
    if(!value){std::fprintf(stderr,"FAIL middleware: %s\n",text);throw std::runtime_error(text);}
}
struct Counts {unsigned poll=0,primary=0,query=0,release=0,destroyed=0;};
// Explicit test ports, not retail MWFILE job bodies. Effects are counted so
// replay, lost ownership and occurrence aliasing are observable.
class Child final:public NativeRetailMiddlewareChild {
public:
    explicit Child(std::shared_ptr<Counts> c):counts(std::move(c)){}
    ~Child(){++counts->destroyed;}
    std::shared_ptr<Counts> counts;
    RetailMiddlewarePollStatus status=RetailMiddlewarePollStatus::source_zero;
    mutable bool query_fails_once=false;
    bool needs_release=true;
    unsigned retain_release_once=0;
    bool tested_nested=false;
    std::weak_ptr<NativeRetailMiddlewareWorkGroup> group;
    std::weak_ptr<NativeRetailMiddlewareDispatcher> dispatcher;
    std::optional<RetailMiddlewarePollStatus> poll(RetailMiddlewareWorkFailure&)override{
        ++counts->poll;
        if(auto g=group.lock()){
            RetailMiddlewareWorkFailure nested;const auto recursive=g->poll(nested);
            check(recursive&&*recursive==0,"source recursive group polling suppressed");
            auto d=dispatcher.lock();check(d&&!d->pump(nested)&&nested.kind==RetailMiddlewareWorkError::active,
                "parent pump cannot drain other work from an active group callback");
            tested_nested=true;
        }
        return status;
    }
    bool primary_complete(RetailMiddlewareWorkFailure&)override{++counts->primary;return true;}
    std::optional<bool> release_required(RetailMiddlewareWorkFailure& f)const override{
        ++counts->query;
        if(query_fails_once){query_fails_once=false;f.kind=RetailMiddlewareWorkError::child_failed;return {};}
        return needs_release;
    }
    std::optional<RetailMiddlewareReleaseResult> release(RetailMiddlewareWorkFailure&)override{
        ++counts->release;
        if(retain_release_once){--retain_release_once;return RetailMiddlewareReleaseResult::retained;}
        return RetailMiddlewareReleaseResult::released;
    }
};
}
void* operator new(std::size_t n){if(fail_allocation)throw std::bad_alloc();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
int main(){
    // A failing assertion must not open a Windows crash dialog while strict
    // owner destructors diagnose unfinished fixture work during unwinding.
    std::set_terminate([]{std::fputs("FAIL middleware: owner left active during fixture unwind\n",stderr);std::fflush(stderr);std::_Exit(3);});
    try{
        RetailMiddlewareWorkFailure f;
        auto group=NativeRetailMiddlewareWorkGroup::create(f);
        auto dispatcher=NativeRetailMiddlewareDispatcher::create(f);
        auto foreign=NativeRetailMiddlewareDispatcher::create(f);
        check(group&&dispatcher&&foreign&&!group->observe().enabled&&!group->observe().children,"actual source constructor starts empty and disabled");
        check(group->configure(2,f),"native reserve is a hint, not a guest resource limit");
        auto counts=std::make_shared<Counts>();auto child=std::make_shared<Child>(counts);
        child->group=group;child->dispatcher=dispatcher;
        auto first=group->append(child,f);check(bool(first),"real submission before parent binding");
        check(dispatcher->attach(group,f)&&dispatcher->completion_phase(*first,f)==RetailMiddlewareCompletionPhase::queued,
            "actual parent association publishes queued ticket issuer");
        check(!foreign->completion_phase(*first,f)&&f.kind==RetailMiddlewareWorkError::foreign_submission,
            "foreign dispatcher cannot claim same ticket");
        check(!dispatcher->completion_phase({},f)&&f.kind==RetailMiddlewareWorkError::invalid_submission,"empty ticket is invalid");
        const auto direct=group->poll(f);
        check(direct&&*direct==0&&child->tested_nested&&counts->primary==0,"direct poll preserves status0 and blocks nested parent work");
        child->status=RetailMiddlewarePollStatus::source_one;
        const auto retained=dispatcher->pump(f);
        check(retained&&*retained==0&&group->observe().children==1,"status1 retains the same child");
        auto second=group->append(child,f);check(bool(second),"duplicate child references are separate source occurrences");
        child->status=RetailMiddlewarePollStatus::transfer_to_parent;child->retain_release_once=1;
        check(!dispatcher->pump(f)&&f.kind==RetailMiddlewareWorkError::release_pending,
            "incomplete release retains completion ownership");
        check(group->observe().children==0&&dispatcher->observe().pending_completions==2&&counts->primary==1&&counts->release==1,
            "both occurrences transfer without replaying primary");
        check(dispatcher->completion_phase(*first,f)==RetailMiddlewareCompletionPhase::release_pending&&
            dispatcher->completion_phase(*second,f)==RetailMiddlewareCompletionPhase::primary_pending,
            "same child has independent per-submission phases");
        std::weak_ptr<Child> weak=child;child.reset();
        check(!weak.expired(),"pending entries retain exact child backing");
        const auto completed=dispatcher->pump(f);
        check(completed&&*completed==2&&counts->primary==2&&counts->release==3&&weak.expired()&&counts->destroyed==1,
            "release retry skips completed primary, duplicate completion executes once per entry");
        check(!dispatcher->completion_phase(*first,f)&&f.kind==RetailMiddlewareWorkError::stale_submission,
            "retired ticket has exact stale identity");

        auto recovery_counts=std::make_shared<Counts>();auto recovery=std::make_shared<Child>(recovery_counts);
        recovery->status=static_cast<RetailMiddlewarePollStatus>(99);
        auto recovery_ticket=group->append(recovery,f);check(bool(recovery_ticket),"recovery child admission");
        check(!dispatcher->pump(f)&&f.kind==RetailMiddlewareWorkError::invalid_status&&
            group->observe().children==1&&recovery_counts->primary==0,"unknown child status cannot fabricate completion");
        recovery->status=RetailMiddlewarePollStatus::transfer_to_parent;recovery->query_fails_once=true;
        check(!dispatcher->pump(f)&&f.kind==RetailMiddlewareWorkError::child_failed&&
            dispatcher->completion_phase(*recovery_ticket,f)==RetailMiddlewareCompletionPhase::primary_done,
            "failed flag query retains latched primary result");
        check(dispatcher->pump(f)==1&&recovery_counts->primary==1&&recovery_counts->release==1,"query retry never replays primary effect");
        recovery.reset();

        auto allocated_counts=std::make_shared<Counts>();auto allocated=std::make_shared<Child>(allocated_counts);
        allocated->status=RetailMiddlewarePollStatus::transfer_to_parent;allocated->needs_release=false;
        fail_allocation=true;auto failed_admission=group->append(allocated,f);fail_allocation=false;
        check(!failed_admission&&f.kind==RetailMiddlewareWorkError::allocation&&!group->observe().children,
            "failed ticket allocation admits no occurrence");
        auto allocated_ticket=group->append(allocated,f);check(bool(allocated_ticket),"retry admission");
        fail_allocation=true;const auto failed_poll=dispatcher->pump(f);fail_allocation=false;
        check(!failed_poll&&f.kind==RetailMiddlewareWorkError::allocation&&allocated_counts->poll==0&&group->observe().children==1,
            "completion storage is reserved before irreversible child poll");
        check(dispatcher->pump(f)==1&&allocated_counts->primary==1&&allocated_counts->release==0,
            "source no-release branch completes without an invented release call");
        allocated.reset();

        bool wrong_thread=false;
        std::thread worker([&]{RetailMiddlewareWorkFailure tf;wrong_thread=!dispatcher->pump(tf)&&tf.kind==RetailMiddlewareWorkError::wrong_thread;});
        worker.join();check(wrong_thread,"foreign thread cannot execute main-owned work");
        check(dispatcher->detach(group,f)&&group->shutdown(f)&&group->observe().closed,
            "configured group can drain detach and shut down");
        check(!dispatcher->completion_phase(*second,f)&&f.kind==RetailMiddlewareWorkError::stale_submission,
            "detaching does not convert retired ticket into foreign identity");
        check(dispatcher->shutdown(f)&&foreign->shutdown(f),"actual dispatcher lifecycle closes normally");
        group.reset();dispatcher.reset();foreign.reset();
        check(counts->destroyed==1&&recovery_counts->destroyed==1&&allocated_counts->destroyed==1,
            "tickets retain issuer metadata without retaining child payloads");
        std::puts("PASS native middleware transfer/retry/ticket/lifetime checks; test ports only, no retail jobs or game/device");
        return 0;
    }catch(const std::exception& e){fail_allocation=false;std::fprintf(stderr,"FAIL native middleware: %s\n",e.what());return 1;}
}
