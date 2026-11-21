#pragma once

#include "mka/retail_dispatch_owners.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>

namespace mka::retail {

struct NativeRetailMainState;
struct StartupFailure;

enum class RetailRemainingConstructorError { none, allocation, host_capacity, native_exception, descriptor };
struct RetailRemainingConstructorFailure {
    RetailRemainingConstructorError kind = RetailRemainingConstructorError::none;
    RetailDispatchOwnerFailure descriptor;
};

// Source 6489D0 creates a one-byte service record and a distinct 351390
// prefix descriptor. Their later producer/callback contracts remain separate.
class NativeRetailTwoServiceObjects final {
public:
    static std::shared_ptr<NativeRetailTwoServiceObjects> create(
        RetailRemainingConstructorFailure&) noexcept;
    NativeRetailTwoServiceObjects(const NativeRetailTwoServiceObjects&) = delete;
    NativeRetailTwoServiceObjects& operator=(const NativeRetailTwoServiceObjects&) = delete;

    std::uint8_t first_state() const noexcept { return first_state_; }
    NativeRetailPrefixDispatchDescriptor& prefix_descriptor() noexcept { return *prefix_descriptor_; }

private:
    NativeRetailTwoServiceObjects() = default;
    std::uint8_t first_state_{};
    std::shared_ptr<NativeRetailPrefixDispatchDescriptor> prefix_descriptor_;
};

// 648E50 -> 4E00B0 -> 4E00F0(null): the original static object contains
// three cleared words. No MAN payload is read on this constructor path.
struct NativeRetailEmptyBinding final {
    std::uint32_t state{};
    std::uint32_t source_record{};
    std::uint32_t source_cursor{};
};

// 4ACC20 calls 1DEE00 on the original zero-terminated label, then
// 4ACB20 computes MD5 and XORs its four little-endian state words.
std::uint32_t retail_static_key_hash(std::string_view bytes) noexcept;
using RetailStaticKeyObserver = std::function<void(std::string_view, std::uint32_t)>;

struct NativeRetailPairedKey final {
    std::uint32_t group{}, field{};
};
enum class RetailStaticHandlerBody {
    release_other_player, two_player_animation,
    execute_mk_background_script, execute_script_function
};
struct NativeRetailStaticHandler final {
    std::uint32_t key{};
    RetailStaticHandlerBody body{};
};
// Exact initialized rows. A callback may observe each hash before its word is
// written, as in 4ACC20; partial construction cannot be retried.
struct NativeRetailStaticKeyOwners final {
    std::array<std::uint32_t, 4> initial_keys{};
    std::array<NativeRetailStaticHandler, 4> initial_handlers{};
    std::array<NativeRetailPairedKey, 6> first_pairs{};
    std::array<NativeRetailPairedKey, 10> second_pairs{};
    std::array<NativeRetailStaticHandler, 4> first_handlers{};
    std::array<NativeRetailStaticHandler, 4> second_handlers{};
    bool initial_ready{}, first_pairs_ready{}, second_pairs_ready{}, first_handlers_ready{}, second_handlers_ready{};
    bool construction_active{}, construction_failed{};
};

namespace main_binding {
bool construct_retail_two_service_objects(NativeRetailMainState&, StartupFailure&);
bool construct_retail_empty_binding(NativeRetailMainState&, StartupFailure&);
bool construct_retail_key_action_records(NativeRetailMainState&, StartupFailure&);
bool construct_retail_first_paired_key_records(NativeRetailMainState&, StartupFailure&);
bool construct_retail_second_paired_key_records(NativeRetailMainState&, StartupFailure&);
bool construct_retail_first_handler_records(NativeRetailMainState&, StartupFailure&);
bool construct_retail_second_handler_records(NativeRetailMainState&, StartupFailure&);
}

} // namespace mka::retail
