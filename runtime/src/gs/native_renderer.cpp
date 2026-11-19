#include "native_renderer.hpp"

#include "native_gl_backend.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ps2::gs::native_renderer {

namespace {

[[noreturn]] void stop(const char *reason) {
    std::fprintf(stderr, "[native-renderer-stop] %s\n", reason);
    std::fflush(stderr);
    std::abort();
}

void kick(bool draw) {
    State &s = g_state;
    const std::uint32_t prim = static_cast<std::uint32_t>(s.prim) & 7u;
    const int needed = prim == 0 ? 1 : (prim == 1 || prim == 2 || prim == 6) ? 2 : prim == 7 ? 1 : 3;
    if (s.count < needed) return;

    if (draw) {
        const bool consumed = native_gl::submit(s, s.queue, needed);
        if (!consumed) {
            stop("native OpenGL rejected a primitive from the direct GIF packet path; no compatibility draw is permitted");
        }
        ps2::gs::note_native_primitive();
    }

    switch (prim) {
    case 2:
        s.queue[0] = s.queue[1];
        s.count = 1;
        break;
    case 4:
        s.queue[0] = s.queue[1];
        s.queue[1] = s.queue[2];
        s.count = 2;
        break;
    case 5:
        s.queue[1] = s.queue[2];
        s.count = 2;
        break;
    default:
        s.count = 0;
        break;
    }
}

void vertex(std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint8_t fog, bool draw) {
    State &s = g_state;
    if (s.count < 0 || s.count >= 3) stop("invalid native primitive assembly queue count");
    Vertex &v = s.queue[s.count++];
    v.x = x;
    v.y = y;
    v.z = z;
    v.f = fog;
    v.r = static_cast<std::uint8_t>(s.rgbaq);
    v.g = static_cast<std::uint8_t>(s.rgbaq >> 8);
    v.b = static_cast<std::uint8_t>(s.rgbaq >> 16);
    v.a = static_cast<std::uint8_t>(s.rgbaq >> 24);
    const std::uint32_t qbits = static_cast<std::uint32_t>(s.rgbaq >> 32);
    const std::uint32_t sbits = static_cast<std::uint32_t>(s.st);
    const std::uint32_t tbits = static_cast<std::uint32_t>(s.st >> 32);
    std::memcpy(&v.q, &qbits, sizeof(v.q));
    std::memcpy(&v.s, &sbits, sizeof(v.s));
    std::memcpy(&v.t, &tbits, sizeof(v.t));
    v.u = static_cast<std::uint32_t>(s.uv) & 0x3FFFu;
    v.v = static_cast<std::uint32_t>(s.uv >> 16) & 0x3FFFu;
    kick(draw);
}

void write_register(std::uint32_t reg, std::uint64_t value) {
    State &s = g_state;
    switch (reg) {
    case 0x00: s.prim = value; s.count = 0; break;
    case 0x01: s.rgbaq = value; break;
    case 0x02: s.st = value; break;
    case 0x03: s.uv = value; break;
    case 0x04:
        vertex(static_cast<std::uint32_t>(value) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 16) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 32) & 0xFFFFFFu,
               static_cast<std::uint8_t>(value >> 56), true);
        break;
    case 0x05:
        vertex(static_cast<std::uint32_t>(value) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 16) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 32), static_cast<std::uint8_t>(s.fog >> 56), true);
        break;
    // TEX0/TEX2 also load the current CLUT into the texture decoder's palette.
    // These register writes do not submit geometry to the compatibility renderer.
    case 0x06: case 0x07: case 0x16: case 0x17:
        ps2::gs::write(reg, value);
        break;
    case 0x08: case 0x09: s.ctx[reg - 0x08].clamp = value; break;
    case 0x0A: s.fog = value; break;
    case 0x0C:
        vertex(static_cast<std::uint32_t>(value) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 16) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 32) & 0xFFFFFFu,
               static_cast<std::uint8_t>(value >> 56), false);
        break;
    case 0x0D:
        vertex(static_cast<std::uint32_t>(value) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 16) & 0xFFFFu,
               static_cast<std::uint32_t>(value >> 32), static_cast<std::uint8_t>(s.fog >> 56), false);
        break;
    case 0x14: case 0x15: s.ctx[reg - 0x14].tex1 = value; break;
    case 0x18: case 0x19: s.ctx[reg - 0x18].xyoffset = value; break;
    case 0x1A: s.prmodecont = value; break;
    case 0x1B: s.prmode = value; break;
    case 0x1C: s.texclut = value; break;
    case 0x22: s.scanmsk = value; break;
    case 0x34: case 0x35: s.ctx[reg - 0x34].miptbp1 = value; break;
    case 0x36: case 0x37: s.ctx[reg - 0x36].miptbp2 = value; break;
    case 0x3B: s.texa = value; break;
    case 0x3D: s.fogcol = value; break;
    case 0x3F:
        // Invalidate the native GL texture cache after TEXFLUSH.
        ++g_vram_generation;
        break;
    case 0x40: case 0x41: s.ctx[reg - 0x40].scissor = value; break;
    case 0x42: case 0x43: s.ctx[reg - 0x42].alpha = value; break;
    case 0x44: s.dimx = value; break;
    case 0x45: s.dthe = value; break;
    case 0x46: s.colclamp = value; break;
    case 0x47: case 0x48: s.ctx[reg - 0x47].test = value; break;
    case 0x49: s.pabe = value; break;
    case 0x4A: case 0x4B: s.ctx[reg - 0x4A].fba = value; break;
    case 0x4C: case 0x4D: s.ctx[reg - 0x4C].frame = value; break;
    case 0x4E: case 0x4F: s.ctx[reg - 0x4E].zbuf = value; break;
    // Native uploads still use the existing VRAM transfer helper. Draw packets
    // themselves do not pass through gs::write()/kick()/raster::draw().
    case 0x50: case 0x51: case 0x52: case 0x53: case 0x54:
        ps2::gs::write(reg, value);
        break;
    case 0x60: case 0x61: case 0x62:
        gs::signal(reg, value);
        break;
    case 0x7F: // GS_REG_NOP
        break;
    default:
        std::fprintf(stderr,
                     "[native-renderer-stop] unhandled GS register 0x%02X value=0x%016llX in direct GIF packet path\n",
                     reg, static_cast<unsigned long long>(value));
        std::fflush(stderr);
        std::abort();
    }
}

