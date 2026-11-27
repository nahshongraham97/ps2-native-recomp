#include "mka/retail_scene_archive.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_game_session.hpp"
#include "mka/retail_scene_owners.hpp"
#include "mka/retail_scene_bootstrap_required.hpp"
#include "mka/retail_display_service.hpp"
#include "mka/retail_renderer_allocator.hpp"
#include "mka/retail_renderer_cameras.hpp"
#include "native_retail_startup.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <new>
#include <stdexcept>
#include <string_view>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef near
#undef far
#include <GL/gl.h>
#endif

using namespace mka::retail;

namespace {
thread_local int fail_after = -1;
std::size_t g_checks = 0;

void check(bool ok, const char* why) {
    ++g_checks;
    if (!ok) {
        std::fprintf(stderr, "FAIL ssf_native_geometry: %s\n", why);
        throw std::runtime_error(why);
    }
}
} // namespace

void* operator new(std::size_t n) {
    if (fail_after == 0) throw std::bad_alloc();
    if (fail_after > 0) --fail_after;
    if (auto p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

namespace {

void push_u32(std::vector<std::byte>& out, std::uint32_t v) {
    out.push_back(std::byte(v & 0xffu));
    out.push_back(std::byte((v >> 8) & 0xffu));
    out.push_back(std::byte((v >> 16) & 0xffu));
    out.push_back(std::byte((v >> 24) & 0xffu));
}
void push_u16(std::vector<std::byte>& out, std::uint16_t v) {
    out.push_back(std::byte(v & 0xffu));
    out.push_back(std::byte((v >> 8) & 0xffu));
}
void push_f32(std::vector<std::byte>& out, float v) {
    push_u32(out, std::bit_cast<std::uint32_t>(v));
}

std::vector<std::byte> rw_chunk(std::uint32_t id, const std::vector<std::byte>& body) {
    std::vector<std::byte> out;
    out.reserve(12 + body.size());
    push_u32(out, id);
    push_u32(out, static_cast<std::uint32_t>(body.size()));
    push_u32(out, 0x1803ffffu);
    out.insert(out.end(), body.begin(), body.end());
    return out;
}

std::vector<std::byte> make_synthetic_native_clump(
    const std::vector<std::byte>& dma_vif_payload,
    std::uint32_t declared_pkt_size_override = 0,
    std::uint32_t num_verts = 8,
    std::uint32_t num_tris = 4) {
    // Clump struct
    std::vector<std::byte> cs;
    push_u32(cs, 1); push_u32(cs, 0); push_u32(cs, 0);
    // Frame list: 1 identity frame
    std::vector<std::byte> fls;
    push_u32(fls, 1);
    for (int i = 0; i < 12; ++i) push_f32(fls, (i == 0 || i == 4 || i == 8) ? 1.0f : 0.0f);
    push_u32(fls, 0xffffffffu); push_u32(fls, 0);
    std::vector<std::byte> fl = rw_chunk(1, fls);
    auto fext = rw_chunk(3, {});
    fl.insert(fl.end(), fext.begin(), fext.end());
    auto flist = rw_chunk(14, fl);

    // Geometry: rpGEOMETRYNATIVE (0x0101003f)
    std::vector<std::byte> gbody;
    std::vector<std::byte> gs_hdr;
    push_u32(gs_hdr, 0x0101003fu);
    push_u32(gs_hdr, num_tris);
    push_u32(gs_hdr, num_verts);
    push_u32(gs_hdr, 1); // morph_count
    push_f32(gs_hdr, 0.0f); push_f32(gs_hdr, 0.0f); push_f32(gs_hdr, 0.0f); push_f32(gs_hdr, 5.0f);
    push_u32(gs_hdr, 0); push_u32(gs_hdr, 0);
    // STRUCT chunk header (synthetic length 16 + 24)
    push_u32(gbody, 1); push_u32(gbody, 40); push_u32(gbody, 0x1803ffffu);
    gbody.insert(gbody.end(), gs_hdr.begin(), gs_hdr.end());

    // Material list: 1 untextured material
    std::vector<std::byte> mls;
    push_u32(mls, 1); push_u32(mls, 0xffffffffu);
    std::vector<std::byte> ms;
    push_u32(ms, 0); push_u32(ms, 0xffffffffu); push_u32(ms, 0); push_u32(ms, 0);
    push_f32(ms, 1.0f); push_f32(ms, 0.0f); push_f32(ms, 1.0f);
    std::vector<std::byte> mp = rw_chunk(1, ms);
    std::vector<std::byte> mext_data;
    push_u32(mext_data, 5);
    for (int i = 0; i < 6; ++i) push_u32(mext_data, i == 2 ? 0xffffffffu : 0u);
    auto mext = rw_chunk(3, rw_chunk(0x895303u, mext_data));
    mp.insert(mp.end(), mext.begin(), mext.end());
    auto mat = rw_chunk(7, mp);
    auto mlist_body = rw_chunk(1, mls);
    mlist_body.insert(mlist_body.end(), mat.begin(), mat.end());
    auto mlist = rw_chunk(8, mlist_body);
    gbody.insert(gbody.end(), mlist.begin(), mlist.end());

    // Extension -> 0x50e binmesh (1 mesh, material 0)
    std::vector<std::byte> bm;
    push_u32(bm, 1); push_u32(bm, 1); push_u32(bm, num_verts);
    push_u32(bm, num_verts); push_u32(bm, 0);
    auto gext = rw_chunk(3, rw_chunk(0x50eu, bm));
    gbody.insert(gbody.end(), gext.begin(), gext.end());

    // Native mesh packet
    const std::uint32_t pkt_sz = declared_pkt_size_override ? declared_pkt_size_override
        : static_cast<std::uint32_t>(dma_vif_payload.size());
    push_u32(gbody, pkt_sz);
    push_u32(gbody, 1);
    gbody.insert(gbody.end(), dma_vif_payload.begin(), dma_vif_payload.end());

    auto geom = rw_chunk(15, gbody);
    std::vector<std::byte> gl_hdr;
    push_u32(gl_hdr, 1);
    auto gl_body = rw_chunk(1, gl_hdr);
    gl_body.insert(gl_body.end(), geom.begin(), geom.end());
    auto glist = rw_chunk(26, gl_body);

    // Atomic 0
    std::vector<std::byte> as;
    push_u32(as, 0); push_u32(as, 0); push_u32(as, 5); push_u32(as, 0);
    std::vector<std::byte> ae;
    push_u32(ae, 3); push_u32(ae, 0); push_u32(ae, 0); push_u32(ae, 0);
    auto abody = rw_chunk(1, as);
    auto aext = rw_chunk(3, rw_chunk(0x895302u, ae));
    abody.insert(abody.end(), aext.begin(), aext.end());
    auto atomic = rw_chunk(20, abody);

    std::vector<std::byte> clump_body = rw_chunk(1, cs);
    clump_body.insert(clump_body.end(), flist.begin(), flist.end());
    clump_body.insert(clump_body.end(), glist.begin(), glist.end());
    clump_body.insert(clump_body.end(), atomic.begin(), atomic.end());
    auto cext = rw_chunk(3, {});
    clump_body.insert(clump_body.end(), cext.begin(), cext.end());
    return rw_chunk(16, clump_body);
}

std::vector<std::byte> build_synthetic_vif_packet(
    std::uint32_t dma_id,
    std::uint32_t qwc_override,
    std::uint32_t extra_vif_opcode,
    bool use_unsupported_unpack,
    bool truncate_unpack) {
    std::vector<std::byte> vif;
    push_u32(vif, 0x05000000u); // STMOD
    push_u32(vif, 0x01000101u); // STCYCL 1,1
    push_u32(vif, 0x6e81c001u | 0x00010000u); // UNPACK V4_8 usn=1 num=1 addr=0x001 (fix below)
    vif.resize(8);
    // Batch header: UNPACK V4_8 usn=1 num=1 addr=0x001 -> cmd=0x6e, flg=1, usn=1 -> 0x6ec001 + (1<<16) = 0x6e01c001
    push_u32(vif, 0x6e01c001u);
    push_u32(vif, 8u); // 8 vertices
    push_u32(vif, 0x01000104u); // STCYCL cl=4 wl=1
    if (use_unsupported_unpack) {
        // V4_16 usn=1 num=1 addr=0x011
        push_u32(vif, 0x6d01c011u);
        push_u32(vif, 0); push_u32(vif, 0);
    } else {
        // Positions: V3_32 num=8 addr=0x011
        push_u32(vif, 0x68088011u);
        for (int i = 0; i < 8; ++i) {
            push_f32(vif, float(i)); push_f32(vif, float(i & 1)); push_f32(vif, 0.0f);
        }
        // ADC S_16 num=8 addr=0x011 -> vertices 4 and 5 have 0x8000 (strip break!)
        push_u32(vif, 0x61088011u);
        for (int i = 0; i < 8; ++i) {
            push_u16(vif, (i == 4 || i == 5) ? 0x8000u : 0x0000u);
        }
    }
    if (extra_vif_opcode != 0) {
        push_u32(vif, extra_vif_opcode);
    }
    push_u32(vif, 0x14000000u); // MSCAL 0
    while (vif.size() % 16 != 8) push_u32(vif, 0);
    if (truncate_unpack && vif.size() > 16) {
        vif.resize(vif.size() - 12);
    }
    const std::uint32_t qwc = qwc_override ? qwc_override : static_cast<std::uint32_t>((vif.size() - 8) / 16);
    std::vector<std::byte> pkt;
    push_u32(pkt, (dma_id << 28) | (qwc & 0xffffu));
    push_u32(pkt, 0);
    pkt.insert(pkt.end(), vif.begin(), vif.end());
    return pkt;
}

void run_unit_tests() {
    if constexpr (sizeof(std::size_t) > 4) {
        const auto wide_end = ssf_geometry_detail::bounded_chunk_end(0x20, 0xfffffff8u, std::size_t(0x100000100ull));
        check(wide_end && *wide_end == std::size_t(0x100000024ull), "64-bit bounded_chunk_end avoids u32 wrap");
        check(!ssf_geometry_detail::bounded_chunk_end(0x20, 0xfffffff8u, std::size_t(0x100000023ull)),
              "64-bit bounded_chunk_end rejects 1-byte overflow");
    }
    RetailSsfGeometryFailure f;
    // 1. Valid synthetic 8-vertex strip with ADC break at indices 4 and 5 -> 2 + 2 = 4 triangles.
    {
        auto pkt = build_synthetic_vif_packet(6u, 0u, 0u, false, false);
        auto clump = make_synthetic_native_clump(pkt);
        auto dec = decode_retail_ssf_geometry(clump, {0, 0, 0}, f);
        check(dec.has_value() && dec->is_native_ps2_vif, "synthetic native VIF clump decoded");
        check(dec->vertices.size() == 8 && dec->adc_strip_breaks == 2 && dec->triangles.size() == 4,
              "synthetic ADC strip break produced 4 triangles without bridging");
    }
    // 2. Truncated / OOB packet size rejected fail-closed.
    {
        auto pkt = build_synthetic_vif_packet(6u, 0u, 0u, false, false);
        auto clump = make_synthetic_native_clump(pkt, 99999u);
        check(!decode_retail_ssf_geometry(clump, {0, 0, 0}, f) && f.kind == RetailSsfGeometryError::vif_bounds,
              "OOB VIF packet size rejected fail-closed");
    }
    // 3. DMA QWC OOB rejected fail-closed.
    {
        auto pkt = build_synthetic_vif_packet(6u, 500u, 0u, false, false);
        auto clump = make_synthetic_native_clump(pkt);
        check(!decode_retail_ssf_geometry(clump, {0, 0, 0}, f) && f.kind == RetailSsfGeometryError::dma_chain,
              "OOB DMA QWC rejected fail-closed");
    }
    // 4. Unsupported DMA tag ID (ref=3) rejected fail-closed.
    {
        auto pkt = build_synthetic_vif_packet(3u, 0u, 0u, false, false);
        auto clump = make_synthetic_native_clump(pkt);
        check(!decode_retail_ssf_geometry(clump, {0, 0, 0}, f) && f.kind == RetailSsfGeometryError::dma_chain,
              "unsupported DMA tag ID rejected fail-closed");
    }
    // 5. Unsupported VIF opcode (0x25) rejected fail-closed.
    {
        auto pkt = build_synthetic_vif_packet(6u, 0u, 0x25000000u, false, false);
        auto clump = make_synthetic_native_clump(pkt);
        check(!decode_retail_ssf_geometry(clump, {0, 0, 0}, f) && f.kind == RetailSsfGeometryError::unsupported_vif_opcode,
              "unsupported VIF opcode rejected fail-closed");
    }
    // 6. Unsupported UNPACK format (0x6d) rejected fail-closed.
    {
        auto pkt = build_synthetic_vif_packet(6u, 0u, 0u, true, false);
        auto clump = make_synthetic_native_clump(pkt);
        check(!decode_retail_ssf_geometry(clump, {0, 0, 0}, f) && f.kind == RetailSsfGeometryError::unsupported_unpack_format,
              "unsupported UNPACK format rejected fail-closed");
    }
    // 7. Truncated UNPACK payload rejected fail-closed.
    {
        auto pkt = build_synthetic_vif_packet(6u, 0u, 0u, false, true);
        auto clump = make_synthetic_native_clump(pkt);
        check(!decode_retail_ssf_geometry(clump, {0, 0, 0}, f) && f.kind == RetailSsfGeometryError::vif_bounds,
              "truncated UNPACK payload rejected fail-closed");
    }
}

struct Fixture {
    NativeRetailStartupState startup;
    mka::input::ControllerInput input;
    std::unique_ptr<NativeRetailMainState> main_state;
    std::shared_ptr<NativeRetailRendererCore> renderer;
    RetailRendererCoreFailure f;
    explicit Fixture(const std::filesystem::path& root) : startup([&] {
        mka::content::ContentVolumeFailure vf;
        auto v = mka::content::NativeContentVolume::open(root, vf);
        check(bool(v), "open original content volume");
        return std::move(*v);
    }(), {}) {
        StartupFailure sf;
        check(initialize_native_retail_game_session(startup, sf) && initialize_retail_memory_owners(startup, sf),
              "initialize Session and memory owners");
        main_state = std::make_unique<NativeRetailMainState>(startup, input);
        RetailRendererAllocatorFailure af;
        auto allocator = NativeRetailRendererAllocatorService::create(*startup.memory_owners, af);
        check(bool(allocator), "create renderer allocator");
        renderer = NativeRetailRendererCore::for_session(startup.game_session, allocator, startup.content, f);
        check(bool(renderer), "create renderer core");
        auto initialized = renderer->initialize_core(f);
        check(initialized && *initialized, "initialize renderer core");
        auto* cameras = startup.game_session->render_cameras(sf.game);
        RetailCameraWorldFailure cf;
        check(cameras && cameras->store_initial_count_from_127260(cf), "admit 127260 camera count");
    }
};

void run_mkda_pak_census(const std::filesystem::path& root) {
    mka::content::ContentVolumeFailure vf;
    auto vol = mka::content::NativeContentVolume::open(root, vf);
    check(bool(vol), "open volume for census");
    auto pak_file = vol->open_resource("mkda.pak", vf);
    check(bool(pak_file), "open mkda.pak for census");
    RetailArchiveFailure af;
    auto archive = NativeRetailArchive::open(std::move(*pak_file), af);
    check(bool(archive), "open NativeRetailArchive");

    std::size_t total_clumps = 0, native_clumps = 0, non_native_clumps = 0;
    std::size_t decoded_ok = 0, refused_skinned = 0, refused_unpack = 0, refused_vif = 0, refused_other = 0;

    for (std::size_t mi = 0; mi < archive->member_count(); ++mi) {
        RetailSsfBundleFailure bf;
        auto bundle = NativeRetailSsfBundle::open(*archive, mi, bf);
        if (!bundle) continue;
        for (std::size_t si = 0; si < bundle->sections().size(); ++si) {
            if (bundle->sections()[si].raw_type != 1) continue;
            auto sec = bundle->open_art_section(si, bf);
            if (!sec) continue;
            for (std::size_t ri = 0; ri < sec->records().size(); ++ri) {
                if ((sec->records()[ri].raw_type & 0x3fffffffu) != 4u) continue;
                auto clump = sec->clump(ri, bf);
                if (!clump) continue;
                ++total_clumps;
                RetailSsfGeometryFailure gf;
                auto dec = decode_retail_ssf_geometry(
                    clump->rw_bytes(),
                    {0, 0, std::uint64_t(clump->resource().record().payload_offset) + 8},
                    gf);
                if (dec) {
                    if (dec->is_native_ps2_vif) {
                        ++native_clumps;
                        ++decoded_ok;
                    } else {
                        ++non_native_clumps;
                    }
                } else {
                    if (gf.kind == RetailSsfGeometryError::skinned_native_pipe_unsupported) {
                        ++native_clumps;
                        ++refused_skinned;
                    } else if (gf.kind == RetailSsfGeometryError::unsupported_unpack_format) {
                        ++native_clumps;
                        ++refused_unpack;
                    } else if (gf.kind == RetailSsfGeometryError::unsupported_vif_opcode) {
                        ++native_clumps;
                        ++refused_vif;
                    } else {
                        ++refused_other;
                    }
                }
            }
        }
    }
    std::printf("SSF_NATIVE_CENSUS total_clumps=%zu non_native_clumps=%zu native_clumps=%zu "
                "decoded_ok=%zu refused_skinned=%zu refused_unpack=%zu refused_vif=%zu refused_other=%zu\n",
                total_clumps, non_native_clumps, native_clumps,
                decoded_ok, refused_skinned, refused_unpack, refused_vif, refused_other);
    check(total_clumps == 3774, "census reached all 3774 bounded clumps");
    check(decoded_ok >= 150, "census decoded >= 150 native-marked clumps cleanly");
}

} // namespace

int main(int argc, char** argv) try {
    check(argc == 3, "usage: <original_pak_root> <fresh_private_output>");
    const std::filesystem::path original(argv[1]), output(argv[2]);
    std::filesystem::create_directories(output);

    run_unit_tests();
    run_mkda_pak_census(original);

#ifdef _WIN32
    // Authentic Armory rpGEOMETRYNATIVE GPU test through 225D40 -> Main -> 125D60 reset.
    DisplayServiceFailure df;
    auto display = NativeOffscreenDisplayService::create(64, 48, df);
    check(display != nullptr && display->acquire_context(df), "acquire hidden GL context");

    // 1. Allocation-failure rollback test via load_retail_ssf_scene_from_archive
    {
        Fixture x(original);
        auto& core = *x.renderer;
        check(core.construct_registered(x.f),
              "construct core plugins for rollback check");
        auto frames = core.frames(x.f);
        auto materials = core.materials(x.f);
        auto textures = core.textures(x.f);
        auto rasters = core.rasters(x.f);
        auto worlds = core.world_module(x.f);
        RetailRendererWorldFailure wf;
        auto wh = worlds->create_world(wf);
        auto world = wh ? worlds->resolve_world(*wh, wf) : nullptr;
        check(bool(world), "create world for rollback check");
        RetailSceneResources res(rasters, textures, materials, frames, world);

        RetailSsfSceneSelection sel;
        sel.archive_resource_key = "mkda.pak";
        sel.member.index = 0; // armory.ssf
        sel.section_index = 0;
        sel.clump_resource.name = "ARMORY_ROCKS";
        sel.atomic_index = 0;
        sel.morph_index = 0;
        sel.texture_resource_indices = {73}; // A_rocks in ARMORY_ROCKS range
        sel.render_policy = RetailSsfSceneRenderPolicy::diagnostic_native_vif_unlit_raw_rgba8;

        RetailSceneArchiveFailure saf;
        fail_after = 15;
        auto oom = load_retail_ssf_scene_from_archive(
            x.startup.content, sel, 1, *rasters, *textures, *materials, *frames, *world, {}, saf, res);
        fail_after = -1;
        if (oom || !res.empty() || !world->scene_objects().empty()) {
            std::fprintf(stderr, "debug oom=%d kind=%u op=%s empty=%d\n",
                         int(bool(oom)), unsigned(saf.kind), saf.operation, int(res.empty()));
        }
        check(!oom && res.empty() && world->scene_objects().empty(),
              "allocation failure rolled back Armory scene with empty ownership journal");
    }

    // 2. Full 225D40 -> Main -> 125D60 GPU path on Armory ARMORY_ROCKS
    {
        Fixture x(original);
        x.main_state->scene = std::make_shared<NativeRetailSceneOwners>();
        x.main_state->scene->config.viewport_width = 64;
        x.main_state->scene->config.viewport_height = 48;
        x.main_state->scene->config.near_clip = 0.01f;
        x.main_state->scene->config.far_clip = 2000.0f;
        x.main_state->scene->config.display = display;
        x.main_state->scene->config.call_show_raster = false;

        RetailSsfSceneSelection sel;
        sel.archive_resource_key = "mkda.pak";
        sel.member.index = 0;
        sel.section_index = 0;
        sel.clump_resource.name = "ARMORY_ROCKS";
        sel.atomic_index = 0;
        sel.morph_index = 0;
        sel.texture_resource_indices = {73};
        sel.render_policy = RetailSsfSceneRenderPolicy::diagnostic_native_vif_unlit_raw_rgba8;
        x.main_state->scene->config.ssf_scene = sel;

        StartupFailure sf;
        auto gate = scene_bootstrap_required::continue_first_scene_at_225d40(*x.main_state, x.renderer, sf);
        if (gate != RetailStartupGate::ready) {
            std::fprintf(stderr, "Armory 225D40 failed: %s\n", sf.operation ? sf.operation : "(null)");
        }
        check(gate == RetailStartupGate::ready && x.main_state->scene->scene_loaded &&
              x.main_state->scene->ssf_data.has_value() && x.main_state->scene->ssf_data->geometry.is_native_ps2_vif,
              "225D40 loaded authentic Armory rpGEOMETRYNATIVE clump");

        const auto& g = x.main_state->scene->ssf_data->geometry;
        check(!g.vertices.empty() && !g.triangles.empty(), "Armory geometry has non-zero VIF vertices and triangles");
        // Transform bound_center through the atomic's frame ancestry so camera looks directly at the rendered object.
        RetailRendererPoint3 world_center = g.bound_center;
        for (std::int32_t fi = static_cast<std::int32_t>(g.frame_index);
             fi >= 0 && static_cast<std::size_t>(fi) < g.frames.size();
             fi = g.frames[static_cast<std::size_t>(fi)].parent) {
            const auto& m = g.frames[static_cast<std::size_t>(fi)].modelling;
            world_center = {
                m.right.x * world_center.x + m.up.x * world_center.y + m.at.x * world_center.z + m.translation.x,
                m.right.y * world_center.x + m.up.y * world_center.y + m.at.y * world_center.z + m.translation.y,
                m.right.z * world_center.x + m.up.z * world_center.y + m.at.z * world_center.z + m.translation.z
            };
        }
        const float dist = std::max(0.08f, g.bound_radius * 2.0f);
        std::printf("ARMORY_CAMERA world_center=(%.2f, %.2f, %.2f) radius=%.2f dist=%.2f v0=(%.2f, %.2f, %.2f)\n",
                    world_center.x, world_center.y, world_center.z, g.bound_radius, dist,
                    g.vertices[0].position.x, g.vertices[0].position.y, g.vertices[0].position.z);
        RetailRendererFrameFailure ff;
        RetailRendererMatrixValue cam_mod{
            {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}, {world_center.x, world_center.y, world_center.z - dist}},
            0x20000u};
        check(x.main_state->scene->frames->set_modelling(x.main_state->scene->camera_init->primary.frame, cam_mod, ff),
              "position camera facing Armory bound_center");

        auto re = main_binding::retail_render_enabled(*x.main_state, sf);
        check(re && *re, "retail_render_enabled true for Armory");
        check(main_binding::render_retail_frame(*x.main_state, sf) && x.main_state->scene->frames_rendered == 1,
              "render_retail_frame dispatched render_world_scene_frame for Armory");

        const auto& cache = x.main_state->scene->world->gpu_texture_cache();
        check(cache.size() == 1 && cache[0].ready, "Armory A_rocks texture uploaded to GPU");
        const GLuint tex_id = static_cast<GLuint>(cache[0].gl_texture_id);
        check(glIsTexture(tex_id) == GL_TRUE, "Armory GL texture live");

        RetailRendererCameraFailure rcf;
        check(x.main_state->scene->cameras->begin_update(x.main_state->scene->camera_init->primary.camera, rcf),
              "rebind Armory FBO for readback");
        std::vector<std::uint8_t> rgba(64 * 48 * 4);
        std::vector<float> depths(64 * 48);
        glReadPixels(0, 0, 64, 48, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glReadPixels(0, 0, 64, 48, GL_DEPTH_COMPONENT, GL_FLOAT, depths.data());
        display->record_diagnostic_readback(2);
        check(x.main_state->scene->cameras->end_update(x.main_state->scene->camera_init->primary.camera, rcf),
              "end Armory readback");

        std::size_t drawn = 0;
        for (std::size_t i = 0; i < depths.size(); ++i) {
            if (depths[i] < 0.9999f && (rgba[i * 4 + 0] != 13 || rgba[i * 4 + 1] != 20 || rgba[i * 4 + 2] != 36)) {
                ++drawn;
            }
        }
        std::printf("ARMORY_GPU_PIXELS drawn=%zu verts=%zu tris=%zu adc_breaks=%zu\n",
                    drawn, g.vertices.size(), g.triangles.size(), g.adc_strip_breaks);
        check(drawn >= 20, "Armory rpGEOMETRYNATIVE rendered measurable non-background depth-written pixels");

        check(scene_bootstrap_required::continue_scene_reset_at_125d60(*x.main_state, sf),
              "125D60 reset succeeded");
        check(glIsTexture(tex_id) == GL_FALSE && glGetError() == GL_NO_ERROR,
              "125D60 reset deleted GL texture with zero leaks/errors");
        auto re_after = main_binding::retail_render_enabled(*x.main_state, sf);
        check(re_after && !*re_after, "retail_render_enabled false after 125D60 reset");
    }
    (void)display->release_context(df);
#endif

    std::printf("PASS: test_retail_ssf_native_geometry checks=%zu\n", g_checks);
    return 0;
} catch (const std::exception& e) {
    std::fprintf(stderr, "EXCEPTION: %s\n", e.what());
    return 1;
}
