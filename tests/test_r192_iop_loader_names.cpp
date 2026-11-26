// Actual pinned v8 loader + strict AOT selector. No module entry is executed.
#ifndef R192_REFERENCE_KERNEL_SOURCE
#error Explicit pinned disposable kernel source required
#endif
#include R192_REFERENCE_KERNEL_SOURCE
#include "ps2/system.hpp"
#include <fstream>
#include <iterator>

int main(int argc, char **argv) {
    if (argc != 8) return 2;
    using namespace ps2::iop;
    namespace aot = ps2::iop::r192_aot;
    const std::string filename = argv[3], mode = argv[4], output = argv[5];
    const std::string expected_header = argv[6];
    const auto header_offset = static_cast<std::size_t>(std::stoul(argv[7]));
    std::string outcome, failure, selected_digest;
    bool native_boundary{}, equivalent_code{};
    std::uint32_t module_base{}, module_size{};
    try {
        if (!ps2::g_options.memcard_path.empty()) throw std::runtime_error("card_configuration_refused");
        std::ifstream input(argv[1], std::ios::binary);
        std::vector<std::uint8_t> original{std::istreambuf_iterator<char>(input), {}};
        const auto *exact = aot::find_pic_template(filename.c_str(), argv[2]);
        const auto *header = aot::find_pic_template(expected_header.c_str(), argv[2]);
        if (!exact || header != exact) throw std::runtime_error("exact_filename_header_digest_registry_join_failed");
        if (aot::find_pic_template(filename.c_str(), "not-an-original-digest") ||
            aot::find_pic_template("__unknown_module_name__", argv[2]) ||
            aot::find_pic_template(nullptr, argv[2]) || aot::find_pic_template(filename.c_str(), nullptr))
            throw std::runtime_error("unknown_name_or_digest_default_found");
        if (expected_header.empty() && aot::find_pic_template("__empty_is_not_default__", argv[2]))
            throw std::runtime_error("empty_name_treated_as_default");
        if (mode == "unknown-header") {
            if (header_offset >= original.size()) throw std::runtime_error("header_control_out_of_bounds");
            original[header_offset] = '^'; // Owned in-memory mutant only; no original file write.
        }
        kernel::boot();
        const bool filename_path = mode == "filename";
        const auto id = load_module_buffer(original, filename_path ? "cdrom0:\\MODULES\\" + filename + ".IRX;1" : "buffer", "");
        if (id < 0) throw std::runtime_error("actual_loader_failed");
        auto &loaded = kernel::g_modules.at(id);
        module_base = loaded.base; module_size = loaded.size;
        if (!loaded.size) {
            if (!filename_path || loaded.name != filename) throw std::runtime_error("unexpected_native_boundary");
            native_boundary = true; outcome = "EXISTING_NATIVE_MODULE_BOUNDARY";
        } else {
            const std::string expected = filename_path ? filename : expected_header;
            if (mode != "unknown-header" && loaded.name != expected) throw std::runtime_error("actual_loader_name_mismatch");
            if (mode == "unknown-name") loaded.name = "__unknown_module_name__";
            if (mode == "changed-code") {
                std::uint32_t offset{};
                for (; offset < exact->text_size; offset += 4) {
                    bool import{};
                    for (std::uint32_t i = 0; i < exact->import_count; ++i)
                        import |= offset == exact->imports[i].offset || offset == exact->imports[i].offset + 4;
                    if (!import) break;
                }
                if (offset >= exact->text_size) throw std::runtime_error("code_mutation_site_missing");
                set_ram32(loaded.base + offset, 0xFFFFFFFFu);
            }
            Cpu cpu{}; cpu.pc = loaded.base; cpu.npc = cpu.pc + 4;
            std::map<kernel::BindingKey, std::uint64_t> verified;
            try {
                auto &selected = kernel::select(cpu, verified);
                if (mode != "filename" && mode != "buffer") throw std::runtime_error("planted_identity_not_rejected");
                aot::BoundPic expected_bound;
                if (!expected_bound.bind(*exact, loaded.base) || !aot::matches(expected_bound.module, {nullptr, kernel::identity, nullptr}))
                    throw std::runtime_error("actual_loader_original_code_import_join_failed");
                if (selected.words != expected_bound.words || selected.module.text_size != expected_bound.module.text_size ||
                    selected.module.total_size != expected_bound.module.total_size)
                    throw std::runtime_error("selected_executable_differs_from_original");
                for (const auto &entry : aot::pic_registry())
                    if (entry.get().body == selected.module.pic_body) { selected_digest = entry.original_sha256; break; }
                equivalent_code = selected_digest != argv[2];
                outcome = equivalent_code ? "ORIGINAL_RELOCATED_CODE_EQUIVALENT_VARIANT" : "ORIGINAL_RELOCATED_CODE_SELECTED";
            } catch (const std::runtime_error &e) {
                if ((mode == "unknown-name" || mode == "unknown-header" || mode == "changed-code") &&
                    std::string(e.what()).find("IOP_AOT_REFUSED:no_original_relocated_module_identity") == 0)
                    outcome = "PLANTED_IDENTITY_REFUSED";
                else throw;
            }
        }
    } catch (const std::exception &e) { failure = e.what(); }
    std::ofstream result(output);
    result << "{\"status\":\"" << (failure.empty() ? "PASS" : "REFUSED") << "\",\"outcome\":\"" << outcome
        << "\",\"module_base\":" << module_base << ",\"module_size\":" << module_size
        << ",\"existing_native_boundary\":" << (native_boundary ? "true" : "false")
        << ",\"equivalent_code_variant\":" << (equivalent_code ? "true" : "false")
        << ",\"selected_registry_digest\":\"" << selected_digest << "\",\"native_credit\":0,\"failure\":\"" << failure << "\"}\n";
    return failure.empty() ? 0 : 1;
}
