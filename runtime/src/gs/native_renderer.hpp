#pragma once

#include "gs.hpp"

#include <cstdint>

namespace ps2::gs::native_renderer {

// Mandatory direct GIF-event route. Backend failure and unsupported packets
// stop; no environment setting may select compatibility drawing.
bool selected();
bool active();
bool strict();
void reset();
void tag(int path, std::uint64_t tag);
void packed(std::uint32_t register_id, const Reg128 &value);
void reglist(std::uint32_t register_id, std::uint64_t value);
void image(const std::uint8_t *data, std::uint32_t qwords);

} // namespace ps2::gs::native_renderer
