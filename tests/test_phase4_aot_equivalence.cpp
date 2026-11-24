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
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
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

bool load_oracle_ee_vu(const std::vector<std::uint8_t> &raw) {
    constexpr std::size_t kHdr = 8u + 4u + 8u;  // PS2STATE + ver + "ee\0\0\0\0\0\0"
    if (raw.size() < kHdr + sizeof(ps2::Cpu) + ps2::kRamSize) return false;
    if (std::memcmp(raw.data(), "PS2STATE", 8) != 0) return false;
    const char ee_tag[8] = {'e', 'e', 0, 0, 0, 0, 0, 0};
    if (std::memcmp(raw.data() + 12u, ee_tag, 8) != 0) return false;

    const std::size_t ram_off = 20u + sizeof(ps2::Cpu);
    std::memcpy(&ps2::g_cpu, raw.data() + 20u, sizeof(ps2::Cpu));
    std::memcpy(ps2::g_ram, raw.data() + ram_off, ps2::kRamSize);

    const char vu_tag[8] = {'v', 'u', 0, 0, 0, 0, 0, 0};
    const char gs_tag[8] = {'g', 's', 0, 0, 0, 0, 0, 0};
    const std::size_t search_start = ram_off + ps2::kRamSize;
    auto it_vu = std::search(raw.begin() + static_cast<std::ptrdiff_t>(search_start), raw.end(), vu_tag, vu_tag + 8);
    if (it_vu == raw.end()) return false;
    auto it_gs = std::search(it_vu + 8, raw.end(), gs_tag, gs_tag + 8);
    if (it_gs == raw.end()) return false;
    const std::size_t gs_off = static_cast<std::size_t>(it_gs - raw.begin());
    if (gs_off < static_cast<std::size_t>(it_vu - raw.begin()) + 8u + 40960u) return false;
    const std::size_t vu0_code_off = gs_off - 40960u;
    const std::size_t vu0_mem_off = gs_off - 36864u;
    const std::size_t vu1_code_off = gs_off - 32768u;
    std::memcpy(ps2::g_vu0_code, raw.data() + vu0_code_off, 4096u);
    std::memcpy(ps2::g_vu0_mem, raw.data() + vu0_mem_off, 4096u);
    std::memcpy(ps2::g_vu1_code, raw.data() + vu1_code_off, 16384u);
    ps2::vu::code_changed(0);
    ps2::vu::code_changed(1);
    return true;
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

        gen::unit_0001(c, 215u, 0u);
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

        gen::unit_0001(c, 215u, 0u);
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

    gen::unit_002E(c, 132u, 0u);
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

    gen::unit_002E(c, 132u, 0u);
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
        gen::unit_0000(c, 533u, 0u);
        check(c.pc == 0x00100000u, "branch-likely taken return");
        check(c.r[2].d[0] == 5ull, "branch-likely taken executes delay slot (r2=5)");
        check(c.r[16].d[0] == 0x11111111ull && c.r[19].d[0] == 0x44444444ull, "epilogue restores callee-saved");
    }

    {
        ps2::Cpu c;
        init_clean_cpu(c);
        c.r[2].d[0] = 42ull;
        c.r[4].d[0] = 1ull;
        gen::unit_0000(c, 533u, 0u);
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
        gen::m_MOVIER_unit_0000(c, 2u, 0u);
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

void test_oracle_states(const fs::path &states_dir) {
    std::vector<fs::path> states;
    for (const auto &entry : fs::directory_iterator(states_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".state") {
            states.push_back(entry.path());
        }
    }
    std::sort(states.begin(), states.end());
    check(states.size() == 61u, "expected 61 playable oracle .state files");

    int vu0_nonzero_states = 0;
    int vu1_executed = 0;
    int gamer_matched = 0;
    int unit0152_matched = 0;
    std::uint64_t expected_0152_hash = 0;

    for (const auto &sp : states) {
        const std::vector<std::uint8_t> raw = read_binary(sp);
        check(load_oracle_ee_vu(raw), "load_oracle_ee_vu succeeds for oracle state");

        bool any_vu0 = false;
        for (std::size_t i = 0; i < sizeof(ps2::g_vu0_code); ++i) {
            if (ps2::g_vu0_code[i] != 0) {
                any_vu0 = true;
                break;
            }
        }
        if (any_vu0) ++vu0_nonzero_states;

        const auto reach = ps2::vu::reachable(ps2::g_vu1_code, 16383u, 0u);
        check(!reach.empty(), "VU1 reachable > 0");
        const std::uint64_t key = ps2::vu::program_key(ps2::g_vu1_code, reach, 0u);
        check(key == 0x883AC7731B3857A8ull, "VU1 entry-0 matches AOT key 883AC7731B3857A8");

        // Execute static AOT VU1 program directly on the loaded state machine
        ps2::vu::reset();
        auto &r = ps2::vu::regs(1);
        r.vf[15].w[0] = f32_bits(1.5f);
        r.vf[15].w[1] = f32_bits(99.0f);
        r.vf[15].w[2] = f32_bits(-2.25f);
        r.vf[15].w[3] = f32_bits(4.0f);
        r.vf[19].w[1] = f32_bits(3.5f);
        r.vf[20].w[1] = f32_bits(7.25f);
        ps2::vu::run(1, 0u);
        check(ps2::vu::interpreted_calls() == 0u, "zero VU interpretation");
        check(ps2::vu::tpc(1) == 16u && r.i == 2183751697u, "VU1 AOT execution completion");
        check(r.vf[20].w[0] == f32_bits(5.0f) && r.vf[20].w[1] == f32_bits(7.25f) &&
                  r.vf[20].w[2] == f32_bits(1.25f) && r.vf[20].w[3] == f32_bits(7.5f),
              "VU1 vf[20] masked add parity");
        ++vu1_executed;

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

    check(vu0_nonzero_states == 0, "0/61 oracle states contain VU0 microcode");
    std::printf(
        "PASS: Phase 4 AOT equivalence verified: oracle_states=%zu vu0_nonzero=%d vu1_aot_executed=%d "
        "gamer_window_matched=%d unit_0152_matched=%d hash=%016llX ee_differential=OK overlay_switch=OK\n",
        states.size(), vu0_nonzero_states, vu1_executed, gamer_matched, unit0152_matched,
        static_cast<unsigned long long>(expected_0152_hash));
}

}  // namespace

int main(int argc, char **argv) {
    if (argc < 3) {
        std::fprintf(stderr, "Usage: %s <oracle_states_dir> <overlays_dir>\n", argc > 0 ? argv[0] : "test_phase4_aot_equivalence");
        return 2;
    }
    register_generated_code();
    register_generated_vu();

    test_ee_fpu_kernel_and_denormals();
    test_ee_vu0_mmi_matrix_kernel();
    test_ee_branch_likely_annulment();
    test_overlay_switching(fs::path(argv[2]));
    test_oracle_states(fs::path(argv[1]));
    return 0;
}
