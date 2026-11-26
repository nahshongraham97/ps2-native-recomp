#pragma once

#include "mka/stage_a_guest_memory.hpp"
#include "ps2/cpu.hpp"
#include "ps2/fpu.hpp"
#include "ps2/vu.hpp"

#include <cstdint>
#include <cstdlib>

namespace mka::phase7 {

/// Native Stage A implementation of 0x0042EBA0 (unit_00CB:129u):
/// computes normalized triangle face normal (v1 - v0) x (v2 - v0) using VU0
/// into normal_out (16 bytes), zeros normal_out+4, and returns 1 if
/// normal.x != 0 || normal.z != 0 (0 if degenerate).
inline std::uint32_t tri_normal_0042eba0(
    mka::stage_a::GuestMemory &mem,
    ps2::Cpu &c,
    std::uint32_t normal_out_addr,
    std::uint32_t tri_addr) {
    const std::uint32_t sp = c.r[29].w[0] - 32u;
    auto ld128_mem = [&](std::uint32_t addr, auto &q) {
        const std::uint32_t a = addr & ~15u;
        q.w[0] = mem.load<std::uint32_t>(a);
        q.w[1] = mem.load<std::uint32_t>(a + 4u);
        q.w[2] = mem.load<std::uint32_t>(a + 8u);
        q.w[3] = mem.load<std::uint32_t>(a + 12u);
    };
    auto st128_mem = [&](std::uint32_t addr, const auto &q) {
        const std::uint32_t a = addr & ~15u;
        mem.store<std::uint32_t>(a, q.w[0]);
        mem.store<std::uint32_t>(a + 4u, q.w[1]);
        mem.store<std::uint32_t>(a + 8u, q.w[2]);
        mem.store<std::uint32_t>(a + 12u, q.w[3]);
    };

    ld128_mem(tri_addr + 16u, c.vu0.vf[16]);
    ld128_mem(tri_addr, c.vu0.vf[17]);
    ps2::vu::sub(c.vu0, 14u, 16u, 16u, 17u);
    st128_mem(sp + 16u, c.vu0.vf[16]);

    ld128_mem(tri_addr + 48u, c.vu0.vf[16]);
    ld128_mem(tri_addr, c.vu0.vf[17]);
    ps2::vu::sub(c.vu0, 14u, 16u, 16u, 17u);
    st128_mem(sp, c.vu0.vf[16]);

    ld128_mem(sp + 16u, c.vu0.vf[16]);
    ld128_mem(sp, c.vu0.vf[17]);
    ps2::vu::opmula(c.vu0, 16u, 17u);
    ps2::vu::opmsub(c.vu0, 16u, 17u, 16u);
    st128_mem(normal_out_addr, c.vu0.vf[16]);
    mem.store<std::uint32_t>(normal_out_addr + 4u, 0u);

    ld128_mem(normal_out_addr, c.vu0.vf[16]);
    ps2::vu::mul(c.vu0, 14u, 17u, 16u, 16u);
    ps2::vu::addbc(c.vu0, 8u, 17u, 17u, c.vu0.vf[17].w[1]);
    ps2::vu::addbc(c.vu0, 8u, 17u, 17u, c.vu0.vf[17].w[2]);
    ps2::vu::rsqrt(c.vu0, c.vu0.vf[0].w[3], c.vu0.vf[17].w[0]);
    ps2::vu::mulbc(c.vu0, 14u, 17u, 16u, c.vu0.q);
    st128_mem(normal_out_addr, c.vu0.vf[17]);

    const std::uint32_t nx = mem.load<std::uint32_t>(normal_out_addr);
    const std::uint32_t nz = mem.load<std::uint32_t>(normal_out_addr + 8u);
    return (ps2::fp::key(nx) != ps2::fp::key(0u) || ps2::fp::key(nz) != ps2::fp::key(0u)) ? 1u : 0u;
}

/// Native Stage A implementation of 0x00194370 (unit_0025:6u) over captured
/// active sphere/capsule-vs-triangle-plane rejection paths:
/// 1. Stores 0 to a2 + 12.
/// 2. Calls tri_normal_0042eba0(sp_frame + 80, a1).
/// 3. Computes signed plane distance and compares against radius; returns 0 on early reject.
inline std::uint32_t fighter_tri_plane_test_00194370(
    mka::stage_a::GuestMemory &mem,
    ps2::Cpu &c,
    std::uint32_t a0,
    std::uint32_t a1,
    std::uint32_t a2) {
    const std::uint32_t sp_frame = c.r[29].w[0] - 96u;
    mem.store<std::uint32_t>(a2 + 12u, 0u);
    c.r[29].w[0] = sp_frame;
    const std::uint32_t ok = tri_normal_0042eba0(mem, c, sp_frame + 80u, a1);
    c.r[29].w[0] = sp_frame + 96u;
    if (ok == 0u) {
        return 0u;
    }

    const std::uint32_t nz = mem.load<std::uint32_t>(sp_frame + 88u);
    const std::uint32_t nx = mem.load<std::uint32_t>(sp_frame + 80u);
    const std::uint32_t vz = mem.load<std::uint32_t>(a1 + 8u);
    const std::uint32_t vx = mem.load<std::uint32_t>(a1);
    const std::uint32_t pz = mem.load<std::uint32_t>(a0 + 24u);
    const std::uint32_t px = mem.load<std::uint32_t>(a0 + 16u);
    const std::uint32_t rad = mem.load<std::uint32_t>(a0 + 32u);

    std::uint32_t facc = ps2::fp::ee::mul(c.fcr31, nz, vz);
    const std::uint32_t d_tri = ps2::fp::ee::madd(c.fcr31, facc, nx, vx);
    facc = ps2::fp::ee::mul(c.fcr31, nz, pz);
    const std::uint32_t d_pos = ps2::fp::ee::madd(c.fcr31, facc, nx, px);

    if (ps2::fp::key(d_tri) < ps2::fp::key(ps2::fp::ee::add(c.fcr31, d_pos, rad))) {
        return 0u;
    }
    // Further intersection branches not exercised in the 30 captured states; fail closed.
    std::abort();
}

} // namespace mka::phase7
