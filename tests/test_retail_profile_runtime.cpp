#include "mka/retail_profile_runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace memory = mka::memory;
namespace {
bool count_attempts = false;
long attempts = 0, fail_at = -1;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
}
void* operator new(std::size_t bytes) {
    if (count_attempts && attempts++ == fail_at) throw std::bad_alloc();
    if (void* result = std::malloc(bytes ? bytes : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }

int main() {
    try {
        RetailMemoryOwnerFailure group_failure;
        RetailProfileRuntimeFailure failure;
        auto group = NativeRetailMemoryOwners::create(group_failure);
        check(bool(group), "real source memory group");
        auto runtime = NativeRetailProfileRuntime::create(*group, failure);
        check(bool(runtime), "profile generation owner over real Wave group");
        const auto wave = group->observe(RetailMemoryRole::wave, group_failure);
        check(wave && wave->policy == RetailMemoryPolicy::wave_callback,
              "Wave policy and identity come from retained memory group");
        auto before = runtime->observe(failure);
        check(before && before->state == RetailProfileGenerationState::empty &&
              before->generation == 0 && !before->numeric &&
              before->wave_classes == 0 &&
              before->wave_identity == wave->heap.identity,
              "cold state has no fabricated numeric record or Wave graph");
        const auto plan13 = select_native_class_profile(13);
        auto altered = plan13;
        ++altered.object_counts.game_pool_records;
        check(!runtime->apply_numeric_and_wave_classes(altered, *group, failure) &&
              failure.kind == RetailProfileRuntimeError::invalid_profile_plan &&
              !runtime->observe(failure)->numeric,
              "altered source numeric count cannot publish");

        attempts = 0; fail_at = -1; count_attempts = true;
        const bool first = runtime->apply_numeric_and_wave_classes(plan13, *group, failure);
        count_attempts = false;
        const long creation_attempts = attempts;
        check(first && creation_attempts > 0, "source thirteen-class graph created");
        auto created = runtime->observe(failure);
        check(created && created->state == RetailProfileGenerationState::ready &&
              created->generation == 1 && created->wave_classes == 13 &&
              created->numeric->object_counts == plan13.object_counts &&
              group->observe(RetailMemoryRole::wave, group_failure)->heap.child_owners == 13,
              "first generation publishes source numeric counts and real Wave children");
        auto retained = runtime->direct_payload(memory::AllocationClassRole::pfx, failure);
        check(bool(retained), "typed PFX direct payload keeps original child identity");
        const auto old_id = retained->child_identity();
        memory::AllocationClassFailure class_failure;
        auto payload = retained->allocate(0x100, class_failure);
        check(payload && *payload && payload->actual_owner() == old_id,
              "real native payload occupies original child");
        const auto plan11 = select_native_class_profile(11);
        check(!runtime->apply_numeric_and_wave_classes(plan11, *group, failure) &&
              failure.kind == RetailProfileRuntimeError::busy &&
              runtime->observe(failure)->generation == 1 &&
              runtime->observe(failure)->numeric->object_counts == plan11.object_counts,
              "source numeric stores precede busy old-class retirement");
        memory::NativeHeapFailure heap_failure;
        check(payload->release(heap_failure), "live old payload released before retirement");
        payload.reset();
        check(runtime->apply_numeric_and_wave_classes(plan11, *group, failure),
              "second generation retires old thirteen before new thirteen");
        auto replaced = runtime->observe(failure);
        check(replaced && replaced->generation == 2 && replaced->wave_classes == 13 &&
              replaced->numeric->object_counts.game_pool_records == 5000 &&
              replaced->numeric->object_counts.npc_stacks == 19 &&
              replaced->numeric->object_counts.player_collision_objects == 19 &&
              replaced->numeric->object_counts.attack_collision_objects == 6 &&
              group->observe(RetailMemoryRole::wave, group_failure)->heap.child_owners == 13,
              "source selector eleven counts and new Wave graph published");
        check(!retained->allocate(0x100, class_failure) &&
              class_failure.kind == memory::AllocationClassError::heap &&
              class_failure.heap.kind == memory::NativeHeapError::inactive_owner,
              "retained old direct leaf refuses post-retirement allocation");
        auto current = runtime->direct_payload(memory::AllocationClassRole::pfx, failure);
        check(current && current->child_identity() != old_id &&
              current->allocate(0x100, class_failure),
              "new generation has a distinct live child and real payload route");
        current.reset();

        auto foreign = NativeRetailMemoryOwners::create(group_failure);
        check(bool(foreign) &&
              !runtime->apply_numeric_and_wave_classes(plan13, *foreign, failure) &&
              failure.kind == RetailProfileRuntimeError::foreign_group &&
              runtime->observe(failure)->generation == 2,
              "foreign actual memory group cannot replace retained owner");
        bool wrong_thread = false;
        std::thread worker([&] {
            RetailProfileRuntimeFailure other;
            wrong_thread = !runtime->observe(other) &&
                other.kind == RetailProfileRuntimeError::wrong_thread &&
                !runtime->apply_numeric_and_wave_classes(plan13, *group, other) &&
                other.kind == RetailProfileRuntimeError::wrong_thread;
        });
        worker.join();
        check(wrong_thread && runtime->observe(failure)->generation == 2,
              "foreign thread cannot inspect or mutate generation");
        runtime.reset();
        retained.reset();
        group.reset();

        for (long cut = 0; cut < creation_attempts; ++cut) {
            auto fault_group = NativeRetailMemoryOwners::create(group_failure);
            auto fault_runtime = NativeRetailProfileRuntime::create(*fault_group, failure);
            check(bool(fault_runtime), "fault fixture born before cut");
            attempts = 0; fail_at = cut; count_attempts = true;
            const bool applied = fault_runtime->apply_numeric_and_wave_classes(
                plan13, *fault_group, failure);
            count_attempts = false; fail_at = -1;
            check(!applied && failure.kind == RetailProfileRuntimeError::profile_owner &&
                  fault_runtime->observe(failure)->state == RetailProfileGenerationState::empty &&
                  fault_runtime->observe(failure)->generation == 0 &&
                  fault_runtime->observe(failure)->numeric->object_counts == plan13.object_counts &&
                  fault_group->observe(RetailMemoryRole::wave, group_failure)->heap.child_owners == 0,
                  "each first-generation allocation cut leaves no partial class graph");
        }

        auto partial_group = NativeRetailMemoryOwners::create(group_failure);
        auto partial_runtime = NativeRetailProfileRuntime::create(*partial_group, failure);
        check(partial_runtime && partial_runtime->apply_numeric_and_wave_classes(
                  plan13, *partial_group, failure), "partial failure fixture first generation");
        auto retired_leaf = partial_runtime->direct_payload(memory::AllocationClassRole::pfx, failure);
        attempts = 0; fail_at = 0; count_attempts = true;
        const bool reaped = partial_runtime->apply_numeric_and_wave_classes(
            plan11, *partial_group, failure);
        count_attempts = false; fail_at = -1;
        check(!reaped && failure.kind == RetailProfileRuntimeError::partial_irreversible &&
              partial_runtime->observe(failure)->state ==
                  RetailProfileGenerationState::partial_irreversible &&
              !partial_runtime->observe(failure)->wave_classes &&
              partial_runtime->observe(failure)->generation == 1 &&
              !retired_leaf->allocate(0x100, class_failure) &&
              class_failure.heap.kind == memory::NativeHeapError::inactive_owner &&
              !partial_runtime->apply_numeric_and_wave_classes(plan13, *partial_group, failure) &&
              failure.kind == RetailProfileRuntimeError::partial_irreversible,
              "successor allocation failure after retirement latches partial irreversible state");
        std::printf("PASS native retail profile generation prerequisite: %ld first-generation allocation cuts; full reset/callback/object effects open\n",
                    creation_attempts);
        return 0;
    } catch (const std::exception& error) {
        count_attempts = false; fail_at = -1;
        std::fprintf(stderr, "FAIL native retail profile generation prerequisite: %s\n", error.what());
        return 1;
    }
}
