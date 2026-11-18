#pragma once

// Vector unit programs the build does not know, compiled while the game runs (docs/vu-live.md).
//
// A game build compiles the vector unit programs that were recorded before it (vurecomp); every other program runs in the
// interpreter, several times slower. With live compiling the runner keeps interpreting such a program and, in the background,
// makes the C++ for it with the same generator (recomp/vurecomp_gen.cpp), compiles that into a library with a C++ compiler,
// loads the library and from then on uses the compiled program like a built-in one. The libraries stay in a cache folder next
// to the game, so the next start loads them without compiling anything.

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
