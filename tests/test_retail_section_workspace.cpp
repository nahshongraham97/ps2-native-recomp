#include "mka/retail_section_workspace.hpp"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
void check(bool condition, const char* label) {
    if (!condition) {
        std::fprintf(stderr, "retail Section workspace: %s\n", label);
        std::abort();
    }
}
}

int main() {
    using namespace mka::retail;
    RetailMemoryOwnerFailure mf;
    auto group = NativeRetailMemoryOwners::create(mf);
    check(bool(group), "real native owner group");
    RetailSectionWorkspaceFailure failure;
    auto workspace = NativeRetailSectionWorkspace::initialize(*group, failure);
    check(bool(workspace) && failure.kind == RetailSectionWorkspaceError::none,
          "19C4C0 workspace birth");
    auto state = workspace->observe(failure);
    check(state && state->selected_table_index == 0 &&
              state->record_count == 7 &&
              state->active_transient_records == 0 &&
              state->available_transient_records == 0 &&
              state->first_auxiliary_words == std::array<std::uint32_t, 2>{} &&
              state->second_auxiliary_words == std::array<std::uint32_t, 2>{},
          "source-born records, cleared words and native transient state");

    constexpr std::array<std::uint32_t, 7> ids{0, 3, 4, 1, 12, 2, 5};
    constexpr std::array<std::size_t, 7> bytes{
        0x69000, 0x2cb000, 0x2cb000, 0xf7800, 0x11f800, 0x3cf000, 0x1af000};
    constexpr std::array<std::size_t, 7> subcounts{2, 3, 3, 2, 1, 2, 3};
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto record = workspace->record(i, failure);
        check(record && record->logical_id == ids[i] &&
                  record->initial_bytes == bytes[i] &&
                  record->storage_bytes == bytes[i] &&
                  record->subranges.size() == subcounts[i] &&
                  record->storage_owner == state->section_owner,
              "retail table-zero record order and actual Section owner");
        std::size_t end = 0;
        for (const auto& subrange : record->subranges) {
            check(subrange.offset == end, "source successive subrange offset");
            end += subrange.initial_bytes;
        }
        check(end == bytes[i], "source subranges fill initial buffer");
    }
    auto three = workspace->section_storage(3, failure);
    auto four = workspace->section_storage(4, failure);
    check(three && four && three->data() != four->data(),
          "shared descriptor layout has distinct real buffers");
    (*three)[0] = std::byte{0x5a};
    (*three)[three->size() - 1] = std::byte{0xa5};
    const auto new_size = three->size() + 0x12345;
    check(workspace->ensure_section_storage(3, new_size, *group, failure),
          "host section storage grows beyond original extent");
    three = workspace->section_storage(3, failure);
    check(three && three->size() == new_size && (*three)[0] == std::byte{0x5a} &&
              (*three)[bytes[1] - 1] == std::byte{0xa5},
          "growth retains written bytes and exposes new native storage");
    check(workspace->reserve_transient_records(128, failure) &&
              workspace->observe(failure)->available_transient_records == 128,
          "native free-record storage grows past 39 old nodes");

    std::thread other([&] {
        RetailSectionWorkspaceFailure foreign;
        check(!workspace->record(0, foreign) &&
                  foreign.kind == RetailSectionWorkspaceError::wrong_thread,
              "foreign owner access refused");
    });
    other.join();

    check(workspace->reset_for_reentry(*group, failure),
          "source same-table reentry resets auxiliary and free list");
    three = workspace->section_storage(3, failure);
    check(three && three->size() == new_size && (*three)[0] == std::byte{0x5a} &&
              workspace->observe(failure)->available_transient_records == 128,
          "same-table reentry retains real buffer and dynamic free capacity");
    std::puts("native retail Section workspace: PASS");
}
