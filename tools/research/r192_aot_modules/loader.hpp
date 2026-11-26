#pragma once
#include <filesystem>

namespace mka::aot {
// Call once after generated code/VU registration, before boot/load-state.
// package_directory is normally executable_directory / "aot-modules/ee".
// Throws a named error on any package mismatch; no compilation or fallback.
void initialize_precompiled_aot_modules(const std::filesystem::path& package_directory);
}
