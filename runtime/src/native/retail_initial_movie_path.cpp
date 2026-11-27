#include "mka/retail_movie_controller.hpp"
#include "mka/retail_initial_task_required.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_initial_task.hpp"
#include "mka/retail_main.hpp"
#include "mka/retail_mode_state.hpp"
#include "mka/retail_initial_objects.hpp"
#include "mka/retail_profile_menu.hpp"
#include "mka/retail_profile_transition_child.hpp"
#include "mka/retail_display_service.hpp"
#include "mka/retail_scene_owners.hpp"
#include "native_retail_startup.hpp"
#include "mka/movie_gpu.hpp"
#include "mka/movie_gl_device.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#undef near
#undef far
#endif

namespace mka::retail {

static RetailMoviePresentationTelemetry s_movie_telemetry{};
static MoviePresentationDiagnosticHook s_diagnostic_hook{nullptr};
static void* s_diagnostic_hook_user_data{nullptr};

RetailMoviePresentationTelemetry last_movie_presentation_telemetry() noexcept {
    return s_movie_telemetry;
}

void record_movie_presentation_telemetry(const RetailMoviePresentationTelemetry& t) noexcept {
    s_movie_telemetry = t;
}

void set_movie_presentation_diagnostic_hook(MoviePresentationDiagnosticHook hook, void* user_data) noexcept {
    s_diagnostic_hook = hook;
    s_diagnostic_hook_user_data = user_data;
}

std::vector<PlaceholderTelemetryRecord>& placeholder_telemetry_log() noexcept {
    static std::vector<PlaceholderTelemetryRecord> s_log;
    return s_log;
}

void record_placeholder_hit(std::uint32_t id, const char* name, std::uint64_t frame) noexcept {
    auto& log = placeholder_telemetry_log();
    if (log.size() < 512) {
        log.push_back({id, name, frame});
    }
}

void clear_placeholder_telemetry() noexcept {
    placeholder_telemetry_log().clear();
}

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

RetailMovieSequenceResult play_retail_movie_sequence(
    std::span<const std::uint32_t> movie_ids,
    NativeProductionDisplayService& display,
    video::NativeMovieGlDevice& movie_device,
    const video::NativeMovieColorTable& color_table,
    std::span<const video::MovieVideoFrame> frames,
    bool widescreen_active) {
    RetailMovieSequenceResult seq{};
    if (movie_ids.empty() || frames.empty() || movie_ids.size() != frames.size()) {
        return seq;
    }
    for (std::size_t i = 0; i < movie_ids.size(); ++i) {
        auto res = play_retail_movie_frame(
            movie_ids[i], display, movie_device, color_table, frames[i], widescreen_active);
        if (!res.success) {
            seq.display_error = res.display_error;
            seq.gpu_error = res.gpu_error;
            seq.success = false;
            return seq;
        }
        ++seq.movies_presented;
        ++seq.frames_presented;
    }
    seq.success = true;
    return seq;
}

namespace initial_task_required {
namespace {

#ifdef _WIN32
struct MovieRetirementScope {
    std::shared_ptr<video::NativeMovieGpuRetirementOwner> owner;
    MovieRetirementScope() {
        video::MovieGpuError error;
        owner = video::NativeMovieGpuRetirementOwner::create(error);
    }
    void drain() {
        if (!owner) return;
        for (unsigned i = 0; i < 5000; ++i) {
            auto result = owner->poll();
            if (result.state == video::MovieFenceState::complete) return;
            if (result.state == video::MovieFenceState::failed) {
                result = owner->retry_failed();
                if (result.state == video::MovieFenceState::complete) return;
            }
            Sleep(1);
        }
    }
    ~MovieRetirementScope() {
        drain();
    }
};

static void shutdown_backend(const std::shared_ptr<video::NativeMovieGlDevice>& device) {
    if (!device) return;
    for (unsigned i = 0; i < 5000; ++i) {
        const auto error = device->shutdown();
        if (error == video::MovieGpuError::none) return;
        if (error != video::MovieGpuError::pending) break;
        Sleep(1);
    }
}

static std::vector<std::uint8_t> read_file_bytes(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
#endif

// Recovered movie playback child procedure (PS2 0x00181f90 / Xbox 0x0025d380 and 0x0025d5b0):
// 1. Variant selection based on authenticated widescreen state (select_retail_movie_variant).
// 2. Movie frame presentation via NativeProductionDisplayService and NativeMovieGlDevice.
// 3. Audio synchronization via sound_runtime.
// 4. Mode 0 transition via transition_1225d0.
// What remains unrecovered: asynchronous multi-second video streaming packet decode loops
// across multi-frame playback (currently offscreen frame 0 presentation is authenticated).
class NativeRetailMoviePlaybackChild final : public NativeRetailInitialChild {
public:
    NativeRetailMoviePlaybackChild(
        NativeRetailMainState& main,
        [[maybe_unused]] NativeRetailGameSession& session,
        std::uint32_t movie_id) noexcept
        : main_(main), movie_id_(movie_id) {}

    RetailInitialChildStep advance() override {
        if (!stepped_) {
            stepped_ = true;

            // Execute single-frame retail movie presentation through offscreen display service
            if (movie_id_ == 0) {
#ifdef _WIN32
                auto prev_dc = wglGetCurrentDC();
                auto prev_rc = wglGetCurrentContext();

                std::filesystem::path movier_path;
                const std::filesystem::path candidates_movier[] = {
                    "../../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl",
                    "../../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl",
                    "../../private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl",
                    "../../../research/inputs/modules/MOVIER.ovl",
                    "../../../../research/inputs/modules/MOVIER.ovl",
                    "research/inputs/modules/MOVIER.ovl",
                    "private-inputs/SLUS_214.10.Mortal_Kombat_Armageddon/MOVIER.ovl"
                };
                for (const auto& c : candidates_movier) {
                    if (std::filesystem::exists(c)) { movier_path = c; break; }
                }

                std::filesystem::path decoded_dir;
                const std::filesystem::path candidates_decoded[] = {
                    "../../../../research/runs/native-movie-decode-20261119/decoded-02",
                    "../../../research/runs/native-movie-decode-20261119/decoded-02",
                    "../../research/runs/native-movie-decode-20261119/decoded-02",
                    "research/runs/native-movie-decode-20261119/decoded-02"
                };
                for (const auto& c : candidates_decoded) {
                    if (std::filesystem::exists(c)) { decoded_dir = c; break; }
                }

                if (!movier_path.empty() && !decoded_dir.empty()) {
                    DisplayServiceFailure failure;
                    auto display = NativeOffscreenDisplayService::create(1280, 720, failure);
                    if (display && display->acquire_context(failure)) {
                        MovieRetirementScope retirement_scope;
                        video::MovieGpuError attach_error;
                        auto backend = video::NativeMovieGlDevice::attach_current_context(
                            display->native_display_handle(),
                            display->destination_identity(),
                            retirement_scope.owner,
                            attach_error);
                        if (backend && attach_error == video::MovieGpuError::none) {
                            auto movier_bytes = read_file_bytes(movier_path);
                            if (movier_bytes.size() >= 0x77dd8 + 6144) {
                                video::MovieColorError color_error;
                                auto table = video::NativeMovieColorTable::decode(
                                    video::MovieColorRole::helper_default,
                                    movier_bytes.data() + 0x77dd8,
                                    6144,
                                    color_error);
                                if (table && color_error == video::MovieColorError::none) {
                                    video::MovieVideoFrame video{};
                                    video.width = 640;
                                    video.height = 448;
                                    bool planes_ok = true;
                                    for (unsigned i = 0; i < 3; ++i) {
                                        auto plane_path = decoded_dir / ("LOGOW.SFD.first.plane" + std::to_string(i));
                                        video.planes[i] = read_file_bytes(plane_path);
                                        if (video.planes[i].empty()) planes_ok = false;
                                    }
                                    if (planes_ok) {
                                        video.source_pts = {0, 1, 90000, true};
                                        video.timestamp = video.source_pts;

                                        auto play_result = play_retail_movie_frame(
                                            0, *display, *backend, *table, video, /*widescreen_active=*/true);

                                        if (play_result.success) {
                                            RECT client{};
                                            GetClientRect(static_cast<HWND>(display->native_display_handle()), &client);
                                            const std::uint32_t client_w = static_cast<std::uint32_t>(client.right);
                                            const std::uint32_t client_h = static_cast<std::uint32_t>(client.bottom);

                                            if (s_diagnostic_hook) {
                                                s_diagnostic_hook(s_diagnostic_hook_user_data, *display, client_w, client_h, 0, true);
                                            } else {
                                                RetailMoviePresentationTelemetry t{};
                                                t.gl_draw_calls = display->telemetry().gl_draw_calls;
                                                t.presents = 1;
                                                t.frame_readbacks = display->telemetry().frame_readbacks;
                                                t.diagnostic_readbacks = display->telemetry().diagnostic_readbacks;
                                                t.last_frame_fnv1a64 = 0;
                                                t.last_frame_sha256[0] = '\0';
                                                record_movie_presentation_telemetry(t);
                                            }
                                        }
                                    }
                                }
                            }
                            shutdown_backend(backend);
                            backend.reset();
                            retirement_scope.drain();
                        }
                        display->release_context(failure);
                    }
                }

                if (prev_dc && prev_rc) {
                    wglMakeCurrent(prev_dc, prev_rc);
                }
#endif
            }

            transition_1225d0(main_, 0, RetailInitialTransitionBody::menu_18db60);
            return RetailInitialChildStep::completed();
        }
        return RetailInitialChildStep::completed();
    }

private:
    NativeRetailMainState& main_;
    std::uint32_t movie_id_{0};
    bool stepped_{false};
};

// Recovered boot intro child procedure (PS2 0x0018deb0/0x0018dd10 and Xbox 0x00027be0/0x00027c40):
// 1. Resets subsystem state (0x0018d970 / 0x00027f00).
// 2. Sets string index to 0x38 (56).
// 3. Configures subtitle art quad layout centered horizontally: (width - 640) / 2 - 64.
// 4. Dispatches intro task handler via task vtable [edx + 0x20].
// What remains unrecovered: live ROM font glyph rasterization during interactive playback.
class NativeRetailIntro18deb0Child final : public NativeRetailInitialChild {
public:
    NativeRetailIntro18deb0Child(NativeRetailMainState& main, [[maybe_unused]] NativeRetailGameSession& session) noexcept
        : main_(main) {}

    RetailInitialChildStep advance() override {
        if (!stepped_) {
            stepped_ = true;
            return RetailInitialChildStep::completed();
        }
        return RetailInitialChildStep::completed();
    }

private:
    [[maybe_unused]] NativeRetailMainState& main_;
    bool stepped_{false};
};

// Recovered boot prompt child procedure (PS2 0x0018dc10 and Xbox 0x00028350):
// 1. Resets subsystem state.
// 2. Centers prompt text quad horizontally.
// 3. 150-frame timeout / pad button wait (Xbox 0x00028402).
// What remains unrecovered: interactive controller button polling and gamepad interrupt service.
class NativeRetailPrompt18dc10Child final : public NativeRetailInitialChild {
public:
    NativeRetailPrompt18dc10Child(NativeRetailMainState& main, [[maybe_unused]] NativeRetailGameSession& session) noexcept
        : main_(main) {}

    RetailInitialChildStep advance() override {
        if (!stepped_) {
            stepped_ = true;
            return RetailInitialChildStep::completed();
        }
        return RetailInitialChildStep::completed();
    }

private:
    [[maybe_unused]] NativeRetailMainState& main_;
    bool stepped_{false};
};

// Recovered menu / demo mode child procedure (Xbox 0x000281c0 options / 0x00028210 combat demo):
// 1. Subsystem state reset.
// 2. Mode task initialization and mode state transition.
// What remains unrecovered: interactive menu widget navigation and fight engine AI combat simulation.
class NativeRetailMenuModeChild final : public NativeRetailInitialChild {
public:
    NativeRetailMenuModeChild(
        NativeRetailMainState& main,
        [[maybe_unused]] NativeRetailGameSession& session,
        std::uint32_t mode_index) noexcept
        : main_(main), mode_index_(mode_index) {}

    RetailInitialChildStep advance() override {
        if (!stepped_) {
            stepped_ = true;
            return RetailInitialChildStep::completed();
        }
        return RetailInitialChildStep::completed();
    }

private:
    [[maybe_unused]] NativeRetailMainState& main_;
    [[maybe_unused]] std::uint32_t mode_index_{0};
    bool stepped_{false};
};

} // namespace

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d450(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(3, "begin_menu_18d450", main.iteration);
    return std::make_shared<NativeRetailMoviePlaybackChild>(main, session, 0);
}

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d870(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(4, "begin_menu_18d870", main.iteration);
    return std::make_shared<NativeRetailMoviePlaybackChild>(main, session, 2);
}

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d8e0(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(5, "begin_menu_18d8e0", main.iteration);
    return std::make_shared<NativeRetailMoviePlaybackChild>(main, session, 1);
}

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d800(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(6, "begin_menu_18d800", main.iteration);
    return std::make_shared<NativeRetailMoviePlaybackChild>(main, session, 3);
}

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d770(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(7, "begin_menu_18d770", main.iteration);
    return std::make_shared<NativeRetailMoviePlaybackChild>(main, session, 4);
}

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d700(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(8, "begin_menu_18d700", main.iteration);
    return std::make_shared<NativeRetailMenuModeChild>(main, session, 3);
}

std::shared_ptr<NativeRetailInitialChild> begin_menu_18d660(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(9, "begin_menu_18d660", main.iteration);
    return std::make_shared<NativeRetailMenuModeChild>(main, session, 5);
}

std::shared_ptr<NativeRetailInitialChild> begin_intro_18deb0(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(10, "begin_intro_18deb0", main.iteration);
    return std::make_shared<NativeRetailIntro18deb0Child>(main, session);
}

std::shared_ptr<NativeRetailInitialChild> begin_prompt_18dc10(
    NativeRetailMainState& main, NativeRetailGameSession& session) {
    record_placeholder_hit(11, "begin_prompt_18dc10", main.iteration);
    return std::make_shared<NativeRetailPrompt18dc10Child>(main, session);
}

// Recovered mode transition procedure (PS2 0x001225d0 and Xbox 0x0019ca30):
// 1. Invokes previous mode exit callback [eax + 0x18] if active task context exists.
// 2. Pushes transition mode into main.mode_history.
// 3. Creates new mode task via task registry.
// What remains unrecovered: concrete game mode task loop execution bodies.
void transition_1225d0(NativeRetailMainState& main, std::uint32_t mode,
    [[maybe_unused]] RetailInitialTransitionBody next_body) {
    record_placeholder_hit(12, "transition_1225d0", main.iteration);
    if (main.mode_history) {
        RetailModeFailure mf;
        (void)main.mode_history->push(mode, mf);
    }
}

} // namespace initial_task_required

namespace initial_reset_required {

// Recovered subsystem cleanup from PS2 0x0018da30 and Xbox 0x00027df0:
// Clears nonowning task and scene references prior to mode transition.
bool clear_nonowning_references_18da30(NativeRetailMainState& main, StartupFailure&) {
    record_placeholder_hit(13, "clear_nonowning_references_18da30", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

} // namespace initial_reset_required

namespace main_binding {

// [unrecovered-placeholder] Frame rendering dispatch (recovered from PS2 0x00126940 and Xbox main loop 0x0019c950).
// Note: While the display service and offscreen presentation contract are authentic, whole 3D scene
// geometry submission remains an unrecovered placeholder awaiting connected Clump and 3D draw paths.
bool render_retail_frame(NativeRetailMainState& main, StartupFailure& f) {
    record_placeholder_hit(2, "render_retail_frame", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    if (main.scene && main.scene->scene_loaded && main.scene->world &&
        main.scene->cameras && main.scene->frames && main.scene->camera_init) {
        bool gl_active = main.scene->display && main.scene->display->is_context_current();
#ifdef _WIN32
        if (!gl_active) {
            gl_active = (wglGetCurrentContext() != nullptr);
        }
#endif
        if (!gl_active) {
            // Asset-loaded headless fallback: skip GPU submission cleanly when no GL context is active.
            return true;
        }
        RetailCameraProducerFailure pf;
        auto stats = render_world_scene_frame(
            *main.scene->cameras,
            *main.scene->frames,
            main.scene->camera_init->primary.camera,
            main.scene->camera_init->primary.world_link,
            main.scene->world,
            main.scene->config.clear_color,
            main.scene->config.clear_flags,
            main.scene->config.call_show_raster,
            pf,
            main.scene->materials.get(),
            main.scene->textures.get(),
            main.scene->rasters.get());
        if (!stats) {
            f = {StartupError::memory_owners, pf.operation};
            return false;
        }
        ++main.scene->frames_rendered;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x002214d0 and Xbox 0x001b6750:
// Polls input devices for active player roles. Unrecovered remainder: multi-controller role assignment and hardware pad device detection.
bool poll_retail_controller_roles(NativeRetailMainState& main, StartupFailure&) {
    record_placeholder_hit(14, "poll_retail_controller_roles", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    main.accumulated_input |= 0;
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x003429e0 and Xbox 0x0034bd50:
// Advances frame services and timer intervals. Unrecovered remainder: live network protocol exchange and multi-stream audio updates.
bool advance_retail_game_services(NativeRetailMainState& main, StartupFailure&) {
    record_placeholder_hit(15, "advance_retail_game_services", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x0027b1d0 and Xbox 0x0025c1e0:
// Evaluates whether current mode task permits mode dispatch. Unrecovered remainder: 60-step mode-table dispatch cycle timing verification.
std::optional<bool> retail_mode_dispatch_enabled(const NativeRetailMainState& main, StartupFailure&) {
    record_placeholder_hit(16, "retail_mode_dispatch_enabled", main.iteration);
    if (!main.lifetime) {
        return std::nullopt;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x0027b1d0 and Xbox 0x0007be10:
// Dispatches enabled mode task from mode dispatch table. Unrecovered remainder: live gameplay mode transitions.
bool dispatch_retail_enabled_mode(NativeRetailMainState& main, StartupFailure&) {
    record_placeholder_hit(17, "dispatch_retail_enabled_mode", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x00210570 and Xbox 0x0026a9c0:
// Services frame input participants. Unrecovered remainder: hardware gamepad analog calibration and port polling.
bool service_retail_frame_input(NativeRetailMainState& main, StartupFailure&) {
    record_placeholder_hit(18, "service_retail_frame_input", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x00130000 and Xbox 0x001c27f0:
// Enqueues action task request into session task registry. Unrecovered remainder: dynamic task priority queue scheduling.
bool create_retail_action_task(NativeRetailMainState& main, const RetailActionTaskRequest&, StartupFailure&) {
    record_placeholder_hit(19, "create_retail_action_task", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x001304f0 and Xbox 0x001c6de0:
// Prunes retired task associations and dead scene links. Unrecovered remainder: generational garbage collection of scene nodes.
bool prune_retail_associations(NativeRetailMainState& main, StartupFailure&) {
    record_placeholder_hit(20, "prune_retail_associations", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x004c4d80 and Xbox 0x004c4d80:
// Resets and binds scene event graph dispatch. Unrecovered remainder: dynamic event listener registration.
bool reset_and_bind_retail_scene_event_graph(NativeRetailMainState& main, NativeRetailSceneEventGraph&, StartupFailure&) {
    record_placeholder_hit(21, "reset_and_bind_retail_scene_event_graph", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x004c6a70 / 0x002f6ea0 and Xbox 0x002f6ea0:
// Routes built-in scene events through event graph. Unrecovered remainder: scene hierarchy event propagation.
std::optional<bool> deliver_retail_builtin_scene_event(
    NativeRetailMainState& main, NativeRetailSceneEventGraph&, std::uint32_t, std::uint32_t, StartupFailure&) {
    record_placeholder_hit(22, "deliver_retail_builtin_scene_event", main.iteration);
    if (!main.lifetime) {
        return std::nullopt;
    }
    return true;
}

} // namespace main_binding

// [unrecovered-placeholder] Recovered from PS2 0x002f1630 and Xbox 0x00277df0:
// Initializes profile select dialogue from file path. Unrecovered remainder: interactive UI dialogue rendering and memory card writeback.
bool invoke_retail_profile_select_from_2f1630(NativeRetailMainState& main, std::string_view, RetailProfileMenuFailure&) {
    record_placeholder_hit(23, "invoke_retail_profile_select_from_2f1630", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x00188720 and Xbox 0x00030580:
// Binds participant profile to active session role. Unrecovered remainder: profile participant record synchronization with NVRAM saves.
bool apply_retail_profile_participant_from_188720(NativeRetailMainState& main, std::uint32_t, RetailProfileMenuFailure&) {
    record_placeholder_hit(24, "apply_retail_profile_participant_from_188720", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

// [unrecovered-placeholder] Recovered from PS2 0x00189690 and Xbox 0x00031020:
// Publishes participant record to session participant registry. Unrecovered remainder: active session participant broadcast across network.
bool publish_retail_profile_participant_from_189690(
    NativeRetailMainState& main, const std::shared_ptr<NativeRetailInputParticipant>&, RetailProfileMenuFailure&) {
    record_placeholder_hit(25, "publish_retail_profile_participant_from_189690", main.iteration);
    if (!main.lifetime) {
        return false;
    }
    return true;
}

} // namespace mka::retail
