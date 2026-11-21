#include "mka/retail_profile_task_retirement.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_task_domain.hpp"
#include "mka/retail_game_pool.hpp"
#include "mka/retail_observer_list.hpp"
#include "native_retail_startup.hpp"
#include "retail_main_control.hpp"

#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#define NOMINMAX
#include <windows.h>

namespace {
void check(bool value,const char* message) {
    if(!value) { std::cerr<<message<<'\n'; std::abort(); }
}
struct Body final : mka::retail::NativeRetailTaskBody {
    Body(int id,std::vector<int>& events):id(id),events(events) {}
    mka::retail::NativeTaskStep run() override {
        return mka::retail::NativeTaskStep::returned(1.0f);
    }
    void cleanup(bool release_allowed) override {
        ++calls;
        events.push_back(id);
        released.push_back(release_allowed);
        if(probe_guard) {
            mka::retail::RetailTaskRegistryFailure f;
            guarded_refusal=!registry->retire_link(self.lock(),f) &&
                f.kind==mka::retail::RetailTaskRegistryError::callout_active;
        }
        if(ordinary_error) throw std::runtime_error("native body retirement error");
    }
    int id,calls{};
    std::vector<int>& events;
    std::vector<bool> released;
    mka::retail::NativeRetailTaskRegistry* registry{};
    std::weak_ptr<mka::retail::NativeRetailTaskObject> self;
    bool probe_guard{},guarded_refusal{},ordinary_error{};
};
struct Observed final : mka::retail::NativeRetailObservedObject {
    Observed(int id,std::vector<int>& events):id(id),events(events) {}
    std::uint32_t source_cookie() const noexcept override { return cookie; }
    void cleanup_from_12fa00() override {
        ++calls;
        events.push_back(id);
        if(nested_kind) {
            mka::retail::RetailTaskRegistryFailure f;
            if(!registry->retire_matching_from_12edc0(*nested_kind,f))
                throw std::runtime_error("nested source selective query refused");
        }
        if(transfer) throw mka::retail::detail::NativeMainReentry{main,23};
        cookie=0;
    }
    int id,calls{};
    std::vector<int>& events;
    mka::retail::NativeRetailTaskRegistry* registry{};
    mka::retail::NativeRetailMainState* main{};
    std::optional<std::uint32_t> nested_kind;
    std::uint32_t cookie{1};
    bool transfer{};
};
std::shared_ptr<mka::retail::NativeRetailTaskObject> publish(
    mka::retail::NativeRetailTaskRegistry& registry,
    std::uint32_t kind,std::int32_t priority,std::uint32_t cookie,
    const std::shared_ptr<Body>& body) {
    mka::retail::RetailTaskRegistryFailure f;
    auto task=registry.publish_task(kind,priority,cookie,body,f);
    check(bool(task),"publish real executable task on retained pool");
    body->registry=&registry;
    body->self=task;
    return task;
}
std::shared_ptr<mka::retail::NativeRetailTaskObject> publish_observed(
    mka::retail::NativeRetailTaskRegistry& registry,
    const std::shared_ptr<mka::retail::NativeRetailGamePool>& pool,
    std::uint32_t kind,std::int32_t priority,std::uint32_t cookie,
    const std::shared_ptr<Body>& body,const std::shared_ptr<Observed>& observed,
    const std::shared_ptr<Observed>& observed_ec={}) {
    using namespace mka::retail;
    auto e8=std::make_shared<NativeRetailObserverList>(pool);
    auto ec=std::make_shared<NativeRetailObserverList>(pool);
    RetailObserverListFailure of;
    auto link=e8->insert_owned_argument_from_12fee0(observed,of);
    check(bool(link),"attach real owned E8 source observer");
    if(observed_ec) {
        auto ec_link=ec->insert_owned_argument_from_12fee0(observed_ec,of);
        check(bool(ec_link),"attach real owned EC source observer");
        observed_ec->registry=&registry;
    }
    RetailTaskRegistryFailure rf;
    auto task=registry.publish_task(kind,priority,cookie,body,e8,ec,rf);
    check(bool(task),"publish executable task with retained E8 observer");
    body->registry=&registry;
    body->self=task;
    observed->registry=&registry;
    return task;
}
} // namespace

