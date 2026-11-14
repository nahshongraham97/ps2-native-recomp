#include "kernel.hpp"

#include "ps2/state.hpp"

namespace ps2::iop::kernel {

// The IOP state stream keeps the historical "cdvd" and "cdvdfsv" sections in
// their original order and field layout, now backed by the native service's
// live state (cdvd_native.cpp). A save written before the service existed still
// loads field for field, and a new save records the real read/stream position.
void legacy_storage_state(ps2::state::Stream &s) {
    cdvd_state(s);
    cdvdfsv_state(s);
}

} // namespace ps2::iop::kernel
