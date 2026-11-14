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
#include "gpu/gpu.hpp"
#include "raster.hpp"

#include "ps2/os.hpp"

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
#include <fstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
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
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
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

#if defined(_WIN32)
HDC    g_hdc = nullptr;
HGLRC  g_hrc = nullptr;
HWND   g_hwnd = nullptr;
bool   g_owns_context = false;
bool   g_direct_window_context = false;

struct TextureCache {
    GLuint object = 0;
    bool valid = false;
    std::uint64_t tex0 = 0, tex1 = 0, tex2 = 0, clamp = 0, texa = 0;
    std::uint64_t clut_hash = 0, vram_generation = 0;
    std::uint32_t width = 0, height = 0;
};
TextureCache g_texture;

using ActiveTextureProc = void (APIENTRY *)(GLenum texture);
using MultiTexCoord4fProc = void (APIENTRY *)(GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
using BlendFuncSeparateProc = void (APIENTRY *)(GLenum src_rgb, GLenum dst_rgb, GLenum src_alpha, GLenum dst_alpha);
ActiveTextureProc g_active_texture = nullptr;
MultiTexCoord4fProc g_multi_texcoord4f = nullptr;
BlendFuncSeparateProc g_blend_func_separate = nullptr;
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
using CreateShaderProc = GLuint (APIENTRY *)(GLenum);
using ShaderSourceProc = void (APIENTRY *)(GLuint, GLsizei, const char *const *, const GLint *);
using CompileShaderProc = void (APIENTRY *)(GLuint);
using GetShaderivProc = void (APIENTRY *)(GLuint, GLenum, GLint *);
using GetShaderInfoLogProc = void (APIENTRY *)(GLuint, GLsizei, GLsizei *, char *);
using DeleteShaderProc = void (APIENTRY *)(GLuint);
using CreateProgramProc = GLuint (APIENTRY *)();
using AttachShaderProc = void (APIENTRY *)(GLuint, GLuint);
using LinkProgramProc = void (APIENTRY *)(GLuint);
using GetProgramivProc = void (APIENTRY *)(GLuint, GLenum, GLint *);
using GetProgramInfoLogProc = void (APIENTRY *)(GLuint, GLsizei, GLsizei *, char *);
using DeleteProgramProc = void (APIENTRY *)(GLuint);
using UseProgramProc = void (APIENTRY *)(GLuint);
using GetUniformLocationProc = GLint (APIENTRY *)(GLuint, const char *);
using Uniform1iProc = void (APIENTRY *)(GLint, GLint);

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
CreateShaderProc g_create_shader = nullptr;
ShaderSourceProc g_shader_source = nullptr;
CompileShaderProc g_compile_shader = nullptr;
GetShaderivProc g_get_shader_iv = nullptr;
GetShaderInfoLogProc g_get_shader_info_log = nullptr;
DeleteShaderProc g_delete_shader = nullptr;
CreateProgramProc g_create_program = nullptr;
AttachShaderProc g_attach_shader = nullptr;
LinkProgramProc g_link_program = nullptr;
GetProgramivProc g_get_program_iv = nullptr;
GetProgramInfoLogProc g_get_program_info_log = nullptr;
DeleteProgramProc g_delete_program = nullptr;
UseProgramProc g_use_program = nullptr;
GetUniformLocationProc g_get_uniform_location = nullptr;
Uniform1iProc g_uniform_1i = nullptr;

struct AlphaCompositor {
    GLuint framebuffer = 0;
    GLuint source_texture = 0;
    GLuint destination_texture = 0;
    GLuint depth_buffer = 0;
    GLuint program = 0;
    GLint source_uniform = -1;
    GLint destination_uniform = -1;
    GLint saved_active_texture = GL_TEXTURE0;
    GLint saved_texture0_binding = 0;
    GLint saved_texture1_binding = 0;
    GLboolean saved_texture0_enabled = GL_FALSE;
    GLboolean saved_texture1_enabled = GL_FALSE;
    bool checked = false;
    bool ready = false;
};
AlphaCompositor g_alpha_compositor;

struct SceneTarget {
    GLuint framebuffer = 0;
    GLuint color_texture = 0;
    GLuint depth_buffer = 0;
    bool ready = false;
};
SceneTarget g_scene_target;

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
}

template <typename T>
T load_gl_proc(const char *name, const char *extension_name = nullptr) {
    PROC proc = wglGetProcAddress(name);
    if (!valid_gl_proc(proc) && extension_name) proc = wglGetProcAddress(extension_name);
    return valid_gl_proc(proc) ? reinterpret_cast<T>(proc) : nullptr;
}

void load_alpha_compositor_functions() {
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

    g_create_shader = load_gl_proc<CreateShaderProc>("glCreateShader");
    g_shader_source = load_gl_proc<ShaderSourceProc>("glShaderSource");
    g_compile_shader = load_gl_proc<CompileShaderProc>("glCompileShader");
    g_get_shader_iv = load_gl_proc<GetShaderivProc>("glGetShaderiv");
    g_get_shader_info_log = load_gl_proc<GetShaderInfoLogProc>("glGetShaderInfoLog");
    g_delete_shader = load_gl_proc<DeleteShaderProc>("glDeleteShader");
    g_create_program = load_gl_proc<CreateProgramProc>("glCreateProgram");
    g_attach_shader = load_gl_proc<AttachShaderProc>("glAttachShader");
    g_link_program = load_gl_proc<LinkProgramProc>("glLinkProgram");
    g_get_program_iv = load_gl_proc<GetProgramivProc>("glGetProgramiv");
    g_get_program_info_log = load_gl_proc<GetProgramInfoLogProc>("glGetProgramInfoLog");
    g_delete_program = load_gl_proc<DeleteProgramProc>("glDeleteProgram");
    g_use_program = load_gl_proc<UseProgramProc>("glUseProgram");
    g_get_uniform_location = load_gl_proc<GetUniformLocationProc>("glGetUniformLocation");
    g_uniform_1i = load_gl_proc<Uniform1iProc>("glUniform1i");
}

