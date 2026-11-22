# Native Retail Selected Resource, Nonowning References, and Saved Profile Records

## Executive Summary
This delivery implements the required production native bindings and classes for:
1. `mka::retail::initial_reset_required::reset_selected_resource_334700`
2. `mka::retail::initial_reset_required::configure_selected_resource_zero_16fc70`
3. `mka::retail::initial_reset_required::clear_nonowning_references_18da30`
4. `mka::retail::main_binding::initialize_retail_saved_profile_records`
5. `mka::retail::initial_reset_required::reset_saved_profile_children_246070`

All implementations are 100% native C++20 with zero guest emulation (`g_ram`, `g_spr`, `g_memory`, emulated SPU2/IOP), strict thread-affinity validation, fail-closed error propagation, exact retail instruction ordering, and unaliased typed records. Audio files remain strictly untouched.

---

## 1. Selected Resource (`00334700` and `0016FC70`)

### 1.1 Arcade Ladder Reset (`00334700`)
- **Retail Address**: `00334700` in `SLUS_214.10`.
- **Instruction Flow**:
  - `00334710: sw v1, -0x5360(gp)` -> stores `-1` (`0xffffffff`) to progress word at `0x0064ed90`.
  - `00334718: sw zero, 0x2e8(v0)` -> clears `records->word_6602e8 = 0`.
  - `00334720: sw zero, -0x5364(gp)` (delay slot) -> resets tier index to `0` at `0x0064ed8c`.
  - `0033471c: jal 0x00185c30(19)` -> draws PRNG in `[0, 18]` using the same-Session retail PRNG (`random->low16_range(19)`).
  - `0033472c: sw a0, -0x746c(gp)` -> stores selected ladder index to `0x0064cc84`.
  - `0033473c: lw v1, 0(v1) / sw v1, -0x535c(gp)` -> selects the active ladder pointer from the 19-ladder table (`0x005e5630`) and writes to `0x0064ed94`.
- **Retail Arcade Ladder Table**:
  - Exactly 19 ladders extracted directly from `SLUS_214.10` at VMA `0x005e5630`.
  - Each ladder contains 8 tiers with 4 words each (`opponent_fighter_id`, `sub_id`, `arena_id`, `extra_flags`), totaling 128 bytes per ladder.

### 1.2 Font Zero Configuration (`0016FC70`)
- **Retail Address**: `0016FC70` (called with `a0 = 0` for font resource 0).
- **Asset Identification**:
  - Font index 0 corresponds to `EDDMONDGRAY14`.
  - Atlas asset: `art/gameart.ssf` / `MKDA.PAK` at offset `0x3f080`, size 34,560 bytes.
    - SHA256: `085846ff5d595e179e118c2fcbe51f59f322f6c51fc871cac80354f19d1990bc`.
  - Metrics asset: `EDDMONDGRAY14_MET` at offset `0x8b50`, size 8,116 bytes.
    - SHA256: `b91b6ee00f3bed46a1e89471c247ea659b88af927d7bd9576311b57fd0cfd497`.
- **Trace `16FC70(0) → 300B60 / 300440`**:
  - `0016FCE0`: checks if font 0 is already cached (`entry.atlas && entry.metrics`); returns early on cache hit.
  - `300B60`: decodes raw texture bytes into `mka::FontAtlas`.
  - `300440`: parses raw metrics bytes into `mka::text::NativeFontMetrics` (224 glyphs, space advance 6.0f).
  - `0016FD18..0016FD44`: manipulates atlas object flags at `+0x50`:
    `(flags & 0xffff0000) | 0x3301`.
- **Layout & Glyph Geometry Reuse**:
  - Integrates existing `mka::text::layout_font_bytes` and `mka::text::geometry_font_glyph` directly through `NativeRetailSelectedResource::layout_text` and `NativeRetailSelectedResource::glyph_geometry`.

### 1.3 Nonowning Reference Clears (`0018DA30`)
- **Retail Address**: `0018DA30`.
- **Instruction Flow**:
  - `sw zero, -0x5db8(gp)` -> clears `nonowning_object_reference` at `0x0064E348`.
  - `sw zero, -0x5db4(gp)` -> clears `nonowning_object_cookie` at `0x0064E34C`.
  - `sw zero, -0x5dc0(gp)` -> clears `nonowning_task_reference` at `0x0064E340`.
  - `sw zero, -0x5dbc(gp)` -> clears `nonowning_task_cookie` at `0x0064E344`.
- **Native Implementation**:
  - Implemented on `NativeRetailInitialTaskState::clear_nonowning_references_from_18da30` and bound to `initial_reset_required::clear_nonowning_references_18da30`.

---

## 2. Saved Profile Records (`00171B10`, `00246070`, `00245DD0`)

