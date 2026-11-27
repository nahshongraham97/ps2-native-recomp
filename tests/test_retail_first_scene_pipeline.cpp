#include "mka/retail_main.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_scene_owners.hpp"
#include "mka/retail_display_service.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_platform.hpp"
#include "mka/retail_renderer_allocator.hpp"
#include "mka/retail_renderer_core.hpp"
#include "mka/retail_camera_world.hpp"
#include "mka/content_volume.hpp"
#include "native_retail_startup.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#endif

namespace {

void check(bool ok, const char* msg) {
    if (!ok) {
        std::fprintf(stderr, "FAIL first_scene_pipeline: %s\n", msg);
        throw std::runtime_error(msg);
    }
}

void push_u32(std::vector<std::byte>& out, std::uint32_t v) {
    out.push_back(std::byte(v & 0xFFu));
    out.push_back(std::byte((v >> 8) & 0xFFu));
    out.push_back(std::byte((v >> 16) & 0xFFu));
    out.push_back(std::byte((v >> 24) & 0xFFu));
}

void push_u16(std::vector<std::byte>& out, std::uint16_t v) {
    out.push_back(std::byte(v & 0xFFu));
    out.push_back(std::byte((v >> 8) & 0xFFu));
}

void push_f32(std::vector<std::byte>& out, float v) {
    std::uint32_t u = 0;
    std::memcpy(&u, &v, 4);
    push_u32(out, u);
}

std::vector<std::byte> wrap_chunk(std::uint32_t type, const std::vector<std::byte>& body) {
    std::vector<std::byte> out;
    out.reserve(12 + body.size());
    push_u32(out, type);
    push_u32(out, static_cast<std::uint32_t>(body.size()));
    push_u32(out, 0x1C020065u);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

// Build a valid TXD with 1 PS2-swizzled PSMT8 texture ("first_magenta", 128x64, CSM1 CLUT index 9 -> magenta).
std::vector<std::byte> build_swizzled_psmt8_txd() {
    std::vector<std::byte> ns(88, std::byte{0});
    auto put32 = [&](std::size_t o, std::uint32_t v) {
        ns[o + 0] = std::byte(v & 0xFFu);
        ns[o + 1] = std::byte((v >> 8) & 0xFFu);
        ns[o + 2] = std::byte((v >> 16) & 0xFFu);
        ns[o + 3] = std::byte((v >> 24) & 0xFFu);
    };
    auto put16 = [&](std::size_t o, std::uint16_t v) {
        ns[o + 0] = std::byte(v & 0xFFu);
        ns[o + 1] = std::byte((v >> 8) & 0xFFu);
    };
    put32(0, 0x00325350u); // PS2
    put32(4, 0x00001102u);
    constexpr std::string_view kName = "first_magenta";
    for (std::size_t i = 0; i < kName.size(); ++i) ns[8 + i] = static_cast<std::byte>(kName[i]);
    put32(72, 0x00022000u); // swizzled + PAL8
    put32(76, 1u);
    put16(80, 128u);
    put16(82, 64u);
    ns[84] = std::byte{8};
    ns[85] = std::byte{1};
    ns[86] = std::byte{4};
    ns[87] = std::byte{1}; // comp swizzle bit

    // CSM1 CLUT: logical index 9 lives at physical index 9^0x18 = 17.
    std::vector<std::byte> pal(1024, std::byte{0});
    pal[17 * 4 + 0] = std::byte{255};
    pal[17 * 4 + 1] = std::byte{20};
    pal[17 * 4 + 2] = std::byte{220};
    pal[17 * 4 + 3] = std::byte{0x80};
    std::vector<std::byte> pix(8192, std::byte{9});
    ns.insert(ns.end(), pal.begin(), pal.end());
    ns.insert(ns.end(), pix.begin(), pix.end());

    auto tn = wrap_chunk(0x0015u, wrap_chunk(0x0001u, ns));
    std::vector<std::byte> tb = wrap_chunk(0x0001u, {std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}});
    tb.insert(tb.end(), tn.begin(), tn.end());
    return wrap_chunk(0x0016u, tb);
}

// Build a valid RpWorld BSP stream referencing "first_magenta" with 1 visible sector at z=0.
std::vector<std::byte> build_first_scene_bsp() {
    std::vector<std::byte> tex_st;
    push_u32(tex_st, 0x00001102u);
    std::vector<std::byte> tname;
    for (char ch : std::string_view("first_magenta")) tname.push_back(static_cast<std::byte>(ch));
    tname.push_back(std::byte{0});
    auto tp = wrap_chunk(0x0001u, tex_st);
    auto nc = wrap_chunk(0x0002u, tname);
    tp.insert(tp.end(), nc.begin(), nc.end());
    auto rwt = wrap_chunk(0x0006u, tp);

    std::vector<std::byte> mat_st;
    push_u32(mat_st, 0u);
    push_u32(mat_st, 0xFFFFFFFFu);
    push_u32(mat_st, 0u);
    push_u32(mat_st, 1u); // textured
    push_f32(mat_st, 1.0f);
    push_f32(mat_st, 0.0f);
    push_f32(mat_st, 1.0f);
    auto mp = wrap_chunk(0x0001u, mat_st);
    mp.insert(mp.end(), rwt.begin(), rwt.end());
    auto rwm = wrap_chunk(0x0007u, mp);

    std::vector<std::byte> ml_st;
    push_u32(ml_st, 1u);
    push_u32(ml_st, 0xFFFFFFFFu);
    auto mlp = wrap_chunk(0x0001u, ml_st);
    mlp.insert(mlp.end(), rwm.begin(), rwm.end());
    auto matlist = wrap_chunk(0x0008u, mlp);

    std::vector<std::byte> sec_st;
    push_u32(sec_st, 0u); // matListWindowBase
    push_u32(sec_st, 1u); // 1 tri
    push_u32(sec_st, 3u); // 3 verts
    push_f32(sec_st, -2.5f); push_f32(sec_st, -2.5f); push_f32(sec_st, -0.5f);
    push_f32(sec_st,  2.5f); push_f32(sec_st,  2.5f); push_f32(sec_st,  0.5f);
    push_u32(sec_st, 0u);
    push_u32(sec_st, 0u);
    // 3 vertices
    push_f32(sec_st, -2.0f); push_f32(sec_st, -2.0f); push_f32(sec_st, 0.0f);
    push_f32(sec_st,  2.0f); push_f32(sec_st, -2.0f); push_f32(sec_st, 0.0f);
    push_f32(sec_st,  0.0f); push_f32(sec_st,  2.0f); push_f32(sec_st, 0.0f);
    // 3 UVs
    push_f32(sec_st, 0.0f); push_f32(sec_st, 0.0f);
    push_f32(sec_st, 1.0f); push_f32(sec_st, 0.0f);
    push_f32(sec_st, 0.5f); push_f32(sec_st, 1.0f);
    // 1 triangle (mat=0, v=0,1,2)
    push_u16(sec_st, 0u); push_u16(sec_st, 0u); push_u16(sec_st, 1u); push_u16(sec_st, 2u);
    auto atomic_sec = wrap_chunk(0x0009u, wrap_chunk(0x0001u, sec_st));

    std::vector<std::byte> ws_st;
    push_u32(ws_st, 1u); // rootIsWorldSector
    push_f32(ws_st, -2.5f); push_f32(ws_st, -2.5f); push_f32(ws_st, -0.5f);
    push_f32(ws_st,  2.5f); push_f32(ws_st,  2.5f); push_f32(ws_st,  0.5f);
    push_u32(ws_st, 1u); // numTriangles
    push_u32(ws_st, 3u); // numVertices
    push_u32(ws_st, 0u); // numPlaneSectors
    push_u32(ws_st, 1u); // numWorldSectors
    push_u32(ws_st, 0u);
    push_u32(ws_st, 0x04u);

    std::vector<std::byte> world_body = wrap_chunk(0x0001u, ws_st);
    world_body.insert(world_body.end(), matlist.begin(), matlist.end());
    world_body.insert(world_body.end(), atomic_sec.begin(), atomic_sec.end());
    return wrap_chunk(0x000Bu, world_body);
}

void write_bytes(const std::filesystem::path& p, const std::vector<std::byte>& bytes) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    check(out.good(), "write test asset file");
}

