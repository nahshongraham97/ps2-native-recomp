#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <bit>
#include <cstring>
#include <new>
#include <type_traits>
namespace mka::retail {
namespace {
bool fail(RetailRendererMatrixFailure& f,RetailRendererMatrixError k,const char* op)noexcept{f.kind=k;f.operation=op;return false;}
RetailRendererMatrixTolerances source_defaults()noexcept{
 const float value=std::bit_cast<float>(std::uint32_t{0x3c23d70a});return {value,value,value};
}
template<class Call>bool invoke_math(Call call,RetailRendererMatrixFailure& f){
 try{return call();}catch(const detail::NativeMainReentry&){throw;}
 catch(...){return fail(f,RetailRendererMatrixError::after_effect_unknown,"0402 native math exception;caller output may be partial");}
}
}
NativeRetailRendererMatrixDomain::NativeRetailRendererMatrixDomain(
 std::weak_ptr<NativeRetailGameSession> session,std::shared_ptr<NativeRetailRendererAllocatorService> allocator)noexcept
 :session_(std::move(session)),allocator_(std::move(allocator)),tolerances_(source_defaults()){}
std::shared_ptr<NativeRetailRendererMatrixDomain> NativeRetailRendererMatrixDomain::create(
 std::weak_ptr<NativeRetailGameSession> session,std::shared_ptr<NativeRetailRendererAllocatorService> allocator,RetailRendererMatrixFailure& f)noexcept{
 f={};const auto owner=session.lock();RetailGameSessionFailure sf;
 if(!owner||!owner->validate_current_thread(sf)||!allocator){fail(f,RetailRendererMatrixError::owner,"0402 actual Session/Permanent ownership");return {};}
 static_assert(alignof(NativeRetailRendererMatrixDomain)<=16);
 auto storage=allocator->allocate(sizeof(NativeRetailRendererMatrixDomain),f.allocator);
 if(!storage){fail(f,RetailRendererMatrixError::allocator,"0402 native typed matrix-domain allocation");return {};}
 try{
  auto* object=new(storage->data())NativeRetailRendererMatrixDomain(session,allocator);
  return std::shared_ptr<NativeRetailRendererMatrixDomain>(object,[allocator,storage=std::move(*storage)](NativeRetailRendererMatrixDomain* p)mutable{p->~NativeRetailRendererMatrixDomain();});
 }catch(const std::bad_alloc&){fail(f,RetailRendererMatrixError::allocation,"0402 matrix-domain native control ownership");}
 catch(...){fail(f,RetailRendererMatrixError::native_exception,"0402 matrix-domain native construction");}
 return {};
}
std::shared_ptr<NativeRetailGameSession> NativeRetailRendererMatrixDomain::admit(RetailRendererMatrixFailure& f)const noexcept{
 f={};if(std::this_thread::get_id()!=thread_){fail(f,RetailRendererMatrixError::wrong_thread,"0402 matrix-domain owner thread");return {};}
 if(!active_.load(std::memory_order_acquire)){fail(f,RetailRendererMatrixError::retired,"0402 retired matrix domain");return {};}
 auto owner=session_.lock();RetailGameSessionFailure sf;
 if(!owner||!owner->validate_current_thread(sf)){fail(f,RetailRendererMatrixError::owner,"0402 actual Session lifetime");return {};}
 return owner; // Retained for the WHOLE settings/math operation.
}
std::optional<RetailRendererMatrixTolerances> NativeRetailRendererMatrixDomain::tolerances(RetailRendererMatrixFailure& f)const noexcept{
 const auto session=admit(f);if(!session)return {};
 RetailRendererMatrixTolerances result;std::memcpy(&result,&tolerances_,sizeof(result));return result;
}
bool NativeRetailRendererMatrixDomain::set_tolerances(const RetailRendererMatrixTolerances& input,RetailRendererMatrixFailure& f)noexcept{
 const auto session=admit(f);if(!session)return false;
 //1A0210 performs bit-preserving loads/stores,no validation/clamp/arithmetic.
 std::memmove(&tolerances_,&input,sizeof(input));return true;
}
bool NativeRetailRendererMatrixDomain::set_product(RetailRendererMatrixProduct product,RetailRendererMatrixFailure& f)noexcept{
 const auto session=admit(f);if(!session)return false;
 product_=product?product:renderer_required::multiply_affines_19f680;return true;
}
bool NativeRetailRendererMatrixDomain::multiply(RetailRendererAffine& output,const RetailRendererAffine& left,
 const RetailRendererAffine& right,RetailRendererMatrixFailure& f)const{
 const auto session=admit(f);if(!session)return false;const auto product=product_;
 return invoke_math([&]{return product(output,left,right,f);},f);
}
bool NativeRetailRendererMatrixDomain::classify(RetailRendererMatrixValue& matrix,
 const std::optional<RetailRendererMatrixTolerances>& explicit_tolerances,RetailRendererMatrixFailure& f)const{
 const auto session=admit(f);if(!session)return false;
 RetailRendererMatrixTolerances selected;const auto* values=explicit_tolerances?&*explicit_tolerances:&tolerances_;
 std::memcpy(&selected,values,sizeof(selected));
 return invoke_math([&]{return renderer_required::classify_matrix_1a0240(matrix,selected,f);},f);
}
bool NativeRetailRendererMatrixDomain::inverse(RetailRendererMatrixValue& output,
 const RetailRendererMatrixValue& input,RetailRendererMatrixFailure& f)const{
 const auto session=admit(f);if(!session)return false;
 if(input.properties&identity_shortcut_mask){
  static_assert(std::is_trivially_copyable_v<RetailRendererMatrixValue>);
  //1A07D4..800 copies without arithmetic when the source property authorizes
  // the shortcut. No claim is made that arbitrary flagged input is identity.
  std::memmove(&output,&input,sizeof(output));return true;
 }
 return invoke_math([&]{return renderer_required::invert_nonidentity_matrix_1a07a0(output,input,f);},f);
}
std::shared_ptr<NativeRetailRendererMatrixDomain> NativeRetailRendererCore::matrix_domain(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};if(!matrix_domain_){f.kind=RetailRendererCoreError::unbound_factory;f.operation="0402 matrix domain is not constructed";return {};}
 return matrix_domain_;
}
std::optional<std::uint32_t> NativeRetailRendererCore::matrix_domain_count(RetailRendererCoreFailure& f)const noexcept{
 if(!access(f))return {};return matrix_domain_count_.load(std::memory_order_relaxed);
}
namespace renderer_required {
bool construct_core_plugin_0402(NativeRetailRendererCore& core,RetailRendererCoreFailure& f){
 if(!core.access(f))return false;
 if(core.matrix_domain_){f.kind=RetailRendererCoreError::reentrant;f.operation="0402 already has its actual matrix domain";return false;}
 RetailRendererMatrixFailure detail;auto domain=NativeRetailRendererMatrixDomain::create(core.session_,core.allocator_,detail);
 if(!domain){
  f.kind=detail.kind==RetailRendererMatrixError::allocation||detail.allocator.heap.kind==memory::NativeHeapError::allocation_failed?RetailRendererCoreError::allocation:
   detail.kind==RetailRendererMatrixError::native_exception?RetailRendererCoreError::native_exception:RetailRendererCoreError::allocator;
  f.operation=detail.operation;f.allocator=detail.allocator;return false;
 }
 core.matrix_domain_=std::move(domain);core.matrix_domain_count_.fetch_add(1,std::memory_order_relaxed);return true;
}
void destroy_core_plugin_0402(NativeRetailRendererCore& core)noexcept{
 if(core.matrix_domain_){core.matrix_domain_->retire();core.matrix_domain_.reset();}
 core.matrix_domain_count_.fetch_sub(1,std::memory_order_relaxed); // Source ADDIU wrap;valid caller destroys once.
}
}
}
