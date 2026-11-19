// native_gl_backend.cpp — native OpenGL rasterizer consuming the game's retail
// primitive stream at the ps2::vertex::submit_primitive seam.
//
// Interactive Windows runs attach to SDL's visible WGL context, render into a
// fixed-size native scene FBO, and blit the GS-selected display rectangle to the
// visible back buffer with centered nearest scaling. A compatibility readback
// still updates GS VRAM for guest transfer consumers; it is no longer the display
// path. Tests without a host window retain a hidden WGL diagnostic context.
//
// Opt-in via PS2_NATIVE_GL=1 or PS2_GS=native-gl. When disabled, gs::kick()
// continues through the configured CPU/WebGPU path.

#include "native_gl_backend.hpp"
#include "compliance_census.hpp"
#include "gpu/gpu.hpp"
#include "raster.hpp"
#include "mka/gpu_diagnostics.hpp"

#include "ps2/os.hpp"
#include "ps2/system.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <deque>
#include <fstream>
#include <new>
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
    std::uint64_t clut_hash = 0, vram_generation = 0;
    std::uint32_t width = 0, height = 0;
    // The texture's place in VRAM: the unit (256 bytes) it starts at and how
    // many units it covers, so a guest write into it can be found and pushed
    // into this native texture.
    std::uint32_t base = 0, units = 0;
    raster::TextureParams params{};
    std::uint64_t last_use = 0;
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
        if (entry.params.psm != psm || entry.params.tbw != tbw) continue;
        if (entry.clut_hash != clut_hash) continue;
        return &entry;
    }
    return nullptr;
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

// Dedicated movie presentation surface (see ensure_movie_fbo below).
GLuint g_movie_fbo = 0, g_movie_tex = 0;
bool g_movie_fbo_ready = false;
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
};
static constexpr std::size_t kMaxSceneTargets = 16;
SceneTarget g_scene_targets[kMaxSceneTargets];
GLuint g_shared_depth_buffer = 0;
bool g_evicted_color[512]{};
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

// Dedicated movie presentation surface. The game draws its own movie quad
// into the scene targets every frame (prims=2 per present), so even with the
// presented target pinned, blit=NO presents showed the game's quad and
// blit=YES presents showed the blit of the complete frame - two sources at
// different alignments alternating is the shake the 11:14 run still showed.
// This FBO is written only by the runtime's quiet-gated blit, never by the
// game, so the presented content cannot alternate while a movie plays.
bool ensure_movie_fbo() {
    if (g_movie_fbo_ready) return true;
    load_framebuffer_functions();
    if (!g_gen_framebuffers || !g_bind_framebuffer || !g_framebuffer_texture_2d ||
        !g_check_framebuffer_status || !g_blit_framebuffer)
        return false;
    if (!g_movie_tex) glGenTextures(1, &g_movie_tex);
    if (!g_movie_fbo) g_gen_framebuffers(1, &g_movie_fbo);
    if (!g_movie_tex || !g_movie_fbo) return false;
    glBindTexture(GL_TEXTURE_2D, g_movie_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    g_bind_framebuffer(GL_FRAMEBUFFER, g_movie_fbo);
    g_framebuffer_texture_2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_movie_tex, 0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum status = g_check_framebuffer_status(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_bind_framebuffer(GL_FRAMEBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[native-gl] movie presentation framebuffer is incomplete (0x%04X)\n",
                     static_cast<unsigned>(status));
        return false;
    }
    g_movie_fbo_ready = true;
    std::fprintf(stderr, "[native-gl] dedicated movie presentation surface ready\n");
    return true;
}

void release_scene_target() {
    for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
        if (g_scene_targets[i].framebuffer && g_delete_framebuffers)
            g_delete_framebuffers(1, &g_scene_targets[i].framebuffer);
        if (g_scene_targets[i].color_texture)
            glDeleteTextures(1, &g_scene_targets[i].color_texture);
        g_scene_targets[i] = SceneTarget{};
    }
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
    if (!g_timer_objects.empty() || g_timer_active_object || g_shared_depth_buffer ||
        g_movie_fbo || g_movie_tex || g_display_upload.object || g_rect_upload.object) return true;
    for (const SceneTarget &target : g_scene_targets)
        if (target.framebuffer || target.color_texture) return true;
    for (const TextureCache &entry : g_pool) if (entry.object) return true;
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
    for (TextureCache &entry : g_pool) {
        if (entry.object) glDeleteTextures(1, &entry.object);
        entry = TextureCache{};
    }
    if (g_movie_fbo && g_delete_framebuffers) g_delete_framebuffers(1, &g_movie_fbo);
    if (g_movie_tex) glDeleteTextures(1, &g_movie_tex);
    g_movie_fbo = 0; g_movie_tex = 0; g_movie_fbo_ready = false;
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
    for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
        if (g_scene_targets[i].ready && g_scene_targets[i].fbp == fbp) {
            g_scene_targets[i].last_use = ++g_scene_target_age;
            return &g_scene_targets[i];
        }
    }
    return nullptr;
}

SceneTarget *ensure_scene_target(std::uint32_t fbp = 0) {
    SceneTarget *existing = find_scene_target(fbp);
    if (existing) return existing;

    int best_slot = -1;
    for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
        if (!g_scene_targets[i].ready || g_scene_targets[i].fbp == 0xFFFFFFFFu) {
            best_slot = static_cast<int>(i);
            break;
        }
    }
    if (best_slot < 0) {
        const std::uint32_t active_fbp = g_active_frame_fbp.load(std::memory_order_relaxed);
        const DisplayRegs dregs = gs::display_regs();
        const int circuit = (dregs.pmode & 2u) ? 1 : (dregs.pmode & 1u) ? 0 : -1;
        const std::uint32_t display_fbp = circuit >= 0 ? (static_cast<std::uint32_t>(dregs.dispfb[circuit]) & 0x1FFu) : 0xFFFFFFFFu;

        std::uint64_t oldest = UINT64_MAX;
        for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
            if (g_scene_targets[i].fbp == display_fbp || g_scene_targets[i].fbp == active_fbp) {
                continue;
            }
            if (g_scene_targets[i].last_use < oldest) {
                oldest = g_scene_targets[i].last_use;
                best_slot = static_cast<int>(i);
            }
        }
        if (best_slot < 0) {
            for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
                if (g_scene_targets[i].last_use < oldest) {
                    oldest = g_scene_targets[i].last_use;
                    best_slot = static_cast<int>(i);
                }
            }
        }
    }
    if (best_slot < 0) best_slot = 0;

    SceneTarget &target = g_scene_targets[best_slot];
    if (!target.ready) {
        if (!allocate_scene_target(target)) return nullptr;
        std::fprintf(stderr, "[native-gl] direct GS scene target %d: %dx%d color + depth FBO for FBP=%u\n",
                     best_slot, kWidth, kHeight, fbp);
    } else {
        // Preserve GPU-only pixels before reusing the surface. The existing
        // readback routine restores the active draw target when it finishes.
        if (!target.cpu_synced && !sync_surface_to_cpu(target.fbp)) {
            std::fprintf(stderr, "[native-gl] cannot preserve dirty scene target FBP=%u before reuse\n", target.fbp);
            return nullptr;
        }
        if (target.last_write_tick && target.fbp < 512) g_evicted_color[target.fbp] = true;
        g_bind_framebuffer(GL_FRAMEBUFFER, target.framebuffer);
        clear_bound_scene_target(false);
        std::fprintf(stderr, "[native-gl] recycled scene target %d for FBP=%u (cleared)\n", best_slot, fbp);
    }
    // A recycled GL surface is now an empty surface for a different FBP.
    // Its previous owner's write age and readback state must not make the
    // cleared surface appear to contain a newer frame.
    target.prim_count = 0;
    target.write_count = 0;
    target.last_write_tick = 0;
    target.cpu_synced = true;
    target.last_draw_readback_state = {};
    target.fbp = fbp;
    target.last_use = ++g_scene_target_age;
    return &target;
}