struct PipelineHarness {
    mka::retail::NativeRetailStartupState startup;
    mka::input::ControllerInput input;
    std::unique_ptr<mka::retail::NativeRetailMainState> main_state;
    std::shared_ptr<mka::retail::NativeRetailRendererCore> core;

    explicit PipelineHarness(const std::filesystem::path& vol_root)
        : startup([&] {
              mka::content::ContentVolumeFailure cf;
              auto vol = mka::content::NativeContentVolume::open(vol_root, cf);
              check(vol.has_value(), "open content volume");
              return mka::retail::NativeRetailStartupState(std::move(*vol), {"cdrom0:\\SLUS_214.10"});
          }()) {
        mka::retail::StartupFailure sf;
        check(mka::retail::initialize_native_retail_game_session(startup, sf), "initialize game session");
        check(mka::retail::initialize_retail_memory_owners(startup, sf), "initialize retail memory owners");

        main_state = std::make_unique<mka::retail::NativeRetailMainState>(startup, input);
        mka::retail::RetailRendererAllocatorFailure af;
        auto alloc = mka::retail::NativeRetailRendererAllocatorService::create(*startup.memory_owners, af);
        check(alloc != nullptr, "create renderer allocator");
        mka::retail::RetailRendererCoreFailure cf;
        core = mka::retail::NativeRetailRendererCore::for_session(
            startup.game_session, std::move(alloc), startup.content, cf);
        check(core != nullptr, "create renderer core");
        auto init = core->initialize_core(cf);
        check(init && *init, "initialize renderer core");

        auto* world_cams = startup.game_session->render_cameras(sf.game);
        check(world_cams != nullptr, "get world_cams");
        mka::retail::RetailCameraWorldFailure cwf;
        check(world_cams->store_initial_count_from_127260(cwf), "admit 127260 camera count");
    }
};

} // namespace

