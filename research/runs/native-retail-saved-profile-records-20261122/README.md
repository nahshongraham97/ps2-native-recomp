# Native Retail Saved Profile Records and Reset Bindings

## Overview
This run delivers the production native implementations for two previously unresolved retail startup and initial reset bindings:
1. `mka::retail::main_binding::initialize_retail_saved_profile_records` (called at `runtime/src/native/retail_main.cpp:175`)
2. `mka::retail::initial_reset_required::reset_saved_profile_children_246070` (called at `runtime/src/native/retail_initial_reset.cpp:43`)

Both bindings previously existed only as test fixtures, leaving production links incomplete and memory card profile record state without typed native ownership.

## Retail Analysis & Behavioral Parity
- **Startup Sequence (`00171B10`)**:
  - Validates underlying storage readiness (`00225000`).
  - Iterates over both memory card slots (Slot 0 and Slot 1, PS2 stride `0x1E218` bytes each):
    - Sets slot status word to -1 (`0xffffffff`).
    - Initializes 8 profile children per slot with defaults from `00245CB0` (per language) and clears the in-use flag at `+0x18 + index*4`.
    - Clears byte flag at `+0x0C`.
    - Copies default profile preferences (`0x58` bytes) via `002955D0`.
    - Updates aggregate records across parents via `002432A0`.
  - Marks both card slots ready (`status_word = 1`).
  - Resets active player profiles (Player 0 and Player 1) via `00245DD0`.
- **Active Player Reset (`00246070` / `00245DD0`)**:
  - Resets Player 0 and Player 1 active profile state.
  - If a player was attached to a memory card profile (`slot >= 0 && profile_index >= 0`), releases the in-use flag in that card slot.
  - Resets active profile data to defaults (`00245CB0`).
  - Resets player status to 8 (`DAT_0064c720[player] = 8`), context word to 0 (`002817B0`), dirty flag to 0, and unassigns slot/index (`-1`).

## Native C++ Architecture
- **Typed Ownership**:
  - `NativeRetailSavedProfileChild`: owns `in_use` flag and typed `RetailSavedProfileDefaults`, with `make_aggregate_view()` projection for `2432A0`.
  - `NativeRetailSavedCardSlot`: encapsulates slot status word, version word, flag byte 12, `RetailProfilePreferenceValues` (from `2955D0`), and array of 8 `NativeRetailSavedProfileChild`.
  - `NativeRetailActivePlayerProfile`: encapsulates active player assignment (card slot, profile index, dirty flag, status word, context word, defaults).
  - `NativeRetailSavedProfileRecords`: container owned by `NativeRetailMainState::saved_profile_records` via `std::unique_ptr`.
- **Zero Emulation Model**:
  - 100% native host allocations with thread-affinity checks.
  - Eliminates guest fixed addresses (`0x660640`, `0x6af670`, `0x6aba40`, `0x64e910`, `DAT_0064c720`).
  - Zero raw RDRAM pointer manipulation.

## Verification
- Unit test `test_native_retail_saved_profile_records.exe`:
  - 171B10 startup across 2 slots and 8 children each.
  - 246070 and 245DD0 per-player reset with in-use release.
  - Aggregate observation matching `2432A0`.
  - Multi-language defaults (English, German, French).
  - Re-entry and error guards (thread affinity, double-init, uninitialized calls).
- Linked with `test_native_retail_initial_reset.exe` passing cleanly.
- Full negative-link test `test_native_startup_missing_bindings.exe`:
  - Both `initialize_retail_saved_profile_records` and `reset_saved_profile_children_246070` confirmed removed from undefined symbol list.
