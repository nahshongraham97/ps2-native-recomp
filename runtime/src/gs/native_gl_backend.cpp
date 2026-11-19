// native_gl_backend.cpp — native OpenGL rasterizer consuming the game's retail
// primitive stream at the ps2::vertex::submit_primitive seam.
//
// Interactive Windows runs attach to SDL's visible WGL context, render into a
// fixed-size native scene FBO, and blit the GS-selected display rectangle to the
// visible back buffer with centered nearest scaling. A compatibility readback
// still updates GS VRAM for guest transfer consumers; it is no longer the display
// path. Tests without a host window retain a hidden WGL diagnostic context.
//
// Native packet rendering is mandatory; unsupported ownership/format paths fail.

#include "native_gl_backend.hpp"
#include "compliance_census.hpp"
#include "gpu/gpu.hpp"
#include "mka/gpu_diagnostics.hpp"

#include "ps2/os.hpp"
#include "ps2/system.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_FOG_COORDINATE_SOURCE  // GL 1.4 fog coordinate, for the GS per-vertex fog value
#define GL_FOG_COORDINATE_SOURCE 0x8450
#define GL_FOG_COORDINATE 0x8451
#endif
#ifndef GL_CONSTANT_ALPHA  // GL 1.4 blend factors, for the GS ALPHA FIX term
#define GL_CONSTANT_ALPHA 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#endif
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_RGB_SCALE 0x8573
#define GL_ALPHA_SCALE 0x0D1C
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_PRIMARY_COLOR 0x8577
#endif
#ifndef GL_ALPHA_SCALE
#define GL_ALPHA_SCALE 0x0D1C
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE 0x84E0
#endif
#ifndef GL_PREVIOUS
#define GL_PREVIOUS 0x8578
#endif
#ifndef GL_ONE_MINUS_DST_ALPHA
#define GL_ONE_MINUS_DST_ALPHA 0x0305
#endif
#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_READ_FRAMEBUFFER_BINDING 0x8CAA
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_RENDERBUFFER 0x8D41
#define GL_DEPTH_COMPONENT24 0x81A6
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#endif
#ifndef GL_DRAW_FRAMEBUFFER_BINDING
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#endif
#else
// No GL backend on non-Windows yet: submit() always returns false.
#endif

