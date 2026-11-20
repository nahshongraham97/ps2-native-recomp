#pragma once
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