### 2.1 Retail Startup Sequence (`00171B10`)
- **Storage Precondition**: Validates `NativeSaveStore` readiness (replacing console transport `00225000`).
- **Two Memory Card Slots Loop (`card_slot = 0, 1`)**:
  - Publishes `status_word = -1` (`00171B68: sw v0, 0(s2)` where `v0 = -1`) per parent.
  - **8 Child Profile Slots Loop**:
    - Delay slot `00171BA8: sw zero, 0x18(s1)` clears `parent + 0x18 + index*4` (`profile_in_use[index] = false`).
    - Populates known defaults via `initialize_retail_saved_profile_known_defaults_from_245cb0`.
    - Retains distinct child fields: `gate_word_3c28 = 0`, `or_word_fc = 0`, `or_words = {0,0,0,0}`, `and_words_120 = 0xffffffff` (40 words), `and_words_1c0 = 0xffffffff` (30 words).
    - **Crucial**: No aliasing between `gate_word_3c28` and `in_use`; no aliasing between `or_word_fc` and `command.bitword_1fc`.
  - Delay slot `00171BC4: sb zero, 0xc(s2)` of `2432A0` clears `parent + 0x0C` (`flag_byte_0c = 0`).
  - Calls `refresh_retail_profile_aggregate_from_2432a0(main, parent_views, agg_failure)`.
    - Allows `detail::NativeMainReentry` control transfer to propagate cleanly without being swallowed.
  - Calls `2955D0` with `parent + 0x38` in delay slot: sets slot preferences to `retail_profile_preference_defaults_from_2955d0()`.
- **Status Publication**:
  - `00171BF4 / 00171BFC: sw v1, 0x640(v0) / sw v1, -0x17a8(v0)` writes `1` to card slot 0 status and card slot 1 status **only after BOTH parent loops finish**.
- **No Active Player Resets**:
  - `171B10` does **not** touch active players or call `245DD0`/`246070`.

### 2.2 Active Player Profile Reset (`00246070` / `00245DD0`)
- **`00246070`**: Exact two-player wrapper calling `245DD0(0)` then `245DD0(1)`.
- **`00245DD0(player)`**:
  1. `00245DF0: jal 0x00281830`: reads participant's assigned role.
  2. `00245F0C..00245F18`: conditionally clears returned role's first word (`source_control_word = 0`) **BEFORE** active record clear/defaults.
  3. `00245E88..00245F08`: unsets card slot `profile_in_use` flag if active player was attached to a memory card profile.
  4. `00245F1C..00246014`: resets active player profile record to defaults (`245cb0`).
  5. `00246018 / 00246020 / 0024602C`:
     - Clears dirty flag to `0`.
     - Sets `associated_card_slot = -1`.
     - Sets `associated_profile_slot = -1`.
  6. `00246028: jal 0x002817b0`: rereads assigned role, clears its `source_control_word`, and clears retained per-player word (`retained_per_player_word = 0` for `gp - 0x565c` / `gp - 0x5660`).
  7. `00246040: sw a1, 0(v1)`: writes `player_status = 8` (`0x0064C720 + player * 4`).

---

## 3. Verification & Test Evidence
- `test_native_retail_selected_resource.exe`:
  - `PASS: test_ladder_tables` (all 19 ladders, 8 tiers each verified)
  - `PASS: test_selected_resource_lifecycle_and_bindings` (`334700` reset, `18da30` reference clears, `16fc70` atlas decode and metrics parse, text layout, glyph geometry)
  - `PASS: test_wrong_thread_refusal`
  - `ALL SELECTED RESOURCE TESTS PASSED.`
- `test_native_retail_saved_profile_records.exe`:
  - `PASS: test_initialization_order_and_card_slots` (`171B10` sequence, card slot status -1 -> 1, child record defaults, distinct unaliased fields, preference defaults, double-init refusal)
  - `PASS: test_active_player_reset_and_association_cleanup` (`246070` / `245DD0`, role `source_control_word` clearing, card slot in-use release, active defaults reset, slot unassignment, `player_status = 8`)
  - `PASS: test_wrong_thread_refusal`
  - `ALL SAVED PROFILE RECORDS TESTS PASSED.`
- `test_native_retail_initial_reset.exe`:
  - `PASS native reset ordering, live fields, suspension,1799-wait timeout, failure and lifetime; six effects substituted in this caller fixture`
- `test_native_retail_reset_sound.exe`:
  - `PASS_CONNECTED_RETAIL_RESET_SOUND; production bank binding/four real callbacks/wait release; unrelated game/context fixtures; no game/endpoint`
- `test_native_retail_save_startup.exe`:
  - `PASS Main native save readiness and retained file lifetime; full profile records remain required`
- `test_native_retail_profile_preferences.exe`:
  - `PASS actual Main current profile preferences, native alias lifetime and external encoding; full game open`
- `test_native_retail_profile_menu.exe`:
  - `PASS profile menu with actual scene lookup,18A260,18B0D0 and1892D0; required action/event/task producers open`
- `test_native_retail_scene_bootstrap_prefix.exe`:
  - `PASS production1861B0 prefix: source phase/timing, real Session, errors, thread, lifetime, reentry; suffix fixture only`

---

## 4. Audio Lane & Git Isolation
- Zero changes to any audio files (`git diff origin/main -- runtime/src/native/*sound* tests/test_*sound*` is completely empty).
- All work isolated in worktree `external/forks/ps2-native-recomp-selected-resource` on branch `selected-resource-work`.
- Up-to-date with `origin/main` (`a890c72`).
