#pragma once
// Additive paired source-Plan metadata. The original version-8 payload is
// unchanged. This does not serialize active adapter continuation contexts.
#include "reference_resource_owner_epochs.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>

namespace mka::emitted::resource_plan_checkpoint {
// Root host bridge supplies the actual Registry and configured quick/save root.
// The tests use an owned directory; production uses its existing configured root.
void before_save(const std::string&path);
void prepare_save(const std::string&path,std::vector<std::uint8_t>&state);
void saved(const std::string&path,std::span<const std::uint8_t>state);
class PathLoad {
    struct Impl;std::unique_ptr<Impl>impl_;
public:
    PathLoad(const std::string&path,std::span<const std::uint8_t>state);
    ~PathLoad();
    PathLoad(const PathLoad&)=delete;PathLoad&operator=(const PathLoad&)=delete;
};
void begin_buffer_restore(std::span<const std::uint8_t>state);
std::span<const std::uint8_t>payload_for_restore(std::span<const std::uint8_t>state);
void end_buffer_restore(bool full_machine_success);
}

extern "C" mka::emitted::resource_owner_epochs::Registry&r192_family_registry();
extern "C" void r192_family_restore_begin();
extern "C" void r192_family_restore_end(bool success);
extern "C" const char*r192_family_checkpoint_source_id();
extern "C" const char*r192_family_checkpoint_owned_root();