namespace ps2::gs::native_gl {

namespace {

struct ReadbackState {
    std::uint64_t frame = 0;
    std::uint64_t dimx = 0;
    std::uint64_t dthe = 0;
    std::uint64_t colclamp = 0;
};

bool same_readback_state(const ReadbackState &a, const ReadbackState &b) {
    return a.frame == b.frame && a.dimx == b.dimx && a.dthe == b.dthe && a.colclamp == b.colclamp;
}

// -- Lifetime ------------------------------------------------------------
bool g_checked = false;
bool g_enabled = false;

// -- Raster target -------------------------------------------------------
// Cover the common 832x448 GS render target (FRAME.FBW=13) plus PAL-height
// output while keeping the hidden diagnostic target bounded.
constexpr int kWidth = 1024;
constexpr int kHeight = 512;

std::vector<std::uint8_t> g_readback(kWidth * kHeight * 4, 0);

// -- Readback cadence ----------------------------------------------------
using clock_t_ = std::chrono::steady_clock;
clock_t_::time_point g_last_readback = clock_t_::now();
constexpr int kReadbackIntervalMs = 16;

// -- Diagnostics ---------------------------------------------------------
std::atomic<std::uint64_t> g_prims_drawn{0};
std::atomic<std::uint64_t> g_readbacks{0};
std::atomic<std::uint32_t> g_reported_rejections{0};
// Width of the most recently accepted FRAME surface. The direct scene FBO
// contains submitted geometry, while DISPLAY.DW can include unused scanout
// columns; use the active frame width when fitting that FBO for presentation.
std::atomic<int> g_active_frame_width{0};
std::atomic<std::uint32_t> g_active_frame_fbp{0};

#if defined(_WIN32)
std::uint64_t g_depth_key = 0;
bool g_depth_key_valid = false;
ReadbackState g_pending_readback_state{};
bool g_pending_readback_state_valid = false;
#endif

bool reject_packet(const State &st, std::uint64_t attr, std::uint32_t prim_type, int count, unsigned reason, const char *why) {
    ps2::census::gs_packet_rejected(why);
    const std::uint32_t bit = 1u << reason;
    if (!(g_reported_rejections.fetch_or(bit, std::memory_order_relaxed) & bit)) {
        const Context &context = st.ctx[(attr >> 9) & 1u];
        const std::uint32_t frame = static_cast<std::uint32_t>(context.frame);
        std::fprintf(stderr,
                     "[native-gl] rejected GS packet: %s | prim=%u attr=%016llX frame=%016llX fbw=%u fpsm=%02X tex0=%016llX tex1=%016llX clamp=%016llX test=%016llX alpha=%016llX zbuf=%016llX dthe=%llu dimx=%016llX colclamp=%llu count=%d\n",
                     why, prim_type, static_cast<unsigned long long>(attr), static_cast<unsigned long long>(context.frame),
                     (frame >> 16) & 0x3Fu, (frame >> 24) & 0x3Fu,
                     static_cast<unsigned long long>(context.tex0), static_cast<unsigned long long>(context.tex1),
                     static_cast<unsigned long long>(context.clamp), static_cast<unsigned long long>(context.test),
                     static_cast<unsigned long long>(context.alpha), static_cast<unsigned long long>(context.zbuf),
                     static_cast<unsigned long long>(st.dthe & 1u),
                     static_cast<unsigned long long>(st.dimx), static_cast<unsigned long long>(st.colclamp & 1u), count);
    }
    return false;
}

// The census map is keyed by pointer, so every ALPHA tuple needs its own
// stable string: the per-tuple distribution of rejected packets is what says
// which blend equations are still missing (the rejection log itself fires
// once per site and would only ever show the first tuple).
const char *alpha_tuple_key(std::uint32_t alpha) {
    static char kKeys[256][96];
    static const bool kReady = [] {
        for (int i = 0; i < 256; ++i)
            std::snprintf(kKeys[i], sizeof kKeys[i],
                          "GS ALPHA tuple a=%d b=%d c=%d d=%d is not expressible in the native OpenGL blend path",
                          i & 3, (i >> 2) & 3, (i >> 4) & 3, (i >> 6) & 3);
        return true;
    }();
    (void)kReady;
    return kKeys[alpha & 0xFFu];
}

#if defined(_WIN32)
HDC    g_hdc = nullptr;
HGLRC  g_hrc = nullptr;
HWND   g_hwnd = nullptr;
bool   g_owns_context = false;
bool   g_direct_window_context = false;
struct UploadTexture { GLuint object = 0; int width = 0, height = 0; };
UploadTexture g_display_upload, g_rect_upload;

struct TextureCache {
    GLuint object = 0;
    bool valid = false;
    bool stale = true;  // the guest wrote over the texture's VRAM
    std::uint64_t tex0 = 0, tex1 = 0, tex2 = 0, clamp = 0, texa = 0;
    std::uint64_t clut_hash = 0;
    std::uint32_t width = 0, height = 0;
    // The texture's place in VRAM: the unit (256 bytes) it starts at and how
    // many units it covers, so a guest write into it can be found and pushed
    // into this native texture.
    std::uint32_t base = 0, units = 0;
    std::uint32_t psm=0,tbw=0;
    std::uint64_t last_use = 0;
    std::uint64_t native_image_serial = 0;
};

// The native textures the game samples. Keeping them here means a texture is
// decoded and uploaded once, not after every write anywhere in VRAM, and a
// guest write lands in the native texture it belongs to instead of stopping
// in the emulated VRAM.
constexpr std::size_t kTexturePool = 512;
TextureCache g_pool[kTexturePool];
std::uint64_t g_texture_age = 0;

TextureCache *find_pool_entry(std::uint32_t base, std::uint32_t psm, std::uint32_t tbw, std::uint64_t clut_hash) {
    for (TextureCache &entry : g_pool) {
        if (!entry.valid || entry.base != base) continue;
        if (entry.psm != psm || entry.tbw != tbw) continue;
        if (entry.clut_hash != clut_hash) continue;
        return &entry;
    }
    return nullptr;
}

// Native IMAGE resources are keyed by decoded packet resource identity, not
// backed by a PS2-sized byte array. Pixels live in GL; CPU data is transient input.
struct ImageRect { std::uint32_t x, y, w, h; };
struct NativeImage {
    std::uint32_t base = 0, bw = 0, psm = 0, width = 0, height = 0;
    GLuint object = 0;
    std::uint64_t serial = 0;
    std::vector<ImageRect> coverage;
    bool framebuffer = false;
};
std::vector<NativeImage> g_native_images;
GLuint g_native_clut=0;
std::array<bool,256> g_native_clut_valid{};
std::uint64_t g_native_clut_serial=0;
bool g_native_clut_csm2=false;
unsigned g_native_clut_cpsm=0;
std::array<GLint,256> g_native_clut_half{}; // address metadata, never palette pixels
std::uint64_t g_image_serial = 0;
NativeImage* find_native_image(std::uint32_t base) {
    for (auto& image : g_native_images) if (image.base == base) return &image;
    return nullptr;
}

// Conservative serialized CT32 page footprint, used ONLY to reject unresolved
// resource aliases. CT32 pages are 64x32 pixels / 8192 bytes (GS address32).
// No PS2 memory array, address wrap or fixed-capacity allocation is involved.
std::uint64_t ct32_end(std::uint32_t base,std::uint32_t bw,std::uint32_t width,std::uint32_t height) {
    if(!width || !height)return base;
    return std::uint64_t(base)+((std::uint64_t(height-1)/32)*bw+(std::uint64_t(width)+63)/64)*32;
}
bool overlaps_native_image(std::uint32_t base,std::uint32_t bw,std::uint32_t width,std::uint32_t height,
                           const NativeImage* exact=nullptr) {
    const auto end=ct32_end(base,bw,width,height);
    for(const auto& image:g_native_images) {
        if(&image==exact)continue;
        const auto other=ct32_end(image.base,image.bw,image.width,image.height);
        if(std::uint64_t(base)<other && std::uint64_t(image.base)<end)return true;
    }
    return false;
}

// How many 256-byte units a texture of this size covers. The margin keeps a
// write that lands in the page padding of the texture inside its range.
std::uint32_t texture_units(std::uint32_t width, std::uint32_t height) {
    return (width * height * 4u + 8191u) / 256u + 64u;
}

using ActiveTextureProc = void (APIENTRY *)(GLenum texture);
using MultiTexCoord4fProc = void (APIENTRY *)(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
using BlendFuncSeparateProc = void (APIENTRY *)(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha);
using BlendEquationSeparateProc = void (APIENTRY *)(GLenum mode_rgb, GLenum mode_alpha);
using FogCoordfProc = void (APIENTRY *)(GLfloat coord);  // GL 1.4: not in Windows' GL 1.1 export table
using BlendColorProc = void (APIENTRY *)(GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);  // GL 1.4
ActiveTextureProc g_active_texture = nullptr;
MultiTexCoord4fProc g_multi_texcoord4f = nullptr;
BlendFuncSeparateProc g_blend_func_separate = nullptr;
BlendEquationSeparateProc g_blend_equation_separate = nullptr;
FogCoordfProc g_fog_coordf = nullptr;
BlendColorProc g_blend_color = nullptr;

// GL state cache. submit() runs once per GS packet - 9.1M packets in the
// 00:40 run - and every packet used to re-emit ~15 identical GL state calls
// (scissor, fog, depth, alpha test, blend, texture parameters/environment,
// FBO bind, colour mask). When the general ALPHA blend started drawing the
// previously rejected 77% of primitives, those redundant calls became the
// frame-time wall: the 11:14 run measured 16.8 fps (5,067 frames in 302.2 s).
// The cache re-emits a state group only when its value actually changes.
// Every GL write outside the cached setters - present, image transfers,
// readbacks, clears, flush - calls gl_state_invalidate() first, so the next
// submit re-establishes everything; a missed invalidation would cost a stale
// state, so every such site must keep calling it.
struct GlStateCache {
    bool valid = false;
    bool blend = false;
    GLenum blend_eq_rgb = 0x8006u;   // GL_FUNC_ADD
    GLenum blend_eq_alpha = 0x8006u; // GL_FUNC_ADD
    GLenum blend_s = 0xFFFFFFFFu, blend_d = 0xFFFFFFFFu;
    GLenum blend_as = 0xFFFFFFFFu, blend_ad = 0xFFFFFFFFu;
    GLfloat blend_const = -1.0f;
    bool alpha_test = false;
    GLenum alpha_func = 0xFFFFFFFFu;
    GLfloat alpha_ref = -1.0f;
    bool fog = false;
    GLfloat fog_r = -1.0f, fog_g = -1.0f, fog_b = -1.0f;
    bool depth = false;
    GLenum depth_func = 0xFFFFFFFFu;
    bool depth_mask = true;
    bool scissor = false;
    GLint scissor_l = -1, scissor_t = -1, scissor_w = -1, scissor_h = -1;
    bool mask_r = true, mask_g = true, mask_b = true, mask_a = true;
    GLenum unit = 0xFFFFFFFFu;
    bool tex2d0 = false, tex2d1 = false;
    GLuint texture = 0;
    std::uint64_t tex_env_key = 0;
    GLuint draw_fbo = 0;
};
GlStateCache g_glstate;

void gl_state_invalidate() { g_glstate = GlStateCache{}; }

void cache_active_texture(GLenum unit) {
    if (g_active_texture && g_glstate.unit != unit) {
        g_active_texture(unit);
        g_glstate.unit = unit;
    }
}

void cache_texture2d(bool on, GLenum unit) {
    cache_active_texture(unit);
    bool &flag = (unit == GL_TEXTURE1) ? g_glstate.tex2d1 : g_glstate.tex2d0;
    if (!g_glstate.valid || flag != on) {
        if (on) glEnable(GL_TEXTURE_2D);
        else glDisable(GL_TEXTURE_2D);
        flag = on;
    }
}

void cache_blend(bool on, GLenum s, GLenum d, GLenum as, GLenum ad, GLenum eq_rgb = GL_FUNC_ADD, GLenum eq_alpha = GL_FUNC_ADD) {
    GlStateCache &c = g_glstate;
    if (!c.valid || c.blend != on) {
        if (on) glEnable(GL_BLEND);
        else glDisable(GL_BLEND);
        c.blend = on;
    }
    if (on) {
        if (!c.valid || c.blend_eq_rgb != eq_rgb || c.blend_eq_alpha != eq_alpha) {
            if (g_blend_equation_separate) {
                g_blend_equation_separate(eq_rgb, eq_alpha);
            }
            c.blend_eq_rgb = eq_rgb;
            c.blend_eq_alpha = eq_alpha;
        }
        if (!c.valid || c.blend_s != s || c.blend_d != d || c.blend_as != as || c.blend_ad != ad) {
            g_blend_func_separate(s, d, as, ad);
            c.blend_s = s; c.blend_d = d; c.blend_as = as; c.blend_ad = ad;
        }
    }
}

void cache_blend_const(GLfloat v) {
    GlStateCache &c = g_glstate;
    if (g_blend_color && (!c.valid || c.blend_const != v)) {
        g_blend_color(v, v, v, v);
        c.blend_const = v;
    }
}

void cache_alpha_test(bool on, GLenum func, GLfloat ref) {
    GlStateCache &c = g_glstate;
    if (!c.valid || c.alpha_test != on) {
        if (on) glEnable(GL_ALPHA_TEST);
        else glDisable(GL_ALPHA_TEST);
        c.alpha_test = on;
    }
    if (on && (!c.valid || c.alpha_func != func || c.alpha_ref != ref)) {
        glAlphaFunc(func, ref);
        c.alpha_func = func; c.alpha_ref = ref;
    }
}

void cache_fog(bool on, GLfloat r, GLfloat g, GLfloat b) {
    GlStateCache &c = g_glstate;
    if (!c.valid || c.fog != on) {
        if (on) {
            const GLfloat col[4] = {r, g, b, 1.0f};
            glFogi(GL_FOG_MODE, GL_LINEAR);
            glFogf(GL_FOG_START, 0.0f);
            glFogf(GL_FOG_END, 1.0f);
            glFogi(GL_FOG_COORDINATE_SOURCE, GL_FOG_COORDINATE);
            glFogfv(GL_FOG_COLOR, col);
            c.fog_r = r; c.fog_g = g; c.fog_b = b;
            glEnable(GL_FOG);
        } else {
            glDisable(GL_FOG);
        }
        c.fog = on;
    } else if (r != c.fog_r || g != c.fog_g || b != c.fog_b) {
        const GLfloat col[4] = {r, g, b, 1.0f};
        glFogfv(GL_FOG_COLOR, col);
        c.fog_r = r; c.fog_g = g; c.fog_b = b;
    }
}

void cache_depth(bool on, GLenum func, bool mask) {
    GlStateCache &c = g_glstate;
    if (!c.valid || c.depth != on) {
        if (on) glEnable(GL_DEPTH_TEST);
        else glDisable(GL_DEPTH_TEST);
        c.depth = on;
    }
    if (!c.valid || c.depth_mask != mask) {
        glDepthMask(mask ? GL_TRUE : GL_FALSE);
        c.depth_mask = mask;
    }
    if (on && (!c.valid || c.depth_func != func)) {
        glDepthFunc(func);
        c.depth_func = func;
    }
}

void cache_scissor(GLint l, GLint t, GLint w, GLint h) {
    GlStateCache &c = g_glstate;
    if (!c.valid || !c.scissor) {
        glEnable(GL_SCISSOR_TEST);
        c.scissor = true;
    }
    if (!c.valid || c.scissor_l != l || c.scissor_t != t || c.scissor_w != w || c.scissor_h != h) {
        glScissor(l, t, w, h);
        c.scissor_l = l; c.scissor_t = t; c.scissor_w = w; c.scissor_h = h;
    }
}

void cache_color_mask(bool r, bool g, bool b, bool a) {
    GlStateCache &c = g_glstate;
    if (!c.valid || c.mask_r != r || c.mask_g != g || c.mask_b != b || c.mask_a != a) {
        glColorMask(r ? GL_TRUE : GL_FALSE, g ? GL_TRUE : GL_FALSE, b ? GL_TRUE : GL_FALSE, a ? GL_TRUE : GL_FALSE);
        c.mask_r = r; c.mask_g = g; c.mask_b = b; c.mask_a = a;
    }
}

bool g_multitexture_supported = false;

using GenFramebuffersProc = void (APIENTRY *)(GLsizei, GLuint *);
using BindFramebufferProc = void (APIENTRY *)(GLenum, GLuint);
using FramebufferTexture2DProc = void (APIENTRY *)(GLenum, GLenum, GLenum, GLuint, GLint);
using CheckFramebufferStatusProc = GLenum (APIENTRY *)(GLenum);
using DeleteFramebuffersProc = void (APIENTRY *)(GLsizei, const GLuint *);
using GenRenderbuffersProc = void (APIENTRY *)(GLsizei, GLuint *);
using BindRenderbufferProc = void (APIENTRY *)(GLenum, GLuint);
using RenderbufferStorageProc = void (APIENTRY *)(GLenum, GLenum, GLsizei, GLsizei);
using FramebufferRenderbufferProc = void (APIENTRY *)(GLenum, GLenum, GLenum, GLuint);
using DeleteRenderbuffersProc = void (APIENTRY *)(GLsizei, const GLuint *);
using BlitFramebufferProc = void (APIENTRY *)(GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum);

GenFramebuffersProc g_gen_framebuffers = nullptr;
BindFramebufferProc g_bind_framebuffer = nullptr;
FramebufferTexture2DProc g_framebuffer_texture_2d = nullptr;
CheckFramebufferStatusProc g_check_framebuffer_status = nullptr;
DeleteFramebuffersProc g_delete_framebuffers = nullptr;
GenRenderbuffersProc g_gen_renderbuffers = nullptr;
BindRenderbufferProc g_bind_renderbuffer = nullptr;
RenderbufferStorageProc g_renderbuffer_storage = nullptr;
FramebufferRenderbufferProc g_framebuffer_renderbuffer = nullptr;
DeleteRenderbuffersProc g_delete_renderbuffers = nullptr;
BlitFramebufferProc g_blit_framebuffer = nullptr;

struct SceneTarget {
    GLuint framebuffer = 0;
    GLuint color_texture = 0;
    GLuint depth_buffer = 0;
    std::uint32_t fbp = 0xFFFFFFFFu;
    std::uint32_t fbw = 0;
    std::uint64_t last_use = 0;
    std::uint64_t prim_count = 0;
    std::uint64_t write_count = 0;
    std::uint64_t last_write_tick = 0;
    bool ready = false;
    bool cpu_synced = true;
    ReadbackState last_draw_readback_state;
    std::optional<PresentationLayout> presentation_layout;
};
// Stable object addresses and host capacity; no fixed scene pool or CPU eviction.
std::deque<SceneTarget> g_scene_targets;
GLuint g_shared_depth_buffer = 0;
std::uint64_t g_scene_target_age = 0;
static std::atomic<std::uint64_t> g_scene_write_tick{0};

bool valid_gl_proc(PROC proc) {
    const auto address = reinterpret_cast<std::uintptr_t>(proc);
    return address > 3u && address != UINTPTR_MAX;
}

void load_multitexture_functions() {
    PROC active = wglGetProcAddress("glActiveTexture");
    if (!valid_gl_proc(active)) active = wglGetProcAddress("glActiveTextureARB");
    PROC coord = wglGetProcAddress("glMultiTexCoord4f");
    if (!valid_gl_proc(coord)) coord = wglGetProcAddress("glMultiTexCoord4fARB");
    if (valid_gl_proc(active) && valid_gl_proc(coord)) {
        g_active_texture = reinterpret_cast<ActiveTextureProc>(active);
        g_multi_texcoord4f = reinterpret_cast<MultiTexCoord4fProc>(coord);
        GLint units = 0;
        glGetIntegerv(GL_MAX_TEXTURE_UNITS, &units);
        g_multitexture_supported = units >= 2;
    }

    PROC blend = wglGetProcAddress("glBlendFuncSeparate");
    if (!valid_gl_proc(blend)) blend = wglGetProcAddress("glBlendFuncSeparateEXT");
    if (valid_gl_proc(blend)) g_blend_func_separate = reinterpret_cast<BlendFuncSeparateProc>(blend);

    PROC eq = wglGetProcAddress("glBlendEquationSeparate");
    if (!valid_gl_proc(eq)) eq = wglGetProcAddress("glBlendEquationSeparateEXT");
    if (valid_gl_proc(eq)) g_blend_equation_separate = reinterpret_cast<BlendEquationSeparateProc>(eq);

    // The GS fog blends per vertex by the fog value the game supplies, so the native path needs
    // GL's fog coordinate - not GL's depth-derived default. glFogCoordf is GL 1.4 and Windows
    // exports GL 1.1 only, so it comes through wglGetProcAddress like the other extensions.
    PROC fog_coord = wglGetProcAddress("glFogCoordf");
    if (!valid_gl_proc(fog_coord)) fog_coord = wglGetProcAddress("glFogCoordfEXT");
    if (valid_gl_proc(fog_coord)) g_fog_coordf = reinterpret_cast<FogCoordfProc>(fog_coord);

    // glBlendColor supplies the GS ALPHA register's FIX term to GL_CONSTANT_ALPHA
    // factors; GL 1.4, loaded the same way.
    PROC blend_color = wglGetProcAddress("glBlendColor");
    if (!valid_gl_proc(blend_color)) blend_color = wglGetProcAddress("glBlendColorEXT");
    if (valid_gl_proc(blend_color)) g_blend_color = reinterpret_cast<BlendColorProc>(blend_color);
}

template <typename T>
T load_gl_proc(const char *name, const char *extension_name = nullptr) {
    PROC proc = wglGetProcAddress(name);
    if (!valid_gl_proc(proc) && extension_name) proc = wglGetProcAddress(extension_name);
    return valid_gl_proc(proc) ? reinterpret_cast<T>(proc) : nullptr;
}

// The GPU's own busy time.
//
// The window title's "GPU %" used to read the WebGPU path's counter, which is
// always zero in the native GL path. That is what made the title say
// "GPU 0%" while the game was drawing on the Radeon - the number was a
// measurement artifact, not the truth. A GL_TIME_ELAPSED timer query measures
// completed frame-query durations. The public counter is cumulative measured
// time, not latest-frame duration or proof of full-device utilization.
constexpr GLenum kGlTimeElapsed = 0x88BF;        // GL 3.2 / ARB_timer_query
constexpr GLenum kGlQueryResult = 0x8866;
constexpr GLenum kGlQueryResultAvailable = 0x8867;
using BeginQueryProc = void (*)(GLenum, GLuint);
using EndQueryProc = void (*)(GLenum);
using GetQueryObjectui64vProc = void (*)(GLuint, GLenum, std::uint64_t *);
using GenQueriesProc = void (*)(GLsizei, GLuint *);
using DeleteQueriesProc = void (*)(GLsizei, const GLuint *);
BeginQueryProc g_begin_query = nullptr;
EndQueryProc g_end_query = nullptr;
GetQueryObjectui64vProc g_get_query_object = nullptr;
GenQueriesProc g_gen_queries = nullptr;
DeleteQueriesProc g_delete_queries = nullptr;
struct PendingTimer { GLuint object; std::uint64_t session, serial; };
std::vector<GLuint> g_timer_objects, g_timer_free;
std::deque<PendingTimer> g_timer_pending;
GLuint g_timer_active_object = 0;
std::uint64_t g_timer_serial = 0;
bool g_timer_ready = false;
mka::graphics::GpuQueryCounter g_gpu_time;

void load_timer_functions() {
    if (g_timer_ready) return;
    g_begin_query = load_gl_proc<BeginQueryProc>("glBeginQuery");
    g_end_query = load_gl_proc<EndQueryProc>("glEndQuery");
    g_get_query_object = load_gl_proc<GetQueryObjectui64vProc>("glGetQueryObjectui64v");
    g_gen_queries = load_gl_proc<GenQueriesProc>("glGenQueries");
    g_delete_queries = load_gl_proc<DeleteQueriesProc>("glDeleteQueries");
    if (!g_begin_query || !g_end_query || !g_get_query_object || !g_gen_queries || !g_delete_queries) {
        g_begin_query = nullptr;
        g_end_query = nullptr;
        g_get_query_object = nullptr;
        g_gen_queries = nullptr;
        g_delete_queries = nullptr;
        return;
    }
    g_timer_ready = true;
    std::fprintf(stderr, "[native-gl] GPU busy time measured with a GL timer query\n");
}

// Render/context-owner only. Never reuse an ID before its completed result
// has been consumed; a slow GPU grows the host query pool rather than losing
// the previous frame's timing or inventing an unavailable measurement.
void poll_frame_timer(bool wait = false) {
    if (!g_timer_ready) return;
    while (!g_timer_pending.empty()) {
        const PendingTimer query = g_timer_pending.front();
        if (query.object == g_timer_active_object) break; // Not ended yet.
        if (!wait) {
            std::uint64_t available = 0;
            g_get_query_object(query.object, kGlQueryResultAvailable, &available);
            if (!available) break;
        }
        std::uint64_t nanoseconds = 0;
        g_get_query_object(query.object, kGlQueryResult, &nanoseconds);
        g_gpu_time.completed(query.session, query.serial, nanoseconds);
        g_timer_pending.pop_front();
        g_timer_free.push_back(query.object);
    }
}

// Starts the timer for the frame's commands. Called with the first primitive
// of a frame; the query is closed when the frame is presented.
void begin_frame_timer() {
    if (!g_timer_ready || g_timer_active_object) return;
    poll_frame_timer();
    // Reserve host bookkeeping before acquiring/starting a GL object. End and
    // retirement then allocate nothing, including during shutdown cleanup.
    try {
        g_timer_pending.push_back({0, 0, 0});
        if (g_timer_free.empty()) {
            g_timer_objects.reserve(g_timer_objects.size() + 1);
            g_timer_free.reserve(g_timer_objects.size() + 1);
        }
    } catch (const std::bad_alloc&) {
        if (!g_timer_pending.empty() && g_timer_pending.back().object == 0) g_timer_pending.pop_back();
        std::fprintf(stderr, "[native-gl] GPU query timing unavailable: host allocation failed\n");
        return;
    } catch (const std::length_error&) {
        if (!g_timer_pending.empty() && g_timer_pending.back().object == 0) g_timer_pending.pop_back();
        std::fprintf(stderr, "[native-gl] GPU query timing unavailable: host container capacity exceeded\n");
        return;
    }
    GLuint object = 0;
    if (g_timer_free.empty()) {
        g_gen_queries(1, &object);
        if (!object) { g_timer_pending.pop_back(); return; }
        g_timer_objects.push_back(object);
    } else {
        object = g_timer_free.back();
        g_timer_free.pop_back();
    }
    g_timer_pending.back() = {object, g_gpu_time.session(), ++g_timer_serial};
    g_begin_query(kGlTimeElapsed, object);
    g_timer_active_object = object;
}

// Closes the timer and collects the previous frame's result. The result is
// read without blocking: a frame that is still running on the GPU is picked up
// on the next one.
void end_frame_timer() {
    if (!g_timer_ready || !g_timer_active_object) return;
    g_end_query(kGlTimeElapsed);
    g_timer_active_object = 0;
}

// Loads the FBO/blit entry points the scene targets and presentation path
// need. The GLSL shader loaders that used to live here were part of the
// removed fullscreen alpha compositor.
void load_framebuffer_functions() {
    g_gen_framebuffers = load_gl_proc<GenFramebuffersProc>("glGenFramebuffers", "glGenFramebuffersEXT");
    g_bind_framebuffer = load_gl_proc<BindFramebufferProc>("glBindFramebuffer", "glBindFramebufferEXT");
    g_framebuffer_texture_2d = load_gl_proc<FramebufferTexture2DProc>("glFramebufferTexture2D", "glFramebufferTexture2DEXT");
    g_check_framebuffer_status = load_gl_proc<CheckFramebufferStatusProc>("glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
    g_delete_framebuffers = load_gl_proc<DeleteFramebuffersProc>("glDeleteFramebuffers", "glDeleteFramebuffersEXT");
    g_gen_renderbuffers = load_gl_proc<GenRenderbuffersProc>("glGenRenderbuffers", "glGenRenderbuffersEXT");
    g_bind_renderbuffer = load_gl_proc<BindRenderbufferProc>("glBindRenderbuffer", "glBindRenderbufferEXT");
    g_renderbuffer_storage = load_gl_proc<RenderbufferStorageProc>("glRenderbufferStorage", "glRenderbufferStorageEXT");
    g_framebuffer_renderbuffer = load_gl_proc<FramebufferRenderbufferProc>("glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT");
    g_delete_renderbuffers = load_gl_proc<DeleteRenderbuffersProc>("glDeleteRenderbuffers", "glDeleteRenderbuffersEXT");
    g_blit_framebuffer = load_gl_proc<BlitFramebufferProc>("glBlitFramebuffer", "glBlitFramebufferEXT");
}


void release_scene_target() {
    for (std::size_t i = 0; i < g_scene_targets.size(); ++i) {
        if (g_scene_targets[i].framebuffer && g_delete_framebuffers)
            g_delete_framebuffers(1, &g_scene_targets[i].framebuffer);
        if (g_scene_targets[i].color_texture)
            glDeleteTextures(1, &g_scene_targets[i].color_texture);
        g_scene_targets[i] = SceneTarget{};
    }
    g_scene_targets.clear();
    if (g_shared_depth_buffer && g_delete_renderbuffers)
        g_delete_renderbuffers(1, &g_shared_depth_buffer);
    g_shared_depth_buffer = 0;
}

// No driver calls in this reset. It is also safe before any context exists.
void reset_context_entrypoints() {
    g_timer_objects.clear(); g_timer_free.clear(); g_timer_pending.clear();
    g_timer_active_object = 0; g_timer_ready = false; g_timer_serial = 0;
    g_gpu_time.reset_session(); // Process-lifetime completed time survives.
    g_begin_query = nullptr; g_end_query = nullptr; g_get_query_object = nullptr;
    g_gen_queries = nullptr; g_delete_queries = nullptr;
    g_active_texture = nullptr; g_multi_texcoord4f = nullptr;
    g_blend_func_separate = nullptr; g_blend_equation_separate = nullptr;
    g_fog_coordf = nullptr; g_blend_color = nullptr;
    g_multitexture_supported = false;
    g_gen_framebuffers = nullptr; g_bind_framebuffer = nullptr;
    g_framebuffer_texture_2d = nullptr; g_check_framebuffer_status = nullptr;
    g_delete_framebuffers = nullptr; g_gen_renderbuffers = nullptr;
    g_bind_renderbuffer = nullptr; g_renderbuffer_storage = nullptr;
    g_framebuffer_renderbuffer = nullptr; g_delete_renderbuffers = nullptr;
    g_blit_framebuffer = nullptr;
    g_pending_readback_state_valid = false; g_depth_key_valid = false;
    gl_state_invalidate();
}

bool has_context_objects() {
    if(g_native_clut)return true;
    if (!g_timer_objects.empty() || g_timer_active_object || g_shared_depth_buffer ||
        g_display_upload.object || g_rect_upload.object) return true;
    for (const SceneTarget &target : g_scene_targets)
        if (target.framebuffer || target.color_texture) return true;
    for (const TextureCache &entry : g_pool) if (entry.object) return true;
    for (const auto& image : g_native_images) if (image.object) return true;
    return false;
}

// Requires the backend context current. Releases even partially allocated
// objects whose ready flag was never set; borrowed SDL context itself survives.
void release_context_objects() {
    end_frame_timer();
    poll_frame_timer(true);
    if (g_delete_queries)
        for (GLuint object : g_timer_objects) g_delete_queries(1, &object);
    release_scene_target();
    for (auto& image : g_native_images) if (image.object) glDeleteTextures(1, &image.object);
    g_native_images.clear();
    if(g_native_clut)glDeleteTextures(1,&g_native_clut);
    g_native_clut=0;g_native_clut_valid.fill(false);g_native_clut_serial=0;g_native_clut_csm2=false;
    g_native_clut_cpsm=0;g_native_clut_half.fill(0);
    for (TextureCache &entry : g_pool) {
        if (entry.object) glDeleteTextures(1, &entry.object);
        entry = TextureCache{};
    }
    for (UploadTexture *upload : {&g_display_upload, &g_rect_upload}) {
        if (upload->object) glDeleteTextures(1, &upload->object);
        *upload = {};
    }
}

void clear_bound_scene_target(bool clear_depth = true) {
    gl_state_invalidate();
    // glClear respects scissor and write masks left by the previous draw.
    // A newly assigned surface must be cleared in its entirety.
    glPushAttrib(GL_SCISSOR_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_VIEWPORT_BIT);
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glViewport(0, 0, kWidth, kHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | (clear_depth ? GL_DEPTH_BUFFER_BIT : 0));
    glPopAttrib();
}

bool allocate_scene_target(SceneTarget &t) {
    gl_state_invalidate();
    if (t.ready) return true;
    load_framebuffer_functions();
    if (!g_gen_framebuffers || !g_bind_framebuffer || !g_framebuffer_texture_2d ||
        !g_check_framebuffer_status || !g_delete_framebuffers || !g_gen_renderbuffers ||
        !g_bind_renderbuffer || !g_renderbuffer_storage || !g_framebuffer_renderbuffer ||
        !g_delete_renderbuffers) {
        std::fprintf(stderr, "[native-gl] direct presentation requires OpenGL framebuffer objects\n");
        return false;
    }

    const bool new_depth = g_shared_depth_buffer == 0;
    const auto fail = [&] {
        if (t.framebuffer) g_delete_framebuffers(1, &t.framebuffer);
        if (t.color_texture) glDeleteTextures(1, &t.color_texture);
        if (new_depth && g_shared_depth_buffer) g_delete_renderbuffers(1, &g_shared_depth_buffer);
        if (new_depth) g_shared_depth_buffer = 0;
        t = SceneTarget{};
        return false;
    };
    glGenTextures(1, &t.color_texture);
    g_gen_framebuffers(1, &t.framebuffer);
    if (new_depth) g_gen_renderbuffers(1, &g_shared_depth_buffer);
    t.depth_buffer = g_shared_depth_buffer;
    if (!t.color_texture || !t.framebuffer || !t.depth_buffer) {
        std::fprintf(stderr, "[native-gl] direct scene target allocation failed\n");
        return fail();
    }

    glBindTexture(GL_TEXTURE_2D, t.color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    g_bind_framebuffer(GL_FRAMEBUFFER, t.framebuffer);
    g_framebuffer_texture_2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             t.color_texture, 0);
    g_bind_renderbuffer(GL_RENDERBUFFER, t.depth_buffer);
    if (new_depth) g_renderbuffer_storage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kWidth, kHeight);
    g_framebuffer_renderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                               t.depth_buffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum status = g_check_framebuffer_status(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_bind_renderbuffer(GL_RENDERBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[native-gl] direct scene framebuffer is incomplete (0x%04X)\n",
                     static_cast<unsigned>(status));
        return fail();
    }
    t.ready = true;
    clear_bound_scene_target(new_depth);
    return true;
}

SceneTarget *find_scene_target(std::uint32_t fbp) {
    for (std::size_t i = 0; i < g_scene_targets.size(); ++i) {
        if (g_scene_targets[i].ready && g_scene_targets[i].fbp == fbp) {
            g_scene_targets[i].last_use = ++g_scene_target_age;
            return &g_scene_targets[i];
        }
    }
    return nullptr;
}

SceneTarget *ensure_scene_target(std::uint32_t fbp = 0) {
    if (const auto* image=find_native_image(fbp * 32u); image &&
        (!image->framebuffer || !find_scene_target(fbp))) {
        std::fprintf(stderr,"[native-gl-image] unsupported pending/evicted IMAGE framebuffer promotion FBP=%u\n",fbp);
        return nullptr;
    }
    SceneTarget *existing = find_scene_target(fbp);
    if (existing) return existing;

    try {
        g_scene_targets.emplace_back();
    } catch(const std::bad_alloc&) {
        std::fprintf(stderr,"[native-gl] host scene ownership allocation failed\n");
        return nullptr;
    } catch(const std::length_error&) {
        std::fprintf(stderr,"[native-gl] host scene container capacity exceeded\n");
        return nullptr;
    }
    SceneTarget &target=g_scene_targets.back();
    if(!allocate_scene_target(target)) {g_scene_targets.pop_back();return nullptr;}
    target.fbp=fbp;
    target.last_use=++g_scene_target_age;
    std::fprintf(stderr,"[native-gl] owned GPU scene %zu for FBP=%u\n",g_scene_targets.size()-1,fbp);
    return &target;
}

bool bind_draw_target(std::uint32_t fbp) {
    if (!g_direct_window_context) {
        std::fprintf(stderr, "[native-gl] cannot bind FBP=%u without a direct scene context\n", fbp);
        gl_state_invalidate();
        return false;
    }
    SceneTarget *target = ensure_scene_target(fbp);
    if (!target || !target->ready) {
        // Allocation or preservation may have changed the actual GL binding.
        // Never let a cached previous framebuffer make a later draw look valid.
        gl_state_invalidate();
        std::fprintf(stderr, "[native-gl] cannot create/preserve native draw target FBP=%u\n", fbp);
        return false;
    }
    // Cached: bind_draw_target ran per packet with the same target, and
    // after present/readback invalidates the cache the bind is re-emitted.
    if (!g_glstate.valid || g_glstate.draw_fbo != target->framebuffer) {
        g_bind_framebuffer(GL_FRAMEBUFFER, target->framebuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, kWidth, kHeight);
        g_glstate.draw_fbo = target->framebuffer;
    }
    return true;
}

bool image_error(const char* operation) {
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR) return false;
    std::fprintf(stderr,"[native-gl-image] %s GL error=0x%X\n",operation,static_cast<unsigned>(error));
    gl_state_invalidate(); return true;
}
bool image_rect_covered(const NativeImage& image,std::uint32_t x,std::uint32_t y,std::uint32_t width,std::uint32_t height) {
    if (std::uint64_t(x)+width>image.width || std::uint64_t(y)+height>image.height) return false;
    for (std::uint32_t row=y;row<y+height;++row) {
        std::uint32_t end=x;
        while (end<x+width) {
            std::uint32_t next=end;
            for(const auto& r:image.coverage)
                if(row>=r.y && row-r.y<r.h && r.x<=end)next=std::max(next,r.x+r.w);
            if(next==end)return false;end=next;
        }
    }
    return true;
}
bool image_covered(const NativeImage& image,std::uint32_t width,std::uint32_t height) {
    return image_rect_covered(image,0,0,width,height);
}
// Exact GPU rectangle copy. Source/destination can have opposite Y orientation.
// Capture and restore READ and DRAW independently (they need not be equal).
bool blit_image_rect(GLuint source,GLuint destination,GLint sx0,GLint sy0,GLint sx1,GLint sy1,
                     GLint dx0,GLint dy0,GLint dx1,GLint dy1) {
    if(!g_gen_framebuffers || !g_bind_framebuffer || !g_framebuffer_texture_2d ||
       !g_check_framebuffer_status || !g_delete_framebuffers || !g_blit_framebuffer)return false;
    GLint read=0,draw=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    GLuint fbo[2]{};g_gen_framebuffers(2,fbo);
    if(!fbo[0] || !fbo[1]){g_delete_framebuffers(2,fbo);return false;}
    g_bind_framebuffer(GL_READ_FRAMEBUFFER,fbo[0]);
    g_framebuffer_texture_2d(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,source,0);
    g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,fbo[1]);
    g_framebuffer_texture_2d(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,destination,0);
    bool ok=g_check_framebuffer_status(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE &&
        g_check_framebuffer_status(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(ok){glReadBuffer(GL_COLOR_ATTACHMENT0);glDrawBuffer(GL_COLOR_ATTACHMENT0);
        const GLboolean scissor=glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        g_blit_framebuffer(sx0,sy0,sx1,sy1,dx0,dy0,dx1,dy1,GL_COLOR_BUFFER_BIT,GL_NEAREST);
        if(scissor)glEnable(GL_SCISSOR_TEST);
        ok=!image_error("copy native GPU image rectangle");}
    g_bind_framebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(read));
    g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(draw));
    g_delete_framebuffers(2,fbo);gl_state_invalidate();return ok && !image_error("restore native GPU copy bindings");
}
bool copy_image_texture(GLuint source,GLuint destination,std::uint32_t width,std::uint32_t height) {
    return blit_image_rect(source,destination,0,0,width,height,0,0,width,height);
}
bool copy_framebuffer_image(const SceneTarget& target,GLuint destination,std::uint32_t width,std::uint32_t height) {
    return blit_image_rect(target.color_texture,destination,0,kHeight,width,kHeight-height,0,0,width,height);
}
// Materialize CT32 sampling alpha on the GPU while retaining original IMAGE bytes.
bool convert_image_view(const NativeImage& image,GLuint destination,std::uint32_t width,std::uint32_t height) {
    if(!g_gen_framebuffers || !g_delete_framebuffers || !g_multitexture_supported)return false;
    GLint read=0,draw=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    GLuint fbo=0;g_gen_framebuffers(1,&fbo);if(!fbo)return false;
    g_bind_framebuffer(GL_FRAMEBUFFER,fbo);
    g_framebuffer_texture_2d(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,destination,0);
    bool ok=g_check_framebuffer_status(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(ok){
        glPushAttrib(GL_ALL_ATTRIB_BITS);glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0,0,static_cast<GLsizei>(width),static_cast<GLsizei>(height));
        glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FOG);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);glDisable(GL_COLOR_LOGIC_OP);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        if(g_active_texture){g_active_texture(GL_TEXTURE1);glDisable(GL_TEXTURE_2D);g_active_texture(GL_TEXTURE0);}
        glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,image.object);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_RGB_SCALE,1);
        glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);glTexEnvi(GL_TEXTURE_ENV,GL_ALPHA_SCALE,2);
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,0,height,-1,1);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        const GLfloat u=GLfloat(width)/image.width,v=GLfloat(height)/image.height;
        glBegin(GL_QUADS);
        glTexCoord2f(0,0);glVertex2i(0,0);glTexCoord2f(u,0);glVertex2i(width,0);
        glTexCoord2f(u,v);glVertex2i(width,height);glTexCoord2f(0,v);glVertex2i(0,height);glEnd();
        glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
        // Restore the framebuffer owners before color-buffer attributes: a
        // saved GL_BACK is invalid while the temporary color FBO is bound.
        g_bind_framebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(read));
        g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(draw));
        glPopAttrib();
        ok=!image_error("convert native CT32 IMAGE sampling alpha");
    }
    g_bind_framebuffer(GL_READ_FRAMEBUFFER,static_cast<GLuint>(read));g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,static_cast<GLuint>(draw));
    g_delete_framebuffers(1,&fbo);gl_state_invalidate();return ok && !image_error("restore native IMAGE conversion");
}
// Materialize GS virtual texel coordinates with edge replication on GPU.
// A virtual1024x1024 view can therefore depend on only its covered640x448
// clamped footprint. Replicated edge pixels are the defined sampler result,
// never zero-filled unowned source bytes.
bool region_clamp_view(const NativeImage& image,GLuint destination,unsigned width,unsigned height,std::uint64_t clamp) {
    const unsigned wms=clamp&3u,wmt=(clamp>>2)&3u;
    const unsigned x0=wms==2?(clamp>>4)&1023u:0,x1=wms==2?((clamp>>14)&1023u)+1:width;
    const unsigned y0=wmt==2?(clamp>>24)&1023u:0,y1=wmt==2?((clamp>>34)&1023u)+1:height;
    if(x0>=x1 || y0>=y1 || x1>width || y1>height || !image_rect_covered(image,x0,y0,x1-x0,y1-y0))return false;
    const auto* frame=image.framebuffer?find_scene_target(image.base/32u):nullptr;
    const GLuint source=image.framebuffer?(frame&&frame->ready?frame->color_texture:0):image.object;
    if(!source || !g_active_texture)return false;
    struct Segment{unsigned lo,hi;float source_lo,source_hi;};
    std::array<Segment,3> xs{},ys{};unsigned nx=0,ny=0;
    const auto segments=[](auto& out,unsigned& count,unsigned extent,unsigned low,unsigned high){
        if(low)out[count++]={0,low,float(low)+.5f,float(low)+.5f};
        out[count++]={low,high,float(low),float(high)};
        if(high<extent)out[count++]={high,extent,float(high)-.5f,float(high)-.5f};
    };
    segments(xs,nx,width,x0,x1);segments(ys,ny,height,y0,y1);
    GLint read=0,draw=0,old_active=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glGetIntegerv(GL_ACTIVE_TEXTURE,&old_active);glPushAttrib(GL_ALL_ATTRIB_BITS);
    g_active_texture(GL_TEXTURE1);glDisable(GL_TEXTURE_2D);g_active_texture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,source);
    GLint min_filter=0,mag_filter=0;glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,&min_filter);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,&mag_filter);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    GLuint fbo=0;g_gen_framebuffers(1,&fbo);g_bind_framebuffer(GL_FRAMEBUFFER,fbo);
    g_framebuffer_texture_2d(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,destination,0);
    const bool complete=fbo&&g_check_framebuffer_status(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(complete){
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glViewport(0,0,width,height);
        glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FOG);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);glDisable(GL_COLOR_LOGIC_OP);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_RGB_SCALE,1);
        glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);glTexEnvi(GL_TEXTURE_ENV,GL_ALPHA_SCALE,image.framebuffer?1:2);
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,0,height,-1,1);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glBegin(GL_QUADS);
        for(unsigned yi=0;yi<ny;++yi)for(unsigned xi=0;xi<nx;++xi){const auto& x=xs[xi];const auto& y=ys[yi];
            const float u0=x.source_lo/image.width,u1=x.source_hi/image.width;
            const float v0=image.framebuffer?1-y.source_lo/image.height:y.source_lo/image.height;
            const float v1=image.framebuffer?1-y.source_hi/image.height:y.source_hi/image.height;
            glTexCoord2f(u0,v0);glVertex2i(x.lo,y.lo);glTexCoord2f(u1,v0);glVertex2i(x.hi,y.lo);
            glTexCoord2f(u1,v1);glVertex2i(x.hi,y.hi);glTexCoord2f(u0,v1);glVertex2i(x.lo,y.hi);
        }
        glEnd();glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    }
    glBindTexture(GL_TEXTURE_2D,source);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,min_filter);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,mag_filter);
    const bool copied=!image_error("materialize GPU virtual region-clamp view");
    g_bind_framebuffer(GL_READ_FRAMEBUFFER,read);g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,draw);glPopAttrib();g_active_texture(old_active);
    if(fbo)g_delete_framebuffers(1,&fbo);gl_state_invalidate();return complete&&copied&&!image_error("restore virtual region-clamp view");
}

