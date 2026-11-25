#include "hw.hpp"
#include "gs/gif.hpp"
#include "gs/queue.hpp"
#include "ps2/generated.hpp"
#include "ps2/os.hpp"
#include "ps2/system.hpp"
#include "ps2/vu.hpp"
#include "ps2/vu_ops.hpp"
#include "vu_micro.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <regex>
#include <set>
#include <type_traits>
#include <string>
#include <vector>

void register_generated_code();
void register_generated_vu();

// Headless runtime backing state for dispatch.cpp + cop.cpp + vu_micro.cpp + generated units
namespace ps2 {
alignas(64) std::uint8_t g_ram[kRamSize + 64];
alignas(64) std::uint8_t g_spr[kScratchpadSize];
alignas(64) std::uint8_t g_vu0_code[4096];
alignas(64) std::uint8_t g_vu0_mem[4096];
alignas(64) std::uint8_t g_vu1_code[16384];
alignas(64) std::uint8_t g_vu1_mem[16384];
Cpu g_cpu;
Options g_options;
bool g_watch_on = false;
std::uint32_t g_watch_base = 0;
std::uint32_t g_watch_end = 0;

void watch_hit(std::uint32_t, std::uint32_t, const char *) {}
bool preempt(Cpu &) { return false; }
void halt(const std::string &why) {
    std::fprintf(stderr, "FATAL halt: %s\n", why.c_str());
    std::abort();
}
void syscall(Cpu &, std::uint32_t) { halt("unexpected syscall in leaf/overlay equivalence test"); }
void set_interrupts(Cpu &, bool) {}
std::uint32_t mfc0(Cpu &c, std::uint32_t reg, std::uint32_t) { return reg < 32u ? c.cop0[reg] : 0u; }
void mtc0(Cpu &c, std::uint32_t reg, std::uint32_t v) { if (reg < 32u) c.cop0[reg] = v; }
std::uint32_t eret(Cpu &c) { return c.cop0[14]; }
void drain_vu_thread() {}
std::uint64_t now() { return 0; }
std::uint64_t hblank_cycles() { return 0; }
void request_iop_sync() {}
void sync_iop() {}
namespace vif { void transfer(int, const std::uint8_t *, std::uint32_t) {} }
namespace dmac { bool cpcond0() { return true; } }
std::uint8_t slow_ld8(std::uint32_t) { halt("slow_ld8"); return 0; }
std::uint16_t slow_ld16(std::uint32_t) { halt("slow_ld16"); return 0; }
std::uint32_t slow_ld32(std::uint32_t) { halt("slow_ld32"); return 0; }
std::uint64_t slow_ld64(std::uint32_t) { halt("slow_ld64"); return 0; }
void slow_ld128(std::uint32_t, Reg128 &) { halt("slow_ld128"); }
void slow_st8(std::uint32_t, std::uint8_t) { halt("slow_st8"); }
void slow_st16(std::uint32_t, std::uint16_t) { halt("slow_st16"); }
void slow_st32(std::uint32_t, std::uint32_t) { halt("slow_st32"); }
void slow_st64(std::uint32_t, std::uint64_t) { halt("slow_st64"); }
void slow_st128(std::uint32_t, const Reg128 &) { halt("slow_st128"); }

namespace gs::queue {
bool vu_active() { return false; }
Role &role() {
    static Role r = Role::Emulation;
    return r;
}
Ring &vu() { std::abort(); }
void Ring::post(Type, std::uint32_t, std::uint32_t, const void *, std::uint32_t) { std::abort(); }
}  // namespace gs::queue

namespace gif {
void transfer(int, const std::uint8_t *, std::uint32_t) {}
}  // namespace gif
}  // namespace ps2

