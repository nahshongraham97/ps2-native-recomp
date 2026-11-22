// Proposed API for root reconciliation, not compiled/runtime implementation yet.
#pragma once
#include "mka/retail_saved_profile_defaults.hpp"
#include "mka/retail_profile_aggregate.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace mka::retail {
// A VALUE owned by root's stable actual child/active record node. No Session,
// aggregate cache, active/staging resource-ID owner or guest record is duplicated.
// Node identity/thread/lifetime/publication stay in root's actual domain.
enum class RetailSavedChildPhase { zero_born, resetting, source_defaults, failed_reset };
struct RetailSavedChildIdentity {
 std::array<std::uint8_t,6> entry_code{};
 std::uint8_t entry_attribute{};
 std::uint32_t signature{}, aggregate_gate{};
 // The one name/initialized value belongs to the reused known-default storage.
};
struct RetailSavedChildWallet {std::uint32_t current{}, accumulated{};};
struct RetailSavedCounterPair {std::uint32_t first{},second{};};
struct RetailSavedChildStatistics {
 RetailSavedCounterPair first_mode,second_mode,third_mode;
 // Native absence for five packed result slots whose source high word is zero.
 // Populated result decoder must prove the real packed fields before admission.
 std::vector</* concrete typed timed result, pending source proof */ std::uint64_t> timed_results;
};
struct RetailSavedNamedPreset {
 std::string first_name,second_name;
 std::vector<std::string> command_text;
};
// Per-child VALUES. These are not live actor/scene owners. Populated typed
// payloads stay unavailable until their real decoders/producers are supplied.
struct RetailSavedKafSupplement {
 // Explicit flags/phase, triples, control/seed/backup values and eight menu
 // indices named by FIELD_GROUPS.json, excluding the nine authoritative known
 // scalar fields already stored in RetailSavedProfileDefaults::gameplay.
 // This comment is not a proposed generic word array or an implemented type.
};
class RetailSavedProfileChildValue final {
public:
 RetailSavedProfileChildValue(); // zero-BSS semantic values,not reset defaults
 RetailSavedChildPhase phase()const noexcept;
 // Pure leaf mutation. Root passes the ONE actual Working control-word field;
 // null-source targets are handled before calling this reference-taking leaf.
 // No Working resource-ID/aggregate/live-scene side effect is implied.
 bool reset_from_245cb0(std::int32_t language,std::uint32_t& actual_working_control,
                       RetailSavedProfileDefaultFailure&) noexcept;
 RetailProfileAggregateChildView aggregate_view()const noexcept;
 const RetailSavedProfileDefaults& known_values()const noexcept;
 // Typed groups are immutable to callers; root actual node owns mutations.
 const RetailSavedChildIdentity& identity()const noexcept;
 const RetailSavedChildWallet& wallet()const noexcept;
 const RetailSavedChildStatistics& statistics()const noexcept;
 // No byte/offset read,source pointer setter or "load as default" API.
private:
 RetailSavedChildPhase phase_{RetailSavedChildPhase::zero_born};
 RetailSavedProfileDefaults known_; // zeroed explicitly for BSS birth
 RetailSavedChildIdentity identity_;
 RetailSavedChildWallet wallet_;
 RetailSavedChildStatistics statistics_;
 std::vector<RetailSavedNamedPreset> named_presets_; // empty at BSS/reset
 // Actual sparse/shared flag state and complete KAF scalar groups still to add.
};
// Loaded import remains a genuine required operation until all native groups
// have an actual typed decoder. No definition/default fallback is proposed.
}
