#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>

namespace mka::input {
using ActuatorBytes = std::array<std::uint8_t,6>;
enum class HapticError { none, missing_alignment, unsupported_alignment,
    unsupported_small_value, invalid_request, unsupported_command,
    allocation_failure, sequence_exhausted, stale_feedback };
struct HapticRequest { bool small = false; std::uint8_t large = 0; };
struct HapticDecode { HapticRequest request; HapticError error = HapticError::none;
    explicit operator bool() const { return error==HapticError::none; } };
// Actuator IDs0/1 are the verified binary-small / byte-large pair. Other
// actuator identities and nonbinary small values require another contract.
HapticDecode decode_actuators(const ActuatorBytes& alignment,const ActuatorBytes& values);

struct ActDirectRpc { std::uint32_t player=0,slot=0; ActuatorBytes values{}; };
struct ActDirectDecode { ActDirectRpc request; HapticError error=HapticError::none;
    explicit operator bool() const { return error==HapticError::none; } };
// New SDK RPC07 only: command/port/slot u32LE then six bytes at+12.
// The retail EE119BE8 uses a different32-byte DMA path; this decoder alone
// does NOT establish that retail transport binding or invent its completion.
ActDirectDecode decode_set_actdir_rpc(const std::uint8_t* bytes,std::size_t size);

enum class HapticFeedback { pending, applied, failed };
struct HapticSample {
    ActuatorBytes alignment{{255,255,255,255,255,255}};
    ActuatorBytes values{};
    HapticRequest request;
    std::uint64_t sequence=0;
    HapticFeedback feedback=HapticFeedback::pending;
};
class Haptics {
public:
    HapticError set_alignment(std::uint32_t player,const ActuatorBytes& alignment);
    HapticError submit(std::uint32_t player,const ActuatorBytes& values);
    HapticError stop(std::uint32_t player);
    HapticError acknowledge(std::uint32_t player,std::uint64_t sequence,bool applied);
    std::optional<HapticSample> sample(std::uint32_t player) const;
    // Keeps owned identities and publishes explicit zero requests so a host
    // can stop motors after close/reset. It never reuses a sequence number.
    HapticError stop_all();
private:
    mutable std::mutex mutex_;
    std::map<std::uint32_t,HapticSample> players_;
    std::uint64_t sequence_=0;
};
Haptics& haptics();
} // namespace mka::input
