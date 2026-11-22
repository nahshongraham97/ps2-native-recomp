#pragma once
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_renderer_math.hpp"
#include <atomic>
#include <array>
#include <memory>
#include <optional>
#include <thread>
namespace mka::retail {
// Actual game matrix properties are separate from mathematical values. No
// source padding,object byte offsets or guest extension block is represented.
struct RetailRendererMatrixValue {RetailRendererAffine affine;std::uint32_t properties{};};
struct RetailRendererMatrixTolerances {
 float basis_length_error_squared{},orthogonality_error_squared{},identity_error_squared{};
};
enum class RetailRendererMatrixError {none,owner,wrong_thread,retired,allocation,allocator,native_exception,after_effect_unknown,numeric_policy_unbound};
struct RetailRendererMatrixFailure {
 RetailRendererMatrixError kind{RetailRendererMatrixError::none};const char* operation{""};RetailRendererAllocatorFailure allocator;
};
using RetailRendererMatrixProduct=bool(*)(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&);
namespace renderer_required {
// Exact EE accumulator/domain contracts remain unimplemented native math,
// not a separately-rounded float fallback or a guest FPU implementation.
bool multiply_affines_19f680(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&);
bool classify_matrix_1a0240(RetailRendererMatrixValue&,const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&);
bool invert_nonidentity_matrix_1a07a0(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&);
}
class NativeRetailRendererMatrixDomain final {
public:
 NativeRetailRendererMatrixDomain(const NativeRetailRendererMatrixDomain&)=delete;
 NativeRetailRendererMatrixDomain& operator=(const NativeRetailRendererMatrixDomain&)=delete;
 NativeRetailRendererMatrixDomain(NativeRetailRendererMatrixDomain&&)=delete;
 NativeRetailRendererMatrixDomain& operator=(NativeRetailRendererMatrixDomain&&)=delete;
 static constexpr std::uint32_t identity_shortcut_mask=0x20000u;
 std::optional<RetailRendererMatrixTolerances> tolerances(RetailRendererMatrixFailure&)const noexcept;
 bool set_tolerances(const RetailRendererMatrixTolerances&,RetailRendererMatrixFailure&)noexcept;
 bool set_product(RetailRendererMatrixProduct,RetailRendererMatrixFailure&)noexcept;
 // Snapshot native code reference;nullptr setter restores actual required
 // default. No source address or unknown function token is accepted.
 bool multiply(RetailRendererAffine&,const RetailRendererAffine&,const RetailRendererAffine&,RetailRendererMatrixFailure&)const;
 bool classify(RetailRendererMatrixValue&,const std::optional<RetailRendererMatrixTolerances>&,RetailRendererMatrixFailure&)const;
 bool inverse(RetailRendererMatrixValue&,const RetailRendererMatrixValue&,RetailRendererMatrixFailure&)const;
private:
 friend bool renderer_required::construct_core_plugin_0402(NativeRetailRendererCore&,RetailRendererCoreFailure&);
 friend void renderer_required::destroy_core_plugin_0402(NativeRetailRendererCore&)noexcept;
 NativeRetailRendererMatrixDomain(std::weak_ptr<NativeRetailGameSession>,std::shared_ptr<NativeRetailRendererAllocatorService>)noexcept;
 static std::shared_ptr<NativeRetailRendererMatrixDomain> create(std::weak_ptr<NativeRetailGameSession>,std::shared_ptr<NativeRetailRendererAllocatorService>,RetailRendererMatrixFailure&)noexcept;
 std::shared_ptr<NativeRetailGameSession> admit(RetailRendererMatrixFailure&)const noexcept;
 void retire()noexcept{active_.store(false,std::memory_order_release);}
 std::weak_ptr<NativeRetailGameSession> session_;
 std::shared_ptr<NativeRetailRendererAllocatorService> allocator_;
 RetailRendererMatrixTolerances tolerances_;
 RetailRendererMatrixProduct product_{renderer_required::multiply_affines_19f680};
 std::atomic<bool> active_{true};
 const std::thread::id thread_{std::this_thread::get_id()};
};
}