void release_scene_target() {
    if (g_scene_target.framebuffer && g_delete_framebuffers)
        g_delete_framebuffers(1, &g_scene_target.framebuffer);
    if (g_scene_target.depth_buffer && g_delete_renderbuffers)
        g_delete_renderbuffers(1, &g_scene_target.depth_buffer);
    if (g_scene_target.color_texture) glDeleteTextures(1, &g_scene_target.color_texture);
    g_scene_target = SceneTarget{};
}

bool ensure_scene_target() {
    if (g_scene_target.ready) return true;
    load_alpha_compositor_functions();
    if (!g_gen_framebuffers || !g_bind_framebuffer || !g_framebuffer_texture_2d ||
        !g_check_framebuffer_status || !g_delete_framebuffers || !g_gen_renderbuffers ||
        !g_bind_renderbuffer || !g_renderbuffer_storage || !g_framebuffer_renderbuffer ||
        !g_delete_renderbuffers) {
        std::fprintf(stderr, "[native-gl] direct presentation requires OpenGL framebuffer objects\n");
        return false;
    }

    glGenTextures(1, &g_scene_target.color_texture);
    g_gen_framebuffers(1, &g_scene_target.framebuffer);
    g_gen_renderbuffers(1, &g_scene_target.depth_buffer);
    if (!g_scene_target.color_texture || !g_scene_target.framebuffer || !g_scene_target.depth_buffer) {
        std::fprintf(stderr, "[native-gl] direct scene target allocation failed\n");
        release_scene_target();
        return false;
    }

    glBindTexture(GL_TEXTURE_2D, g_scene_target.color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    g_bind_framebuffer(GL_FRAMEBUFFER, g_scene_target.framebuffer);
    g_framebuffer_texture_2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             g_scene_target.color_texture, 0);
    g_bind_renderbuffer(GL_RENDERBUFFER, g_scene_target.depth_buffer);
    g_renderbuffer_storage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kWidth, kHeight);
    g_framebuffer_renderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                               g_scene_target.depth_buffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum status = g_check_framebuffer_status(GL_FRAMEBUFFER);
    glBindTexture(GL_TEXTURE_2D, 0);
    g_bind_renderbuffer(GL_RENDERBUFFER, 0);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[native-gl] direct scene framebuffer is incomplete (0x%04X)\n",
                     static_cast<unsigned>(status));
        release_scene_target();
        g_bind_framebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }
    g_scene_target.ready = true;
    glViewport(0, 0, kWidth, kHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    std::fprintf(stderr, "[native-gl] direct GS scene target: %dx%d color + depth FBO\n", kWidth, kHeight);
    return true;
}

GLuint draw_target_framebuffer() {
    return g_direct_window_context ? g_scene_target.framebuffer : 0;
}

void release_alpha_compositor() {
    if (g_alpha_compositor.program && g_delete_program) g_delete_program(g_alpha_compositor.program);
    if (g_alpha_compositor.framebuffer && g_delete_framebuffers)
        g_delete_framebuffers(1, &g_alpha_compositor.framebuffer);
    if (g_alpha_compositor.depth_buffer && g_delete_renderbuffers)
        g_delete_renderbuffers(1, &g_alpha_compositor.depth_buffer);
    const GLuint textures[] = {g_alpha_compositor.source_texture, g_alpha_compositor.destination_texture};
    if (textures[0] || textures[1]) {
        glDeleteTextures(2, textures);
    }
    g_alpha_compositor = AlphaCompositor{};
}

