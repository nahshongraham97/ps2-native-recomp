#include "mka/matfx_binding.hpp"
#include <exception>
#include <new>
namespace mka::game {
namespace {
bool fail(MatFxBindingFailure& f,MatFxBindingError k,const char* op) noexcept {f={k,op};return false;}
struct Guard {bool& busy;explicit Guard(bool& value) noexcept:busy(value){busy=true;}~Guard(){busy=false;}};
bool valid(const MatFxImportedState& state) noexcept {
 constexpr std::array<std::array<MatFxEffectKind,2>,7> kinds{{
  {MatFxEffectKind::none,MatFxEffectKind::none},{MatFxEffectKind::effect1,MatFxEffectKind::none},
  {MatFxEffectKind::effect2,MatFxEffectKind::none},{MatFxEffectKind::effect1,MatFxEffectKind::effect2},
  {MatFxEffectKind::property,MatFxEffectKind::none},{MatFxEffectKind::parameter_pair,MatFxEffectKind::none},
  {MatFxEffectKind::parameter_pair,MatFxEffectKind::property}}};
 const auto mode=static_cast<std::uint32_t>(state.mode);if(mode>=kinds.size())return false;
 for(std::size_t i=0;i<2;++i){const auto& s=state.slots[i];if(s.kind!=kinds[mode][i])return false;
  if(s.kind!=MatFxEffectKind::parameter_pair&&(!s.pair.first.is_null()||!s.pair.second.is_null()))return false;
  if(s.kind!=MatFxEffectKind::property&&(s.selectors.first||s.selectors.second))return false;
  if(s.primary&&s.kind!=MatFxEffectKind::effect1&&s.kind!=MatFxEffectKind::property)return false;
  if(s.secondary&&s.kind!=MatFxEffectKind::effect1&&s.kind!=MatFxEffectKind::effect2)return false;
 }return true;
}
}
NativeMatFxProperty::~NativeMatFxProperty() noexcept=default;
NativeMatFxBinding::NativeMatFxBinding(MatFxImportedState&& state) noexcept:mode_(state.mode),slots_(std::move(state.slots)){}
std::unique_ptr<NativeMatFxBinding> NativeMatFxBinding::create(MatFxImportedState&& state,MatFxBindingFailure& f) noexcept {
 f={};if(!valid(state)){fail(f,MatFxBindingError::invalid_state,"matfx import mode/slot ownership mismatch");return {};}
 try{return std::unique_ptr<NativeMatFxBinding>(new NativeMatFxBinding(std::move(state)));}
 catch(const std::bad_alloc&){fail(f,MatFxBindingError::allocation,"matfx owned binding allocation");return {};}
}
NativeMatFxBinding::~NativeMatFxBinding(){if(owner_!=std::this_thread::get_id()||busy_)std::terminate();busy_=true;for(auto& s:slots_){s.primary.reset();s.secondary.reset();}}
bool NativeMatFxBinding::access(MatFxBindingFailure& f) const noexcept {
 f={};if(owner_!=std::this_thread::get_id())return fail(f,MatFxBindingError::wrong_thread,"matfx owner thread");
 if(busy_)return fail(f,MatFxBindingError::reentry,"matfx resource finalizer reentry");return true;
}
MatFxEffectSlot* NativeMatFxBinding::find(MatFxEffectKind k) noexcept {for(auto& s:slots_)if(s.kind==k)return &s;return nullptr;}
const MatFxEffectSlot* NativeMatFxBinding::find(MatFxEffectKind k) const noexcept {for(const auto& s:slots_)if(s.kind==k)return &s;return nullptr;}
bool NativeMatFxBinding::lease(const MatFxParameterPair& refs,MatFxParameterPairLease& out,MatFxBindingFailure& f) const noexcept {
 if(!refs.first.is_null()){out.first=refs.first.matrix_.lock();if(!out.first)return fail(f,MatFxBindingError::stale_matrix,"matfx first borrowed matrix expired");}
 if(!refs.second.is_null()){out.second=refs.second.matrix_.lock();if(!out.second)return fail(f,MatFxBindingError::stale_matrix,"matfx second borrowed matrix expired");}return true;
}
std::optional<MatFxBindingSnapshot> NativeMatFxBinding::snapshot(MatFxBindingFailure& f) const noexcept {
 if(!access(f))return {};Guard guard(busy_);return MatFxBindingSnapshot{mode_,slots_};
}
bool NativeMatFxBinding::prepare_direct_tracking(MatFxBindingFailure& f) noexcept {
 if(!access(f))return false;Guard guard(busy_);return prepare_tracking_owned(true,f);
}
bool NativeMatFxBinding::prepare_tracking_owned(bool preserve_property,MatFxBindingFailure& f) noexcept {
 if(mode_==MatFxMode::parameter_pair)return true; // Equal nonzero mode does not reset source slots.
 std::array<MatFxEffectSlot,2> staged;staged[0].kind=MatFxEffectKind::parameter_pair;
 const auto next=preserve_property&&mode_==MatFxMode::property?MatFxMode::parameter_pair_property:MatFxMode::parameter_pair;
 if(preserve_property&&mode_==MatFxMode::property){const auto* old=find(MatFxEffectKind::property);if(!old)return fail(f,MatFxBindingError::missing_slot,"matfx missing source property slot");
  if(!old->primary)return fail(f,MatFxBindingError::missing_property,"matfx source conversion missing owned property");
  staged[1].kind=MatFxEffectKind::property;staged[1].primary=old->primary;staged[1].selectors=old->selectors;}
 // Staged strong property lifetime is an explicit safe native extension across
 // source reset/replacement. Displaced resources finalize while guard is active.
 slots_.swap(staged);mode_=next;return true;
}
bool NativeMatFxBinding::prepare_hierarchy_tracking(MatFxHierarchyRoute route,MatFxBindingFailure& f) noexcept {
 if(!access(f))return false;Guard guard(busy_);
 if(route!=MatFxHierarchyRoute::plugin_absent&&route!=MatFxHierarchyRoute::plugin_present)
  return fail(f,MatFxBindingError::invalid_state,"matfx unknown hierarchy plugin route");
 if(route==MatFxHierarchyRoute::plugin_present)return prepare_tracking_owned(false,f);
 if(mode_==MatFxMode::disabled||mode_==MatFxMode::property)return prepare_tracking_owned(true,f);
 return true; // Source absent-plugin branch leaves all other modes unchanged.
}
std::optional<MatFxParameterPairLease> NativeMatFxBinding::first_parameter_pair(MatFxBindingFailure& f) const noexcept {
 if(!access(f))return {};Guard guard(busy_);const auto* slot=find(MatFxEffectKind::parameter_pair);
 if(!slot){fail(f,MatFxBindingError::missing_slot,"matfx missing kind5 parameter pair");return {};}
 MatFxParameterPairLease held;if(!lease(slot->pair,held,f))return {};return held;
}
bool NativeMatFxBinding::set_first_parameter_pair(const MatFxParameterPair& pair,MatFxBindingFailure& f) noexcept {
 if(!access(f))return false;Guard guard(busy_);auto* slot=find(MatFxEffectKind::parameter_pair);
 if(!slot)return fail(f,MatFxBindingError::missing_slot,"matfx missing kind5 parameter pair");
 MatFxParameterPairLease held;if(!lease(pair,held,f))return false;slot->pair=pair;return true;
}
std::optional<std::shared_ptr<NativeMatFxProperty>> NativeMatFxBinding::first_property(MatFxBindingFailure& f) const noexcept {
 if(!access(f))return {};Guard guard(busy_);const auto* slot=find(MatFxEffectKind::property);
 if(!slot){fail(f,MatFxBindingError::missing_slot,"matfx missing kind4 property");return {};}
 return slot->primary;
}
std::optional<MatFxSelectors> NativeMatFxBinding::property_selectors(MatFxBindingFailure& f) const noexcept {
 if(!access(f))return {};Guard guard(busy_);const auto* slot=find(MatFxEffectKind::property);
 if(!slot){fail(f,MatFxBindingError::missing_slot,"matfx missing kind4 selectors");return {};}return slot->selectors;
}
bool NativeMatFxBinding::replace_property(const std::shared_ptr<NativeMatFxProperty>& value,MatFxBindingFailure& f) noexcept {
 if(!access(f))return false;Guard guard(busy_);auto* slot=find(MatFxEffectKind::property);
 if(!slot)return fail(f,MatFxBindingError::missing_slot,"matfx missing kind4 property");
 if(!value)return fail(f,MatFxBindingError::missing_property,"matfx replacement requires owned property");
 auto displaced=value;slot->primary.swap(displaced);displaced.reset();return true;
}
bool NativeMatFxBinding::set_property_selectors(MatFxSelectors value,MatFxBindingFailure& f) noexcept {
 if(!access(f))return false;Guard guard(busy_);auto* slot=find(MatFxEffectKind::property);
 if(!slot)return fail(f,MatFxBindingError::missing_slot,"matfx missing kind4 selectors");slot->selectors=value;return true;
}
} // namespace mka::game
