// Actual original static records + original6/7/8/9; no broad text exclusions.
#include R192_METADATA_FIXTURE_PREFIX
#include "iop/r192_library_registry_model.hpp"
#include "iop/r192_static_metadata_guard.hpp"
#include <bit>
struct CanonicalSpec { unsigned kind, offset; std::vector<std::uint32_t> words, relocations; };
#include R192_METADATA_SCHEMAS
namespace {
namespace md = ps2::iop::metadata_guard;
namespace lm = ps2::iop::library_model;
md::Guard *active_guard{};
unsigned actual_returns{}, non_return_services{};
void record_metadata_return(std::uint32_t id, Cpu &) {
    if (id == (ram32(kernel::g_return_stub) & 0x03FFFFFFu)) ++actual_returns;
    else ++non_return_services;
}
}
void guarded_metadata_write(std::uint32_t a, std::uint32_t width, std::uint32_t value) {
    if (active_guard) active_guard->observe_write(a, width, value);
}

int main(int argc, char **argv) {
    if (argc != 7) return 2;
    r192_route_reference = std::string(argv[4]) == "reference";
    if (!r192_route_reference && std::string(argv[4]) != "compiled") return 2;
    const std::string output = argv[5], plant = argv[6];
    std::string failure, expected_plant_reason;
    if (plant == "write-immutable") expected_plant_reason = "immutable_export_field";
    else if (plant == "foreign-pointer") expected_plant_reason = "root_field_permission";
    else if (plant == "wrong-slot") expected_plant_reason = "original_export_slot_binding";
    else if (plant == "live-owner") expected_plant_reason = "module_reference_lifetime_unbound";
    else if (plant != "none") expected_plant_reason = "word_identity_changed";
    unsigned transitions{}, stub_comparisons{}, data_entry_refusals{}, owner_generation_refusals{};
    bool control_rejected{};
    try {
        if (!ps2::g_options.memcard_path.empty()) throw std::runtime_error("card_configuration_refused");
        kernel::boot(); for (const auto &[key, id] : kernel::g_hle_ids) known_boot_ids.insert(id);
        struct Loaded { int id{}; std::uint32_t base{}; const char *digest{}, *name{}; std::unique_ptr<aot::BoundPic> bound; md::OwnerHandle owner; std::map<std::uint32_t, md::RecordHandle> records; };
        auto load = [&](const char *path, const char *name, const char *digest) {
            std::ifstream in(path, std::ios::binary); const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in), {}};
            const auto tid = kernel::g_next_thread;
            const auto id = load_module_buffer(bytes, "buffer", "");
            if (id < 0) throw std::runtime_error("actual_loader_failed");
            // Explicitly cancel the unstarted entry using existing reference
            // APIs. This is NOT a claimed module start/on_exit completion.
            Cpu control{}; control.r[4] = static_cast<std::uint32_t>(tid);
            kernel::g_hle.at(kernel::g_hle_ids.at({"thbase", 10})).function(control);
            if (control.r[2] || !kernel::delete_thread(tid)) throw std::runtime_error("unstarted_loader_thread_cancellation_failed");
            Loaded value; value.id = id; value.base = kernel::g_modules.at(id).base; value.digest = digest; value.name = name;
            value.bound = std::make_unique<aot::BoundPic>();
            const auto *source = aot::find_pic_template(name, digest);
            if (!source || !value.bound->bind(*source, value.base)) throw std::runtime_error("original_template_binding_failed");
            return value;
        };
        auto sdk = load(argv[1], "LOADCORE", "93d205474b13ab152c74a578948351daee2d4ad108745797721816e77ae15f3c");
        auto provider = load(argv[2], "LIBSD", "94915a3d9673e8a370e144ae493552d5e1f3956bb9ae5bd91e2da5f36c83ce7d");
        auto consumer = load(argv[3], "SDRDRV", "85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16");
        // Reproduce canonical pre-link source only in this owned fixture.
        // Runtime adoption must defer the legacy loader patch for these SDK
        // clients, rather than repair unknown live state after the fact.
        for (const auto &s : original_schemas.at(consumer.digest)) if (s.kind == 1 && s.offset == 0x10C0)
            for (unsigned i = 20; i + 8 <= s.words.size() * 4 - 8; i += 8) {
                set_ram32(consumer.base + s.offset + i, s.words[i / 4]);
                set_ram32(consumer.base + s.offset + i + 4, s.words[i / 4 + 1]);
            }
        md::Guard guard([](std::uint32_t address, unsigned width) {
            if (width == 1) return static_cast<std::uint32_t>(*ram_ptr(address));
            if (width == 2) return static_cast<std::uint32_t>(read16(address));
            return ram32(address);
        });
        auto bind = [&](Loaded &m, bool root) {
            const auto &specs = original_schemas.at(m.digest);
            if (!m.bound->bind(*aot::find_pic_template(m.name, m.digest), m.base)) throw std::runtime_error("fresh_original_view_failed");
            m.records.clear();
            m.owner = guard.bind_owner(m.bound->module, {nullptr, kernel::identity, nullptr}, specs.size() + (root ? 1 : 0));
            for (const auto &s : specs) {
                auto words = s.words; for (auto offset : s.relocations) words[offset / 4] += m.base;
                std::vector<std::uint8_t> canonical;
                for (auto word : words) for (unsigned i = 0; i < 4; ++i) canonical.push_back(static_cast<std::uint8_t>(word >> (8 * i)));
                m.records.emplace(s.offset, guard.bind_record(m.owner, s.kind == 0 ? md::Kind::exports : md::Kind::client, s.offset, std::move(canonical)));
            }
            if (root) m.records.emplace(0x1F50, guard.bind_record(m.owner, md::Kind::registry_root, 0x1F50, std::vector<std::uint8_t>(16)));
            guard.seal_owner(m.owner);
        };
        bind(sdk, true); bind(provider, false); bind(consumer, false);
        struct IdentityContext { md::Guard *guard; md::OwnerHandle owner; };
        auto validate_import = +[](void *opaque, std::uint32_t address, const aot::Import &entry) {
            const auto &context = *static_cast<IdentityContext *>(opaque);
            return context.guard->import_identity(context.owner, address, entry);
        };
        const auto table_record = provider.records.at(0), client_record = consumer.records.at(0x10C0);
        const auto table_address = guard.address(table_record), client_address = guard.address(client_record);
        lm::Registry registry; auto table = std::make_shared<lm::Table>(); auto client = std::make_shared<lm::Client>();
        std::copy_n("libsd", 5, table->name.begin()); client->name = table->name; table->version = client->version = 0x0105;
        const auto *provider_source = aot::find_pic_template("LIBSD", provider.digest);
        std::size_t functions{}; while (provider.bound->words[(20 + functions * 4) / 4]) ++functions;
        std::vector<lm::CallableIdentity> callables(functions);
        std::map<const lm::CallableIdentity *, std::uint32_t> targets;
        for (std::size_t i = 0; i < functions; ++i) { table->functions.push_back(&callables[i]); targets[&callables[i]] = provider.bound->words[(20 + i * 4) / 4]; }
        const auto &client_spec = *std::find_if(original_schemas.at(consumer.digest).begin(), original_schemas.at(consumer.digest).end(), [](const auto &s) { return s.kind == 1 && s.offset == 0x10C0; });
        for (std::size_t at = 24; at < client_spec.words.size() * 4 - 8; at += 8) client->ordinals.push_back(static_cast<std::uint16_t>(client_spec.words[at / 4]));
        client->targets.resize(client->ordinals.size());
        std::vector<md::Write> plan;
        registry.observe = [&](const lm::Change &c) {
            using lm::Field;
            switch (c.field) {
            case Field::head: plan.push_back({sdk.base + 0x1F50, 4, c.table_value ? table_address : 0}); break;
            case Field::pending: plan.push_back({sdk.base + 0x1F5C, 4, c.client_value ? client_address : 0}); break;
            case Field::table_previous: plan.push_back({table_address, 4, c.table_value ? table_address : 0}); break;
            case Field::table_marker: plan.push_back({table_address, 4, 0x41C00000u}); break;
            case Field::table_clients: plan.push_back({table_address + 4, 4, c.client_value ? client_address : 0}); break;
            case Field::table_flags: plan.push_back({table_address + 10, 2, c.flags}); break;
            case Field::client_next: plan.push_back({client_address + 4, 4, c.client_value ? client_address : 0}); break;
            case Field::client_flags: plan.push_back({client_address + 10, 2, c.flags}); break;
            case Field::stub_target: plan.push_back({client_address + 20 + static_cast<std::uint32_t>(8 * c.slot), 4, c.target ? 0x08000000u | ((targets.at(c.target) >> 2) & 0x03FFFFFFu) : 0x03E00008u}); break;
            }
        };
        auto operation = [&](md::Operation op) {
            plan.clear(); std::int32_t expected{}; std::uint32_t entry{}, argument{};
            if (op == md::Operation::registration) { expected = registry.register_table(table); entry = 0x938; argument = table_address; }
            else if (op == md::Operation::release) { expected = registry.release_table(table); entry = 0xB80; argument = table_address; }
            else if (op == md::Operation::link) { expected = registry.link_client(client); entry = 0xCAC; argument = client_address; }
            else { expected = registry.unlink_client(client); entry = 0xD68; argument = client_address; }
            if (plant == "write-immutable" && op == md::Operation::registration) { plan[0] = {table_address + 8, 2, 0x0205}; }
            if (plant == "foreign-pointer" && op == md::Operation::registration) { plan.back().value = 0x12345678; }
            if (plant == "wrong-slot" && op == md::Operation::link) { plan[0].value ^= 1u; }
            guard.begin(op, table_record, plan); active_guard = &guard; actual_returns = 0;
            if (!guard.permits_entry(sdk.owner, sdk.base + entry)) throw std::runtime_error("SDK_method_entry_not_callable");
            if ((plant == "write-immutable" || plant == "foreign-pointer") && op == md::Operation::registration || plant == "wrong-slot" && op == md::Operation::link) {
                for (const auto &write : plan) {
                    guard.observe_write(write.address, write.width, write.value);
                    for (unsigned i = 0; i < write.width; ++i) *ram_ptr(write.address + i) = static_cast<std::uint8_t>(write.value >> (8 * i));
                }
                throw std::runtime_error("forged_metadata_plan_admitted");
            }
            const auto span = static_cast<std::uint32_t>(client_spec.words.size() * 4);
            const auto result = kernel::call_guest(sdk.base + entry, argument, (op == md::Operation::link || op == md::Operation::unlink) ? span : 0);
            active_guard = nullptr; guard.end();
            if (actual_returns != 1 || std::bit_cast<std::int32_t>(result) != expected) throw std::runtime_error("original_transition_return_mismatch");
            ++transitions;
        };
        operation(md::Operation::registration);
        if (plant == "version") { set_ram32(table_address + 8, ram32(table_address + 8) ^ 1u); guard.verify(); }
        if (plant == "name") { set_ram32(table_address + 12, ram32(table_address + 12) ^ 1u); guard.verify(); }
        if (plant == "function-pointer") { set_ram32(table_address + 20, ram32(table_address + 20) ^ 4u); guard.verify(); }
        guard.decorate(provider.owner, *provider.bound);
        IdentityContext provider_identity{&guard, provider.owner};
        if (!aot::matches(provider.bound->module, {&provider_identity, validate_import, nullptr})) throw std::runtime_error("projected_exact_provider_identity_failed");
        if (guard.permits_entry(provider.owner, table_address)) throw std::runtime_error("metadata_admitted_as_code"); ++data_entry_refusals;
        operation(md::Operation::link);
        guard.decorate(consumer.owner, *consumer.bound);
        IdentityContext consumer_identity{&guard, consumer.owner};
        if (!aot::matches(consumer.bound->module, {&consumer_identity, validate_import, nullptr})) throw std::runtime_error("projected_exact_client_identity_failed");
        if (guard.permits_entry(consumer.owner, client_address)) throw std::runtime_error("client_header_admitted_as_code"); ++data_entry_refusals;
        if (plant == "code-word") { set_ram32(provider.base + 0xB0, ram32(provider.base + 0xB0) ^ 4u); guard.verify(); }
        for (auto alias : {0u, 0x80000000u, 0xA0000000u}) for (unsigned split = 0; split < 2; ++split) {
            Cpu reference{}, native{}; for (unsigned i = 1; i < 32; ++i) reference.r[i] = 0x13570000u + i;
            reference.pc = alias | (client_address + 20); reference.npc = reference.pc + 4; native = reference;
            for (unsigned step = 0; step < 2; ++step) {
                if (split) { auto unchanged = native; std::int64_t zero{}; if (!guard.step_sdk_stub(consumer.owner, native, zero) || std::memcmp(&unchanged, &native, sizeof native)) throw std::runtime_error("zero_budget_stub_changed"); }
                std::int64_t one = 1, other = 1; ps2::iop::reference_execute(reference, one);
                if (!guard.step_sdk_stub(consumer.owner, native, other) || std::memcmp(&reference, &native, sizeof native)) throw std::runtime_error("SDK_stub_CPU_delay_mismatch");
                ++stub_comparisons;
            }
        }
        operation(md::Operation::release); // In use: no metadata permission/write.
        operation(md::Operation::unlink);
        operation(md::Operation::release);
        guard.retirement_lifetime_closed = [&](std::uint32_t base, std::uint32_t size) {
            if (non_return_services) return false;
            for (const auto &[id, t] : kernel::g_threads)
                if ((t.entry >= base && t.entry - base < size) || (t.context.pc >= base && t.context.pc - base < size)) return false;
            return true;
        };
        const auto old_owner = consumer.owner;
        const auto old_record = client_record;
        if (plant == "live-owner") {
            const auto &m = kernel::g_modules.at(consumer.id);
            if (kernel::create_thread(m.entry, 0x4000u, kernel::kDefaultModulePriority, m.gp, 0x02000000u, 0) <= 0) throw std::runtime_error("live_thread_control_failed");
        }
        guard.retire_owner(consumer.owner);
        try { guard.address(old_record); } catch (const std::runtime_error &e) { if (std::string(e.what()).find("stale_record_generation") != std::string::npos) ++owner_generation_refusals; else throw; }
        try { guard.permits_entry(old_owner, client_address + 20); } catch (const std::runtime_error &e) { if (std::string(e.what()).find("stale_owner_generation") != std::string::npos) ++owner_generation_refusals; else throw; }
        if (owner_generation_refusals != 2) throw std::runtime_error("retired_owner_or_record_admitted");
        // Rebind the same actual loaded code/storage as a fresh generation;
        // this is a guard binding replay, NOT a claimed module unload/reload.
        bind(consumer, false);
        if (consumer.owner.generation == old_owner.generation) throw std::runtime_error("owner_generation_reused");
        guard.verify();
    } catch (const std::exception &e) {
        active_guard = nullptr; failure = e.what();
        if (plant != "none" && failure == "IOP_METADATA_REFUSED:" + expected_plant_reason) control_rejected = true;
    }
    std::ofstream out(output);
    out << "{\"status\":\"" << (failure.empty() ? "PASS" : control_rejected ? "PLANTED_REFUSAL" : "REFUSED") << "\",\"failure\":\"" << failure
        << "\",\"transitions\":" << transitions << ",\"stub_CPU_comparisons\":" << stub_comparisons << ",\"data_entry_refusals\":" << data_entry_refusals
        << ",\"stale_generation_refusals\":" << owner_generation_refusals << ",\"native_credit\":0}\n";
    return failure.empty() || control_rejected ? 0 : 1;
}