bool ensure_alpha_compositor() {
    if (g_alpha_compositor.checked) return g_alpha_compositor.ready;
    g_alpha_compositor.checked = true;
    load_alpha_compositor_functions();
    if (!g_multitexture_supported || !g_active_texture || !g_gen_framebuffers || !g_bind_framebuffer ||
        !g_framebuffer_texture_2d || !g_check_framebuffer_status || !g_gen_renderbuffers ||
        !g_bind_renderbuffer || !g_renderbuffer_storage || !g_framebuffer_renderbuffer ||
        !g_delete_framebuffers || !g_delete_renderbuffers || !g_blit_framebuffer ||
        !g_create_shader || !g_shader_source || !g_compile_shader || !g_get_shader_iv ||
        !g_get_shader_info_log || !g_delete_shader || !g_create_program || !g_attach_shader || !g_link_program ||
        !g_get_program_iv || !g_get_program_info_log || !g_delete_program || !g_use_program ||
        !g_get_uniform_location || !g_uniform_1i) {
        std::fprintf(stderr, "[native-gl] GS alpha compositor unavailable: required OpenGL 2.0/FBO functions are missing\n");
        return false;
    }

    glGenTextures(1, &g_alpha_compositor.source_texture);
    glGenTextures(1, &g_alpha_compositor.destination_texture);
    g_gen_framebuffers(1, &g_alpha_compositor.framebuffer);
    g_gen_renderbuffers(1, &g_alpha_compositor.depth_buffer);
    if (!g_alpha_compositor.source_texture || !g_alpha_compositor.destination_texture ||
        !g_alpha_compositor.framebuffer || !g_alpha_compositor.depth_buffer) {
        std::fprintf(stderr, "[native-gl] GS alpha compositor could not allocate framebuffer resources\n");
        return false;
    }

    const auto configure_texture = [](GLuint texture) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kWidth, kHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    };
    configure_texture(g_alpha_compositor.source_texture);
    configure_texture(g_alpha_compositor.destination_texture);

    GLint previous_framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    g_bind_framebuffer(GL_FRAMEBUFFER, g_alpha_compositor.framebuffer);
    g_framebuffer_texture_2d(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                             g_alpha_compositor.source_texture, 0);
    g_bind_renderbuffer(GL_RENDERBUFFER, g_alpha_compositor.depth_buffer);
    g_renderbuffer_storage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kWidth, kHeight);
    g_framebuffer_renderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                               g_alpha_compositor.depth_buffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    const GLenum framebuffer_status = g_check_framebuffer_status(GL_FRAMEBUFFER);
    g_bind_framebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous_framebuffer));
    if (framebuffer_status != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[native-gl] GS alpha compositor framebuffer is incomplete (0x%04X)\n",
                     static_cast<unsigned>(framebuffer_status));
        return false;
    }

    static constexpr char kFragmentShader[] =
        "#version 120\n"
        "uniform sampler2D uSource;\n"
        "uniform sampler2D uDestination;\n"
        "void main() {\n"
        "  vec2 uv = gl_TexCoord[0].st;\n"
        "  vec4 src = texture2D(uSource, uv);\n"
        "  vec4 dst = texture2D(uDestination, uv);\n"
        "  float factor = src.a * (255.0 / 128.0);\n"
        "  gl_FragColor = clamp(dst + (src - dst) * factor, 0.0, 1.0);\n"
        "}\n";
    const GLuint shader = g_create_shader(GL_FRAGMENT_SHADER);
    if (!shader) {
        std::fprintf(stderr, "[native-gl] GS alpha compositor could not create its fragment shader\n");
        return false;
    }
    const char *shader_source = kFragmentShader;
    g_shader_source(shader, 1, &shader_source, nullptr);
    g_compile_shader(shader);
    GLint compiled = 0;
    g_get_shader_iv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        GLint log_length = 0;
        g_get_shader_iv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
        GLsizei written = 0;
        g_get_shader_info_log(shader, static_cast<GLsizei>(log.size()), &written, log.data());
        std::fprintf(stderr, "[native-gl] GS alpha compositor shader compile failed: %s\n", log.data());
        if (g_delete_shader) g_delete_shader(shader);
        return false;
    }

    g_alpha_compositor.program = g_create_program();
    if (!g_alpha_compositor.program) {
        g_delete_shader(shader);
        std::fprintf(stderr, "[native-gl] GS alpha compositor could not create its program\n");
        return false;
    }
    g_attach_shader(g_alpha_compositor.program, shader);
    g_link_program(g_alpha_compositor.program);
    if (g_delete_shader) g_delete_shader(shader);
    GLint linked = 0;
    g_get_program_iv(g_alpha_compositor.program, GL_LINK_STATUS, &linked);
    if (!linked) {
        GLint log_length = 0;
        g_get_program_iv(g_alpha_compositor.program, GL_INFO_LOG_LENGTH, &log_length);
        std::vector<char> log(static_cast<std::size_t>(std::max(log_length, 1)), '\0');
        GLsizei written = 0;
        g_get_program_info_log(g_alpha_compositor.program, static_cast<GLsizei>(log.size()), &written, log.data());
        std::fprintf(stderr, "[native-gl] GS alpha compositor shader link failed: %s\n", log.data());
        return false;
    }

    g_alpha_compositor.source_uniform = g_get_uniform_location(g_alpha_compositor.program, "uSource");
    g_alpha_compositor.destination_uniform = g_get_uniform_location(g_alpha_compositor.program, "uDestination");
    if (g_alpha_compositor.source_uniform < 0 || g_alpha_compositor.destination_uniform < 0) {
        std::fprintf(stderr, "[native-gl] GS alpha compositor sampler uniforms were not found\n");
        return false;
    }
    g_use_program(g_alpha_compositor.program);
    g_uniform_1i(g_alpha_compositor.source_uniform, 0);
    g_uniform_1i(g_alpha_compositor.destination_uniform, 1);
    g_use_program(0);
    g_alpha_compositor.ready = true;
    std::fprintf(stderr, "[native-gl] GS /128 alpha compositor initialized (GLSL + framebuffer objects)\n");
    return true;
}

bool begin_alpha_composite(bool textured, int scissor_left, int scissor_top,
                           int scissor_right, int scissor_bottom) {
    if (!ensure_alpha_compositor()) return false;

    // The texture uploader caches its GL bindings. Save both units here and
    // restore them after the fullscreen composite so that cache and driver
    // state remain in sync for the next primitive.
    glGetIntegerv(GL_ACTIVE_TEXTURE, &g_alpha_compositor.saved_active_texture);
    g_active_texture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &g_alpha_compositor.saved_texture0_binding);
    g_alpha_compositor.saved_texture0_enabled = glIsEnabled(GL_TEXTURE_2D);
    g_active_texture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &g_alpha_compositor.saved_texture1_binding);
    g_alpha_compositor.saved_texture1_enabled = glIsEnabled(GL_TEXTURE_2D);
    g_active_texture(GL_TEXTURE0);

    const GLuint target_framebuffer = draw_target_framebuffer();
    g_bind_framebuffer(GL_READ_FRAMEBUFFER, target_framebuffer);
    glReadBuffer(g_direct_window_context ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glBindTexture(GL_TEXTURE_2D, g_alpha_compositor.destination_texture);
    glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0, kWidth, kHeight, 0);

    // The source pass must retain the GS sampling setup created by
    // prepare_texture(), so restore the original binding after the snapshot.
    if (textured) glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(g_alpha_compositor.saved_texture0_binding));

    g_bind_framebuffer(GL_READ_FRAMEBUFFER, target_framebuffer);
    g_bind_framebuffer(GL_DRAW_FRAMEBUFFER, g_alpha_compositor.framebuffer);
    g_blit_framebuffer(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight,
                       GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    g_bind_framebuffer(GL_FRAMEBUFFER, g_alpha_compositor.framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, kWidth, kHeight);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glEnable(GL_SCISSOR_TEST);
    glScissor(scissor_left, kHeight - scissor_bottom,
              std::max(0, scissor_right - scissor_left),
              std::max(0, scissor_bottom - scissor_top));
    glDisable(GL_BLEND);
    return true;
}

