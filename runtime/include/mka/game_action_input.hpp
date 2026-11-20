#pragma once
#include "mka/controller_input.hpp"
#include <memory>
#include <thread>
#include <vector>
namespace mka::input {
struct GameActionParticipant { std::uint32_t player; std::map<std::uint32_t,std::uint32_t> mappings; };
struct GameActionState { std::uint32_t player; std::uint32_t previous=0,current=0,rising=0; bool connected=false,active=false; };
enum class GameActionError { none,invalid_configuration,missing_player,missing_mapping,wrong_thread,allocation };
struct GameActionFailure { GameActionError kind=GameActionError::none; const char* operation=""; };
class NativeGameActionInput final {
public:
 static std::unique_ptr<NativeGameActionInput> create(std::vector<GameActionParticipant>,GameActionFailure&) noexcept;
 bool validate_logon_bindings(GameActionFailure&) const noexcept;
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
 explicit NativeGameActionInput(std::vector<GameActionParticipant>);
 bool access(GameActionFailure&) const noexcept;
 bool mappings_present(std::uint32_t,GameActionFailure&) const noexcept;
 std::vector<GameActionParticipant> participants_;
 std::vector<GameActionState> states_;
 std::thread::id owner_=std::this_thread::get_id();
 bool suspended_=false,intro_skip_=false;
};
} // namespace mka::input