bool allocate_image_texture(GLuint& object, std::uint32_t width, std::uint32_t height) {
    glGenTextures(1,&object); if (!object) return false;
    glBindTexture(GL_TEXTURE_2D,object);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,static_cast<GLsizei>(width),static_cast<GLsizei>(height),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    if (image_error("allocate native IMAGE texture")) { glDeleteTextures(1,&object); object=0; return false; }
    return true;
}

std::uint32_t depth_max_for_psm(std::uint32_t psm) {
    switch (psm) {
    case gs::PSMZ24: return 0xFFFFFFu;
    case gs::PSMZ16: case gs::PSMZ16S: return 0xFFFFu;
    default: return 0xFFFFFFFFu;
    }
}

bool indexed_psm(std::uint32_t psm) {
    return psm==gs::PSMT8 || psm==gs::PSMT4 || psm==gs::PSMT8H || psm==gs::PSMT4HL || psm==gs::PSMT4HH;
}
std::uint32_t indexed_offset4(unsigned x,unsigned y) {
    static constexpr unsigned block[8][4]={{0,2,8,10},{1,3,9,11},{4,6,12,14},{5,7,13,15},{16,18,24,26},{17,19,25,27},{20,22,28,30},{21,23,29,31}};
    static constexpr unsigned word[2][2][8]={{{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}},{{8,9,12,13,0,1,4,5},{10,11,14,15,2,3,6,7}}};
    const unsigned wy=y&15,wx=x&31,column=wy/4,table=((wy&2)>>1)^(column&1);
    return (block[(y/16)&7][(x/32)&3]*256+column*64+word[table][wy&1][wx&7]*4)*2+((wx&24)+((wy&2)<<1))/4;
}
// Serialized GS page layout only. These coordinates identify bytes in a
// host-owned CT32 upload, never a CPU GS memory allocation or wrapped address.
std::uint32_t indexed_offset8(unsigned x,unsigned y) {
    static constexpr unsigned block[4][8]={{0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},{8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}};
    static constexpr unsigned word[2][2][8]={{{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}},{{8,9,12,13,0,1,4,5},{10,11,14,15,2,3,6,7}}};
    const unsigned wy=y&15,wx=x&15,table=((wy&2)>>1)^((y/4)&1);
    return block[(y/16)&3][(x/16)&7]*256+(wy/4)*64+word[table][wy&1][wx&7]*4+((wx&8)>>2)+((wy&2)>>1);
}
std::uint32_t ct32_offset(unsigned x,unsigned y) {
    static constexpr unsigned block[4][8]={{0,1,4,5,16,17,20,21},{2,3,6,7,18,19,22,23},{8,9,12,13,24,25,28,29},{10,11,14,15,26,27,30,31}};
    static constexpr unsigned word[2][8]={{0,1,4,5,8,9,12,13},{2,3,6,7,10,11,14,15}};
    return block[y/8][x/8]*256+((y&7)/2)*64+word[y&1][x&7]*4;
}
bool indexed_image_view(const NativeImage& image,GLuint destination,std::uint32_t psm,
                        std::uint32_t tbw,std::uint32_t width,std::uint32_t height,unsigned csa,
                        unsigned cpsm,std::uint64_t texa) {
    if(image.framebuffer || !g_native_clut || !g_active_texture)return false;
    const unsigned entries=(psm==gs::PSMT8 || psm==gs::PSMT8H)?256:16;
    const unsigned palette_offset=(csa&15u)*16;
    for(unsigned i=0;i<entries;++i){const unsigned slot=entries==256?std::min((i&~15u)+palette_offset,240u)+(i&15u):palette_offset+i;
        if(!g_native_clut_valid[slot])return false;}
    GLint units=0;glGetIntegerv(0x8872,&units);if(units<3)return false; // actual fragment sampler capacity
    // Inverse CT32 page permutation: no pixel data is read on the host.
    std::array<std::array<unsigned,2>,2048> inverse{};
    for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)inverse[ct32_offset(x,y)/4]={x,y};
    std::vector<float> mapping(static_cast<std::size_t>(width)*height*4);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        unsigned px=x,py=y,channel=3;float nibble=-1;
        if(psm==gs::PSMT8 || psm==gs::PSMT4){
            const bool four=psm==gs::PSMT4;
            const std::uint64_t page=std::uint64_t(y/(four?128:64))*std::max(1u,tbw/2)+x/128;
            const auto packed=four?indexed_offset4(x&127,y&127):indexed_offset8(x&127,y&63);
            const auto address=four?packed/2:packed;
            if(four)nibble=float(packed&1);
            const auto& xy=inverse[address/4];channel=address&3;
            const std::uint64_t ry=(page/image.bw)*32+xy[1];
            if(ry>UINT_MAX)return false;
            px=static_cast<unsigned>(page%image.bw)*64+xy[0];py=static_cast<unsigned>(ry);
        }else if(psm==gs::PSMT4HL)nibble=0;
        else if(psm==gs::PSMT4HH)nibble=1;
        if(!image_rect_covered(image,px,py,1,1))return false;
        const auto at=(static_cast<std::size_t>(y)*width+x)*4;
        mapping[at]=(float(px)+0.5f)/image.width;mapping[at+1]=(float(py)+0.5f)/image.height;
        mapping[at+2]=float(channel);mapping[at+3]=nibble;
    }
    // Compile a data-conversion shader, not a chip/device shader or CPU decode.
    using CreateShader=GLuint(APIENTRY*)(GLenum);using ShaderSource=void(APIENTRY*)(GLuint,GLsizei,const char*const*,const GLint*);
    using One=void(APIENTRY*)(GLuint);using Get=void(APIENTRY*)(GLuint,GLenum,GLint*);
    using CreateProgram=GLuint(APIENTRY*)();using Attach=void(APIENTRY*)(GLuint,GLuint);
    using Location=GLint(APIENTRY*)(GLuint,const char*);using Uniform=void(APIENTRY*)(GLint,GLint);
    using UniformArray=void(APIENTRY*)(GLint,GLsizei,const GLint*);
    const auto create=reinterpret_cast<CreateShader>(wglGetProcAddress("glCreateShader"));
    const auto source=reinterpret_cast<ShaderSource>(wglGetProcAddress("glShaderSource"));
    const auto compile=reinterpret_cast<One>(wglGetProcAddress("glCompileShader"));
    const auto get_shader=reinterpret_cast<Get>(wglGetProcAddress("glGetShaderiv"));
    const auto create_program=reinterpret_cast<CreateProgram>(wglGetProcAddress("glCreateProgram"));
    const auto attach=reinterpret_cast<Attach>(wglGetProcAddress("glAttachShader"));
    const auto link=reinterpret_cast<One>(wglGetProcAddress("glLinkProgram"));
    const auto get_program=reinterpret_cast<Get>(wglGetProcAddress("glGetProgramiv"));
    const auto use=reinterpret_cast<One>(wglGetProcAddress("glUseProgram"));
    const auto delete_shader=reinterpret_cast<One>(wglGetProcAddress("glDeleteShader"));
    const auto delete_program=reinterpret_cast<One>(wglGetProcAddress("glDeleteProgram"));
    const auto location=reinterpret_cast<Location>(wglGetProcAddress("glGetUniformLocation"));
    const auto uniform=reinterpret_cast<Uniform>(wglGetProcAddress("glUniform1i"));
    const auto uniform_array=reinterpret_cast<UniformArray>(wglGetProcAddress("glUniform1iv"));
    if(!create||!source||!compile||!get_shader||!create_program||!attach||!link||!get_program||!use||!delete_shader||!delete_program||!location||!uniform||!uniform_array)return false;
    GLuint shader=create(GL_FRAGMENT_SHADER),program=create_program(),map=0,fbo=0;
    struct Release{GLuint& shader;GLuint& program;GLuint& map;GLuint& fbo;One ds,dp;
        ~Release(){if(shader)ds(shader);if(program)dp(program);if(map)glDeleteTextures(1,&map);if(fbo)g_delete_framebuffers(1,&fbo);}}
        release{shader,program,map,fbo,delete_shader,delete_program};
    if(!shader||!program)return false;
    const char* text="#version 120\nuniform sampler2D bytesTexture;uniform sampler2D addressTexture;uniform sampler2D paletteTexture;uniform int paletteOffset;uniform int eightBit;"
        "uniform int palette16;uniform int paletteHalf[256];uniform int ta0;uniform int ta1;uniform int aem;"
        "void main(){vec4 m=texture2D(addressTexture,gl_TexCoord[0].xy);vec4 b=texture2D(bytesTexture,m.xy);"
        "float v=m.z<0.5?b.r:(m.z<1.5?b.g:(m.z<2.5?b.b:b.a));v=floor(v*255.0+0.5);"
        "if(m.w>0.5)v=floor(v/16.0);else if(m.w> -0.5)v=mod(v,16.0);"
        "v=eightBit!=0?min(floor(v/16.0)*16.0+float(paletteOffset),240.0)+mod(v,16.0):v+float(paletteOffset);"
        "vec4 c=texture2D(paletteTexture,vec2((v+0.5)/256.0,0.5));"
        "if(palette16!=0){vec2 p=paletteHalf[int(v)]==0?c.rg:c.ba;float word=floor(p.x*255.0+0.5)+floor(p.y*255.0+0.5)*256.0;"
        "float alpha=word>=32768.0?float(ta1):((aem!=0 && word==0.0)?0.0:float(ta0));"
        "c=vec4(mod(word,32.0)*8.0,mod(floor(word/32.0),32.0)*8.0,mod(floor(word/1024.0),32.0)*8.0,alpha)/255.0;}"
        "c.a=min(1.0,c.a*2.0);gl_FragColor=c;}";
    source(shader,1,&text,nullptr);compile(shader);GLint ok=0;get_shader(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){std::fprintf(stderr,"[native-gl-indexed] GLSL byte/palette shader compilation failed\n");return false;}
    attach(program,shader);link(program);get_program(program,GL_LINK_STATUS,&ok);
    if(!ok){std::fprintf(stderr,"[native-gl-indexed] GLSL byte/palette program link failed\n");return false;}
    GLint read=0,draw=0,old_program=0,active=0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glGetIntegerv(0x8B8D,&old_program);glGetIntegerv(GL_ACTIVE_TEXTURE,&active);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    g_active_texture(GL_TEXTURE1);glGenTextures(1,&map);glBindTexture(GL_TEXTURE_2D,map);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,0x8814,width,height,0,GL_RGBA,GL_FLOAT,mapping.data()); // RGBA32F addresses only
    g_active_texture(GL_TEXTURE0+2);glBindTexture(GL_TEXTURE_2D,g_native_clut);
    g_active_texture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,image.object);
    g_gen_framebuffers(1,&fbo);g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,fbo);
    g_framebuffer_texture_2d(GL_DRAW_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,destination,0);
    const bool complete=fbo && g_check_framebuffer_status(GL_DRAW_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
    if(complete){
        glDrawBuffer(GL_COLOR_ATTACHMENT0);glViewport(0,0,width,height);
        glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_SCISSOR_TEST);
        glDisable(GL_FOG);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_DITHER);glDisable(GL_COLOR_LOGIC_OP);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);use(program);
        uniform(location(program,"bytesTexture"),0);uniform(location(program,"addressTexture"),1);uniform(location(program,"paletteTexture"),2);
        uniform(location(program,"paletteOffset"),palette_offset);uniform(location(program,"eightBit"),entries==256);
        uniform(location(program,"palette16"),cpsm!=0);
        uniform_array(location(program,"paletteHalf[0]"),256,g_native_clut_half.data());
        uniform(location(program,"ta0"),texa&255u);uniform(location(program,"ta1"),(texa>>32)&255u);
        uniform(location(program,"aem"),(texa>>15)&1u);
        glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,width,0,height,-1,1);
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
        glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2i(0,0);glTexCoord2f(1,0);glVertex2i(width,0);
        glTexCoord2f(1,1);glVertex2i(width,height);glTexCoord2f(0,1);glVertex2i(0,height);glEnd();
        glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    }
    use(old_program);g_bind_framebuffer(GL_READ_FRAMEBUFFER,read);g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,draw);
    glPopAttrib();g_active_texture(active);gl_state_invalidate();
    return complete && !image_error("GPU indexed CT32 byte/palette conversion");
}

