#include "mka/retail_main.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_current_sound_required.hpp"
#include "mka/retail_sound_bank_catalog.hpp"
#include "native_retail_startup.hpp"
#include "../runtime/src/native/retail_main_control.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace mka::retail;
namespace sound=mka::sound;
extern "C" int SDL_InitSubSystem(unsigned){std::abort();}
namespace {
void check(bool ok,const char* reason){
 if(!ok){std::fprintf(stderr,"FAIL mode service: %s\n",reason);throw std::runtime_error(reason);}
}
struct Game {
 mka::input::ControllerInput input;
 std::unique_ptr<NativeRetailStartupState> startup;
 std::unique_ptr<NativeRetailMainState> main;
 explicit Game(const std::filesystem::path& root){
  mka::content::ContentVolumeFailure cf;auto volume=mka::content::NativeContentVolume::open(root,cf);
  check(bool(volume),"original loose content root");
  startup=std::make_unique<NativeRetailStartupState>(std::move(*volume),std::vector<std::string>{});
  StartupFailure f;check(initialize_native_retail_game_session(*startup,f),"actual game Session");
  main=std::make_unique<NativeRetailMainState>(*startup,input);
  check(!main_binding::initialize_retail_mode_service(*main,f),"missing native catalog/playback never reports success");
  RetailFilePathFailure path;main->file_paths=NativeRetailFilePaths::create(path);
  check(main->file_paths && main_binding::initialize_retail_current_sound(*main,f),"actual file/catalog/current-sound setup");
 }
 std::shared_ptr<NativeRetailSoundBankCatalog> catalog(){return main->sound_bank_catalog;}
 sound::AudioBankHandle bank(unsigned index){
  RetailSoundCatalogFailure f;auto row=catalog()->observe(index,f);
  check(row&&row->bank.has_value(),"actual loaded native bank");return *row->bank;
 }
 unsigned drain(){
  unsigned count=0;RetailSoundCatalogFailure f;
  for(;;){
   auto pending=catalog()->pending(f);check(pending.has_value(),"real pending observation");
   if(!*pending)return count;
   check(++count<=12&&catalog()->pump_one(f),"bounded actual job/callback progress");
  }
 }
 void mode_one(){
  StartupFailure f;check(initial_reset_required::request_mode_one_banks_12a4f0(*main,f),"production mode-one request");
  check(drain()==4,"mode-one still performs four actual bank completions");
 }
 sound::NativePlayHandle held(const sound::AudioBankHandle& bank,std::int32_t selector){
  // Explicit component-test parameters, not proof of retail gains or heard audio.
  sound::NativePlayParameters p{sound::initial_program_control_factors(),sound::initial_program_control_factors(),
    {{1,14336,false},{1,-16384,true},{1,14336,false},{1,-16384,false},2048},
    {1000},3,8,sound::NativeProgramOutputRequest{-1,0}};
  sound::PlaybackFailure f;auto result=main->playback->play(*main->current_sound,bank,selector,p,f);
  if(!result)std::fprintf(stderr,"held fixture failure: playback=%d native_bank=%d preparation=%d program=%d copy=%d voice=%d\n",
    static_cast<int>(f.kind),static_cast<int>(f.native_bank.kind),static_cast<int>(f.preparation.kind),
    static_cast<int>(f.program.kind),static_cast<int>(f.copy.kind),static_cast<int>(f.voice.kind));
  check(result.has_value(),"actual held native play for bank-reference retirement");return *result;
 }
 void finish(){
  sound::PlaybackFailure pf;
  for(const auto& ref:main->game_sound_references)if(!ref.playback.empty()){
   auto present=main->playback->observe(ref.playback,pf);
   if(present){auto retired=main->playback->force_retire(ref.playback,pf);check(retired&&retired->native_resources_released,"test-owned real play retirement");}
   else check(pf.kind==sound::PlaybackError::retired_handle,"known retired test play");
  }
  main->game_sound_references.clear();
  RetailSoundCatalogFailure cf;check(catalog()->shutdown(cf),"actual bank backing retirement");
  check(main->playback->release_sound_context(*main->current_sound,pf),"native context release");
  main->current_sound.reset();check(main->playback->close_when_idle(pf),"canonical native Session close");
  main->playback.reset();main->sound_bank_catalog.reset();
 }
};
std::int32_t first_present_selector(const std::filesystem::path& root,std::string_view name){
 // Original metadata chooses the fixture input. No failed plays are skipped
 // and production lookup has no fallback: selector0 is absent in shell.
 sound::ArchiveFailure af;auto archive=sound::SoundArchive::open(root/"sndsps2/MSLASSET.MS2",af);
 check(archive.has_value(),"original program-selector metadata archive");
 auto source=archive->read_bank(name,af);check(source.has_value(),"original selected bank metadata");
 sound::BankError error;auto bank=sound::SoundBank::parse(std::move(source->bytes),error);
 check(bank.has_value(),"native metadata parser");
 for(std::size_t i=0;i<bank->selector_count();++i)if(bank->lookup(static_cast<std::int32_t>(i)).status==sound::LookupStatus::found){
  std::printf("Original fixture %.*s selector=%zu\n",static_cast<int>(name.size()),name.data(),i);
  return static_cast<std::int32_t>(i);
 }
 throw std::runtime_error("original fixture bank has no present program");
}
struct RetirementProbe:NativeRetailBankReferenceRetirement {
 Game& game;std::vector<unsigned> order;bool refuse{},transfer{};
 explicit RetirementProbe(Game& g):game(g){}
 bool retire(const sound::AudioBankHandle& bank,RetailSoundCatalogFailure& f)override{
  f={};RetailSoundCatalogFailure q;
  check(game.catalog()->selected_descriptor(q)==0,"descriptor store precedes reference cleanup");
  unsigned index=0;
  for(;index!=344;++index){auto row=game.catalog()->observe(index,q);check(row.has_value(),"actual catalog record");
   if(row->bank&&row->bank->same_identity(bank))break;}
  check(index!=344,"retirement receives actual owned bank, not an invented index");order.push_back(index);
  check(!game.catalog()->select_mode_one(q)&&q.kind==RetailSoundCatalogError::selection_active,
        "recursive selection refused before modifying outer selection");
  check(!game.catalog()->shutdown(q)&&q.kind==RetailSoundCatalogError::selection_active,
        "shutdown cannot invalidate an active selection");
  if(transfer)throw detail::NativeMainReentry{game.main.get(),73};
  if(refuse){f.kind=RetailSoundCatalogError::runtime;f.operation="explicit component retirement refusal";return false;}
  return true;
 }
};
}
// Only the endpoint/context construction is an explicit fixture. Mode selection,
// catalog, requests, reads, preparation, callback, cleanup and unload are production.
namespace mka::retail::current_sound_required {
bool create_playback_context_316320(NativeRetailMainState& main,sound::AudioBankSession&& session,StartupFailure&){
 sound::PlaybackFailure f;auto owner=sound::NativePlaybackOwner::create(std::move(session),48000,sound::AudioControlCurve::startup_linear,f);
 check(owner.has_value(),"actual native playback owner");auto context=owner->create_sound_context({3},f);
 check(context.has_value(),"explicit native context");
 main.current_sound=std::move(context);main.playback=std::make_unique<sound::NativePlaybackOwner>(std::move(*owner));return true;
}
bool update_programs_after_bank_results_3152c0(NativeRetailMainState&,StartupFailure&){std::abort();}
}
int main(int argc,char** argv)try{
 check(argc==2,"original content root required");const auto root=std::filesystem::absolute(argv[1]);
 {
  Game g(root);StartupFailure f;RetailSoundCatalogFailure cf;
  check(main_binding::initialize_retail_mode_service(*g.main,f),"ACTUAL production12A4F0(0) binding");
  for(auto [id,slot]:std::array<std::pair<unsigned,unsigned>,2>{{{0,0},{8,4}}}){
   auto row=g.catalog()->observe(id,cf);auto selected=g.catalog()->selection(slot,cf);
   check(row&&row->pending&&!row->bank&&selected&&selected->bank==id&&selected->flags==0,"actual two requests/slots, pending not loaded");
  }
  check(!g.catalog()->selection(21,cf)&&cf.kind==RetailSoundCatalogError::missing_slot,"no fabricated shell slot in mode zero");
  check(main_binding::initialize_retail_mode_service(*g.main,f)&&g.drain()==2,"repeat pending selection never duplicates actual requests");
  const auto permanent=g.bank(0),announcr=g.bank(8);
  check(main_binding::initialize_retail_mode_service(*g.main,f)&&g.drain()==0&&
        g.bank(0).same_identity(permanent)&&g.bank(8).same_identity(announcr),"loaded selected banks retain exact identities");
  bool refused=false;std::thread t([&]{StartupFailure q;refused=!main_binding::initialize_retail_mode_service(*g.main,q);});t.join();
  check(refused,"wrong owner thread rejected");g.finish();
 }
 {
  Game g(root);StartupFailure f;RetailSoundCatalogFailure cf;
  check(initial_reset_required::request_mode_one_banks_12a4f0(*g.main,f),"four queued requests before mode-zero cancellation");
  check(main_binding::initialize_retail_mode_service(*g.main,f),"mode zero cancels only nonselected queued work");
  for(unsigned id:{1u,2u}){auto row=g.catalog()->observe(id,cf);
   check(row&&!row->pending&&!row->bank&&row->last_completion==sound::RetailSoundBankJobStatus::cancelled_queued,"actual nonhead cancellation callback");}
  check(g.drain()==2,"selected common requests still complete once");g.finish();
 }
 {
  Game g(root);StartupFailure f;RetailSoundCatalogFailure cf;
  check(initial_reset_required::request_mode_one_banks_12a4f0(*g.main,f),"four requests before deferred-head cancellation");
  check(g.catalog()->pump_one(cf)&&g.catalog()->pump_one(cf),"two real common-bank completions before switch");
  const auto permanent=g.bank(0),announcr=g.bank(8);
  check(main_binding::initialize_retail_mode_service(*g.main,f),"mode zero requests real head cancellation");
  auto head=g.catalog()->observe(2,cf),queued=g.catalog()->observe(1,cf);
  check(head&&head->pending&&!head->bank&&!head->last_completion,
        "cancelled head stays pending until its actual work/completion runs");
  check(queued&&!queued->pending&&!queued->bank&&
        queued->last_completion==sound::RetailSoundBankJobStatus::cancelled_queued,
        "nonhead cancellation still publishes its immediate queued callback");
  check(g.catalog()->pending(cf)==true&&g.catalog()->pump_one(cf),"one real pump retires the cancelled head");
  head=g.catalog()->observe(2,cf);
  check(head&&!head->pending&&!head->bank&&
        head->last_completion==sound::RetailSoundBankJobStatus::cancelled_after_active&&g.drain()==0,
        "head callback clears pending only after actual cancelled work/backing cleanup");
  check(g.bank(0).same_identity(permanent)&&g.bank(8).same_identity(announcr),
        "head cancellation preserves both selected loaded identities");g.finish();
 }
 {
  Game g(root);g.mode_one();StartupFailure f;RetailSoundCatalogFailure cf;
  const auto shell=g.bank(1),keep=g.bank(8);
  const auto shell_selector=first_present_selector(root,"shell.msb");
  auto a=g.held(shell,shell_selector),b=g.held(keep,0),c=g.held(shell,shell_selector);
  g.main->game_sound_references={{a,shell,11},{b,keep,11},{c,shell,22}};
  check(main_binding::initialize_retail_mode_service(*g.main,f),"real native bank-reference cleanup/unload");
  check(g.main->game_sound_references.size()==1&&g.main->game_sound_references[0].playback.same_identity(b),
        "only matching BANK references retire; selected same-identifier reference and order survive");
  sound::PlaybackFailure pf;check(!g.main->playback->observe(a,pf)&&pf.kind==sound::PlaybackError::retired_handle,"first real native play retired");
  check(!g.main->playback->observe(c,pf)&&pf.kind==sound::PlaybackError::retired_handle&&g.main->playback->observe(b,pf).has_value(),
        "second matching play retired and selected play remains owned");
  check(!g.catalog()->observe(1,cf)->bank&&!g.catalog()->observe(2,cf)->bank&&g.bank(8).same_identity(keep),"nonselected backing unloaded only after references");
  check(g.catalog()->selection(21,cf)->bank==1&&g.catalog()->selection(22,cf)->bank==2,"source stale slot IDs are not silently cleared");
  g.finish();
 }
 {
  Game g(root);g.mode_one();RetirementProbe probe(g);RetailSoundCatalogFailure f;
  probe.transfer=true;bool moved=false;
  try{(void)g.catalog()->select_mode_zero(probe,f);}catch(const detail::NativeMainReentry& e){moved=e.owner==g.main.get()&&e.value==73;}
  check(moved&&g.catalog()->observe(1,f)->bank.has_value(),"legitimate Main transfer propagates without pretending bank unload completed");
  probe.transfer=false;probe.order.clear();
  check(g.catalog()->select_mode_zero(probe,f)&&probe.order==std::vector<unsigned>{1,2},"transfer releases admission without poisoning owner; cleanup scans ascending records");
  g.finish();
 }
 {
  Game g(root);g.mode_one();RetirementProbe probe(g);RetailSoundCatalogFailure f;probe.refuse=true;
  check(!g.catalog()->select_mode_zero(probe,f)&&f.kind==RetailSoundCatalogError::runtime&&
        g.catalog()->observe(1,f)->bank.has_value()&&g.catalog()->observe(2,f)->bank.has_value(),
        "ordinary cleanup failure preserves bank backing and untouched suffix");
  check(!g.catalog()->select_mode_zero(probe,f)&&f.kind==RetailSoundCatalogError::after_effect_failure,"ordinary after-effect failure never silently replays");
  g.finish();
 }
 std::puts("PASS real mode-zero two-bank requests, repeat/queued/deferred-head cancellation, selected identity, live per-bank reference retirement, ordered unload, failure/reentry/thread/control-transfer; window=false audio_endpoint=false game_acceptance=OPEN");
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"FAIL native mode service: %s\n",e.what());return 1;}
