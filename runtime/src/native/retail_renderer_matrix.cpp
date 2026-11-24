#include "mka/retail_renderer_matrix.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
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

__attribute__((weak)) bool multiply_affines_19f680(RetailRendererAffine& out, const RetailRendererAffine& left,
    const RetailRendererAffine& right, RetailRendererMatrixFailure& f) {
    f = {};
    // Option C (Partitioned): Native host IEEE 754 float math matching Xbox 0x002be460 (RwMatrixMultiply)
    const float r_x = left.right.x * right.right.x + left.right.y * right.up.x + left.right.z * right.at.x;
    const float r_y = left.right.x * right.right.y + left.right.y * right.up.y + left.right.z * right.at.y;
    const float r_z = left.right.x * right.right.z + left.right.y * right.up.z + left.right.z * right.at.z;

    const float u_x = left.up.x * right.right.x + left.up.y * right.up.x + left.up.z * right.at.x;
    const float u_y = left.up.x * right.right.y + left.up.y * right.up.y + left.up.z * right.at.y;
    const float u_z = left.up.x * right.right.z + left.up.y * right.up.z + left.up.z * right.at.z;

    const float a_x = left.at.x * right.right.x + left.at.y * right.up.x + left.at.z * right.at.x;
    const float a_y = left.at.x * right.right.y + left.at.y * right.up.y + left.at.z * right.at.y;
    const float a_z = left.at.x * right.right.z + left.at.y * right.up.z + left.at.z * right.at.z;

    const float t_x = left.translation.x * right.right.x + left.translation.y * right.up.x + left.translation.z * right.at.x + right.translation.x;
    const float t_y = left.translation.x * right.right.y + left.translation.y * right.up.y + left.translation.z * right.at.y + right.translation.y;
    const float t_z = left.translation.x * right.right.z + left.translation.y * right.up.z + left.translation.z * right.at.z + right.translation.z;

    out.right = {r_x, r_y, r_z};
    out.up = {u_x, u_y, u_z};
    out.at = {a_x, a_y, a_z};
    out.translation = {t_x, t_y, t_z};
    return true;
}

__attribute__((weak)) bool classify_matrix_1a0240(RetailRendererMatrixValue& matrix, const RetailRendererMatrixTolerances& t, RetailRendererMatrixFailure& f) {
    f = {};
    // Option C: Native host math matching Xbox 0x002be150 tolerance classification
    const auto& m = matrix.affine;
    const float len_r2 = m.right.x * m.right.x + m.right.y * m.right.y + m.right.z * m.right.z;
    const float len_u2 = m.up.x * m.up.x + m.up.y * m.up.y + m.up.z * m.up.z;
    const float len_a2 = m.at.x * m.at.x + m.at.y * m.at.y + m.at.z * m.at.z;

    const float dot_ru = m.right.x * m.up.x + m.right.y * m.up.y + m.right.z * m.up.z;
    const float dot_ra = m.right.x * m.at.x + m.right.y * m.at.y + m.right.z * m.at.z;
    const float dot_ua = m.up.x * m.at.x + m.up.y * m.at.y + m.up.z * m.at.z;

    const float basis_err = std::max({std::abs(len_r2 - 1.0f), std::abs(len_u2 - 1.0f), std::abs(len_a2 - 1.0f)});
    const float ortho_err = std::max({std::abs(dot_ru), std::abs(dot_ra), std::abs(dot_ua)});

    std::uint32_t flags = 0;
    if (basis_err <= t.basis_length_error_squared && ortho_err <= t.orthogonality_error_squared) {
        flags |= 0x01; // Orthonormal
    }

    const float id_err = (m.right.x - 1.0f)*(m.right.x - 1.0f) + m.right.y*m.right.y + m.right.z*m.right.z
                       + m.up.x*m.up.x + (m.up.y - 1.0f)*(m.up.y - 1.0f) + m.up.z*m.up.z
                       + m.at.x*m.at.x + m.at.y*m.at.y + (m.at.z - 1.0f)*(m.at.z - 1.0f)
                       + m.translation.x*m.translation.x + m.translation.y*m.translation.y + m.translation.z*m.translation.z;
    if (id_err <= t.identity_error_squared) {
        flags |= NativeRetailRendererMatrixDomain::identity_shortcut_mask;
    }

    matrix.properties = flags;
    return true;
}

__attribute__((weak)) bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue& out, const RetailRendererMatrixValue& in, RetailRendererMatrixFailure& f) {
    f = {};
    // Option C: Native host math matching Xbox 0x002be560 (RwMatrixInvert)
    const auto& m = in.affine;

    const float det = m.right.x * (m.up.y * m.at.z - m.up.z * m.at.y)
                    - m.right.y * (m.up.x * m.at.z - m.up.z * m.at.x)
                    + m.right.z * (m.up.x * m.at.y - m.up.y * m.at.x);

    if (std::abs(det) < 1e-12f) {
        f.kind = RetailRendererMatrixError::numeric_policy_unbound;
        f.operation = "singular matrix cannot be inverted";
        return false;
    }

    const float inv_det = 1.0f / det;

    RetailRendererAffine inv;
    inv.right.x =  (m.up.y * m.at.z - m.up.z * m.at.y) * inv_det;
    inv.right.y = -(m.right.y * m.at.z - m.right.z * m.at.y) * inv_det;
    inv.right.z =  (m.right.y * m.up.z - m.right.z * m.up.y) * inv_det;

    inv.up.x    = -(m.up.x * m.at.z - m.up.z * m.at.x) * inv_det;
    inv.up.y    =  (m.right.x * m.at.z - m.right.z * m.at.x) * inv_det;
    inv.up.z    = -(m.right.x * m.up.z - m.right.z * m.up.x) * inv_det;

    inv.at.x    =  (m.up.x * m.at.y - m.up.y * m.at.x) * inv_det;
    inv.at.y    = -(m.right.x * m.at.y - m.right.y * m.at.x) * inv_det;
    inv.at.z    =  (m.right.x * m.up.y - m.right.y * m.up.x) * inv_det;

    inv.translation.x = -(m.translation.x * inv.right.x + m.translation.y * inv.up.x + m.translation.z * inv.at.x);
    inv.translation.y = -(m.translation.x * inv.right.y + m.translation.y * inv.up.y + m.translation.z * inv.at.y);
    inv.translation.z = -(m.translation.x * inv.right.z + m.translation.y * inv.up.z + m.translation.z * inv.at.z);

    out.affine = inv;
    out.properties = in.properties & ~NativeRetailRendererMatrixDomain::identity_shortcut_mask;
    return true;
}
}
}