bool prepare_texture(const State &st, const Context &context, std::uint64_t attr,
                    std::uint32_t prim_type, int count, unsigned width_reason,
                    std::uint32_t &width, std::uint32_t &height) {
    const std::uint32_t tex1 = static_cast<std::uint32_t>(context.tex1);
    const std::uint32_t mmin = (tex1 >> 6) & 7u;
    const std::uint32_t mmag = (tex1 >> 5) & 1u;
    const std::uint32_t wms = static_cast<std::uint32_t>(context.clamp) & 3u;
    const std::uint32_t wmt = (static_cast<std::uint32_t>(context.clamp) >> 2) & 3u;
    const std::uint32_t tfx = static_cast<std::uint32_t>((context.tex0 >> 35) & 3u);
    // MMIN 2..7 select a mip level. The reference rasterizer has no mip
    // levels: it samples the base level with the MAG filter (raster.cpp:365,
    // "s.bilinear = (c.tex1 >> 5) & 1u") and ignores MMIN. Rejecting those
    // packets dropped 752,835 primitives in the 13:21 run - the character
    // select's own textures - so the native path samples the base level the
    // same way instead.
    if (wms == 3 || wmt == 3)
        return reject_packet(st, attr, prim_type, count, width_reason + 1, "GS region repeat is not implemented yet");
    const bool tcc = (context.tex0 & (1ull << 34)) != 0;
    if (tfx >= 2 && (!g_multitexture_supported || !g_active_texture || !g_multi_texcoord4f))
        return reject_packet(st, attr, prim_type, count, width_reason + 2, "GS highlight texture functions require two OpenGL texture units");

    cache_active_texture(GL_TEXTURE0);
    const std::uint64_t palette = g_native_clut_serial*0x9E3779B97F4A7C15ull;
    // The native texture for this base and palette, if the pool holds it and
    // the guest has not written over it since.
    const std::uint32_t tex_base = static_cast<std::uint32_t>(context.tex0 & 0x3fffu);
    const std::uint32_t tex_psm = static_cast<std::uint32_t>((context.tex0 >> 20) & 0x3Fu);
    const std::uint32_t tex_tbw = static_cast<std::uint32_t>((context.tex0 >> 14) & 0x3Fu);
    NativeImage* image=find_native_image(tex_base);
    if(!image && tex_base%32u==0 && tex_psm==gs::PSMCT32) {
        if(auto* frame=find_scene_target(tex_base/32u);frame && frame->ready && frame->fbw==tex_tbw) {
            try {
                NativeImage resource;
                resource.base=tex_base;resource.bw=tex_tbw;resource.psm=gs::PSMCT32;
                resource.width=kWidth;resource.height=kHeight;resource.framebuffer=true;
                resource.coverage.push_back({0,0,kWidth,kHeight});resource.serial=++g_image_serial;
                g_native_images.push_back(std::move(resource));image=&g_native_images.back();
            } catch(const std::bad_alloc&) {
                return reject_packet(st,attr,prim_type,count,width_reason+4,"native framebuffer view ownership allocation failed");
            } catch(const std::length_error&) {
                return reject_packet(st,attr,prim_type,count,width_reason+4,"native framebuffer view host capacity exceeded");
            }
        }
    }
    if(!image)
        return reject_packet(st,attr,prim_type,count,width_reason+4,"texture has no GPU-owned source; CPU VRAM fallback is removed");
    const bool indexed=indexed_psm(tex_psm);
    if((indexed || tex_psm==gs::PSMT4) && !image)
        return reject_packet(st,attr,prim_type,count,width_reason+4,"indexed texture has no exact GPU-owned raw IMAGE source");
    const unsigned cpsm=(context.tex0>>51)&15u;
    if(indexed && ((cpsm!=0 && cpsm!=gs::PSMCT16 && cpsm!=gs::PSMCT16S) || image->framebuffer))
        return reject_packet(st,attr,prim_type,count,width_reason+4,"native indexed view requires raw CT32 and a supported palette format");
    if(indexed && (cpsm!=g_native_clut_cpsm || (cpsm && (((context.tex0>>56)&31u) || ((context.tex0>>55)&1u)))))
        return reject_packet(st,attr,prim_type,count,width_reason+4,"native palette format reinterpretation or CPSM16 CSA/CSM2 is unsupported");
    if(indexed && tex_psm!=gs::PSMT8 && tex_psm!=gs::PSMT4 && tex_tbw!=image->bw)
        return reject_packet(st,attr,prim_type,count,width_reason+4,"native high-byte indexed stride reinterpretation is unsupported");
    if(indexed && tex_psm!=gs::PSMT8 && tex_psm!=gs::PSMT8H && bool((context.tex0>>55)&1u)!=g_native_clut_csm2)
        return reject_packet(st,attr,prim_type,count,width_reason+4,"four-bit palette CSM reinterpretation is unsupported");
    if(!image && overlaps_native_image(tex_base,tex_tbw,1u<<((context.tex0>>26)&15u),1u<<((context.tex0>>30)&15u)))
        return reject_packet(st,attr,prim_type,count,width_reason+4,"texture overlaps owned native IMAGE bytes through an unsupported resource alias");
    if (image && ((!indexed && (tex_psm!=image->psm || tex_tbw!=image->bw)) || !tex_tbw))
        return reject_packet(st,attr,prim_type,count,width_reason+4,"native IMAGE texture format/stride alias is unsupported");
    if (indexed && (wms==2 || wmt==2))
        return reject_packet(st,attr,prim_type,count,width_reason+5,"indexed region-clamp view is not implemented");
    TextureCache *entry = tex_base == 0xFFFFFFFFu ? nullptr : find_pool_entry(tex_base, tex_psm, tex_tbw, palette);
    if (!entry) {
        for (TextureCache &candidate : g_pool) {
            if (!candidate.valid) { entry = &candidate; break; }
        }
        if (!entry) {
            entry = &g_pool[0];
            for (TextureCache &candidate : g_pool) {
                if (candidate.last_use < entry->last_use) entry = &candidate;
            }
        }
    }
    if (!entry->object) glGenTextures(1, &entry->object);
    if (!entry->object)
        return reject_packet(st, attr, prim_type, count, width_reason + 3, "OpenGL could not allocate a texture object");

    const bool changed = !entry->valid || entry->stale || entry->tex0 != context.tex0 ||
                         entry->tex1 != context.tex1 || entry->tex2 != context.tex2 ||
                         entry->clamp != context.clamp || entry->texa != st.texa ||
                         entry->clut_hash != palette ||
                         entry->native_image_serial != (image ? image->serial : 0) || (image && image->framebuffer);
    const std::uint64_t env_key = context.tex0 ^ (context.tex1 * 0x9E3779B97F4A7C15ull) ^
                                  (context.clamp << 1) ^ (static_cast<std::uint64_t>(st.texa) << 32) ^ palette;

    if (!changed && g_glstate.valid && g_glstate.texture == entry->object &&
        g_glstate.tex_env_key == env_key) {
        width = entry->width;
        height = entry->height;
        entry->last_use = ++g_texture_age;
        cache_texture2d(true, GL_TEXTURE0);
        if (g_active_texture) cache_texture2d(tfx >= 2, GL_TEXTURE1);
        return true;
    }
    glBindTexture(GL_TEXTURE_2D, entry->object);
    g_glstate.texture = entry->object;
    if (changed) {
        width=1u<<((context.tex0>>26)&15u);height=1u<<((context.tex0>>30)&15u);
        const bool region=wms==2||wmt==2;
        if(!indexed && !region && !image_covered(*image,width,height))
            return reject_packet(st,attr,prim_type,count,width_reason+4,"native IMAGE texture view contains undefined pixels");
        if (image) {
            // Materialize exactly this TEX0 view from GPU storage. Raw IMAGE
            // alpha remains unchanged; its CT32 sampling conversion is GPU-only.
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,static_cast<GLsizei>(width),static_cast<GLsizei>(height),0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            if (image_error("allocate native IMAGE view"))
                return reject_packet(st,attr,prim_type,count,width_reason+4,"native IMAGE texture view allocation failed");
            SceneTarget* source=image->framebuffer?find_scene_target(image->base/32u):nullptr;
            const bool copied=indexed?indexed_image_view(*image,entry->object,tex_psm,tex_tbw,width,height,(context.tex0>>56)&31u,cpsm,st.texa):
                region?region_clamp_view(*image,entry->object,width,height,context.clamp):
                image->framebuffer ? (source && source->ready && copy_framebuffer_image(*source,entry->object,width,height)) :
                convert_image_view(*image,entry->object,width,height);
            if (!copied)
                return reject_packet(st,attr,prim_type,count,width_reason+4,"native IMAGE texture view GPU copy failed");
            glBindTexture(GL_TEXTURE_2D,entry->object);
        }
        entry->native_image_serial=image ? image->serial : 0;
        entry->tex0 = context.tex0;
        entry->tex1 = context.tex1;
        entry->tex2 = context.tex2;
        entry->clamp = context.clamp;
        entry->texa = st.texa;
        entry->clut_hash = palette;
        entry->width = width;
        entry->height = height;
        entry->valid = true;
        entry->stale = false;
        entry->base = tex_base;
        entry->units = texture_units(width, height);
        entry->psm = tex_psm;
        entry->tbw = tex_tbw;
        entry->last_use = ++g_texture_age;
    } else {
        width = entry->width;
        height = entry->height;
        entry->last_use = ++g_texture_age;
    }

    // A mipmapped mode has only the base level here, so the magnification
    // filter decides both - that is what the reference rasterizer does.
    const GLint mag_filter = mmag ? GL_LINEAR : GL_NEAREST;
    const GLint min_filter = mmin == 1 ? GL_LINEAR : (mmin > 1 ? mag_filter : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wms == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmt == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);

    // TEX1 MMIN 0/1 selects non-mipmapped nearest/linear filtering; MXL matters
    // only when a mipmapped MMIN mode selects another level. TFX 2/3 use two
    // fixed-function texture units to add the vertex-alpha highlight term.
    if (g_active_texture) {
        cache_texture2d(tfx >= 2, GL_TEXTURE1);
        cache_active_texture(GL_TEXTURE0);
    }
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    if (tfx == 0) {
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);
        if (context.tex0 & (1ull << 34)) {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_MODULATE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_TEXTURE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
            // Both modulate inputs are already scaled into 0..255 with 255 =
            // GS 0x80 (texture upload and glColor), so their product is
            // already the GS (Ta * A) >> 7 result on the 0..255 scale. The
            // RGB path needs GL_RGB_SCALE=2 because GS RGB modulate divides
            // by 128 rather than 255; alpha does not, and a second scale here
            // doubled every partially transparent modulated fragment - it made
            // ALPHA=0x44 text fully opaque where the GS blends at As/128.
            glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
        } else {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
        }
    } else if (tfx == 1) {
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 1);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, (context.tex0 & (1ull << 34)) ? GL_TEXTURE : GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
    } else {
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_TEXTURE);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_RGB_SCALE, 2);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, tcc ? GL_TEXTURE : GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);

        cache_active_texture(GL_TEXTURE1);
        cache_texture2d(true, GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, entry->object);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
        glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_ADD);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_RGB, GL_PREVIOUS);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_RGB, GL_SRC_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_RGB, GL_PRIMARY_COLOR);
        glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_RGB, GL_SRC_ALPHA);
        if (tfx == 2 && tcc) {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_ADD);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE1_ALPHA, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND1_ALPHA, GL_SRC_ALPHA);
        } else {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PREVIOUS);
            glTexEnvi(GL_TEXTURE_ENV, GL_OPERAND0_ALPHA, GL_SRC_ALPHA);
        }
        glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
        cache_active_texture(GL_TEXTURE0);
    }
    cache_texture2d(true, GL_TEXTURE0);
    g_glstate.tex_env_key = env_key;
    return true;
}

void initialize_context_state() {
    gl_state_invalidate();
    load_multitexture_functions();
    load_timer_functions();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
    glViewport(0, 0, kWidth, kHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    // PS2 screen coords: origin top-left, y down. GL default: origin bottom-left,
    // y up. Ortho with inverted Y maps (0,0) top-left directly.
    glOrtho(0.0, kWidth, kHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

// Diagnostic fallback for tests that initialize native GL without an SDL
// window. Interactive runs attach to SDL's visible OpenGL window instead.
bool create_hidden_gl_context() {
    HINSTANCE hi = GetModuleHandleW(nullptr);

    static const wchar_t kClass[] = L"ps2natgl_hidden";
    WNDCLASSW wc = {};
    wc.style         = CS_OWNDC;
    wc.lpfnWndProc   = DefWindowProcW;
    wc.hInstance     = hi;
    wc.lpszClassName = kClass;
    if (!RegisterClassW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        std::fprintf(stderr, "[native-gl] RegisterClassW failed (%lu)\n", GetLastError());
        return false;
    }

    // Own every partial window/DC/context from its first allocation onward.
    g_owns_context = true;
    const auto fail = [] { shutdown(); return false; };
    g_hwnd = CreateWindowExW(0, kClass, L"", WS_POPUP | WS_DISABLED,
                             0, 0, kWidth, kHeight,
                             nullptr, nullptr, hi, nullptr);
    if (!g_hwnd) {
        std::fprintf(stderr, "[native-gl] CreateWindowExW failed (%lu)\n", GetLastError());
        return fail();
    }

    g_hdc = GetDC(g_hwnd);
    if (!g_hdc) {
        std::fprintf(stderr, "[native-gl] GetDC failed\n");
        return fail();
    }

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize        = sizeof(pfd);
    pfd.nVersion     = 1;
    pfd.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType   = PFD_TYPE_RGBA;
    pfd.cColorBits   = 32;
    pfd.cAlphaBits   = 8;
    pfd.cDepthBits   = 24;
    pfd.iLayerType   = PFD_MAIN_PLANE;

    const int pf = ChoosePixelFormat(g_hdc, &pfd);
    if (!pf) {
        std::fprintf(stderr, "[native-gl] ChoosePixelFormat failed\n");
        return fail();
    }
    if (!SetPixelFormat(g_hdc, pf, &pfd)) {
        std::fprintf(stderr, "[native-gl] SetPixelFormat failed (%lu)\n", GetLastError());
        return fail();
    }

    g_hrc = wglCreateContext(g_hdc);
    if (!g_hrc) {
        std::fprintf(stderr, "[native-gl] wglCreateContext failed\n");
        return fail();
    }
    if (!wglMakeCurrent(g_hdc, g_hrc)) {
        std::fprintf(stderr, "[native-gl] wglMakeCurrent failed\n");
        return fail();
    }
    g_owns_context = true;
    g_direct_window_context = true;
    initialize_context_state();
    if (!ensure_scene_target()) {
        return fail();
    }

    return true;
}
#endif // _WIN32

static const bool s_enable_readback = [] {
    const std::string value = ps2::os::env("PS2_NATIVE_GL_READBACK");
    return !value.empty() && value != "0" && value != "off" && value != "false";
}();

bool do_readback_target(SceneTarget *target, const ReadbackState &state) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc) return false;
    const std::uint32_t fbp = target ? target->fbp : (static_cast<std::uint32_t>(state.frame) & 0x1FFu);
    std::uint32_t fbw = (static_cast<std::uint32_t>(state.frame) >> 16) & 0x3Fu;
    std::uint32_t fpsm = (static_cast<std::uint32_t>(state.frame) >> 24) & 0x3Fu;
    if (fbw == 0) fbw = static_cast<std::uint32_t>(kWidth / 64);
    if (fpsm != gs::PSMCT32 && fpsm != gs::PSMCT24 && fpsm != gs::PSMCT16 && fpsm != gs::PSMCT16S) {
        fpsm = gs::PSMCT32;
    }

    glFinish();
    const int render_width = std::min(kWidth, static_cast<int>(fbw * 64u));
    GLint prev_read_fbo = 0;
    if (g_direct_window_context) {
        if (!target) target = ensure_scene_target(fbp);
        if (!target || !target->framebuffer) return false;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read_fbo);
        g_bind_framebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
    } else {
        glReadBuffer(GL_BACK);
    }
    glReadPixels(0, 0, render_width, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, g_readback.data());

    // glReadPixels returns bottom-up; PS2 VRAM is top-down. Flip during the copy.
    for (int y = 0; y < kHeight; ++y) {
        const int src_y = kHeight - 1 - y;
        const std::uint8_t *row = g_readback.data() + static_cast<std::size_t>(src_y) * render_width * 4;
        for (int x = 0; x < render_width; ++x) {
            const std::uint8_t *p = row + x * 4;
            // FRAME.FBP is in 8192-byte pages; write_pixel takes 256-byte blocks.
            const std::uint32_t rgb24 = static_cast<std::uint32_t>(p[0])
                                      | (static_cast<std::uint32_t>(p[1]) << 8)
                                      | (static_cast<std::uint32_t>(p[2]) << 16);
            std::uint32_t stored = rgb24;
            if (fpsm == gs::PSMCT32) {
                stored |= static_cast<std::uint32_t>(p[3]) << 24;
            } else if (fpsm == gs::PSMCT16 || fpsm == gs::PSMCT16S) {
                std::uint32_t r = p[0], g = p[1], b = p[2];
                if (state.dthe & 1u) {
                    const unsigned matrix_index = static_cast<unsigned>((y & 3) * 4 + (x & 3));
                    const std::uint32_t encoded = static_cast<std::uint32_t>((state.dimx >> (matrix_index * 4u)) & 7u);
                    const int dither = (encoded & 4u) ? static_cast<int>(encoded) - 8 : static_cast<int>(encoded);
                    const auto apply_dither = [&](std::uint32_t channel) {
                        int value = static_cast<int>(channel) + dither;
                        if (state.colclamp & 1u) value = std::clamp(value, 0, 255);
                        else value = static_cast<int>(static_cast<unsigned int>(value) & 0xFFu);
                        return static_cast<std::uint32_t>(value);
                    };
                    r = apply_dither(r);
                    g = apply_dither(g);
                    b = apply_dither(b);
                }
                stored = ((r >> 3) & 0x1Fu)
                       | (((g >> 3) & 0x1Fu) << 5)
                       | (((b >> 3) & 0x1Fu) << 10)
                       | ((p[3] >= 128) ? 0x8000u : 0u);
            }
            gs::write_pixel(fpsm, fbp * 32u, fbw, static_cast<std::uint32_t>(x), static_cast<std::uint32_t>(y), stored);
        }
    }
    if (target) target->cpu_synced = true;
    if (g_direct_window_context) {
        g_bind_framebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prev_read_fbo));
    }
    g_readbacks.fetch_add(1, std::memory_order_relaxed);
    return true;
#else
    (void)target; (void)state;
    return false;
#endif
}

// Reads the current GL target, packs pixels for the selected GS color format,
// and writes them into VRAM at FRAME.FBP for remaining GS transfer consumers.
void do_readback(const ReadbackState &state) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc) return;
    if (!s_enable_readback) return;
    const std::uint32_t fbp = static_cast<std::uint32_t>(state.frame) & 0x1FFu;
    SceneTarget *target = find_scene_target(fbp);
    if (!target) target = ensure_scene_target(fbp);
    if (target) do_readback_target(target, state);
#else
    (void)state;
#endif
}

} // namespace

bool sync_surface_to_cpu(std::uint32_t fbp) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc) return false;
    SceneTarget *target = find_scene_target(fbp);
    if (!target || target->cpu_synced || !target->framebuffer || !target->ready) return false;
    ReadbackState state = target->last_draw_readback_state;
    if (state.frame == 0) {
        state.frame = static_cast<std::uint64_t>(fbp) | (10ull << 16) | (static_cast<std::uint64_t>(gs::PSMCT32) << 24);
    }
    return do_readback_target(target, state);
#else
    (void)fbp;
    return false;
#endif
}

void sync_all_surfaces_to_cpu() {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc) return;
    for (std::size_t i = 0; i < g_scene_targets.size(); ++i) {
        if (g_scene_targets[i].ready && !g_scene_targets[i].cpu_synced) {
            sync_surface_to_cpu(g_scene_targets[i].fbp);
        }
    }
#endif
}

