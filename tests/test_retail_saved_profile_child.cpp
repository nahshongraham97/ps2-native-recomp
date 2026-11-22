#include "mka/retail_saved_profile_child.hpp"
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <type_traits>
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {bool fail_new{};}
void* operator new(std::size_t n){if(fail_new)throw std::bad_alloc();if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}void operator delete(void* p,std::size_t)noexcept{std::free(p);}
using namespace mka::retail;
namespace {
void check(bool b,const char* op){if(!b)throw std::runtime_error(op);}
void bss(const RetailSavedProfileChildValue& x){
 check(x.phase()==RetailSavedChildPhase::zero_born,"explicit zero-BSS phase");
 const auto& k=x.known_values();
 check(!k.initialized&&k.empty_name.empty(),"BSS name/initialized distinct from EMPTY/default");
 check(k.gameplay.bitword_1164==0&&k.gameplay.bitword_1168==0&&k.gameplay.bitword_1188==0&&k.gameplay.bitword_1194==0,"BSS gameplay is not struct's nonzero reset defaults");
 check(k.command.bitword_1fc==0&&k.command.flag_bit2==0&&k.command.bitword_2e0==0,"BSS customization values");
 for(auto v:k.first_action_masks)check(v==0,"BSS action map");
 for(auto v:k.first_unassigned_indices)check(v==0,"BSS first index words");
 for(auto v:k.second_unassigned_indices)check(v==0,"BSS second index words");
 RetailSavedProfileChildFailure f;auto view=x.aggregate_view(f);
 check(view&&view->gate_word_3c28==0&&view->or_word_fc==0,"BSS actual aggregate projection");
 for(auto v:view->and_words_120)check(v==0,"BSS projection is zero,not reset FFFFFFFF");
}
void defaults(const RetailSavedProfileChildValue& x){
 check(x.phase()==RetailSavedChildPhase::source_defaults,"source reset phase");
 const auto& k=x.known_values();
 check(k.initialized&&k.gameplay.bitword_1164==18&&k.gameplay.bitword_1168==1&&k.gameplay.bitword_1188==4&&k.gameplay.bitword_1194==0xffffffffu,"original final nonzero gameplay defaults");
 for(unsigned i=0;i<16;++i)check(k.first_action_masks[i]==(1u<<i)&&k.second_action_masks[i]==(1u<<i),"two source mask table copies");
 RetailSavedProfileChildFailure f;auto view=x.aggregate_view(f);check(bool(view),"default aggregate is admitted");
 check(view->gate_word_3c28==0&&view->or_word_fc==0&&x.identity().signature==0,"gate/signature are child fields,not parent in-use or command1FC");
 for(auto v:view->or_words_100_108_110_118)check(v==0,"actual default masks zero");
 for(auto v:view->and_words_120)check(v==0xffffffffu,"first source final -1 indices");
 for(auto v:view->and_words_1c0)check(v==0xffffffffu,"second source final -1 indices");
 check(x.named_presets().empty()&&x.gameplay().additional.object_snapshots.empty()&&!x.gameplay().additional.preview_definition&&x.gameplay().additional.resource_ids.empty(),"proved empty saved domains,not live actor/scene retirement");
 check(&x.gameplay().source_values==&x.known_values().gameplay&&&x.customization().source_values==&x.known_values().command,"semantic projections share actual field authorities");
 check(x.gameplay().source_values.bitword_1164==18&&x.gameplay().source_values.bitword_1188==4&&x.customization().source_values.bitword_1fc==1,"semantic projections expose nonzero defaults");
 check(x.customization_flags()==4&&x.customization_condition_word()==0,"custom bit0 absent,bit2 present,unconditional condition word0");
 check(x.wallet().current==0&&x.wallet().accumulated==0&&x.gameplay().additional.control40==0&&x.gameplay().additional.event_count==0,"retained scalar defaults");
 for(const auto& pair:x.statistics().mode_pairs)check(pair.first==0&&pair.second==0,"three counter pairs reset");
 for(const auto& result:x.statistics().timed_results)check(!result.present()&&result.packed_clock==0,"five packed results genuinely absent");
 for(auto index:x.gameplay().additional.category_indices)check(index==0,"eight live category indices default0");
}
}
int main()try{
 static_assert(std::is_nothrow_move_assignable_v<RetailSavedProfileChildValue>);
 RetailSavedProfileChildFailure f;auto first=RetailSavedProfileChildValue::zero_born();auto second=RetailSavedProfileChildValue::zero_born();
 bss(first);bss(second);std::uint32_t working=0xdeadbeef;
 check(first.reset_from_245cb0(0,working,f)&&working==0,"nonnull reset clears the actual supplied Working field");defaults(first);bss(second);
 check(first.known_values().empty_name=="EMPTY","actual language0 name");
 // The second untouched parent/child must stay BSS across the first callback.
 check(second.known_values().gameplay.bitword_1164==0&&first.known_values().gameplay.bitword_1164==18,"mixed first-callback child state");
 working=91;check(second.reset_child_body_from_245dd0(5,working,f)&&working==0,"direct245DD0 inline child body reuse");defaults(second);
 check(second.known_values().empty_name=="EMPTY"&&second.known_values().command.labels==first.known_values().command.labels,"inline default equivalence and language5->0");
 auto copied=first;defaults(copied);check(copied.aggregate_view(f)->and_words_120==first.aggregate_view(f)->and_words_120,"typed value copy preserves admitted projection");
 check(first.reset_from_245cb0(-1,working,f)&&first.known_values().empty_name==" ","source out-of-language fallback");
 RetailSavedKafProgress progress;
 for(auto aggregate:{0u,1u<<13,1u<<14,0xffffffffu})for(unsigned id:{0u,7u,12u,13u,14u,15u,159u,160u,191u,192u,351u,352u}){
  const bool expected=(id==13||id==14)&&bool(aggregate&(1u<<(id&31)));
  check(progress.primary(id,aggregate)==expected,"empty flags preserve actual aggregate fallback13/14");
 }
 check(progress.store_primary(13,true)&&progress.secondary(173),"shared primary13/secondary173 is one flag");
 check(progress.store_secondary(173,false)&&!progress.secondary(173)&&!progress.primary(13,0)&&progress.primary(13,1u<<13),"shared clear does not erase external aggregate");
 check(progress.store_secondary(159,true)&&progress.secondary(159)&&!progress.primary(159,0),"secondary-only field is distinct");
 check(progress.store_primary(351,true)&&progress.primary(351,0)&&!progress.secondary(351),"primary high namespace");
 check(!progress.store_primary(352,true)&&!progress.store_secondary(192,true),"source ID bounds,not host resource ceiling");progress.clear();check(!progress.primary(351,0)&&!progress.secondary(159),"real flag reset");
 // Encoded external file fixture only. Never retained as runtime child state.
 std::vector<std::uint8_t> prefix(retail_profile_image_prefix_size),encoded_child(retail_profile_child_image_size),tail(retail_profile_image_tail_size);
 RetailProfileImageEncodedParts parts{prefix,{},tail};for(auto& row:parts.children)row=encoded_child;
 RetailProfileImageFailure image_failure;auto image=NativeRetailProfileImage::encode_card_write_222da0(parts,RetailProfileImageFooterMode::ordinary_checksum,image_failure);check(bool(image),"actual existing image codec");
 check(!first.import_from_image(*image,8,f)&&f.kind==RetailSavedProfileChildError::invalid_child_index&&first.phase()==RetailSavedChildPhase::source_defaults,"invalid file index is distinct and cannot mutate value");
 check(!first.import_from_image(*image,0,f)&&f.kind==RetailSavedProfileChildError::loaded_import_required&&first.phase()==RetailSavedChildPhase::import_blocked&&!first.aggregate_view(f),"checksum-valid zero file is not a ready/default runtime record");
 working=73;check(first.reset_from_245cb0(0,working,f)&&working==0,"real reset recovers blocked import without guessing loaded fields");defaults(first);
 working=123;fail_new=true;const bool reset=first.reset_from_245cb0(0,working,f);fail_new=false;
 check(!reset&&f.kind==RetailSavedProfileChildError::known_defaults&&f.defaults.kind==RetailSavedProfileDefaultError::allocation&&first.phase()==RetailSavedChildPhase::failed_reset,"host allocation failure never publishes source-ready child");
 check(working==0&&first.known_values().first_unassigned_indices[0]==0&&!first.aggregate_view(f),"ordered failure after shared KAF clear, before final -1 fill");
 check(first.reset_from_245cb0(0,working,f),"subsequent actual source reset");defaults(first);
 std::puts("PASS_TYPED_CHILD_ZERO_BSS_AND_SOURCE_RESET_DEFAULTS;loaded import refused;no parent/game/endpoint acceptance");return 0;
}catch(const std::exception& e){fail_new=false;std::fprintf(stderr,"saved child: %s\n",e.what());return 1;}
