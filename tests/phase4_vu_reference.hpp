#pragma once

// Test-only, snapshot-seeded VU reference. Never link this into a game target.
// The supported instruction set is the 109 pairs at 0000..0360 of the pinned
// 951776F70CC01D5D program. Decoding, scalar operations, lane commits, clip
// visibility and GIF packet construction do not call generator/runtime helpers.
//
// Numeric admission is deliberately narrower than a general VU arithmetic model:
// finite exact binary32 operations and exact ITOF0 are supported; an operation
// needing an arithmetic rounding decision fails with its PC/operands. FTOI4's
// specified truncation/signed saturation is explicit. Exponent-255, subnormal,
// arithmetic underflow/overflow and nonexact arithmetic remain named limits.
// An exact-real result alone is NOT a proof of every hardware multiplier bit;
// the existing general VU/COP1 numeric-authority dispute remains open. This
// header must not be advertised as a hardware oracle or complete VU emulator.
// Saved TPC=0368 is an end boundary; replaying entry0 is not an observed call.

#include "ps2/cpu.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace phase4_reference {
struct Packet { int path; std::vector<std::uint8_t> bytes; };
struct Result {
    ps2::Vu0Regs regs;
    std::array<std::uint8_t, 16384> memory;
    std::vector<Packet> packets;
};

