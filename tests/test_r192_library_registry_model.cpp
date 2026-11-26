// Original LOADCORE execution versus a native ownership model. The raw memory
// projection is TEST ONLY and is not a runtime mutable-code permission.
#include R192_REGISTRY_FIXTURE_PREFIX
#include "iop/r192_library_registry_model.hpp"
#include <bit>

namespace {
namespace model = ps2::iop::library_model;
struct Write { std::uint32_t address, width, value; bool operator==(const Write &) const = default; };
std::vector<Write> original_writes, projected_writes;
std::vector<std::pair<std::uint32_t, std::uint32_t>> metadata_ranges;
bool collect_metadata{};
unsigned observed_returns{};
void record_metadata_impl(std::uint32_t address, std::uint32_t width, std::uint32_t value) {
    if (collect_metadata)
        for (const auto &[at, size] : metadata_ranges)
            if (address >= at && address + width <= at + size) { original_writes.push_back({address, width, value}); break; }
}
void record_registry_original_return(std::uint32_t id, Cpu &) {
    if (collect_metadata && id == (ram32(kernel::g_return_stub) & 0x03FFFFFFu)) ++observed_returns;
}
}
void record_registry_metadata_write(std::uint32_t address, std::uint32_t width, std::uint32_t value) {
    record_metadata_impl(address, width, value);
}

