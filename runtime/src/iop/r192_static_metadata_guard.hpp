#pragma once
// Experimental reference-route guard. Exact expected words are updated by a
// checked native-model projection; no text masks or arbitrary pointer writes.
#include "iop/r192_pic.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <stdexcept>
#include <set>
#include <string>
#include <vector>

namespace ps2::iop::metadata_guard {
enum class Kind { exports, client, registry_root };
enum class Operation { registration, release, link, unlink };
struct OwnerHandle { std::size_t index{}; std::uint64_t generation{}; };
struct RecordHandle { std::size_t index{}; std::uint64_t generation{}; };
struct Write { std::uint32_t address{}, width{}, value{}; bool operator==(const Write &) const = default; };
class Guard {
public:
    using Read = std::function<std::uint32_t(std::uint32_t, unsigned)>;
    std::function<bool(std::uint32_t, std::uint32_t)> retirement_lifetime_closed;
    explicit Guard(Read read) : read_(std::move(read)) {}
    OwnerHandle bind_owner(const r192_aot::Module &module, const r192_aot::Services &legacy, std::size_t authenticated_record_count) {
        ready(); if (active_) fail("bind_during_transition");
        if (!r192_aot::matches(module, legacy)) fail("initial_original_module_identity");
        for (const auto &o : owners_) if (o.live && module.base < o.module.base + o.module.total_size && o.module.base < module.base + module.total_size)
            fail("overlapping_live_owner");
        Owner o; o.module = module; o.legacy = legacy; o.live = true; o.generation = ++generation_; o.record_count = authenticated_record_count;
        for (std::uint32_t at = 0; at < module.text_size; at += 4) o.words.push_back(read_(module.base + at, 4));
        for (std::uint32_t i = 0; i < module.import_count; ++i) {
            const auto offset = module.imports[i].offset;
            o.canonical_imports.emplace(offset, std::array<std::uint32_t, 2>{module.original_words[offset / 4], module.original_words[offset / 4 + 1]});
        }
        owners_.push_back(std::move(o)); return {owners_.size() - 1, generation_};
    }
    // canonical is generated from the pinned original descriptor/relocations,
    // not inferred from mutable runtime bytes. Import bodies may already have
    // only the separately authenticated frozen-loader patches.
    RecordHandle bind_record(OwnerHandle owner, Kind kind, std::uint32_t offset,
                             std::vector<std::uint8_t> canonical) {
        auto &o = get(owner); ready(); if (active_) fail("bind_during_transition");
        if (offset > o.module.total_size || canonical.size() > o.module.total_size - offset || (offset & 3u)) fail("record_bounds");
        if (kind == Kind::registry_root ? canonical.size() != 16 : canonical.size() < 28) fail("record_layout");
        const auto base = o.module.base + offset;
        for (const auto &r : records_) if (live(r) && base < r.base + r.bytes.size() && r.base < base + canonical.size()) fail("overlapping_records");
        Record r; r.owner = owner; r.kind = kind; r.base = base; r.canonical = canonical; r.bytes = canonical;
        if (kind != Kind::registry_root) {
            if (word(canonical, 0, 4) != (kind == Kind::exports ? 0x41C00000u : 0x41E00000u) || word(canonical, 4, 4)) fail("canonical_record_header");
            if (kind == Kind::exports) {
                if ((canonical.size() - 24) % 4 || word(canonical, canonical.size() - 4, 4)) fail("export_termination");
                r.count = (canonical.size() - 24) / 4;
                for (std::size_t i = 0; i < r.count; ++i) {
                    const auto target = word(canonical, 20 + i * 4, 4);
                    if ((target & 3u) || target < o.module.base || target - o.module.base >= o.module.text_size) fail("original_export_target");
                }
            } else {
                if ((canonical.size() - 28) % 8 || word(canonical, canonical.size() - 8, 4) || word(canonical, canonical.size() - 4, 4)) fail("client_termination");
                r.count = (canonical.size() - 28) / 8;
                for (std::size_t i = 0; i < r.count; ++i)
                    if (word(canonical, 20 + 8 * i, 4) != 0x03E00008u || (word(canonical, 24 + 8 * i, 4) & 0xFFFF0000u) != 0x24000000u) fail("original_client_stub");
            }
        }
        for (std::size_t i = 0; i < canonical.size(); ++i) {
            const bool stub_body = kind == Kind::client && i >= 20 && i < 20 + 8 * r.count;
            const auto actual = read_(base + static_cast<std::uint32_t>(i), 1);
            if (!stub_body && actual != canonical[i]) fail("initial_record_identity");
            r.bytes[i] = static_cast<std::uint8_t>(actual);
        }
        records_.push_back(std::move(r)); return {records_.size() - 1, owner.generation};
    }
    void seal_owner(OwnerHandle owner) {
        auto &o = get(owner); ready(); std::size_t count{};
        for (const auto &r : records_) if (live(r) && r.owner.index == owner.index) ++count;
        if (count != o.record_count) fail("incomplete_authenticated_schema");
        o.sealed = true; verify();
        for (const auto &r : records_) if (live(r) && r.owner.index == owner.index && r.kind == Kind::exports)
            for (std::size_t i = 0; i < r.count; ++i) if (!permits_entry(owner, word(r.canonical, 20 + 4 * i, 4))) fail("export_points_to_data");
    }
    std::uint32_t address(RecordHandle record) const { return get(record).base; }
    void verify() const {
        ready();
        for (const auto &o : owners_) if (o.live)
            for (std::size_t i = 0; i < o.words.size(); ++i)
                if (read_(o.module.base + static_cast<std::uint32_t>(i * 4), 4) != o.words[i]) fail("word_identity_changed");
        for (const auto &r : records_) if (live(r))
            for (std::size_t i = 0; i < r.bytes.size(); ++i)
                if (read_(r.base + static_cast<std::uint32_t>(i), 1) != r.bytes[i]) fail("record_identity_changed");
    }
    bool permits_entry(OwnerHandle owner, std::uint32_t pc) const {
        const auto &o = get(owner); verify(); if (!o.sealed) fail("unsealed_owner"); const auto physical = pc & 0x1FFFFFFFu;
        if (active_ && owner.index != root().owner.index) fail("transition_context_restore_unbound");
        if ((pc & 3u) || physical < o.module.base || physical - o.module.base >= o.module.text_size) return false;
        for (const auto &r : records_) if (live(r) && physical >= r.base && physical - r.base < r.bytes.size()) {
            const auto at = physical - r.base;
            if (r.kind != Kind::client || at < 20 || at >= 20 + 8 * r.count) return false;
        }
        return true;
    }
    void decorate(OwnerHandle owner, r192_aot::BoundPic &bound) const {
        const auto &o = get(owner); verify(); if (!o.sealed) fail("unsealed_owner");
        if (bound.module.base != o.module.base || bound.module.text_size != o.module.text_size || bound.words.size() != o.words.size()) fail("view_owner_mismatch");
        std::copy(o.words.begin(), o.words.end(), bound.words.begin());
        // Preserve the old descriptor's canonical import-schema invariant.
        // Live patched words are independently authenticated by import_identity.
        for (const auto &[offset, words] : o.canonical_imports) {
            bound.words[offset / 4] = words[0]; bound.words[offset / 4 + 1] = words[1];
        }
    }
    bool import_identity(OwnerHandle owner, std::uint32_t address, const r192_aot::Import &entry) const {
        const auto &o = get(owner); verify();
        if (!entry.library || entry.ordinal > 0xFFFFu) return false;
        if (address != o.module.base + entry.offset || address + 8 > o.module.base + o.module.text_size) return false;
        const auto canonical = o.canonical_imports.find(entry.offset);
        if (canonical == o.canonical_imports.end() || canonical->second[1] != (0x24000000u | entry.ordinal)) return false;
        for (const auto &r : records_) if (live(r) && r.owner.index == owner.index && r.kind == Kind::client &&
            address >= r.base + 20 && address < r.base + 20 + 8 * r.count && (address - r.base - 20) % 8 == 0) {
            std::string name;
            for (unsigned i = 12; i < 20 && r.canonical[i]; ++i) name.push_back(static_cast<char>(r.canonical[i]));
            if (name != entry.library) return false;
            const auto slot = (address - r.base - 20) / 8;
            const auto first = word(r.bytes, 20 + 8 * slot, 4), second = word(r.bytes, 24 + 8 * slot, 4);
            if ((first >> 26) == 2u && second == canonical->second[1] && registered_target(r, slot, first)) return true;
        }
        return o.legacy.validate_import && o.legacy.validate_import(o.legacy.user, address, entry);
    }
    void begin(Operation operation, RecordHandle table, std::vector<Write> plan) {
        ready(); if (active_) fail("nested_transition"); verify();
        if (get(table).kind != Kind::exports) fail("transition_table_role");
        operation_ = operation; table_ = table; plan_ = std::move(plan); cursor_ = 0; active_ = true;
    }
    // Called before the actual write. The queue comes from the independently
    // tested native model, and every item must also satisfy the static schema.
    void observe_write(std::uint32_t address, std::uint32_t width, std::uint32_t value) {
        try { observe_write_impl(address, width, value); } catch (...) { poisoned_ = true; throw; }
    }
private:
    void observe_write_impl(std::uint32_t address, std::uint32_t width, std::uint32_t value) {
        ready();
        Record *record{};
        for (auto &r : records_) if (live(r) && address >= r.base && address < r.base + r.bytes.size()) { record = &r; break; }
        if (!record) {
            if (active_) for (const auto &o : owners_) if (o.live && address >= o.module.base && address < o.module.base + o.module.total_size)
                poison("unowned_module_write");
            return; // Original reference stack/cache transport outside scope.
        }
        if (!active_ || cursor_ >= plan_.size() || plan_[cursor_] != Write{address, width, value}) poison("unplanned_metadata_write");
        auto &r = *record; const auto at = address - r.base;
        if (at + width > r.bytes.size()) poison("field_width_crosses_record");
        const auto old = word(r.bytes, at, width);
        if (r.kind == Kind::registry_root) {
            if (width != 4 || (at != 0 && at != 12) || !pointer(value, at == 0 ? Kind::exports : Kind::client)) poison("root_field_permission");
        } else if (r.kind == Kind::exports) {
            if (width == 4 && at == 0) {
                if (value == 0x41C00000u) { if (operation_ != Operation::release || r.base != get(table_).base) poison("canonical_marker_permission"); }
                else if (!pointer(value, Kind::exports)) poison("previous_table_permission");
            } else if (width == 4 && at == 4) { if (!pointer(value, Kind::client)) poison("table_client_permission"); }
            else if (width == 2 && at == 10) { if (operation_ != Operation::registration || value != (old & ~1u)) poison("table_flags_permission"); }
            else poison("immutable_export_field");
        } else {
            if (width == 4 && at == 4) { if (!pointer(value, Kind::client)) poison("client_next_permission"); }
            else if (width == 2 && at == 10) {
                const auto expected = operation_ == Operation::unlink ? old & ~7u : (old & ~6u) | 2u;
                if (value != expected) poison("client_flags_permission");
            } else if (width == 4 && at >= 20 && at < 20 + 8 * r.count && (at - 20) % 8 == 0) {
                const auto ordinal_word = word(r.canonical, at + 4, 4);
                if (word(r.bytes, at + 4, 4) != ordinal_word) poison("sdk_delay_word_unproved");
                std::uint32_t expected = 0x03E00008u;
                if (operation_ != Operation::unlink) {
                    const auto &table = get(table_);
                    if (!same_library(r, table)) poison("library_version_binding");
                    const auto ordinal = ordinal_word & 0xFFFFu;
                    if (ordinal < table.count) expected = 0x08000000u | ((word(table.canonical, 20 + 4 * ordinal, 4) >> 2) & 0x03FFFFFFu);
                }
                if (value != expected) poison("original_export_slot_binding");
            } else poison("immutable_client_field");
        }
        for (unsigned i = 0; i < width; ++i) r.bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
        auto &o = owners_[r.owner.index];
        for (unsigned i = 0; i < width; ++i) {
            const auto offset = address + i - o.module.base;
            if (offset < o.module.text_size) {
                auto &v = o.words[offset / 4]; const auto shift = (offset & 3u) * 8;
                v = (v & ~(255u << shift)) | (static_cast<std::uint32_t>(r.bytes[at + i]) << shift);
            }
        }
        ++cursor_;
    }
public:
    void end() {
        try {
            ready(); if (!active_ || cursor_ != plan_.size()) poison("incomplete_transition");
            active_ = false; verify(); validate_registry_graph();
        } catch (...) { poisoned_ = true; active_ = false; throw; }
    }
    void retire_owner(OwnerHandle owner) {
        auto &o = get(owner); ready(); if (active_) fail("retire_during_transition"); verify(); validate_registry_graph();
        for (const auto &r : records_) if (live(r) && r.owner.index == owner.index) {
            if (r.kind == Kind::registry_root) fail("registry_root_process_lifetime");
            if (r.kind == Kind::exports && registered(r.base)) fail("registered_owner_still_live");
            if (r.kind == Kind::client && reachable_client(r.base)) fail("client_owner_still_live");
        }
        if (!retirement_lifetime_closed || !retirement_lifetime_closed(o.module.base, o.module.total_size)) fail("module_reference_lifetime_unbound");
        o.live = false; ++o.generation;
    }
    // Exact SDK J/original-ADDIU-to-zero platform linkage, not an opcode
    // interpreter. GPRs/RA remain unchanged; partial budgets preserve delay PC.
    bool step_sdk_stub(OwnerHandle owner, Cpu &cpu, std::int64_t &budget) const {
        if (!get(owner).sealed) fail("unsealed_owner"); verify(); if (active_) fail("transition_context_restore_unbound"); const auto physical = cpu.pc & 0x1FFFFFFFu;
        for (const auto &r : records_) if (live(r) && r.owner.index == owner.index && r.kind == Kind::client &&
            physical >= r.base + 20 && physical < r.base + 20 + 8 * r.count) {
            const auto slot = (physical - r.base - 20) / 8; const auto at = 20 + slot * 8;
            const auto first = word(r.bytes, at, 4), second = word(r.bytes, at + 4, 4);
            if ((first >> 26) != 2u || second != word(r.canonical, at + 4, 4)) return false;
            if (!registered_target(r, slot, first)) fail("stale_original_export_binding");
            if (!budget) return true;
            if (budget < 0 || cpu.r[0]) fail("invalid_stub_state");
            --budget;
            if ((physical - r.base - 20) % 8 == 0) {
                const auto target = (cpu.pc & 0xF0000000u) | ((first & 0x03FFFFFFu) << 2);
                cpu.pc = cpu.npc; cpu.npc = target;
            } else { cpu.pc = cpu.npc; cpu.npc += 4; }
            cpu.r[0] = 0; return true;
        }
        return false;
    }
private:
    struct Owner { r192_aot::Module module{}; r192_aot::Services legacy{}; std::vector<std::uint32_t> words; std::map<std::uint32_t, std::array<std::uint32_t, 2>> canonical_imports; std::uint64_t generation{}; std::size_t record_count{}; bool live{}, sealed{}; };
    struct Record { OwnerHandle owner; Kind kind{}; std::uint32_t base{}; std::size_t count{}; std::vector<std::uint8_t> canonical, bytes; };
    Read read_; std::vector<Owner> owners_; std::vector<Record> records_; std::uint64_t generation_{};
    Operation operation_{}; RecordHandle table_{}; std::vector<Write> plan_; std::size_t cursor_{}; bool active_{}, poisoned_{};
    [[noreturn]] static void fail(const char *why) { throw std::runtime_error(std::string("IOP_METADATA_REFUSED:") + why); }
    [[noreturn]] void poison(const char *why) { poisoned_ = true; fail(why); }
    void ready() const { if (poisoned_) fail("transition_fault_requires_rebuild"); }
    const Owner &get(OwnerHandle h) const { if (h.index >= owners_.size() || !owners_[h.index].live || owners_[h.index].generation != h.generation) fail("stale_owner_generation"); return owners_[h.index]; }
    Owner &get(OwnerHandle h) { return const_cast<Owner &>(static_cast<const Guard *>(this)->get(h)); }
    const Record &get(RecordHandle h) const { if (h.index >= records_.size() || records_[h.index].owner.generation != h.generation || !live(records_[h.index])) fail("stale_record_generation"); return records_[h.index]; }
    bool live(const Record &r) const { return r.owner.index < owners_.size() && owners_[r.owner.index].live && owners_[r.owner.index].generation == r.owner.generation; }
    static std::uint32_t word(const std::vector<std::uint8_t> &bytes, std::size_t at, unsigned width) { std::uint32_t value{}; if (width > 4 || at + width > bytes.size()) fail("word_bounds"); for (unsigned i = 0; i < width; ++i) value |= static_cast<std::uint32_t>(bytes[at + i]) << (8 * i); return value; }
    bool pointer(std::uint32_t value, Kind kind) const { if (!value) return true; for (const auto &r : records_) if (live(r) && r.base == value && r.kind == kind) return true; return false; }
    static bool same_library(const Record &client, const Record &table) { return std::equal(client.canonical.begin() + 12, client.canonical.begin() + 20, table.canonical.begin() + 12) && (word(client.canonical, 8, 2) >> 8) == (word(table.canonical, 8, 2) >> 8); }
    const Record &at(std::uint32_t address, Kind kind) const { for (const auto &r : records_) if (live(r) && r.base == address && r.kind == kind) return r; fail("unowned_record_pointer"); }
    const Record &root() const { for (const auto &r : records_) if (live(r) && r.kind == Kind::registry_root) return r; fail("registry_root_unavailable"); }
    bool registered(std::uint32_t address) const { auto next = word(root().bytes, 0, 4); for (std::size_t i = 0; next && i <= records_.size(); ++i) { if (next == address) return true; next = word(at(next, Kind::exports).bytes, 0, 4); } return false; }
    void validate_registry_graph() const {
        auto next = word(root().bytes, 0, 4); std::size_t count{}; std::set<std::uint32_t> clients;
        while (next) {
            if (++count > records_.size()) fail("registry_cycle");
            const auto &table = at(next, Kind::exports); auto client = word(table.bytes, 4, 4);
            while (client) {
                if (!clients.insert(client).second) fail("client_cycle_or_multiple_owners");
                const auto &c = at(client, Kind::client);
                if (!(word(c.bytes, 10, 2) & 2u) || !same_library(c, table)) fail("client_library_membership");
                client = word(c.bytes, 4, 4);
            }
            next = word(table.bytes, 0, 4);
        }
        next = word(root().bytes, 12, 4);
        while (next) {
            if (!clients.insert(next).second) fail("pending_client_cycle");
            const auto &c = at(next, Kind::client); if (word(c.bytes, 10, 2) & 2u) fail("pending_client_already_bound");
            next = word(c.bytes, 4, 4);
        }
        for (const auto &r : records_) if (live(r) && r.kind == Kind::client && (word(r.bytes, 10, 2) & 2u) && !clients.count(r.base)) fail("orphan_bound_client");
    }
    bool reachable_client(std::uint32_t address) const {
        auto next = word(root().bytes, 0, 4);
        for (std::size_t i = 0; next && i <= records_.size(); ++i) {
            const auto &table = at(next, Kind::exports); auto client = word(table.bytes, 4, 4);
            for (std::size_t j = 0; client && j <= records_.size(); ++j) { if (client == address) return true; client = word(at(client, Kind::client).bytes, 4, 4); }
            next = word(table.bytes, 0, 4);
        }
        next = word(root().bytes, 12, 4);
        for (std::size_t i = 0; next && i <= records_.size(); ++i) { if (next == address) return true; next = word(at(next, Kind::client).bytes, 4, 4); }
        return false;
    }
    bool registered_target(const Record &client, std::size_t slot, std::uint32_t first) const {
        auto next = word(root().bytes, 0, 4);
        for (std::size_t i = 0; next && i <= records_.size(); ++i) {
            const auto &table = at(next, Kind::exports); const auto ordinal = word(client.canonical, 24 + slot * 8, 4) & 0xFFFFu;
            if (same_library(client, table) && ordinal < table.count && first == (0x08000000u | ((word(table.canonical, 20 + 4 * ordinal, 4) >> 2) & 0x03FFFFFFu))) return true;
            next = word(table.bytes, 0, 4);
        }
        return false;
    }
};
} // namespace ps2::iop::metadata_guard
