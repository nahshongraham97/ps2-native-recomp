#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include <utility>
namespace mka::game {
struct GameTransitionConfiguration {
 std::vector<std::int32_t> initial_contexts;
 std::int32_t main_mode;std::uint64_t active_render_view_count;std::int32_t transition_selector;
 GameTransitionConfiguration(std::vector<std::int32_t> contexts,std::int32_t mode,std::uint64_t views,std::int32_t selector):initial_contexts(std::move(contexts)),main_mode(mode),active_render_view_count(views),transition_selector(selector){}
};
struct GameTransitionSnapshot {std::vector<std::int32_t> contexts;std::int32_t main_mode;std::uint64_t active_render_view_count;std::int32_t transition_selector;};
enum class GameTransitionError {none,wrong_thread,reentry,allocation};
struct GameTransitionFailure {GameTransitionError kind=GameTransitionError::none;const char* operation="";};
// Native game state only. No scheduler/world teardown or production bootstrap.
class NativeGameTransitionState final {
public:
 ~NativeGameTransitionState();
 static std::unique_ptr<NativeGameTransitionState> create(GameTransitionConfiguration,GameTransitionFailure&) noexcept;
 bool reset_context(GameTransitionFailure&) noexcept;
 bool push_context(std::int32_t,GameTransitionFailure&) noexcept;
 bool pop_context(GameTransitionFailure&) noexcept;
 std::optional<std::int32_t> current_context(GameTransitionFailure&) const noexcept;
 std::optional<std::int32_t> current_past21(GameTransitionFailure&) const noexcept;
 bool enter_transition(GameTransitionFailure&) noexcept;
 // Actual caller must invoke AFTER its required successful world teardown.
 bool apply_post_world_transition(GameTransitionFailure&) noexcept;
 // Explicit native state ingress; these do not establish a bootstrap producer.
 bool set_main_mode(std::int32_t,GameTransitionFailure&) noexcept;
 bool set_active_render_view_count(std::uint64_t,GameTransitionFailure&) noexcept;
 bool set_transition_selector(std::int32_t,GameTransitionFailure&) noexcept;
 std::optional<GameTransitionSnapshot> snapshot(GameTransitionFailure&) const noexcept;
 NativeGameTransitionState(const NativeGameTransitionState&)=delete;
 NativeGameTransitionState& operator=(const NativeGameTransitionState&)=delete;
private:
 explicit NativeGameTransitionState(GameTransitionConfiguration);
 struct Guard {bool& busy;explicit Guard(bool& b) noexcept:busy(b){busy=true;}~Guard(){busy=false;}};
 bool access(GameTransitionFailure&) const noexcept;
 std::vector<std::int32_t> contexts_;
 std::int32_t main_mode_,selector_;
 std::uint64_t views_;
 std::thread::id owner_=std::this_thread::get_id();
 mutable bool busy_=false;
};
}
