#pragma once
#include <filesystem>

namespace mka::aot {
// Startup only. Every DLL is authenticated before one merged registration.
// Validated native code and locked files remain resident until process exit.
void initialize_precompiled_vu_modules(const std::filesystem::path& directory);
}
