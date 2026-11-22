#pragma once
#include "mka/retail_renderer_math.hpp"
#include <memory>
#include <span>
#include <thread>
namespace mka::retail {
class NativeRetailGameSession;class NativeRetailRendererCore;
struct RetailRendererCoreFailure;
using RetailRendererTransformOne=void(*)(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&);
using RetailRendererTransformMany=void(*)(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&);
struct RetailRendererTransformCallbacks {
 RetailRendererTransformOne point{};RetailRendererTransformMany points{};
 RetailRendererTransformOne vector{};RetailRendererTransformMany vectors{};
};
namespace renderer_required {
// Exact native mathematical kernels. The EE accumulator rounding is not
// replaced by ordinary IEEE arithmetic. No guest register/device interface.
void transform_point_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&);
void transform_points_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&);
void transform_vector_exact(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&);
void transform_vectors_exact(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&);
}
class NativeRetailRendererTransforms final:public std::enable_shared_from_this<NativeRetailRendererTransforms> {
public:
 NativeRetailRendererTransforms(const NativeRetailRendererTransforms&)=delete;
 NativeRetailRendererTransforms& operator=(const NativeRetailRendererTransforms&)=delete;
 NativeRetailRendererTransforms(NativeRetailRendererTransforms&&)=delete;
 NativeRetailRendererTransforms& operator=(NativeRetailRendererTransforms&&)=delete;
 // Complete1A3880 selection: each absent function independently selects its
 // source default. No context pointer, numeric dispatch or borrowed capture.
 bool set_callbacks(RetailRendererTransformCallbacks,RetailRendererCoreFailure&)noexcept;
 bool point(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&,RetailRendererCoreFailure&);
 bool points(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&,RetailRendererCoreFailure&);
 bool vector(RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&,RetailRendererCoreFailure&);
 bool vectors(std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&,RetailRendererCoreFailure&);
private:
 friend class NativeRetailRendererCore;
 explicit NativeRetailRendererTransforms(std::weak_ptr<NativeRetailGameSession>);
 void retire()noexcept{active_=false;}
 std::shared_ptr<NativeRetailGameSession> access(RetailRendererCoreFailure&)const noexcept;
 bool one(bool,RetailRendererPoint3&,const RetailRendererPoint3&,const RetailRendererAffine&,RetailRendererCoreFailure&);
 bool many(bool,std::span<RetailRendererPoint3>,std::span<const RetailRendererPoint3>,const RetailRendererAffine&,RetailRendererCoreFailure&);
 std::weak_ptr<NativeRetailGameSession> session_;
 RetailRendererTransformCallbacks callbacks_;
 const std::thread::id thread_{std::this_thread::get_id()};
 bool active_{true},busy_{},after_effect_unknown_{};
};
}
