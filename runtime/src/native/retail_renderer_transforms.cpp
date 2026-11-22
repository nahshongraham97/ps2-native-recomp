#include "mka/retail_renderer_transforms.hpp"
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_game_session.hpp"
#include "retail_main_control.hpp"
namespace mka::retail {
namespace {
bool fail(RetailRendererCoreFailure& f,RetailRendererCoreError kind,const char* op)noexcept{f.kind=kind;f.operation=op;return false;}
RetailRendererTransformCallbacks defaults()noexcept{using namespace renderer_required;return {transform_point_exact,transform_points_exact,transform_vector_exact,transform_vectors_exact};}
struct Busy {bool& value;explicit Busy(bool& b):value(b){value=true;}~Busy(){value=false;}};
}
NativeRetailRendererTransforms::NativeRetailRendererTransforms(std::weak_ptr<NativeRetailGameSession> s):session_(std::move(s)),callbacks_(defaults()){}
std::shared_ptr<NativeRetailGameSession> NativeRetailRendererTransforms::access(RetailRendererCoreFailure& f)const noexcept{
 f={};if(thread_!=std::this_thread::get_id()){fail(f,RetailRendererCoreError::wrong_thread,"0401 transform owner thread");return {};}
 auto session=session_.lock();RetailGameSessionFailure sf;
 // Check the weak Session first. Its retained identity prevents Core teardown
 // racing a live invocation; an expired Session never reads retired Core state.
 if(!session||!session->validate_current_thread(sf)||!active_){fail(f,RetailRendererCoreError::owner,"0401 retired transform/Session");return {};}
 if(after_effect_unknown_){fail(f,RetailRendererCoreError::after_effect_unknown,"0401 native transform partial effect");return {};}
 if(busy_){fail(f,RetailRendererCoreError::reentrant,"0401 transform callback active");return {};}
 return session;
}
bool NativeRetailRendererTransforms::set_callbacks(RetailRendererTransformCallbacks c,RetailRendererCoreFailure& f)noexcept{
 const auto session=access(f);if(!session)return false;const auto d=defaults();
 callbacks_={c.point?c.point:d.point,c.points?c.points:d.points,c.vector?c.vector:d.vector,c.vectors?c.vectors:d.vectors};return true;
}
bool NativeRetailRendererTransforms::one(bool vector,RetailRendererPoint3& out,const RetailRendererPoint3& in,const RetailRendererAffine& matrix,RetailRendererCoreFailure& f){
 const auto self=weak_from_this().lock();const auto session=access(f);if(!self||!session)return false;
 const auto callback=vector?callbacks_.vector:callbacks_.point;Busy busy(busy_);
 try{callback(out,in,matrix);}catch(const detail::NativeMainReentry&){throw;}
 catch(...){after_effect_unknown_=true;return fail(f,RetailRendererCoreError::after_effect_unknown,"0401 native transform exception");}
 if(!active_)return fail(f,RetailRendererCoreError::owner,"0401 retired during transform");return true;
}
bool NativeRetailRendererTransforms::many(bool vector,std::span<RetailRendererPoint3> out,std::span<const RetailRendererPoint3> in,const RetailRendererAffine& matrix,RetailRendererCoreFailure& f){
 const auto self=weak_from_this().lock();const auto session=access(f);if(!self||!session)return false;
 if(out.size()<in.size())return fail(f,RetailRendererCoreError::construction,"0401 transform destination extent");
 const auto callback=vector?callbacks_.vectors:callbacks_.points;Busy busy(busy_);
 // Preserve actual aliases and order; even zero-count invokes the callback.
 try{callback(out.first(in.size()),in,matrix);}catch(const detail::NativeMainReentry&){throw;}
 catch(...){after_effect_unknown_=true;return fail(f,RetailRendererCoreError::after_effect_unknown,"0401 native transforms exception");}
 if(!active_)return fail(f,RetailRendererCoreError::owner,"0401 retired during transforms");return true;
}
bool NativeRetailRendererTransforms::point(RetailRendererPoint3& o,const RetailRendererPoint3& i,const RetailRendererAffine& m,RetailRendererCoreFailure& f){return one(false,o,i,m,f);}
bool NativeRetailRendererTransforms::vector(RetailRendererPoint3& o,const RetailRendererPoint3& i,const RetailRendererAffine& m,RetailRendererCoreFailure& f){return one(true,o,i,m,f);}
bool NativeRetailRendererTransforms::points(std::span<RetailRendererPoint3> o,std::span<const RetailRendererPoint3> i,const RetailRendererAffine& m,RetailRendererCoreFailure& f){return many(false,o,i,m,f);}
bool NativeRetailRendererTransforms::vectors(std::span<RetailRendererPoint3> o,std::span<const RetailRendererPoint3> i,const RetailRendererAffine& m,RetailRendererCoreFailure& f){return many(true,o,i,m,f);}
}
