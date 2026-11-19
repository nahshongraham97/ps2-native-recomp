#pragma once

// Retired test-only API shim. These empty functions do not compile or load
// programs. The host/runtime no longer includes this header. The stale
// scripts/test_vu_codegen.py fixture still references it and must be replaced
// with an independent AOT math oracle before this header can be deleted.
// This shim supplies no runtime coverage or native-port acceptance evidence.

#include "ps2/vu_ops.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace ps2::vu::live {

inline void start(const std::string &) {}
inline bool active() { return false; }
inline void unknown(std::uint64_t, std::uint32_t, const std::uint8_t *) {}
inline void note_call(std::uint64_t) {}
inline bool ready() { return false; }
inline std::vector<Program> take() { return {}; }

}  // namespace ps2::vu::live
