#pragma once
#include <cstdint>
#include <functional>
#include <memory>

namespace mka::native_logon {
// Labels must have static storage; no borrowed operation/session string may survive cleanup.
struct EffectFailure { const char* operation = ""; std::int64_t code = 0; };
using Effect = std::function<bool(EffectFailure&)>;
struct SurroundFailure {EffectFailure primary,graphics,audio;};
enum class EffectProgress { pending, complete, error };

// One owned native movie handoff, created without platform side effects.
// begin is called once. Even a failed/partial begin has an unconditional finish.
// Pending must have empty failure fields. Retain earlier causes internally while
// rollback is pending, then report all causes on terminal error. Pending is not
// success; error retains ownership until an explicit retry.
// Dependencies and native receipts must be owned through final completion.
// Finish must join actual movie retirement, perform both boundary/restoration
// operations and attempt audio release independently of a graphics failure.
// These are required implementations, not default successful platform stubs.
class NativeLogonSurround {
public:
    virtual ~NativeLogonSurround() = default;
    virtual EffectProgress begin(SurroundFailure&) = 0;
    virtual EffectProgress poll_begin(SurroundFailure&) = 0;
    virtual EffectProgress begin_finish(SurroundFailure&) = 0;
    virtual EffectProgress poll_finish(SurroundFailure&) = 0;
    virtual EffectProgress retry_finish(SurroundFailure&) = 0;
};
using SurroundFactory = std::function<std::unique_ptr<NativeLogonSurround>(EffectFailure&)>;
} // namespace mka::native_logon
