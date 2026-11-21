#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
namespace mka::retail {
enum class RetailCrtError {none,allocation,host_capacity,native_exception,wrong_thread,closed,already_finalized,not_finalized,clock_regressed,clock_overflow,invalid_capability,unknown_callback,work_active,callbacks_active,callback_failed};
struct RetailCrtFailure {RetailCrtError kind=RetailCrtError::none;};
enum class NativeCrtTaskOperation {wake,park,yield};
// Implementations bind real cooperative native task effects. No guest IDs,
// priority emulation, OS thread suspension or successful default exists.
class NativeCrtTaskCapability {
public:
    virtual ~NativeCrtTaskCapability()=default;
    virtual bool wake(RetailCrtFailure&)=0;
    virtual bool park(RetailCrtFailure&)=0;
    virtual bool yield(RetailCrtFailure&)=0;
};
enum class NativeCrtWorkState {pending,running,completed,failed};
struct NativeCrtWorkObservation {NativeCrtWorkState state;RetailCrtFailure failure;};
class NativeCrtWorkReceipt final {
public:
    NativeCrtWorkObservation observe() const noexcept;
private:
    struct State;std::shared_ptr<State> state_;
    explicit NativeCrtWorkReceipt(std::shared_ptr<State>);
    friend class NativeRetailCrt;
};
class NativeCrtCallbackArgument {public:virtual ~NativeCrtCallbackArgument()=default;};
using NativeCrtCallback = bool(*)(const std::shared_ptr<const NativeCrtCallbackArgument>&,RetailCrtFailure&);
class NativeCrtCallbackToken final {
public: NativeCrtCallbackToken()=default;
private:std::shared_ptr<const void> owner_,identity_;friend class NativeRetailCrt;
};
struct NativeRetailCrtObservation {bool finalized=false,closed=false;std::size_t queued=0,running=0,callbacks=0;};
// A real frame-thread executor and clock, not the complete original alarm ABI.
// Pump/invoke/finalize/shutdown require the creating thread. Clock reads and
// submit/register/unregister are serialized across threads. Callouts execute
// unlocked; nested pump/invoke is rejected, unregister during invocation keeps
// the selected argument alive until return. Owner lifetime must span all calls.
class NativeRetailCrt final {
public:
    static std::unique_ptr<NativeRetailCrt> create(RetailCrtFailure&) noexcept;
    ~NativeRetailCrt();
    NativeRetailCrt(const NativeRetailCrt&)=delete;
    NativeRetailCrt& operator=(const NativeRetailCrt&)=delete;
    bool finalize_pre_main(RetailCrtFailure&) noexcept;
    bool validate_current_thread(RetailCrtFailure&) const noexcept;
    std::optional<std::uint64_t> now_nanoseconds(RetailCrtFailure&) noexcept;
    std::shared_ptr<NativeCrtWorkReceipt> submit(std::shared_ptr<NativeCrtTaskCapability>,NativeCrtTaskOperation,RetailCrtFailure&) noexcept;
    std::optional<std::size_t> pump(std::size_t max_work,RetailCrtFailure&) noexcept;
    std::optional<NativeCrtCallbackToken> register_callback(NativeCrtCallback,std::shared_ptr<const NativeCrtCallbackArgument>,RetailCrtFailure&) noexcept;
    bool invoke_callback(const NativeCrtCallbackToken&,RetailCrtFailure&) noexcept;
    bool unregister_callback(const NativeCrtCallbackToken&,RetailCrtFailure&) noexcept;
    NativeRetailCrtObservation observe() const noexcept;
    bool shutdown(RetailCrtFailure&) noexcept;
private:
    struct State;std::unique_ptr<State> state_;
    explicit NativeRetailCrt(std::unique_ptr<State>);
};
} // namespace mka::retail
