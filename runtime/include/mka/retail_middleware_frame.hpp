#pragma once

#include "mka/retail_crt.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <thread>

namespace mka::retail {

// Native logical time, measured from the first sample of this owner's CRT.
// The source Count/frequency/wrap representation is deliberately absent.
enum class RetailMiddlewareFrameError {
    none,
    allocation,
    host_capacity,
    native_exception,
    wrong_thread,
    wrong_clock,
    crt,
    clock_regressed,
    foreign_sample,
    invalid_callback
};
struct RetailMiddlewareFrameFailure {
    RetailMiddlewareFrameError kind = RetailMiddlewareFrameError::none;
    RetailCrtFailure crt;
};

class NativeRetailMiddlewareFrame;
struct RetailMiddlewareTimeSample {
    std::uint64_t nanoseconds_since_first = 0;
    std::uint64_t nanoseconds_since_previous = 0;
    std::uint64_t milliseconds_since_first = 0;
    std::uint64_t milliseconds_since_previous = 0;
private:
    std::shared_ptr<const void> issuer_;
    std::uint64_t clock_nanoseconds_ = 0;
    friend class NativeRetailMiddlewareFrame;
};

// This owner must be retired before the CRT to which it is bound. It persists
// across MainState reentry. The callback starts empty, as the source BSS slot
// does; installing one requires a separately proved native producer.
class NativeRetailMiddlewareFrame final {
public:
    using Callback = std::function<void(std::uint8_t)>;

    static std::unique_ptr<NativeRetailMiddlewareFrame> create(
        NativeRetailCrt&, RetailMiddlewareFrameFailure&) noexcept;
    NativeRetailMiddlewareFrame(const NativeRetailMiddlewareFrame&) = delete;
    NativeRetailMiddlewareFrame& operator=(const NativeRetailMiddlewareFrame&) = delete;

    bool validate(NativeRetailCrt&, RetailMiddlewareFrameFailure&) const noexcept;
    std::optional<RetailMiddlewareTimeSample> sample(
        NativeRetailCrt&, RetailMiddlewareFrameFailure&) noexcept;
    std::optional<std::uint64_t> elapsed_nanoseconds(
        const RetailMiddlewareTimeSample& earlier,
        const RetailMiddlewareTimeSample& later,
        RetailMiddlewareFrameFailure&) const noexcept;
    static std::uint64_t logical_milliseconds(std::uint64_t nanoseconds) noexcept;
    static bool reached_100_milliseconds(std::uint64_t nanoseconds) noexcept;

    bool install_callback(Callback, RetailMiddlewareFrameFailure&) noexcept;
    bool clear_callback(RetailMiddlewareFrameFailure&) noexcept;
    // Call only after the actual dispatcher pump. Selection happens inside
    // this method so a completion may change the callback before invocation.
    bool invoke_after_dispatcher(
        NativeRetailCrt&, std::uint8_t source_argument,
        RetailMiddlewareFrameFailure&);
    std::optional<bool> has_callback(RetailMiddlewareFrameFailure&) const noexcept;
private:
    explicit NativeRetailMiddlewareFrame(NativeRetailCrt&);
    bool owner_thread(RetailMiddlewareFrameFailure&) const noexcept;
    bool same_live_crt(NativeRetailCrt&, RetailMiddlewareFrameFailure&) const noexcept;

    NativeRetailCrt* crt_;
    const std::thread::id thread_;
    std::shared_ptr<const void> issuer_;
    std::optional<std::uint64_t> first_nanoseconds_;
    std::optional<std::uint64_t> previous_nanoseconds_;
    std::shared_ptr<const Callback> callback_;
};
} // namespace mka::retail
