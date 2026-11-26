// Test-only real-loader/export/scheduler connection, with no substitute import.
// The prefix is derived from the pinned independent published fixture.
#include R192_EXPORT_ROUTE_FIXTURE_PREFIX

namespace {
bool capture_original_export_return{};
unsigned original_export_returns{};
Cpu original_export_return_cpu{};
void record_original_export_return(std::uint32_t id, Cpu &cpu) {
    if (capture_original_export_return && id == (ram32(kernel::g_return_stub) & 0x03FFFFFFu)) {
        ++original_export_returns; original_export_return_cpu = cpu;
    }
}
}

int main(int argc, char **argv) {
    if (argc != 7) return 2;
    const std::string mode = argv[3], route = argv[4], output = argv[5], plant = argv[6];
    if ((mode != "reference" && mode != "compiled") || (route != "filename" && route != "buffer")) return 2;
    r192_route_reference = mode == "reference";
    auto read = [](const char *path) { std::ifstream file(path, std::ios::binary); return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(file), {}}; };
    const auto provider = read(argv[1]); auto consumer = read(argv[2]);
    std::string failure, phase = "boot";
    unsigned ticks{}, provider_ticks{}, consumer_ticks{}, joined_stubs{}, export_call_ticks{};
    std::int32_t provider_result = -999, consumer_result = -999;
    bool provider_done{}, consumer_done{};
    std::uint32_t provider_base{}, provider_size{}, registered_table{}, provider_version{}, consumer_version{};
    std::uint32_t original_export_result{};
    try {
        if (!ps2::g_options.memcard_path.empty()) throw std::runtime_error("card_configuration_refused");
        // Actual public cold reset installs original DMA/SPU jobs before boot.
        // With empty card/audio configuration this opens no device or card.
        ps2::iop::reset();
        for (const auto &[key, id] : kernel::g_hle_ids) known_boot_ids.insert(id);
        trace.open(output + ".trace", std::ios::binary); observe = true; checkpoint(g_cpu);
        auto run_start = [&](bool &done) {
            const auto begin = ticks;
            while (!done && ticks - begin < 5000) {
                ps2::iop::run(kClockDivider); ++ticks;
                checkpoint(g_cpu);
                write_bytes(&kernel::g_now, sizeof kernel::g_now);
                write_bytes(&kernel::g_current, sizeof kernel::g_current);
                write_bytes(&kernel::g_switch, sizeof kernel::g_switch);
            }
            if (!done) throw std::runtime_error("actual_scheduler_module_start_5000_tick_limit");
            return ticks - begin;
        };
        phase = "provider_start";
        if (plant != "provider-absent") {
            const auto id = load_module_buffer(provider, route == "buffer" ? "buffer" : "LIBSD.IRX", "",
                [&](int, int result) { provider_done = true; provider_result = result; });
            if (id < 0) throw std::runtime_error("actual_provider_loader_failed");
            provider_base = kernel::g_modules.at(id).base; provider_size = kernel::g_modules.at(id).size;
            provider_ticks = run_start(provider_done);
            if ((provider_result & 3) == 1) throw std::runtime_error("provider_return_frees_image_export_lifetime_unproved");
        }
        phase = "registered_library";
        const auto library = kernel::g_libraries.find("libsd");
        if (library == kernel::g_libraries.end()) throw std::runtime_error("original_libsd_export_not_registered");
        registered_table = library->second;
        if (registered_table < provider_base || registered_table + 20 > provider_base + provider_size || ram32(registered_table) != 0x41C00000u)
            throw std::runtime_error("registered_original_export_table_identity_refused");
        provider_version = read16(registered_table + 8);
        phase = "consumer_load";
        if (plant == "wrong-version") {
            bool changed{};
            for (std::size_t at = 0; at + 20 <= consumer.size(); at += 4)
                if (consumer[at] == 0 && consumer[at + 1] == 0 && consumer[at + 2] == 0xE0 && consumer[at + 3] == 0x41 &&
                    std::memcmp(consumer.data() + at + 12, "libsd\0", 6) == 0) { consumer[at + 8] ^= 1; changed = true; break; }
            if (!changed) throw std::runtime_error("wrong_version_control_site_missing");
        }
        const auto id = load_module_buffer(consumer, route == "buffer" ? "buffer" : "SDRDRV.IRX", "",
            [&](int, int result) { consumer_done = true; consumer_result = result; });
        if (id < 0) throw std::runtime_error("actual_consumer_loader_failed");
        const auto &loaded = kernel::g_modules.at(id);
        const auto *source = aot::find_pic_template("SDRDRV", "85a465ab251bde0c3d33151b3a67ec2780d57cc40ebb7df9d5ecb7bad3396b16");
        if (!source) throw std::runtime_error("original_consumer_template_missing");
        for (std::uint32_t i = 0; i < source->import_count; ++i) {
            const auto &entry = source->imports[i];
            if (std::string(entry.library) != "libsd") continue;
            const auto stub = loaded.base + entry.offset;
            const auto slot = registered_table + 20u + entry.ordinal * 4u;
            const auto target = ram32(slot);
            if (slot + 4 > provider_base + provider_size || target < provider_base || target >= provider_base + provider_size || (target & 3u))
                throw std::runtime_error("original_export_slot_target_out_of_provider");
            if (ram32(stub) != (0x08000000u | ((target >> 2) & 0x03FFFFFFu)) || ram32(stub + 4) != 0 || !kernel::identity(nullptr, stub, entry))
                throw std::runtime_error("actual_loader_J_NOP_original_export_join_refused");
            ++joined_stubs;
        }
        consumer_version = 0;
        for (std::uint32_t at = loaded.base; at + 20 <= loaded.base + loaded.size; at += 4)
            if (ram32(at) == 0x41E00000u && std::memcmp(ram_ptr(at + 12), "libsd\0", 6) == 0) { consumer_version = read16(at + 8); break; }
        if (provider_version != 0x0105u || consumer_version != provider_version || joined_stubs != 26)
            throw std::runtime_error("original_library_version_or_ordinal_set_mismatch");
        phase = "consumer_start"; consumer_ticks = run_start(consumer_done);
        // Constructed source-call-bundle vector, not a historical RPC entry or
        // a fabricated native signature. Original LW obtains mode=0 from the
        // source packet; original JAL/NOP enters the real loader-linked stub.
        phase = "original_caller_stub_prefix";
        const auto packet = kernel::alloc(8);
        if (!packet) throw std::runtime_error("constructed_packet_allocation_failed");
        set_ram32(packet, 0xE630u); set_ram32(packet + 4, 0);
        const auto tid = kernel::create_thread(loaded.base + 0x85Cu, 0x4000u, kernel::kDefaultModulePriority, loaded.gp, 0x02000000u, 0);
        if (tid <= 0 || !kernel::thread(tid)) throw std::runtime_error("constructed_source_call_thread_failed");
        kernel::start_thread(tid, 0, 0);
        kernel::thread(tid)->context.r[7] = packet;
        if (plant == "wrong-export-target") set_ram32(registered_table + 20 + 4 * 4, ram32(registered_table + 20 + 4 * 4) ^ 4u);
        const auto entry = ram32(registered_table + 20 + 4 * 4);
        while ((kernel::g_current != tid || g_cpu.pc != entry) && export_call_ticks < 16) {
            ps2::iop::run(kClockDivider); ++export_call_ticks;
            checkpoint(g_cpu); write_bytes(&kernel::g_now, sizeof kernel::g_now);
            write_bytes(&kernel::g_current, sizeof kernel::g_current); write_bytes(&kernel::g_switch, sizeof kernel::g_switch);
        }
        if (kernel::g_current != tid || g_cpu.pc != entry || g_cpu.r[4] != 0 || g_cpu.r[31] != loaded.base + 0x868u)
            throw std::runtime_error("original_caller_stub_prefix_target_argument_return_unproved");
        // Separate, explicitly constructed whole-export call through the
        // unchanged production callback adapter. No per-instruction trace is
        // claimed inside its kSlice batches; writes/services and raw return
        // CPU plus full final state are independently compared.
        phase = "original_call_guest_export";
        const Cpu before_bridge = g_cpu;
        capture_original_export_return = true;
        original_export_result = plant == "unproved-zero-result" ? 0u : kernel::call_guest(entry, g_cpu.r[4], g_cpu.r[5], g_cpu.r[6], g_cpu.r[7]);
        capture_original_export_return = false;
        if (original_export_returns != 1) throw std::runtime_error("original_call_guest_guard_or_return_unproved");
        if (std::memcmp(&g_cpu, &before_bridge, sizeof g_cpu)) throw std::runtime_error("original_callback_adapter_CPU_restore_changed");
        checkpoint(g_cpu);
        save(output + ".export-return-cpu", &original_export_return_cpu, sizeof original_export_return_cpu);
        save(output + ".export-return-value", &original_export_result, sizeof original_export_result);
        observe = false; trace.close();
        save(output + ".cpu", &g_cpu, sizeof g_cpu); save(output + ".ram", g_ram, kRamSize);
        ps2::state::Writer writer; writer.set_version(8); ps2::iop::state(writer);
        save(output + ".iop-state", writer.out.data(), writer.out.size());
        phase = "actual_starts_and_constructed_original_export_bridge_complete";
    } catch (const std::exception &e) { failure = e.what(); observe = false; if (trace.is_open()) trace.close(); }
    std::ofstream result(output + ".json");
    result << "{\"status\":\"" << (failure.empty() ? "PASS" : "REFUSED") << "\",\"phase\":\"" << phase
        << "\",\"failure\":\"" << failure << "\",\"provider_done\":" << (provider_done ? "true" : "false")
        << ",\"consumer_done\":" << (consumer_done ? "true" : "false") << ",\"provider_result\":" << provider_result
        << ",\"consumer_result\":" << consumer_result << ",\"provider_ticks\":" << provider_ticks << ",\"consumer_ticks\":" << consumer_ticks
        << ",\"export_call_ticks\":" << export_call_ticks
        << ",\"original_export_result\":" << original_export_result << ",\"observed_original_export_returns\":" << original_export_returns
        << ",\"provider_base\":" << provider_base << ",\"registered_table\":" << registered_table << ",\"provider_version\":" << provider_version
        << ",\"consumer_version\":" << consumer_version << ",\"joined_original_export_stubs\":" << joined_stubs
        << ",\"native_service_calls\":" << service_calls << ",\"forbidden_interpreter_calls\":" << forbidden_interpreter_calls
        << ",\"synthesized_success_callbacks_executed\":" << kernel::g_missing_reported.size() << ",\"native_credit\":0}\n";
    return failure.empty() ? 0 : 1;
}