void finish_alpha_composite() {
    // Copy depth-test/write results from the source pass back to the normal
    // target before drawing the color-only full-screen composition.
    g_bind_framebuffer(GL_READ_FRAMEBUFFER, g_alpha_compositor.framebuffer);
    g_bind_framebuffer(GL_DRAW_FRAMEBUFFER, draw_target_framebuffer());
    g_blit_framebuffer(0, 0, kWidth, kHeight, 0, 0, kWidth, kHeight,
                       GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    g_bind_framebuffer(GL_FRAMEBUFFER, draw_target_framebuffer());
    glReadBuffer(g_direct_window_context ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glDrawBuffer(g_direct_window_context ? GL_COLOR_ATTACHMENT0 : GL_BACK);
    glViewport(0, 0, kWidth, kHeight);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);

    g_active_texture(GL_TEXTURE0);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_alpha_compositor.source_texture);
    g_active_texture(GL_TEXTURE1);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g_alpha_compositor.destination_texture);
    g_use_program(g_alpha_compositor.program);
    g_active_texture(GL_TEXTURE0);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(0.0f, 0.0f);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(static_cast<GLfloat>(kWidth), 0.0f);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(static_cast<GLfloat>(kWidth), static_cast<GLfloat>(kHeight));
    glTexCoord2f(0.0f, 0.0f); glVertex2f(0.0f, static_cast<GLfloat>(kHeight));
    glEnd();
    g_use_program(0);
    g_active_texture(GL_TEXTURE1);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(g_alpha_compositor.saved_texture1_binding));
    g_active_texture(GL_TEXTURE0);
    glDisable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(g_alpha_compositor.saved_texture0_binding));
    if (g_alpha_compositor.saved_texture0_enabled) glEnable(GL_TEXTURE_2D);
    if (g_alpha_compositor.saved_texture1_enabled) {
        g_active_texture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
    }
    g_active_texture(static_cast<GLenum>(g_alpha_compositor.saved_active_texture));
    glDepthMask(GL_TRUE);
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
    const std::uint32_t wms = static_cast<std::uint32_t>(context.clamp) & 3u;
    const std::uint32_t wmt = (static_cast<std::uint32_t>(context.clamp) >> 2) & 3u;
    const std::uint32_t tfx = static_cast<std::uint32_t>((context.tex0 >> 35) & 3u);
    if (mmin > 1)
        return reject_packet(st, attr, prim_type, count, width_reason, "mipmapped GS filtering is not implemented yet");
    if (wms == 3 || wmt == 3)
        return reject_packet(st, attr, prim_type, count, width_reason + 1, "GS region repeat is not implemented yet");
    const bool tcc = (context.tex0 & (1ull << 34)) != 0;
    if (tfx >= 2 && (!g_multitexture_supported || !g_active_texture || !g_multi_texcoord4f))
        return reject_packet(st, attr, prim_type, count, width_reason + 2, "GS highlight texture functions require two OpenGL texture units");

    if (g_active_texture) g_active_texture(GL_TEXTURE0);
    const std::uint64_t palette = clut_hash(st);
    if (!g_texture.object) glGenTextures(1, &g_texture.object);
    if (!g_texture.object)
        return reject_packet(st, attr, prim_type, count, width_reason + 3, "OpenGL could not allocate a texture object");

    const bool changed = !g_texture.valid || g_texture.tex0 != context.tex0 || g_texture.tex1 != context.tex1 ||
                         g_texture.tex2 != context.tex2 || g_texture.clamp != context.clamp || g_texture.texa != st.texa ||
                         g_texture.clut_hash != palette || g_texture.vram_generation != gs::g_vram_generation;
    glBindTexture(GL_TEXTURE_2D, g_texture.object);
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
            std::fprintf(stderr,
                         "[native-gl-texture] primitive=%llu dump=%u %s TEX0=%016llX TEX1=%016llX PSM=%u size=%ux%u MMIN=%u MMAG=%u TFX=%u TCC=%u saved=%d\n",
                         static_cast<unsigned long long>(primitive_index), serial, path.c_str(),
                         static_cast<unsigned long long>(context.tex0),
                         static_cast<unsigned long long>(context.tex1), tpsm, width, height,
                         mmin, mmag, tfx, static_cast<unsigned>((context.tex0 >> 34) & 1u), file.good() ? 1 : 0);
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
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        g_texture.tex0 = context.tex0;
        g_texture.tex1 = context.tex1;
        g_texture.tex2 = context.tex2;
        g_texture.clamp = context.clamp;
        g_texture.texa = st.texa;
        g_texture.clut_hash = palette;
        g_texture.vram_generation = gs::g_vram_generation;
        g_texture.width = width;
        g_texture.height = height;
        g_texture.valid = true;
    } else {
        width = g_texture.width;
        height = g_texture.height;
    }

    const GLint min_filter = mmin == 1 ? GL_LINEAR : GL_NEAREST;
    const GLint mag_filter = ((tex1 >> 5) & 1u) ? GL_LINEAR : GL_NEAREST;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wms == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wmt == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);

    // TEX1 MMIN 0/1 selects non-mipmapped nearest/linear filtering; MXL matters
    // only when a mipmapped MMIN mode selects another level. TFX 2/3 use two
    // fixed-function texture units to add the vertex-alpha highlight term.
    if (g_active_texture) {
        g_active_texture(GL_TEXTURE1);
        if (tfx >= 2) glEnable(GL_TEXTURE_2D);
        else glDisable(GL_TEXTURE_2D);
        g_active_texture(GL_TEXTURE0);
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
            glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 2);
        } else {
            glTexEnvi(GL_TEXTURE_ENV, GL_COMBINE_ALPHA, GL_REPLACE);
            glTexEnvi(GL_TEXTURE_ENV, GL_SOURCE0_ALPHA, GL_PRIMARY_COLOR);
            glTexEnvi(GL_TEXTURE_ENV, GL_ALPHA_SCALE, 1);
        }
    } else if (tfx == 1) {
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

        g_active_texture(GL_TEXTURE1);
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, g_texture.object);
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
        g_active_texture(GL_TEXTURE0);
    }
    glEnable(GL_TEXTURE_2D);
    return true;
}

