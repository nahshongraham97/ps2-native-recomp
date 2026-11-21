#include "mka/retail_pad_control.hpp"
#include "mka/retail_input_task_argument.hpp"
#include <array>
#include <cstdio>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct CpuBody final:NativeRetailTaskBody {
    NativeTaskStep run() override {return NativeTaskStep::returned(1.0f);}
    void cleanup(bool) override {++cleanup_calls;}
    unsigned cleanup_calls{};
};
struct CleanupFlagWriter final:NativeRetailObservedObject {
    std::uint32_t cookie{0x1234};
    std::shared_ptr<NativeRetailGameControls> controls;
    unsigned cleanup_calls{};
    std::uint32_t source_cookie() const noexcept override {return cookie;}
    void cleanup_from_12fa00() override {
        ++cleanup_calls;
        if(controls) {
            RetailGameControlsFailure f;
            check(controls->store_analog_input_inhibited_from_source(false,f),"real cleanup changes live control");
        }
        cookie=0;
    }
};
}
int main() {try {
    RetailGameSessionFailure sf;auto session=NativeRetailGameSession::create(sf);
    check(bool(session),"actual native game Session");
    auto controls=session->controls(sf);auto* domain=session->task_domain(sf);
    RetailInputRoleFailure role_failure;auto roles=NativeRetailInputRoles::create(role_failure);
    check(controls&&domain&&roles,"one real control/domain/role set");
    RetailGameRecordResetFailure f;RetailGameControlsFailure cf;
    check(suspend_retail_input_from_163ba0(*session,*domain,*roles,f)&&
          *controls->analog_input_inhibited(cf),"source first-birth suspend before task registry");
    check(resume_retail_input_from_163a50(*session,*domain,*roles,f)&&
          !*controls->analog_input_inhibited(cf),"source first-birth resume before task registry");
    RetailTaskDomainFailure df;RetailGamePoolFailure pf;
    std::shared_ptr<NativeRetailGamePool> pool(NativeRetailGamePool::initialize(4,pf));
    check(pool&&domain->seed_counter_from_185ee0(df)&&domain->reset_task_roots_from_12f0d0(pool,df),
          "source-ordered one pool/task registry birth");
    auto* registry=domain->registry(df);check(registry!=nullptr,"same live task authority");
    RetailTaskRegistryFailure rf;RetailObserverListFailure of;RetailInputTaskArgumentFailure af;
    const std::array kinds{RetailInputTaskArgumentKind::task_2065,RetailInputTaskArgumentKind::task_2066,
                           RetailInputTaskArgumentKind::task_208b};
    std::array<std::shared_ptr<NativeRetailInputTaskArgument>,3> arguments;
    std::array<std::shared_ptr<NativeRetailObserverList>,3> lists;
    std::array<std::shared_ptr<CpuBody>,3> bodies;
    auto writer=std::make_shared<CleanupFlagWriter>();writer->controls=controls;
    for(std::size_t i=0;i<kinds.size();++i) {
        // This is a CPU capability fixture, not a substituted retail body.
        // Shared native cookie policy matches130D00's counter/override logic;
        // this fixture does not execute the actual130D00 allocator/producer.
        const auto argument_cookie=domain->next_cookie_from_130a50(df);
        check(argument_cookie.has_value(),"shared native source-cookie policy value");
        arguments[i]=NativeRetailInputTaskArgument::create(kinds[i],*argument_cookie,af);
        lists[i]=std::make_shared<NativeRetailObserverList>(pool);
        check(arguments[i]&&arguments[i]->store_role_from_source(RetailInputRole::role0,af)&&
              arguments[i]->store_restore_analog_inhibited_from_source(true,af)&&
              lists[i]->insert_owned_argument_from_12fee0(arguments[i],of),"real owning task argument attachment");
        if(i==0) {
            check(bool(lists[i]->insert_owned_argument_from_12fee0(writer,of)),"owning stale object before argument");
            writer->cookie=1; // Cached identity differs;130860 writes1 before12FA00.
        }
        if(i==1) check(arguments[i]->retire_from_12df60(af),"source-released argument leaves a stale observer");
        bodies[i]=std::make_shared<CpuBody>();
        auto cleanup_list=std::make_shared<NativeRetailObserverList>(pool);
        const auto task_cookie=domain->next_cookie_from_130a50(df);
        check(task_cookie&&registry->publish_task(static_cast<std::uint32_t>(kinds[i]),i==2?0x20:4,*task_cookie,
                  bodies[i],lists[i],cleanup_list,rf),"actual registry admits CPU test body with both owned roots");
    }
    check(suspend_retail_input_from_163ba0(*session,*domain,*roles,f)&&writer->cleanup_calls==1&&
          !*controls->analog_input_inhibited(cf),"lookup cleanup changes live flag after suspend publication");
    check(arguments[0]->observe(af)->restore_analog_inhibited==false&&
          arguments[2]->observe(af)->restore_analog_inhibited==false,
          "280810/280760 reload actual bit6 after getter effects, not an earlier snapshot");
    const auto middle=registry->find_first_live(0x2066,rf);
    check(middle&&*middle,"middle task remains live after its argument expires");
    const auto absent_argument=registry->input_argument_from_12e930(*middle,rf);
    check(absent_argument&&!*absent_argument,"legitimate null argument getter skips its store");
    check(suspend_retail_input_from_163ba0(*session,*domain,*roles,f)&&
          arguments[0]->observe(af)->restore_analog_inhibited==true&&
          arguments[2]->observe(af)->restore_analog_inhibited==true,
          "later suspend updates both remaining actual arguments");
    check(arguments[0]->store_restore_analog_inhibited_from_source(false,af)&&
          suspend_retail_input_from_163ba0(*session,*domain,*roles,f)&&
          arguments[0]->observe(af)->restore_analog_inhibited==false,
          "already-set163BA0 branch does not replay argument writes");
    check(resume_retail_input_from_163a50(*session,*domain,*roles,f)&&
          !*controls->analog_input_inhibited(cf)&&
          arguments[2]->observe(af)->restore_analog_inhibited==false,"resume writes the same live native arguments");
    auto unsupported=std::make_shared<CleanupFlagWriter>();
    auto unsupported_link=lists[2]->insert_owned_argument_from_12fee0(unsupported,of);
    check(unsupported_link&&!resume_retail_input_from_163a50(*session,*domain,*roles,f)&&
          f.registry.kind==RetailTaskRegistryError::unsupported_source_branch,
          "unknown reached virtual getter is named, never reinterpreted or fabricated null");
    check(lists[2]->retire_link_from_12fa00(unsupported_link,of)&&
          resume_retail_input_from_163a50(*session,*domain,*roles,f),"actual cleanup permits a later known source path");
    auto foreign=NativeRetailGameSession::create(sf);auto* foreign_domain=foreign->task_domain(sf);
    check(controls->store_analog_input_inhibited_from_source(true,cf)&&
          !resume_retail_input_from_163a50(*session,*foreign_domain,*roles,f)&&
          f.kind==RetailGameRecordResetError::foreign_domain&&*controls->analog_input_inhibited(cf),
          "foreign task domain fails before control effects");
    bool wrong_thread=false;
    std::thread worker([&] {RetailGameRecordResetFailure failure;
        wrong_thread=!resume_retail_input_from_163a50(*session,*domain,*roles,failure)&&
            failure.kind==RetailGameRecordResetError::wrong_thread;
    });worker.join();
    check(wrong_thread&&*controls->analog_input_inhibited(cf),"wrong-thread reset preserves source state");
    check(domain->cancel_first_root_from_12f030(df),"actual source task and owning-argument cleanup");
    for(const auto& body:bodies) check(body->cleanup_calls==1,"real callback capability retired exactly once");
    check(arguments[0]->observe(af)->retired&&arguments[2]->observe(af)->retired&&
          pool->observe(pf)->active_records==0,"owning arguments invalidate/release with the shared native pool");
    std::puts("PASS native pad control and live task arguments; CPU test bodies, physical input/game still open");
    return 0;
}catch(const std::exception& error) {std::fprintf(stderr,"FAIL native pad control: %s\n",error.what());return 1;}}
