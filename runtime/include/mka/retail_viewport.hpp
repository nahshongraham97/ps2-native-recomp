#pragma once
#include <cstdint>

namespace mka::retail {
// Game-coordinate values from225DB0, not host framebuffer/texture capacities.
// Display configuration is a distinct source value; it must not silently
// become the visible game width or a native allocation limit.
struct RetailLogicalViewport {
    std::uint32_t width{},height{},display_configuration{};
    bool operator==(const RetailLogicalViewport&) const = default;
};
} // namespace mka::retail
