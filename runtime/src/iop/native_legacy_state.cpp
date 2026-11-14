#include "kernel.hpp"

#include "ps2/state.hpp"

#include <cstdint>
#include <vector>

namespace ps2::iop::kernel {

namespace {

// Compatibility slots for existing save files. No CD/DVD service is registered.
struct LegacyReadState {
    bool busy = false;
    std::uint32_t callback = 0;
    std::uint32_t error = 0;
    std::vector<int> waiters;
};

struct LegacyStreamState {
    std::uint32_t buffer_sectors = 0;
    std::uint32_t bank_sectors = 0;
    std::uint32_t buffer_address = 0;
    std::uint32_t position = 0;
    bool active = false;
};

LegacyReadState g_legacy_read;
LegacyStreamState g_legacy_stream;
bool g_legacy_file_streaming = false;
std::uint32_t g_legacy_file_stream_position = 0;

} // namespace

void legacy_storage_state(ps2::state::Stream &s) {
    s.tag("cdvd");
    s.pod(g_legacy_read.busy);
    s.pod(g_legacy_read.callback);
    s.pod(g_legacy_read.error);
    s.vec(g_legacy_read.waiters);
    if (s.version() >= 6) s.pod(g_legacy_stream);

    s.tag("cdvdfsv");
    s.pod(g_legacy_file_streaming);
    s.pod(g_legacy_file_stream_position);
}

} // namespace ps2::iop::kernel
