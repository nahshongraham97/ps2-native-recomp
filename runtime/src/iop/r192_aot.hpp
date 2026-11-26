#pragma once

// Bounded Stage A bridge: compiled R3000 bodies retain the existing IOP state
// and memory-service interfaces. This is not final native-port credit.
#include "iop/iop.hpp"
#include "native/iop_native.h"

#include <cstdint>

namespace ps2::iop::r192_aot {

struct Import {
    std::uint32_t offset;
    const char *library;
    std::uint16_t version;
    std::uint32_t ordinal;
};

using Body = std::uint32_t (*)(ps2x::iop::detail::IopCpuState &,
                              ps2x::iop::detail::IopMemory &, std::uint32_t);
using PicBody = std::uint32_t (*)(ps2x::iop::detail::IopCpuState &,
                                 ps2x::iop::detail::IopMemory &, std::uint32_t, std::uint32_t);
struct Module {
    const char *name;
    std::uint32_t base, text_size, total_size;
    const std::uint32_t *original_words;
    const Import *imports;
    std::uint32_t import_count;
    Body body;
    PicBody pic_body{};
};

struct Services {
    void *user{};
    // A changed import word is accepted only after the real loader validates
    // its HLE/library/version/ordinal binding. No arbitrary patched word passes.
    bool (*validate_import)(void *, std::uint32_t, const Import &){};
    // A false return means unbound; it must not mutate CPU/memory/services.
    bool (*invoke_import)(void *, const Import &, Cpu &){};
};

// The preserved v8 Cpu has no pending-load or BadVAddr fields. Keep that ABI
// unchanged; the bounded R3000 route carries these explicitly, never as EE data.
struct Pipeline {
    std::uint32_t pending_load_reg{}, pending_load_value{}, badvaddr{};
    bool pending_load{};
};

enum class Status {
    budget_exhausted, import_unbound, external_destination, architectural_exception,
    code_identity_mismatch, invalid_module, invalid_entry, invalid_body_result,
};
struct Result {
    Status status;
    std::uint32_t instructions;
    const Import *import;
    const char *reason;
};

bool matches(const Module &, const Services & = {});
Result dispatch(const Module &, Cpu &, Pipeline &, std::uint32_t budget, const Services & = {});
const char *status_name(Status);

// Defined only by the separately generated, private first-module descriptor.
const Module &selected_module();

} // namespace ps2::iop::r192_aot
