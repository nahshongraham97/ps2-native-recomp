#include "mka/allocation_profile.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

using namespace mka::memory;
using namespace mka::retail;
namespace {
void check(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::abort(); }
}
struct OriginalDescriptor {
    AllocationClassRole role;
    unsigned source_word;
    std::size_t payload, alignment;
    ClassCallbackPolicy callback;
};
struct OriginalProfiles {
    std::array<OriginalDescriptor, 13> descriptors;
    std::array<std::array<std::uint64_t, 18>, 14> values;
};
OriginalProfiles read_original(const char* path) {
    std::ifstream in(path); check(bool(in), "independent source-derived profile rows open");
    OriginalProfiles result{};
    unsigned count; check(bool(in >> count) && count == result.descriptors.size(), "exact original descriptor count");
    for (unsigned i = 0; i < count; ++i) {
        unsigned role, callback; auto& item = result.descriptors[i];
        check(bool(in >> role >> item.source_word >> item.payload >> item.alignment >> callback) &&
              role == i + 1 && item.source_word < 18 && item.source_word != 7 && callback <= 6,
              "independent original role/count-field/payload/alignment/callback metadata");
        item.role = static_cast<AllocationClassRole>(role); item.callback = static_cast<ClassCallbackPolicy>(callback);
    }
    for (unsigned mode = 0; mode < 14; ++mode) {
        unsigned actual; check(bool(in >> actual) && actual == mode, "exact ordered original selector row");
        for (unsigned word = 0; word < 18; ++word) {
            std::string value; check(bool(in >> value), "complete actual source profile extent");
            if (word == 7) check(value == "unwritten", "source unwritten word never becomes an invented value");
            else result.values[mode][word] = std::stoull(value);
        }
    }
    std::string extra; check(!(in >> extra) && in.eof(), "exact14 source rows only"); return result;
}
void compare_profiles(const char* path) {
    const auto expected = read_original(path);
    std::vector<std::uint32_t> selectors;
    for (std::uint32_t mode = 0; mode < 14; ++mode) selectors.push_back(mode);
    selectors.insert(selectors.end(), {14, 15, 255, 0x7fffffff, 0x80000000, 0xffffffff});
    for (auto mode : selectors) {
        const auto plan = select_native_class_profile(mode);
        const auto resolved = mode < 13 ? mode : 13;
        check(plan.requested_selector == mode && plan.selected_profile == resolved, "unsigned default selection preserves selector provenance");
        check(plan.object_counts.game_pool_records == expected.values[resolved][6] &&
              plan.object_counts.npc_stacks == expected.values[resolved][8] &&
              plan.object_counts.player_collision_objects == expected.values[resolved][9] &&
              plan.object_counts.attack_collision_objects == expected.values[resolved][10],
              "game-pool and companion-class counts match actual original stores without inventing word7");
        for (std::size_t i = 0; i < expected.descriptors.size(); ++i) {
            const auto& source = expected.descriptors[i];
            const auto* item = plan.find(source.role);
            check(item && item == &plan.classes[i] && item->configured_game_count == expected.values[resolved][source.source_word] &&
                  item->payload_bytes == source.payload && item->alignment == source.alignment && item->callback == source.callback,
                  "native class descriptor agrees with independently reconstructed original stores");
        }
        check(!plan.find(AllocationClassRole::unbound) && !plan.find(static_cast<AllocationClassRole>(999)), "unknown class is absent, never defaulted");
    }
}
void real_profile_storage() {
    const auto plan = select_native_class_profile(13); const auto* pfx = plan.find(AllocationClassRole::pfx);
    NativeHeapFailure hf; auto wave = NativeHeapOwner::create("fixture actual Wave", {}, hf);
    auto owner = NativeHeapOwner::create("fixture actual PFX", wave, hf);
    check(wave && owner && pfx, "explicit native profile class binding");
    AllocationClassFailure f;
    auto storage_class = NativeAllocationClass::create({owner, pfx->role, AllocationClassSchema::source_kind2_payload,
        pfx->payload_bytes, pfx->alignment}, f);
    check(bool(storage_class), "actual native class from selected game metadata");
    std::vector<ClassAllocation> allocations;
    for (std::uint64_t i = 0; i < pfx->configured_game_count + 7; ++i) {
        auto allocation = storage_class->allocate(owner, pfx->payload_bytes, pfx->alignment, f);
        check(allocation.has_value(), "source gamecount is not a native slot/arena cap");
        static_cast<unsigned char*>(allocation->data())[pfx->payload_bytes - 1] = static_cast<unsigned char>(i);
        allocations.push_back(std::move(*allocation));
    }
    const auto headroom = storage_class->pfx_headroom(pfx->configured_game_count, f);
    check(headroom && *headroom == -7 && *headroom < 5 &&
          owner->observe().actual_live_allocations == allocations.size() && wave->observe().actual_live_allocations == 0,
          "original signed game predicate uses actual PFX count independently of dynamic backing");
    allocations.clear(); check(storage_class->pfx_headroom(pfx->configured_game_count, f) ==
        std::optional<std::int64_t>{static_cast<std::int64_t>(pfx->configured_game_count)}, "actual releases restore game headroom");
    check(owner->shutdown(hf) && !storage_class->pfx_headroom(pfx->configured_game_count, f) &&
          f.heap.kind == NativeHeapError::inactive_owner, "revoked profile binding cannot report live game readiness");
}
}
int main(int argc, char** argv) {
    check(argc == 2, "independent source-derived profile row argument");
    compare_profiles(argv[1]); real_profile_storage();
    std::puts("PASS_NATIVE_CLASS_PROFILE: 20 selectors/13 classes and game-pool/companion counts match original stores; actual dynamic PFX storage and signed game predicate, no reset/producer/game claim");
    return std::fflush(stdout) || std::ferror(stdout) ? 1 : 0;
}
