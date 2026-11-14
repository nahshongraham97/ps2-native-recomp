#pragma once

#include "gs.hpp"

#include <cstdint>

namespace ps2::gs::native_renderer {

// Opt-in direct GIF-event renderer. When selected, gif::render sends packet
// events here instead of through the compatibility GsSink.
bool selected();
bool active();
bool strict();
void reset();
void tag(int path, std::uint64_t tag);
void packed(std::uint32_t register_id, const Reg128 &value);
void reglist(std::uint32_t register_id, std::uint64_t value);
void image(const std::uint8_t *data, std::uint32_t qwords);

} // namespace ps2::gs::native_renderer
