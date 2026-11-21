#include "mka/retail_section_workspace.hpp"

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <thread>

namespace {
bool fail_next_allocation = false;
void check(bool value, const char* label) {
    if (!value) {
        std::fprintf(stderr, "retail Section selection: %s\n", label);
        std::abort();
    }
}
}

void* operator new(std::size_t n) {
    if (fail_next_allocation) {
        fail_next_allocation = false;
        throw std::bad_alloc();
    }
    if (auto* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

int main() {
    using namespace mka::retail;
    RetailMemoryOwnerFailure memory_failure;
    auto memory = NativeRetailMemoryOwners::create(memory_failure);
    check(bool(memory), "real Section heap owner");
    RetailSectionWorkspaceFailure f;
    auto workspace = NativeRetailSectionWorkspace::initialize(*memory, f);
    check(bool(workspace), "source table-zero birth");
    auto first = workspace->section_storage(0, f);
    check(first && first->size() == 0x69000, "shared first descriptor storage");
    auto* retained = first->data();
    (*first)[0] = std::byte{0x5a};
    auto prior = workspace->observe(f);
    check(prior && prior->selected_table_index == 0 && prior->record_count == 7,
          "source table-zero selection");

    check(workspace->select_from_19c550(0, *memory, f),
          "same-pointer source return");
    first = workspace->section_storage(0, f);
    check(first && first->data() == retained && (*first)[0] == std::byte{0x5a},
          "same-table return preserves real allocation");
    constexpr auto grown_first = std::size_t{0x69000 + 0x101};
    check(workspace->ensure_section_storage(0, grown_first, *memory, f),
          "shared source record accepts larger native storage");
    first = workspace->section_storage(0, f);
    check(first && first->size() == grown_first &&
          (*first)[0] == std::byte{0x5a},
          "growth retains actual Section content");
    retained = first->data();

    std::thread foreign([&] {
        RetailSectionWorkspaceFailure ff;
        check(!workspace->select_from_19c550(3, *memory, ff) &&
              ff.kind == RetailSectionWorkspaceError::wrong_thread,
              "foreign selection refused before effects");
    });
    foreign.join();
    check(!workspace->select_from_19c550(2, *memory, f) &&
          f.kind == RetailSectionWorkspaceError::unsupported_selection &&
          workspace->observe(f)->record_count == 7,
          "unproved table selection is named refusal");

    check(workspace->select_from_19c550(3, *memory, f),
          "source 18D970 selects table three");
    auto selected = workspace->observe(f);
    check(selected && selected->selected_table_index == 3 &&
          selected->record_count == 3 &&
          selected->section_owner == prior->section_owner,
          "table three retains Section heap identity");
    constexpr std::array<std::uint32_t, 3> ids{0, 8, 23};
    constexpr std::array<std::size_t, 3> extents{0x69000, 0x799000, 0x19000};
    constexpr std::array<std::size_t, 3> subcounts{2, 2, 1};
    for (std::size_t i = 0; i < ids.size(); ++i) {
        auto record = workspace->record(i, f);
        check(record && record->logical_id == ids[i] &&
              record->initial_bytes == extents[i] &&
              record->storage_bytes == (i == 0 ? grown_first : extents[i]) &&
              record->subranges.size() == subcounts[i],
              "original table-three descriptor and native storage");
        std::size_t end = 0;
        for (const auto& sub : record->subranges) {
            check(sub.offset == end, "source subrecord offset order");
            end += sub.initial_bytes;
        }
        check(end == extents[i], "subrecords fill source descriptor");
    }
    auto zero = workspace->section_storage(0, f);
    check(zero && zero->data() == retained && (*zero)[0] == std::byte{0x5a},
          "shared ordinal-zero allocation and bytes survive retirement");
    check(!workspace->find_record(3, f) &&
          f.kind == RetailSectionWorkspaceError::invalid_record,
          "retired old record absent");
    auto eight = workspace->section_storage(8, f);
    check(eight && eight->size() == 0x799000,
          "table-three new record has real native allocation");
    auto* same_eight = eight->data();
    check(workspace->select_from_19c550(3, *memory, f) &&
          workspace->section_storage(8, f)->data() == same_eight,
          "repeated table-three selection is source no-op");

    check(workspace->reset_for_reentry(*memory, f),
          "19C4C0 selects table zero again on Main reentry");
    auto reset = workspace->observe(f);
    zero = workspace->section_storage(0, f);
    check(reset && reset->selected_table_index == 0 &&
          reset->record_count == 7 && zero && zero->data() == retained &&
          (*zero)[0] == std::byte{0x5a},
          "reverse transition retains shared first storage");

    check(workspace->mark_loaded_subrecords_unbound_from_source(3, f),
          "later real loader marks populated retiring record");
    check(!workspace->select_from_19c550(3, *memory, f) &&
          f.kind == RetailSectionWorkspaceError::loaded_resource_retirement_unbound,
          "populated 19C940 child cleanup cannot silently disappear");
    check(workspace->observe(f)->selected_table_index == 0 &&
          workspace->find_record(3, f).has_value(),
          "unbound child refusal retains original table and owner");

    // The table-zero vector already has capacity seven. Source publishes the
    // new table and retires divergent records before the first new native
    // descriptor allocation. Force that precise after-effect failure.
    auto fault_workspace = NativeRetailSectionWorkspace::initialize(*memory, f);
    check(bool(fault_workspace), "independent fault-cut workspace");
    auto fault_first = fault_workspace->section_storage(0, f);
    check(bool(fault_first), "retained prefix before fault");
    auto* fault_prefix = fault_first->data();
    fail_next_allocation = true;
    check(!fault_workspace->select_from_19c550(3, *memory, f) &&
          !fail_next_allocation &&
          f.kind == RetailSectionWorkspaceError::allocation,
          "real allocation failure after selected-table publication");
    auto partial = fault_workspace->observe(f);
    check(partial && partial->selected_table_index == 3 &&
          partial->record_count == 1,
          "published partial transition remains visible for diagnostics");
    auto prefix_view = fault_workspace->record(0, f);
    check(prefix_view && prefix_view->logical_id == 0 &&
          prefix_view->storage_bytes == 0x69000 &&
          fault_prefix != nullptr,
          "shared prefix remains owned after partial transition");
    check(!fault_workspace->select_from_19c550(3, *memory, f) &&
          f.kind == RetailSectionWorkspaceError::incomplete_selection,
          "same-table retry refuses partial state");
    check(!fault_workspace->select_from_19c550(0, *memory, f) &&
          f.kind == RetailSectionWorkspaceError::incomplete_selection,
          "different-table retry refuses partial state");
    check(!fault_workspace->reset_for_reentry(*memory, f) &&
          f.kind == RetailSectionWorkspaceError::incomplete_selection,
          "reentry cannot conceal partial selection");
    check(!fault_workspace->ensure_section_storage(0, 0x69001, *memory, f) &&
          f.kind == RetailSectionWorkspaceError::incomplete_selection &&
          !fault_workspace->reserve_transient_records(64, f) &&
          f.kind == RetailSectionWorkspaceError::incomplete_selection &&
          !fault_workspace->mark_loaded_subrecords_unbound_from_source(0, f) &&
          f.kind == RetailSectionWorkspaceError::incomplete_selection &&
          !fault_workspace->section_storage(0, f) &&
          f.kind == RetailSectionWorkspaceError::incomplete_selection,
          "all exposed mutation routes refuse incomplete selection");
    check(fault_workspace->observe(f)->selected_table_index == 3 &&
          fault_workspace->observe(f)->record_count == 1,
          "refused retry leaves partial identity and count unchanged");
    std::puts("native retail Section selection: PASS");
}
