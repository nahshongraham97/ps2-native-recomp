#pragma once
#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>
#include <utility>
namespace mka::input {
enum class GameInputTaskKind : std::uint32_t { kind2065=0x2065,kind2066=0x2066,kind208b=0x208b };
enum class GameInputTaskError {none,wrong_thread,invalid_kind,foreign_owner,stale_task,overflow,allocation};
struct GameInputTaskFailure {GameInputTaskError kind=GameInputTaskError::none;const char* operation="";};
class NativeGameInputTasks;
class GameInputTaskToken {
public: GameInputTaskToken(const GameInputTaskToken&)=default;GameInputTaskToken& operator=(const GameInputTaskToken&)=default;
private: friend class NativeGameInputTasks;
 GameInputTaskToken(std::shared_ptr<const void> o,std::uint64_t g):owner_(std::move(o)),generation_(g){}
 std::shared_ptr<const void> owner_;std::uint64_t generation_;
};
struct GameInputTaskState {GameInputTaskKind kind;std::uint64_t generation;bool live;std::optional<bool> saved_restore;};
// Existing-task saved restore state only. No task body, execution or exit cleanup.
class NativeGameInputTasks final {
public:
 static std::unique_ptr<NativeGameInputTasks> create(GameInputTaskFailure&) noexcept;
 std::optional<GameInputTaskToken> create_task(GameInputTaskKind,std::optional<bool>,GameInputTaskFailure&) noexcept;
 bool retire(const GameInputTaskToken&,GameInputTaskFailure&) noexcept;
 std::optional<GameInputTaskState> get(const GameInputTaskToken&,GameInputTaskFailure&) const noexcept;
 std::optional<GameInputTaskState> first(GameInputTaskKind,GameInputTaskFailure&) const noexcept;
 bool validate_access(GameInputTaskFailure&) const noexcept;
 bool write_existing_restore(bool,GameInputTaskFailure&) noexcept;
 NativeGameInputTasks(const NativeGameInputTasks&)=delete;NativeGameInputTasks& operator=(const NativeGameInputTasks&)=delete;
private:
 NativeGameInputTasks();
 std::shared_ptr<const void> identity_;
 std::vector<GameInputTaskState> records_;
 std::uint64_t generation_=0;
 std::thread::id thread_=std::this_thread::get_id();
};
}
