// Finite original corpus, loader identity only. No module entry is executed.
#include R192_OWNER_FIXTURE_PREFIX
#include "iop/r192_loader_code_owner.hpp"
#include "ps2/state.hpp"
#include <optional>
namespace lo = ps2::iop::loader_owner;
#include R192_OWNER_SOURCES
namespace {
lo::Facts corpus_fact(int id) {
    const auto &m = kernel::g_modules.at(id);
    return {m.id, m.name, m.base, m.size, m.entry, m.gp};
}
std::vector<lo::Facts> corpus_facts() {
    std::vector<lo::Facts> result;
    for (const auto &[id, m] : kernel::g_modules) result.push_back(corpus_fact(id));
    return result;
}
std::string quoted(const std::string &text) {
    std::string out = "\"";
    for (unsigned char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c >= 32 && c < 127) out += c;
        else { static constexpr char hex[]="0123456789abcdef"; out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
    }
    return out + '"';
}
void require_corpus(bool value, const char *why) { if (!value) throw std::runtime_error(why); }
void write_corpus(const std::string &path, const void *data, std::size_t size) {
    std::ofstream out(path, std::ios::binary); out.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
    require_corpus(static_cast<bool>(out), "corpus_output_write_failed");
}
struct Slot { unsigned table{}, ordinal{}, target{}; std::string status, reason; };
}
int main(int argc, char **argv) {
    if (argc != 5) return 2;
    const std::string mode = argv[1], route = argv[3], output = argv[4];
    const bool connected = mode == "connector";
    std::string fatal, owner_refusal, digest, actual_name;
    std::vector<Slot> slots; bool admitted{}; int loaded_id{}; unsigned module_base{}, module_size{};
    try {
        require_corpus(connected || mode == "reference", "unknown_capture_mode");
        require_corpus(route == "filename" || route == "buffer", "unknown_actual_loader_route");
        require_corpus(ps2::g_options.memcard_path.empty(), "card_path_not_empty");
        std::ifstream input(argv[2], std::ios::binary); require_corpus(static_cast<bool>(input), "original_input_unreadable");
        const std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input), {}};
        digest = lo::image_sha256(bytes);
        const auto found = std::find_if(original_sources.begin(), original_sources.end(), [&](const auto &s) { return s.digest == digest; });
        require_corpus(found != original_sources.end(), "original_input_not_in_source_catalogue");
        const auto &source = *found;
        std::memset(g_ram, 0, kRamSize); kernel::boot();
        auto read = [](std::uint32_t a, unsigned width) { const auto at = phys(a); std::uint32_t value{};
            if (at >= kRamSize || width > kRamSize - at) throw std::runtime_error("read_bounds");
            std::memcpy(&value, g_ram + at, width); return value; };
        const auto path = route == "buffer" ? "buffer" : source.filename_name + ".IRX";
        auto loader = [&](const auto &data, const auto &name, const auto &args) {
            loaded_id = load_module_buffer(data, name, args); return loaded_id; };
        lo::Session session(original_sources, read, {nullptr, kernel::identity, nullptr}, loader, corpus_fact, corpus_facts);
        std::optional<lo::OwnerHandle> owner;
        if (connected) {
            try { owner = session.load(bytes, path, ""); admitted = true; }
            catch (const std::exception &e) { owner_refusal = e.what(); }
        } else loaded_id = loader(bytes, path, "");
        require_corpus(loaded_id > 0, "actual_original_loader_failed");
        const auto fact = corpus_fact(loaded_id); actual_name = fact.name; module_base = fact.base; module_size = fact.size;
        for (const auto &record : source.records) if (record.kind == ps2::iop::metadata_guard::Kind::exports) {
            for (unsigned ordinal = 0; ordinal < record.words.size() - 6; ++ordinal) {
                Slot slot{record.offset, ordinal, record.words[5 + ordinal], "REFERENCE_SOURCE_SLOT", ""};
                if (connected) {
                    if (!owner) { slot.status = "OWNER_UNADMITTED"; slot.reason = owner_refusal; }
                    else try {
                        const auto entry = session.export_entry(*owner, record.offset, ordinal);
                        const auto proof = session.validate_entry(entry);
                        require_corpus(proof.original_digest == digest && proof.loaded_id == loaded_id && proof.compiled_body == source.code->body &&
                            entry.target_offset == slot.target && proof.base + slot.target == ram32(proof.base + slot.table + 20 + ordinal * 4),
                            "original_export_compiled_identity_mismatch");
                        slot.status = "VALIDATED_IDENTITY";
                    } catch (const std::exception &e) { slot.status = "REFUSED"; slot.reason = e.what(); }
                }
                slots.push_back(std::move(slot));
            }
        }
        // Cancel ONLY actual unstarted fixture entry threads; no module entry
        // or on_exit completion is fabricated. Both captures do the same work.
        std::vector<int> tids;
        for (const auto &[id, thread] : kernel::g_threads) if (thread.on_exit) tids.push_back(id);
        for (int tid : tids) { Cpu c{}; c.r[4] = static_cast<std::uint32_t>(tid);
            kernel::g_hle.at(kernel::g_hle_ids.at({"thbase", 10})).function(c);
            require_corpus(!c.r[2] && kernel::delete_thread(tid), "unstarted_fixture_thread_cancel_failed"); }
        write_corpus(output + ".cpu", &g_cpu, sizeof g_cpu); write_corpus(output + ".ram", g_ram, kRamSize);
        ps2::state::Writer writer; writer.set_version(8); ps2::iop::state(writer);
        write_corpus(output + ".iop-state", writer.out.data(), writer.out.size());
        require_corpus(forbidden_interpreter_calls == 0 && ps2::g_options.memcard_path.empty(), "guest_execution_or_card_used");
    } catch (const std::exception &e) { fatal = e.what(); }
    std::ofstream json(output + ".json");
    json << "{\"capture_status\":" << quoted(fatal.empty() ? "COMPLETE" : "REFUSED") << ",\"fatal\":" << quoted(fatal)
         << ",\"mode\":" << quoted(mode) << ",\"route\":" << quoted(route) << ",\"original_sha256\":" << quoted(digest)
         << ",\"actual_name\":" << quoted(actual_name) << ",\"loaded_id\":" << loaded_id << ",\"base\":" << module_base << ",\"size\":" << module_size
         << ",\"source_owner_admitted\":" << (admitted ? "true" : "false") << ",\"owner_refusal\":" << quoted(owner_refusal)
         << ",\"executed_instructions\":" << forbidden_interpreter_calls << ",\"native_credit\":0,\"slots\":[";
    for (std::size_t i = 0; i < slots.size(); ++i) { const auto &s = slots[i]; if (i) json << ',';
        json << "{\"table_offset\":" << s.table << ",\"ordinal\":" << s.ordinal << ",\"target_offset\":" << s.target
             << ",\"status\":" << quoted(s.status) << ",\"reason\":" << quoted(s.reason) << '}'; }
    json << "]}\n";
    return fatal.empty() ? 0 : 1; // Capture completion is not ownership success.
}
