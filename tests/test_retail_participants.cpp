#include "mka/retail_participants.hpp"

#include <cstdio>
#include <cstdlib>
#include <new>
#include <stdexcept>
#include <thread>

using namespace mka::retail;
namespace {
bool fail_allocation = false;
int allocations_until_failure = -1;
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
} // namespace

void* operator new(std::size_t size) {
    if (fail_allocation || allocations_until_failure == 0) throw std::bad_alloc();
    if (allocations_until_failure > 0) --allocations_until_failure;
    if (void* result = std::malloc(size ? size : 1)) return result;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }

int main() {
    try {
        RetailParticipantFailure failure;
        fail_allocation = true;
        auto failed = NativeRetailParticipants::create_first_two(failure);
        fail_allocation = false;
        check(!failed && failure.kind == RetailParticipantError::allocation,
            "failed native allocation cannot publish a participant collection");
        bool saw_success = false;
        for (int cut = 0; cut != 16; ++cut) {
            allocations_until_failure = cut;
            auto trial = NativeRetailParticipants::create_first_two(failure);
            allocations_until_failure = -1;
            if (trial) {
                saw_success = true;
                break;
            }
            check(failure.kind == RetailParticipantError::allocation,
                "every owner or control-block allocation cut remains a named failure");
        }
        check(saw_success, "constructor eventually succeeds after all allocation cuts");

        auto participants = NativeRetailParticipants::create_first_two(failure);
        check(bool(participants), "two source-constructed participants exist");
        const auto first = participants->first_two(0, failure);
        const auto second = participants->first_two(1, failure);
        check(bool(first) && bool(second) && first != second,
            "first and second source identities have distinct stable owners");
        for (const auto& participant : {first, second}) {
            const auto state = participant->observe(failure);
            check(state && state->assigned_role == -1 && state->ordinal == 5 &&
                state->game_state == 0 && state->source_float_0c_bits == 0x3f800000 &&
                state->source_word_2c == 0x40 && !state->has_fighter &&
                state->source_word_44 == 3,
                "137810 participant birth values match the original words");
            for (auto word : state->initial_game_words)
                check(word == 0, "137810 first zero group");
            for (auto word : state->linked_game_words)
                check(word == 0, "137810 linked zero group");
        }
        check(!participants->first_two(2, failure) &&
            failure.kind == RetailParticipantError::invalid_identity,
            "mode-indexed third record is not fabricated by boot");

        check(first->set_game_state(7, failure) &&
            second->set_game_state(9, failure), "1319E0 separate state writes");
        check(participants->reconstruct_first_two_after_detach(failure),
            "source-ordered 137810 reconstruction");
        check(participants->first_two(0, failure) == first &&
            participants->first_two(1, failure) == second,
            "reconstruction retains participant identity");
        const auto first_after = first->observe(failure);
        const auto second_after = second->observe(failure);
        check(first_after && second_after && first_after->game_state == 7 &&
            second_after->game_state == 9 && first_after->assigned_role == -1 &&
            second_after->assigned_role == -1,
            "137810 does not overwrite separately written +8 state");

        bool wrong_thread = false, collection_wrong_thread = false;
        std::thread worker([&] {
            RetailParticipantFailure other;
            wrong_thread = !first->observe(other) &&
                other.kind == RetailParticipantError::wrong_thread;
            collection_wrong_thread = !participants->first_two(0, other) &&
                other.kind == RetailParticipantError::wrong_thread &&
                !participants->reconstruct_first_two_after_detach(other) &&
                other.kind == RetailParticipantError::wrong_thread &&
                !first->set_game_state(0, other) &&
                other.kind == RetailParticipantError::wrong_thread;
        });
        worker.join();
        check(wrong_thread && collection_wrong_thread,
            "participant and collection mutations remain on the owner thread");
        participants.reset();
        const auto retained = first->observe(failure);
        check(retained && retained->game_state == 7,
            "a retained participant survives collection destruction with stable state");
        std::puts("PASS native participant birth and menu reconstruction; no game or endpoint");
        return 0;
    } catch (const std::exception& error) {
        fail_allocation = false;
        std::fprintf(stderr, "FAIL retail participants: %s\n", error.what());
        return 1;
    }
}
