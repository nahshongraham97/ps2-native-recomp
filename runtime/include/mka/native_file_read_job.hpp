#pragma once
#include "mka/native_file.hpp"
#include <memory>
#include <optional>
#include <span>
namespace mka::content {
class NativeContentFile;
enum class NativeFileReadError {none,unsupported_host,closed,wrong_thread,invalid_range,allocation,host_capacity,open,event,worker,submit,read,short_read,cancel,not_complete};
struct NativeFileReadFailure {NativeFileReadError kind{NativeFileReadError::none};std::error_code host;};
enum class NativeFileReadStatus {pending,complete,cancelled,failed};
// Actual asynchronous file work over the SAME opened file identity, with an
// independent native OVERLAPPED handle/cursor. No pathname reopening.
// Poll never waits for IO. Cancellation retains buffer/OVERLAPPED until a
// terminal poll. Destroying still-pending work is a contract violation.
class NativeFileReadJob final {
public:
    static std::unique_ptr<NativeFileReadJob> begin(NativeFile&,std::uint64_t offset,std::size_t bytes,NativeFileReadFailure&) noexcept;
    static std::unique_ptr<NativeFileReadJob> begin(NativeContentFile&,std::uint64_t offset,std::size_t bytes,NativeFileReadFailure&) noexcept;
    ~NativeFileReadJob();
    NativeFileReadJob(const NativeFileReadJob&)=delete;
    NativeFileReadJob& operator=(const NativeFileReadJob&)=delete;
    std::optional<NativeFileReadStatus> poll(NativeFileReadFailure&) noexcept;
    bool cancel(NativeFileReadFailure&) noexcept;
    std::optional<std::span<const std::uint8_t>> bytes(NativeFileReadFailure&) const noexcept;
private:
    struct State;std::shared_ptr<State> state_;
    explicit NativeFileReadJob(std::shared_ptr<State>);
};
}