void initialize_context_state() {
    load_multitexture_functions();
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

    g_hwnd = CreateWindowExW(0, kClass, L"", WS_POPUP | WS_DISABLED,
                             0, 0, kWidth, kHeight,
                             nullptr, nullptr, hi, nullptr);
    if (!g_hwnd) {
        std::fprintf(stderr, "[native-gl] CreateWindowExW failed (%lu)\n", GetLastError());
        return false;
    }

    g_hdc = GetDC(g_hwnd);
    if (!g_hdc) {
        std::fprintf(stderr, "[native-gl] GetDC failed\n");
        return false;
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
        return false;
    }
    if (!SetPixelFormat(g_hdc, pf, &pfd)) {
        std::fprintf(stderr, "[native-gl] SetPixelFormat failed (%lu)\n", GetLastError());
        return false;
    }

    g_hrc = wglCreateContext(g_hdc);
    if (!g_hrc) {
        std::fprintf(stderr, "[native-gl] wglCreateContext failed\n");
        return false;
    }
    if (!wglMakeCurrent(g_hdc, g_hrc)) {
        std::fprintf(stderr, "[native-gl] wglMakeCurrent failed\n");
        return false;
    }
    g_owns_context = true;
    g_direct_window_context = false;
    initialize_context_state();

    return true;
}
#endif // _WIN32

// Reads the current GL target, packs pixels for the selected GS color format,
// and writes them into VRAM at FRAME.FBP for remaining GS transfer consumers.
void do_readback(const ReadbackState &state) {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc) return;

    // PS2 FRAME register layout: FBP bits 0..8 (9 bits, in 8192-byte units),
    // FBW bits 16..21, PSM bits 24..29.
    const std::uint32_t fbp = static_cast<std::uint32_t>(state.frame) & 0x1FFu;
    const std::uint32_t fbw = (static_cast<std::uint32_t>(state.frame) >> 16) & 0x3Fu;
    const std::uint32_t fpsm = (static_cast<std::uint32_t>(state.frame) >> 24) & 0x3Fu;
    if (fbw == 0) return;  // FRAME_1 not yet written; nothing meaningful to blit
    if (fpsm != gs::PSMCT32 && fpsm != gs::PSMCT24 && fpsm != gs::PSMCT16 && fpsm != gs::PSMCT16S) {
        static bool warned = false;
        if (!warned) {
            std::fprintf(stderr, "[native-gl] framebuffer format 0x%02X is not supported by diagnostic readback\n", fpsm);
            warned = true;
        }
        return;
    }

    glFinish();
    const int render_width = std::min(kWidth, static_cast<int>(fbw * 64u));
    if (g_direct_window_context) {
        g_bind_framebuffer(GL_READ_FRAMEBUFFER, g_scene_target.framebuffer);
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
    if (g_direct_window_context) {
        g_bind_framebuffer(GL_FRAMEBUFFER, g_scene_target.framebuffer);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, kWidth, kHeight);
    }
    g_readbacks.fetch_add(1, std::memory_order_relaxed);
#else
    (void)state;
#endif
}

} // namespace

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
        g_direct_window_context = false;
        g_hdc = nullptr;
        g_hrc = nullptr;
        g_hwnd = nullptr;
        g_checked = true;
        return false;
    }
    g_enabled = true;
    g_checked = true;
    std::fprintf(stderr, "[native-gl] attached SDL-owned visible-window WGL context | %s\n",
                 reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
    return true;
#else
    (void)host_window;
    return false;
#endif
}

bool direct_window() {
    return g_enabled && g_direct_window_context;
}