namespace detail {
[[noreturn]] inline void refuse(const char* reason, std::uint32_t pc,
                               std::uint32_t a = 0, std::uint32_t b = 0) {
    char text[240];
    std::snprintf(text, sizeof text,
        "phase4_reference:%s pc=%04X operands=%08X,%08X", reason, pc, a, b);
    throw std::runtime_error(text);
}
inline std::uint32_t read32(const std::uint8_t* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
}
inline void write32(std::uint8_t* p, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = std::uint8_t(value >> (8 * i));
}
inline std::int32_t signed16(std::uint32_t v) {
    v &= 65535;
    return (v & 32768) ? std::int32_t(v) - 65536 : std::int32_t(v);
}
inline std::int64_t signed32(std::uint32_t v) {
    return (v & 0x80000000u) ? std::int64_t(v) - 0x100000000ll : v;
}
inline bool selected(unsigned mask, unsigned lane) { return (mask & (8u >> lane)) != 0; }

// A finite dyadic rational, magnitude * 2^power. No host float operation or
// generic floating-point instruction dispatcher is used by this test reference.
struct Number { bool negative; std::uint64_t magnitude; int power; };
inline Number number(std::uint32_t bits, std::uint32_t pc) {
    const unsigned e = (bits >> 23) & 255;
    const auto fraction = bits & 0x7fffffu;
    if (e == 255) refuse("unresolved_exponent255", pc, bits);
    if (!e) {
        if (fraction) refuse("unresolved_subnormal_operand", pc, bits);
        return {bool(bits >> 31), 0, 0};
    }
    return {bool(bits >> 31), std::uint64_t(0x800000u | fraction), int(e) - 150};
}
inline std::uint32_t encode_exact(Number n, std::uint32_t pc,
                                  std::uint32_t a, std::uint32_t b) {
    const auto sign = n.negative ? 0x80000000u : 0u;
    if (!n.magnitude) return sign;
    const auto bits = 64u - std::countl_zero(n.magnitude);
    const int exponent = n.power + int(bits) - 1;
    if (exponent < -126) refuse("unresolved_arithmetic_underflow", pc, a, b);
    if (exponent > 127) refuse("unresolved_arithmetic_overflow", pc, a, b);
    std::uint64_t mantissa = n.magnitude;
    if (bits > 24) {
        const auto shift = bits - 24;
        if (mantissa & ((std::uint64_t(1) << shift) - 1))
            refuse("unresolved_nonexact_arithmetic_rounding", pc, a, b);
        mantissa >>= shift;
    } else mantissa <<= 24 - bits;
    return sign | (std::uint32_t(exponent + 127) << 23) |
           (std::uint32_t(mantissa) & 0x7fffffu);
}
inline std::uint32_t add(std::uint32_t a, std::uint32_t b, std::uint32_t pc) {
    auto x = number(a, pc), y = number(b, pc);
    if (!x.magnitude && !y.magnitude) return (a & b) & 0x80000000u;
    if (!x.magnitude) return b;
    if (!y.magnitude) return a;
    const int power = x.power < y.power ? x.power : y.power;
    const unsigned sx = unsigned(x.power - power), sy = unsigned(y.power - power);
    // Two nonzero 24-bit inputs separated this far cannot have an exact
    // binary32 sum. Refuse without overflowing a host integer alignment.
    if (sx > 39 || sy > 39) refuse("unresolved_wide_add_rounding", pc, a, b);
    const std::uint64_t mx = x.magnitude << sx, my = y.magnitude << sy;
    Number sum{}; sum.power = power;
    if (x.negative == y.negative) {
        // At most one shift is nonzero; this bound also prevents addition wrap.
        if (~mx < my) refuse("unresolved_wide_add_capacity", pc, a, b);
        sum.negative = x.negative; sum.magnitude = mx + my;
    } else if (mx >= my) {
        sum.negative = mx == my ? false : x.negative; sum.magnitude = mx - my;
    } else { sum.negative = y.negative; sum.magnitude = my - mx; }
    return encode_exact(sum, pc, a, b);
}
inline std::uint32_t multiply(std::uint32_t a, std::uint32_t b, std::uint32_t pc) {
    const auto x = number(a, pc), y = number(b, pc);
    return encode_exact({x.negative != y.negative, x.magnitude * y.magnitude,
                         x.power + y.power}, pc, a, b);
}
inline int compare(std::uint32_t a, std::uint32_t b, std::uint32_t pc) {
    (void)number(a, pc); (void)number(b, pc);
    const auto am = a & 0x7fffffffu, bm = b & 0x7fffffffu;
    if (!am && !bm) return 0;
    const bool an = a >> 31, bn = b >> 31;
    if (an != bn) return an ? -1 : 1;
    if (am == bm) return 0;
    const int order = am < bm ? -1 : 1;
    return an ? -order : order;
}
inline std::uint32_t integer_to_float(std::uint32_t bits, std::uint32_t pc) {
    const auto value = signed32(bits);
    return encode_exact({value < 0, std::uint64_t(value < 0 ? -value : value), 0},
                        pc, bits, 0);
}
inline std::uint32_t float_to_fixed4(std::uint32_t bits, std::uint32_t pc) {
    auto n = number(bits, pc);
    if (!n.magnitude) return 0;
    n.power += 4;
    const auto width = 64u - std::countl_zero(n.magnitude);
    const int leading = int(width) - 1 + n.power;
    if (leading >= 31) return n.negative ? 0x80000000u : 0x7fffffffu;
    std::uint64_t value;
    if (n.power >= 0) value = n.magnitude << unsigned(n.power);
    else value = n.power <= -64 ? 0 : n.magnitude >> unsigned(-n.power);
    const auto low = std::uint32_t(value);
    return n.negative ? 0u - low : low;
}

struct Upper {
    int target = -1;
    unsigned mask = 0;
    std::array<std::uint32_t, 4> value{};
    bool arithmetic = false, writes_clip = false;
    std::uint32_t clip = 0;
};
inline Upper upper(std::uint32_t w, const ps2::Vu0Regs& r, std::uint32_t pc) {
    Upper out;
    out.mask = (w >> 21) & 15;
    const unsigned ft = (w >> 16) & 31, fs = (w >> 11) & 31;
    const unsigned fd = (w >> 6) & 31, fn = w & 63;
    const unsigned special = fd * 4 + (fn & 3);
    if (w & 0x3e000000u) refuse("unsupported_upper_control_bits", pc, w);
    if (fn >= 60 && special == 47) return out;
    if (fn >= 60 && special == 31) {
        out.writes_clip = true;
        const auto limit = r.vf[ft].w[3] & 0x7fffffffu;
        std::uint32_t flags = 0;
        for (unsigned lane = 0; lane < 3; ++lane) {
            if (compare(r.vf[fs].w[lane], limit, pc) > 0) flags |= 1u << (lane * 2);
            if (compare(r.vf[fs].w[lane], limit ^ 0x80000000u, pc) < 0)
                flags |= 2u << (lane * 2);
        }
        out.clip = ((r.clip << 6) | flags) & 0xffffffu;
        return out;
    }
    const bool itof = fn >= 60 && special == 16;
    const bool ftoi = fn >= 60 && special == 21;
    const bool mula = fn >= 60 && special == 24;
    if (!(itof || ftoi || mula || fn == 0 || fn == 1 || fn == 9 ||
          fn == 34 || fn == 40 || fn == 43 || fn == 44))
        refuse("unsupported_upper_opcode", pc, w);
    out.target = mula ? 32 : (itof || ftoi) ? int(ft) : int(fd);
    out.arithmetic = !(itof || ftoi || fn == 43);
    for (unsigned lane = 0; lane < 4; ++lane) {
        if (!selected(out.mask, lane)) continue;
        const auto a = r.vf[fs].w[lane];
        if (itof) out.value[lane] = integer_to_float(a, pc);
        else if (ftoi) out.value[lane] = float_to_fixed4(a, pc);
        else if (mula) out.value[lane] = multiply(a, r.vf[ft].w[0], pc);
        else if (fn == 9)
            out.value[lane] = add(r.acc.w[lane], multiply(a, r.vf[ft].w[1], pc), pc);
        else if (fn == 43) {
            const auto b = r.vf[ft].w[lane];
            out.value[lane] = compare(a, b, pc) >= 0 ? a : b;
        } else {
            const auto b = fn == 34 ? r.i : fn == 0 || fn == 1
                         ? r.vf[ft].w[fn] : r.vf[ft].w[lane];
            out.value[lane] = add(a, fn == 44 ? b ^ 0x80000000u : b, pc);
        }
    }
    return out;
}
inline void commit(ps2::Vu0Regs& r, const Upper& out) {
    if (out.target < 0) return;
    if (out.target) {
        auto& target = out.target == 32 ? r.acc : r.vf[out.target];
        for (unsigned lane = 0; lane < 4; ++lane)
            if (selected(out.mask, lane)) target.w[lane] = out.value[lane];
    }
    if (!out.arithmetic) return;
    std::uint32_t mac = 0;
    for (unsigned lane = 0; lane < 4; ++lane) {
        if (!selected(out.mask, lane)) continue;
        const auto value = out.value[lane], bit = 3 - lane;
        if (!(value & 0x7fffffffu)) mac |= 1u << bit;
        if (value >> 31) mac |= 1u << (bit + 4);
    }
    const std::uint32_t current = ((mac & 15) ? 1 : 0) | ((mac & 240) ? 2 : 0);
    r.mac = mac;
    // Current Z/S and their sticky bits; retain I/D and all previous stickies.
    // U/O cannot arise in the admitted exact finite arithmetic domain.
    r.status = (r.status & 0xff0u) | current | (current << 6);
}
struct ClipWrite { std::uint32_t value; std::uint64_t visible; };
inline std::uint32_t visible_clip(const std::vector<ClipWrite>& history,
                                  std::uint64_t cycle) {
    for (auto i = history.rbegin(); i != history.rend(); ++i)
        if (i->visible <= cycle) return i->value;
    throw std::runtime_error("phase4_reference:missing_initial_clip_history");
}
inline void packet(Result& out, std::uint32_t address, std::uint32_t pc) {
    Packet p{1, {}};
    std::uint32_t qword = address & 1023, consumed = 0;
    for (;;) {
        if (consumed == 1024) refuse("packet_without_EOP", pc, address);
        const auto* tag = out.memory.data() + qword * 16;
        const std::uint64_t low = read32(tag) | (std::uint64_t(read32(tag + 4)) << 32);
        const auto loops = std::uint32_t(low & 32767);
        const auto format = unsigned((low >> 58) & 3);
        unsigned nreg = unsigned(low >> 60); if (!nreg) nreg = 16;
        const std::uint32_t body = format == 0 ? loops * nreg :
                                   format == 1 ? (loops * nreg + 1) / 2 : loops;
        if (body > 1023 - consumed) refuse("packet_exceeds_source_memory", pc, address, body);
        for (std::uint32_t i = 0; i <= body; ++i) {
            const auto* bytes = out.memory.data() + qword * 16;
            p.bytes.insert(p.bytes.end(), bytes, bytes + 16);
            qword = (qword + 1) & 1023; ++consumed;
        }
        if (low & 32768) break;
    }
    out.packets.push_back(std::move(p));
}
} // namespace detail

