#include "mka/retail_input_admission.hpp"
#include "mka/retail_input_poll.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <stdexcept>
#include <thread>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
using namespace mka::retail;
namespace {bool fail_next_allocation=false;}
void* operator new(std::size_t n){if(fail_next_allocation){fail_next_allocation=false;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
namespace {
void check(bool b,const char* label){if(!b)throw std::runtime_error(label);}
struct Body final:NativeRetailTaskBody {
    std::function<void()> on_cleanup;
    NativeTaskStep run() override{return NativeTaskStep::returned(1);}
    void cleanup(bool) override{if(on_cleanup)on_cleanup();}
};
struct Fixture {
    mka::input::ControllerInput samples;
    NativeRetailStartupState startup;
    NativeRetailMainState main;
    RetailGameSessionFailure sf;RetailInputRoleFailure rf;
    RetailParticipantFailure pf;RetailInputAssignmentFailure af;
    RetailTaskDomainFailure tf;RetailTaskRegistryFailure registry_failure;
    RetailInputAdmissionFailure f;
    Fixture(const std::filesystem::path& path):startup(open(path),{}),main(startup,samples){
        StartupFailure failure;check(initialize_native_retail_game_session(startup,failure),"actual Session");
        main.input_roles=NativeRetailInputRoles::create(rf);
        main.participants=NativeRetailParticipants::create_first_two(pf);
        main.input_assignments=NativeRetailInputAssignments::create(main.input_roles,main.participants,af);
        check(startup.game_session->store_scalar_mode_from_source(7,sf),"fixture source scalar7 fixes participant identity");
        main.mode_history.emplace();check(bool(main.input_assignments),"actual shared assignment domain");
    }
    static mka::content::NativeContentVolume open(const std::filesystem::path& p){
        mka::content::ContentVolumeFailure f;auto v=mka::content::NativeContentVolume::open(p,f);check(bool(v),"native content owner");return std::move(*v);
    }
    std::shared_ptr<NativeRetailInputParticipant> participant(unsigned i){return main.participants->first_two(i,pf);}
    void roots(){
        RetailGamePoolFailure f;std::shared_ptr<NativeRetailGamePool> pool(NativeRetailGamePool::initialize(1,f));
        check(pool&&startup.game_session->task_domain(sf)->reset_task_roots_from_12f0d0(pool,tf),"actual task registry born");
    }
    NativeRetailTaskRegistry* registry(){return startup.game_session->task_domain(sf)->registry(tf);}
    void ready(RetailInputRole role){
        // Explicit fixture producer: run existing assigned-ready leaf, then
        // real detach. This is not Windows connection/route acceptance.
        auto p=participant(static_cast<unsigned>(role));
        check(main.input_assignments->bind_direct(p,role,af),"fixture participant bind");
        mka::input::NativeControllerAxes axes;
        check(samples.publish(7,mka::input::digital_state(0,true,{{128,128,128,128}},255,255,axes)),"fixture full host sample");
        RetailAssignedPollFailure f;
        auto controls=startup.game_session->controls(sf);
        auto poll=NativeRetailAssignedPoll::create(main.input_roles,controls,samples,f);
        check(poll&&poll->ready_assigned(*main.input_roles,*controls,samples,role,7,RetailAnalogDeadzone{48},f),"existing source ready producer");
        check(main.input_assignments->detach(p,af),"fixture detach keeps initialized role");
    }
    bool admit(RetailInputRole r){auto v=admit_retail_physical_role_from_280c50(main,r,f);check(v.has_value(),f.operation);return *v;}
    int count(){return *main.input_assignments->association_count(af);}
};
}
int main(int argc,char** argv){try{
    check(argc==2,"existing content directory required");const std::filesystem::path path(argv[1]);
    Fixture allocation(path);fail_next_allocation=true;
    check(!admit_retail_physical_role_from_280c50(allocation.main,RetailInputRole::role0,allocation.f)&&
          allocation.f.kind==RetailInputAdmissionError::allocation&&!allocation.main.input_admission&&allocation.count()==0,
          "admission owner allocation failure publishes no state");
    Fixture x(path);
    check(!x.admit(RetailInputRole::role0)&&x.count()==0,"uninitialized source role refuses before unborn registry");
    x.ready(RetailInputRole::role0);
    x.ready(RetailInputRole::role1);
    check(!admit_retail_physical_role_from_280c50(x.main,RetailInputRole::role0,x.f)&&x.f.kind==RetailInputAdmissionError::tasks,"born-empty registry is not accepted implicitly");
    x.roots();
    check(x.admit(RetailInputRole::role0)&&x.count()==1,"actual selector count and first binding");
    auto p0=x.participant(0);auto p1=x.participant(1);
    check(p0->observe(x.pf)->assigned_role==0,"participant observes actual role");
    check(x.admit(RetailInputRole::role0)&&x.count()==1,"same source reassignment detaches then increments, no duplicate count");
    check(x.admit(RetailInputRole::role1)&&x.count()==2&&p1->observe(x.pf)->assigned_role==1,"second physical candidate uses retained count");
    constexpr int contexts[]{0,2,3,8,11,12,-1};
    constexpr int modes[]{0,8,9,17,7};
    // Independent truth rows from original280CB8..280D28/280DAC..280E1C.
    constexpr bool allowed[7][5]{{true,true,true,true,true},{true,true,true,true,true},
        {true,true,true,true,true},{true,true,true,true,true},{true,false,false,false,true},
        {false,false,false,false,false},{false,false,false,false,false}};
    for(unsigned ci=0;ci<7;++ci)for(unsigned mi=0;mi<5;++mi){
        x.main.mode_history->clear();RetailModeFailure mf;check(x.main.mode_history->push(contexts[ci],mf),"fixture context producer");
        check(x.startup.game_session->store_scalar_mode_from_source(modes[mi],x.sf),"actual scalar writer");
        check(x.admit(RetailInputRole::role0)==allowed[ci][mi],"source context/mode truth rows");
    }
    x.main.mode_history->clear();check(x.startup.game_session->store_scalar_mode_from_source(7,x.sf),"fixture scalar7 restored");
    check(p0->set_game_state(3,x.pf),"actual game state writer");
    check(!x.admit(RetailInputRole::role0),"source participant game-state3 refuses");
    check(p0->set_game_state(0,x.pf),"actual state restored");
    auto body=std::make_shared<Body>();auto* registry=x.registry();
    auto task=registry->publish_task(0x2065,4,55,body,x.registry_failure);check(bool(task),"real warning-kind fixture body published");
    const auto count=x.count();check(!x.admit(RetailInputRole::role0)&&x.count()==count,"actual task query suppresses admission before selection/count");
    bool recursive=false;body->on_cleanup=[&]{RetailInputAdmissionFailure nested;auto v=admit_retail_physical_role_from_280c50(x.main,RetailInputRole::role0,nested);recursive=!v&&nested.kind==RetailInputAdmissionError::reentrant;};
    check(registry->store_object_cookie_from_source(task,1,x.registry_failure),"source stale-cookie producer");
    check(x.admit(RetailInputRole::role0)&&recursive,"effectful query cleanup refuses recursive admission then outer proceeds");
    body=std::make_shared<Body>();body->on_cleanup=[] {throw detail::NativeMainReentry{nullptr,7};};
    task=registry->publish_task(0x2066,4,56,body,x.registry_failure);check(bool(task),"second actual query task");
    check(registry->store_object_cookie_from_source(task,1,x.registry_failure),"second stale task");
    bool transferred=false;try{x.admit(RetailInputRole::role0);}catch(const detail::NativeMainReentry& e){transferred=e.value==7;}
    check(transferred&&x.count()==count,"cleanup Main transfer propagates before assignment effects");
    check(x.admit(RetailInputRole::role0),"admission guard released after Main transfer");
    Fixture cleanup_failure(path);cleanup_failure.ready(RetailInputRole::role0);cleanup_failure.roots();
    auto throwing=std::make_shared<Body>();throwing->on_cleanup=[]{throw std::runtime_error("fixture cleanup");};
    auto bad_task=cleanup_failure.registry()->publish_task(99,4,77,throwing,cleanup_failure.registry_failure);
    check(bad_task&&cleanup_failure.registry()->store_object_cookie_from_source(bad_task,1,cleanup_failure.registry_failure),"actual stale fixture for exception");
    auto bad=admit_retail_physical_role_from_280c50(cleanup_failure.main,RetailInputRole::role0,cleanup_failure.f);
    check(!bad&&cleanup_failure.f.kind==RetailInputAdmissionError::native_exception&&cleanup_failure.count()==0,"cleanup exception named before assignment");
    Fixture revoked(path);revoked.ready(RetailInputRole::role0);revoked.roots();
    auto revoke=std::make_shared<Body>();revoke->on_cleanup=[&]{revoked.main.lifetime->retire();};
    auto revoked_task=revoked.registry()->publish_task(98,4,78,revoke,revoked.registry_failure);
    check(revoked_task&&revoked.registry()->store_object_cookie_from_source(revoked_task,1,revoked.registry_failure),"actual stale fixture for lifetime revocation");
    auto revoked_result=admit_retail_physical_role_from_280c50(revoked.main,RetailInputRole::role0,revoked.f);
    check(!revoked_result&&revoked.f.kind==RetailInputAdmissionError::owner&&revoked.count()==0,"cleanup revocation stops before selection/count");
    bool wrong_thread=false;std::thread t([&]{RetailInputAdmissionFailure f;wrong_thread=!admit_retail_physical_role_from_280c50(x.main,RetailInputRole::role0,f)&&f.kind==RetailInputAdmissionError::owner;});t.join();check(wrong_thread,"wrong thread refused before native mutation");
    Fixture other(path);other.main.input_admission=x.main.input_admission;
    check(!admit_retail_physical_role_from_280c50(other.main,RetailInputRole::role0,other.f)&&other.f.kind==RetailInputAdmissionError::owner,"foreign Main domain refused");
    Fixture foreign(path);foreign.main.input_assignments=x.main.input_assignments;
    check(!admit_retail_physical_role_from_280c50(foreign.main,RetailInputRole::role0,foreign.f)&&
          foreign.f.kind==RetailInputAdmissionError::assignment&&x.count()==count,
          "foreign assignment service refused before mutating its count");
    x.main.lifetime->retire();check(!admit_retail_physical_role_from_280c50(x.main,RetailInputRole::role0,x.f),"retired Main lifetime refused");
    std::puts("PASS_NATIVE_PHYSICAL_INPUT_ADMISSION_SOURCE280C50_ONLY");return 0;
}catch(const std::exception& e){std::fprintf(stderr,"input admission: %s\n",e.what());return 1;}}