// Draws a VRAM display rectangle into the visible back buffer as a textured
// quad. Nearest filtering keeps the integer-scaled pixels exact. Restores the
// fixed-function state the primitive path leaves behind (texture unit, texenv,
// matrices) so the next submitted primitive sees the same context it expects.
bool draw_vram_display(const std::uint32_t *rgba, int width, int height,
                       int output_width, int output_height,
                       int draw_x, int draw_y, int draw_width, int draw_height) {
    static GLuint texture = 0;
    static int texture_width = 0, texture_height = 0;
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
    if (!g_enabled || !g_hdc || !g_hrc || !g_scene_target.ready) return false;
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

    static GLuint texture = 0;
    static int texture_width = 0, texture_height = 0;
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
    g_bind_framebuffer(GL_FRAMEBUFFER, g_scene_target.framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, kWidth, kHeight);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
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
    return true;
#else
    (void)psm; (void)base; (void)bw; (void)x; (void)y; (void)width; (void)height;
    return false;
#endif
}

bool present_frame(const DisplayRegs &regs) {
#if defined(_WIN32)
    if (!direct_window() || !g_scene_target.ready || !g_hdc || !g_hwnd || !g_blit_framebuffer) return false;
    if (wglGetCurrentContext() != g_hrc || wglGetCurrentDC() != g_hdc) {
        if (!wglMakeCurrent(g_hdc, g_hrc)) {
            std::fprintf(stderr, "[native-gl] could not make the SDL WGL context current for presentation\n");
            return false;
        }
    }

    RECT client{};
    if (!GetClientRect(g_hwnd, &client)) return false;
    const int output_width = client.right - client.left;
    const int output_height = client.bottom - client.top;
    if (output_width <= 0 || output_height <= 0) return false;

    g_bind_framebuffer(GL_READ_FRAMEBUFFER, g_scene_target.framebuffer);
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

    const int circuit = (regs.pmode & 2u) ? 1 : (regs.pmode & 1u) ? 0 : -1;
    if (circuit < 0) {
        g_bind_framebuffer(GL_FRAMEBUFFER, g_scene_target.framebuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, kWidth, kHeight);
        return true;
    }

    const std::uint64_t fb = regs.dispfb[circuit], disp = regs.display[circuit];
    const int dbx = static_cast<int>((fb >> 32) & 0x7FFu);
    const int dby = static_cast<int>((fb >> 43) & 0x7FFu);
    const int magh = static_cast<int>((disp >> 23) & 0xFu) + 1;
    const int magv = static_cast<int>((disp >> 27) & 3u) + 1;
    const int display_width = (static_cast<int>((disp >> 32) & 0xFFFu) + 1) / magh;
    int height = (static_cast<int>((disp >> 44) & 0x7FFu) + 1) / magv;
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
    const int picture_width = scissor_x1 >= scissor_x0 ? scissor_x1 - scissor_x0 + 1 : display_width;
    const int picture_height = scissor_y1 >= scissor_y0 ? scissor_y1 - scissor_y0 + 1 : height;
    const int width = std::min(display_width, picture_width);
    height = std::min(height, picture_height);
    const bool halved_field = (regs.smode2 & 3u) == 3u;
    const int source_height = halved_field ? (height + 1) / 2 : height;
    const int active_frame_width = g_active_frame_width.load(std::memory_order_relaxed);
    const std::uint32_t active_frame_fbp = g_active_frame_fbp.load(std::memory_order_relaxed);
    const std::uint32_t display_fbp = static_cast<std::uint32_t>(fb) & 0x1FFu;
    const std::uint32_t display_fbw = static_cast<std::uint32_t>(fb >> 9) & 0x3Fu;
    const std::uint32_t display_psm = static_cast<std::uint32_t>(fb >> 15) & 0x1Fu;
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
        g_bind_framebuffer(GL_FRAMEBUFFER, g_scene_target.framebuffer);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glViewport(0, 0, kWidth, kHeight);
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
    // Integer scaling keeps the GS pixels square: 1.5x nearest mixes doubled
    // and single lines, which is what makes front-end text look uneven. Below
    // 2x there is no integer step worth taking, so fill the window instead.
    const double scale = fit >= 2.0 ? std::floor(fit) : fit;
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
                     fit >= 2.0 ? "integer" : "fit", draw_x, draw_y, draw_width, draw_height);
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

    g_bind_framebuffer(GL_FRAMEBUFFER, g_scene_target.framebuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glViewport(0, 0, kWidth, kHeight);
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

    // Pick GL primitive from the PS2 PRIM register (low 3 bits: prim type).
    const Context &context = st.ctx[(attr >> 9) & 1u];
    const std::uint32_t frame = static_cast<std::uint32_t>(context.frame);
    const std::uint32_t fbw = (frame >> 16) & 0x3Fu;
    const std::uint32_t fpsm = (frame >> 24) & 0x3Fu;
    if (fbw == 0) return reject_packet(st, attr, prim_type, count, 4, "FRAME.FBW is zero");
    if (fpsm != gs::PSMCT32 && fpsm != gs::PSMCT24 && fpsm != gs::PSMCT16 && fpsm != gs::PSMCT16S)
        return reject_packet(st, attr, prim_type, count, 5, "framebuffer pixel format is unsupported");
    if (attr & (1ull << 5)) return reject_packet(st, attr, prim_type, count, 13, "GS fog is not implemented yet");
    const bool textured = (attr & (1ull << 4)) != 0;
    const bool framebuffer_16 = fpsm == gs::PSMCT16 || fpsm == gs::PSMCT16S;
    const bool tex_alpha_enabled = (context.tex0 & (1ull << 34)) != 0;
    const std::uint32_t tfx = static_cast<std::uint32_t>((context.tex0 >> 35) & 3u);
    if (framebuffer_16 && textured && tex_alpha_enabled)
        return reject_packet(st, attr, prim_type, count, 14, "16-bit framebuffer alpha quantization with texture alpha is not implemented yet");
    const bool alpha_blend = (attr & (1ull << 6)) != 0;
    bool native_source_over = false;
    if (alpha_blend) {
        const std::uint32_t alpha = static_cast<std::uint32_t>(context.alpha);
        const std::uint32_t a = alpha & 3u;
        const std::uint32_t b = (alpha >> 2) & 3u;
        const std::uint32_t c = (alpha >> 4) & 3u;
        const std::uint32_t d = (alpha >> 6) & 3u;
        // This exact ALPHA tuple computes Cd * (1 - Ad/128). On a 16-bit GS
        // framebuffer Ad is one bit (0 or 0x80), so OpenGL's destination-alpha
        // factor is exact once the alpha channel is quantized on each write.
        // Other tuples need custom GS arithmetic and remain fail-closed.
        const bool identity_blend = a == b && a <= 2 && d == 0;
        const bool exact_destination_alpha_fade = framebuffer_16 && a == 2 && b == 1 && c == 1 && d == 1;
        const bool gs_source_over = a == 0 && b == 1 && c == 0 && d == 1;
        if (identity_blend) {
            glDisable(GL_BLEND);
        } else if (exact_destination_alpha_fade && !(textured && (tex_alpha_enabled || tfx != 0)) &&
                   g_blend_func_separate) {
            glEnable(GL_BLEND);
            g_blend_func_separate(GL_ZERO, GL_ONE_MINUS_DST_ALPHA, GL_ONE, GL_ZERO);
        } else if (gs_source_over && fpsm == gs::PSMCT32 && (st.colclamp & 1u) &&
                   (!textured || tfx == 0)) {
            if (!ensure_alpha_compositor())
                return reject_packet(st, attr, prim_type, count, 14, "GS /128 source-over requires the native alpha compositor");
            native_source_over = true;
            glDisable(GL_BLEND);
        } else {
            return reject_packet(st, attr, prim_type, count, 14, "GS ALPHA tuple is not exactly supported by the native OpenGL blend path");
        }
    } else {
        glDisable(GL_BLEND);
    }
    const std::uint64_t test = context.test;
    const bool zte = (test & (1ull << 16)) != 0;
    const std::uint32_t ztst = static_cast<std::uint32_t>(test >> 17) & 3u;
    const std::uint32_t afail = static_cast<std::uint32_t>(test >> 12) & 3u;
    const bool zmsk = (context.zbuf & (1ull << 32)) != 0;
    if (test & 1u) {
        // With AFAIL=FB_ONLY, failed alpha tests still write color and only
        // suppress depth. The test is therefore inert when depth is disabled,
        // or when ZTST is ALWAYS and ZBUF masks every depth write.
        const bool no_depth_change = !zte || (ztst == 1u && zmsk);
        if (afail != 1u || !no_depth_change)
            return reject_packet(st, attr, prim_type, count, 15, "GS alpha test changes framebuffer or depth results");
    }
    if (context.test & (1ull << 14)) return reject_packet(st, attr, prim_type, count, 16, "GS destination-alpha test is not implemented yet");
    const std::uint32_t zpsm = 0x30u | (static_cast<std::uint32_t>(context.zbuf >> 24) & 0xFu);
    const std::uint64_t zbuf_key = context.zbuf & 0xFFFFFFFFull;
    const bool depth_surface_effect = zte && (ztst == 2 || ztst == 3 || (!zmsk && ztst != 0));
    const std::uint32_t zmax = depth_max_for_psm(zpsm);
    if (depth_surface_effect) {
        if (zpsm != gs::PSMZ32 && zpsm != gs::PSMZ24 && zpsm != gs::PSMZ16 && zpsm != gs::PSMZ16S)
            return reject_packet(st, attr, prim_type, count, 17, "GS depth-buffer pixel format is unsupported");
        if (g_depth_key_valid && zbuf_key != g_depth_key)
            return reject_packet(st, attr, prim_type, count, 24, "multiple GS ZBUF surfaces are not implemented yet");
        if (!g_depth_key_valid) {
            glDepthMask(GL_TRUE);
            glClearDepth(1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
            g_depth_key = zbuf_key;
            g_depth_key_valid = true;
        }
    }
    if ((context.frame >> 32) != 0) return reject_packet(st, attr, prim_type, count, 18, "GS framebuffer write mask is not implemented yet");
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
        if (g_active_texture) {
            g_active_texture(GL_TEXTURE1);
            glDisable(GL_TEXTURE_2D);
            g_active_texture(GL_TEXTURE0);
        }
        glDisable(GL_TEXTURE_2D);
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
    glEnable(GL_SCISSOR_TEST);
    glScissor(clip_left, kHeight - clip_bottom,
              std::max(0, clip_right - clip_left), std::max(0, clip_bottom - clip_top));
    const bool fst = (attr & (1ull << 8)) != 0;
    const auto emit = [&](const Vertex &v, const Vertex &color, std::uint32_t x, std::uint32_t y,
                          float s, float t, float q) {
        const float fx = static_cast<float>(static_cast<int>(x) - ofx) / 16.0f;
        const float fy = static_cast<float>(static_cast<int>(y) - ofy) / 16.0f;
        const std::uint32_t z = std::min(v.z, zmax);
        const float z01 = static_cast<float>(static_cast<double>(z) / static_cast<double>(zmax));
        const float fz = z01 * 2.0f - 1.0f;
        if (textured) {
            glTexCoord4f(s, t, 0.0f, q);
            if (((context.tex0 >> 35) & 3u) >= 2u && g_multi_texcoord4f)
                g_multi_texcoord4f(GL_TEXTURE1, s, t, 0.0f, q);
        }
        const GLubyte alpha = framebuffer_16 && (!textured || !tex_alpha_enabled)
            ? (color.a >= 0x80u ? 0xFFu : 0u)
            : color.a;
        glColor4ub(color.r, color.g, color.b, alpha);
        glVertex3f(fx, fy, fz);
    };
    const auto uv = [&](const Vertex &v) {
        if (fst) {
            return std::array<float, 2>{
                static_cast<float>(v.u) / (16.0f * static_cast<float>(texture_width)),
                static_cast<float>(v.v) / (16.0f * static_cast<float>(texture_height))};
        }
        return std::array<float, 2>{v.s, v.t};
    };
    const Vertex &flat_color = verts[count - 1];
    if (zte) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(zmsk ? GL_FALSE : GL_TRUE);
        switch (ztst) {
        case 0: glDepthFunc(GL_NEVER); break;
        case 1: glDepthFunc(GL_ALWAYS); break;
        case 2: glDepthFunc(GL_LEQUAL); break;
        default: glDepthFunc(GL_LESS); break;
        }
    } else {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
    }
    const ReadbackState readback_state{context.frame, st.dimx, st.dthe, st.colclamp};
    if (g_pending_readback_state_valid && !same_readback_state(g_pending_readback_state, readback_state)) {
        // Snapshot draws with the state that produced them before a new target
        // or packing state can reinterpret the pending pixels.
        glFinish();
        do_readback(g_pending_readback_state);
        glDisable(GL_SCISSOR_TEST);
        if (!g_direct_window_context) glClear(GL_COLOR_BUFFER_BIT);
        g_pending_readback_state_valid = false;
        g_last_readback = clock_t_::now();
    }

    if (native_source_over) {
        if (!begin_alpha_composite(textured, clip_left, clip_top, clip_right, clip_bottom))
            return reject_packet(st, attr, prim_type, count, 14, "GS /128 source-over could not start the native alpha compositor");
        static bool reported_alpha_composite = false;
        if (!reported_alpha_composite) {
            reported_alpha_composite = true;
            std::fprintf(stderr, "[native-gl] GS ALPHA=0x44 PSMCT32 primitive uses the /128 source-over compositor\n");
        }
    }

    glBegin(mode);
    if (prim_type == 6) {
        // A GS sprite is encoded by two opposite corners. Expand it into a
        // triangle strip. The texture coordinates follow the same two-corner
        // interpolation as the software rasterizer's sprite() implementation.
        const Vertex &a = verts[0], &b = verts[1];
        const auto auv = uv(a), buv = uv(b);
        const float q = fst || b.q == 0.0f ? 1.0f : b.q;
        emit(a, b, a.x, a.y, auv[0], auv[1], q);
        emit(b, b, b.x, a.y, buv[0], auv[1], q);
        emit(a, b, a.x, b.y, auv[0], buv[1], q);
        emit(b, b, b.x, b.y, buv[0], buv[1], q);
    } else {
        for (int i = 0; i < count; ++i) {
            const auto texcoord = uv(verts[i]);
            const Vertex &color = (attr & 8u) ? verts[i] : flat_color;
            const float q = fst || verts[i].q == 0.0f ? 1.0f : verts[i].q;
            emit(verts[i], color, verts[i].x, verts[i].y, texcoord[0], texcoord[1], q);
        }
    }
    glEnd();
    if (native_source_over) finish_alpha_composite();
    g_pending_readback_state = readback_state;
    g_pending_readback_state_valid = true;

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

    // Keep the existing GS-memory compatibility sync cadence for reads and
    // transfers. Direct presentation reads the scene FBO independently.
    const auto now = clock_t_::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - g_last_readback).count();
    if (elapsed >= kReadbackIntervalMs) {
        glFlush();
        do_readback(g_pending_readback_state);
        glDisable(GL_SCISSOR_TEST);
        if (!g_direct_window_context) glClear(GL_COLOR_BUFFER_BIT);
        g_pending_readback_state_valid = false;
        g_last_readback = now;
    }
    return true;
