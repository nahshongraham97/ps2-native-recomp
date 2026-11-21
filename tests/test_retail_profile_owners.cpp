#include "mka/retail_profile_owners.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace memory = mka::memory;
namespace {
std::atomic<long> live_allocations{0};
bool count_attempts = false;
long fail_at = -1, attempts = 0;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}
void* operator new(std::size_t n) {
    if (count_attempts && attempts++ == fail_at) throw std::bad_alloc();
    if (void* result = std::malloc(n ? n : 1)) {
        ++live_allocations;
        return result;
    }
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept {
    if (p) { --live_allocations; std::free(p); }
}
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }

int main() {
    try {
        RetailMemoryOwnerFailure group_failure;
        auto group = NativeRetailMemoryOwners::create(group_failure);
        check(bool(group), "actual native retail memory group created");
        const auto wave = group->observe(RetailMemoryRole::wave, group_failure);
        check(wave && wave->policy == RetailMemoryPolicy::wave_callback,
            "source Wave role comes from actual retained memory group");
        const auto wave_id = wave->heap.identity;
        memory::NativeHeapFailure heap_failure;
        RetailProfileOwnerFailure failure;
        const auto plan = select_native_class_profile(13);
        auto altered = plan;
        altered.classes[5].configured_game_count++;
        check(!NativeRetailProfileOwners::create_initial_storage(altered, *group, failure) &&
            failure.kind == RetailProfileOwnerError::invalid_profile_plan,
            "class plan must come from exact source selector data");
        auto altered_counts = select_native_class_profile(11);
        check(altered_counts.object_counts.game_pool_records == 5000 &&
            altered_counts.object_counts.npc_stacks == 19 &&
            altered_counts.object_counts.player_collision_objects == 19 &&
            altered_counts.object_counts.attack_collision_objects == 6,
            "selector eleven carries the actual four source numeric counts");
        altered_counts.object_counts.game_pool_records++;
        check(!NativeRetailProfileOwners::create_initial_storage(altered_counts, *group, failure) &&
            failure.kind == RetailProfileOwnerError::invalid_profile_plan,
            "altered source numeric count cannot enter class graph construction");

        count_attempts = true;
        auto measured = NativeRetailProfileOwners::create_initial_storage(plan, *group, failure);
        count_attempts = false;
        const long creation_attempts = attempts;
        check(measured && creation_attempts > 0 &&
            group->observe(RetailMemoryRole::wave, group_failure)->heap.child_owners == plan.classes.size(),
            "actual class graph created in source order under Wave");
        measured.reset();
        check(group->observe(RetailMemoryRole::wave, group_failure)->heap.child_owners == 0,
            "retired graph releases all actual child identities");
        for (long cut = 0; cut < creation_attempts; ++cut) {
            attempts = 0; fail_at = cut; count_attempts = true;
            auto failed = NativeRetailProfileOwners::create_initial_storage(plan, *group, failure);
            count_attempts = false; fail_at = -1;
            check(!failed && failure.kind != RetailProfileOwnerError::none &&
                group->observe(RetailMemoryRole::wave, group_failure)->heap.child_owners == 0,
                "every construction allocation cut leaves no partial class graph");
        }

        auto graph = NativeRetailProfileOwners::create_initial_storage(plan, *group, failure);
        check(bool(graph), "actual initial class graph can retry after faults");
        for (const auto& expected : plan.classes) {
            const auto item = graph->observe(expected.role, failure);
            check(item && item->profile.role == expected.role &&
                item->profile.configured_game_count == expected.configured_game_count &&
                item->payload_leaf.configured_payload_bytes == expected.payload_bytes &&
                item->child.parent == wave_id,
                "every source class owns real child and direct payload leaf");
        }
        auto leaf = graph->payload_leaf(memory::AllocationClassRole::pfx, failure);
        check(bool(leaf), "PFX direct payload capability retained");
        auto payload = graph->allocate_direct_payload_leaf(
            memory::AllocationClassRole::pfx, 0x310, failure);
        check(payload && *payload && payload->data() && payload->payload_size() == 0x310,
            "direct PFX class leaf owns real writable native payload");
        auto child_id = payload->actual_owner();
        graph.reset(); group.reset();
        check(payload->actual_owner() == child_id && payload->data(),
            "live payload and capability retain actual child after graph and group retirement");
        check(payload->release(heap_failure), "actual payload releases once");
        payload.reset(); leaf.reset();
        group = NativeRetailMemoryOwners::create(group_failure);
        check(bool(group), "new real group can be created after retained payload retirement");
        bool foreign_refused = false;
        auto again = NativeRetailProfileOwners::create_initial_storage(plan, *group, failure);
        std::thread worker([&] {
            RetailProfileOwnerFailure other;
            foreign_refused = !again->observe(memory::AllocationClassRole::pfx, other) &&
                other.kind == RetailProfileOwnerError::wrong_thread &&
                !again->payload_leaf(memory::AllocationClassRole::pfx, other) &&
                other.kind == RetailProfileOwnerError::wrong_thread;
        });
        worker.join();
        check(foreign_refused && bool(again->observe(memory::AllocationClassRole::pfx, failure)) &&
            failure.kind == RetailProfileOwnerError::none,
            "off-thread refusal does not damage owner-thread access");
        again.reset();
        check(group->observe(RetailMemoryRole::wave, group_failure)->heap.child_owners == 0,
            "final graph releases all child identities");
        std::printf("PASS native retail profile initial class storage: %ld allocation cuts, 13 Wave children; reset/callback/profile effects open\n",
            creation_attempts);
        return 0;
    } catch (const std::exception& e) {
        count_attempts = false; fail_at = -1;
        std::fprintf(stderr, "FAIL native retail profile initial class storage: %s\n", e.what());
        return 1;
    }
}