// A guest write into the game's VRAM.
//
// The game's own textures live in VRAM and the native path samples them from
// native GL textures. When a write belongs to one of those textures, the
// written rectangle is pushed into it, so the data reaches a native resource
// instead of stopping in the emulated VRAM. A write that does not belong to
// a native texture marks the textures it overlaps as stale, so they are
// decoded again when they are sampled.
bool upload_image(std::uint32_t psm, std::uint32_t base, std::uint32_t bw,
                  std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h,
                  const std::uint8_t* packed, std::size_t bytes) {
    auto fail=[&](const char* why) {
        std::fprintf(stderr,"[native-gl-image] rejected psm=%u base=%u bw=%u rect=%u,%u %ux%u: %s\n",psm,base,bw,x,y,w,h,why);
        return false;
    };
#if defined(_WIN32)
    if (psm!=gs::PSMCT32) return fail("only exact CT32 IMAGE resources are implemented");
    if (!packed || !bw || !w || !h) return fail("empty IMAGE data or zero stride/rectangle");
    const std::uint64_t row=std::uint64_t(bw)*64u, right=std::uint64_t(x)+w, bottom=std::uint64_t(y)+h;
    const std::uint64_t pixels=std::uint64_t(w)*h;
    if (pixels>UINT64_MAX/4u) return fail("IMAGE byte-count arithmetic overflows");
    const std::uint64_t required=pixels*4u;
    if (right>row || row>std::uint64_t(INT_MAX) || bottom>std::uint64_t(INT_MAX) || required!=bytes)
        return fail("IMAGE extent/stride or exact packed byte count is invalid");
    if (!enabled() || !g_hdc || !g_hrc) return fail("native GL context is unavailable");
    if ((wglGetCurrentContext()!=g_hrc || wglGetCurrentDC()!=g_hdc) && !wglMakeCurrent(g_hdc,g_hrc))
        return fail("native GL context cannot be made current");
    SceneTarget* target=base%32u==0?find_scene_target(base/32u):nullptr;
    if (target && (target->fbw && target->fbw!=bw)) return fail("framebuffer IMAGE stride alias is unsupported");
    if (target && (right>kWidth || bottom>kHeight)) return fail("IMAGE rectangle exceeds the actual native framebuffer");
    if (target && target->prim_count && ((target->last_draw_readback_state.frame>>24)&0x3fu)!=gs::PSMCT32)
        return fail("framebuffer IMAGE pixel-format alias is unsupported");
    GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    if (maximum<=0 || row>static_cast<std::uint64_t>(maximum) || bottom>static_cast<std::uint64_t>(maximum))
        return fail("IMAGE exceeds actual GL_MAX_TEXTURE_SIZE");
    if (image_error("before native IMAGE upload")) return fail("prior GL error prevents verified upload");
    GLuint staged=0;
    try {
        NativeImage* image=find_native_image(base);
        if(overlaps_native_image(base,bw,static_cast<std::uint32_t>(row),
            std::max(image?image->height:0u,static_cast<std::uint32_t>(bottom)),image))
            return fail("IMAGE overlaps another owned native resource identity");
        if (image && (image->psm!=psm || image->bw!=bw)) return fail("resource identity format/stride alias is unsupported");
        if (image && image->framebuffer && !target) return fail("native framebuffer IMAGE storage is no longer available");
        NativeImage fresh;
        if (!image) {
            g_native_images.reserve(g_native_images.size()+1);
            fresh.base=base;fresh.bw=bw;fresh.psm=psm;image=&fresh;
        }
        image->coverage.reserve(image->coverage.size()+1);
        std::vector<std::uint8_t> rgba(packed,packed+bytes);
        gl_state_invalidate();if(g_active_texture)g_active_texture(GL_TEXTURE0);
        if (target) {
            for (std::size_t i=3;i<rgba.size();i+=4) rgba[i]=static_cast<std::uint8_t>(std::min(255u,unsigned(rgba[i])*2u));
            // Scene targets store top-left GS rows in bottom-up GL attachment
            // order. Reverse only transient host input rows, never read VRAM.
            std::vector<std::uint8_t> flipped(rgba.size());
            const std::size_t pitch=std::size_t(w)*4;
            for(std::uint32_t iy=0;iy<h;++iy)
                std::copy_n(rgba.data()+std::size_t(iy)*pitch,pitch,flipped.data()+std::size_t(h-1-iy)*pitch);
            glBindTexture(GL_TEXTURE_2D,target->color_texture);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
            glTexSubImage2D(GL_TEXTURE_2D,0,static_cast<GLint>(x),static_cast<GLint>(kHeight-y-h),static_cast<GLsizei>(w),static_cast<GLsizei>(h),GL_RGBA,GL_UNSIGNED_BYTE,flipped.data());
            if(image_error("upload native framebuffer IMAGE"))return fail("GPU framebuffer upload failed");
            target->fbw=bw;target->cpu_synced=false;++target->write_count;target->last_write_tick=++g_scene_write_tick;
            target->last_draw_readback_state.frame=std::uint64_t(target->fbp)|(std::uint64_t(bw)<<16);
            image->framebuffer=true;image->width=kWidth;image->height=kHeight;
            image->coverage.push_back({x,y,w,h});image->serial=++g_image_serial;
            if(image==&fresh)g_native_images.push_back(std::move(fresh));
            for(auto& entry:g_pool)if(entry.base==base)entry.stale=true;
            gl_state_invalidate();return true;
        }
        const auto height=std::max(image->height,static_cast<std::uint32_t>(bottom));
        const bool replace=!image->object || image->height!=height;
        if (replace) {
            if (!allocate_image_texture(staged,static_cast<std::uint32_t>(row),height)) return fail("GPU image allocation failed");
            if (image->object && !copy_image_texture(image->object,staged,image->width,image->height)) {
                glDeleteTextures(1,&staged);return fail("GPU image growth could not preserve previous pixels");
            }
        }
        const GLuint object=replace?staged:image->object;
        glBindTexture(GL_TEXTURE_2D,object);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
        glTexSubImage2D(GL_TEXTURE_2D,0,static_cast<GLint>(x),static_cast<GLint>(y),static_cast<GLsizei>(w),static_cast<GLsizei>(h),GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        if (image_error("upload native IMAGE pixels")) {
            if(staged)glDeleteTextures(1,&staged);return fail("GPU upload failed; no coverage published");
        }
        if(replace){if(image->object)glDeleteTextures(1,&image->object);image->object=staged;staged=0;}
        image->width=static_cast<std::uint32_t>(row);image->height=height;
        image->coverage.push_back({x,y,w,h});image->serial=++g_image_serial;
        if(image==&fresh)g_native_images.push_back(std::move(fresh));
        for(auto& entry:g_pool)if(entry.base==base)entry.stale=true;
        gl_state_invalidate();return true;
    } catch(const std::bad_alloc&) {
        if(staged)glDeleteTextures(1,&staged);return fail("host allocation failed while preparing native IMAGE");
    } catch(const std::length_error&) {
        if(staged)glDeleteTextures(1,&staged);return fail("host container cannot represent native IMAGE metadata");
    }
#else
    (void)packed;(void)bytes;return fail("native IMAGE upload requires the Windows GL backend");
#endif
}

bool load_clut(std::uint64_t tex0,std::uint64_t texclut) {
    auto fail=[](const char* why){std::fprintf(stderr,"[native-gl-clut] rejected: %s\n",why);return false;};
#if defined(_WIN32)
    const unsigned psm=(tex0>>20)&63u,cpsm=(tex0>>51)&15u,csm=(tex0>>55)&1u,csa=(tex0>>56)&31u;
    if(!indexed_psm(psm) || (cpsm!=0 && cpsm!=gs::PSMCT16 && cpsm!=gs::PSMCT16S))return fail("unsupported native indexed palette format");
    if(cpsm && (csm || csa))return fail("CPSM16 palette load currently requires CSM1 and CSA0");
    if(!enabled() || !g_hdc || !g_hrc ||
       ((wglGetCurrentContext()!=g_hrc || wglGetCurrentDC()!=g_hdc) && !wglMakeCurrent(g_hdc,g_hrc)))return fail("native context unavailable");
    if(image_error("before native CLUT load"))return fail("prior GL error");
    const auto* image=find_native_image((tex0>>37)&16383u);
    if(!image || image->framebuffer || image->psm!=gs::PSMCT32)return fail("palette requires an exact raw CT32 GPU resource");
    const unsigned entries=(psm==gs::PSMT8 || psm==gs::PSMT8H)?256:16;
    const unsigned offset=(csa&15u)*16;
    if(csm && entries==256 && offset)return fail("CSM2 nonzero-CSA eight-bit palette load is unsupported");
    const unsigned first=(!csm && entries==256)?offset:0;
    std::array<std::array<unsigned,2>,256> coordinates{};
    std::array<GLint,256> halves{};
    std::array<std::array<unsigned,2>,2048> inverse{};
    if(cpsm)for(unsigned y=0;y<32;++y)for(unsigned x=0;x<64;++x)inverse[ct32_offset(x,y)/4]={x,y};
    for(unsigned i=first;i<entries;++i){
        unsigned x=0,y=0;
        if(cpsm){
            // Primary GSClut WriteCLUT_T16_I8_CSM1 shuffle (606..628)
            // equals this 32-halfword permutation, repeated eight times.
            // I4 uses its first16 entries (631..638). Both 16/16S start
            // at the same physical BlockPtr(0,0), without CPU pixel access.
            static constexpr unsigned order[32]={0,2,8,10,16,18,24,26,4,6,12,14,20,22,28,30,1,3,9,11,17,19,25,27,5,7,13,15,21,23,29,31};
            const unsigned half=(i&~31u)+order[i&31u];
            const auto& xy=inverse[half/2];x=xy[0];y=xy[1];halves[i]=half&1;
        }
        else if(csm){if((texclut&63u)!=image->bw)return fail("CSM2 palette stride mismatch");x=((texclut>>6)&63u)*16+i;y=(texclut>>12)&1023u;}
        else if(entries==256){const unsigned j=(i&~24u)|((i&8u)<<1)|((i&16u)>>1);x=j&15;y=j>>4;}
        else {x=i&7;y=i>>3;}
        if(!csm && image->bw!=1)return fail("CSM1 palette requires exact one-page-row CT32 stride");
        if(!image_rect_covered(*image,x,y,1,1))return fail("palette source contains undefined pixels");
        coordinates[i]={x,y};
    }
    GLuint replacement=0;
    struct Release{GLuint& object;~Release(){if(object)glDeleteTextures(1,&object);}}release{replacement};
    if(!allocate_image_texture(replacement,256,1))return fail("palette GPU allocation failed");
    if(g_native_clut && !copy_image_texture(g_native_clut,replacement,256,1))return fail("prior palette preservation failed");
    for(unsigned i=first;i<entries;++i){const auto& xy=coordinates[i];const unsigned slot=entries==256?i:offset+i;
        if(!blit_image_rect(image->object,replacement,xy[0],xy[1],xy[0]+1,xy[1]+1,slot,0,slot+1,1))return fail("palette GPU load failed");}
    if(g_native_clut)glDeleteTextures(1,&g_native_clut);
    g_native_clut=replacement;replacement=0;
    if(cpsm!=g_native_clut_cpsm){g_native_clut_valid.fill(false);g_native_clut_half.fill(0);}
    for(unsigned i=first;i<entries;++i){const unsigned slot=entries==256?i:offset+i;
        g_native_clut_valid[slot]=true;g_native_clut_half[slot]=halves[i];}
    g_native_clut_cpsm=cpsm;
    g_native_clut_csm2=csm!=0;++g_native_clut_serial;gl_state_invalidate();return true;
#else
    (void)tex0;(void)texclut;return fail("native palette load requires Windows GL");
#endif
}

bool copy_image(std::uint32_t source_psm,std::uint32_t source_base,std::uint32_t source_bw,
                std::uint32_t sx,std::uint32_t sy,
                std::uint32_t destination_psm,std::uint32_t destination_base,std::uint32_t destination_bw,
                std::uint32_t dx,std::uint32_t dy,std::uint32_t w,std::uint32_t h,std::uint32_t direction) {
    auto fail=[&](const char* why){std::fprintf(stderr,"[native-gl-copy] rejected src=%u/%u/%u %u,%u dst=%u/%u/%u %u,%u %ux%u dir=%u: %s\n",
        source_psm,source_base,source_bw,sx,sy,destination_psm,destination_base,destination_bw,dx,dy,w,h,direction,why);return false;};
#if defined(_WIN32)
    if(source_psm!=gs::PSMCT32 || destination_psm!=gs::PSMCT32)return fail("only exact CT32 copies are implemented");
    if(direction>3 || !source_bw || !destination_bw || !w || !h)return fail("invalid copy direction/stride/rectangle");
    const std::uint64_t sr=std::uint64_t(sx)+w,sb=std::uint64_t(sy)+h,dr=std::uint64_t(dx)+w,db=std::uint64_t(dy)+h;
    const std::uint64_t source_row=std::uint64_t(source_bw)*64,destination_row=std::uint64_t(destination_bw)*64;
    if(sr>source_row || dr>destination_row || source_row>INT_MAX || destination_row>INT_MAX || sb>INT_MAX || db>INT_MAX)
        return fail("copy extent exceeds exact resource stride or host coordinate range");
    if(!enabled() || !g_hdc || !g_hrc ||
       ((wglGetCurrentContext()!=g_hrc || wglGetCurrentDC()!=g_hdc) && !wglMakeCurrent(g_hdc,g_hrc)))
        return fail("native GL context is unavailable");
    if(image_error("before native image copy"))return fail("prior GL error prevents a verified copy");
    const NativeImage* source=find_native_image(source_base);
    SceneTarget* sfbo=source_base%32u==0?find_scene_target(source_base/32u):nullptr;
    const bool source_frame=source?source->framebuffer:sfbo!=nullptr;
    if(source && (source->psm!=source_psm || source->bw!=source_bw))return fail("source format/stride alias is unsupported");
    if(source && !image_rect_covered(*source,sx,sy,w,h))return fail("source copy rectangle contains undefined pixels");
    if(!source && overlaps_native_image(source_base,source_bw,static_cast<std::uint32_t>(sr),static_cast<std::uint32_t>(sb)))
        return fail("source overlaps an owned resource through an unsupported interior alias");
    if(source_frame && (!sfbo || !sfbo->ready || (sfbo->fbw && sfbo->fbw!=source_bw) || sr>kWidth || sb>kHeight ||
       ((sfbo->prim_count || sfbo->write_count) && ((sfbo->last_draw_readback_state.frame>>24)&0x3fu)!=gs::PSMCT32)))
        return fail("source framebuffer identity/format/stride is unavailable");
    if(!source && (!sfbo || (!sfbo->prim_count && !sfbo->write_count)))return fail("no GPU-owned source pixels");
    const GLuint source_object=source_frame?sfbo->color_texture:source->object;
    if(!source_object)return fail("source GPU storage is unavailable");
    NativeImage* destination=find_native_image(destination_base);
    SceneTarget* dfbo=destination_base%32u==0?find_scene_target(destination_base/32u):nullptr;
    const bool destination_frame=destination?destination->framebuffer:dfbo!=nullptr;
    if(destination && (destination->psm!=destination_psm || destination->bw!=destination_bw))return fail("destination format/stride alias is unsupported");
    if(source_frame && !destination_frame)return fail("scaled framebuffer alpha cannot reconstruct raw IMAGE bytes");
    if(destination_frame && (!dfbo || !dfbo->ready || (dfbo->fbw && dfbo->fbw!=destination_bw) || dr>kWidth || db>kHeight ||
       ((dfbo->prim_count || dfbo->write_count) && ((dfbo->last_draw_readback_state.frame>>24)&0x3fu)!=gs::PSMCT32)))
        return fail("destination framebuffer identity/format/stride is unavailable");
    const auto height=std::max(destination?destination->height:0u,static_cast<std::uint32_t>(db));
    if(overlaps_native_image(destination_base,destination_bw,static_cast<std::uint32_t>(destination_row),height,destination))
        return fail("destination overlaps another owned resource identity");
    GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    if(maximum<=0 || destination_row>static_cast<std::uint64_t>(maximum) || height>static_cast<std::uint32_t>(maximum))
        return fail("copy destination exceeds actual GL texture limit");
    GLuint temporary[3]{};
    struct Release{GLuint* objects;~Release(){glDeleteTextures(3,objects);}}release{temporary};
    try {
        NativeImage fresh;
        if(!destination){g_native_images.reserve(g_native_images.size()+1);fresh.base=destination_base;
            fresh.bw=destination_bw;fresh.psm=destination_psm;destination=&fresh;}
        destination->coverage.reserve(destination->coverage.size()+1);
        if(!allocate_image_texture(temporary[0],w,h))return fail("GPU copy snapshot allocation failed");
        // Primary PCSX2 GSState::Move 3088..3107,3181..3216 advances BOTH
        // source and destination for DIRX/Y. It changes traversal, never mirrors.
        // Snapshot staging makes same-resource overlap independent of traversal.
        if(!blit_image_rect(source_object,temporary[0],sx,source_frame?kHeight-sy:sy,sx+w,
             source_frame?kHeight-sy-h:sy+h,0,0,w,h))return fail("GPU source snapshot failed");
        GLuint staged=temporary[0];
        if(!source_frame && destination_frame){
            if(!allocate_image_texture(temporary[1],w,h))return fail("GPU alpha-view allocation failed");
            NativeImage raw;raw.object=staged;raw.width=w;raw.height=h;
            if(!convert_image_view(raw,temporary[1],w,h))return fail("GPU raw-to-framebuffer alpha conversion failed");
            staged=temporary[1];
        }
        bool replace=false;
        GLuint object=destination_frame?dfbo->color_texture:destination->object;
        if(!destination_frame && (!object || destination->height!=height)){
            if(!allocate_image_texture(temporary[2],static_cast<std::uint32_t>(destination_row),height))return fail("GPU copy destination allocation failed");
            if(object && !copy_image_texture(object,temporary[2],destination->width,destination->height))return fail("GPU destination growth lost prior pixels");
            object=temporary[2];replace=true;
        }
        if(!blit_image_rect(staged,object,0,0,w,h,dx,destination_frame?kHeight-dy:dy,
             dx+w,destination_frame?kHeight-dy-h:dy+h))return fail("GPU destination copy failed");
        if(replace){if(destination->object)glDeleteTextures(1,&destination->object);destination->object=temporary[2];temporary[2]=0;}
        destination->framebuffer=destination_frame;destination->width=destination_frame?kWidth:static_cast<std::uint32_t>(destination_row);
        destination->height=destination_frame?kHeight:height;destination->coverage.push_back({dx,dy,w,h});destination->serial=++g_image_serial;
        if(destination_frame){dfbo->fbw=destination_bw;dfbo->cpu_synced=false;++dfbo->write_count;dfbo->last_write_tick=++g_scene_write_tick;
            dfbo->last_draw_readback_state.frame=std::uint64_t(dfbo->fbp)|(std::uint64_t(destination_bw)<<16);}
        if(destination==&fresh)g_native_images.push_back(std::move(fresh));
        for(auto& entry:g_pool)if(entry.base==destination_base)entry.stale=true;
        gl_state_invalidate();return true;
    }catch(const std::bad_alloc&){return fail("host allocation failed before copy publication");}
     catch(const std::length_error&){return fail("host metadata cannot represent native copy");}
#else
    return fail("native image copy requires Windows GL");
#endif
}

bool guest_wrote_vram(std::uint32_t psm, std::uint32_t base, std::uint32_t bw,
                      std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc || !w || !h) return false;
    if (wglGetCurrentContext() != g_hrc && !wglMakeCurrent(g_hdc, g_hrc)) return false;
    // A CPU reference/snapshot write is not a native GPU upload. It may
    // invalidate cached views, but cannot repopulate a native resource.
    (void)psm;(void)bw;(void)x;(void)y;
    for(auto& entry:g_pool)if(entry.valid && entry.base==base)entry.stale=true;
    return false;
#else
    (void)psm; (void)base; (void)bw; (void)x; (void)y; (void)w; (void)h;
    return false;
#endif
}

std::uint32_t active_frame_fbp() {
    return g_active_frame_fbp.load(std::memory_order_relaxed);
}

void clear_surfaces() {
#if defined(_WIN32)
    for (std::size_t i = 0; i < g_scene_targets.size(); ++i) {
        if (g_scene_targets[i].ready && g_scene_targets[i].framebuffer) {
            g_bind_framebuffer(GL_FRAMEBUFFER, g_scene_targets[i].framebuffer);
            clear_bound_scene_target();
            g_scene_targets[i].prim_count = 0;
            g_scene_targets[i].write_count = 0;
            g_scene_targets[i].last_write_tick = 0;
            g_scene_targets[i].cpu_synced = true;
        }
    }
    g_pending_readback_state_valid = false;
    g_depth_key_valid = false;
    std::fprintf(stderr, "[native-gl] scene boundary: cleared all native surfaces\n");
#endif
}

