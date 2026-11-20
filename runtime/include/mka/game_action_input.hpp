#pragma once
#include "mka/controller_input.hpp"
#include "mka/game_input_tasks.hpp"
#include <memory>
#include <thread>
#include <vector>
namespace mka::input {
struct GameActionParticipant {
 std::uint32_t player;std::map<std::uint32_t,std::uint32_t> mappings;bool game_assigned,dispatch_blocked;
 GameActionParticipant(std::uint32_t p,std::map<std::uint32_t,std::uint32_t> m,bool assigned,bool blocked):player(p),mappings(std::move(m)),game_assigned(assigned),dispatch_blocked(blocked){}
};
struct GameActionState { std::uint32_t player; std::uint32_t previous=0,current=0,rising=0; bool connected=false,active=false,game_assigned=false,dispatch_blocked=false; };
enum class GameActionError { none,invalid_configuration,missing_player,missing_mapping,wrong_thread,task,allocation };
struct GameActionFailure { GameActionError kind=GameActionError::none; const char* operation=""; GameInputTaskFailure tasks; };
class NativeGameActionInput final {
public:
 static std::unique_ptr<NativeGameActionInput> create(std::vector<GameActionParticipant>,bool explicit_initial_inhibit,GameActionFailure&) noexcept;
 bool validate_logon_bindings(GameActionFailure&) const noexcept;
 bool add_unassigned_record(std::uint32_t role,GameActionFailure&) noexcept;
 bool set_game_assigned(std::uint32_t,bool,GameActionFailure&) noexcept;
 bool set_dispatch_blocked(std::uint32_t,bool,GameActionFailure&) noexcept;
 // State ingress only: does not implement full source182BB0.
 bool set_suspend_inhibit(bool,GameActionFailure&) noexcept;
 bool validate_post_movie_targets(GameActionFailure&) const noexcept;
 bool suspend_after_logon(NativeGameInputTasks&,GameActionFailure&) noexcept;
 bool resume_source_inputs(NativeGameInputTasks&,GameActionFailure&) noexcept;
 bool poll(const ControllerInput&,GameActionFailure&) noexcept;
 std::optional<std::uint32_t> query_action(std::uint32_t,GameActionFailure&) const noexcept;
 std::optional<std::uint32_t> query_ordinary_action(std::uint32_t,GameActionFailure&) const noexcept;
 std::optional<bool> poll_logon_skip(const ControllerInput&,GameActionFailure&) noexcept;
 bool remap(std::uint32_t player,std::map<std::uint32_t,std::uint32_t>,GameActionFailure&) noexcept;
 bool set_ordinary_suspended(bool,GameActionFailure&) noexcept;
 bool reset_intro_skip(GameActionFailure&) noexcept;
 std::optional<std::vector<GameActionState>> snapshot(GameActionFailure&) const noexcept;
 bool intro_skip() const noexcept { return intro_skip_; }
 NativeGameActionInput(const NativeGameActionInput&)=delete;
 NativeGameActionInput& operator=(const NativeGameActionInput&)=delete;
private:
 explicit NativeGameActionInput(std::vector<GameActionParticipant>,bool);
 bool access(GameActionFailure&) const noexcept;
 bool mappings_present(std::uint32_t,GameActionFailure&) const noexcept;
 std::vector<GameActionParticipant> participants_;
 std::vector<GameActionState> states_;
 std::vector<bool> host_bound_;
 std::thread::id owner_=std::this_thread::get_id();
 bool suspended_=false,intro_skip_=false,inhibit_;
};
} // namespace mka::input