int main(int argc, char **argv) {
    if (argc != 4) return 2;
    r192_route_reference = std::string(argv[2]) == "reference";
    if (!r192_route_reference && std::string(argv[2]) != "compiled") return 2;
    unsigned checks{}, failure_count{}, data_write_comparisons{}, returns{};
    std::string failure;
    std::vector<std::string> cases;
    try {
        if (!ps2::g_options.memcard_path.empty()) throw std::runtime_error("card_configuration_refused");
        kernel::boot(); for (const auto &[key, id] : kernel::g_hle_ids) known_boot_ids.insert(id);
        std::ifstream in(argv[1], std::ios::binary); const std::vector<std::uint8_t> elf{std::istreambuf_iterator<char>(in), {}};
        const auto id = load_module_buffer(elf, "buffer", "");
        if (id < 0) throw std::runtime_error("original_LOADCORE_loader_failed");
        const auto base = kernel::g_modules.at(id).base, root = base + 0x1F50u;
        // Original LOADCORE boot is not run. Explicit component preconditions:
        // empty registry/pending list, absent interrupt callback table in BSS.
        if (ram32(root) || ram32(root + 12) || ram32(root + 32)) throw std::runtime_error("component_BSS_precondition_unproved");
        metadata_ranges.push_back({root, 16});
        model::Registry registry;
        std::array<model::CallableIdentity, 3> callable_objects;
        std::map<const model::CallableIdentity *, std::uint32_t> callable;
        for (unsigned i = 0; i < 3; ++i) callable[&callable_objects[i]] = ram32(base + 0x1E40u + 20 + (i + 3) * 4);
        std::vector<std::shared_ptr<model::Table>> tables;
        std::vector<std::shared_ptr<model::Client>> clients;
        std::map<const model::Table *, std::uint32_t> table_addresses;
        std::map<const model::Client *, std::uint32_t> client_addresses;
        auto ta = [&](const model::Table *v) { return v ? table_addresses.at(v) : 0u; };
        auto ca = [&](const model::Client *v) { return v ? client_addresses.at(v) : 0u; };
        auto new_table = [&](const char *name, std::uint16_t version) {
            auto value = std::make_shared<model::Table>(); std::copy_n(name, std::min<std::size_t>(std::strlen(name), 8), value->name.begin());
            value->version = version; for (const auto &f : callable_objects) value->functions.push_back(&f);
            const auto at = kernel::alloc(256); if (!at) throw std::runtime_error("owned_table_allocation_failed");
            table_addresses[value.get()] = at; metadata_ranges.push_back({at, 256}); tables.push_back(value); return value;
        };
        auto new_client = [&](const char *name, std::uint16_t version, std::uint16_t flags, model::Table *owner) {
            auto value = std::make_shared<model::Client>(); std::copy_n(name, std::min<std::size_t>(std::strlen(name), 8), value->name.begin());
            value->version = version; value->flags = flags; value->ordinals = {0, 2, 5};
            const auto at = kernel::alloc(256); if (!at) throw std::runtime_error("owned_client_allocation_failed");
            client_addresses[value.get()] = at; metadata_ranges.push_back({at, 256}); clients.push_back(value);
            registry.seed_client(value, owner); return value;
        };
        auto projection = [&] {
            std::map<std::uint32_t, std::uint8_t> bytes;
            auto put = [&](std::uint32_t at, unsigned width, std::uint32_t value) {
                for (unsigned i = 0; i < width; ++i) bytes[at + i] = static_cast<std::uint8_t>(value >> (8 * i));
            };
            put(root, 4, ta(registry.head())); put(root + 12, 4, ca(registry.pending()));
            for (const auto &table : tables) {
                const auto at = ta(table.get());
                put(at, 4, table->marker == model::Marker::canonical ? 0x41C00000u : table->marker == model::Marker::malformed ? 0x12345678u : ta(table->previous));
                put(at + 4, 4, ca(table->clients)); put(at + 8, 2, table->version); put(at + 10, 2, table->flags);
                for (unsigned i = 0; i < 8; ++i) put(at + 12 + i, 1, static_cast<unsigned char>(table->name[i]));
                for (unsigned i = 0; i < table->functions.size(); ++i) put(at + 20 + 4 * i, 4, callable.at(table->functions[i]));
                put(at + 20 + 4 * static_cast<unsigned>(table->functions.size()), 4, 0);
            }
            for (const auto &client : clients) {
                const auto at = ca(client.get()); put(at, 4, 0x41E00000u); put(at + 4, 4, ca(client->next));
                put(at + 8, 2, client->version); put(at + 10, 2, client->flags);
                for (unsigned i = 0; i < 8; ++i) put(at + 12 + i, 1, static_cast<unsigned char>(client->name[i]));
                for (unsigned i = 0; i < client->ordinals.size(); ++i) {
                    const auto *target = client->targets[i];
                    put(at + 20 + 8 * i, 4, target ? 0x08000000u | ((callable.at(target) >> 2) & 0x03FFFFFFu) : 0x03E00008u);
                    put(at + 24 + 8 * i, 4, 0x24000000u | client->ordinals[i]);
                }
                put(at + 20 + 8 * static_cast<unsigned>(client->ordinals.size()), 4, 0);
                put(at + 24 + 8 * static_cast<unsigned>(client->ordinals.size()), 4, 0);
            }
            return bytes;
        };
        registry.observe = [&](const model::Change &c) {
            using model::Field; Write write{};
            switch (c.field) {
            case Field::table_clients: write = {ta(c.table) + 4, 4, ca(c.client_value)}; break;
            case Field::table_previous: write = {ta(c.table), 4, ta(c.table_value)}; break;
            case Field::table_marker: write = {ta(c.table), 4, 0x41C00000u}; break;
            case Field::table_flags: write = {ta(c.table) + 10, 2, c.flags}; break;
            case Field::client_next: write = {ca(c.client) + 4, 4, ca(c.client_value)}; break;
            case Field::client_flags: write = {ca(c.client) + 10, 2, c.flags}; break;
            case Field::head: write = {root, 4, ta(c.table_value)}; break;
            case Field::pending: write = {root + 12, 4, ca(c.client_value)}; break;
            case Field::stub_target: write = {ca(c.client) + 20 + 8 * static_cast<std::uint32_t>(c.slot), 4,
                c.target ? 0x08000000u | ((callable.at(c.target) >> 2) & 0x03FFFFFFu) : 0x03E00008u}; break;
            }
            projected_writes.push_back(write);
        };
        auto compare = [&](const char *label, bool registering, const std::shared_ptr<model::Table> &table) {
            for (const auto &[at, value] : projection()) *ram_ptr(at) = value;
            original_writes.clear(); projected_writes.clear(); observed_returns = 0;
            const auto expected = registering ? registry.register_table(table) : registry.release_table(table);
            const auto after = projection(); collect_metadata = true;
            const auto result = kernel::call_guest(base + (registering ? 0x938u : 0xB80u), ta(table.get()));
            collect_metadata = false; ++checks; cases.emplace_back(label); returns += observed_returns;
            if (observed_returns != 1 || std::bit_cast<std::int32_t>(result) != expected)
                throw std::runtime_error(std::string("original_return_or_error_mismatch:") + label);
            if (original_writes != projected_writes) {
                std::fprintf(stderr, "metadata write mismatch %s original=%zu projected=%zu\n", label, original_writes.size(), projected_writes.size());
                for (const auto &v : original_writes) std::fprintf(stderr, " O %x %u %x\n", v.address, v.width, v.value);
                for (const auto &v : projected_writes) std::fprintf(stderr, " N %x %u %x\n", v.address, v.width, v.value);
                throw std::runtime_error(std::string("original_ordered_metadata_write_mismatch:") + label);
            }
            data_write_comparisons += static_cast<unsigned>(original_writes.size());
            for (const auto &[at, value] : after)
                if (*ram_ptr(at) != value) throw std::runtime_error(std::string("original_projected_metadata_mismatch:") + label);
        };
        auto first = new_table("libsd", 0x0105), equal = new_table("libsd", 0x0105), lower = new_table("libsd", 0x0104);
        auto foreign = new_table("libsd", 0x0105), malformed = new_table("libsd", 0x0106);
        malformed->marker = model::Marker::malformed;
        compare("register_empty", true, first); compare("register_same_pointer_illegal", true, first);
        compare("same_name_version_found", true, equal); compare("lower_minor_found", true, lower);
        compare("foreign_pointer_same_name_notfound", false, foreign); compare("malformed_header_illegal", true, malformed);
        compare("release_empty_success", false, first); compare("second_release_notfound", false, first);
        compare("reload_released_object", true, first);
        new_client("libsd", 0x0105, 0, first.get()); new_client("libsd", 0x0105, 1, first.get());
        new_client("unrelated", 0x0101, 0, nullptr); new_client("libsd", 0x0107, 0, nullptr);
        compare("release_linked_clients_inuse", false, first);
        auto major = new_table("libsd", 0x0205); compare("different_major_keeps_clients", true, major);
        auto higher = new_table("libsd", 0x0106); compare("higher_minor_migrates_unlocked_and_pending", true, higher);
        compare("locked_old_client_keeps_old_inuse", false, first); compare("migrated_new_clients_inuse", false, higher);
        compare("release_interior_different_major", false, major);
        auto newest = new_table("libsd", 0x0107); newest->functions.resize(2);
        compare("higher_minor_missing_ordinal_becomes_return_stub", true, newest);
        compare("release_empty_old_version_after_migration", false, higher);
        auto spare = new_table("other", 0x0101); compare("register_other_name", true, spare); compare("release_head", false, spare);
        auto foreign_version = new_table("libsd", 0x0207); compare("foreign_pointer_different_version_notfound", false, foreign_version);
        // Undefined/malformed pointer graphs are refused without running the
        // original on fabricated invalid memory or inventing a numeric error.
        bool null_refused{}, graph_refused{};
        try { registry.release_table({}); } catch (const std::runtime_error &e) { null_refused = std::string(e.what()) == "unadmitted_null_table"; }
        auto *saved = registry.head()->previous; registry.head()->previous = registry.head();
        try { registry.validate(); } catch (const std::runtime_error &e) { graph_refused = std::string(e.what()) == "unproved_library_graph"; }
        registry.head()->previous = saved; registry.validate();
        if (!null_refused || !graph_refused) throw std::runtime_error("unadmitted_pointer_controls_failed");
    } catch (const std::exception &e) { failure = e.what(); ++failure_count; }
    std::ofstream out(argv[3]);
    out << "{\"status\":\"" << (failure.empty() ? "PASS" : "REFUSED") << "\",\"checks\":" << checks
        << ",\"failures\":" << failure_count << ",\"ordered_metadata_writes\":" << data_write_comparisons
        << ",\"actual_original_returns\":" << returns << ",\"failure\":\"" << failure << "\",\"native_credit\":0,\"cases\":[";
    for (std::size_t i = 0; i < cases.size(); ++i) out << (i ? "," : "") << '"' << cases[i] << '"';
    out << "]}\n"; return failure.empty() ? 0 : 1;
}