bool selected() {
    // Native GPU rendering is mandatory, independent of legacy mode switches.
    return true;
}

bool enabled() {
    if (!g_checked) {
        const bool want = selected();
        if (want) {
#if defined(_WIN32)
            g_enabled = create_hidden_gl_context();
#else
            g_enabled = false;
            std::fprintf(stderr, "[native-gl] PS2_NATIVE_GL=1 ignored on non-Windows build\n");
#endif
        }
        if (g_enabled) {
            std::fprintf(stderr, "[native-gl] enabled: GL %s | %s | %s\n",
                         reinterpret_cast<const char *>(glGetString(GL_VERSION)),
                         reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
                         reinterpret_cast<const char *>(glGetString(GL_VENDOR)));
        }
        g_checked = true;
    }
    return g_enabled;
}

bool attach_current_context(void *host_window) {
#if defined(_WIN32)
    if (!selected() || !host_window || g_checked) return false;
    const HWND hwnd = static_cast<HWND>(host_window);
    const HDC hdc = wglGetCurrentDC();
    const HGLRC hrc = wglGetCurrentContext();
    if (!hwnd || !hdc || !hrc || WindowFromDC(hdc) != hwnd) {
        std::fprintf(stderr, "[native-gl] SDL's current WGL context does not belong to the supplied host window\n");
        g_checked = true;
        return false;
    }

    g_hwnd = hwnd;
    g_hdc = hdc;
    g_hrc = hrc;
    g_owns_context = false;
    g_direct_window_context = true;
    initialize_context_state();
    if (!ensure_scene_target()) {
        // Cleanup before losing the valid current caller-owned context.
        // shutdown deletes only backend objects, never this SDL context.
        shutdown();
        g_checked = true;
        return false;
    }
    g_enabled = true;
    g_checked = true;
    // The full context identity, logged once: the window title's "GPU %" used
    // to read zero because it measured the wrong path, and there was no way to
    // tell from the log which driver was drawing.
    std::fprintf(stderr, "[native-gl] attached SDL-owned visible-window WGL context | %s | %s | %s\n",
                 reinterpret_cast<const char *>(glGetString(GL_VENDOR)),
                 reinterpret_cast<const char *>(glGetString(GL_RENDERER)),
                 reinterpret_cast<const char *>(glGetString(GL_VERSION)));
    return true;
#else
    (void)host_window;
    return false;
#endif
}

bool direct_window() {
    return g_enabled && g_direct_window_context;
}

double gpu_busy_ms() {
#if defined(_WIN32)
    // Called by host/title observers that may not own the GL context.
    return g_gpu_time.total_ms();
#else
    return 0.0;
#endif
}

// Draws a VRAM display rectangle into the visible back buffer as a textured
// quad. Nearest filtering keeps the integer-scaled pixels exact. Restores the
// fixed-function state the primitive path leaves behind (texture unit, texenv,
// matrices) so the next submitted primitive sees the same context it expects.

// Copies a rectangle the guest wrote straight into VRAM onto the native scene
// surface. A buffer clear and a movie frame both arrive as image transfers
// rather than as primitives, so without this the surface keeps whatever the
// primitives last drew: a stale ghost of an earlier frame, or nothing at all.
// Runs on the render thread, which is the thread that drains GIF packets and
// owns the WGL context.
bool draw_guest_rect(std::uint32_t psm, std::uint32_t base, std::uint32_t bw,
                     std::uint32_t x, std::uint32_t y,
                     std::uint32_t width, std::uint32_t height) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc || !g_hrc) return false;
    gl_state_invalidate();
    if (!width || !height || x >= static_cast<std::uint32_t>(kWidth) ||
        y >= static_cast<std::uint32_t>(kHeight))
        return false;
    const std::uint32_t clipped_w = std::min<std::uint32_t>(width, static_cast<std::uint32_t>(kWidth) - x);
    const std::uint32_t clipped_h = std::min<std::uint32_t>(height, static_cast<std::uint32_t>(kHeight) - y);
    if (!clipped_w || !clipped_h) return false;

    static std::vector<std::uint32_t> pixels;
    pixels.resize(static_cast<std::size_t>(clipped_w) * clipped_h);
    if (!gs::read_rect_rgba(psm, base, bw, x, y, clipped_w, clipped_h, false, pixels.data())) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "[native-gl] guest VRAM write pixel format %u cannot reach the native surface\n", psm);
        }
        return false;
    }

    if (wglGetCurrentContext() != g_hrc && !wglMakeCurrent(g_hdc, g_hrc)) {
        std::fprintf(stderr, "[native-gl] could not make the WGL context current for a guest VRAM write\n");
        return false;
    }

    GLuint &texture = g_rect_upload.object;
    int &texture_width = g_rect_upload.width, &texture_height = g_rect_upload.height;
    if (!texture) {
        glGenTextures(1, &texture);
        if (!texture) return false;
    }
    if (g_active_texture) g_active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (static_cast<int>(clipped_w) != texture_width || static_cast<int>(clipped_h) != texture_height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(clipped_w), static_cast<GLsizei>(clipped_h),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        texture_width = static_cast<int>(clipped_w);
        texture_height = static_cast<int>(clipped_h);
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, static_cast<GLsizei>(clipped_w), static_cast<GLsizei>(clipped_h),
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    }

    // The scene surface uses the same top-left GS coordinate space the
    // primitives are submitted in, so the transferred rectangle lands exactly
    // where the guest addressed it.
    const std::uint32_t target_fbp = base / 32u;
    SceneTarget *target = ensure_scene_target(target_fbp);
    if (!target || !target->ready) return false;
    ++target->prim_count;
    ps2::census::gs_primitive(target_fbp);
    ++target->write_count;
    target->last_write_tick = ++g_scene_write_tick;
    g_bind_framebuffer(GL_FRAMEBUFFER, target->framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, kWidth, kHeight);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);  // an image transfer has no GS alpha test; do not inherit the primitive path's
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, kWidth, kHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(static_cast<GLint>(x), static_cast<GLint>(y));
    glTexCoord2f(1.0f, 0.0f); glVertex2i(static_cast<GLint>(x + clipped_w), static_cast<GLint>(y));
    glTexCoord2f(1.0f, 1.0f); glVertex2i(static_cast<GLint>(x + clipped_w), static_cast<GLint>(y + clipped_h));
    glTexCoord2f(0.0f, 1.0f); glVertex2i(static_cast<GLint>(x), static_cast<GLint>(y + clipped_h));
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    return bind_draw_target(g_active_frame_fbp.load(std::memory_order_relaxed));
#else
    (void)psm; (void)base; (void)bw; (void)x; (void)y; (void)width; (void)height;
    return false;
#endif
}

// The rectangle the GS scans out, computed once for the window blit and for
// the host picture capture so the two can never disagree about what the frame
// shows.
struct DisplayRect {
    int dbx = 0, dby = 0;        // where the rectangle starts in the surface
    int width = 0, height = 0;   // visible size
    int display_width = 0;       // the width DISPLAY.DW describes
    int source_height = 0;       // rows read from the source (halved field)
    bool halved_field = false;
    bool valid = false;
};

DisplayRect display_rect(const DisplayRegs &regs, std::uint64_t fb, std::uint64_t disp,
                         const PresentationLayout* layout=nullptr) {
    DisplayRect rect;
    rect.dbx = static_cast<int>((fb >> 32) & 0x7FFu);
    rect.dby = static_cast<int>((fb >> 43) & 0x7FFu);
    const int magh = static_cast<int>((disp >> 23) & 0xFu) + 1;
    const int magv = static_cast<int>((disp >> 27) & 3u) + 1;
    const int display_width = (static_cast<int>((disp >> 32) & 0xFFFu) + 1) / magh;
    int height = (static_cast<int>((disp >> 44) & 0x7FFFu) + 1) / magv;
    if ((regs.smode2 & 3u) == 1u && height > 576) height /= 2;
    // Scanout dimensions belong to DISPLAY, independent of the latest
    // primitive's target/scissor. DISPFB DBX/DBY are never reset by activity.
    const int width = display_width;
    rect.width = width;
    rect.height = height;
    rect.display_width = display_width;
    rect.halved_field = (regs.smode2 & 3u) == 3u;
    rect.source_height = rect.halved_field ? (height + 1) / 2 : height;
    if(layout){
        if(rect.halved_field || layout->scan_width!=static_cast<unsigned>(display_width) ||
           layout->scan_height!=static_cast<unsigned>(height))return rect;
        const int left=std::max(rect.dbx,static_cast<int>(layout->x));
        const int top=std::max(rect.dby,static_cast<int>(layout->y));
        const int right=std::min(rect.dbx+rect.width,static_cast<int>(layout->x+layout->width));
        const int bottom=std::min(rect.dby+rect.source_height,static_cast<int>(layout->y+layout->height));
        rect.dbx=left;rect.dby=top;rect.width=right-left;rect.height=rect.source_height=bottom-top;
    }
    rect.valid = width > 0 && height > 0 && rect.dbx >= 0 && rect.dby >= 0 &&
                 rect.dbx + width <= kWidth && rect.dby + rect.source_height <= kHeight;
    return rect;
}

// DISPLAY identifies the owned resource. No latest-writer, movie-base or
// implicit allocation can substitute a different surface.
SceneTarget* select_display_target(std::uint32_t fbp,std::uint32_t bw,std::uint32_t psm) {
    auto* target=find_scene_target(fbp);
    if(!target || !target->ready || !bw || target->fbw!=bw ||
       (psm!=gs::PSMCT32 && psm!=gs::PSMCT24 && psm!=gs::PSMCT16 && psm!=gs::PSMCT16S) ||
       ((target->last_draw_readback_state.frame>>24)&63u)!=psm) {
        std::fprintf(stderr,"[native-gl-display] exact DISPLAY framebuffer %u/%u/%u is unavailable or aliases another format/stride\n",fbp,bw,psm);
        return nullptr;
    }
    return target;
}

#if defined(_WIN32)
bool present_native_image(const NativeImage& image,const DisplayRegs& regs,int circuit,
                          std::uint64_t fb,std::uint64_t disp,int output_width,int output_height) {
    const auto bw=static_cast<std::uint32_t>((fb>>9)&0x3fu),psm=static_cast<std::uint32_t>((fb>>15)&0x1fu);
    if(image.framebuffer || psm!=gs::PSMCT32 || image.bw!=bw || (regs.pmode&3u)==3u) {
        std::fprintf(stderr,"[native-gl-image] unsupported native IMAGE display format/stride/dual-circuit alias\n");return false;
    }
    const auto rect=display_rect(regs,fb,disp);
    if(!rect.valid || !image_rect_covered(image,rect.dbx,rect.dby,rect.width,rect.source_height)) {
        std::fprintf(stderr,"[native-gl-image] selected DISPLAY rectangle contains undefined native IMAGE pixels\n");return false;
    }
    const double scale=std::min(double(output_width)/rect.width,double(output_height)/rect.height);
    const int width=std::max(1,int(rect.width*scale)),height=std::max(1,int(rect.height*scale));
    const int left=(output_width-width)/2,top=(output_height-height)/2;
    GLint read=0,draw=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glPushAttrib(GL_ALL_ATTRIB_BITS);g_bind_framebuffer(GL_FRAMEBUFFER,0);glDrawBuffer(GL_BACK);
    glViewport(0,0,output_width,output_height);glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);
    glDisable(GL_SCISSOR_TEST);glDisable(GL_FOG);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    if(g_active_texture){g_active_texture(GL_TEXTURE1);glDisable(GL_TEXTURE_2D);g_active_texture(GL_TEXTURE0);}
    glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,image.object);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glOrtho(0,output_width,output_height,0,-1,1);
    glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
    const GLfloat u0=GLfloat(rect.dbx)/image.width,u1=GLfloat(rect.dbx+rect.width)/image.width;
    const GLfloat v0=GLfloat(rect.dby)/image.height,v1=GLfloat(rect.dby+rect.source_height)/image.height;
    glBegin(GL_QUADS);glTexCoord2f(u0,v0);glVertex2i(left,top);glTexCoord2f(u1,v0);glVertex2i(left+width,top);
    glTexCoord2f(u1,v1);glVertex2i(left+width,top+height);glTexCoord2f(u0,v1);glVertex2i(left,top+height);glEnd();
    glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);
    const bool drawn=!image_error("draw selected native IMAGE display");
    // GL_COLOR_BUFFER_BIT restores the saved draw/read buffer enums. Restore
    // their owning FBOs FIRST: COLOR_ATTACHMENT0 is invalid on default FBO 0.
    g_bind_framebuffer(GL_READ_FRAMEBUFFER,read);g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,draw);
    glPopAttrib();gl_state_invalidate();
    if(image_error("restore native IMAGE display state") || !drawn)return false;
    glFlush();return true; // Host owns the window swap, as for other presentation paths.
}
#endif

bool configure_presentation_layout(std::uint32_t fbp,std::uint32_t bw,std::uint32_t psm,const PresentationLayout& layout) {
#if defined(_WIN32)
    auto* target=select_display_target(fbp,bw,psm);
    const std::uint64_t right=std::uint64_t(layout.x)+layout.width,bottom=std::uint64_t(layout.y)+layout.height;
    if(!target || !layout.scan_width || !layout.scan_height || layout.scan_width>kWidth || layout.scan_height>kHeight ||
       !layout.width || !layout.height || right>std::uint64_t(bw)*64 || right>kWidth || bottom>kHeight){
        std::fprintf(stderr,"[native-gl-layout] invalid or unowned exact resource layout\n");return false;
    }
    target->presentation_layout=layout;return true;
#else
    (void)fbp;(void)bw;(void)psm;(void)layout;return false;
#endif
}

bool present_frame(const DisplayRegs &regs) {
    gl_state_invalidate();
#if defined(_WIN32)
    if(!direct_window() || !g_hdc || !g_hwnd || !g_blit_framebuffer)return false;
    if((wglGetCurrentContext()!=g_hrc || wglGetCurrentDC()!=g_hdc) && !wglMakeCurrent(g_hdc,g_hrc))return false;
    end_frame_timer();poll_frame_timer();
    RECT client{};if(!GetClientRect(g_hwnd,&client))return false;
    const int output_width=client.right-client.left,output_height=client.bottom-client.top;
    if(output_width<=0 || output_height<=0)return false;
    const int circuits=regs.pmode&3u;
    if(circuits==0){std::fprintf(stderr,"[native-gl-display] no enabled DISPLAY circuit\n");return false;}
    if(circuits==3){std::fprintf(stderr,"[native-gl-display] dual-circuit composition is unsupported\n");return false;}
    const int circuit=circuits==2?1:0;
    const auto fb=regs.dispfb[circuit],disp=regs.display[circuit];
    const unsigned fbp=fb&511u,bw=(fb>>9)&63u,psm=(fb>>15)&31u;
    if(const auto* image=find_native_image(fbp*32u);image && !image->framebuffer)
        return present_native_image(*image,regs,circuit,fb,disp,output_width,output_height);
    auto* target=select_display_target(fbp,bw,psm);if(!target)return false;
    const auto rect=display_rect(regs,fb,disp,target->presentation_layout?&*target->presentation_layout:nullptr);
    if(!rect.valid){std::fprintf(stderr,"[native-gl-display] DISPLAY rectangle exceeds selected framebuffer\n");return false;}
    if(const auto* image=find_native_image(fbp*32u);image &&
       !image_rect_covered(*image,rect.dbx,rect.dby,rect.width,rect.source_height)){
        std::fprintf(stderr,"[native-gl-display] DISPLAY rectangle includes undefined uploaded framebuffer pixels\n");return false;
    }
    if(image_error("before exact framebuffer presentation"))return false;
    // Preserve the established centered nearest integer scale for scene FBOs.
    const double fit=std::min(double(output_width)/rect.width,double(output_height)/rect.height);
    const double scale=fit>=1.0?std::floor(fit):fit;
    const int draw_width=std::max(1,int(std::round(rect.width*scale)));
    const int draw_height=std::max(1,int(std::round(rect.height*scale)));
    const int left=(output_width-draw_width)/2,bottom=(output_height-draw_height)/2;
    GLint read=0,draw=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING,&draw);
    glPushAttrib(GL_ALL_ATTRIB_BITS);
    g_bind_framebuffer(GL_READ_FRAMEBUFFER,target->framebuffer);g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);glDrawBuffer(GL_BACK);glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    g_blit_framebuffer(rect.dbx,kHeight-rect.dby-rect.source_height,rect.dbx+rect.width,kHeight-rect.dby,
        left,bottom,left+draw_width,bottom+draw_height,GL_COLOR_BUFFER_BIT,GL_NEAREST);
    const bool copied=!image_error("present exact DISPLAY framebuffer");
    g_bind_framebuffer(GL_READ_FRAMEBUFFER,read);g_bind_framebuffer(GL_DRAW_FRAMEBUFFER,draw);
    glPopAttrib();gl_state_invalidate();
    if(image_error("restore exact framebuffer presentation") || !copied)return false;
    glFlush();return true;
#else
    (void)regs;return false;
#endif
}

bool present_current() {
    return present_frame(gs::display_regs());
}

