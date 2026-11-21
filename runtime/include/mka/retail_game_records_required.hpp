#pragma once

namespace mka::retail {
struct NativeRetailMainState;
struct StartupFailure;

namespace main_binding {
// Reused by the later 1861B0 source call after its own ordered teardown.
// This entry performs the complete 130350 sequence, including task births.
bool reset_retail_game_records_130350(NativeRetailMainState&,StartupFailure&);
// Source 16FD80 installs real 12DA40 Wave allocation and 16FDA0/12D9C0
// immediate-or-deferred retirement consumed by the text renderer.
bool install_retail_text_allocator_pair_16fd80(
    NativeRetailMainState&,StartupFailure&);
}
} // namespace mka::retail
