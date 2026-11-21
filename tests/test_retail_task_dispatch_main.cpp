#include "mka/retail_main.hpp"
#include "mka/retail_task_domain.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"

#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
void check(bool value,const char* message) {
    if(!value) throw std::runtime_error(message);
}
struct Context final : NativeRetailTaskScriptContext {
    std::uint32_t source_cookie() const noexcept override {return 5;}
    void cleanup_from_12fa00() override {++cleanups;}
    int cleanups{};
};
struct Body final : NativeRetailTaskBody {
    NativeTaskStep run() override {++calls;return effect();}
    void cleanup(bool) override {++cleanups;}
    std::function<NativeTaskStep()> effect;
    int calls{},cleanups{};
};
struct Owners {
    explicit Owners(const std::filesystem::path& root):startup(volume(root),{}),main(startup,samples) {
        RetailGameSessionFailure gf;
        startup.game_session=NativeRetailGameSession::create(gf);
        RetailGamePoolFailure pf;
        startup.game_pool=NativeRetailGamePool::initialize(3,pf);
        domain=startup.game_session->task_domain(gf);
        RetailTaskDomainFailure df;
        check(domain && domain->reset_task_roots_from_12f0d0(startup.game_pool,df),"actual source root birth");
        registry=domain->registry(df);
        contexts=domain->script_context(df);
        check(registry && contexts,"same Session domain context and registry");
    }
    static mka::content::NativeContentVolume volume(const std::filesystem::path& root) {
        mka::content::ContentVolumeFailure f;
        auto v=mka::content::NativeContentVolume::open(root,f);
        check(v.has_value(),"disposable native content directory");
        return std::move(*v);
    }
    std::shared_ptr<Body> publish(int priority,std::function<NativeTaskStep()> effect) {
        auto b=std::make_shared<Body>();b->effect=std::move(effect);
        RetailTaskRegistryFailure f;
        check(bool(registry->publish_task(0x1000+priority,priority,10+priority,b,f)),"actual ordered native body");
        return b;
    }
    NativeRetailStartupState startup;
    mka::input::ControllerInput samples;
    NativeRetailMainState main;
    NativeRetailTaskDomain* domain{};
    NativeRetailTaskRegistry* registry{};
    NativeRetailTaskScriptContextState* contexts{};
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"requires disposable content directory");
    const std::filesystem::path root=argv[1];
    {
        Owners o(root);
        StartupFailure f;
        auto session=o.startup.game_session;o.startup.game_session.reset();
        check(!main_binding::dispatch_retail_task_lists(o.main,f),"Main refuses missing Session");
        o.startup.game_session=std::move(session);
        RetailTaskScriptContextFailure sf;
        check(o.contexts->active_context(sf).has_value() && !*o.contexts->active_context(sf),"BSS-born context is empty");
        auto first_context=std::make_shared<Context>();
        auto second_context=std::make_shared<Context>();
        // Test supplies an already-matched typed object. This does not claim
        // to exercise the still-unimplemented2168F0 scan/216990 producer.
        check(o.contexts->publish_matched_context_from_2168f0(first_context,sf),"explicit matched test context");
        auto first=o.publish(1,[&] {
            auto active=o.contexts->active_context(sf);
            check(active && *active==first_context,"first body sees actual retained context reference");
            return NativeTaskStep::returned(1.0f);
        });
        auto second=o.publish(2,[&] {
            auto active=o.contexts->active_context(sf);
            check(active && !*active,"2168E0 cleared context before next body");
            check(o.contexts->publish_matched_context_from_2168f0(second_context,sf),"second body context publication");
            return NativeTaskStep::returned(1.0f);
        });
        bool refused=false;
        std::thread other([&] {StartupFailure ef;refused=!main_binding::dispatch_retail_task_lists(o.main,ef);});
        other.join();
        check(refused && first->calls==0 && second->calls==0,"foreign dispatch has no body effects");
        check(main_binding::dispatch_retail_task_lists(o.main,f) && first->calls==1 && second->calls==1,
              "actual Main dispatch drives both source task bodies");
        auto active=o.contexts->active_context(sf);
        check(active && !*active && first_context->cleanups==0 && second_context->cleanups==0,
              "each normal return clears nonowning reference without deleting script object");
        RetailTaskDomainFailure df;
        check(o.domain->cancel_first_root_from_12f030(df),"retire source tasks before root reset");
        check(o.contexts->publish_matched_context_from_2168f0(first_context,sf) &&
              o.domain->reset_task_roots_from_12f0d0(o.startup.game_pool,df) &&
              o.domain->script_context(df)==o.contexts && *o.contexts->active_context(sf)==first_context,
              "root reset retains Session script-context identity and does not invent source clear");
    }
    {
        Owners o(root);StartupFailure f;RetailTaskScriptContextFailure sf;
        auto context=std::make_shared<Context>();
        auto body=o.publish(1,[&]() -> NativeTaskStep {
            check(o.contexts->publish_matched_context_from_2168f0(context,sf),"transfer body context");
            throw detail::NativeMainReentry{&o.main,9};
        });
        bool propagated=false;
        try {(void)main_binding::dispatch_retail_task_lists(o.main,f);}
        catch(const detail::NativeMainReentry& e) {propagated=e.owner==&o.main && e.value==9;}
        check(propagated && body->calls==1 && *o.contexts->active_context(sf)==context,
              "Main transfer skips normal-return2168E0 and preserves real context");
    }
    {
        Owners o(root);StartupFailure f;RetailTaskScriptContextFailure sf;
        auto context=std::make_shared<Context>();
        auto body=o.publish(1,[&]() -> NativeTaskStep {
            check(o.contexts->publish_matched_context_from_2168f0(context,sf),"failed body context");
            throw std::runtime_error("explicit fixture after-effect failure");
        });
        check(!main_binding::dispatch_retail_task_lists(o.main,f) && body->calls==1 &&
              *o.contexts->active_context(sf)==context &&
              !main_binding::dispatch_retail_task_lists(o.main,f) && body->calls==1,
              "ordinary partial failure retains context and cannot replay body");
    }
    {
        NativeRetailTaskScriptContextState contexts;RetailTaskScriptContextFailure sf;
        auto context=std::make_shared<Context>();std::weak_ptr<Context> weak=context;
        check(contexts.publish_matched_context_from_2168f0(context,sf),"weak native context publication");
        context.reset();
        check(weak.expired() && !contexts.active_context(sf) && sf.kind==RetailTaskScriptContextError::expired_context,
              "context reference does not extend source object lifetime or silently return empty");
        check(contexts.clear_from_2168e0(sf) && contexts.active_context(sf).has_value() && !*contexts.active_context(sf),
              "source clear requires no dereference of expired prior context");
    }
    std::cout<<"PASS native Main task dispatch and per-body script-context release; full game open\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