int main() {
    using namespace mka::retail;
    const std::filesystem::path base = std::filesystem::current_path() / "b" / "check-temp" / "first_scene_pipeline";
    std::error_code ec;
    std::filesystem::remove_all(base, ec);

    // 1. Asset-less headless mode: must succeed without failing startup and keep has_world == false.
    {
        const auto empty_root = base / "empty_vol";
        std::filesystem::create_directories(empty_root);
        PipelineHarness h(empty_root);
        StartupFailure sf;
        auto gate = scene_bootstrap_required::continue_first_scene_at_225d40(*h.main_state, h.core, sf);
        check(gate == RetailStartupGate::ready, "asset-less headless returns ready");
        check(h.main_state->scene && !h.main_state->scene->scene_loaded, "asset-less headless leaves scene_loaded false");
        auto re = main_binding::retail_render_enabled(*h.main_state, sf);
        check(re && !*re, "asset-less headless keeps retail_render_enabled false");
        check(main_binding::render_retail_frame(*h.main_state, sf), "asset-less render_retail_frame succeeds");
    }

    // 2. Corrupt/truncated archive stream must fail closed.
    {
        const auto bad_root = base / "bad_vol";
        write_bytes(bad_root / "scene" / "first.txd", std::vector<std::byte>(16, std::byte{0x11}));
        PipelineHarness h(bad_root);
        StartupFailure sf;
        auto gate = scene_bootstrap_required::continue_first_scene_at_225d40(*h.main_state, h.core, sf);
        check(gate == RetailStartupGate::native_failure && sf.kind == StartupError::content,
              "corrupt first-scene TXD fails closed");
        auto re = main_binding::retail_render_enabled(*h.main_state, sf);
        check(re && !*re, "failed first-scene load keeps retail_render_enabled false");
    }

    // 3. Asset-loaded headless fallback (no active GL context): loads world/textures, skips GPU draw cleanly.
    const auto live_root = base / "live_vol";
    write_bytes(live_root / "scene" / "first.txd", build_swizzled_psmt8_txd());
    write_bytes(live_root / "scene" / "first.bsp", build_first_scene_bsp());
    {
        PipelineHarness h(live_root);
        h.main_state->scene=std::make_shared<NativeRetailSceneOwners>();
        h.main_state->scene->config.diagnostic_headless_without_display=true;
        StartupFailure sf;
        auto gate = scene_bootstrap_required::continue_first_scene_at_225d40(*h.main_state, h.core, sf);
        if (gate != RetailStartupGate::ready) {
            std::fprintf(stderr, "debug sf.operation=%s\n", sf.operation ? sf.operation : "(null)");
        }
        check(gate == RetailStartupGate::ready, "asset-loaded headless returns ready");
        check(h.main_state->scene && h.main_state->scene->scene_loaded, "scene_loaded true");
        check(h.main_state->scene->stream_result.txd_ingested && h.main_state->scene->stream_result.world_ingested,
              "both TXD and BSP streams ingested");
        check(h.main_state->scene->world && h.main_state->scene->world->bsp_sectors().size() == 1,
              "1 BSP sector loaded");
        auto re = main_binding::retail_render_enabled(*h.main_state, sf);
        check(re && *re, "retail_render_enabled true when scene loaded");
        check(main_binding::render_retail_frame(*h.main_state, sf), "headless fallback render_retail_frame succeeds");
        check(h.main_state->scene->frames_rendered == 0, "headless fallback without GL context skips GPU draw");
    }

    // 4. Live hidden-window GPU path: continue_first_scene_at_225d40 -> render_retail_frame -> render_world_scene_frame -> teardown.
#ifdef _WIN32
    {
        DisplayServiceFailure df;
        auto display = NativeOffscreenDisplayService::create(64, 48, df);
        check(display != nullptr, "create offscreen display");
        check(display->acquire_context(df), "acquire hidden GL context");

        PipelineHarness h(live_root);
        h.main_state->scene = std::make_shared<NativeRetailSceneOwners>();
        h.main_state->scene->config.viewport_width = 64;
        h.main_state->scene->config.viewport_height = 48;
        h.main_state->scene->config.display = display;

        StartupFailure sf;
        auto gate = scene_bootstrap_required::continue_first_scene_at_225d40(*h.main_state, h.core, sf);
        check(gate == RetailStartupGate::ready, "GPU first-scene load returns ready");
        check(h.main_state->scene && h.main_state->scene->scene_loaded && h.main_state->scene->gl_context_active,
              "GPU first-scene loaded with active GL context");
        check(h.main_state->scene->world && h.main_state->scene->world->bsp_sectors().size() == 1,
              "GPU world has 1 loaded BSP sector");
        check(h.main_state->scene->camera_init.has_value(), "GPU camera initialized");

        RetailRendererFrameFailure ff;
        RetailRendererMatrixValue cam_mod{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {0, 0, -4.0f}}, 0x20000u};
        check(h.main_state->scene->frames->set_modelling(h.main_state->scene->camera_init->primary.frame, cam_mod, ff),
              "position first-scene camera at z=-4");

        auto re = main_binding::retail_render_enabled(*h.main_state, sf);
        check(re && *re, "GPU retail_render_enabled true");
        if (!main_binding::render_retail_frame(*h.main_state, sf)) {
            std::fprintf(stderr, "debug render sf.operation=%s\n", sf.operation ? sf.operation : "(null)");
            check(false, "GPU render_retail_frame succeeded");
        }
        check(h.main_state->scene->frames_rendered == 1, "render_retail_frame dispatched render_world_scene_frame");
        check(!h.main_state->scene->world->gpu_texture_cache().empty(), "GPU texture cache populated");
        const GLuint tex_id = static_cast<GLuint>(h.main_state->scene->world->gpu_texture_cache()[0].gl_texture_id);
        check(glIsTexture(tex_id) == GL_TRUE, "uploaded GL texture handle is valid");

        RetailRendererCameraFailure rcf;
        check(h.main_state->scene->cameras->begin_update(h.main_state->scene->camera_init->primary.camera, rcf),
              "rebind primary FBO for readback");
        std::vector<unsigned char> rgba(64 * 48 * 4);
        std::vector<float> depths(64 * 48);
        glReadPixels(0, 0, 64, 48, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glReadPixels(0, 0, 64, 48, GL_DEPTH_COMPONENT, GL_FLOAT, depths.data());
        display->record_diagnostic_readback(2);
        check(h.main_state->scene->cameras->end_update(h.main_state->scene->camera_init->primary.camera, rcf),
              "end readback pass");

        unsigned magenta_pixels = 0;
        for (unsigned y = 6; y < 42; ++y) {
            for (unsigned x = 8; x < 56; ++x) {
                const std::size_t idx = y * 64 + x;
                if (rgba[idx * 4 + 0] > 200 && rgba[idx * 4 + 1] < 50 && rgba[idx * 4 + 2] > 170 && depths[idx] < 0.99f) {
                    ++magenta_pixels;
                }
            }
        }
        check(magenta_pixels > 60, "first-scene path rendered measurable PS2-unswizzled magenta pixels");

        check(scene_bootstrap_required::continue_scene_reset_at_125d60(*h.main_state, sf),
              "continue_scene_reset_at_125d60 teardown succeeded");
        check(glIsTexture(tex_id) == GL_FALSE, "teardown deleted GL texture with zero leaks");
        check(glGetError() == GL_NO_ERROR, "zero GL errors on teardown");
        auto re_after = main_binding::retail_render_enabled(*h.main_state, sf);
        check(re_after && !*re_after, "retail_render_enabled false after scene reset");
        (void)display->release_context(df);
    }
#endif

    std::filesystem::remove_all(base, ec);
    std::printf("PASS: test_retail_first_scene_pipeline (headless, fail-closed, PS2 unswizzle, live GPU + teardown)\n");
    return 0;
}