void write_packed(std::uint32_t reg, const Reg128 &q) {
    State &s = g_state;
    switch (reg) {
    case 0x0: write_register(0x00, q.d[0] & 0x7FFu); break;
    case 0x1: {
        std::uint32_t qbits = 0;
        std::memcpy(&qbits, &s.q, sizeof(qbits));
        s.rgbaq = (q.w[0] & 0xFFu) | (static_cast<std::uint64_t>(q.w[1] & 0xFFu) << 8) |
                  (static_cast<std::uint64_t>(q.w[2] & 0xFFu) << 16) |
                  (static_cast<std::uint64_t>(q.w[3] & 0xFFu) << 24) |
                  (static_cast<std::uint64_t>(qbits) << 32);
        break;
    }
    case 0x2:
        s.st = q.d[0];
        std::memcpy(&s.q, &q.w[2], sizeof(s.q));
        break;
    case 0x3: s.uv = (q.w[0] & 0x3FFFu) | (static_cast<std::uint64_t>(q.w[1] & 0x3FFFu) << 16); break;
    case 0x4: {
        const std::uint64_t v = (q.w[0] & 0xFFFFu) | (static_cast<std::uint64_t>(q.w[1] & 0xFFFFu) << 16) |
                                (static_cast<std::uint64_t>((q.w[2] >> 4) & 0xFFFFFFu) << 32) |
                                (static_cast<std::uint64_t>((q.w[3] >> 4) & 0xFFu) << 56);
        write_register(((q.d[1] >> 47) & 1u) ? 0x0C : 0x04, v);
        break;
    }
    case 0x5: {
        const std::uint64_t v = (q.w[0] & 0xFFFFu) | (static_cast<std::uint64_t>(q.w[1] & 0xFFFFu) << 16) |
                                (static_cast<std::uint64_t>(q.w[2]) << 32);
        write_register(((q.d[1] >> 47) & 1u) ? 0x0D : 0x05, v);
        break;
    }
    case 0xA: s.fog = static_cast<std::uint64_t>((q.w[3] >> 4) & 0xFFu) << 56; break;
    case 0xC:
        write_register(0x0C, (q.w[0] & 0xFFFFu) | (static_cast<std::uint64_t>(q.w[1] & 0xFFFFu) << 16) |
                                (static_cast<std::uint64_t>((q.w[2] >> 4) & 0xFFFFFFu) << 32) |
                                (static_cast<std::uint64_t>((q.w[3] >> 4) & 0xFFu) << 56));
        break;
    case 0xD:
        write_register(0x0D, (q.w[0] & 0xFFFFu) | (static_cast<std::uint64_t>(q.w[1] & 0xFFFFu) << 16) |
                                (static_cast<std::uint64_t>(q.w[2]) << 32));
        break;
    case 0xE: write_register(static_cast<std::uint32_t>(q.d[1] & 0xFFu), q.d[0]); break;
    case 0xF: break;
    default: write_register(reg, q.d[0]); break;
    }
}

} // namespace

// Packet rendering is mandatory. Keep these query functions for callers, but
// environment flags cannot select the compatibility GIF sink or relax errors.
bool selected() { return true; }

bool strict() { return true; }

bool active() {
    if (!native_gl::enabled()) {
        stop("the mandatory native GIF packet renderer requires an available native OpenGL backend/context");
    }
    static bool announced = false;
    if (!announced) {
        std::fprintf(stderr, "[native-renderer] consuming GIF packet events before the compatibility GsSink\n");
        announced = true;
    }
    return true;
}

void reset() { g_state.count = 0; }

void tag(int path, std::uint64_t tag_value) {
    (void)path;
    g_state.q = 1.0f;
    const std::uint32_t flg = static_cast<std::uint32_t>(tag_value >> 58) & 3u;
    if (((tag_value >> 46) & 1u) && flg == 0)
        write_register(0x00, (tag_value >> 47) & 0x7FFu);
}

void packed(std::uint32_t register_id, const Reg128 &value) { write_packed(register_id, value); }

void reglist(std::uint32_t register_id, std::uint64_t value) { write_register(register_id, value); }

void image(const std::uint8_t *data, std::uint32_t qwords) { ps2::gs::write_image(data, qwords); }

} // namespace ps2::gs::native_renderer
