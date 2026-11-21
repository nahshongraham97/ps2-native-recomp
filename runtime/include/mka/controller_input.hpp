#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>

namespace mka::input {

// Retail button/axis encodings are compatibility data, not a serial device.
// Each publication owns one complete host sample. No registers, bus, DMA,
// controller protocol, or hardware port capacity is modeled here.
struct NativeControllerAxes {
    // Signed host values retain SDL's full stick/trigger resolution.
    std::array<std::int16_t, 4> sticks{}; // right x/y, left x/y
    std::array<std::int16_t, 2> triggers{}; // left, right
};
struct ControllerState {
    std::uint16_t buttons = 0;
    std::array<std::uint8_t, 4> axes{{128,128,128,128}}; // right x/y, left x/y
    std::array<std::uint8_t, 16> pressure{};
    std::optional<NativeControllerAxes> native_axes;
    bool connected = false;
};
struct ControllerSample {
    ControllerState state;
    std::uint64_t sequence = 0;
};

ControllerState digital_state(std::uint16_t buttons, bool connected,
    const std::array<std::uint8_t,4>& axes = {{128,128,128,128}},
    std::uint8_t left_trigger = 255, std::uint8_t right_trigger = 255,
    std::optional<NativeControllerAxes> native_axes = std::nullopt);
std::string encode_pressure(const std::array<std::uint8_t,16>&);
std::optional<std::array<std::uint8_t,16>> decode_pressure(std::string_view);

class ControllerInput {
public:
    // Player identity is assigned by the game/host, never a hardware address.
    // Dynamic storage: no two-port or other PS2 resource ceiling.
    bool publish(std::uint32_t player, const ControllerState& state);
    std::optional<ControllerSample> sample(std::uint32_t player) const;
private:
    mutable std::mutex mutex_;
    std::map<std::uint32_t,ControllerSample> players_;
    std::uint64_t sequence_ = 0;
};

ControllerInput& controllers();
} // namespace mka::input