void bind_draw_target(std::uint32_t fbp) {
    if (!g_direct_window_context) return;
    SceneTarget *target = ensure_scene_target(fbp);
    if (target && target->ready) {
        // Cached: bind_draw_target ran per packet with the same target, and
        // after present/readback invalidates the cache the bind is re-emitted.
        if (!g_glstate.valid || g_glstate.draw_fbo != target->framebuffer) {
            g_bind_framebuffer(GL_FRAMEBUFFER, target->framebuffer);
            glDrawBuffer(GL_COLOR_ATTACHMENT0);
            glReadBuffer(GL_COLOR_ATTACHMENT0);
            glViewport(0, 0, kWidth, kHeight);
            g_glstate.draw_fbo = target->framebuffer;
        }
    }
}

std::uint64_t clut_hash(const State &st) {
    std::uint64_t hash = 1469598103934665603ull;
    for (const std::uint32_t value : st.clut) {
        hash ^= value;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::uint32_t depth_max_for_psm(std::uint32_t psm) {
    switch (psm) {
    case gs::PSMZ24: return 0xFFFFFFu;
    case gs::PSMZ16: case gs::PSMZ16S: return 0xFFFFu;
    default: return 0xFFFFFFFFu;
    }
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
    const std::uint64_t palette = clut_hash(st);
    // The native texture for this base and palette, if the pool holds it and
    // the guest has not written over it since.
    const std::uint32_t tex_base = raster::texture_base(st);
    const std::uint32_t tex_psm = static_cast<std::uint32_t>((context.tex0 >> 20) & 0x3Fu);
    const std::uint32_t tex_tbw = static_cast<std::uint32_t>((context.tex0 >> 14) & 0x3Fu);
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
                         entry->clut_hash != palette;
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
        std::vector<std::uint32_t> rgba;
        if (!raster::decode_texture_rgba(st, rgba, width, height))
            return reject_packet(st, attr, prim_type, count, width_reason + 4, "GS texture pixel format is unsupported by the decoder");

        static const std::string texture_dump_dir = ps2::os::env("PS2_NATIVE_GL_TEXTURE_DUMP_DIR");
        static const std::uint64_t texture_dump_first_primitive = [] {
            const std::string value = ps2::os::env("PS2_NATIVE_GL_TEXTURE_DUMP_FIRST_PRIMITIVE");
            return static_cast<std::uint64_t>(std::strtoull(value.c_str(), nullptr, 10));
        }();
        static const std::uint64_t texture_dump_last_primitive = [] {
            const std::string value = ps2::os::env("PS2_NATIVE_GL_TEXTURE_DUMP_LAST_PRIMITIVE");
            return static_cast<std::uint64_t>(std::strtoull(value.c_str(), nullptr, 10));
        }();
        static unsigned texture_dump_serial = 0;
        const std::uint64_t primitive_index = g_prims_drawn.load(std::memory_order_relaxed);
        if (!texture_dump_dir.empty() && primitive_index >= texture_dump_first_primitive &&
            (texture_dump_last_primitive == 0 || primitive_index <= texture_dump_last_primitive) &&
            texture_dump_serial < 96) {
            const unsigned serial = ++texture_dump_serial;
            const std::uint32_t tpsm = static_cast<std::uint32_t>((context.tex0 >> 20) & 0x3Fu);
            const std::uint32_t mmin = (tex1 >> 6) & 7u;
            const std::uint32_t mmag = (tex1 >> 5) & 1u;
            const std::string path = texture_dump_dir + "/texture_" + std::to_string(serial) + ".ppm";
            std::ofstream file(path, std::ios::binary);
            file << "P6\n" << width << " " << height << "\n255\n";
            for (const std::uint32_t pixel : rgba) {
                const char rgb[] = {static_cast<char>(pixel), static_cast<char>(pixel >> 8),
                                    static_cast<char>(pixel >> 16)};
                file.write(rgb, sizeof(rgb));
            }
            // The RGB dump cannot show alpha, and whether the front-end font
            // has holes is an alpha question. Write the alpha channel as its
            // own grayscale image and summarise it in the log line.
            const std::string alpha_path = texture_dump_dir + "/texture_" + std::to_string(serial) + "_alpha.ppm";
            std::ofstream alpha_file(alpha_path, std::ios::binary);
            alpha_file << "P6\n" << width << " " << height << "\n255\n";
            std::uint32_t alpha_hist[256] = {};
            for (const std::uint32_t pixel : rgba) {
                const std::uint8_t a = static_cast<std::uint8_t>(pixel >> 24);
                alpha_hist[a]++;
                const char gray[] = {static_cast<char>(a), static_cast<char>(a), static_cast<char>(a)};
                alpha_file.write(gray, sizeof(gray));
            }
            std::uint32_t distinct = 0, amin = 255, amax = 0;
            for (unsigned i = 0; i < 256; ++i) {
                if (!alpha_hist[i]) continue;
                distinct++;
                if (i < amin) amin = i;
                if (i > amax) amax = i;
            }
            const std::uint32_t opaque = alpha_hist[255], transparent = alpha_hist[0];
            std::fprintf(stderr,
                         "[native-gl-texture] primitive=%llu dump=%u %s TEX0=%016llX TEX1=%016llX PSM=%u size=%ux%u MMIN=%u MMAG=%u TFX=%u TCC=%u saved=%d alpha distinct=%u min=%u max=%u opaque=%u transparent=%u %s\n",
                         static_cast<unsigned long long>(primitive_index), serial, path.c_str(),
                         static_cast<unsigned long long>(context.tex0),
                         static_cast<unsigned long long>(context.tex1), tpsm, width, height,
                         mmin, mmag, tfx, static_cast<unsigned>((context.tex0 >> 34) & 1u), file.good() ? 1 : 0,
                         distinct, amin, amax, opaque, transparent, alpha_path.c_str());
        }

        const std::uint32_t minu = static_cast<std::uint32_t>(context.clamp >> 4) & 0x3FFu;
        const std::uint32_t maxu = static_cast<std::uint32_t>(context.clamp >> 14) & 0x3FFu;
        const std::uint32_t minv = static_cast<std::uint32_t>(context.clamp >> 24) & 0x3FFu;
        const std::uint32_t maxv = static_cast<std::uint32_t>(context.clamp >> 34) & 0x3FFu;
        if ((wms == 2 && (minu > maxu || maxu >= width)) || (wmt == 2 && (minv > maxv || maxv >= height)))
            return reject_packet(st, attr, prim_type, count, width_reason + 5, "GS region clamp bounds fall outside the decoded texture");
        if (wms == 2 || wmt == 2) {
            const std::vector<std::uint32_t> source = rgba;
            for (std::uint32_t y = 0; y < height; ++y) {
                const std::uint32_t source_y = wmt == 2 ? std::clamp(y, minv, maxv) : y;
                for (std::uint32_t x = 0; x < width; ++x) {
                    const std::uint32_t source_x = wms == 2 ? std::clamp(x, minu, maxu) : x;
                    rgba[static_cast<std::size_t>(y) * width + x] = source[static_cast<std::size_t>(source_y) * width + source_x];
                }
            }
        }
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        // The GS alpha range is 0..128 with 128 meaning opaque; OpenGL is
        // 0..255. Uploading the byte unchanged put every texture at half
        // opacity (128 -> 0.502), which washes out text and UI and makes thin
        // strokes look broken. Scale it, saturating at opaque.
        for (std::uint32_t &pixel : rgba) {
            const std::uint32_t a = (pixel >> 24) & 0xFFu;
            pixel = (pixel & 0x00FFFFFFu) | (std::min<std::uint32_t>(255u, a * 2u) << 24);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        entry->tex0 = context.tex0;
        entry->tex1 = context.tex1;
        entry->tex2 = context.tex2;
        entry->clamp = context.clamp;
        entry->texa = st.texa;
        entry->clut_hash = palette;
        entry->vram_generation = gs::g_vram_generation;
        entry->width = width;
        entry->height = height;
        entry->valid = true;
        entry->stale = false;
        entry->base = tex_base;
        entry->units = texture_units(width, height);
        entry->params.psm = tex_psm;
        entry->params.tbp = tex_base;
        entry->params.tbw = tex_tbw;
        entry->params.cpsm = static_cast<std::uint32_t>((context.tex0 >> 51) & 0xFu);
        entry->params.csa = static_cast<std::uint32_t>((context.tex0 >> 56) & 0x1Fu);
        entry->params.ta0 = static_cast<std::uint32_t>(st.texa) & 0xFFu;
        entry->params.aem = (st.texa >> 15) & 1u;
        entry->params.clut = st.clut;
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
    for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
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
bool guest_wrote_vram(std::uint32_t psm, std::uint32_t base, std::uint32_t bw,
                      std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc || !w || !h) return false;
    if (wglGetCurrentContext() != g_hrc && !wglMakeCurrent(g_hdc, g_hrc)) return false;
    const std::uint32_t bytes_per_pixel =
        psm == gs::PSMCT24 ? 3u : (psm == gs::PSMCT16 || psm == gs::PSMCT16S) ? 2u : 4u;
    const std::uint32_t units = (w * h * bytes_per_pixel + 255u) / 256u + 1u;
    bool pushed = false;
    for (TextureCache &entry : g_pool) {
        if (!entry.valid) continue;
        if (base + units <= entry.base || base >= entry.base + entry.units) continue;
        if (entry.base == base && entry.params.psm == psm && entry.params.tbw == bw &&
            x + w <= entry.width && y + h <= entry.height) {
            std::vector<std::uint32_t> pixels;
            if (raster::decode_rect_rgba(entry.params, x, y, w, h, pixels)) {
                // The GS alpha range is 0..128 with 128 meaning opaque;
                // OpenGL is 0..255 (the same scaling the full upload uses).
                for (std::uint32_t &pixel : pixels) {
                    const std::uint32_t a = (pixel >> 24) & 0xFFu;
                    pixel = (pixel & 0x00FFFFFFu) | (std::min<std::uint32_t>(255u, a * 2u) << 24);
                }
                const GLuint previous = g_glstate.texture;
                glBindTexture(GL_TEXTURE_2D, entry.object);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
                glTexSubImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(x), static_cast<GLint>(y),
                                static_cast<GLsizei>(w), static_cast<GLsizei>(h), GL_RGBA,
                                GL_UNSIGNED_BYTE, pixels.data());
                glBindTexture(GL_TEXTURE_2D, previous);
                entry.stale = false;
                pushed = true;
                continue;
            }
        }
        entry.stale = true;
    }
    return pushed;
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
    std::fill(std::begin(g_evicted_color), std::end(g_evicted_color), false);
    for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
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
    const auto env_enabled = [](const char *name) {
        std::string value = ps2::os::env(name);
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return !value.empty() && value != "0" && value != "off" && value != "false" && value != "no";
    };
    std::string backend = ps2::os::env("PS2_GS");
    std::transform(backend.begin(), backend.end(), backend.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return env_enabled("PS2_NATIVE_GL") || env_enabled("PS2_NATIVE_GL_STRICT") || backend == "native-gl" ||
           env_enabled("PS2_GS_PACKET_NATIVE") || env_enabled("PS2_GS_PACKET_NATIVE_STRICT");
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
bool draw_vram_display(const std::uint32_t *rgba, int width, int height,
                       int output_width, int output_height,
                       int draw_x, int draw_y, int draw_width, int draw_height) {
    gl_state_invalidate();
    GLuint &texture = g_display_upload.object;
    int &texture_width = g_display_upload.width, &texture_height = g_display_upload.height;
    if (!texture) {
        glGenTextures(1, &texture);
        if (!texture) return false;
    }
    if (g_active_texture) g_active_texture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    if (width != texture_width || height != texture_height) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        texture_width = width;
        texture_height = height;
    } else {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    }

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, output_width, output_height, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f); glVertex2i(draw_x, draw_y);
    glTexCoord2f(1.0f, 0.0f); glVertex2i(draw_x + draw_width, draw_y);
    glTexCoord2f(1.0f, 1.0f); glVertex2i(draw_x + draw_width, draw_y + draw_height);
    glTexCoord2f(0.0f, 1.0f); glVertex2i(draw_x, draw_y + draw_height);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    return true;
}

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
    bind_draw_target(g_active_frame_fbp.load(std::memory_order_relaxed));
    return true;
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

DisplayRect display_rect(const DisplayRegs &regs, int circuit, std::uint64_t fb, std::uint64_t disp,
                         bool is_active_target) {
    DisplayRect rect;
    rect.dbx = is_active_target ? 0 : static_cast<int>((fb >> 32) & 0x7FFu);
    rect.dby = is_active_target ? 0 : static_cast<int>((fb >> 43) & 0x7FFu);
    const int magh = static_cast<int>((disp >> 23) & 0xFu) + 1;
    const int magv = static_cast<int>((disp >> 27) & 3u) + 1;
    const int display_width = (static_cast<int>((disp >> 32) & 0xFFFu) + 1) / magh;
    int height = (static_cast<int>((disp >> 44) & 0x7FFFu) + 1) / magv;
    if ((regs.smode2 & 3u) == 1u && height > 576) height /= 2;
    // The display registers can describe a wider buffer than the game draws
    // into. The front end renders inside a 640x448 scissor while DISPFB reads
    // 832 columns of an 832-pixel-wide buffer; presenting those extra columns
    // showed 192 pixels the game never drew and forced a 1.538x scale, whose
    // uneven 1/2/3-pixel doubling is what blurs the text. Clamp the displayed
    // rectangle to the area the GS scissor actually allows the game to draw.
    const std::uint64_t scissor = g_state.ctx[circuit].scissor;
    const int scissor_x0 = static_cast<int>(scissor & 0x7FFu);
    const int scissor_x1 = static_cast<int>((scissor >> 16) & 0x7FFu);
    const int scissor_y0 = static_cast<int>((scissor >> 32) & 0x7FFu);
    const int scissor_y1 = static_cast<int>((scissor >> 48) & 0x7FFu);
    // An unset scissor reads as (0,0)-(0,0): treat that as "no scissor yet"
    // instead of a one-pixel picture, which stretched a single pixel across
    // the whole window at boot and during the movie.
    const bool scissor_valid = scissor_x1 > scissor_x0 && scissor_y1 > scissor_y0;
    const int picture_width = scissor_valid ? scissor_x1 - scissor_x0 + 1 : display_width;
    const int picture_height = scissor_valid ? scissor_y1 - scissor_y0 + 1 : height;
    const int width = std::min(display_width, picture_width);
    height = std::min(height, picture_height);
    rect.width = width;
    rect.height = height;
    rect.display_width = display_width;
    rect.halved_field = (regs.smode2 & 3u) == 3u;
    rect.source_height = rect.halved_field ? (height + 1) / 2 : height;
    rect.valid = width > 0 && height > 0 && rect.dbx >= 0 && rect.dby >= 0 &&
                 rect.dbx + width <= kWidth && rect.dby + rect.source_height <= kHeight;
    return rect;
}

// The surface the display controller scans out, and whether a movie is
// playing. DISPFB stays at FBP 0 in MKA's front end while everything is drawn
// into FRAME_1 (FBP 91), so the surface with the most recent write wins.
// During front end: Target 91 wins because Target 0 is idle. During movie
// playback: Target 0 / 140 win because movie quads/transfers update their
// ticks past the stale front-end target.
//
// Movie state is needed before target selection: while a movie plays, the
// presentation pins one surface (below), so the movie source is looked up first.
struct PresentTarget {
    SceneTarget *target = nullptr;
    SceneTarget *movie_source = nullptr;
    bool movie_playing = false;
};

PresentTarget select_present_target(std::uint32_t display_fbp, std::uint32_t active_frame_fbp) {
    PresentTarget selection;
    selection.movie_source = find_scene_target(280);
    SceneTarget *active_target = find_scene_target(active_frame_fbp);
    SceneTarget *display_target = find_scene_target(display_fbp);
    const std::uint64_t movie_tick = selection.movie_source ? selection.movie_source->last_write_tick : 0;
    const std::uint64_t active_tick = active_target ? active_target->last_write_tick : 0;
    const std::uint64_t display_tick = display_target ? display_target->last_write_tick : 0;
    selection.movie_playing = (movie_tick > 0 && movie_tick >= active_tick && movie_tick >= display_tick);
    SceneTarget *target = nullptr;
    if (active_target && display_target) {
        if (active_target->last_write_tick == 0 && display_target->last_write_tick == 0) {
            target = display_target;
        } else {
            target = (active_target->last_write_tick >= display_target->last_write_tick)
                         ? active_target : display_target;
        }
    } else if (active_target && active_target->last_write_tick > 0) {
        target = active_target;
    } else {
        target = display_target;
    }
    if (!target) target = ensure_scene_target(display_fbp);
    if (!target || !target->ready) return selection;

    // Pin the presented surface while a movie is playing. The game double-buffers
    // (FBP 0 and 140) and quads the movie into whichever buffer drew last, so the
    // tick-based selection above alternates the presented buffer between frames:
    // one present showed the fresh blit, the next showed the other buffer's quad -
    // two different sources, which is the shake the 00:40 run measured (blit=NO on
    // write-presents, blit=YES on the next, targets alternating 0/140). While a
    // movie surface exists, presentation therefore sticks to the surface that was
    // selected first, and that surface is refreshed only from complete movie frames
    // (the quiet-gated blit below, which now reaches every scene target). When no
    // movie surface exists any more the pin is released and normal selection resumes.
    static std::uint32_t s_movie_display_fbp = 0xFFFFFFFFu;
    if (selection.movie_playing) {
        SceneTarget *pinned = (s_movie_display_fbp != 0xFFFFFFFFu) ? find_scene_target(s_movie_display_fbp) : nullptr;
        if (!pinned) {
            s_movie_display_fbp = target->fbp;
        } else if (pinned != target) {
            target = pinned;
        }
    } else {
        s_movie_display_fbp = 0xFFFFFFFFu;
    }
    selection.target = target;
    return selection;
}

bool present_frame(const DisplayRegs &regs) {
    gl_state_invalidate();
#if defined(_WIN32)
    if (!direct_window() || !g_hdc || !g_hwnd || !g_blit_framebuffer) return false;
    if (wglGetCurrentContext() != g_hrc || wglGetCurrentDC() != g_hdc) {
        if (!wglMakeCurrent(g_hdc, g_hrc)) {
            std::fprintf(stderr, "[native-gl] could not make the SDL WGL context current for presentation\n");
            return false;
        }
    }
    // Query operations require the same current context as the draw commands.
    end_frame_timer();
    poll_frame_timer();

    RECT client{};
    if (!GetClientRect(g_hwnd, &client)) return false;
    const int output_width = client.right - client.left;
    const int output_height = client.bottom - client.top;
    if (output_width <= 0 || output_height <= 0) return false;

    const int circuit = (regs.pmode & 2u) ? 1 : (regs.pmode & 1u) ? 0 : -1;
    static std::uint64_t last_dfb0 = 0xFFFFFFFFull, last_dfb1 = 0xFFFFFFFFull;
    if (regs.dispfb[0] != last_dfb0 || regs.dispfb[1] != last_dfb1) {
        last_dfb0 = regs.dispfb[0];
        last_dfb1 = regs.dispfb[1];
        std::fprintf(stderr, "[native-gl-debug] dispfb change: [0]=%016llX [1]=%016llX pmode=%016llX\n",
                     static_cast<unsigned long long>(last_dfb0),
                     static_cast<unsigned long long>(last_dfb1),
                     static_cast<unsigned long long>(regs.pmode));
    }
    if (circuit < 0) {
        bind_draw_target(g_active_frame_fbp.load(std::memory_order_relaxed));
        return true;
    }

    const std::uint64_t fb = regs.dispfb[circuit], disp = regs.display[circuit];
    const std::uint32_t display_fbp = static_cast<std::uint32_t>(fb) & 0x1FFu;
    const std::uint32_t display_fbw = static_cast<std::uint32_t>(fb >> 9) & 0x3Fu;
    const std::uint32_t display_psm = static_cast<std::uint32_t>(fb >> 15) & 0x1Fu;
    const std::uint32_t active_frame_fbp = g_active_frame_fbp.load(std::memory_order_relaxed);

    // Target selection:
    // In normal execution, DISPFB designates the completed frame scanned out to
    // the display controller. However, in MKA's front end (legal screen, GameSpy,
    // memory card prompts), DISPFB remains at FBP 0 while all drawing occurs in
    // FRAME_1 (FBP 91). Target 0 receives no writes after boot, while Target 91
    // receives tens of thousands of primitives per frame.
    // By comparing last_write_tick between active_target and display_target,
    // the target that received the most recent write wins presentation.
    // During front end: Target 91 wins because Target 0 is idle.
    // During movie playback: Target 0 / 140 win because movie quads/transfers
    // update their ticks past the stale front-end target.
    // Movie state is needed before target selection: while a movie plays, the
    // presentation pins one surface (below), so the movie source is looked up first.
    const PresentTarget selection = select_present_target(display_fbp, active_frame_fbp);
    SceneTarget *movie_source = selection.movie_source;
    const bool movie_playing = selection.movie_playing;
    SceneTarget *active_target = find_scene_target(active_frame_fbp);
    SceneTarget *display_target = find_scene_target(display_fbp);
    SceneTarget *target = selection.target;
    if (!target || !target->ready) return false;

    static std::uint32_t s_last_logged_target = 0xFFFFFFFFu;
    if (target->fbp != s_last_logged_target) {
        s_last_logged_target = target->fbp;
        std::fprintf(stderr, "[native-gl-target] display_fbp=%u active_fbp=%u selected_target_fbp=%u prim_count=%llu\n",
                     display_fbp, active_frame_fbp, target->fbp, static_cast<unsigned long long>(target->prim_count));
        std::fprintf(stderr, "[native-gl-present-debug] disp_fbp=%u act_fbp=%u targets:", display_fbp, active_frame_fbp);
        for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
            if (g_scene_targets[i].ready && g_scene_targets[i].fbp != 0xFFFFFFFFu) {
                std::fprintf(stderr, " [%zu]=(fbp=%u,cnt=%llu,wr=%llu)",
                             i, g_scene_targets[i].fbp,
                             static_cast<unsigned long long>(g_scene_targets[i].prim_count),
                             static_cast<unsigned long long>(g_scene_targets[i].write_count));
            }
        }
        std::fprintf(stderr, "\n");
    }

    const bool is_active_target = (target == active_target && target != display_target);
    const std::uint32_t target_fbw = (target && target->fbw > 0) ? target->fbw : display_fbw;

    if (target->fbp < 512 && g_evicted_color[target->fbp]) {
        const int width = std::min(kWidth, static_cast<int>(target_fbw * 64u));
        std::vector<std::uint32_t> rgba(static_cast<std::size_t>(width) * kHeight);
        for (int y = 0; y < kHeight; ++y)
            for (int x = 0; x < width; ++x)
                rgba[static_cast<std::size_t>(kHeight - 1 - y) * width + x] =
                    gs::read_pixel(display_psm, target->fbp * 32u, target_fbw, x, y);
        GLint previous_texture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
        glBindTexture(GL_TEXTURE_2D, target->color_texture);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
        g_evicted_color[target->fbp] = false;
    }

    const bool movie_existed = (movie_playing && movie_source != target);
    bool blit_ran = false;

    static std::uint64_t s_last_vram_gen = 0;
    const std::uint64_t cur_vram_gen = gs::g_vram_generation;
    const bool vram_gen_changed = (cur_vram_gen != s_last_vram_gen);
    s_last_vram_gen = cur_vram_gen;

    // A movie is playing: present the movie's own surface instead of leaving the screen on the
    // game's textured quad, which samples FBP 280 while the decoder is still writing it. The
    // movie arrives as ~40 strip transfers per frame, so a present inside that burst shows a
    // half-written frame - that is the flicker (22:50 run: the old prim/write gate never fired,
    // blit 0 of 16,691, and 8,328 of those presents landed on a VRAM write in progress). The
    // blit therefore only runs while FBP 280 is QUIET (no VRAM write since the previous present,
    // which means the frame's strips are all in): between complete frames the presented target
    // keeps the last blit, so the screen holds a whole frame instead of alternating clean/torn.
    // If the writes never quiet down, blit anyway after 8 presents rather than freeze the movie.
    static std::uint64_t s_movie_changed_run = 0;
    static bool s_movie_shown = false;
    if (movie_existed) {
        s_movie_changed_run = vram_gen_changed ? s_movie_changed_run + 1 : 0;
        // The first present of a movie blits even when not quiet (a single
        // possibly-torn frame beats a black surface), then the quiet gate
        // takes over.
        if (s_movie_changed_run == 0 || s_movie_changed_run >= 8 || !s_movie_shown) {
            // Dedicated movie surface first: it is the only content that
            // cannot alternate with the game's own quad, which keeps drawing
            // into the scene targets every frame (see ensure_movie_fbo).
            const bool have_movie_fbo = ensure_movie_fbo();
            g_bind_framebuffer(GL_READ_FRAMEBUFFER, movie_source->framebuffer);
            if (have_movie_fbo) {
                g_bind_framebuffer(GL_DRAW_FRAMEBUFFER, g_movie_fbo);
                g_blit_framebuffer(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                s_movie_shown = true;
            }
            // Then refresh EVERY scene target so buffer alternation after the
            // pin releases also carries the same complete frame. cpu_synced is
            // cleared because the CPU-side copy of each blitted surface is now
            // stale - the next reuse must read the new content back first
            // (ensure_scene_target already syncs an unsynced target before
            // recycling it).
            int blitted = 0;
            for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
                SceneTarget &other = g_scene_targets[i];
                if (!other.ready || other.fbp == 0xFFFFFFFFu || &other == movie_source) continue;
                g_bind_framebuffer(GL_DRAW_FRAMEBUFFER, other.framebuffer);
                g_blit_framebuffer(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                other.write_count = movie_source->write_count;
                other.last_write_tick = movie_source->last_write_tick;
                other.cpu_synced = false;
                ++blitted;
            }
            blit_ran = true;
            std::fprintf(stderr, "[native-gl] sampled offscreen target FBP=%u into %d scene targets + movie surface=%d (presented FBP=%u)\n",
                         movie_source->fbp, blitted, have_movie_fbo ? 1 : 0, target->fbp);
        }
    } else {
        s_movie_shown = false;
    }

    if (movie_existed) {
        ps2::census::movie_present(blit_ran, true);
        // Every present is logged unless PS2_TRACE_MOVIE_PRESENT says otherwise: this line is the
        // flicker evidence, and sampling it used to hide exactly the frames that matter.
        // 0 = off, N = every Nth present, unset = every present.
        static const std::uint64_t movie_every = [] {
            const char *value = std::getenv("PS2_TRACE_MOVIE_PRESENT");
            return (value && *value) ? std::strtoull(value, nullptr, 10) : 1ull;
        }();
        static std::uint64_t s_movie_log_counter = 0;
        if (movie_every && (++s_movie_log_counter % movie_every == 0)) {
            std::fprintf(stderr,
                         "[native-gl-movie-present #%llu] target_fbp=%u (prims=%llu, writes=%llu, tick=%llu) "
                         "movie280_writes=%llu blit=%s vram_gen_changed=%d vram_gen=%llu\n",
                         static_cast<unsigned long long>(s_movie_log_counter), target->fbp,
                         static_cast<unsigned long long>(target->prim_count),
                         static_cast<unsigned long long>(target->write_count),
                         static_cast<unsigned long long>(target->last_write_tick),
                         static_cast<unsigned long long>(movie_source ? movie_source->write_count : 0),
                         blit_ran ? "YES" : "NO",
                         vram_gen_changed ? 1 : 0,
                         static_cast<unsigned long long>(cur_vram_gen));
        }
    }

    // During a movie the dedicated surface is the only content that cannot
    // alternate: the game's quad keeps overwriting the scene targets, so
    // presenting those alternated quad content with the blit (the shake).
    const bool present_movie = movie_existed && s_movie_shown && g_movie_fbo_ready;
    g_bind_framebuffer(GL_READ_FRAMEBUFFER, present_movie ? g_movie_fbo : target->framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    g_bind_framebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glDrawBuffer(GL_BACK);
    glViewport(0, 0, output_width, output_height);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    const DisplayRect rect = display_rect(regs, circuit, fb, disp, is_active_target);
    const int dbx = rect.dbx, dby = rect.dby;
    const int width = rect.width, height = rect.height;
    const int display_width = rect.display_width;
    const bool halved_field = rect.halved_field;
    const int source_height = rect.source_height;
    const int active_frame_width = g_active_frame_width.load(std::memory_order_relaxed);
    // FRAME.FBW is the actual draw surface pitch in 64-pixel units. The
    // current direct FBO captures that surface's geometry, not empty columns
    // implied by a wider DISPLAY.DW. Keep DISPLAY.DW as the fallback before
    // the first accepted primitive and whenever the active surface is wider.
    const bool display_is_primitive_surface = display_fbp == active_frame_fbp;
    if (width <= 0 || height <= 0 || dbx < 0 || dby < 0 || dbx + width > kWidth || dby + source_height > kHeight) {
        static bool warned = false;
        if (!warned) {
            warned = true;
            std::fprintf(stderr, "[native-gl] GS display rectangle is outside the direct scene target: x=%d y=%d w=%d h=%d field_h=%d\n",
                         dbx, dby, width, height, source_height);
        }
        bind_draw_target(active_frame_fbp);
        return false;
    }

    // The display buffer is scanned out of VRAM, so the pipeline that last
    // wrote that surface decides what the screen shows. A guest write (image
    // transfer or local copy) means the picture lives in VRAM and never
    // reached the primitive scene target: that is the movie and still-image
    // path. A primitive write means the scene target holds it. Anything else
    // (the display buffer only mirrors what the game rendered) keeps using
    // the scene target, which is what the front end needs.
    bool guest_owned = false;
    std::uint64_t surface_generation = 0;
    const bool owner_known = gs::surface_owner(display_fbp * 32u, guest_owned, surface_generation);
    const bool present_from_vram = owner_known && guest_owned;
    std::size_t write_count = 0;
    const gs::GuestWrite *writes = gs::guest_writes(write_count);
    const gs::GuestWrite *display_write = nullptr;
    for (std::size_t i = 0; i < write_count; ++i) {
        const gs::GuestWrite &write = writes[i];
        if (write.psm != display_psm || write.base != display_fbp * 32u) continue;
        const int write_x = static_cast<int>(write.x), write_y = static_cast<int>(write.y);
        if (write_x >= dbx + display_width || dbx >= write_x + static_cast<int>(write.w)) continue;
        if (write_y >= dby + source_height || dby >= write_y + static_cast<int>(write.h)) continue;
        if (!display_write || write.generation > display_write->generation) display_write = &write;
    }

    static int last_present_source = -1;
    static std::vector<std::uint32_t> vram_pixels;
    bool presented_from_vram = false;
    if (present_from_vram) {
        vram_pixels.resize(static_cast<std::size_t>(width) * source_height);
        if (gs::read_display_rgba(display_psm, display_fbp, display_fbw, static_cast<std::uint32_t>(dbx),
                                  static_cast<std::uint32_t>(dby), static_cast<std::uint32_t>(width),
                                  static_cast<std::uint32_t>(source_height), halved_field, vram_pixels.data()))
            presented_from_vram = true;
    }
    if (present_from_vram && !presented_from_vram) {
        static bool warned_vram = false;
        if (!warned_vram) {
            warned_vram = true;
            std::fprintf(stderr, "[native-gl] cannot present DISPFB from VRAM: pixel format %u is unsupported; using the scene target\n",
                         display_psm);
        }
    }

    const double fit = std::min(static_cast<double>(output_width) / width,
                                static_cast<double>(output_height) / height);
    // Never upscale by a fractional factor. 1.05x, 1.5x or 1.71x nearest
    // drops some source columns and doubles others, which reads as missing
    // vertical pixel columns and sliced glyphs and borders. An integer step
    // keeps every source column; below 1x the window is smaller than the
    // picture and the only honest choice is to shrink it.
    const double scale = fit >= 1.0 ? std::floor(fit) : fit;
    const int draw_width = std::max(1, static_cast<int>(std::lround(width * scale)));
    const int draw_height = std::max(1, static_cast<int>(std::lround(height * scale)));
    const int draw_x = (output_width - draw_width) / 2;
    const int draw_y = (output_height - draw_height) / 2;
    const int source_y0 = kHeight - dby - source_height;
    const int source_y1 = kHeight - dby;
    if (presented_from_vram) {
        draw_vram_display(vram_pixels.data(), width, source_height, output_width, output_height,
                          draw_x, draw_y, draw_width, draw_height);
    } else {
        g_blit_framebuffer(dbx, source_y0, dbx + width, source_y1,
                           draw_x, draw_y, draw_x + draw_width, draw_y + draw_height,
                           GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    const int present_source = presented_from_vram ? 1 : 0;
    static std::uint32_t last_present_fbp = 0xFFFFFFFFu;
    static std::uint32_t last_present_psm = 0xFFFFFFFFu;
    static int last_present_width = -1;
    if (present_source != last_present_source || display_fbp != last_present_fbp ||
        display_psm != last_present_psm || width != last_present_width) {
        last_present_source = present_source;
        last_present_fbp = display_fbp;
        last_present_psm = display_psm;
        last_present_width = width;
        std::fprintf(stderr,
                     "[native-gl] present source: %s %dx%d DISPFB FBP=%u PSM=%u FBW=%u DBX=%d DBY=%d, scene FBP=%u width=%d same_surface=%d owner=%s gen=%llu, write=%s, scale=%.3f (%s) -> %d,%d %dx%d\n",
                     presented_from_vram ? "vram" : "scene", width, source_height, display_fbp, display_psm,
                     display_fbw, dbx, dby, active_frame_fbp, active_frame_width,
                     display_is_primitive_surface ? 1 : 0,
                     owner_known ? (guest_owned ? "guest" : "primitive") : "unknown",
                     static_cast<unsigned long long>(surface_generation),
                     display_write ? "display" : "none", scale,
                     fit >= 1.0 ? "integer" : "fit", draw_x, draw_y, draw_width, draw_height);
        if (display_write)
            std::fprintf(stderr, "[native-gl] guest write on the display surface: psm=%u base=%u rect=%u,%u %ux%u generation=%llu\n",
                         display_write->psm, display_write->base, display_write->x, display_write->y,
                         display_write->w, display_write->h,
                         static_cast<unsigned long long>(display_write->generation));
    }
    static const bool diagnose_present = [] {
        const std::string value = ps2::os::env("PS2_NATIVE_GL_DIAGNOSTICS");
        return !value.empty() && value != "0" && value != "off" && value != "false";
    }();
    static const unsigned diagnostics_every = [] {
        const std::string value = ps2::os::env("PS2_NATIVE_GL_DIAGNOSTIC_EVERY");
        const long interval = std::strtol(value.c_str(), nullptr, 10);
        return interval > 0 ? static_cast<unsigned>(interval) : 0u;
    }();
    static const unsigned diagnostic_dump_present = [] {
        const std::string value = ps2::os::env("PS2_NATIVE_GL_DIAGNOSTIC_DUMP_PRESENT");
        const long present = std::strtol(value.c_str(), nullptr, 10);
        return present > 0 ? static_cast<unsigned>(present) : 0u;
    }();
    static const std::string diagnostic_dump_prefix = ps2::os::env("PS2_NATIVE_GL_DIAGNOSTIC_DUMP_PREFIX");
    static const unsigned diagnostic_dump_every = [] {
        const std::string value = ps2::os::env("PS2_NATIVE_GL_DIAGNOSTIC_DUMP_EVERY");
        const long interval = std::strtol(value.c_str(), nullptr, 10);
        return interval > 0 ? static_cast<unsigned>(interval) : 0u;
    }();
    static unsigned present_serial = 0;
    const unsigned present_index = ++present_serial;
    static unsigned diagnostic_frames = 0;
    const bool dump_this_present = diagnostic_dump_present != 0 &&
        diagnostic_dump_present == present_index && !diagnostic_dump_prefix.empty();
    // A rolling dump records the presented back buffer every N presents. It
    // shows what the window actually displays without touching the user's
    // screen, so a movie frame can be inspected without disturbing a run.
    const bool dump_rolling = diagnostic_dump_every != 0 && !diagnostic_dump_prefix.empty() &&
        present_index % diagnostic_dump_every == 0;
    const bool sample_present = dump_this_present || dump_rolling || (diagnose_present &&
        (diagnostics_every ? (present_index % diagnostics_every == 0) : diagnostic_frames < 8));
    if (sample_present) {
        const unsigned diagnostic_frame = ++diagnostic_frames;
        g_bind_framebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        const GLenum blit_error = glGetError();

        // A successful packet submission and valid display-register rectangle
        // do not prove that the source target contains pixels. Sample the whole
        // scene target, the selected GS crop, and the just-blitted back buffer.
        glReadPixels(0, 0, kWidth, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, g_readback.data());
        const GLenum scene_read_error = glGetError();
        std::uint64_t scene_nonblack = 0, crop_nonblack = 0, crop_gold = 0;
        int scene_x0 = kWidth, scene_y0 = kHeight, scene_x1 = -1, scene_y1 = -1;
        int crop_gold_x0 = kWidth, crop_gold_x1 = -1;
        for (int y = 0; y < kHeight; ++y) {
            for (int x = 0; x < kWidth; ++x) {
                const std::size_t at = (static_cast<std::size_t>(y) * kWidth + x) * 4;
                if ((g_readback[at] | g_readback[at + 1] | g_readback[at + 2]) == 0) continue;
                ++scene_nonblack;
                scene_x0 = std::min(scene_x0, x);
                scene_y0 = std::min(scene_y0, y);
                scene_x1 = std::max(scene_x1, x);
                scene_y1 = std::max(scene_y1, y);
                const bool inside_crop = x >= dbx && x < dbx + width && y >= source_y0 && y < source_y1;
                if (inside_crop)
                    ++crop_nonblack;
                const unsigned red = g_readback[at], green = g_readback[at + 1], blue = g_readback[at + 2];
                if (inside_crop && red > 90u && red * 100u > green * 112u && green * 100u > blue * 120u) {
                    ++crop_gold;
                    crop_gold_x0 = std::min(crop_gold_x0, x);
                    crop_gold_x1 = std::max(crop_gold_x1, x);
                }
            }
        }

        g_bind_framebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        std::vector<std::uint8_t> output_pixels(static_cast<std::size_t>(draw_width) * draw_height * 4);
        glReadPixels(draw_x, draw_y, draw_width, draw_height, GL_RGBA, GL_UNSIGNED_BYTE, output_pixels.data());
        const GLenum output_read_error = glGetError();
        std::uint64_t output_nonblack = 0, output_gold = 0;
        int output_gold_x0 = output_width, output_gold_x1 = -1;
        for (int y = 0; y < draw_height; ++y) {
            for (int x = 0; x < draw_width; ++x) {
                const std::size_t at = (static_cast<std::size_t>(y) * draw_width + x) * 4;
                const unsigned red = output_pixels[at], green = output_pixels[at + 1], blue = output_pixels[at + 2];
                if ((red | green | blue) != 0) ++output_nonblack;
                if (red > 90u && red * 100u > green * 112u && green * 100u > blue * 120u) {
                    ++output_gold;
                    output_gold_x0 = std::min(output_gold_x0, draw_x + x);
                    output_gold_x1 = std::max(output_gold_x1, draw_x + x);
                }
            }
        }

        if (dump_this_present) {
            const auto write_header = [](std::ofstream &file, int width_px, int height_px) {
                file << "P6\n" << width_px << " " << height_px << "\n255\n";
                return static_cast<bool>(file);
            };
            const std::string scene_path = diagnostic_dump_prefix + "_scene.ppm";
            const std::string crop_path = diagnostic_dump_prefix + "_crop.ppm";
            const std::string output_path = diagnostic_dump_prefix + "_present.ppm";
            bool saved = true;
            {
                std::ofstream file(scene_path, std::ios::binary);
                saved = saved && write_header(file, kWidth, kHeight);
                for (int y = kHeight - 1; saved && y >= 0; --y) {
                    for (int x = 0; x < kWidth; ++x) {
                        const std::size_t at = (static_cast<std::size_t>(y) * kWidth + x) * 4;
                        const char rgb[] = {static_cast<char>(g_readback[at]), static_cast<char>(g_readback[at + 1]),
                                            static_cast<char>(g_readback[at + 2])};
                        file.write(rgb, sizeof(rgb));
                    }
                    saved = static_cast<bool>(file);
                }
            }
            {
                std::ofstream file(crop_path, std::ios::binary);
                saved = saved && write_header(file, width, source_height);
                for (int y = source_y1 - 1; saved && y >= source_y0; --y) {
                    for (int x = dbx; x < dbx + width; ++x) {
                        const std::size_t at = (static_cast<std::size_t>(y) * kWidth + x) * 4;
                        const char rgb[] = {static_cast<char>(g_readback[at]), static_cast<char>(g_readback[at + 1]),
                                            static_cast<char>(g_readback[at + 2])};
                        file.write(rgb, sizeof(rgb));
                    }
                    saved = static_cast<bool>(file);
                }
            }
            {
                std::ofstream file(output_path, std::ios::binary);
                saved = saved && write_header(file, draw_width, draw_height);
                for (int y = draw_height - 1; saved && y >= 0; --y) {
                    for (int x = 0; x < draw_width; ++x) {
                        const std::size_t at = (static_cast<std::size_t>(y) * draw_width + x) * 4;
                        const char rgb[] = {static_cast<char>(output_pixels[at]), static_cast<char>(output_pixels[at + 1]),
                                            static_cast<char>(output_pixels[at + 2])};
                        file.write(rgb, sizeof(rgb));
                    }
                    saved = static_cast<bool>(file);
                }
            }
            std::fprintf(stderr, "[native-gl-diag] present=%u PPM dump %s (%s, %s, %s)\n",
                         present_index, saved ? "saved" : "failed", scene_path.c_str(), crop_path.c_str(), output_path.c_str());
        }

        if (dump_rolling) {
            const std::string path = diagnostic_dump_prefix + "_present_" + std::to_string(present_index) + ".ppm";
            std::ofstream file(path, std::ios::binary);
            file << "P6\n" << draw_width << " " << draw_height << "\n255\n";
            for (int y = draw_height - 1; file && y >= 0; --y) {
                for (int x = 0; x < draw_width; ++x) {
                    const std::size_t at = (static_cast<std::size_t>(y) * draw_width + x) * 4;
                    const char rgb[] = {static_cast<char>(output_pixels[at]), static_cast<char>(output_pixels[at + 1]),
                                        static_cast<char>(output_pixels[at + 2])};
                    file.write(rgb, sizeof rgb);
                }
            }
            std::fprintf(stderr, "[native-gl-diag] present=%u rolling PPM dump %s\n", present_index, path.c_str());
        }

        const std::uint32_t accepted = static_cast<std::uint32_t>(g_prims_drawn.load(std::memory_order_relaxed));
        const std::uint64_t frame0 = g_state.ctx[0].frame, frame1 = g_state.ctx[1].frame;
        const std::uint64_t xyoffset0 = g_state.ctx[0].xyoffset, xyoffset1 = g_state.ctx[1].xyoffset;
        const std::uint64_t scissor0 = g_state.ctx[0].scissor, scissor1 = g_state.ctx[1].scissor;
        std::fprintf(stderr,
                     "[native-gl-diag] present=%u sample=%u accepted_primitives=%u pmode=%016llX smode2=%016llX dispfb=%016llX display=%016llX frame0=%016llX frame1=%016llX xyoffset0=%016llX xyoffset1=%016llX scissor0=%016llX scissor1=%016llX dx=%u dy=%u crop=%d,%d %dx%d dest=%d,%d %dx%d scene_nonblack=%llu crop_nonblack=%llu scene_bounds=%d,%d-%d,%d crop_gold=%llu crop_gold_x=%d..%d output_nonblack=%llu output_gold=%llu output_gold_x=%d..%d gl_errors=%04X/%04X/%04X\n",
                     present_index, diagnostic_frame, accepted, static_cast<unsigned long long>(regs.pmode),
                     static_cast<unsigned long long>(regs.smode2), static_cast<unsigned long long>(fb),
                     static_cast<unsigned long long>(disp), static_cast<unsigned long long>(frame0),
                     static_cast<unsigned long long>(frame1), static_cast<unsigned long long>(xyoffset0),
                     static_cast<unsigned long long>(xyoffset1), static_cast<unsigned long long>(scissor0),
                     static_cast<unsigned long long>(scissor1), static_cast<unsigned>(disp & 0xFFFu),
                     static_cast<unsigned>((disp >> 12) & 0x7FFu), dbx, dby, width, source_height,
                     draw_x, draw_y, draw_width, draw_height,
                     static_cast<unsigned long long>(scene_nonblack), static_cast<unsigned long long>(crop_nonblack),
                     scene_x0, scene_y0, scene_x1, scene_y1,
                     static_cast<unsigned long long>(crop_gold), crop_gold_x0, crop_gold_x1,
                     static_cast<unsigned long long>(output_nonblack), static_cast<unsigned long long>(output_gold),
                     output_gold_x0, output_gold_x1, static_cast<unsigned>(blit_error),
                     static_cast<unsigned>(scene_read_error), static_cast<unsigned>(output_read_error));
    }
    static bool reported_direct_rect = false;
    if (!reported_direct_rect) {
        reported_direct_rect = true;
        std::fprintf(stderr, "[native-gl] direct present crop x=%d y=%d %dx%d (DISPLAY width=%d, latest FRAME width=%d FBP=%u, DISPFB FBP=%u) -> client rect %d,%d %dx%d (nearest)\n",
                     dbx, dby, width, source_height, display_width, active_frame_width,
                     active_frame_fbp, display_fbp, draw_x, draw_y, draw_width, draw_height);
    }

    bind_draw_target(active_frame_fbp);
    glFlush();
    return true;
#else
    (void)regs;
    return false;
#endif
}

bool present_current() {
    return present_frame(gs::display_regs());
}

bool read_picture(std::vector<std::uint32_t> &rgba, std::uint32_t &width, std::uint32_t &height) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc || !g_hrc) return false;
    const DisplayRegs regs = gs::display_regs();
    const int circuit = (regs.pmode & 2u) ? 1 : (regs.pmode & 1u) ? 0 : -1;
    if (circuit < 0) return false;
    const std::uint64_t fb = regs.dispfb[circuit], disp = regs.display[circuit];
    const std::uint32_t display_fbp = static_cast<std::uint32_t>(fb) & 0x1FFu;
    const std::uint32_t active_frame_fbp = g_active_frame_fbp.load(std::memory_order_relaxed);
    const PresentTarget selection = select_present_target(display_fbp, active_frame_fbp);
    SceneTarget *target = selection.target;
    if (!target || !target->ready) return false;
    SceneTarget *active_target = find_scene_target(active_frame_fbp);
    SceneTarget *display_target = find_scene_target(display_fbp);
    const bool is_active_target = (target == active_target && target != display_target);
    const DisplayRect rect = display_rect(regs, circuit, fb, disp, is_active_target);
    if (!rect.valid) return false;
    // Which surface the capture reads, and how much the game has drawn into it.
    // Without this the capture cannot be told apart from a stale frame.
    {
        std::fprintf(stderr,
                     "[native-gl] picture capture: display_fbp=%u active_fbp=%u target_fbp=%u prims=%llu writes=%llu tick=%llu rect=%d,%d %dx%d\n",
                     display_fbp, active_frame_fbp, target->fbp,
                     static_cast<unsigned long long>(target->prim_count),
                     static_cast<unsigned long long>(target->write_count),
                     static_cast<unsigned long long>(target->last_write_tick),
                     rect.dbx, rect.dby, rect.width, rect.source_height);
    }
    if (wglGetCurrentContext() != g_hrc && !wglMakeCurrent(g_hdc, g_hrc)) {
        std::fprintf(stderr, "[native-gl] could not make the WGL context current for the picture capture\n");
        return false;
    }
    // The picture comes from the same framebuffer the window blits, so the
    // capture shows what the window shows. The native path never writes its
    // pixels into the CPU VRAM array: reading that array instead showed the
    // game's texture data, not the game. GL reads bottom-up and the GS scans
    // out top-down, so the rows are flipped while they are copied.
    const int source_y0 = kHeight - rect.dby - rect.source_height;
    static std::vector<std::uint8_t> raw;
    raw.resize(static_cast<std::size_t>(rect.width) * rect.source_height * 4);
    while (glGetError() != GL_NO_ERROR) {}
    g_bind_framebuffer(GL_READ_FRAMEBUFFER, target->framebuffer);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(rect.dbx, source_y0, rect.width, rect.source_height, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());
    const GLenum read_error = glGetError();
    if (read_error != GL_NO_ERROR) {
        std::fprintf(stderr, "[native-gl] picture capture failed: glReadPixels error 0x%X\n", read_error);
        return false;
    }
    rgba.resize(static_cast<std::size_t>(rect.width) * rect.source_height);
    for (int y = 0; y < rect.source_height; ++y) {
        const std::uint8_t *src = raw.data() + static_cast<std::size_t>(rect.source_height - 1 - y) * rect.width * 4;
        std::uint32_t *dst = rgba.data() + static_cast<std::size_t>(y) * rect.width;
        for (int x = 0; x < rect.width; ++x) {
            const std::uint8_t *p = src + static_cast<std::size_t>(x) * 4;
            dst[x] = static_cast<std::uint32_t>(p[3]) << 24 | static_cast<std::uint32_t>(p[2]) << 16 |
                     static_cast<std::uint32_t>(p[1]) << 8 | static_cast<std::uint32_t>(p[0]);
        }
    }

    for (std::size_t i = 0; i < kMaxSceneTargets; ++i) {
        if (!g_scene_targets[i].ready || g_scene_targets[i].fbp == 0xFFFFFFFFu) continue;
        std::fprintf(stderr, "[scene-target-census #%zu] fbp=%u prims=%llu writes=%llu tick=%llu\n",
                     i, g_scene_targets[i].fbp,
                     static_cast<unsigned long long>(g_scene_targets[i].prim_count),
                     static_cast<unsigned long long>(g_scene_targets[i].write_count),
                     static_cast<unsigned long long>(g_scene_targets[i].last_write_tick));
        std::vector<std::uint8_t> dump_raw(640 * 448 * 4);
        g_bind_framebuffer(GL_READ_FRAMEBUFFER, g_scene_targets[i].framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, kHeight - 448, 640, 448, GL_RGBA, GL_UNSIGNED_BYTE, dump_raw.data());
        char fname[256];
        std::snprintf(fname, sizeof(fname), "C:/Users/nahsh/mk port/Armageddon-backup/armageddon-reimplementation/external/forks/ps2-native-recomp/build_native_gl_full/dump_target_%u.ppm", g_scene_targets[i].fbp);
        std::ofstream f(fname, std::ios::binary);
        if (f.good()) {
            f << "P6\n640 448\n255\n";
            for (int y = 0; y < 448; ++y) {
                const std::uint8_t *row = dump_raw.data() + static_cast<std::size_t>(448 - 1 - y) * 640 * 4;
                for (int x = 0; x < 640; ++x) {
                    f.write(reinterpret_cast<const char*>(row + x * 4), 3);
                }
            }
        }
    }
    width = static_cast<std::uint32_t>(rect.width);
    height = static_cast<std::uint32_t>(rect.source_height);
    return true;
#else
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
    if (g_direct_window_context) {
        bind_draw_target(active_fbp);
        if (g_evicted_color[active_fbp]) {
            // Restore exact stored RGBA for the supported 32-bit path. Other
            // formats require their own alpha conversion and remain rejected.
            if (fpsm != gs::PSMCT32)
                return reject_packet(st, attr, prim_type, count, 25, "evicted framebuffer format cannot be restored yet");
            SceneTarget *target = find_scene_target(active_fbp);
            if (!target) return reject_packet(st, attr, prim_type, count, 25, "evicted framebuffer target unavailable");
            const int width = std::min(kWidth, static_cast<int>(fbw * 64u));
            std::vector<std::uint32_t> rgba(static_cast<std::size_t>(width) * kHeight);
            for (int y = 0; y < kHeight; ++y)
                for (int x = 0; x < width; ++x)
                    rgba[static_cast<std::size_t>(kHeight - 1 - y) * width + x] =
                        gs::read_pixel(fpsm, active_fbp * 32u, fbw, x, y);
            GLint previous_texture = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
            glBindTexture(GL_TEXTURE_2D, target->color_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, kHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previous_texture));
            g_evicted_color[active_fbp] = false;
        }
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
        bind_draw_target(active_fbp);
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
        bind_draw_target(active_fbp);
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
    std::fill(std::begin(g_evicted_color), std::end(g_evicted_color), false);
#endif
    g_enabled = false;
    g_checked = false;
}

} // namespace ps2::gs::native_gl