inline Result execute(std::span<const std::uint8_t, 16384> code,
                      const ps2::Vu0Regs& before,
                      std::span<const std::uint8_t, 16384> memory,
                      std::uint32_t top, std::uint32_t itop, std::uint32_t entry = 0) {
    using namespace detail;
    if (entry != 0) refuse("unverified_entry", entry);
    if (before.vi[0] || before.vf[0].w[0] || before.vf[0].w[1] ||
        before.vf[0].w[2] || before.vf[0].w[3] != 0x3f800000u)
        refuse("noncanonical_constant_register_input", entry);
    (void)itop; // No XITOP appears in this verified instruction subset.
    Result out{before, {}, {}};
    for (std::size_t i = 0; i < memory.size(); ++i) out.memory[i] = memory[i];
    auto& r = out.regs;
    std::uint32_t pc = entry;
    bool pending_branch = false, end_delay = false;
    std::uint32_t branch_address = 0;
    std::array<std::uint16_t, 16> previous_vi{};
    bool vi_delay = false;
    std::vector<ClipWrite> clip_history{{r.clip, 0}};
    std::uint64_t cycle = 0;
    for (unsigned steps = 0; steps < 100000; ++steps) {
        if ((pc & 7) || pc > 0x360) refuse("unverified_instruction_span", pc);
        ++cycle;
        const auto lo = read32(code.data() + pc), hi = read32(code.data() + pc + 4);
        const auto upper_result = upper(hi, r, pc); // Read both upper sources before lower effects.
        std::array<std::uint16_t, 16> vi_before{};
        for (unsigned i = 0; i < 16; ++i) vi_before[i] = r.vi[i];
        const auto read_branch_vi = [&](unsigned i) { return vi_delay ? previous_vi[i] : r.vi[i]; };
        const auto set_integer = [&](unsigned i, std::uint32_t value) { if (i) r.vi[i] = std::uint16_t(value); };
        const auto data = [&](std::uint32_t q) { return out.memory.data() + (q & 1023) * 16; };
        const bool returning = end_delay;
        const bool take_pending = pending_branch;
        const auto pending_target = branch_address;
        pending_branch = false;
        bool current_branch = false, delay_exempt = false;
        if (hi & 0x80000000u) { r.i = lo; delay_exempt = true; }
        else {
            const unsigned major = lo >> 25, mask = (lo >> 21) & 15;
            const unsigned ft = (lo >> 16) & 31, fs = (lo >> 11) & 31;
            const unsigned it = ft & 15, is = fs & 15, id = (lo >> 6) & 15;
            if (major == 8 || major == 9) {
                const auto immediate = ((lo >> 10) & 0x7800u) | (lo & 0x7ffu);
                set_integer(it, major == 8 ? r.vi[is] + immediate : r.vi[is] - immediate);
            } else if (major == 4) {
                const int offset = int(lo & 0x7ff) - ((lo & 0x400) ? 2048 : 0);
                const auto* q = data(r.vi[is] + std::uint32_t(offset));
                for (unsigned lane = 0; lane < 4; ++lane) if (selected(mask, lane)) {
                    set_integer(it, read32(q + lane * 4) & 65535); break;
                }
                delay_exempt = true;
            } else if (major == 17) {
                r.clip = lo & 0xffffffu;
                clip_history.push_back({r.clip, cycle}); delay_exempt = true;
            } else if (major == 18) {
                set_integer(1, (visible_clip(clip_history, cycle) & lo & 0xffffffu) ? 1 : 0);
                delay_exempt = true;
            } else if (major == 45 || major == 46) {
                const auto value = signed16(read_branch_vi(is));
                current_branch = major == 45 ? value > 0 : value <= 0;
                if (current_branch) {
                    const int offset = int(lo & 0x7ff) - ((lo & 0x400) ? 2048 : 0);
                    branch_address = (pc + 8 + std::uint32_t(offset * 8)) & 16383;
                    pending_branch = true;
                }
            } else if (major == 64) {
                const unsigned fn = lo & 63, special = ((lo >> 6) & 31) * 4 + (fn & 3);
                if (fn < 60 && (fn == 48 || fn == 52))
                    set_integer(id, fn == 48 ? r.vi[is] + r.vi[it] : r.vi[is] & r.vi[it]);
                else if (fn >= 60 && special == 48) {
                    std::array<std::uint32_t, 4> value{};
                    for (unsigned lane = 0; lane < 4; ++lane) value[lane] = r.vf[fs].w[lane];
                    for (unsigned lane = 0; lane < 4; ++lane)
                        if (ft && selected(mask, lane)) r.vf[ft].w[lane] = value[lane];
                } else if (fn >= 60 && special == 52) {
                    const auto* q = data(r.vi[is]);
                    for (unsigned lane = 0; lane < 4; ++lane)
                        if (ft && selected(mask, lane)) r.vf[ft].w[lane] = read32(q + lane * 4);
                    set_integer(is, r.vi[is] + 1u);
                } else if (fn >= 60 && special == 53) {
                    auto* q = data(r.vi[it]);
                    for (unsigned lane = 0; lane < 4; ++lane)
                        if (selected(mask, lane)) write32(q + lane * 4, r.vf[fs].w[lane]);
                    set_integer(it, r.vi[it] + 1u);
                } else if (fn >= 60 && special == 61) {
                    const auto value = std::uint32_t(signed16(r.vi[is]));
                    for (unsigned lane = 0; lane < 4; ++lane)
                        if (ft && selected(mask, lane)) r.vf[ft].w[lane] = value;
                } else if (fn >= 60 && special == 104) set_integer(it, top);
                else if (fn >= 60 && special == 108) packet(out, r.vi[is], pc);
                else refuse("unsupported_lower_special", pc, lo);
            } else refuse("unsupported_lower_opcode", pc, lo);
        }
        if (take_pending && current_branch) refuse("unverified_branch_in_delay_slot", pc, lo);
        previous_vi = vi_before;
        vi_delay = !(hi & 0x80000000u) && !delay_exempt;
        commit(r, upper_result);
        if (upper_result.writes_clip) {
            r.clip = upper_result.clip;
            clip_history.push_back({r.clip, cycle + 4});
        }
        r.vf[0].w[0] = r.vf[0].w[1] = r.vf[0].w[2] = 0;
        r.vf[0].w[3] = 0x3f800000u; r.vi[0] = 0;
        const auto next = take_pending ? pending_target : ((pc + 8) & 16383);
        if (returning) { r.tpc = next; return out; }
        end_delay = (hi & 0x40000000u) != 0;
        pc = next;
    }
    refuse("instruction_budget_exhausted", pc);
}
} // namespace phase4_reference