#else
    (void)st; (void)verts; (void)count;
    return false;
#endif
}

void flush() {
#if defined(_WIN32)
    if (!g_enabled || !g_hdc) return;
    if (g_direct_window_context) {
        if (g_pending_readback_state_valid) {
            glFinish();
            do_readback(g_pending_readback_state);
            g_pending_readback_state_valid = false;
        } else {
            glFlush();
        }
        return;
    }
    if (!g_pending_readback_state_valid) return;
    glFinish();
    do_readback(g_pending_readback_state);
    glDisable(GL_SCISSOR_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    g_pending_readback_state_valid = false;
#endif
}

void shutdown() {
#if defined(_WIN32)
    if (g_hrc && g_hdc) {
        if (wglGetCurrentContext() != g_hrc) wglMakeCurrent(g_hdc, g_hrc);
        release_alpha_compositor();
        release_scene_target();
        if (g_texture.object) glDeleteTextures(1, &g_texture.object);
        g_texture = TextureCache{};
        if (g_owns_context) {
            wglMakeCurrent(g_hdc, nullptr);
            wglDeleteContext(g_hrc);
        }
        g_hrc = nullptr;
    }
    if (g_owns_context && g_hwnd && g_hdc) {
        ReleaseDC(g_hwnd, g_hdc);
    }
    if (g_owns_context && g_hwnd) {
        DestroyWindow(g_hwnd);
    }
    g_hdc = nullptr;
    g_hwnd = nullptr;
    g_owns_context = false;
    g_direct_window_context = false;
    g_pending_readback_state_valid = false;
    g_depth_key_valid = false;
#endif
    g_enabled = false;
    g_checked = false;
}

} // namespace ps2::gs::native_gl