bool read_picture(std::vector<std::uint32_t> &rgba, std::uint32_t &width, std::uint32_t &height) {
    return read_picture(gs::display_regs(),rgba,width,height);
}
bool read_picture(const DisplayRegs& regs,std::vector<std::uint32_t>& rgba,std::uint32_t& width,std::uint32_t& height) {
    rgba.clear();width=height=0;
#if defined(_WIN32)
    if (!g_enabled || !g_hdc || !g_hrc) return false;
    const int circuit = (regs.pmode & 2u) ? 1 : (regs.pmode & 1u) ? 0 : -1;
    if (circuit < 0) return false;
    const std::uint64_t fb = regs.dispfb[circuit], disp = regs.display[circuit];
    const std::uint32_t display_fbp = static_cast<std::uint32_t>(fb) & 0x1FFu;
    if(const auto* image=find_native_image(display_fbp*32u);image && !image->framebuffer) {
        rgba.clear();width=height=0;
        const auto rect=display_rect(regs,fb,disp);
        const auto bw=static_cast<std::uint32_t>((fb>>9)&0x3fu),psm=static_cast<std::uint32_t>((fb>>15)&0x1fu);
        if(psm!=gs::PSMCT32 || image->bw!=bw || (regs.pmode&3u)==3u || !rect.valid ||
           !image_rect_covered(*image,rect.dbx,rect.dby,rect.width,rect.source_height)) {
            std::fprintf(stderr,"[native-gl-image] capture DISPLAY source has unsupported identity or undefined pixels\n");return false;
        }
        if((wglGetCurrentContext()!=g_hrc || wglGetCurrentDC()!=g_hdc) && !wglMakeCurrent(g_hdc,g_hrc))return false;
        if(image_error("before native IMAGE capture"))return false;
        try {
            std::vector<std::uint8_t> bytes(std::size_t(rect.width)*rect.source_height*4);
            std::vector<std::uint32_t> pixels(std::size_t(rect.width)*rect.source_height);
            GLint read=0,pack=0;glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);
            GLuint fbo=0;g_gen_framebuffers(1,&fbo);if(!fbo)return false;
            g_bind_framebuffer(GL_READ_FRAMEBUFFER,fbo);
            g_framebuffer_texture_2d(GL_READ_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,image->object,0);
            bool ok=g_check_framebuffer_status(GL_READ_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE;
            if(ok){glReadBuffer(GL_COLOR_ATTACHMENT0);glPixelStorei(GL_PACK_ALIGNMENT,4);
                // Raw IMAGE row0 is the DISPLAY's logical top row; no scene-FBO flip.
                glReadPixels(rect.dbx,rect.dby,rect.width,rect.source_height,GL_RGBA,GL_UNSIGNED_BYTE,bytes.data());
                ok=!image_error("capture selected native IMAGE pixels");}
            g_bind_framebuffer(GL_READ_FRAMEBUFFER,read);glPixelStorei(GL_PACK_ALIGNMENT,pack);
            g_delete_framebuffers(1,&fbo);gl_state_invalidate();
            if(image_error("restore native IMAGE capture") || !ok)return false;
            for(std::size_t i=0;i<pixels.size();++i)pixels[i]=std::uint32_t(bytes[4*i])|(std::uint32_t(bytes[4*i+1])<<8)|
                (std::uint32_t(bytes[4*i+2])<<16)|(std::uint32_t(bytes[4*i+3])<<24);
            rgba=std::move(pixels);width=rect.width;height=rect.source_height;return true;
        }catch(const std::bad_alloc&){return false;}catch(const std::length_error&){return false;}
    }
    if((regs.pmode&3u)==3u){std::fprintf(stderr,"[native-gl-display] dual-circuit capture is unsupported\n");return false;}
    const unsigned bw=(fb>>9)&63u,psm=(fb>>15)&31u;
    auto* target=select_display_target(display_fbp,bw,psm);if(!target)return false;
    const auto rect=display_rect(regs,fb,disp,target->presentation_layout?&*target->presentation_layout:nullptr);
    if(!rect.valid){std::fprintf(stderr,"[native-gl-display] capture rectangle exceeds selected framebuffer\n");return false;}
    if(const auto* image=find_native_image(display_fbp*32u);image &&
       !image_rect_covered(*image,rect.dbx,rect.dby,rect.width,rect.source_height)){
        std::fprintf(stderr,"[native-gl-display] capture includes undefined uploaded framebuffer pixels\n");return false;
    }
    if((wglGetCurrentContext()!=g_hrc || wglGetCurrentDC()!=g_hdc) && !wglMakeCurrent(g_hdc,g_hrc))return false;
    if(image_error("before exact framebuffer capture"))return false;
    try{
        std::vector<std::uint8_t> raw(std::size_t(rect.width)*rect.source_height*4);
        std::vector<std::uint32_t> pixels(std::size_t(rect.width)*rect.source_height);
        GLint read=0,pack=0,read_buffer=0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING,&read);glGetIntegerv(GL_PACK_ALIGNMENT,&pack);glGetIntegerv(GL_READ_BUFFER,&read_buffer);
        g_bind_framebuffer(GL_READ_FRAMEBUFFER,target->framebuffer);glReadBuffer(GL_COLOR_ATTACHMENT0);glPixelStorei(GL_PACK_ALIGNMENT,4);
        glReadPixels(rect.dbx,kHeight-rect.dby-rect.source_height,rect.width,rect.source_height,GL_RGBA,GL_UNSIGNED_BYTE,raw.data());
        const bool copied=!image_error("capture exact DISPLAY framebuffer");
        g_bind_framebuffer(GL_READ_FRAMEBUFFER,read);glReadBuffer(read_buffer);glPixelStorei(GL_PACK_ALIGNMENT,pack);gl_state_invalidate();
        if(image_error("restore exact framebuffer capture") || !copied)return false;
        for(int y=0;y<rect.source_height;++y)for(int x=0;x<rect.width;++x){
            const auto at=(std::size_t(rect.source_height-1-y)*rect.width+x)*4;
            pixels[std::size_t(y)*rect.width+x]=std::uint32_t(raw[at])|(std::uint32_t(raw[at+1])<<8)|
                (std::uint32_t(raw[at+2])<<16)|(std::uint32_t(raw[at+3])<<24);
        }
        rgba=std::move(pixels);width=rect.width;height=rect.source_height;return true;
    }catch(const std::bad_alloc&){return false;}catch(const std::length_error&){return false;}

#else
    (void)regs;
    (void)rgba;
    (void)width;
    (void)height;
    return false;
#endif
}

