#pragma once

namespace ps2::state {
class Stream;
}

namespace ps2::iop::kernel {

// Registers the host C++ service for MWFILEFR's observed SIF RPC endpoint.
// The module descriptor calls this before EE clients bind to the endpoint.
void install_native_mwfilefr_service();
void register_native_mwfilefr_job();
void mwfilefr_state(ps2::state::Stream &s);

} // namespace ps2::iop::kernel
