#include "mka/retail_middleware_frame.hpp"
#include "retail_main_control.hpp"

#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mka::retail {
namespace {
bool fail(RetailMiddlewareFrameFailure& f, RetailMiddlewareFrameError kind) noexcept {
    f.kind = kind;
    return false;
}
} // namespace

NativeRetailMiddlewareFrame::NativeRetailMiddlewareFrame(NativeRetailCrt& crt)
    : crt_(&crt), thread_(std::this_thread::get_id()),
      issuer_(std::make_shared<int>(0)) {}

std::unique_ptr<NativeRetailMiddlewareFrame> NativeRetailMiddlewareFrame::create(
    NativeRetailCrt& crt, RetailMiddlewareFrameFailure& f) noexcept {
    f = {};
    if (!crt.validate_current_thread(f.crt)) {
        f.kind = RetailMiddlewareFrameError::crt;
        return {};
    }
    try {
        return std::unique_ptr<NativeRetailMiddlewareFrame>(
            new NativeRetailMiddlewareFrame(crt));
    } catch (const std::bad_alloc&) {
        f.kind = RetailMiddlewareFrameError::allocation;
    } catch (const std::length_error&) {
        f.kind = RetailMiddlewareFrameError::host_capacity;
    } catch (...) {
        f.kind = RetailMiddlewareFrameError::native_exception;
    }
    return {};
}

bool NativeRetailMiddlewareFrame::owner_thread(
    RetailMiddlewareFrameFailure& f) const noexcept {
    if (std::this_thread::get_id() != thread_)
        return fail(f, RetailMiddlewareFrameError::wrong_thread);
    return true;
}

bool NativeRetailMiddlewareFrame::same_live_crt(
    NativeRetailCrt& crt, RetailMiddlewareFrameFailure& f) const noexcept {
    if (!owner_thread(f)) return false;
    if (&crt != crt_)
        return fail(f, RetailMiddlewareFrameError::wrong_clock);
    if (!crt.validate_current_thread(f.crt)) {
        f.kind = RetailMiddlewareFrameError::crt;
        return false;
    }
    return true;
}

bool NativeRetailMiddlewareFrame::validate(NativeRetailCrt& crt, RetailMiddlewareFrameFailure& f) const noexcept {
    f = {};
    return same_live_crt(crt, f);
}
std::optional<RetailMiddlewareTimeSample> NativeRetailMiddlewareFrame::sample(
    NativeRetailCrt& crt, RetailMiddlewareFrameFailure& f) noexcept {
    f = {};
    if (!same_live_crt(crt, f)) return {};
    auto now = crt.now_nanoseconds(f.crt);
    if (!now) {
        f.kind = RetailMiddlewareFrameError::crt;
        return {};
    }
    if (previous_nanoseconds_ && *now < *previous_nanoseconds_) {
        f.kind = RetailMiddlewareFrameError::clock_regressed;
        return {};
    }
    // First sample defines the native logical origin, regardless of when the
    // CRT itself was constructed. Neither source Count nor an EE rate survives.
    const auto first = first_nanoseconds_.value_or(*now);
    const auto previous = previous_nanoseconds_.value_or(*now);
    RetailMiddlewareTimeSample result;
    result.nanoseconds_since_first = *now - first;
    result.nanoseconds_since_previous = *now - previous;
    result.milliseconds_since_first = logical_milliseconds(result.nanoseconds_since_first);
    result.milliseconds_since_previous = logical_milliseconds(result.nanoseconds_since_previous);
    result.issuer_ = issuer_;
    result.clock_nanoseconds_ = *now;
    if (!first_nanoseconds_) first_nanoseconds_ = *now;
    previous_nanoseconds_ = *now;
    return result;
}

std::optional<std::uint64_t> NativeRetailMiddlewareFrame::elapsed_nanoseconds(
    const RetailMiddlewareTimeSample& earlier,
    const RetailMiddlewareTimeSample& later,
    RetailMiddlewareFrameFailure& f) const noexcept {
    f = {};
    if (!same_live_crt(*crt_, f)) return {};
    if (earlier.issuer_ != issuer_ || later.issuer_ != issuer_) {
        f.kind = RetailMiddlewareFrameError::foreign_sample;
        return {};
    }
    if (later.clock_nanoseconds_ < earlier.clock_nanoseconds_) {
        f.kind = RetailMiddlewareFrameError::clock_regressed;
        return {};
    }
    return later.clock_nanoseconds_ - earlier.clock_nanoseconds_;
}

std::uint64_t NativeRetailMiddlewareFrame::logical_milliseconds(
    std::uint64_t nanoseconds) noexcept {
    return nanoseconds / 1000000u;
}
bool NativeRetailMiddlewareFrame::reached_100_milliseconds(
    std::uint64_t nanoseconds) noexcept {
    return nanoseconds >= 100000000u;
}

bool NativeRetailMiddlewareFrame::install_callback(
    Callback callback, RetailMiddlewareFrameFailure& f) noexcept {
    f = {};
    if (!same_live_crt(*crt_, f)) return false;
    if (!callback) return fail(f, RetailMiddlewareFrameError::invalid_callback);
    try {
        // Publication is one pointer replacement. The old selected callable
        // stays alive through a concurrent-in-control-flow invocation.
        auto retained = std::make_shared<const Callback>(std::move(callback));
        callback_ = std::move(retained);
        return true;
    } catch (const std::bad_alloc&) {
        return fail(f, RetailMiddlewareFrameError::allocation);
    } catch (const std::length_error&) {
        return fail(f, RetailMiddlewareFrameError::host_capacity);
    } catch (...) {
        return fail(f, RetailMiddlewareFrameError::native_exception);
    }
}

bool NativeRetailMiddlewareFrame::clear_callback(
    RetailMiddlewareFrameFailure& f) noexcept {
    f = {};
    if (!same_live_crt(*crt_, f)) return false;
    callback_.reset();
    return true;
}

bool NativeRetailMiddlewareFrame::invoke_after_dispatcher(
    NativeRetailCrt& crt, std::uint8_t source_argument,
    RetailMiddlewareFrameFailure& f) {
    f = {};
    if (!same_live_crt(crt, f)) return false;
    // Load after the caller's dispatcher pump, matching 328CE0/328CA0.
    // Retain through invocation: this callable may replace/clear its slot.
    auto selected = callback_;
    try {
        if (selected) (*selected)(source_argument);
        return true;
    } catch (const detail::NativeMainReentry&) {
        // Retail mode changes are control flow, not callback failures.
        throw;
    } catch (const std::bad_alloc&) {
        return fail(f, RetailMiddlewareFrameError::allocation);
    } catch (const std::length_error&) {
        return fail(f, RetailMiddlewareFrameError::host_capacity);
    } catch (...) {
        return fail(f, RetailMiddlewareFrameError::native_exception);
    }
}

std::optional<bool> NativeRetailMiddlewareFrame::has_callback(
    RetailMiddlewareFrameFailure& f) const noexcept {
    f = {};
    if (!same_live_crt(*crt_, f)) return {};
    return bool(callback_);
}
} // namespace mka::retail
