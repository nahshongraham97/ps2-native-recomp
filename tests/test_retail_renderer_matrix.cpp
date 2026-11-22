#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <thread>
#include <type_traits>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {thread_local bool fail_new=false;thread_local int fail_after=-1;}
void* operator new(std::size_t n){if(fail_new)throw std::bad_alloc();if(fail_after==0){fail_after=-1;throw std::bad_alloc();}if(fail_after>0)--fail_after;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool b,const char* what){if(!b)throw std::runtime_error(what);}
unsigned default_calls{},custom_calls{},classify_calls{},inverse_calls{};
RetailRendererMatrixTolerances observed_tolerances;
const RetailRendererAffine* observed_left{};const RetailRendererAffine* observed_right{};
NativeRetailMainState* transfer_main{};bool throw_native{};
auto bits(const RetailRendererMatrixTolerances& t){static_assert(sizeof(t)==12);return std::bit_cast<std::array<std::uint32_t,3>>(t);}
bool custom_product(RetailRendererAffine& out,const RetailRendererAffine& left,const RetailRendererAffine& right,RetailRendererMatrixFailure&){
 ++custom_calls;observed_left=&left;observed_right=&right;
 if(transfer_main)reenter_native_retail_main(*transfer_main,29);
 if(throw_native)throw std::runtime_error("explicit native math fixture failure");
 std::memmove(&out,&left,sizeof(out));return true; // Fixture code,not claimed as19F680 arithmetic.
}
struct Fixture {
 NativeRetailStartupState startup;mka::input::ControllerInput samples;NativeRetailMainState main;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator;std::shared_ptr<NativeRetailRendererCore> core;
 RetailRendererCoreFailure cf;StartupFailure sf;
 explicit Fixture(const std::filesystem::path& root):startup(open(root),{}),main(startup,samples){
  check(initialize_native_retail_game_session(startup,sf),"actual Session");check(initialize_retail_memory_owners(startup,sf),"actual memory group");
  RetailRendererAllocatorFailure af;allocator=NativeRetailRendererAllocatorService::create(*startup.memory_owners,af);check(bool(allocator),"actual Permanent capability");
  core=NativeRetailRendererCore::for_session(startup.game_session,allocator,startup.content,cf);check(bool(core),"actual persistent renderer Core");
  check(bool(core->register_factory({0x402,24},{renderer_required::construct_core_plugin_0402,renderer_required::destroy_core_plugin_0402},cf)),"actual0402 factory bound in real registry");
 }
 static mka::content::NativeContentVolume open(const std::filesystem::path& p){mka::content::ContentVolumeFailure f;auto v=mka::content::NativeContentVolume::open(p,f);check(bool(v),"native existing root");return std::move(*v);}
};
}
namespace mka::retail::renderer_required {
bool construct_core_plugin_0401(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0401(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0403(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0403(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0404(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0404(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0405(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0405(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0406(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0406(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0407(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0407(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0408(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0408(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0409(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0409(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_0412(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_0412(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_040a(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_040a(NativeRetailRendererCore&)noexcept{std::abort();}
bool construct_core_plugin_040b(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void destroy_core_plugin_040b(NativeRetailRendererCore&)noexcept{std::abort();}
std::optional<bool> register_object_extensions_1b72b0(NativeRetailRendererCore&,RetailRendererCoreFailure&){std::abort();}
void discard_object_extensions_after_core_init(NativeRetailRendererCore&)noexcept{std::abort();}
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine& left,const RetailRendererAffine& right,RetailRendererMatrixFailure& f){
 ++default_calls;observed_left=&left;observed_right=&right;f={RetailRendererMatrixError::numeric_policy_unbound,"test-only unresolved default19F680"};return false;
}
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances& t,RetailRendererMatrixFailure& f){
 ++classify_calls;std::memcpy(&observed_tolerances,&t,sizeof(t));f={RetailRendererMatrixError::numeric_policy_unbound,"test-only unresolved1A0240"};return false;
}
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure& f){
 ++inverse_calls;f={RetailRendererMatrixError::numeric_policy_unbound,"test-only unresolved nonidentity1A07A0"};return false;
}
}
static_assert(!std::is_default_constructible_v<NativeRetailRendererMatrixDomain>);
static_assert(!std::is_copy_constructible_v<NativeRetailRendererMatrixDomain>);
static_assert(!std::is_move_constructible_v<NativeRetailRendererMatrixDomain>);
int main(int argc,char** argv)try{
 check(argc==2,"existing CPU-only content root required");Fixture x(argv[1]);RetailRendererMatrixFailure f;
 check(!x.core->matrix_domain(x.cf)&&*x.core->matrix_domain_count(x.cf)==0,"actual module BSS count/missing owner");
 RetailMemoryOwnerFailure memory;const auto heap_before=x.startup.memory_owners->observe(RetailMemoryRole::permanent,memory)->heap.live_allocations;
 check(x.core->construct_registered(x.cf),"real registry constructs complete0402 owner/settings");auto module=x.core->matrix_domain(x.cf);
 check(x.startup.memory_owners->observe(RetailMemoryRole::permanent,memory)->heap.live_allocations==heap_before+1,"actual domain storage belongs to existing Permanent owner");
 check(module&&*x.core->matrix_domain_count(x.cf)==1,"actual0402 module count published after construction");
 check(bits(*module->tolerances(f))==std::array<std::uint32_t,3>{0x3c23d70au,0x3c23d70au,0x3c23d70au},"allthree original5A3600 defaults exactly copied");
 check(default_calls==0&&classify_calls==0&&inverse_calls==0,"registration/construction does not execute missing arithmetic");
 check(!renderer_required::construct_core_plugin_0402(*x.core,x.cf)&&x.cf.kind==RetailRendererCoreError::reentrant&&*x.core->matrix_domain_count(x.cf)==1,"native duplicate live-domain construction preserves ownership");
 const auto input=std::bit_cast<RetailRendererMatrixTolerances>(std::array<std::uint32_t,3>{0x80000000u,0x7f800000u,0x7fc01234u});
 check(module->set_tolerances(input,f)&&bits(*module->tolerances(f))==bits(input),"1A0210 exact data copies,not numeric validation;unusualbits are fixtures only");
 RetailRendererMatrixValue value{};value.properties=3;
 check(!module->classify(value,std::nullopt,f)&&f.kind==RetailRendererMatrixError::numeric_policy_unbound&&bits(observed_tolerances)==bits(input),"1A0240 null tolerance consumes current actual module settings");
 const RetailRendererMatrixTolerances explicit_values{1,2,3};
 check(!module->classify(value,explicit_values,f)&&bits(observed_tolerances)==bits(explicit_values)&&bits(*module->tolerances(f))==bits(input),"explicit tolerance overrides only invocation");
 RetailRendererAffine left{{1,2,3},{4,5,6},{7,8,9},{10,11,12}},right{{13,14,15},{16,17,18},{19,20,21},{22,23,24}},out{};
 check(!module->multiply(out,left,right,f)&&f.kind==RetailRendererMatrixError::numeric_policy_unbound&&default_calls==1,"actual required default is not guessed float math");
 check(module->set_product(custom_product,f)&&module->multiply(out,left,right,f)&&out==left&&observed_left==&left&&observed_right==&right,"19FFA0 stores actual typed native code;dispatch preserves operand order");
 check(module->set_product(nullptr,f)&&!module->multiply(out,left,right,f)&&default_calls==2,"null restores actual required default reference");
 check(module->set_product(custom_product,f),"actual custom native product restored");
 RetailRendererMatrixValue copy{left,0x20000u|0x4000u},destination{};
 check(module->inverse(destination,copy,f)&&std::memcmp(&destination,&copy,sizeof(copy))==0&&inverse_calls==0,"proved source propertymask selects pure typed bitcopy");
 check(module->inverse(copy,copy,f)&&inverse_calls==0,"identity shortcut supports same-object alias without arithmetic");
 copy.properties=3;check(!module->inverse(destination,copy,f)&&f.kind==RetailRendererMatrixError::numeric_policy_unbound&&inverse_calls==1,"orthonormal numeric path stays required,not guessed transpose/float dots");
 copy.properties=0;check(!module->inverse(destination,copy,f)&&inverse_calls==2,"general inverse including singular policy stays required");
 copy.properties=0x20000;for(unsigned i=0;i<300;++i)check(module->inverse(destination,copy,f),"native operations do not add a256 slot policy");
 transfer_main=&x.main;bool transferred=false;try{module->multiply(out,left,right,f);}catch(const detail::NativeMainReentry& e){transferred=e.owner==&x.main&&e.value==29;}transfer_main=nullptr;
 check(transferred&&module->tolerances(f).has_value(),"legitimate Main transfer propagates without poisoning native owner");
 throw_native=true;check(!module->multiply(out,left,right,f)&&f.kind==RetailRendererMatrixError::after_effect_unknown,"native kernel exception names possible partial caller output");throw_native=false;
 bool wrong=false;std::thread thread([&]{RetailRendererMatrixFailure bad;wrong=!module->set_product(nullptr,bad)&&bad.kind==RetailRendererMatrixError::wrong_thread;});thread.join();check(wrong,"foreign thread rejected before module mutation");
 check(x.core->destroy_constructed(x.cf)&&*x.core->matrix_domain_count(x.cf)==0&&!x.core->matrix_domain(x.cf),"dtor clears actual Core owner and source count");
 check(!module->tolerances(f)&&f.kind==RetailRendererMatrixError::retired&&!module->multiply(out,left,right,f),"external native owner retires immediately with real factory teardown");check(x.startup.memory_owners->observe(RetailMemoryRole::permanent,memory)->heap.live_allocations==heap_before+1,"retired external native capability retains safe storage,not live game access");module.reset();check(x.startup.memory_owners->observe(RetailMemoryRole::permanent,memory)->heap.live_allocations==heap_before,"last typed capability releases actual Permanent storage");
 fail_new=true;const bool failed=!x.core->construct_registered(x.cf);fail_new=false;check(failed&&x.cf.kind==RetailRendererCoreError::allocation&&*x.core->matrix_domain_count(x.cf)==0&&!x.core->matrix_domain(x.cf),"native allocation failure publishes no partial owner/count");
 unsigned failed_cuts=0,completed_cuts=0;for(int cut=0;cut<8;++cut){fail_after=cut;const bool constructed=x.core->construct_registered(x.cf);fail_after=-1;if(constructed){++completed_cuts;check(x.core->destroy_constructed(x.cf),"successful allocation-cut probe retires real owner");}else{++failed_cuts;check(x.cf.kind==RetailRendererCoreError::allocation,"actual native allocation failure remains named");}check(*x.core->matrix_domain_count(x.cf)==0&&!x.core->matrix_domain(x.cf)&&x.startup.memory_owners->observe(RetailMemoryRole::permanent,memory)->heap.live_allocations==heap_before,"every native creation fault cut preserves counter/owner/Permanent allocation balance");}check(failed_cuts>=2&&completed_cuts>0,"both storage/control-block failure cuts plus normal native construction reached");
 check(x.core->construct_registered(x.cf),"real registry retry after failed native allocation");module=x.core->matrix_domain(x.cf);check(bits(*module->tolerances(f))==std::array<std::uint32_t,3>{0x3c23d70au,0x3c23d70au,0x3c23d70au},"recreation resets actual defaults,not previous retired settings");
 std::weak_ptr<NativeRetailGameSession> lifetime=x.startup.game_session;x.startup.game_record_reset.reset();x.startup.game_session.reset();check(lifetime.expired(),"all actual Session owners retired");
 check(!module->tolerances(f)&&f.kind==RetailRendererMatrixError::owner,"retained domain refuses after actual Session lifetime");
 std::puts("PASS_NATIVE0402_MATRIX_OWNER_DEFAULTS_SETTER_DISPATCH_IDENTITYCOPY_LIFETIME; exact math callbacks REQUIRED;no renderer/GPU acceptance");return 0;
}catch(const std::exception& e){fail_new=false;std::fprintf(stderr,"0402 matrix: %s\n",e.what());return 1;}
