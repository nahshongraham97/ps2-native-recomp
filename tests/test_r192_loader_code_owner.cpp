// Actual original loader/byte-restore versus the bounded identity connector.
// No module entry, guest instruction, card or game is executed here.
#include R192_OWNER_FIXTURE_PREFIX
#include "iop/r192_loader_code_owner.hpp"
#include "ps2/state.hpp"
#include <cstring>
namespace lo = ps2::iop::loader_owner;
#include R192_OWNER_SOURCES
namespace {
unsigned loader_calls{}, negative_checks{};
lo::Facts facts(int id) {
    const auto &m = kernel::g_modules.at(id);
    return {m.id, m.name, m.base, m.size, m.entry, m.gp};
}
std::vector<lo::Facts> module_facts() {
    std::vector<lo::Facts> result;
    for (const auto &[id, m] : kernel::g_modules) result.push_back(facts(id));
    return result;
}
std::vector<std::uint8_t> read_file(const char *path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("input_unreadable");
    return {std::istreambuf_iterator<char>(in), {}};
}
void check(bool value, const char *reason) { if (!value) throw std::runtime_error(reason); }
template <typename Fn> void refused(const char *reason, Fn operation) {
    try { operation(); } catch (const std::exception &e) {
        if (std::string(e.what()) != reason) throw std::runtime_error(std::string("unexpected_refusal_reason:") + e.what());
        ++negative_checks; return;
    }
    throw std::runtime_error("planted_operation_not_refused");
}
void cancel_unstarted() {
    std::vector<int> tids;
    for (const auto &[id, t] : kernel::g_threads) if (t.on_exit) tids.push_back(id);
    for (int tid : tids) {
        Cpu control{}; control.r[4] = static_cast<std::uint32_t>(tid);
        kernel::g_hle.at(kernel::g_hle_ids.at({"thbase", 10})).function(control);
        check(!control.r[2] && kernel::delete_thread(tid), "unstarted_loader_thread_cancellation_failed");
    }
}
void write_file(const std::string &path, const void *bytes, std::size_t size) {
    std::ofstream out(path, std::ios::binary); out.write(static_cast<const char *>(bytes), static_cast<std::streamsize>(size));
    check(static_cast<bool>(out), "owned_output_write_failed");
}
}
int main(int argc, char **argv) {
    if (argc != 7) return 2;
    const std::string mode = argv[1], route = argv[3], scenario = argv[4], output = argv[5];
    const bool connected = mode == "connector";
    std::string failure; unsigned entries{}, restored_candidates{}; bool partial_changed{}, is_poisoned{};
    try {
        check(mode == "reference" || connected, "unknown_mode");
        check(ps2::g_options.memcard_path.empty(), "card_path_not_empty");
        std::memset(g_ram, 0, kRamSize); kernel::boot();
        const auto bytes = read_file(argv[2]); const auto digest = lo::image_sha256(bytes);
        const auto found = std::find_if(original_sources.begin(), original_sources.end(), [&](const auto &s) { return s.digest == digest; });
        check(found != original_sources.end(), "original_fixture_not_in_catalogue");
        auto source = *found; // Owned test catalogue, exact original schemas.
        const std::string path = route == "buffer" ? "buffer" : source.filename_name + ".IRX";
        auto loader = [&](const auto &data, const auto &name, const auto &args) {
            ++loader_calls; const auto id = load_module_buffer(data, name, args);
            if (scenario == "overlap_admission") {
                const auto m = facts(id);
                kernel::g_modules[id + 100] = kernel::Module{id + 100, "PLANTED_STALE", m.base + 0x100, 0x100, m.entry, m.gp};
            }
            if (scenario == "post_mutation_failure") throw std::bad_alloc();
            return id;
        };
        auto read = [](std::uint32_t address, unsigned width) {
            const auto at = phys(address);
            if (at >= kRamSize || width > kRamSize - at) throw std::runtime_error("read_bounds");
            std::uint32_t value{}; std::memcpy(&value, g_ram + at, width); return value;
        };
        auto actual_facts = [&](int id) { auto f = facts(id); if (scenario == "wrong_facts") f.entry += 4; return f; };
        lo::Session session(original_sources, read, {nullptr, kernel::identity, nullptr}, loader, actual_facts, module_facts);
        lo::OwnerHandle handle{}; lo::EntryIdentity old_entry{};
        if (scenario == "wrong_image") {
            auto mutant = bytes; mutant.back() ^= 1;
            refused("unknown_original_image_before_loader", [&] { session.load(mutant, path, ""); });
            check(loader_calls == 0 && !session.poisoned() && session.admitted_owners() == 0, "wrong_image_changed_loader_state");
        } else if (scenario == "wrong_facts") {
            refused("actual_loaded_facts_not_original_source", [&] { session.load(bytes, path, ""); });
            check(session.poisoned(), "wrong_loaded_facts_did_not_poison");
        } else if (scenario == "overlap_admission") {
            refused("overlapping_loaded_module_facts", [&] { session.load(bytes, path, ""); });
            check(session.poisoned(), "overlapping_admission_did_not_poison");
        } else if (scenario == "post_mutation_failure") {
            try { session.load(bytes, path, ""); throw std::runtime_error("allocation_failure_not_injected"); }
            catch (const std::bad_alloc &) { ++negative_checks; }
            check(loader_calls == 1 && !kernel::g_modules.empty() && session.poisoned(), "post_mutation_failure_resumed_or_rolled_back");
        } else {
            int id{};
            if (connected) { handle = session.load(bytes, path, ""); id = kernel::g_next_module - 1; }
            else { id = load_module_buffer(bytes, path, ""); ++loader_calls; }
            check(id > 0 && facts(id).size, "actual_original_loader_failed");
            if (connected) {
                for (const auto &record : source.records) if (record.kind == ps2::iop::metadata_guard::Kind::exports) {
                    for (std::uint32_t ordinal = 0; ordinal < record.words.size() - 6; ++ordinal) {
                        const auto entry = session.export_entry(handle, record.offset, ordinal);
                        const auto proof = session.validate_entry(entry);
                        check(entry.target_offset == record.words[5 + ordinal] && proof.compiled_body == source.code->body &&
                              proof.original_digest == digest && proof.loaded_id == id &&
                              proof.base + entry.target_offset == ram32(proof.base + record.offset + 20 + ordinal * 4), "compiled_original_slot_identity_mismatch");
                        old_entry = entry; ++entries;
                    }
                }
                check(entries, "no_original_export_entries_proved");
                if (scenario == "forged_entry") {
                    auto forged = old_entry; forged.target_offset += 4;
                    refused("forged_compiled_entry_identity", [&] { session.validate_entry(forged); });
                }
                if (scenario == "wrong_body") {
                    auto forged = old_entry; forged.compiled_body = nullptr;
                    refused("forged_compiled_entry_identity", [&] { session.validate_entry(forged); });
                }
                if (scenario == "changed_code") {
                    const auto at = facts(id).base + old_entry.target_offset;
                    const auto saved = ram32(at); set_ram32(at, saved ^ 1);
                    refused("IOP_METADATA_REFUSED:word_identity_changed", [&] { session.validate_entry(old_entry); });
                    set_ram32(at, saved);
                }
                if (scenario == "retire") {
                    refused("module_retirement_lifetime_unbound", [&] { session.retire(handle); });
                    session.validate_entry(old_entry);
                }
                if (scenario == "overlap_revalidation") {
                    const auto m = facts(id);
                    kernel::g_modules[id + 100] = kernel::Module{id + 100, "PLANTED_STALE", m.base + 0x100, 0x100, m.entry, m.gp};
                    refused("overlapping_loaded_module_facts", [&] { session.validate_entry(old_entry); });
                    check(session.poisoned(), "overlapping_revalidation_did_not_poison");
                }
                if (scenario == "reused_owner") {
                    cancel_unstarted();
                    const auto old_base = facts(id).base;
                    kernel::free(old_base); // Actual reference free: leaves g_modules facts intact.
                    refused("overlapping_loaded_module_facts", [&] { session.load(bytes, path, ""); });
                    const auto reloaded = facts(kernel::g_next_module - 1);
                    check(reloaded.id != id && reloaded.base == old_base && kernel::g_modules.count(id), "actual_allocator_reuse_not_observed");
                    aot::BoundPic same_code;
                    check(same_code.bind(*source.code, old_base) && aot::matches(same_code.module, {nullptr, kernel::identity, nullptr}),
                          "reused_source_code_was_not_identical");
                    refused("owner_session_poisoned_requires_rebuild", [&] { session.validate_entry(old_entry); });
                    check(session.poisoned(), "reused_old_owner_resumed");
                }
            }
            cancel_unstarted();
            if (scenario == "restore" || scenario == "partial_restore") {
                auto state = read_file(argv[6]);
                // Let the real parser restore IOP then fail on the final end
                // tag; no hand-computed IOP offset or tag scanning is used.
                if (scenario == "partial_restore") { check(!state.empty(), "empty_state"); state.pop_back(); }
                const auto before = ram_hash();
                auto restore = [&] {
                    if (connected) refused("stale_code_owner_generation", [&] { session.validate_entry(old_entry); });
                    return ps2::load_state(state.data(), state.size()); // Actual bytes overload, no card note.
                };
                const bool success = connected ? session.restore(restore) : restore();
                partial_changed = before != ram_hash();
                check(success == (scenario == "restore"), "actual_parser_success_unexpected");
                if (connected && success) {
                    refused("stale_code_owner_generation", [&] { session.validate_entry(old_entry); });
                    check(session.admitted_owners() == 0, "restored_metadata_invented_source_owner");
                    for (const auto &[key, m] : kernel::g_modules) if (m.size) {
                        const auto candidates = session.restored_range_candidates(m.base);
                        std::size_t expected{};
                        for (const auto &[other_id, other] : kernel::g_modules)
                            expected += other.size && other.base <= m.base && m.base - other.base < other.size;
                        check(candidates.size() == expected, "restored_owner_ambiguity_was_removed");
                        restored_candidates = std::max(restored_candidates, static_cast<unsigned>(candidates.size()));
                    }
                } else if (connected) {
                    check(session.poisoned() && partial_changed, "failed_progressive_restore_did_not_poison");
                    refused("owner_session_poisoned_requires_rebuild", [&] { session.validate_entry(old_entry); });
                    refused("owner_session_poisoned_requires_rebuild", [&] { session.load(bytes, path, ""); });
                }
            }
        }
        cancel_unstarted();
        write_file(output + ".cpu", &g_cpu, sizeof g_cpu);
        write_file(output + ".ram", g_ram, kRamSize);
        // Actual IOP serializer, excluding only explicitly cancelled unstarted
        // fixture init callbacks; no fabricated on_exit/module boot completion.
        ps2::state::Writer writer; writer.set_version(8); ps2::iop::state(writer);
        write_file(output + ".iop-state", writer.out.data(), writer.out.size());
        is_poisoned = session.poisoned();
        check(ps2::g_options.memcard_path.empty() && forbidden_interpreter_calls == 0, "execution_or_card_path_used");
    } catch (const std::exception &e) { failure = e.what(); }
    std::ofstream result(output + ".json");
    result << "{\"status\":\"" << (failure.empty() ? "PASS" : "REFUSED") << "\",\"failure\":\"" << failure
           << "\",\"classification\":\"loader identity capability only\",\"native_credit\":0,\"loader_calls\":" << loader_calls
           << ",\"export_entry_identities\":" << entries << ",\"negative_checks\":" << negative_checks
           << ",\"restored_max_range_candidates\":" << restored_candidates << ",\"partial_restore_changed_ram\":" << (partial_changed ? "true" : "false")
           << ",\"session_poisoned\":" << (is_poisoned ? "true" : "false") << ",\"executed_instructions\":" << forbidden_interpreter_calls << "}\n";
    return failure.empty() ? 0 : 1;
}