int main() {
    using namespace mka::retail;
    const auto temp=std::filesystem::temp_directory_path();
    std::filesystem::path root;
    for(unsigned i=0;;++i) {
        root=temp/("mka-selective-task-owned-"+
            std::to_string(GetCurrentProcessId())+"-"+
            std::to_string(GetTickCount64())+"-"+std::to_string(i));
        std::error_code error;
        if(std::filesystem::create_directory(root,error)) break;
        check(i<100u,"create exclusively owned test directory");
    }
    {
        mka::content::ContentVolumeFailure cf;
        auto volume=mka::content::NativeContentVolume::open(root,cf);
        check(bool(volume),"open owned native volume");
        NativeRetailStartupState startup(std::move(*volume),{});
        StartupFailure sf;
        check(initialize_native_retail_game_session(startup,sf),
              "real retained game Session birth");
        mka::input::ControllerInput samples;
        NativeRetailMainState main(startup,samples);
        RetailProfileMenuFailure mf;
        check(!retire_retail_profile_tasks_from_12edc0(main,0x11004u,mf) &&
              mf.kind==RetailProfileMenuError::session &&
              mf.session.task_domain.kind==RetailTaskDomainError::registry_unborn,
              "unborn source first root is named rather than empty success");

        RetailGameSessionFailure gf;
        auto* domain=startup.game_session->task_domain(gf);
        check(bool(domain),"one persistent task domain");
        RetailGamePoolFailure pf;
        std::shared_ptr<NativeRetailGamePool> pool(
            NativeRetailGamePool::initialize(3,pf));
        check(bool(pool),"actual growable native task link pool");
        RetailTaskDomainFailure df;
        check(domain->reset_task_roots_from_12f0d0(pool,df),
              "source first task root birth");
        auto* registry=domain->registry(df);
        check(bool(registry),"Main and direct test share same Registry");
        std::vector<int> events;
        auto b0=std::make_shared<Body>(0,events);
        auto b1=std::make_shared<Body>(1,events);
        auto b2=std::make_shared<Body>(2,events);
        auto t0=publish(*registry,0x11004u,1,11,b0);
        auto t1=publish(*registry,0x11005u,2,12,b1);
        auto t2=publish(*registry,0x11004u,3,13,b2);
        RetailTaskRegistryFailure rf;
        check(registry->set_current_task(t0,rf),"actual source current task selected");
        b2->probe_guard=true;
        check(retire_retail_profile_tasks_from_12edc0(main,0x11004u,mf) &&
              events==std::vector<int>{2} && b0->calls==0 && b1->calls==0 &&
              b2->calls==1 && b2->released==std::vector<bool>{true} &&
              b2->guarded_refusal,
              "ordered kind match excludes current and guards direct cleanup");
        auto observed=registry->observe(rf);
        check(observed && observed->ordered.size()==3 &&
              observed->ordered[2].object==t2 &&
              observed->ordered[2].object_cookie==0,
              "direct virtual cleanup invalidates cookie while link stays active");
        auto absent=registry->find_first_live(0x99999u,rf);
        observed=registry->observe(rf);
        check(absent && !*absent && observed && observed->ordered.size()==2 &&
              b2->calls==1,
              "subsequent same-root lookup retires stale link without cleanup replay");
        check(registry->clear_current_task(rf) &&
              retire_retail_profile_tasks_from_12edc0(main,0x11004u,mf) &&
              events==std::vector<int>{2,0},
              "cleared current becomes eligible on later source call");
        check(registry->store_object_retention_flag2_from_source(t1,true,rf) &&
              retire_retail_profile_tasks_from_12edc0(main,0x11005u,mf) &&
              b1->calls==0 && b1->released.empty() &&
              registry->find_first_live(0x11005u,rf)->get()==t1.get(),
              "12EFB0 bit2 guard returns before native body retirement");
        check(registry->store_object_retention_flag2_from_source(t1,false,rf) &&
              retire_retail_profile_tasks_from_12edc0(main,0x11005u,mf) &&
              b1->calls==1 && b1->released==std::vector<bool>{true},
              "later source call can release previously retained task");

        auto nested_a=std::make_shared<Body>(3,events);
        auto nested_b=std::make_shared<Body>(4,events);
        auto nested_observed=std::make_shared<Observed>(30,events);
        nested_observed->nested_kind=0x11005u;
        (void)publish_observed(*registry,pool,0x11004u,4,14,
                              nested_a,nested_observed);
        (void)publish(*registry,0x11005u,5,15,nested_b);
        check(retire_retail_profile_tasks_from_12edc0(main,0x11004u,mf) &&
              nested_a->calls==1 && nested_b->calls==1 &&
              nested_observed->calls==1 &&
              events[events.size()-3]==30 &&
              events[events.size()-2]==4 && events.back()==3,
              "owned E8 cleanup nests source query before native body release");
        absent=registry->find_first_live(0x99999u,rf);
        check(absent && !*absent && nested_a->calls==1 && nested_b->calls==1,
              "later lookup does not replay nested direct cleanup");

        auto stale_body=std::make_shared<Body>(8,events);
        auto stale_e8=std::make_shared<Observed>(80,events);
        auto stale_ec=std::make_shared<Observed>(81,events);
        auto stale_task=publish_observed(*registry,pool,0x44000u,6,19,
                                         stale_body,stale_e8,stale_ec);
        check(registry->store_object_cookie_from_source(stale_task,1,rf),
              "source stale link can meet live cookie one after cached rewrite");
        absent=registry->find_first_live(0x99999u,rf);
        check(absent && !*absent && stale_body->calls==1 &&
              stale_e8->calls==1 && stale_ec->calls==1 &&
              events[events.size()-3]==80 &&
              events[events.size()-2]==81 && events.back()==8,
              "generic stale retirement drains stock E8 then EC before body release");

        auto cancel_body=std::make_shared<Body>(9,events);
        auto cancel_e8=std::make_shared<Observed>(90,events);
        auto cancel_ec=std::make_shared<Observed>(91,events);
        (void)publish_observed(*registry,pool,0x55000u,7,20,
                               cancel_body,cancel_e8,cancel_ec);
        check(registry->cancel_first_root_from_12f030(rf) &&
              cancel_body->calls==1 && cancel_e8->calls==1 &&
              cancel_ec->calls==1 && events[events.size()-3]==90 &&
              events[events.size()-2]==91 && events.back()==9,
              "first-root cancellation uses stock E8 EC native release order");

        auto transfer_body=std::make_shared<Body>(5,events);
        auto transfer_observed=std::make_shared<Observed>(50,events);
        transfer_observed->transfer=true;
        transfer_observed->main=&main;
        auto transferred_task=publish_observed(*registry,pool,0x11004u,6,16,
                                               transfer_body,transfer_observed);
        std::weak_ptr<Body> retained_body=transfer_body;
        bool transferred=false;
        try { (void)retire_retail_profile_tasks_from_12edc0(main,0x11004u,mf); }
        catch(const detail::NativeMainReentry&) { transferred=true; }
        check(transferred && transfer_observed->calls==1 &&
              transfer_body->calls==0,
              "source E8 control transfer precedes native body retirement");
        transfer_body.reset();
        transferred_task.reset();
        check(!retained_body.expired(),"pending transfer body stays native owned");
        auto post=publish(*registry,0x22000u,7,17,
            std::make_shared<Body>(6,events));
        auto found=registry->find_first_live(0x22000u,rf);
        check(found && *found==post && retained_body.lock()->calls==0 &&
              transfer_observed->calls==1,
              "post-transfer same-root lookup/publication works without replay");

        bool foreign_refused=false;
        std::thread worker([&] {
            RetailProfileMenuFailure foreign;
            foreign_refused=!retire_retail_profile_tasks_from_12edc0(
                main,0x11004u,foreign) &&
                foreign.kind==RetailProfileMenuError::main_lifetime;
        });
        worker.join();
        check(foreign_refused,"foreign thread cannot mutate Main task root");

        auto ordinary_body=std::make_shared<Body>(7,events);
        ordinary_body->ordinary_error=true;
        (void)publish(*registry,0x33000u,8,18,ordinary_body);
        check(!retire_retail_profile_tasks_from_12edc0(main,0x33000u,mf) &&
              mf.session.task_domain.registry.kind==
                  RetailTaskRegistryError::native_exception &&
              ordinary_body->calls==1,
              "ordinary cleanup failure is named after its source effect");
        check(!retire_retail_profile_tasks_from_12edc0(main,0x33000u,mf) &&
              mf.session.task_domain.registry.kind==
                  RetailTaskRegistryError::after_effect_unknown &&
              ordinary_body->calls==1,
              "unknown partial cleanup cannot replay on retry");
    }
    std::error_code error;
    std::filesystem::remove(root,error);
    check(!error,"remove only owned disposable fixture directory");
    std::cout<<"native retail selective task retirement fixture passed\n";
}
