#include "native_closure_codegen.hpp"
#include <array>
#include <bit>
#include <limits>
#include <iterator>
#include <new>
#include <stdexcept>
namespace recomp { namespace {
constexpr const char* identity="a2d205b4be9d7bb08fa32de56d2b30652fbdf29d7f104adde181f8ad222cff7f";
constexpr NativeClosureWord words[]={{0x0018d450u,0x27bdfff0u},{0x0018d454u,0xffbf0000u},{0x0018d458u,0x0c06365cu},{0x0018d45cu,0x00000000u},{0x0018d460u,0x3c050019u},{0x0018d464u,0x0000202du},{0x0018d468u,0x0c0607e4u},{0x0018d46cu,0x24a5e090u},{0x0018d470u,0x0c058ee8u},{0x0018d474u,0x00000000u},{0x0018d478u,0x3c050019u},{0x0018d47cu,0x0000202du},{0x0018d480u,0x0c048974u},{0x0018d484u,0x24a5db60u},{0x0018d488u,0xdfbf0000u},{0x0018d48cu,0x03e00008u},{0x0018d490u,0x27bd0010u},{0x00181f90u,0x27bdfe70u},{0x00182234u,0x0240202du},{0x00182238u,0x0260282du},{0x0018223cu,0x0280302du},{0x00182240u,0x0c0cd904u},{0x00182244u,0x02c0382du},{0x00336410u,0x27bdffa0u},{0x00336414u,0xffbf0040u},{0x00336420u,0x00a0982du},{0x00336428u,0x00e0902du},{0x00336430u,0x0080882du}};
constexpr NativeLogonEffect recipe[]={NativeLogonEffect::prepare_game_world,NativeLogonEffect::surround_movie,NativeLogonEffect::finish_surround_movie,NativeLogonEffect::suspend_game_inputs_after_movie,NativeLogonEffect::transition_task};
constexpr NativeClosureWord input_effect_words[]={
 {0x00163bb0u,0x00051ebcu},{0x00163bb4u,0x00031ffeu},{0x00163bb8u,0x5460001du},
 {0x00163bc8u,0xac400258u},{0x00163bd4u,0xac60025cu},{0x00163bd8u,0xac400260u},
 {0x00163be4u,0xac600278u},{0x00163be8u,0xac40027cu},{0x00163bf4u,0xac600280u},
 {0x00163bf8u,0xac400298u},{0x00163c04u,0xac60029cu},{0x00163c08u,0xac4002a0u},
 {0x00163c0cu,0x2402ffbfu},{0x00163c18u,0x00641825u},{0x00163c1cu,0x0c0a0204u},
 {0x00163c20u,0xa0430081u},{0x00163c24u,0x0c0a01d8u},
 {0x00163a64u,0x2403ffbfu},{0x00163a90u,0x8ce201d4u},{0x00163a94u,0x10400008u},
 {0x00163ab0u,0xa08201ccu},{0x00163abcu,0x29020003u},
 {0x00280824u,0x24102065u},{0x00280828u,0x0c04bb44u},{0x0028083cu,0x0c04ba4cu},
 {0x0028085cu,0xac43000cu},{0x00280768u,0x0c04bb44u},{0x0028076cu,0x2404208bu},
 {0x0028077cu,0x0c04ba4cu},{0x0028079cu,0xac43000cu},
 {0x0012d860u,0x03e00008u},{0x0012d864u,0x0080102du}};
std::vector<NativeClosureWord> source_words(){
 std::vector<NativeClosureWord> out(std::begin(words),std::end(words));
 out.insert(out.end(),std::begin(input_effect_words),std::end(input_effect_words));return out;
}
std::uint32_t le32(std::span<const std::uint8_t> b,std::size_t p){
 if(p>b.size()||b.size()-p<4)throw std::invalid_argument("ELF extent");
 return b[p]|std::uint32_t(b[p+1])<<8|std::uint32_t(b[p+2])<<16|std::uint32_t(b[p+3])<<24;
}
std::optional<std::uint32_t> mapped(std::span<const std::uint8_t>b,std::uint32_t pc){
 if(b.size()<52||b[0]!=0x7f||b[1]!='E'||b[2]!='L'||b[3]!='F'||b[4]!=1||b[5]!=1)throw std::invalid_argument("ELF32LE");
 const auto ph=le32(b,28);const auto sizes=le32(b,40);const std::uint64_t stride=sizes>>16;const std::uint64_t count=le32(b,44)&65535;
 if(stride<32)throw std::invalid_argument("ELF program header");
 for(std::uint64_t i=0;i<count;++i){const std::uint64_t at=std::uint64_t(ph)+i*stride;
  if(at>b.size()||b.size()-std::size_t(at)<32)throw std::invalid_argument("ELF program header bounds");
  const auto p=std::size_t(at);if(le32(b,p)!=1)continue;const auto off=le32(b,p+4),v=le32(b,p+8),n=le32(b,p+16);
  if(pc>=v&&std::uint64_t(pc)-v+4<=n){const std::uint64_t pos=std::uint64_t(off)+pc-v;if(pos>b.size()||b.size()-std::size_t(pos)<4)throw std::invalid_argument("ELF load extent");return le32(b,std::size_t(pos));}
 }return {};
}
std::string digest(std::span<const std::uint8_t> b){
 constexpr std::uint32_t k[]={0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
 std::array<std::uint32_t,8> h={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
 if(b.size()>std::numeric_limits<std::uint64_t>::max()/8||b.size()>std::numeric_limits<std::size_t>::max()-72)throw std::length_error("SHA extent");
 const auto blocks=b.size()/64+1+(b.size()%64>=56);const auto bits=std::uint64_t(b.size())*8;
 for(std::size_t block=0;block<blocks;++block){std::array<std::uint8_t,64> data{};
  for(std::size_t j=0;j<64;++j){const auto at=block*64+j;if(at<b.size())data[j]=b[at];else if(at==b.size())data[j]=128;}
  if(block+1==blocks)for(unsigned j=0;j<8;++j)data[63-j]=std::uint8_t(bits>>(j*8));
  std::uint32_t w[64]{};for(unsigned j=0;j<16;++j)w[j]=std::uint32_t(data[4*j])<<24|std::uint32_t(data[4*j+1])<<16|std::uint32_t(data[4*j+2])<<8|data[4*j+3];
  for(unsigned j=16;j<64;++j){auto a=w[j-15],z=w[j-2];w[j]=w[j-16]+(std::rotr(a,7)^std::rotr(a,18)^(a>>3))+w[j-7]+(std::rotr(z,17)^std::rotr(z,19)^(z>>10));}
  auto a=h[0],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],z=h[7],q=h[1];
  for(unsigned j=0;j<64;++j){auto t=z+(std::rotr(e,6)^std::rotr(e,11)^std::rotr(e,25))+((e&f)^(~e&g))+k[j]+w[j];auto u=(std::rotr(a,2)^std::rotr(a,13)^std::rotr(a,22))+((a&q)^(a&c)^(q&c));z=g;g=f;f=e;e=d+t;d=c;c=q;q=a;a=t+u;}
  h[0]+=a;h[1]+=q;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=z;
 }
 std::string out;out.reserve(64);constexpr char hex[]="0123456789abcdef";for(auto x:h)for(int i=7;i>=0;--i)out.push_back(hex[(x>>(i*4))&15]);return out;
}
} // namespace
NativeLogonManifest native_logon_manifest(){return {identity,source_words(),{std::begin(recipe),std::end(recipe)}};}
std::optional<NativeClosureSources> emit_native_logon_closure(std::span<const std::uint8_t> b,const NativeLogonManifest& m,NativeClosureFailure& f)noexcept {
 f={};try {
  if(m.original_elf_sha256!=identity||digest(b)!=identity){f={NativeClosureError::source_identity,"native LOGON original ELF SHA256"};return {};}
  if(m.words!=source_words()){f={NativeClosureError::source_word,"native LOGON exact ordered source evidence"};return {};}
  for(const auto& w:m.words)if(mapped(b,w.pc)!=w.word){f={NativeClosureError::source_word,"native LOGON mapped source word"};return {};}
  if(m.effects!=std::vector<NativeLogonEffect>(std::begin(recipe),std::end(recipe))){f={NativeClosureError::recipe,"native LOGON complete ordered effect coverage"};return {};}
  return NativeClosureSources{R"NATIVE(#pragma once
#include "mka/movie_service.hpp"
#include "mka/game_action_input.hpp"
#include <functional>
#include <cstdint>
#include <string>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
namespace mka::native_logon {
struct Settings {
    video::MovieViewSelection view;
    std::int64_t language;
    float base_volume;
    Settings(video::MovieViewSelection v,std::int64_t l,float volume):view(v),language(l),base_volume(volume){}
};
struct EffectFailure { const char* operation=""; std::int64_t code=0; };
using Effect=std::function<bool(EffectFailure&)>;
struct Effects {
    Effect prepare_game_world,surround_movie,finish_surround_movie,transition_task;
};
enum class Error { none,invalid_state,wrong_thread,foreign_request,missing_effect,input,effect,service,allocation };
struct Failure {
    Error kind=Error::none; const char* operation="";
    EffectFailure effect; input::GameActionFailure input;
    video::MovieServiceFailure service,cleanup,ownership;
    std::optional<EffectFailure> restoration_failure;
};
enum class Phase { fresh,playing,complete,failed,cancelled };
class NativeLogonState final {
public:
    NativeLogonState(video::NativeMovieService&,input::NativeGameActionInput&,const input::ControllerInput&,input::NativeGameInputTasks&,Settings,Effects);
    ~NativeLogonState();
    NativeLogonState(const NativeLogonState&)=delete;
    NativeLogonState& operator=(const NativeLogonState&)=delete;
    bool native_logon_enter(Failure&) noexcept;
    std::optional<video::MovieControlResult> poll(Failure&) noexcept;
    bool cancel(Failure&) noexcept;
    Phase phase() const noexcept {return phase_;}
private:
    struct SkipLease;
    bool access(Failure&) const noexcept;
    bool invoke(const Effect&,Failure&) noexcept;
    bool native_logon_select(Failure&) noexcept;
    bool native_logon_start(const video::RetailMovieRequest&,Failure&) noexcept;
    void invalidate() noexcept;
    void terminate(Failure&) noexcept;
    bool restore_surround(Failure&) noexcept;
    bool check_request(Failure&,bool require_active) noexcept;
    bool close_owned(Failure&) noexcept;
    video::NativeMovieService& service_;
    input::NativeGameActionInput& actions_;
    const input::ControllerInput& host_;
    input::NativeGameInputTasks& tasks_;
    Settings settings_; Effects effects_;
    std::shared_ptr<SkipLease> skip_;
    Phase phase_=Phase::fresh;
    bool owns_session_=false,surround_owned_=false,in_call_=false;
    std::string source_key_;
    std::uint64_t generation_=0;
    std::shared_ptr<const video::MovieSourceObservationIdentity> source_;
    const std::thread::id owner_=std::this_thread::get_id();
};
} // namespace mka::native_logon
)NATIVE",R"NATIVE(#include "native_logon.hpp"
#include <cmath>
#include <exception>
#include <new>
#include <stdexcept>
#include <type_traits>
namespace mka::native_logon {
namespace {
struct Guard {bool& active;explicit Guard(bool& b):active(b){active=true;}~Guard(){active=false;}};
bool fail(Failure& f,Error e,const char* op){f.kind=e;f.operation=op;return false;}
}
struct NativeLogonState::SkipLease {
    input::NativeGameActionInput* actions;
    const input::ControllerInput* host;
    bool active=true;
};
NativeLogonState::NativeLogonState(video::NativeMovieService& s,input::NativeGameActionInput& a,
    const input::ControllerInput& h,input::NativeGameInputTasks& tasks,Settings v,Effects e):service_(s),actions_(a),host_(h),tasks_(tasks),settings_(v),effects_(std::move(e)){}
NativeLogonState::~NativeLogonState(){
    if(std::this_thread::get_id()!=owner_||in_call_)std::terminate();
    in_call_=true;Failure ignored;invalidate();close_owned(ignored);restore_surround(ignored);
}
bool NativeLogonState::access(Failure& f)const noexcept {
    f={};if(std::this_thread::get_id()!=owner_)return fail(f,Error::wrong_thread,"native LOGON owner thread");
    if(in_call_)return fail(f,Error::invalid_state,"native LOGON reentry");return true;
}
void NativeLogonState::invalidate()noexcept {if(skip_){skip_->active=false;skip_->actions=nullptr;skip_->host=nullptr;}skip_.reset();}
bool NativeLogonState::check_request(Failure& f,bool require_active)noexcept {
    if(!owns_session_)return true;
    video::MovieServiceFailure query;auto actual=service_.request_view(query);
    if(query.kind!=video::MovieServiceError::none){
        f.ownership=std::move(query);owns_session_=false;source_.reset();
        if(f.kind==Error::none)fail(f,Error::service,"native LOGON request observation failure");return false;
    }
    if(!actual){owns_session_=false;source_.reset();
        if(require_active){if(f.kind==Error::none)fail(f,Error::foreign_request,"native LOGON owned request no longer active");return false;}return true;}
    if(actual->generation!=generation_||actual->source_key!=source_key_||actual->source!=source_){
        owns_session_=false;source_.reset();
        f.ownership.kind=video::MovieServiceError::foreign_request;f.ownership.operation="native LOGON foreign live request";
        if(f.kind==Error::none)fail(f,Error::foreign_request,"native LOGON foreign live request");return false;
    }return true;
}
bool NativeLogonState::close_owned(Failure& f)noexcept {
    if(!check_request(f,false))return false;
    if(!owns_session_)return true;
    // Same creating thread; no callbacks occur between the identity check and close.
    owns_session_=false;source_.reset();video::MovieServiceFailure cleanup;
    if(!service_.close(cleanup)){f.cleanup=std::move(cleanup);if(f.kind==Error::none)fail(f,Error::service,"native LOGON resource cleanup failed");return false;}return true;
}
bool NativeLogonState::restore_surround(Failure& f)noexcept {
    if(!surround_owned_)return true;
    // An attempted restore is never retried, even if the effect fails or throws.
    surround_owned_=false;Failure restoration;
    if(invoke(effects_.finish_surround_movie,restoration))return true;
    auto detail=restoration.effect;
    if(!detail.operation||!*detail.operation)detail.operation=restoration.operation;
    f.restoration_failure=detail;
    if(f.kind==Error::none)fail(f,restoration.kind,"native LOGON surround restoration failed");return false;
}
void NativeLogonState::terminate(Failure& f)noexcept {
    invalidate();close_owned(f);restore_surround(f);phase_=Phase::failed;
}
bool NativeLogonState::invoke(const Effect& effect,Failure& f)noexcept {
    try {EffectFailure ef;const bool ok=effect(ef);if(!ok||ef.code!=0||!ef.operation||*ef.operation){f.effect=ef;return fail(f,Error::effect,"native LOGON required game effect failed");}return true;}
    catch(const std::bad_alloc&){return fail(f,Error::allocation,"native LOGON game effect allocation");}
    catch(...){return fail(f,Error::effect,"native LOGON game effect exception");}
}
bool NativeLogonState::native_logon_enter(Failure& f)noexcept {
    if(!access(f))return false;Guard guard(in_call_);
    if(phase_!=Phase::fresh)return fail(f,Error::invalid_state,"native LOGON enter once");
    if(!effects_.prepare_game_world||!effects_.surround_movie||!effects_.finish_surround_movie||!effects_.transition_task)
        return fail(f,Error::missing_effect,"native LOGON unbound game-world effect");
    // Existing Service/control/gain admit all finite binary32 base values.
    if(!std::isfinite(settings_.base_volume))return fail(f,Error::invalid_state,"native LOGON nonfinite base volume");
    if(!actions_.validate_logon_bindings(f.input))return fail(f,Error::input,"native LOGON action mappings");
    if(!actions_.validate_post_movie_targets(f.input))return fail(f,Error::input,"native LOGON post-movie action ownership");
    if(!tasks_.validate_access(f.input.tasks)){
        f.input.kind=input::GameActionError::wrong_thread;f.input.operation="native LOGON task owner thread";
        return fail(f,Error::input,"native LOGON task ownership");}
    auto service_state=service_.snapshot(f.service);if(!service_state)return fail(f,Error::service,"native LOGON service observation");
    if(service_state->active)return fail(f,Error::invalid_state,"native LOGON service already active");
    video::MovieCatalogFailure catalog_failure;auto request=video::RetailMovieCatalog{}.resolve(0,settings_.view,settings_.language,catalog_failure);
    if(!request)return fail(f,Error::invalid_state,"native LOGON catalog request");
    try {source_key_=std::move(request->logical_key);skip_=std::make_shared<SkipLease>(SkipLease{&actions_,&host_,true});}
    catch(...){return fail(f,Error::allocation,"native LOGON skip lease allocation");}
    if(!invoke(effects_.prepare_game_world,f)){terminate(f);return false;}
    // Source preparation clears the SAME persistent intro-skip owner.
    if(!actions_.reset_intro_skip(f.input)){fail(f,Error::input,"native LOGON preparation skip reset");terminate(f);return false;}
    // A failed surround callback owns rollback of its own partial effects.
    if(!invoke(effects_.surround_movie,f)){terminate(f);return false;}
    surround_owned_=true;
    return native_logon_select(f);
}
bool NativeLogonState::native_logon_select(Failure& f)noexcept {
    video::MovieCatalogFailure cf;auto request=video::RetailMovieCatalog{}.resolve(0,settings_.view,settings_.language,cf);
    if(!request||request->logical_key!=source_key_){fail(f,Error::invalid_state,"native LOGON catalog selection");terminate(f);return false;}
    return native_logon_start(*request,f);
}
bool NativeLogonState::native_logon_start(const video::RetailMovieRequest& request,Failure& f)noexcept {
    try {
        // Required game effects may have used the shared Service. Never overwrite
        // the controls or cancel a foreign session they started.
        video::MovieServiceFailure query;auto active=service_.request_view(query);
        if(query.kind!=video::MovieServiceError::none){f.ownership=std::move(query);fail(f,Error::service,"native LOGON pre-start ownership query");terminate(f);return false;}
        if(active){fail(f,Error::foreign_request,"native LOGON foreign request before start");terminate(f);return false;}
        auto lease=skip_;
        if(!service_.set_base_volume(settings_.base_volume,f.service)||
            !service_.install_skip_if_present([lease]()->std::int64_t{
                if(!lease->active)return 0;
                input::GameActionFailure af;auto edge=lease->actions->poll_logon_skip(*lease->host,af);
                if(!edge)throw std::runtime_error(af.operation);return *edge?1:0;
            },f.service)) {fail(f,Error::service,"native LOGON native control settings");terminate(f);return false;}
        if(!service_.start(request.id,request.requested_view,settings_.language,4,f.service)){
            fail(f,Error::service,"native LOGON service start");terminate(f);return false;}
        // Nonallocating active view; own no borrowed key. The key was moved into
        // this State before acquisition. No intervening callback occurs here.
        auto actual=service_.request_view(f.ownership);
        if(!actual||f.ownership.kind!=video::MovieServiceError::none||actual->source_key!=source_key_||!actual->source||actual->generation==0){
            fail(f,Error::foreign_request,"native LOGON successful start missing identity");terminate(f);return false;}
        generation_=actual->generation;source_=std::move(actual->source);owns_session_=true;
        phase_=Phase::playing;return true;
    }catch(...){fail(f,Error::allocation,"native LOGON skip callback allocation");terminate(f);return false;}
}
std::optional<video::MovieControlResult> NativeLogonState::poll(Failure& f)noexcept {
    if(!access(f))return {};Guard guard(in_call_);
    if(phase_!=Phase::playing){fail(f,Error::invalid_state,"native LOGON poll without active movie");return {};}
    if(!check_request(f,true)){terminate(f);return {};}
    auto result=service_.poll(f.service);
    if(result.progress==video::MovieControlProgress::ongoing)return result;
    // Service.poll terminal results already perform actual request teardown.
    // Drop ownership before any continuation/restore callback can start another.
    owns_session_=false;source_.reset();invalidate();
    if(result.progress==video::MovieControlProgress::error){fail(f,Error::service,"native LOGON movie failure");restore_surround(f);phase_=Phase::failed;return result;}
    if(!restore_surround(f)){phase_=Phase::failed;return {};}
    if(!actions_.suspend_after_logon(tasks_,f.input)){
        fail(f,Error::input,"native LOGON post-movie input suspension");phase_=Phase::failed;return {};}
    if(!invoke(effects_.transition_task,f)){
        phase_=Phase::failed;return {};}
    phase_=Phase::complete;return result;
}
bool NativeLogonState::cancel(Failure& f)noexcept {
    if(!access(f))return false;Guard guard(in_call_);invalidate();
    const bool closed=close_owned(f);const bool restored=restore_surround(f);
    if(!closed||!restored){phase_=Phase::failed;return false;}
    if(phase_!=Phase::complete&&phase_!=Phase::failed)phase_=Phase::cancelled;return true;
}
static_assert(std::is_nothrow_move_assignable_v<video::MovieServiceFailure>);
} // namespace mka::native_logon
)NATIVE"};
 }catch(const std::bad_alloc&){f={NativeClosureError::allocation,"native LOGON emitter allocation"};}
 catch(const std::length_error&){f={NativeClosureError::allocation,"native LOGON emitter extent"};}
 catch(...){f={NativeClosureError::source_format,"native LOGON original ELF mapping"};}return {};
}
} // namespace recomp
