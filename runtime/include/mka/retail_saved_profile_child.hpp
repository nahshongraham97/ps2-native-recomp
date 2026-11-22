#pragma once
#include "mka/retail_saved_profile_defaults.hpp"
#include "mka/retail_profile_aggregate.hpp"
#include "mka/retail_profile_image.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace mka::retail {
enum class RetailSavedChildPhase {zero_born,resetting,source_defaults,failed_reset,import_blocked};
enum class RetailSavedProfileChildError {none,known_defaults,not_observable,invalid_child_index,loaded_import_required};
struct RetailSavedProfileChildFailure {
 RetailSavedProfileChildError kind{RetailSavedProfileChildError::none};
 const char* operation{""};
 RetailSavedProfileDefaultFailure defaults;
};
struct RetailSavedChildIdentity {
 std::array<std::uint8_t,6> entry_code{};
 std::uint8_t entry_attribute{};
 std::uint32_t terminal_metadata{},aggregate_gate{},signature{};
};
struct RetailSavedChildWallet {std::uint32_t current{},accumulated{};};
struct RetailSavedCounterPair {std::uint32_t first{},second{};};
// Game result data, not host time or a guest register. The source display
// tests the high word before reading the packed low-byte H/M/S fields.
struct RetailSavedTimedResult {
 std::uint32_t marker{},packed_clock{};
 bool present()const noexcept{return marker!=0;}
};
struct RetailSavedChildStatistics {
 std::array<RetailSavedCounterPair,3> mode_pairs{};
 std::array<RetailSavedTimedResult,5> timed_results{};
};
struct RetailSavedNamedPreset {
 std::string first_name,second_name;
 std::vector<std::string> command_text;
};
// Logical game flags. Secondary160..191 and primary0..31 share storage.
// Source ID ranges are game-data namespaces,not host allocation ceilings.
class RetailSavedKafProgress final {
public:
 bool primary(std::uint32_t id,std::uint32_t live_aggregate_660500)const noexcept;
 bool secondary(std::uint32_t id)const noexcept;
 // Value mutations only,not the whole515240 operation with its other owners.
 bool store_primary(std::uint32_t id,bool);
 bool store_secondary(std::uint32_t id,bool);
 void clear()noexcept{primary_.clear();secondary_only_.clear();}
private:
 std::set<std::uint32_t> primary_,secondary_only_;
};
struct RetailSavedTriple {std::array<std::uint32_t,3> component_bits{};};
struct RetailSavedGameRestoreValues {
 std::uint32_t context64{},context68{},context9c{},contexta0{},context928{},context924{};
};
class NativeRetailSavedObjectKindSnapshot;
class NativeRetailSavedPreviewDefinition;
// Immutable saved VALUE payload types remain unimplemented. They must never
// own/retire live actors,scene references or26F070's current file handle.
struct RetailSavedKafSupplement {
 std::uint32_t flags{},phase{};
 RetailSavedTriple captured_components;
 std::uint32_t selected_option{};
 RetailSavedTriple position,orientation;
 std::uint32_t pose_scalar{},saved_context_selector{},control40{};
 std::uint32_t derived_selection{},context_mode{};
 std::uint32_t third_parameter{},saved_third_parameter{},fourth_parameter{};
 std::uint32_t retained_third_parameter{},retained_fourth_parameter{},sixth_parameter{};
 std::uint32_t alternate_count{},operation_phase{};
 std::array<std::uint32_t,8> category_indices{};
 RetailSavedKafProgress progress;
 RetailSavedGameRestoreValues game_restore;
 std::uint32_t accumulated_delta{},event_count{};
 // Per-child values. Session's active/staging owners are NOT copied here.
 std::vector<std::uint8_t> resource_ids;
 std::vector<std::shared_ptr<const NativeRetailSavedObjectKindSnapshot>> object_snapshots;
 std::shared_ptr<const NativeRetailSavedPreviewDefinition> preview_definition;
};

// Borrowed projections combine existing proved defaults with the additional
// semantic fields. References point to ONE authority per source field.
struct RetailSavedKafView {
 const RetailSavedProfileGameplayDefaults& source_values;
 const RetailSavedKafSupplement& additional;
};
struct RetailSavedCustomizationView {
 const RetailSavedProfileCommandDefaults& source_values;
 const std::uint32_t& pending;
 const std::uint32_t& condition;
};

// A native VALUE owned by root's stable parent/slot/active record node. Copy
// copies data,not node identity. The actual domain owns lifetime/thread checks
// and the SINGLE Working control40; no Session/aggregate/live-ID owner here.
// Only zero-BSS and source-reset defaults are admitted by this implementation.
class RetailSavedProfileChildValue final {
public:
 RetailSavedProfileChildValue()noexcept;
 static RetailSavedProfileChildValue zero_born()noexcept{return {};}
 RetailSavedChildPhase phase()const noexcept{return phase_;}
 bool reset_from_245cb0(std::int32_t language,std::uint32_t& actual_working_control40,
                       RetailSavedProfileChildFailure&)noexcept;
 // The direct245DD0 inline CHILD body has the same resulting values/order.
 // Surrounding captured-role/in-use/active metadata effects belong to root.
 bool reset_child_body_from_245dd0(std::int32_t language,std::uint32_t& actual_working_control40,
                                 RetailSavedProfileChildFailure&)noexcept;
 bool import_from_image(const NativeRetailProfileImage&,std::size_t child_index,
                        RetailSavedProfileChildFailure&)noexcept;
 std::optional<RetailProfileAggregateChildView> aggregate_view(RetailSavedProfileChildFailure&)const noexcept;
 // Borrowed views expire on value mutation/move. Stable name-field references
 // must retain the outer child node and resolve these current values each use.
 const RetailSavedProfileDefaults& known_values()const noexcept{return known_;}
 const RetailSavedChildIdentity& identity()const noexcept{return identity_;}
 const RetailSavedChildWallet& wallet()const noexcept{return wallet_;}
 const RetailSavedChildStatistics& statistics()const noexcept{return statistics_;}
 RetailSavedKafView gameplay()const noexcept{return {known_.gameplay,gameplay_};}
 RetailSavedCustomizationView customization()const noexcept{return {known_.command,customization_pending_,customization_condition_};}
 const std::vector<RetailSavedNamedPreset>& named_presets()const noexcept{return named_presets_;}
 std::uint8_t customization_flags()const noexcept{return static_cast<std::uint8_t>(known_.command.flag_bit2);}
 std::uint32_t customization_condition_word()const noexcept{return customization_condition_;}
private:
 bool observable(RetailSavedProfileChildFailure&)const noexcept;
 RetailSavedChildPhase phase_{RetailSavedChildPhase::zero_born};
 RetailSavedProfileDefaults known_;
 RetailSavedChildIdentity identity_;
 RetailSavedChildWallet wallet_;
 RetailSavedChildStatistics statistics_;
 RetailSavedKafSupplement gameplay_;
 std::vector<RetailSavedNamedPreset> named_presets_;
 std::set<std::uint32_t> achievement_flags_;
 std::uint32_t unlock_word_{};
 std::array<std::uint64_t,4> unlock_masks_{};
 std::uint32_t customization_pending_{},customization_condition_{};
};
}