bool submit(const State &st, const Vertex *verts, int count) {
    const std::uint32_t prim_type = static_cast<std::uint32_t>(st.prim) & 0x7u;
    const std::uint64_t attr = (st.prmodecont & 1u) ? st.prim : ((st.prim & 7u) | (st.prmode & ~7ull));
#ifdef PS2_GPU
    // GL writes diagnostic readback to CPU GS memory, whereas primary WebGPU
    // owns a separate GPU copy and presents from it. Intercepting a primitive
    // here would make that primitive invisible to the active presentation path.
    // The visible native-GL path owns its window/context and scene FBO directly.
    if (gpu::mode() == gpu::Mode::primary && !direct_window()) {
        return reject_packet(st, attr, prim_type, count, 0, "primary WebGPU presentation is active");
    }
#endif
    if (!enabled()) return false;
    if (count <= 0 || !verts) return reject_packet(st, attr, prim_type, count, 1, "empty primitive or null vertex stream");
#if defined(_WIN32)
    if (!g_hdc || !g_hrc) return reject_packet(st, attr, prim_type, count, 2, "OpenGL device/context is not current");
    if ((wglGetCurrentContext() != g_hrc || wglGetCurrentDC() != g_hdc) && !wglMakeCurrent(g_hdc, g_hrc))
        return reject_packet(st, attr, prim_type, count, 2, "OpenGL context could not be made current");
    // The first primitive of a frame starts the GPU timer.
    begin_frame_timer();

    // Pick GL primitive from the PS2 PRIM register (low 3 bits: prim type).
    const Context &context = st.ctx[(attr >> 9) & 1u];
    const std::uint32_t frame = static_cast<std::uint32_t>(context.frame);
    const std::uint32_t fbw = (frame >> 16) & 0x3Fu;
    const std::uint32_t fpsm = (frame >> 24) & 0x3Fu;
    if (fbw == 0) return reject_packet(st, attr, prim_type, count, 4, "FRAME.FBW is zero");
    if (fpsm != gs::PSMCT32 && fpsm != gs::PSMCT24 && fpsm != gs::PSMCT16 && fpsm != gs::PSMCT16S)
        return reject_packet(st, attr, prim_type, count, 5, "framebuffer pixel format is unsupported");

    const std::uint32_t active_fbp = frame & 0x1FFu;
    if(const auto* image=find_native_image(active_fbp*32u);image && image->framebuffer &&
       (fpsm!=gs::PSMCT32 || image->bw!=fbw))
        return reject_packet(st,attr,prim_type,count,26,"native IMAGE framebuffer format/stride alias is unsupported");
    if (g_direct_window_context) {
        if (!bind_draw_target(active_fbp))
            return reject_packet(st, attr, prim_type, count, 26, "native draw framebuffer could not be created or preserved");

    }
    // Build-specific diagnostic snapshot: raw state and vertices, GS memory,
    // and the actual GL destination before this draw (RGBA8, bottom row first).
    // Capture only when the caller supplies both an output path and TEX0.
    if (const char *capture_path = std::getenv("MKA_GLYPH_CAPTURE_PATH")) {
        static bool captured = false;
        const char *wanted = std::getenv("MKA_GLYPH_CAPTURE_TEX0");
        if (!captured && *capture_path && wanted && *wanted && (attr & 0x10u) &&
            context.tex0 == std::strtoull(wanted, nullptr, 0) && g_direct_window_context &&
            find_scene_target(active_fbp)) {
            std::vector<std::uint8_t> destination(kWidth * kHeight * 4u);
            glFinish();
            glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, destination.data());
            if (std::FILE *f = ps2::os::open(capture_path, "wb")) {
                const std::uint32_t header[] = {0x32434B4Du, sizeof(State), sizeof(Vertex),
                    static_cast<std::uint32_t>(count), gs::kMemorySize, kWidth, kHeight};
                const bool ok = std::fwrite(header, sizeof header, 1, f) == 1 &&
                    std::fwrite(&st, sizeof st, 1, f) == 1 &&
                    std::fwrite(verts, sizeof(Vertex), count, f) == static_cast<std::size_t>(count) &&
                    std::fwrite(gs::memory(), gs::kMemorySize, 1, f) == 1 &&
                    std::fwrite(destination.data(), destination.size(), 1, f) == 1;
                const bool closed = std::fclose(f) == 0;
                captured = ok && closed;
                std::fprintf(stderr, "[native-gl-capture] %s %s TEX0=%016llX vertices=%d\n",
                             captured ? "saved" : "failed", capture_path,
                             static_cast<unsigned long long>(context.tex0), count);
            } else {
                std::fprintf(stderr, "[native-gl-capture] could not open %s\n", capture_path);
            }
        }
    }
    // GS fog (attr bit 5): the game supplies a fog value per vertex and the GS blends the final
    // colour towards FOGCOL by it - (F*color + (255-F)*fogcol) >> 8, after texturing and before
    // the alpha test, exactly as the reference rasterizer does (raster.cpp:206-212). Implemented
    // with GL's fixed-function fog below instead of dropping the primitive: GL_LINEAR over the
    // fog coordinate computes fogcol*(1-x) + color*x, so the coordinate is (255-F)/255 to give
    // the colour the F weight as on the GS. The rasterizer divides by 256 where GL divides by
    // 255 - at most one 8-bit unit of difference - and the reference suite does not sample fog.
    const bool fog_enabled = (attr & (1ull << 5)) != 0;
    if (fog_enabled && !g_fog_coordf)
        return reject_packet(st, attr, prim_type, count, 13, "GL fog coordinate support is unavailable");
    const bool textured = (attr & (1ull << 4)) != 0;
    const bool framebuffer_16 = fpsm == gs::PSMCT16 || fpsm == gs::PSMCT16S;
    const bool tex_alpha_enabled = (context.tex0 & (1ull << 34)) != 0;
    const std::uint32_t tfx = static_cast<std::uint32_t>((context.tex0 >> 35) & 3u);
    const bool alpha_blend = (attr & (1ull << 6)) != 0;
    if (alpha_blend) {
        const std::uint32_t alpha = static_cast<std::uint32_t>(context.alpha);
        const std::uint32_t a = alpha & 3u;
        const std::uint32_t b = (alpha >> 2) & 3u;
        const std::uint32_t c = (alpha >> 4) & 3u;
        const std::uint32_t d = (alpha >> 6) & 3u;
        const bool identity_blend = a == b && a <= 2 && d == 0;
        // ALPHA=0xA8 with FIX=0x80: (Cs - 0) * 128 / 128 + 0 = Cs.
        // This is a source replacement, not a standard alpha blend.
        const bool fixed_source_identity = a == 0 && b == 2 && c == 2 && d == 2 &&
                                           ((context.alpha >> 32) & 0xFFu) == 0x80u;
        const bool exact_destination_alpha_fade = framebuffer_16 && a == 2 && b == 1 && c == 1 && d == 1;
        if (identity_blend || fixed_source_identity) {
            cache_blend(false, 0, 0, 0, 0);
        } else if (exact_destination_alpha_fade && !(textured && (tex_alpha_enabled || tfx != 0)) &&
                   g_blend_func_separate) {
            // 16-bit destination-alpha fade: Cd * (1 - Ad/128). On PSMCT16 Ad
            // is one bit (0 or 0x80), so GL's destination-alpha factor is
            // exact once the alpha channel is quantized on each write.
            cache_blend(true, GL_ZERO, GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ZERO);
        } else if ((st.colclamp & 1u) && (!textured || tfx <= 1u) && g_blend_func_separate &&
                   // The destination alpha only enters the blend when the ALPHA
                   // register multiplies by it (C = Ad). A 24-bit framebuffer has
                   // no alpha channel at all, so its destination alpha is zero;
                   // a 16-bit framebuffer keeps one bit, which is a per-pixel
                   // value GL's destination-alpha factor cannot carry.
                   (fpsm == gs::PSMCT32 || fpsm == gs::PSMCT24 ||
                    ((fpsm == gs::PSMCT16 || fpsm == gs::PSMCT16S) && c != 1))) {
            // The general GS blend - what the reference rasterizer computes:
            // result = (A - B) * C / 128 + D over RGB (raster.cpp:267-272),
            // with the alpha channel written from the source (raster.cpp:282-286).
            // This replaces the tuple whitelist that dropped 6,985,985 of
            // 9,105,842 primitives in a single run: every ALPHA tuple the game
            // sent fell through to "not exactly supported" and was drawn by
            // neither GL nor (after 2026-11-16) the CPU rasterizer.
            // GL computes Cs * srcFactor + Cd * dstFactor, so the coefficient
            // of Cs is [A==Cs]*C - [B==Cs]*C + [D==Cs] and the coefficient of
            // Cd is the same with Cd. Each must be one of {0, 1, C, 1 - C} for
            // GL's factor set to express it (C+1 and -C cannot be).
            // C is As - the fragment alpha, which this path already scales to
            // GS/128 - or Ad - stored alpha is written GS*255/128, so GL's
            // /255 is the rasterizer's /128 - or FIX via glBlendColor at
            // FIX/128. FIX above 0x80 would need a factor above GL's clamped
            // 1.0 while the rasterizer keeps multiplying: rejected, counted.
            // colclamp is required because GL clamps where the rasterizer wraps
            // (raster.cpp:272). TFX >= 2 (HIGHLIGHT) has an unverified
            // two-texture-unit alpha path and stays rejected, counted; the
            // rasterizer ignores TFX on untextured primitives (raster.cpp:169)
            // and so does this check.
            const std::uint32_t fix = static_cast<std::uint32_t>(context.alpha >> 32) & 0xFFu;
            const int ck = c == 0 ? 0 : c == 1 ? 1 : 2;  // As, Ad, FIX
            int sc = 0, so = 0, dc = 0, dt = 0;          // C coefficient and the +1 term
            if (a == 0) ++sc; else if (a == 1) ++dc;
            if (b == 0) --sc; else if (b == 1) --dc;
            if (d == 0) ++so; else if (d == 1) ++dt;
            // A framebuffer without an alpha channel (PSMCT24) has a
            // destination alpha of zero, so the term that multiplies by Ad
            // vanishes and the result is the D term alone (raster.cpp:265-272).
            const bool destination_alpha_zero = (ck == 1) && (fpsm == gs::PSMCT24);
            const auto factor = [&](int coeff, int one, GLenum &out) -> bool {
                static const GLenum kC[3] = {GL_SRC_ALPHA, GL_DST_ALPHA, GL_CONSTANT_ALPHA};
                static const GLenum kOneMinusC[3] = {GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_DST_ALPHA,
                                                     GL_ONE_MINUS_CONSTANT_ALPHA};
                if (destination_alpha_zero) {
                    // Only the D term survives: the result is the source
                    // colour, the destination colour or black.
                    out = one ? GL_ONE : GL_ZERO;
                    return true;
                }
                if (coeff == 0) { out = one ? GL_ONE : GL_ZERO; return true; }
                if (coeff == 1 && one == 0) { out = kC[ck]; return true; }
                if (coeff == -1 && one == 1) { out = kOneMinusC[ck]; return true; }
                return false;
            };
            GLenum eq_rgb = GL_FUNC_ADD;
            GLenum src_factor = GL_ZERO, dst_factor = GL_ZERO;
            bool expressible = factor(sc, so, src_factor) && factor(dc, dt, dst_factor);
            if (!expressible && g_blend_equation_separate) {
                if (factor(sc, so, src_factor) && factor(-dc, -dt, dst_factor)) {
                    eq_rgb = GL_FUNC_SUBTRACT;
                    expressible = true;
                } else if (factor(-sc, -so, src_factor) && factor(dc, dt, dst_factor)) {
                    eq_rgb = GL_FUNC_REVERSE_SUBTRACT;
                    expressible = true;
                }
            }
            const bool fix_ok = ck != 2 || (fix <= 0x80u && g_blend_color != nullptr);
            if (expressible && fix_ok) {
                if (ck == 2)
                    cache_blend_const(static_cast<float>(fix) / 128.0f);
                cache_blend(true, src_factor, dst_factor, GL_ONE, GL_ZERO, eq_rgb, GL_FUNC_ADD);
            } else {
                static const char *kFixHigh = "GS ALPHA FIX above 0x80 needs a GL blend factor above 1.0";
                static const char *kNoBlendColor = "GL blend color is unavailable for the GS ALPHA FIX factor";
                const char *why = !fix_ok ? (fix <= 0x80u ? kNoBlendColor : kFixHigh) : alpha_tuple_key(alpha);
                return reject_packet(st, attr, prim_type, count, 14, why);
            }
        } else {
            static const char *kNoSeparate = "GL blend-func-separate is unavailable";
            static const char *kFrame = "GS ALPHA blend with the per-pixel destination alpha of a 16-bit framebuffer is not implemented yet";
            static const char *kClamp = "GS ALPHA blend without COLCLAMP wraps, which GL cannot express";
            static const char *kTfx = "GS ALPHA blend with TFX >= 2 is not verified against the rasterizer yet";
            const bool framebuffer_alpha_per_pixel = (fpsm == gs::PSMCT16 || fpsm == gs::PSMCT16S) && c == 1;
            const char *why = !g_blend_func_separate ? kNoSeparate
                           : framebuffer_alpha_per_pixel ? kFrame
                           : !(st.colclamp & 1u)     ? kClamp
                                                     : kTfx;
            return reject_packet(st, attr, prim_type, count, 14, why);
        }
    } else {
        cache_blend(false, 0, 0, 0, 0);
    }
    const std::uint64_t test = context.test;
    const bool zte = (test & (1ull << 16)) != 0;
    const std::uint32_t ztst = static_cast<std::uint32_t>(test >> 17) & 3u;
    const std::uint32_t atst = static_cast<std::uint32_t>(test >> 1) & 7u;
    const std::uint32_t aref = static_cast<std::uint32_t>(test >> 4) & 0xFFu;
    const std::uint32_t afail = static_cast<std::uint32_t>(test >> 12) & 3u;
    const bool zmsk = (context.zbuf & (1ull << 32)) != 0;
    // The GS alpha test. ATST/AREF live in TEST bits 1-3 / 4-11 (raster.cpp:335-336) and ATST's
    // order matches GL's alpha-function enums one for one (raster.cpp:218-226). AFAIL=0 (keep
    // nothing) is exactly GL's alpha test - discard the fragment - which is what cutout geometry
    // needs and what was being dropped. The fragment alpha carries GS*255/128 (the scaling the
    // blend path already applies), so the reference is aref/128; above 128 the GL value clamps at
    // 1.0 and the comparison would differ from the rasterizer's, so those stay rejected (counted).
    // AFAIL=1 keeps colour and only suppresses depth - already accepted when depth is unaffected;
    // AFAIL=2/3 cannot be expressed in one GL pass and stay rejected (counted).
    bool alpha_test_discard = false;
    if (test & 1u) {
        const bool no_depth_change = !zte || zmsk;
        if (afail == 0u && aref <= 128u) {
            alpha_test_discard = true;
        } else if (afail != 1u || !no_depth_change) {
            return reject_packet(st, attr, prim_type, count, 15, "GS alpha test changes framebuffer or depth results");
        }
    }
    if (alpha_test_discard) {
        static const GLenum kAtst[8] = {GL_NEVER, GL_ALWAYS, GL_LESS, GL_LEQUAL, GL_EQUAL, GL_GEQUAL, GL_GREATER, GL_NOTEQUAL};
        cache_alpha_test(true, kAtst[atst], static_cast<GLfloat>(aref) / 128.0f);
    } else {
        cache_alpha_test(false, 0, 0.0f);
    }
    if (context.test & (1ull << 14)) return reject_packet(st, attr, prim_type, count, 16, "GS destination-alpha test is not implemented yet");
    const std::uint32_t zpsm = 0x30u | (static_cast<std::uint32_t>(context.zbuf >> 24) & 0xFu);
    const std::uint64_t zbuf_key = context.zbuf & 0xFFFFFFFFull;
    const bool depth_surface_effect = zte && (ztst == 2 || ztst == 3 || (!zmsk && ztst != 0));
    const std::uint32_t zmax = depth_max_for_psm(zpsm);
    if (depth_surface_effect) {
        // The rasterizer accepts any 0x3X depth format: z_max() and
        // write_pixel() treat the formats they do not enumerate as 32-bit
        // (raster.cpp:150-155, gs.cpp:640), which is where the front-end's
        // ZBUF format nibble 0x0A (zpsm 0x3A, seen on FBP 91) lands.
        // depth_max_for_psm() already defaults to 32-bit the same way, so the
        // vertex depth mapping is consistent; only the validity check was
        // rejecting the surface after the ALPHA blend let the packet through.
        if ((zpsm & 0xF0u) != 0x30u)
            return reject_packet(st, attr, prim_type, count, 17, "GS depth-buffer pixel format is unsupported");
        if (g_depth_key_valid && zbuf_key != g_depth_key)
            return reject_packet(st, attr, prim_type, count, 24, "multiple GS ZBUF surfaces are not implemented yet");
        if (!g_depth_key_valid) {
            glPushAttrib(GL_SCISSOR_BIT | GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
            glDepthMask(GL_TRUE);
            glClearDepth(1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
            glPopAttrib();
            g_depth_key = zbuf_key;
            g_depth_key_valid = true;
        }
    }
    const std::uint32_t fbmask = static_cast<std::uint32_t>(context.frame >> 32);
    // A whole alpha-byte mask maps exactly to the GL channel write mask on
    // PSMCT32. Partial-bit masks and other framebuffer formats still need a
    // bitwise compositor and must not be approximated.
    if (fbmask != 0 && !(fpsm == gs::PSMCT32 && fbmask == 0xFF000000u))
        return reject_packet(st, attr, prim_type, count, 18, "GS framebuffer write mask is not implemented yet");
    if (context.fba & 1u) return reject_packet(st, attr, prim_type, count, 19, "GS framebuffer alpha expansion is not implemented yet");
    if (st.pabe & 1u) return reject_packet(st, attr, prim_type, count, 20, "GS per-pixel alpha blend enable is not implemented yet");
    if ((st.scanmsk & 3u) != 0) return reject_packet(st, attr, prim_type, count, 21, "GS scan mask is not implemented yet");
    // DTHE is defined for RGBA16. The GS manual says its result is not
    // guaranteed for PSMCT32/24, so this backend leaves those formats alone.
    GLenum mode = 0;
    switch (prim_type) {
        case 0: mode = GL_POINTS;        break;  // Point
        case 1: mode = GL_LINES;         break;  // Line
        case 2: mode = GL_LINE_STRIP;    break;  // LineStrip
        case 3: mode = GL_TRIANGLES;     break;  // Triangle
        case 4: mode = GL_TRIANGLE_STRIP;break;  // TriangleStrip
        case 5: mode = GL_TRIANGLE_FAN;  break;  // TriangleFan
        case 6: mode = GL_TRIANGLE_STRIP;break;  // Sprite: expand the two corners below
        default: return reject_packet(st, attr, prim_type, count, 6, "primitive type is unsupported");
    }
    if (prim_type == 6 && count != 2) return reject_packet(st, attr, prim_type, count, 7, "sprite packet does not contain exactly two corners");

    std::uint32_t texture_width = 1, texture_height = 1;
    if (textured) {
        if (!prepare_texture(st, context, attr, prim_type, count, 8, texture_width, texture_height)) return false;
    } else {
        if (g_active_texture) cache_texture2d(false, GL_TEXTURE1);
        cache_texture2d(false, GL_TEXTURE0);
    }
    const std::uint64_t submit_prim_idx = g_prims_drawn.load(std::memory_order_relaxed);
    if (submit_prim_idx >= 4085000 && submit_prim_idx <= 4095000) {
        const int ofx_d = static_cast<int>(st.ctx[(attr >> 9) & 1u].xyoffset & 0xFFFFu);
        const int ofy_d = static_cast<int>((st.ctx[(attr >> 9) & 1u].xyoffset >> 32) & 0xFFFFu);
        const float fx0 = static_cast<float>(static_cast<int>(verts[0].x) - ofx_d) / 16.0f;
        const float fy0 = static_cast<float>(static_cast<int>(verts[0].y) - ofy_d) / 16.0f;
        if ((fx0 >= 50.0f && fx0 <= 130.0f && fy0 >= 60.0f && fy0 <= 150.0f) ||
            (fx0 >= 230.0f && fx0 <= 320.0f && fy0 >= 140.0f && fy0 <= 210.0f)) {
            std::fprintf(stderr, "[prim-draw #%llu] type=%u cnt=%d tex=%d tw=%u th=%u fbp=%u pos=(%.1f,%.1f) rgba=(%u,%u,%u,%u) stq=(%g,%g,%g) uv=(%u,%u) fst=%d test=%016llX tex0=%016llX\n",
                         static_cast<unsigned long long>(submit_prim_idx), prim_type, count, textured, texture_width, texture_height,
                         active_fbp, fx0, fy0, verts[0].r, verts[0].g, verts[0].b, verts[0].a,
                         verts[0].s, verts[0].t, verts[0].q, verts[0].u, verts[0].v, (attr & (1ull << 8)) != 0,
                         static_cast<unsigned long long>(context.test),
                         static_cast<unsigned long long>(context.tex0));
        }
    }
    if (g_direct_window_context) {
        if (!bind_draw_target(active_fbp))
            return reject_packet(st, attr, prim_type, count, 26, "native draw framebuffer could not be created or preserved");
    }

    const std::uint64_t xyoffset = st.ctx[(attr >> 9) & 1u].xyoffset;
    const int ofx = static_cast<int>(xyoffset & 0xFFFFu);
    const int ofy = static_cast<int>((xyoffset >> 32) & 0xFFFFu);
    const std::uint64_t scissor = context.scissor;
    const int scax0 = static_cast<int>(scissor & 0x7FFu);
    const int scax1 = static_cast<int>((scissor >> 16) & 0x7FFu);
    const int scay0 = static_cast<int>((scissor >> 32) & 0x7FFu);
    const int scay1 = static_cast<int>((scissor >> 48) & 0x7FFu);
    if (scax0 > scax1 || scay0 > scay1)
        return reject_packet(st, attr, prim_type, count, 23, "GS scissor rectangle is inverted");
    const int clip_left = std::clamp(scax0, 0, kWidth);
    const int target_width = std::min(kWidth, static_cast<int>(fbw * 64u));
    const int clip_right = std::clamp(scax1 + 1, 0, target_width);
    const int clip_top = std::clamp(scay0, 0, kHeight);
    const int clip_bottom = std::clamp(scay1 + 1, 0, kHeight);
    cache_scissor(clip_left, kHeight - clip_bottom,
                  std::max(0, clip_right - clip_left), std::max(0, clip_bottom - clip_top));
    const bool fst = (attr & (1ull << 8)) != 0;
    const auto emit = [&](const Vertex &v, const Vertex &color, std::uint32_t x, std::uint32_t y,
                          float s, float t, float q) {
        // Pixel-centre alignment. The GS samples at the pixel's own coordinate;
        // GL samples at the fragment centre, half a pixel further on. This
        // shift moves the native path onto the GS sample point.
        static const float pixel_shift = [] {
            const char *value = std::getenv("PS2_NATIVE_GL_PIXEL_SHIFT");
            return value ? static_cast<float>(std::atof(value)) : 0.0f;
        }();
        const float fx = static_cast<float>(static_cast<int>(x) - ofx) / 16.0f + pixel_shift;
        const float fy = static_cast<float>(static_cast<int>(y) - ofy) / 16.0f + pixel_shift;
        const std::uint32_t z = std::min(v.z, zmax);
        const float z01 = static_cast<float>(static_cast<double>(z) / static_cast<double>(zmax));
        const float fz = z01 * 2.0f - 1.0f;
        if (textured) {
            glTexCoord4f(s, t, 0.0f, q);
            if (((context.tex0 >> 35) & 3u) >= 2u && g_multi_texcoord4f)
                g_multi_texcoord4f(GL_TEXTURE1, s, t, 0.0f, q);
        }
        // RGBAQ.A is also 0..128 with 128 opaque on the GS, so the same
        // saturation applies to the vertex colour, not just to the texture.
        const GLubyte alpha = framebuffer_16 && (!textured || !tex_alpha_enabled)
            ? (color.a >= 0x80u ? 0xFFu : 0u)
            : static_cast<GLubyte>(std::min<int>(255, static_cast<int>(color.a) * 2));
        glColor4ub(color.r, color.g, color.b, alpha);
        // Per-vertex fog value: the GS blends by this (0 = fog colour, 255 = full colour), and
        // GL_LINEAR with the coordinate (255-F)/255 reproduces that weighting (see fog_enabled).
        if (fog_enabled) g_fog_coordf((255.0f - static_cast<float>(v.f)) / 255.0f);
        glVertex3f(fx, fy, fz);
    };
    const auto uv = [&](const Vertex &v) {
        // Sampling alignment. The GS samples at the pixel's own coordinate and
        // its texel centres sit at the fractional .5 position; GL samples at
        // the fragment centre and blends around p-0.5. These two offsets make
        // the native path land on the same texel as the software rasterizer,
        // which is the reference implementation of the GS sampling rules.
        // tests/test_gl_vs_raster.cpp measures the agreement.
        static const float texel_offset = [] {
            const char *value = std::getenv("PS2_NATIVE_GL_TEXEL_OFFSET");
            return value ? static_cast<float>(std::atof(value)) : 0.0f;
        }();
        if (fst) {
            return std::array<float, 2>{
                static_cast<float>(v.u) / (16.0f * static_cast<float>(texture_width)) +
                    texel_offset / static_cast<float>(texture_width),
                static_cast<float>(v.v) / (16.0f * static_cast<float>(texture_height)) +
                    texel_offset / static_cast<float>(texture_height)};
        }
        return std::array<float, 2>{v.s + texel_offset / static_cast<float>(texture_width),
                                    v.t + texel_offset / static_cast<float>(texture_height)};
    };
    const Vertex &flat_color = verts[count - 1];
    if (fog_enabled) {
        // Fog colour comes from the GS FOGCOL register (RGB in the low 24 bits, raster.cpp:375).
        cache_fog(true,
                  static_cast<float>(st.fogcol & 0xFFu) / 255.0f,
                  static_cast<float>((st.fogcol >> 8) & 0xFFu) / 255.0f,
                  static_cast<float>((st.fogcol >> 16) & 0xFFu) / 255.0f);
    } else {
        cache_fog(false, 0.0f, 0.0f, 0.0f);
    }
    GLenum depth_func = GL_LESS;
    switch (ztst) {
    case 0: depth_func = GL_NEVER; break;
    case 1: depth_func = GL_ALWAYS; break;
    case 2: depth_func = GL_LEQUAL; break;
    default: depth_func = GL_LESS; break;
    }
    cache_depth(zte, depth_func, !zte || !zmsk);
    const ReadbackState readback_state{context.frame, st.dimx, st.dthe, st.colclamp};
    if (g_pending_readback_state_valid && (!g_direct_window_context || s_enable_readback) &&
        !same_readback_state(g_pending_readback_state, readback_state)) {
        // Snapshot draws with the state that produced them before a new target
        // or packing state can reinterpret the pending pixels.
        glFinish();
        do_readback(g_pending_readback_state);
        glDisable(GL_SCISSOR_TEST);
        if (!g_direct_window_context) glClear(GL_COLOR_BUFFER_BIT);
        g_pending_readback_state_valid = false;
        g_last_readback = clock_t_::now();
        gl_state_invalidate();
    }

    if (g_direct_window_context) {
        if (!bind_draw_target(active_fbp))
            return reject_packet(st, attr, prim_type, count, 26, "native draw framebuffer could not be created or preserved");
    }
    if (fbmask) cache_color_mask(true, true, true, false);
    glBegin(mode);
    if (prim_type == 6) {
        // A GS sprite is encoded by two opposite corners. Expand it into a
        // triangle strip. The texture coordinates follow the same two-corner
        // interpolation as the software rasterizer's sprite() implementation.
        const Vertex &a = verts[0], &b = verts[1];
        const auto auv = uv(a), buv = uv(b);
        const float q = fst || b.q == 0.0f ? 1.0f : b.q;
        // Half-picture-pixel UV correction. The GS samples at the pixel's own
        // integer coordinate; GL samples at the fragment centre, half a pixel further
        // on (x + 0.5, y + 0.5). Shifting the sprite UV by half a picture pixel
        // (-0.5f in UV space, with a +0.01f epsilon to break exact 0.5 midpoints
        // upward identically to the GS/WebGPU software rasterizer rounding) aligns
        // native GL sprite sampling with the reference implementation.
        const float span_x = static_cast<float>(static_cast<int>(b.x) - static_cast<int>(a.x)) / 16.0f;
        const float span_y = static_cast<float>(static_cast<int>(b.y) - static_cast<int>(a.y)) / 16.0f;
        static const float shift_x = [] {
            const char *value = std::getenv("PS2_NATIVE_GL_SPRITE_SHIFT_X");
            return value ? static_cast<float>(std::atof(value)) : -0.49f;
        }();
        static const float shift_y = [] {
            const char *value = std::getenv("PS2_NATIVE_GL_SPRITE_SHIFT_Y");
            return value ? static_cast<float>(std::atof(value)) : 0.0f;
        }();
        const float shift_u = span_x != 0.0f ? shift_x * (buv[0] - auv[0]) / span_x : 0.0f;
        const float shift_v = span_y != 0.0f ? shift_y * (buv[1] - auv[1]) / span_y : 0.0f;
        const float su = auv[0] + shift_u, sv = auv[1] + shift_v;
        const float tu = buv[0] + shift_u, tv = buv[1] + shift_v;
        emit(a, b, a.x, a.y, su, sv, q);
        emit(b, b, b.x, a.y, tu, sv, q);
        emit(a, b, a.x, b.y, su, tv, q);
        emit(b, b, b.x, b.y, tu, tv, q);
    } else {
        // Half-picture-pixel UV alignment for triangle and triangle-strip
        // packets (prim 3/4). The retail front-end draws its font glyphs and
        // UI quads as prim-4 strips, so the sprite-only correction below never
        // reached them and their texel columns were sampled half a pixel off,
        // which drops columns under nearest-neighbour minification.
        // The GS evaluates texture coordinates at the integer pixel (x, y);
        // GL evaluates the fragment centre (x + 0.5, y + 0.5), so the UV must
        // move by -0.49 * gradient in both axes: -0.5 would land exactly on
        // the GS sample point and float error could then round down at texel
        // boundaries, so the +0.01 tie-break biases upward like the sprite
        // shift. The gradient is constant across an affine primitive, so one
        // delta applies to every vertex.
        float du = 0.0f, dv = 0.0f;
        if (textured && count >= 3 && (prim_type == 3u || prim_type == 4u)) {
            static const float shift = [] {
                const char *value = std::getenv("PS2_NATIVE_GL_TRI_SHIFT");
                return value ? static_cast<float>(std::atof(value)) : -0.49f;
            }();
            const auto pixel = [&](const Vertex &v) {
                return std::array<float, 2>{(static_cast<int>(v.x) - ofx) / 16.0f,
                                            (static_cast<int>(v.y) - ofy) / 16.0f};
            };
            const std::array<float, 2> p0 = pixel(verts[0]), p1 = pixel(verts[1]), p2 = pixel(verts[2]);
            const std::array<float, 2> t0 = uv(verts[0]), t1 = uv(verts[1]), t2 = uv(verts[2]);
            const float ex1 = p1[0] - p0[0], ey1 = p1[1] - p0[1];
            const float ex2 = p2[0] - p0[0], ey2 = p2[1] - p0[1];
            const float det = ex1 * ey2 - ex2 * ey1;
            if (det != 0.0f) {
                const float du1 = t1[0] - t0[0], du2 = t2[0] - t0[0];
                const float dv1 = t1[1] - t0[1], dv2 = t2[1] - t0[1];
                const float dudx = (du1 * ey2 - du2 * ey1) / det;
                const float dudy = (ex1 * du2 - ex2 * du1) / det;
                const float dvdx = (dv1 * ey2 - dv2 * ey1) / det;
                const float dvdy = (ex1 * dv2 - ex2 * dv1) / det;
                du = shift * (dudx + dudy);
                dv = shift * (dvdx + dvdy);
            }
        }
        for (int i = 0; i < count; ++i) {
            const auto texcoord = uv(verts[i]);
            const Vertex &color = (attr & 8u) ? verts[i] : flat_color;
            const float q = fst || verts[i].q == 0.0f ? 1.0f : verts[i].q;
            emit(verts[i], color, verts[i].x, verts[i].y, texcoord[0] + du, texcoord[1] + dv, q);
        }
    }
    glEnd();
    if (fbmask) cache_color_mask(true, true, true, true);
    g_pending_readback_state = readback_state;
    g_pending_readback_state_valid = true;
    SceneTarget *active_target = find_scene_target(frame & 0x1FFu);
    if (active_target) {
        ++active_target->prim_count;
        ps2::census::gs_primitive(frame & 0x1FFu);
        active_target->last_write_tick = ++g_scene_write_tick;
        active_target->cpu_synced = false;
        active_target->last_draw_readback_state = readback_state;
        active_target->fbw = fbw;
    }

    g_prims_drawn.fetch_add(1, std::memory_order_relaxed);
    g_active_frame_width.store(target_width, std::memory_order_relaxed);
    g_active_frame_fbp.store(frame & 0x1FFu, std::memory_order_relaxed);
    // The primitive pipeline owns the surface it rendered into, so
    // presentation can tell a primitive picture from one the guest uploaded.
    gs::note_primitive_write(static_cast<std::uint32_t>(frame & 0x1FFu) * 32u);
    static bool reported = false;
    if (!reported) {
        reported = true;
        std::fprintf(stderr, "[native-gl] first GS primitive consumed by the native OpenGL path\n");
    }

    // CPU VRAM readback is only performed if explicitly requested via
    // PS2_NATIVE_GL_READBACK=1; otherwise on-demand lazy sync handles it.
    if (s_enable_readback) {
        const auto now = clock_t_::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_readback).count();
        if (elapsed >= kReadbackIntervalMs) {
            glFlush();
            do_readback(g_pending_readback_state);
            glDisable(GL_SCISSOR_TEST);
            if (!g_direct_window_context) glClear(GL_COLOR_BUFFER_BIT);
            g_pending_readback_state_valid = false;
            g_last_readback = now;
            gl_state_invalidate();
        }
    }
    return true;
#else
    (void)st; (void)verts; (void)count;
    return false;
#endif
}

void flush() {
    gl_state_invalidate();
#if defined(_WIN32)
    if (!g_enabled || !g_hdc) return;
    if (s_enable_readback && g_pending_readback_state_valid) {
        glFinish();
        do_readback(g_pending_readback_state);
        glDisable(GL_SCISSOR_TEST);
        if (!g_direct_window_context) glClear(GL_COLOR_BUFFER_BIT);
        g_pending_readback_state_valid = false;
    } else {
        glFlush();
    }
#endif
}

void shutdown() {
#if defined(_WIN32)
    const HDC previous_dc = wglGetCurrentDC();
    const HGLRC previous_context = wglGetCurrentContext();
    const HGLRC backend_context = g_hrc;
    if (has_context_objects()) {
        if (!g_hrc || !g_hdc ||
            ((previous_context != g_hrc || previous_dc != g_hdc) && !wglMakeCurrent(g_hdc, g_hrc))) {
            // Never abandon live IDs or permit attach over deferred cleanup.
            g_checked = true;
            std::fprintf(stderr, "[native-gl] shutdown deferred: resource-owning WGL context unavailable\n");
            return;
        }
        release_context_objects();
    }
    // Also reset pointer-only initialization after a failed attach with no
    // object allocations. This path makes no GL calls without an owner.
    reset_context_entrypoints();
    if (g_owns_context && g_hrc) {
        if (wglGetCurrentContext() == g_hrc && !wglMakeCurrent(nullptr, nullptr)) {
            g_checked = true;
            std::fprintf(stderr, "[native-gl] shutdown deferred: owned context could not be detached\n");
            return;
        }
        if (!wglDeleteContext(g_hrc)) {
            g_checked = true;
            std::fprintf(stderr, "[native-gl] shutdown deferred: owned context could not be deleted\n");
            return;
        }
    }
    // Borrowed SDL contexts are never deleted/detached here. Restore any
    // different context that was current before temporary resource cleanup.
    if (previous_context != backend_context && wglGetCurrentContext() != previous_context &&
        !wglMakeCurrent(previous_dc, previous_context))
        std::fprintf(stderr, "[native-gl] could not restore the caller's prior WGL context\n");
    g_hrc = nullptr;
    if (g_owns_context && g_hwnd && g_hdc) {
        if (!ReleaseDC(g_hwnd, g_hdc)) {
            g_checked = true;
            std::fprintf(stderr, "[native-gl] shutdown deferred: owned window DC could not be released\n");
            return;
        }
    }
    g_hdc = nullptr;
    if (g_owns_context && g_hwnd && !DestroyWindow(g_hwnd)) {
        g_checked = true;
        std::fprintf(stderr, "[native-gl] shutdown deferred: owned window could not be destroyed\n");
        return;
    }
    g_hwnd = nullptr;
    g_owns_context = false;
    g_direct_window_context = false;
#endif
    g_enabled = false;
    g_checked = false;
}

} // namespace ps2::gs::native_gl
