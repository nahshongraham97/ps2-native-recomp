#include "mka/retail_saved_profile_child.hpp"
#include <utility>
namespace mka::retail {
namespace {
bool fail(RetailSavedProfileChildFailure& f,RetailSavedProfileChildError k,const char* op)noexcept{f.kind=k;f.operation=op;return false;}
void zero_known(RetailSavedProfileDefaults& k)noexcept{
 // The existing known-default structs intentionally have nonzero default
 // members. Those are NOT source BSS birth values.
 auto& g=k.gameplay;
 g.bitword_1164=g.bitword_1168=g.bitword_1188=g.bitword_118c=g.bitword_1190=0;
 g.bitword_1194=g.bitword_11a4=g.bitword_11ac=g.bitword_11b0=0;
 auto& c=k.command;
 c.bitword_1fc=c.bitword_264=c.bitword_268=c.bitword_2d0=c.bitword_2d4=c.bitword_2e0=c.flag_bit2=0;
}
}
bool RetailSavedKafProgress::primary(std::uint32_t id,std::uint32_t aggregate)const noexcept{
 if(id>=352)return false;
 if(primary_.contains(id))return true;
 return (id==13||id==14)&&(aggregate&(std::uint32_t{1}<<(id&31)))!=0;
}
bool RetailSavedKafProgress::secondary(std::uint32_t id)const noexcept{
 if(id>=192)return false;
 return id>=160?primary_.contains(id-160):secondary_only_.contains(id);
}
bool RetailSavedKafProgress::store_primary(std::uint32_t id,bool set){
 if(id>=352)return false;if(set)primary_.insert(id);else primary_.erase(id);return true;
}
bool RetailSavedKafProgress::store_secondary(std::uint32_t id,bool set){
 if(id>=192)return false;if(id>=160)return store_primary(id-160,set);
 if(set)secondary_only_.insert(id);else secondary_only_.erase(id);return true;
}
RetailSavedProfileChildValue::RetailSavedProfileChildValue()noexcept{zero_known(known_);}
bool RetailSavedProfileChildValue::reset_from_245cb0(std::int32_t language,std::uint32_t& working,RetailSavedProfileChildFailure& f)noexcept{
 f={};
 // Replace the VALUE at the stable outer node,not the node itself. Only saved
 // immutable data is dropped; no live actor/scene/file retirement is performed.
 *this=RetailSavedProfileChildValue{};
 phase_=RetailSavedChildPhase::resetting;
 if(!initialize_retail_saved_profile_known_defaults_from_245cb0(known_,language,working,f.defaults)){
  phase_=RetailSavedChildPhase::failed_reset;
  return fail(f,RetailSavedProfileChildError::known_defaults,f.defaults.operation);
 }
 phase_=RetailSavedChildPhase::source_defaults;return true;
}
bool RetailSavedProfileChildValue::reset_child_body_from_245dd0(std::int32_t language,std::uint32_t& working,RetailSavedProfileChildFailure& f)noexcept{
 return reset_from_245cb0(language,working,f);
}
bool RetailSavedProfileChildValue::import_from_image(const NativeRetailProfileImage&,std::size_t index,RetailSavedProfileChildFailure& f)noexcept{
 f={};
 if(index>=retail_profile_child_count)return fail(f,RetailSavedProfileChildError::invalid_child_index,"saved child external image index");
 // A checksum-valid encoded file is not a typed runtime child. In particular
 // zero flagbit0 is not a whole-default discriminator. Do not inspect/mirror
 // an unknown record or promote previous/default data as the imported value.
 phase_=RetailSavedChildPhase::import_blocked;
 return fail(f,RetailSavedProfileChildError::loaded_import_required,"required complete typed saved-child import/field decoders");
}
bool RetailSavedProfileChildValue::observable(RetailSavedProfileChildFailure& f)const noexcept{
 f={};if(phase_==RetailSavedChildPhase::zero_born||phase_==RetailSavedChildPhase::source_defaults)return true;
 return fail(f,RetailSavedProfileChildError::not_observable,"saved child not admitted as BSS/reset data");
}
std::optional<RetailProfileAggregateChildView> RetailSavedProfileChildValue::aggregate_view(RetailSavedProfileChildFailure& f)const noexcept{
 if(!observable(f))return {};
 RetailProfileAggregateChildView out;
 out.gate_word_3c28=identity_.aggregate_gate;out.or_word_fc=unlock_word_;out.or_words_100_108_110_118=unlock_masks_;
 for(std::size_t i=0;i<out.and_words_120.size();++i)out.and_words_120[i]=static_cast<std::uint32_t>(known_.first_unassigned_indices[i]);
 for(std::size_t i=0;i<out.and_words_1c0.size();++i)out.and_words_1c0[i]=static_cast<std::uint32_t>(known_.second_unassigned_indices[i]);
 return out;
}
}
