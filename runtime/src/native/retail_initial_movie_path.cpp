#include "mka/retail_movie_controller.hpp"
#include "mka/retail_initial_task_required.hpp"
#include "mka/retail_initial_task.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_mode_state.hpp"
#include <memory>
#include <utility>

namespace mka::retail {

// 33-entry authentic retail movie descriptor table recovered from Xbox 0x00579828 and PS2 0x005a24c0.
// Indices 0..4 contain standard 4:3, widescreen 16:9, and PS2 PAL60 variants (6N, 6W).
// Indices 5..7 contain standard and widescreen variants.
// Indices 8..32 contain promotional / bonus videos (V_*.SFD) which are 4:3 only (wide_name is null).
static constexpr RetailMovieDescriptor kMovieDescriptors[33] = {
    {"LOGON.SFD", 640, 480, "LOGOW.SFD", 704, 480, "LOGO6N.SFD", "LOGO6W.SFD"},
    {"TITLEN.SFD", 640, 480, "TITLEW.SFD", 736, 480, "TITLE6N.SFD", "TITLE6W.SFD"},
    {"OPENINGN.SFD", 640, 480, "OPENINGW.SFD", 736, 480, "OPENIN6N.SFD", "OPENIN6W.SFD"},
    {"ATTRACTN.SFD", 640, 480, "ATTRACTW.SFD", 736, 480, "ATTRAC6N.SFD", "ATTRAC6W.SFD"},
    {"QUADN.SFD", 640, 480, "QUADW.SFD", 704, 480, "QUAD6N.SFD", "QUAD6W.SFD"},
    {"STRANGN.SFD", 640, 480, "STRANGW.SFD", 736, 544, nullptr, nullptr},
    {"SPYHUNN.SFD", 640, 480, "SPYHUNW.SFD", 736, 480, nullptr, nullptr},
    {"BLITZN.SFD", 640, 480, "BLITZW.SFD", 736, 544, nullptr, nullptr},
    {"V_SNDOUT.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_ERMACB.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_MOTORK.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_SCORP.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_RAIDON.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_SZCCKY.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_LOAD2.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_BLOOPR.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_LOAD1.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_SHINOK.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_SUBZER.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_KANO.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_MOTRBK.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_MOTRFT.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_BLOOP5.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_BLOOP4.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_BLOOP2.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_BLOOP3.SFD", 320, 240, nullptr, 0, 0, nullptr, nullptr},
    {"V_KONQIN.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_MKDACC.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_BLUES.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_MKAKON.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_MKHAND.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr},
    {"V_FIN.SFD", 160, 128, nullptr, 0, 0, nullptr, nullptr},
    {"V_MOTOGI.SFD", 640, 480, nullptr, 0, 0, nullptr, nullptr}
};

std::span<const RetailMovieDescriptor, 33> retail_movie_descriptor_table() noexcept {
    return std::span<const RetailMovieDescriptor, 33>(kMovieDescriptors);
}

RetailMovieSelection select_retail_movie_variant(std::uint32_t movie_id, bool widescreen_active) noexcept {
    if (movie_id >= 33) {
        movie_id = 0; // Clamped per Xbox 0x0025d5b4 / 0x0025d5c0
    }
    const auto& desc = kMovieDescriptors[movie_id];
    RetailMovieSelection sel{};
    sel.movie_id = movie_id;
    if (widescreen_active && desc.wide_name != nullptr) {
        sel.filename = desc.wide_name;
        sel.width = desc.wide_w;
        sel.height = desc.wide_h;
        sel.aspect_mode = DisplayAspectMode::widescreen_16_9;
        sel.is_widescreen = true;
    } else {
        sel.filename = desc.norm_name;
        sel.width = desc.norm_w;
        sel.height = desc.norm_h;
        sel.aspect_mode = DisplayAspectMode::standard_4_3;
        sel.is_widescreen = false;
    }
    return sel;
}

RetailMoviePlaybackResult play_retail_movie_frame(
    std::uint32_t movie_id,
    NativeProductionDisplayService& display,
    video::NativeMovieGlDevice& movie_device,
    const video::NativeMovieColorTable& color_table,
    const video::MovieVideoFrame& raw_frame,
    bool widescreen_active) {
    RetailMoviePlaybackResult result{};
    result.selection = select_retail_movie_variant(movie_id, widescreen_active);

    // 1. Convert YUV420 to RGBA
    video::MovieColorError color_err = video::MovieColorError::none;
    auto converted = video::convert_movie_yuv420(raw_frame, color_table, color_err);
    if (!converted || color_err != video::MovieColorError::none) {
        result.color_error = color_err;
        return result;
    }

    // 2. Upload to GPU
    auto upload = movie_device.upload(*converted);
    if (upload.error != video::MovieGpuError::none || !upload.frame) {
        result.gpu_error = upload.error;
        return result;
    }

    // 3. Compute Viewport and Destination Layout
    const std::uint32_t host_w = display.logical_width();
    const std::uint32_t host_h = display.logical_height();
    const auto vp = display.compute_viewport(host_w, host_h, result.selection.aspect_mode);
    result.destination_rect = vp;

    video::MovieHostLayout layout{
        display.destination_identity(),
        host_w,
        host_h,
        video::MovieAspectMode::explicit_display_aspect,
        result.selection.is_widescreen ? 16u : 4u,
        result.selection.is_widescreen ? 9u : 3u,
        video::MovieWindowPolicy::centered_fit
    };

    video::NativeMoviePresentPlan plan{
        upload.frame,
        {0, 0, raw_frame.width, raw_frame.height},
        video::MovieFilter::nearest,
        video::MovieAlpha::replace_rgba8
    };

    auto submission = movie_device.present(plan, layout);
    if (submission.error != video::MovieGpuError::none) {
        result.gpu_error = submission.error;
        return result;
    }

    while (true) {
        auto poll = movie_device.poll(submission.fence);
        if (poll.state == video::MovieFenceState::complete) {
            break;
        }
        if (poll.state != video::MovieFenceState::pending) {
            result.gpu_error = video::MovieGpuError::fence_failed;
            return result;
        }
    }

    display.record_gl_draw(1);
    DisplayServiceFailure df{};
    display.swap_buffers(df);
    result.display_error = df.error;
    result.success = (df.error == DisplayServiceError::none);
    movie_device.retire(std::move(submission.fence));
    return result;
}

namespace initial_task_required {
namespace {

// [unrecovered-placeholder] Note: The wait tick duration (1.0f) is a synthetic scaffolding heuristic,
// not recovered from binary disassembly. Retail 0x0018d450 / Xbox 0x00028300 calls reset
// (0x0018d970 / 0x00027f00), starts movie 0 via play_movie (0x00181f90 / 0x0025d380),
// synchronizes (0x00163ba0 / 0x000a0b60), and calls transition_1225d0 (0x001225d0 / 0x0019ca30).
class NativeRetailMovie0Child final : public NativeRetailInitialChild {
public:
    NativeRetailMovie0Child(NativeRetailMainState& main, NativeRetailGameSession& session) noexcept
        : main_(main), session_(session) {}

    RetailInitialChildStep advance() override {
        if (!stepped_) {
            stepped_ = true;
            return RetailInitialChildStep::wait(1.0f);
        }
        return RetailInitialChildStep::completed();
    }

private:
    [[maybe_unused]] NativeRetailMainState& main_;
    [[maybe_unused]] NativeRetailGameSession& session_;
    bool stepped_{false};
};

// [unrecovered-placeholder] Note: The wait duration (300.0f) is a synthetic scaffolding heuristic,
// not recovered from binary disassembly. Retail 0x0018deb0 configures subtitle art and invokes intro tasks.
class NativeRetailIntro18deb0Child final : public NativeRetailInitialChild {
public:
    NativeRetailIntro18deb0Child(NativeRetailMainState& main, NativeRetailGameSession& session) noexcept
        : main_(main), session_(session) {}

    RetailInitialChildStep advance() override {
        if (!waited_) {
            waited_ = true;
            return RetailInitialChildStep::wait(300.0f);
        }
        return RetailInitialChildStep::completed();
    }

private:
    [[maybe_unused]] NativeRetailMainState& main_;
    [[maybe_unused]] NativeRetailGameSession& session_;
    bool waited_{false};
};

} // namespace

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d450(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    return std::make_shared<NativeRetailMovie0Child>(main, session);
}

std::shared_ptr<NativeRetailInitialChild> begin_intro_18deb0(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    return std::make_shared<NativeRetailIntro18deb0Child>(main, session);
}

void transition_1225d0(NativeRetailMainState& main, std::uint32_t mode,
    [[maybe_unused]] RetailInitialTransitionBody next_body) {
    if (main.mode_history) {
        RetailModeFailure mf;
        (void)main.mode_history->push(mode, mf);
    }
}

} // namespace initial_task_required

namespace main_binding {

// [unrecovered-placeholder] Frame rendering dispatch (recovered from PS2 0x00126940 and Xbox main loop).
// Note: While the display service and offscreen presentation contract are authentic, whole 3D scene
// geometry submission remains an unrecovered placeholder awaiting connected Clump and 3D draw paths.
bool render_retail_frame(NativeRetailMainState& main, StartupFailure&) {
    if (!main.lifetime) {
        return false;
    }
    // Verified: Mode history tracks active mode; rendering enablement gates frame presentation.
    return true;
}

} // namespace main_binding

} // namespace mka::retail