namespace gen {
void unit_0000(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void unit_0001(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void unit_002E(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void m_GAMER_unit_0000(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
void m_MOVIER_unit_0000(ps2::Cpu &c, std::uint32_t entry, std::uint32_t depth);
}  // namespace gen

namespace {

namespace fs = std::filesystem;

void check(bool ok, const char *why) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", why);
        std::exit(1);
    }
}

// Version8 native save ABI, independently checked against the protected backup's
// cpu.hpp and system.cpp:machine_state/vu_micro.cpp:state/ipu/ipu.cpp:state.
// No reinterpretation of unknown versions or host layouts is admitted.
static_assert(std::is_trivially_copyable_v<ps2::Cpu> && std::is_trivially_copyable_v<ps2::Vu0Regs>);
static_assert(sizeof(ps2::Cpu) == 1456 && alignof(ps2::Cpu) == 16);
static_assert(offsetof(ps2::Cpu, pc) == 548 && offsetof(ps2::Cpu, f) == 560 &&
              offsetof(ps2::Cpu, vu0) == 704 && offsetof(ps2::Cpu, cop0) == 1312 &&
              offsetof(ps2::Cpu, pcr) == 1440 && offsetof(ps2::Cpu, pcr_at) == 1448);
static_assert(sizeof(ps2::Vu0Regs) == 608 && alignof(ps2::Vu0Regs) == 16 &&
              offsetof(ps2::Vu0Regs, acc) == 512 && offsetof(ps2::Vu0Regs, vi) == 528 &&
              offsetof(ps2::Vu0Regs, q) == 560 && offsetof(ps2::Vu0Regs, tpc) == 592);
static_assert(sizeof(bool) == 1 && ps2::kRamSize == 33554432 && ps2::kScratchpadSize == 16384);
static_assert(std::endian::native == std::endian::little);

struct OracleVuLayout {
    static constexpr std::size_t cpu = 20, ram = cpu + 1456, scratch = ram + 33554432;
    static constexpr std::size_t frame_offset = scratch + 16384 + 8 + 8;
    std::size_t vu_tag{}, regs{}, code0{}, data0{}, code1{}, data1{}, ipu_tag{}, gs_tag{};
    std::uint32_t top{}, itop{};
    std::uint64_t frame{};
};

bool source_vu_regions(const OracleVuLayout& x) {
    // This invariant rejects the old gs-minus40960 model regardless of code
    // hash equality, AOT decodability or how many snapshots repeat that hash.
    return x.regs == x.vu_tag + 8 && x.code0 == x.regs + 608 + 4 + 4 &&
           x.data0 == x.code0 + 4096 && x.code1 == x.data0 + 4096 &&
           x.data1 == x.code1 + 16384 && x.ipu_tag == x.data1 + 16384 &&
           x.gs_tag >= x.ipu_tag && x.gs_tag - x.ipu_tag >= 235;
}

bool parse_oracle_ee_vu_regions_v8(const std::vector<std::uint8_t>& raw, OracleVuLayout& result, std::string& reason) {
    auto fits = [&](std::size_t at, std::size_t n) { return at <= raw.size() && n <= raw.size() - at; };
    auto tag = [&](std::size_t at, const char* text) {
        char expected[8]{};
        std::strncpy(expected, text, sizeof expected);
        return fits(at, 8) && std::memcmp(raw.data() + at, expected, 8) == 0;
    };
    auto u32 = [&](std::size_t at) {
        return std::uint32_t(raw[at]) | std::uint32_t(raw[at + 1]) << 8 |
               std::uint32_t(raw[at + 2]) << 16 | std::uint32_t(raw[at + 3]) << 24;
    };
    auto u64 = [&](std::size_t at) { return std::uint64_t(u32(at)) | std::uint64_t(u32(at + 4)) << 32; };
    auto refuse = [&](const char* message) { reason = message; return false; };
    if (!fits(0, 12) || std::memcmp(raw.data(), "PS2STATE", 8) != 0)
        return refuse("missing/truncated PS2STATE header");
    if (u32(8) != 8) return refuse("unsupported state version; only source-proved native version8 admitted");
    if (!tag(12, "ee") || !fits(OracleVuLayout::scratch, 16384 + 8 * 4 + 1 + 4))
        return refuse("truncated/malformed version8 EE/CPU/RAM/scratch/metadata prefix");
    if (raw.size() < 8 || !tag(raw.size() - 8, "end"))
        return refuse("truncated state or missing final source end tag");

    // The format has unframed tags, not a chunk-size directory. We do not claim
    // to deserialize kernel/hw/dmac/vif or GS/IOP/SIF. Require a UNIQUE bounded
    // VU -> fully sized IPU -> GS chain, never an unqualified first tag match.
    const char vu_tag[8] = {'v','u',0,0,0,0,0,0};
    std::size_t from = OracleVuLayout::scratch + 16384 + 8 * 4 + 1 + 4;
    unsigned candidates = 0;
    OracleVuLayout found;
    while (from < raw.size()) {
        const auto it = std::search(raw.begin() + static_cast<std::ptrdiff_t>(from), raw.end(), vu_tag, vu_tag + 8);
        if (it == raw.end()) break;
        OracleVuLayout x;
        x.vu_tag = static_cast<std::size_t>(it - raw.begin());
        from = x.vu_tag + 1;
        if (!fits(x.vu_tag, 8 + 608 + 8 + 40960 + 8)) continue;
        x.regs = x.vu_tag + 8;
        x.code0 = x.regs + 608 + 8;
        x.data0 = x.code0 + 4096;
        x.code1 = x.data0 + 4096;
        x.data1 = x.code1 + 16384;
        x.ipu_tag = x.data1 + 16384;
        if (!tag(x.ipu_tag, "ipu")) continue;
        std::size_t at = x.ipu_tag + 8;
        auto skip = [&](std::size_t n) { if (!fits(at, n)) return false; at += n; return true; };
        auto vector_bytes = [&] {
            if (!fits(at, 4)) return false;
            const auto n = u32(at); at += 4;
            return skip(n); // Stream::vec<uint8_t>: u32 count, then exactly count bytes.
        };
        auto boolean = [&] { if (!fits(at, 1) || raw[at] > 1) return false; ++at; return true; };
        // Original IPU container layout only, not an IPU/device implementation:
        // input bytes,64-bit bit index,output bytes,64-bit output index; then
        // ctrl/result/command/last,4 bools,phase/qsc/count,first and fixed arrays.
        if (!vector_bytes() || !skip(8) || !vector_bytes() || !skip(8) || !skip(4 * 4) ||
            !boolean() || !boolean() || !boolean() || !boolean() || !skip(3 * 4) || !boolean() ||
            !skip(64 + 64 + 16 * 2 + 2 + 2 + 3 * 2) || !tag(at, "gs")) continue;
        x.gs_tag = at;
        if (!source_vu_regions(x)) continue;
        x.top = u32(x.regs + 608);
        x.itop = u32(x.regs + 608 + 4);
        x.frame = u64(OracleVuLayout::frame_offset);
        found = x;
        ++candidates;
    }
    if (candidates != 1) return refuse(candidates ? "ambiguous source-sized VU/IPU/GS chains" :
                                                  "no complete source-sized VU/IPU/GS chain");
    result = found;
    reason.clear();
    return true;
}

bool load_oracle_ee_vu(const std::vector<std::uint8_t>& raw, OracleVuLayout& layout, std::string& reason) {
    OracleVuLayout parsed;
    // ALL bounds/version/structure admission precedes every global mutation.
    if (!parse_oracle_ee_vu_regions_v8(raw, parsed, reason)) return false;
    std::memcpy(&ps2::g_cpu, raw.data() + OracleVuLayout::cpu, sizeof(ps2::Cpu));
    std::memcpy(ps2::g_ram, raw.data() + OracleVuLayout::ram, ps2::kRamSize);
    std::memcpy(ps2::g_spr, raw.data() + OracleVuLayout::scratch, ps2::kScratchpadSize);
    std::memcpy(&ps2::vu::regs(1), raw.data() + parsed.regs, sizeof(ps2::Vu0Regs));
    ps2::vu::set_tops(parsed.top, parsed.itop);
    std::memcpy(ps2::g_vu0_code, raw.data() + parsed.code0, 4096);
    std::memcpy(ps2::g_vu0_mem, raw.data() + parsed.data0, 4096);
    std::memcpy(ps2::g_vu1_code, raw.data() + parsed.code1, 16384);
    std::memcpy(ps2::g_vu1_mem, raw.data() + parsed.data1, 16384);
    ps2::vu::code_changed(0);
    ps2::vu::code_changed(1);
    layout = parsed;
    return true;
}

void check_restored_vu(const std::vector<std::uint8_t>& raw, const OracleVuLayout& x) {
    check(std::memcmp(&ps2::g_cpu, raw.data() + OracleVuLayout::cpu, sizeof(ps2::Cpu)) == 0 &&
          std::memcmp(ps2::g_ram, raw.data() + OracleVuLayout::ram, ps2::kRamSize) == 0 &&
          std::memcmp(ps2::g_spr, raw.data() + OracleVuLayout::scratch, ps2::kScratchpadSize) == 0,
          "exact admitted EE CPU/RAM/scratch restoration or failed-load preservation");
    check(std::memcmp(&ps2::vu::regs(1), raw.data() + x.regs, sizeof(ps2::Vu0Regs)) == 0 &&
          ps2::vu::top() == x.top && ps2::vu::itop() == x.itop,
          "exact source VU1 registers/TOP/ITOP restoration or failed-load preservation");
    check(std::memcmp(ps2::g_vu0_code, raw.data() + x.code0, 4096) == 0 &&
          std::memcmp(ps2::g_vu0_mem, raw.data() + x.data0, 4096) == 0 &&
          std::memcmp(ps2::g_vu1_code, raw.data() + x.code1, 16384) == 0 &&
          std::memcmp(ps2::g_vu1_mem, raw.data() + x.data1, 16384) == 0,
          "all four source-mapped VU code/data extents, not GS-relative extraction");
}

void test_oracle_loader_rejections(const std::vector<std::uint8_t>& raw, const OracleVuLayout& x) {
    OracleVuLayout old = x;
    old.code0 = x.gs_tag - 40960;
    old.data0 = x.gs_tag - 36864;
    old.code1 = x.gs_tag - 32768;
    old.data1 = x.gs_tag - 16384;
    check(source_vu_regions(x) && !source_vu_regions(old),
          "reject backwards-from-GS mapping despite repeated/decodable code identities");
    auto refused_without_mutation = [&](const std::vector<std::uint8_t>& bad, const char* label) {
        OracleVuLayout result;
        std::string why;
        check(!load_oracle_ee_vu(bad, result, why) && !why.empty(), label);
        check_restored_vu(raw, x);
    };
    // Grow ONLY the serialized IPU input vector in an in-memory schema fixture.
    // VU bytes/keys are identical, but GS moves: the admitted VU mapping MUST NOT.
    auto expanded = raw;
    std::uint32_t input_count{};
    std::memcpy(&input_count, raw.data() + x.ipu_tag + 8, sizeof input_count);
    check(input_count != std::numeric_limits<std::uint32_t>::max(), "IPU regression vector count is extensible");
    expanded.insert(expanded.begin() + static_cast<std::ptrdiff_t>(x.ipu_tag + 12 + input_count), 0x5a);
    for (unsigned i = 0; i < 4; ++i)
        expanded[x.ipu_tag + 8 + i] = static_cast<std::uint8_t>((input_count + 1u) >> (i * 8));
    OracleVuLayout moved_gs;
    std::string expanded_reason;
    check(parse_oracle_ee_vu_regions_v8(expanded, moved_gs, expanded_reason) &&
          moved_gs.gs_tag == x.gs_tag + 1 && moved_gs.code0 == x.code0 && moved_gs.code1 == x.code1 &&
          std::memcmp(expanded.data() + moved_gs.code1, raw.data() + x.code1, 16384) == 0,
          "variable IPU gap moves GS without moving identical VU code; code hashes/keys cannot validate backwards extraction");
    check_restored_vu(raw, x); // Parsing a schema fixture never restores global owners.
    auto bad = raw;
    bad.resize(11);
    refused_without_mutation(bad, "truncated state header is refused");
    bad = raw;
    bad[12] = 'x';
    refused_without_mutation(bad, "wrong required EE tag is refused");
    bad = raw;
    bad[8] = 9; bad[9] = bad[10] = bad[11] = 0;
    refused_without_mutation(bad, "unsupported version is refused");
    bad = raw;
    bad.resize(x.code1 + 16383);
    const std::array<std::uint8_t, 8> end_tag{'e','n','d',0,0,0,0,0};
    bad.insert(bad.end(), end_tag.begin(), end_tag.end());
    refused_without_mutation(bad, "truncated VU1 extent is refused even with an end tag");
    bad = raw;
    bad[OracleVuLayout::cpu] ^= 1; // Would expose an erroneous partial CPU restore.
    bad[x.ipu_tag + 8] = bad[x.ipu_tag + 9] = bad[x.ipu_tag + 10] = bad[x.ipu_tag + 11] = 0xff;
    refused_without_mutation(bad, "malformed IPU vector extent is refused before any owner mutation");
    bad = raw;
    bad[x.ipu_tag] = 'x';
    refused_without_mutation(bad, "missing mandatory IPU tag is refused");
    bad = raw;
    bad.insert(bad.begin() + static_cast<std::ptrdiff_t>(x.ipu_tag), 0xaa);
    refused_without_mutation(bad, "undeclared gap between VU extents and IPU is refused");
    bad = raw;
    bad[x.gs_tag] = 'x';
    refused_without_mutation(bad, "GS tag must follow the parsed IPU payload");
    bad = raw;
    bad.insert(bad.end() - 8, raw.begin() + static_cast<std::ptrdiff_t>(x.vu_tag),
               raw.begin() + static_cast<std::ptrdiff_t>(x.gs_tag + 8));
    refused_without_mutation(bad, "multiple source-sized VU/IPU/GS candidates are refused rather than first-tag selection");
    bad = raw;
    bad.back() = 1;
    refused_without_mutation(bad, "missing final source end tag is refused");
}

std::vector<std::uint8_t> read_binary(const fs::path &p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "FAIL: cannot open %s\n", p.string().c_str());
        std::exit(1);
    }
    return std::vector<std::uint8_t>{std::istreambuf_iterator<char>(in), {}};
}

std::uint32_t read_u32_le(const std::uint8_t *p) {
    std::uint32_t v = 0;
    std::memcpy(&v, p, sizeof v);
    return v;
}

std::uint32_t f32_bits(float f) { return std::bit_cast<std::uint32_t>(f); }

std::uint64_t fnv1a64(const std::uint8_t *data, std::size_t n) {
    std::uint64_t h = 14695981039346656037ull;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}

void init_clean_cpu(ps2::Cpu &c, std::uint32_t ra = 0x00100000u) {
    std::memset(&c, 0, sizeof c);
    c.budget = 1000000;
    c.r[31].sd[0] = static_cast<std::int32_t>(ra);
    c.r[29].sd[0] = static_cast<std::int32_t>(0x00190000u);
    c.r[28].sd[0] = static_cast<std::int32_t>(0x00188000u);
    c.vu0.vf[0].w[3] = 0x3F800000u;
    c.pc = 0;
}

void test_ee_fpu_kernel_and_denormals() {
    std::memset(ps2::g_ram + 0x00180000u, 0, 256u);
    {
        // Case 1: Normal finite inputs cleanly testing mul/sub/add lanes in unit_0001 entry 215 (0x00104004)
        ps2::Cpu c;
        init_clean_cpu(c);
        c.r[4].sd[0] = static_cast<std::int32_t>(0x00180000u);
        c.f[0] = f32_bits(2.0f);
        c.f[1] = f32_bits(3.0f);
        c.f[2] = f32_bits(-4.0f);
        c.f[3] = f32_bits(5.0f);
        c.f[4] = f32_bits(11.0f);
        c.f[5] = f32_bits(0.5f);
        c.f[6] = f32_bits(6.0f);
        c.f[7] = f32_bits(7.0f);
        c.f[9] = f32_bits(1.25f);
        c.f[10] = f32_bits(-2.5f);
        c.f[14] = f32_bits(2.5f);
        c.f[15] = f32_bits(3.5f);
        c.f[16] = f32_bits(-1.5f);

        c.pc = 0x00104004u;
        ps2::dispatch(c, 0u, 0u);
        check(c.pc == 0x00100000u, "unit_0001 fpu return target");
        check(ps2::ld32(0x00180000u) == f32_bits(3.25f), "fpu [a0+0] = f9 + f0");
        check(ps2::ld32(0x00180004u) == f32_bits(11.0f), "fpu [a0+4] = f4");
        check(ps2::ld32(0x00180008u) == f32_bits(-7.5f), "fpu [a0+8] = f16 * f3");
        check(ps2::ld32(0x00180010u) == f32_bits(7.0f), "fpu [a0+16] = f7");
        check(ps2::ld32(0x00180014u) == f32_bits(-0.5f), "fpu [a0+20] = f10 + f0");
        check(ps2::ld32(0x00180018u) == f32_bits(17.5f), "fpu [a0+24] = f15 * f3");
        check(ps2::ld32(0x00180020u) == f32_bits(7.5f), "fpu [a0+32] = f14 * f1");
        check(ps2::ld32(0x00180024u) == f32_bits(-10.0f), "fpu [a0+36] = f14 * f2");
        check(ps2::ld32(0x00180028u) == f32_bits(8.0f), "fpu [a0+40] = f6 + f0");
        check(ps2::ld32(0x0018003Cu) == f32_bits(1.5f), "fpu [a0+60] = f0 - f5");
    }

    {
        // Case 2: PS2 EE denormal-to-zero flushing (denormals 0x00400001 and 0x80400001 flush to signed zero)
        ps2::Cpu c;
        init_clean_cpu(c);
        c.r[4].sd[0] = static_cast<std::int32_t>(0x00180040u);
        c.f[0] = 0x00400001u;  // +denorm -> +0
        c.f[1] = 0x00400001u;  // +denorm * 2.0 -> +0 on EE (IEEE would be non-zero)
        c.f[2] = 0x80400001u;  // -denorm * 2.0 -> -0 on EE
        c.f[3] = 0x01000000u;  // tiny normal (~1.175e-38)
        c.f[5] = f32_bits(1.0f);
        c.f[9] = f32_bits(3.0f);
        c.f[14] = f32_bits(2.0f);
        c.f[15] = 0x3E800000u; // 0.25f * 1.175e-38 -> underflows to +0 on EE
        c.f[16] = 0xBE800000u; // -0.25f * 1.175e-38 -> underflows to -0 on EE

        c.pc = 0x00104004u;
        ps2::dispatch(c, 0u, 0u);
        check(ps2::ld32(0x00180040u) == f32_bits(3.0f), "ee denorm +f0 flushed in add");
        check(ps2::ld32(0x00180060u) == 0x00000000u, "ee denorm +f1*2 flushed to +0");
        check(ps2::ld32(0x00180064u) == 0x80000000u, "ee denorm -f2*2 flushed to -0");
        check(ps2::ld32(0x00180048u) == 0x80000000u, "ee underflow mul flushed to -0");
        check(ps2::ld32(0x00180058u) == 0x00000000u, "ee underflow mul flushed to +0");
        check(ps2::ld32(0x0018007Cu) == f32_bits(-1.0f), "ee +denorm - 1.0 == -1.0");
    }
}

void test_ee_vu0_mmi_matrix_kernel() {
    // unit_002E entry 132 (L_001BB260): out(a0) = mat4x3(a2) * vec3(a1)
    const std::uint32_t out_addr = 0x00180200u;
    const std::uint32_t vec_addr = 0x00180220u;
    const std::uint32_t mat_addr = 0x00180240u;

    auto write_vec4 = [](std::uint32_t addr, float x, float y, float z, float w) {
        ps2::st32(addr + 0u, f32_bits(x));
        ps2::st32(addr + 4u, f32_bits(y));
        ps2::st32(addr + 8u, f32_bits(z));
        ps2::st32(addr + 12u, f32_bits(w));
    };

    // Matrix columns: scale(2, 3, -4) + translation(10, -20, 30)
    write_vec4(mat_addr + 0u, 2.0f, 0.0f, 0.0f, 0.0f);
    write_vec4(mat_addr + 16u, 0.0f, 3.0f, 0.0f, 0.0f);
    write_vec4(mat_addr + 32u, 0.0f, 0.0f, -4.0f, 0.0f);
    write_vec4(mat_addr + 48u, 10.0f, -20.0f, 30.0f, 1.0f);
    write_vec4(vec_addr, 1.5f, -2.0f, 0.5f, 0.0f);

    ps2::Cpu c;
    init_clean_cpu(c);
    c.r[4].sd[0] = static_cast<std::int32_t>(out_addr);
    c.r[5].sd[0] = static_cast<std::int32_t>(vec_addr);
    c.r[6].sd[0] = static_cast<std::int32_t>(mat_addr);

    c.pc = 0x001BB260u;
    ps2::dispatch(c, 0u, 0u);
    check(c.pc == 0x00100000u, "unit_002E matrix return target");
    check(c.r[2].w[0] == out_addr, "unit_002E returns a0");
    check(ps2::ld32(out_addr + 0u) == f32_bits(13.0f), "mat*vec x = 2*1.5 + 10 = 13");
    check(ps2::ld32(out_addr + 4u) == f32_bits(-26.0f), "mat*vec y = 3*-2 - 20 = -26");
    check(ps2::ld32(out_addr + 8u) == f32_bits(28.0f), "mat*vec z = -4*0.5 + 30 = 28");

    // Non-diagonal 90-degree Z rotation + translation: (-y + 1, x + 2, z - 3)
    write_vec4(mat_addr + 0u, 0.0f, 1.0f, 0.0f, 0.0f);
    write_vec4(mat_addr + 16u, -1.0f, 0.0f, 0.0f, 0.0f);
    write_vec4(mat_addr + 32u, 0.0f, 0.0f, 1.0f, 0.0f);
    write_vec4(mat_addr + 48u, 1.0f, 2.0f, -3.0f, 1.0f);
    write_vec4(vec_addr, 4.0f, 5.0f, 6.0f, 0.0f);

    init_clean_cpu(c);
    c.r[4].sd[0] = static_cast<std::int32_t>(out_addr);
    c.r[5].sd[0] = static_cast<std::int32_t>(vec_addr);
    c.r[6].sd[0] = static_cast<std::int32_t>(mat_addr);

    c.pc = 0x001BB260u;
    ps2::dispatch(c, 0u, 0u);
    check(ps2::ld32(out_addr + 0u) == f32_bits(-4.0f), "rot90 x = -5 + 1 = -4");
    check(ps2::ld32(out_addr + 4u) == f32_bits(6.0f), "rot90 y = 4 + 2 = 6");
    check(ps2::ld32(out_addr + 8u) == f32_bits(3.0f), "rot90 z = 6 - 3 = 3");
}

void test_ee_branch_likely_annulment() {
    // unit_0000 entry 533 (L_001003B4):
    // beql r2, r0, +delay(r2 = 5) -> if r4 != 0 exits via L_001004EC restoring s0/s1/s2 and JR ra.
    // Note L_001003C4/D4 writes s0/s1/s2 from stack sp+0/16/32 and JR target from sp+64;
    // sp += 80 happens in L_001004F8.
    const std::uint32_t sp = 0x00190000u;
    ps2::st64(sp + 0u, 0x11111111ull);
    ps2::st64(sp + 16u, 0x22222222ull);
    ps2::st64(sp + 32u, 0x33333333ull);
    ps2::st64(sp + 48u, 0x44444444ull);
    ps2::st64(sp + 64u, 0x00100000ull);

    {
        ps2::Cpu c;
        init_clean_cpu(c);
        c.r[2].d[0] = 0ull;
        c.r[4].d[0] = 1ull;
        c.pc = 0x001003B4u;
        ps2::dispatch(c, 0u, 0u);
        check(c.pc == 0x00100000u, "branch-likely taken return");
        check(c.r[2].d[0] == 5ull, "branch-likely taken executes delay slot (r2=5)");
        check(c.r[16].d[0] == 0x11111111ull && c.r[19].d[0] == 0x44444444ull, "epilogue restores callee-saved");
    }

    {
        ps2::Cpu c;
        init_clean_cpu(c);
        c.r[2].d[0] = 42ull;
        c.r[4].d[0] = 1ull;
        c.pc = 0x001003B4u;
        ps2::dispatch(c, 0u, 0u);
        check(c.pc == 0x00100000u, "branch-likely untaken return");
        check(c.r[2].d[0] == 42ull, "branch-likely untaken annuls delay slot (r2 stays 42)");
    }
}

void test_overlay_switching(const fs::path &overlays_dir) {
    const std::vector<std::uint8_t> gamer = read_binary(overlays_dir / "GAMER.ovl");
    const std::vector<std::uint8_t> movier = read_binary(overlays_dir / "MOVIER.ovl");

    check(gamer.size() >= 0x40 && std::memcmp(gamer.data(), "MWo3", 4) == 0, "GAMER.ovl MWo3 header");
    check(movier.size() >= 0x40 && std::memcmp(movier.data(), "MWo3", 4) == 0, "MOVIER.ovl MWo3 header");
    check(read_u32_le(gamer.data() + 8u) == 0x007C4880u, "GAMER.ovl base 0x007C4880");
    check(read_u32_le(movier.data() + 8u) == 0x007C4880u, "MOVIER.ovl base 0x007C4880");

    // Clear 0x007C4000 window
    std::memset(ps2::g_ram + 0x007C4000u, 0, 0x00090000u);
    check(!ps2::window_module_in_place(0x007C4980u), "zeroed window does not match GAMER");
    check(!ps2::window_module_in_place(0x007C4900u), "zeroed window does not match MOVIER");

    // Load GAMER.ovl and verify window dispatch
    std::memcpy(ps2::g_ram + 0x007C4880u, gamer.data(), gamer.size());
    check(ps2::window_module_in_place(0x007C4980u), "GAMER.ovl window active at 0x007C4980");
    {
        ps2::Cpu c;
        init_clean_cpu(c, 0x00100000u);
        const std::uint32_t gp = 0x00188000u;
        c.r[28].sd[0] = static_cast<std::int32_t>(gp);
        ps2::st32(gp - 20304u, 0u);
        c.pc = 0x007C4980u;
        ps2::dispatch(c, 0u, 0u);
        check(c.pc == 0x00100000u, "GAMER dispatch returns cleanly");
        check(c.r[3].sd[0] == 1 && ps2::ld32(gp - 20304u) == 1u, "GAMER 0x007C4980 side effect");
    }

    // Overwrite 0x007C4880 with MOVIER.ovl (shared 0x007C4000 address space)
    std::memset(ps2::g_ram + 0x007C4000u, 0, 0x00090000u);
    std::memcpy(ps2::g_ram + 0x007C4880u, movier.data(), movier.size());
    check(!ps2::window_module_in_place(0x007C4980u) || ps2::window_module_in_place(0x007C4900u),
          "MOVIER window active after reload");
    check(ps2::window_module_in_place(0x007C4900u), "MOVIER.ovl check words verified");

    // Execute m_MOVIER leaf at 0x007C4930u (entry 2 of m_MOVIER_unit_0000): increments *r16 and restores r16/ra from sp
    {
        ps2::Cpu c;
        init_clean_cpu(c);
        const std::uint32_t counter_addr = 0x00180400u;
        ps2::st32(counter_addr, 41u);
        const std::uint32_t sp = 0x00190000u;
        c.r[29].sd[0] = static_cast<std::int32_t>(sp);
        c.r[16].sd[0] = static_cast<std::int32_t>(counter_addr);
        ps2::st64(sp + 0u, 0xCAFEBABEull);
        ps2::st64(sp + 8u, 0x00100000ull);
        c.pc = 0x007C4930u;
        ps2::dispatch(c, 0u, 0u);
        check(c.pc == 0x00100000u, "MOVIER leaf return target");
        check(ps2::ld32(counter_addr) == 42u, "MOVIER 0x007C4930 increments counter");
        check(c.r[16].d[0] == 0xCAFEBABEull && c.r[29].w[0] == sp + 16u, "MOVIER epilogue restores sp & s0");
    }

    // Corrupting a word covered by both MOVIER and its legacy 256K window (0x007C4950)
    // must fail window validation within one check-word sweep.
    ps2::st32(0x007C4950u, ps2::ld32(0x007C4950u) ^ 1u);
    bool still_valid = true;
    for (std::uint32_t k = 0; k < 200000u; ++k) {
        if (!ps2::window_module_in_place(0x007C4950u)) {
            still_valid = false;
            break;
        }
    }
    check(!still_valid, "corrupted MOVIER word fails window check within sweep");
}

void test_oracle_states(const fs::path &states_dir, const fs::path &overlays_dir, const fs::path& aot_table) {
    // Original GAMER.ovl SHA256 (pinned input receipt):
    // 70714c6f147128fb6462ac57bfe28e3e27a0dcf90730c4054e90c70908feba89.
    // Raw overlay base007C4880 + file offset100 = complete leaf007C4980.
    // Admit the exact three original words BEFORE using the fixed reference;
    // this is not an interpreter or a reference built from generator helpers.
    static_assert(sizeof(ps2::Reg128) == 16, "field comparison requires the complete128-bit register representation");
    const auto gamer = read_binary(overlays_dir / "GAMER.ovl");
    constexpr std::uint32_t gamer_leaf = 0x007C4980u;
    constexpr std::array<std::uint32_t, 3> leaf_words{
        0x24030001u, // ADDIU v1,zero,1: sign-extended32 result in low64 only.
        0x03E00008u, // JR ra: capture return target, execute the delay slot.
        0xAF83B0B0u  // SW v1,-20304(gp): exactly four little-endian bytes.
    };
    check(gamer.size() >= 0x10cu && std::memcmp(gamer.data(), "MWo3", 4) == 0 &&
          read_u32_le(gamer.data() + 8u) == 0x007C4880u, "pinned GAMER leaf file/base admission");
    for (std::size_t i = 0; i < leaf_words.size(); ++i)
        check(read_u32_le(gamer.data() + 0x100u + 4u * i) == leaf_words[i],
              "GAMER original file100..10B complete leaf words");

    // Read the declared table associated with this build; receipts must pin
    // this source to the compiled table. This is membership evidence only,
    // not an execution/query of the runtime's private registration registry.
    const auto table_bytes = read_binary(aot_table);
    const std::string table_text(table_bytes.begin(), table_bytes.end());
    const std::regex table_entry(R"(\{\s*0x([0-9a-fA-F]{1,16})ull,\s*vu1_([0-9a-fA-F]+)\s*\})");
    std::set<std::uint64_t> declared_keys;
    for (auto it = std::sregex_iterator(table_text.begin(), table_text.end(), table_entry);
         it != std::sregex_iterator(); ++it) {
        const auto key = std::stoull((*it)[1].str(), nullptr, 16);
        check(key == std::stoull((*it)[2].str(), nullptr, 16), "declared VU table key/function-name agreement");
        check(declared_keys.insert(key).second, "duplicate declared VU table key");
    }
    check(!declared_keys.empty(), "declared generated VU table has actual key/function entries");
    std::printf("DECLARED_VU_TABLE_SOURCE path=%s fnv=%016llX keys=%zu scope=source_membership_not_execution\n",
                aot_table.string().c_str(), static_cast<unsigned long long>(fnv1a64(table_bytes.data(), table_bytes.size())),
                declared_keys.size());
    std::set<std::uint64_t> observed_keys, missing_keys;
    unsigned missing_states = 0;
    bool loader_rejections_checked = false;
    std::vector<fs::path> states;
    for (const auto &entry : fs::directory_iterator(states_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".state") {
            states.push_back(entry.path());
        }
    }
    std::sort(states.begin(), states.end());
    check(states.size() == 61u, "expected 61 playable oracle .state files");

    int vu0_nonzero_states = 0;
    int vu1_identity_states = 0;
    int gamer_matched = 0;
    int gamer_leaf_references = 0;
    int unit0152_matched = 0;
    std::uint64_t expected_0152_hash = 0;

    for (const auto &sp : states) {
        const std::vector<std::uint8_t> raw = read_binary(sp);
        OracleVuLayout layout;
        std::string loader_reason;
        if (!load_oracle_ee_vu(raw, layout, loader_reason)) {
            std::fprintf(stderr, "FAIL oracle source layout state=%s: %s\n", sp.string().c_str(), loader_reason.c_str());
            std::exit(1);
        }
        check_restored_vu(raw, layout);
        if (!loader_rejections_checked) {
            test_oracle_loader_rejections(raw, layout);
            loader_rejections_checked = true;
        }

        auto state_check = [&](bool ok, const char* why) {
            if (!ok) {
                std::fprintf(stderr, "FAIL GAMER bounded leaf state=%s: %s; pc=%08X gp=%08X ra=%08X host_budget=%d\n",
                             sp.string().c_str(), why, ps2::g_cpu.pc, ps2::g_cpu.r[28].w[0],
                             ps2::g_cpu.r[31].w[0], ps2::g_cpu.budget);
                std::exit(1);
            }
        };
        for (std::size_t i = 0; i < leaf_words.size(); ++i)
            state_check(read_u32_le(ps2::g_ram + gamer_leaf + 4u * i) == leaf_words[i],
                        "loaded RAM does not contain the three admitted original leaf words");
        const ps2::Cpu saved = ps2::g_cpu;
        state_check(saved.r[0].d[0] == 0 && saved.r[0].d[1] == 0, "saved architectural zero register is nonzero");
        const std::uint32_t address = saved.r[28].w[0] - 20304u;
        state_check((address & 3u) == 0 && address <= ps2::kRamSize - 4u,
                    "actual saved GP does not admit an aligned four-byte RAM store");
        state_check((saved.r[31].w[0] & 3u) == 0, "actual saved RA is not an aligned JR return target");
        state_check(saved.budget >= std::numeric_limits<std::int32_t>::min() + 3,
                    "saved host budget cannot represent the generated three-instruction debit");

        std::vector<std::uint8_t> expected_ram(ps2::g_ram, ps2::g_ram + ps2::kRamSize);
        std::array<std::uint8_t, 4> old_store{};
        std::copy_n(expected_ram.data() + address, old_store.size(), old_store.data());
        // Fixed original three-word reference: no generated arithmetic/store
        // helpers, no invented GP/RA/registers, and no sentinel output overwrite.
        ps2::Cpu expected = saved;
        expected.r[3].d[0] = 1u; // Upper64 of the128-bit GPR remains the saved value.
        expected.pc = saved.r[31].w[0];
        expected_ram[address] = 1u;
        expected_ram[address + 1u] = expected_ram[address + 2u] = expected_ram[address + 3u] = 0u;
        ps2::Cpu& actual = ps2::g_cpu;
        actual.pc = gamer_leaf; // Select this boundary; all saved data inputs remain intact.
        // This overlay's complete leaf entry1 is fixed; synthetic EE tests above
        // use source-address dispatch so base-unit entry-table reseeding is safe.
        gen::m_GAMER_unit_0000(actual, 1u, 0u);

        // Compare fields, not Cpu/Vu0Regs object padding. All128 bits of every
        // GPR/HI/LO/VF/ACC and every architectural scalar/flag are checked.
        state_check(std::memcmp(actual.r, expected.r, sizeof actual.r) == 0, "complete GPR128 delta/preservation");
        state_check(std::memcmp(&actual.hi, &expected.hi, sizeof actual.hi) == 0 &&
                    std::memcmp(&actual.lo, &expected.lo, sizeof actual.lo) == 0, "complete HI/LO preservation");
        state_check(actual.sa == expected.sa && actual.pc == expected.pc, "SA/return PC");
        state_check(std::memcmp(actual.f, expected.f, sizeof actual.f) == 0 &&
                    actual.facc == expected.facc && actual.fcr31 == expected.fcr31, "FPR/FACC/FCR31 preservation");
        const auto& av = actual.vu0;
        const auto& ev = expected.vu0;
        state_check(std::memcmp(av.vf, ev.vf, sizeof av.vf) == 0 &&
                    std::memcmp(&av.acc, &ev.acc, sizeof av.acc) == 0 &&
                    std::memcmp(av.vi, ev.vi, sizeof av.vi) == 0, "VU0 complete VF/ACC/VI preservation");
        state_check(av.q == ev.q && av.p == ev.p && av.i == ev.i && av.r == ev.r &&
                    av.mac == ev.mac && av.status == ev.status && av.clip == ev.clip &&
                    av.cmsar0 == ev.cmsar0 && av.tpc == ev.tpc, "VU0 complete scalar/flag preservation");
        state_check(std::memcmp(actual.cop0, expected.cop0, sizeof actual.cop0) == 0 &&
                    std::memcmp(actual.pcr, expected.pcr, sizeof actual.pcr) == 0, "COP0/PCR preservation");
        // Host bookkeeping is checked separately, never counted as guest ISA
        // semantics: direct unit invocation preserves base/performance epoch;
        // its generated accounting debits budget by3, not3 proven EE cycles.
        state_check(actual.base == saved.base && actual.pcr_at == saved.pcr_at &&
                    actual.budget == saved.budget - 3, "host base/performance epoch/budget bookkeeping");
        state_check(std::memcmp(ps2::g_ram, expected_ram.data(), ps2::kRamSize) == 0,
                    "exact whole-RAM delta: only the reference SW bytes may differ");
        ++gamer_leaf_references;
        // Preserve the independently loaded snapshot for the existing smoke
        // checks below; the full32MiB comparison has already proved every other byte unchanged.
        std::copy(old_store.begin(), old_store.end(), ps2::g_ram + address);
        ps2::g_cpu = saved;

        bool any_vu0 = false;
        for (std::size_t i = 0; i < sizeof(ps2::g_vu0_code); ++i) {
            if (ps2::g_vu0_code[i] != 0) {
                any_vu0 = true;
                break;
            }
        }
        if (any_vu0) ++vu0_nonzero_states;

        // Static inspection of the CORRECT serialized code extent only. Never
        // run loaded/unknown VU packets against the no-op GIF sink below.
        const auto reach = ps2::vu::reachable(ps2::g_vu1_code, 16383u, 0u);
        check(!reach.empty(), "real source VU1 entry0 has a bounded reachable identity");
        const auto key = ps2::vu::program_key(ps2::g_vu1_code, reach, 0u);
        const bool declared = declared_keys.contains(key);
        observed_keys.insert(key);
        if (!declared) { missing_keys.insert(key); ++missing_states; }
        std::printf("ORACLE_VU_SOURCE state=%s frame=%llu vu_tag=%zu regs=%zu top=%u itop=%u "
                    "saved_tpc=%u code0=%zu data0=%zu code1=%zu data1=%zu ipu_tag=%zu ipu_bytes=%zu gs_tag=%zu "
                    "code1_fnv=%016llX entry0_key=%016llX declared_AOT=%s execution=NOT_ATTEMPTED\n",
                    sp.filename().string().c_str(), static_cast<unsigned long long>(layout.frame),
                    layout.vu_tag, layout.regs, layout.top, layout.itop, ps2::vu::tpc(1), layout.code0, layout.data0,
                    layout.code1, layout.data1, layout.ipu_tag, layout.gs_tag - layout.ipu_tag, layout.gs_tag,
                    static_cast<unsigned long long>(fnv1a64(ps2::g_vu1_code, 16384)),
                    static_cast<unsigned long long>(key), declared ? "yes" : "MISSING");
        ++vu1_identity_states;

        // Verify GAMER/GAMEFD window at 0x007C4880 is active in state RAM and dispatchable
        check(ps2::window_module_in_place(0x007C4980u), "state 0x007C4980 matches GAMER AOT window");
        {
            ps2::Cpu test_cpu;
            init_clean_cpu(test_cpu, 0x00100000u);
            const std::uint32_t gp_save = ps2::ld32(0x00188000u - 20304u);
            test_cpu.r[28].sd[0] = static_cast<std::int32_t>(0x00188000u);
            ps2::st32(0x00188000u - 20304u, 0u);
            test_cpu.pc = 0x007C4980u;
            ps2::dispatch(test_cpu, 0u, 0u);
            check(test_cpu.r[3].sd[0] == 1 && ps2::ld32(0x00188000u - 20304u) == 1u, "state GAMER dispatch result");
            ps2::st32(0x00188000u - 20304u, gp_save);
        }
        ++gamer_matched;

        // Verify SLUS_214.10 unit_0152 region (0x00648680..0x00649350) matches across all 61 states
        const std::uint64_t h = fnv1a64(ps2::g_ram + 0x00648680u, 0x0CD0u);
        if (expected_0152_hash == 0) expected_0152_hash = h;
        check(h == expected_0152_hash, "state 0x00648680..0x00649350 bytes match across all 61 states");
        check(ps2::ld32(0x00648680u) == 0x27BDFFE0u, "unit_0152 entry prologue addiu sp,-32");
        ++unit0152_matched;
    }

    check(gamer_leaf_references == static_cast<int>(states.size()), "one complete GAMER leaf reference per admitted state");
    check(loader_rejections_checked, "source layout/malformed/version/IPU-gap regression checks executed");
    for (auto key : missing_keys)
        std::fprintf(stderr, "OPEN missing declared VU1 entry0 AOT key=%016llX; no VU effects executed\n",
                     static_cast<unsigned long long>(key));
    std::printf(
        "%s: Phase4 loader/synthetic smoke and bounded GAMER leaf reference: oracle_states=%zu "
        "vu0_nonzero=%d vu1_source_identity_states=%d distinct_entry0_keys=%zu missing_AOT_states=%u "
        "gamer_window_matched=%d gamer_complete_leaf_references=%d unit_0152_matched=%d hash=%016llX "
        "ee_synthetic_checks=OK overlay_switch=OK VU_execution=NOT_ATTEMPTED; "
        "VU_output_packets=OPEN general_function_boundary_equivalence=OPEN\n",
        missing_keys.empty() ? "PASS" : "OPEN", states.size(), vu0_nonzero_states, vu1_identity_states,
        observed_keys.size(), missing_states, gamer_matched, gamer_leaf_references, unit0152_matched,
        static_cast<unsigned long long>(expected_0152_hash));
    check(missing_keys.empty(), "named real VU1 entry0 AOT keys missing from declared table; no packet execution attempted");
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <oracle_states_dir> <overlays_dir> [generated_vu1_table.cpp]\n", argc > 0 ? argv[0] : "test_phase4_aot_equivalence");
        return 2;
    }
    register_generated_code();
    register_generated_vu();

    test_ee_fpu_kernel_and_denormals();
    test_ee_vu0_mmi_matrix_kernel();
    test_ee_branch_likely_annulment();
    test_overlay_switching(fs::path(argv[2]));
    const fs::path aot_table = argc > 3 ? fs::path(argv[3]) :
        fs::path(__FILE__).parent_path().parent_path() / "build/generated-aot/vu1/vu1_table.cpp";
    test_oracle_states(fs::path(argv[1]), fs::path(argv[2]), aot_table);
    return 0;
}
