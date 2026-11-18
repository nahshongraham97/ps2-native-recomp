#pragma once

// Main processor code the build does not know, translated while the game runs (docs/ee-live.md).
//
// A game build translates the code of the program and of the code windows it has seen (eerecomp). Code a game copies into a window
// later, or a function of a window image that nobody called while the build explored, has no translation, and the runner used to stop
// with "missing code". Now the game waits: the runner takes the window's image from memory (the way scripts/windows.py capture does),
// translates it with the same translator as the build (recomp/eerecomp_gen.cpp), compiles the C++ into a library with a C++ compiler,
// loads it, adds it to the modules it has (add_modules) and goes on. The libraries stay in a cache folder next to the game.

#include <cstdint>
#include <string>

namespace ps2::ee::live {

inline void start(const std::string &, const std::string &, const std::string &) {}
inline bool active() { return false; }
inline bool translate_missing(std::uint32_t pc) {
    (void)pc;
    return false;
}
inline void frame_tick() {}

}  // namespace ps2::ee::live

namespace ps2 {

// Is there a translated module in place for the code that sits in the window at `base` over its first `size` bytes? A question only
// (runtime/src/dispatch.cpp): nothing is added or dropped and the spot-check cursor stays where it is, so asking does not change what
// the game does.
bool window_covered(std::uint32_t base, std::uint32_t size);

struct ModuleInfo;

// add_modules (generated.hpp) with a rank: of two modules of the same content that are both in place and both have the function, the
// one of the higher rank runs. A quick library (rank 0) is followed by one made with the normal flags (rank 1) for the same window.
void add_modules_ranked(const ModuleInfo *modules, std::uint32_t count, int rank);

}  // namespace ps2
