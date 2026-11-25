// Test-only reuse of the exact Phase 4 v8 loader and headless reference backing.
// Its main is renamed and never called. The native DUT is a separate generated
// library with no Cpu, RAM, runtime-dispatch or oracle-loader dependency.
#define main phase4_main_not_invoked_by_phase5
#include "test_phase4_aot_equivalence.cpp"
#undef main
#include "native_overlay_initializers.hpp"
#include <chrono>

namespace gen {
void m_MOVIER_unit_0021(ps2::Cpu&, std::uint32_t, std::uint32_t);
}

namespace {
using mka::retail::generated::OverlaySelection;
using TypedInitializer = std::uint64_t (*)(OverlaySelection&) noexcept;
using RawUnit = void (*)(ps2::Cpu&, std::uint32_t, std::uint32_t);
struct InitializerCase {
    const char* name;
    std::uint32_t pc, entry, immediate;
    RawUnit raw;
    TypedInitializer typed;
};
const std::array<InitializerCase, 2> phase5_cases{{
    {"GAMER", 0x007C4980u, 1u, 1u, gen::m_GAMER_unit_0000,
        mka::retail::generated::initialize_gamer},
    {"MOVIER", 0x0084A100u, 8u, 2u, gen::m_MOVIER_unit_0021,
        mka::retail::generated::initialize_movier},
}};
std::string phase5_context;
volatile std::uint64_t phase5_benchmark_sink = 0;

void phase5_require(bool ok, const char* reason) {
    if (!ok) {
        std::fprintf(stderr, "FAIL Phase5 context=%s: %s\n", phase5_context.c_str(), reason);
        std::exit(1);
    }
}

// Compare every declared field, including all 128 GPR bits and VU0 flags.
// Padding bytes are not architectural state. Host accounting is separate.
void phase5_compare_cpu(const ps2::Cpu& actual, const ps2::Cpu& expected) {
    phase5_require(std::memcmp(actual.r, expected.r, sizeof actual.r) == 0,
                   "complete GPR128 delta (V1 low64 liveout; upper64 preserved)");
    phase5_require(std::memcmp(&actual.hi, &expected.hi, sizeof actual.hi) == 0 &&
                   std::memcmp(&actual.lo, &expected.lo, sizeof actual.lo) == 0,
                   "HI/LO preservation");
    phase5_require(actual.sa == expected.sa && actual.pc == expected.pc, "SA/return PC");
    phase5_require(std::memcmp(actual.f, expected.f, sizeof actual.f) == 0 &&
                   actual.facc == expected.facc && actual.fcr31 == expected.fcr31,
                   "FPR/FACC/FCR31 preservation");
    const auto& av = actual.vu0;
    const auto& ev = expected.vu0;
    phase5_require(std::memcmp(av.vf, ev.vf, sizeof av.vf) == 0 &&
                   std::memcmp(&av.acc, &ev.acc, sizeof av.acc) == 0 &&
                   std::memcmp(av.vi, ev.vi, sizeof av.vi) == 0,
                   "VU0 VF/ACC/VI preservation");
    phase5_require(av.q == ev.q && av.p == ev.p && av.i == ev.i && av.r == ev.r &&
                   av.mac == ev.mac && av.status == ev.status && av.clip == ev.clip &&
                   av.cmsar0 == ev.cmsar0 && av.tpc == ev.tpc, "VU0 scalar/flag preservation");
    phase5_require(std::memcmp(actual.cop0, expected.cop0, sizeof actual.cop0) == 0 &&
                   std::memcmp(actual.pcr, expected.pcr, sizeof actual.pcr) == 0,
                   "COP0/PCR preservation");
    phase5_require(actual.budget == expected.budget && actual.base == expected.base &&
                   actual.pcr_at == expected.pcr_at,
                   "host budget/base/performance epoch accounting (not guest cycles)");
}

std::uint32_t phase5_admit_cpu(const ps2::Cpu& c) {
    const auto address = c.r[28].w[0] - 20304u;
    phase5_require(c.r[0].d[0] == 0 && c.r[0].d[1] == 0, "noncanonical saved zero register");
    phase5_require(!(address & 3u) && address <= ps2::kRamSize - 4u,
                   "saved GP does not admit the exact aligned four-byte RAM field");
    phase5_require(!(c.r[31].w[0] & 3u), "saved RA is not an aligned return target");
    phase5_require(c.budget >= std::numeric_limits<std::int32_t>::min() + 3,
                   "saved host budget cannot represent three-instruction accounting");
    return address;
}

void phase5_admit_originals(const fs::path& overlays) {
    for (const auto& which : phase5_cases) {
        phase5_context = std::string("original-") + which.name;
        const auto bytes = read_binary(overlays / (std::string(which.name) + ".ovl"));
        constexpr auto raw_base = 0x007C4880u;
        const auto offset = which.pc - raw_base;
        phase5_require(bytes.size() >= offset + 12u &&
                       std::memcmp(bytes.data(), "MWo3", 4) == 0 &&
                       read_u32_le(bytes.data() + 8u) == raw_base, "original overlay/base admission");
        const std::array<std::uint32_t, 3> original{
            0x24030000u | which.immediate, 0x03E00008u, 0xAF83B0B0u};
        for (unsigned i = 0; i != original.size(); ++i)
            phase5_require(read_u32_le(bytes.data() + offset + i * 4u) == original[i],
                           "complete original ADDIU V1/JR RA/delay-slot SW words");
        std::printf("PHASE5_ORIGINAL module=%s pc=%08X entry=%u file_offset=%08X words=%08X,%08X,%08X\n",
                    which.name, which.pc, which.entry, offset, original[0], original[1], original[2]);
    }
}

void phase5_compare_one(const InitializerCase& which, const ps2::Cpu& saved,
                        const std::vector<std::uint8_t>& initial_ram, bool corrupt_mode) {
    const auto address = phase5_admit_cpu(saved);
    const auto old_mode = read_u32_le(initial_ram.data() + address);
    OverlaySelection owner{old_mode};
    ps2::g_cpu = saved;
    const auto live_v1 = which.typed(owner);
    // The typed code can access only its native owner and scalar liveout.
    // Verify that invoking it does not mutate the reference backing incidentally.
    phase5_compare_cpu(ps2::g_cpu, saved);
    phase5_require(std::memcmp(ps2::g_ram, initial_ram.data(), ps2::kRamSize) == 0,
                   "typed operation mutated unrelated reference RAM");
    if (corrupt_mode) owner.selected_mode = which.immediate == 1u ? 2u : 1u;

    ps2::Cpu expected = saved;
    expected.r[3].d[0] = live_v1;
    expected.pc = saved.r[31].w[0];
    expected.budget = saved.budget - 3; // Host-only generated accounting.
    auto expected_ram = initial_ram;
    for (unsigned i = 0; i != 4; ++i)
        expected_ram[address + i] = std::uint8_t(owner.selected_mode >> (i * 8));
    ps2::g_cpu.pc = which.pc; // Select the boundary; saved inputs remain intact.
    which.raw(ps2::g_cpu, which.entry, 0u);
    phase5_compare_cpu(ps2::g_cpu, expected);
    phase5_require(std::memcmp(ps2::g_ram, expected_ram.data(), ps2::kRamSize) == 0,
                   "full32MiB RAM differential: only selected_mode may change");
    // Full comparison proved all other bytes unchanged; restore the four bytes.
    std::copy_n(initial_ram.data() + address, 4, ps2::g_ram + address);
    ps2::g_cpu = saved;
}

unsigned phase5_synthetic(bool corrupt_mode) {
    const std::array<std::uint32_t, 4> targets{0u, 4u, 0x00123454u, ps2::kRamSize - 4u};
    const std::array<std::uint32_t, 4> old_values{0u, 1u, 2u, 0xffffffffu};
    unsigned comparisons = 0;
    for (unsigned sample = 0; sample != targets.size(); ++sample) {
        ps2::Cpu c;
        std::memset(&c, 0xa5 + sample, sizeof c);
        c.r[0].d[0] = c.r[0].d[1] = 0;
        c.r[28].w[0] = targets[sample] + 20304u;
        c.r[31].w[0] = sample & 1u ? 0xfffffffcu : 0x00100000u;
        c.r[3].d[0] = 0xfedcba9876543210ull;
        c.r[3].d[1] = 0x0123456789abcdefull ^ sample;
        c.budget = sample & 1u ? std::numeric_limits<std::int32_t>::min() + 3 :
                                std::numeric_limits<std::int32_t>::max();
        for (std::size_t i = 0; i != ps2::kRamSize; ++i)
            ps2::g_ram[i] = std::uint8_t((i * 17u) ^ (i >> 8) ^ sample);
        for (unsigned i = 0; i != 4; ++i)
            ps2::g_ram[targets[sample] + i] = std::uint8_t(old_values[sample] >> (i * 8));
        const std::vector<std::uint8_t> ram(ps2::g_ram, ps2::g_ram + ps2::kRamSize);
        for (const auto& which : phase5_cases) {
            phase5_context = std::string("synthetic-") + std::to_string(sample) + "-" + which.name;
            phase5_compare_one(which, c, ram, corrupt_mode && comparisons == 0);
            ++comparisons;
        }
    }
    return comparisons;
}

void phase5_benchmark(const ps2::Cpu& loaded) {
    phase5_context = "benchmark-admission";
    const auto address = phase5_admit_cpu(loaded);
    const auto old_mode = read_u32_le(ps2::g_ram + address);
    constexpr std::uint32_t iterations = 200000;
    ps2::Cpu c = loaded;
    c.budget = std::numeric_limits<std::int32_t>::max(); // Explicit benchmark host allowance.
    OverlaySelection owner{old_mode};
    std::array<RawUnit, 2> raw_functions{phase5_cases[0].raw, phase5_cases[1].raw};
    std::array<TypedInitializer, 2> typed_functions{phase5_cases[0].typed, phase5_cases[1].typed};
    std::uint64_t raw_checksum = 0, typed_checksum = 0;
    const auto raw_start = std::chrono::steady_clock::now();
    for (std::uint32_t i = 0; i != iterations; ++i) for (unsigned k = 0; k != 2; ++k) {
        RawUnit volatile fn = raw_functions[k];
        c.pc = phase5_cases[k].pc;
        fn(c, phase5_cases[k].entry, 0u);
        raw_checksum += c.r[3].d[0] + read_u32_le(ps2::g_ram + address);
    }
    const auto raw_end = std::chrono::steady_clock::now();
    const auto typed_start = std::chrono::steady_clock::now();
    for (std::uint32_t i = 0; i != iterations; ++i) for (unsigned k = 0; k != 2; ++k) {
        TypedInitializer volatile fn = typed_functions[k];
        const auto value = fn(owner);
        typed_checksum += value + owner.selected_mode;
    }
    const auto typed_end = std::chrono::steady_clock::now();
    phase5_benchmark_sink = raw_checksum ^ (typed_checksum << 1);
    phase5_require(raw_checksum == 6ull * iterations && typed_checksum == raw_checksum,
                   "benchmark observable checksums");
    const auto raw_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(raw_end - raw_start).count();
    const auto typed_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(typed_end - typed_start).count();
    std::printf("PHASE5_BENCH iterations=%u calls_per_route=%u generated_ns=%lld typed_ns=%lld checksum=%llu "
                "typed_owner_bytes=%zu typed_liveout_bytes=%zu reference_cpu_bytes=%zu reference_ram_bytes=%zu "
                "scope=isolated_repeated_initializer_not_frame_or_process_memory\n",
                iterations, iterations * 2u, static_cast<long long>(raw_ns), static_cast<long long>(typed_ns),
                static_cast<unsigned long long>(raw_checksum), sizeof owner, sizeof(std::uint64_t),
                sizeof c, std::size_t(ps2::kRamSize));
    for (unsigned i = 0; i != 4; ++i) ps2::g_ram[address + i] = std::uint8_t(old_mode >> (i * 8));
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4 || (argc == 4 && std::string(argv[3]) != "--planted-failure-mode")) {
        std::fprintf(stderr, "Usage: %s <oracle_states_dir> <original_overlays_dir> [--planted-failure-mode]\n", argv[0]);
        return 2;
    }
    static_assert(sizeof(OverlaySelection) == sizeof(std::uint32_t));
    static_assert(noexcept(mka::retail::generated::initialize_gamer(std::declval<OverlaySelection&>())));
    static_assert(noexcept(mka::retail::generated::initialize_movier(std::declval<OverlaySelection&>())));
    phase5_admit_originals(fs::path(argv[2]));
    const auto synthetic = phase5_synthetic(argc == 4);
    std::vector<fs::path> states;
    for (const auto& entry : fs::directory_iterator(fs::path(argv[1])))
        if (entry.is_regular_file() && entry.path().extension() == ".state") states.push_back(entry.path());
    std::sort(states.begin(), states.end());
    phase5_require(states.size() == 61u, "expected61 pinned .state inputs");
    unsigned comparisons = 0;
    ps2::Cpu last_loaded{};
    for (const auto& path : states) {
        const auto raw = read_binary(path);
        OracleVuLayout layout;
        std::string reason;
        phase5_context = path.filename().string();
        if (!load_oracle_ee_vu(raw, layout, reason)) {
            std::fprintf(stderr, "FAIL Phase5 state=%s source_admission=%s\n", path.filename().string().c_str(), reason.c_str());
            return 1;
        }
        check_restored_vu(raw, layout);
        const auto saved = ps2::g_cpu;
        const std::vector<std::uint8_t> ram(ps2::g_ram, ps2::g_ram + ps2::kRamSize);
        for (const auto& which : phase5_cases) {
            phase5_context = path.filename().string() + ":" + which.name;
            phase5_compare_one(which, saved, ram, false);
            ++comparisons;
        }
        last_loaded = saved;
    }
    phase5_benchmark(last_loaded);
    std::printf("PASS_PHASE5_GENERATED_TRANSFORM states=%zu generated_typed_comparisons=%u synthetic_comparisons=%u "
                "modules=GAMER_007C4980_entry1,MOVIER_0084A100_entry8 liveout=V1_low64 "
                "full_register_fields=PASS full_RAM_bytes=%zu scope=selected_snapshot_seeded_boundaries_not_gameplay\n",
                states.size(), comparisons, synthetic, std::size_t(ps2::kRamSize));
    return 0;
}
