#include "mka/retail_selected_resource.hpp"
#include "mka/retail_initial_reset_required.hpp"
#include "mka/retail_initial_task_state.hpp"
#include "mka/retail_main.hpp"
#include "native_retail_startup.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <thread>
#include <vector>

#define NOMINMAX
#include <windows.h>

using namespace mka;
using namespace mka::retail;

namespace {

void check(bool condition, const char* msg) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", msg);
        std::exit(1);
    }
}

std::filesystem::path make_temp_root() {
    const auto temp = std::filesystem::temp_directory_path();
    for (unsigned i = 0; ; ++i) {
        auto root = temp / ("mka-res-test-" + std::to_string(GetCurrentProcessId()) +
            "-" + std::to_string(GetTickCount64()) + "-" + std::to_string(i));
        std::error_code error;
        if (std::filesystem::create_directories(root, error)) return root;
        check(i < 100, "create temp test root");
    }
}

void test_ladder_tables() {
    const auto& ladders = retail_arcade_ladders();
    check(ladders.size() == 19, "ladder table count must be 19");

    // Ladder 0 checks
    check(ladders[0][0].opponent_fighter_id == 18, "ladder 0 tier 0 fighter");
    check(ladders[0][0].arena_id == 14, "ladder 0 tier 0 arena");
    check(ladders[0][7].opponent_fighter_id == 11, "ladder 0 tier 7 fighter");
    check(ladders[0][7].arena_id == 3, "ladder 0 tier 7 arena");

    // Ladder 18 checks
    check(ladders[18][0].opponent_fighter_id == 18, "ladder 18 tier 0 fighter");
    check(ladders[18][7].opponent_fighter_id == 11, "ladder 18 tier 7 fighter");
    std::cout << "PASS: test_ladder_tables\n";
}

void test_selected_resource_lifecycle_and_bindings() {
    const auto root = make_temp_root();
    mka::content::ContentVolumeFailure content_failure;
    auto content = mka::content::NativeContentVolume::open(root, content_failure);
    check(bool(content), "native content volume open");

    NativeRetailStartupState startup(std::move(*content), {});
    StartupFailure sf;
    check(initialize_retail_memory_owners(startup, sf), "initialize_retail_memory_owners");
    check(initialize_native_retail_game_session(startup, sf), "initialize_native_retail_game_session");

    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);
    check(main.lifetime && main.lifetime->active_on_owner_thread(), "main lifetime active");

    auto session = main.startup.game_session;
    check(session != nullptr, "game session non-null");

    // Test reset_selected_resource_334700
    check(initial_reset_required::reset_selected_resource_334700(main, sf),
          "reset_selected_resource_334700");
    check(main.selected_resource != nullptr, "selected_resource allocated");

    const auto& ladder_state = main.selected_resource->ladder_state();
    check(ladder_state.progress_word == -1, "progress word must be -1");
    check(ladder_state.tier_index == 0, "tier index must be 0");
    check(ladder_state.ladder_index < 19, "ladder index within 0..18");
    check(ladder_state.active_ladder != nullptr, "active ladder non-null");
    check(ladder_state.active_ladder == &retail_arcade_ladders()[ladder_state.ladder_index],
          "active ladder matches table index");

    auto* records = session->game_records_values(sf.game);
    check(records != nullptr, "game records non-null");
    check(records->word_6602e8 == 0, "records word_6602e8 cleared to 0");

    // Test clear_nonowning_references_18da30
    check(initial_reset_required::clear_nonowning_references_18da30(main, sf),
          "clear_nonowning_references_18da30");
    auto* task_state = session->initial_task_state(sf.game);
    check(task_state != nullptr, "task state non-null");
    RetailInitialTaskStateFailure tf;
    const auto obs = task_state->observe(tf);
    check(obs.has_value(), "task state observed");
    check(obs->nonowning_object_reference == 0, "nonowning_object_reference cleared");
    check(obs->nonowning_object_cookie == 0, "nonowning_object_cookie cleared");
    check(obs->nonowning_task_reference == 0, "nonowning_task_reference cleared");
    check(obs->nonowning_task_cookie == 0, "nonowning_task_cookie cleared");

    // Test configure_selected_resource_zero_16fc70
    check(initial_reset_required::configure_selected_resource_zero_16fc70(main, sf),
          "configure_selected_resource_zero_16fc70");

    const auto& font_zero = main.selected_resource->font_entry(0);
    check(font_zero.name == "EDDMONDGRAY14", "font 0 name");
    check(font_zero.atlas != nullptr, "font 0 atlas decoded");
    check(font_zero.metrics != nullptr, "font 0 metrics parsed");
    check((font_zero.flags & 0xffff) == 0x3301, "font 0 flags 0x3301 mask");

    // Cache hit test (should succeed without error)
    check(initial_reset_required::configure_selected_resource_zero_16fc70(main, sf),
          "configure_selected_resource_zero_16fc70 cache hit");

    // Layout and geometry test
    text::FontLayoutConfig layout_config{
        .total_byte_budget = 256,
        .per_line_scan_budget = 256,
        .center = 0,
        .baseline = 100,
        .line_advance = 16,
    };
    text::FontLayoutFailure lf;
    auto layout = main.selected_resource->layout_text("MORTAL KOMBAT", layout_config, lf);
    check(layout.has_value(), "text layout succeeded");
    check(!layout->glyphs.empty(), "glyphs generated");

    text::FontGeometryConfig geom_config{
        .logical_screen_width = 640,
        .logical_screen_height = 448,
        .draw_canvas_width = 640,
        .draw_canvas_height = 448,
        .atlas_width = 256,
        .atlas_height = 128,
        .vertical_scale = text::FontVerticalScale::one,
    };
    text::FontGeometryFailure gf;
    auto geom = main.selected_resource->glyph_geometry(layout->glyphs[0], geom_config, gf);
    check(geom.has_value(), "glyph geometry succeeded");
    check(geom->x1 > geom->x0, "glyph geometry extent valid");

    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::cout << "PASS: test_selected_resource_lifecycle_and_bindings\n";
}

void test_wrong_thread_refusal() {
    const auto root = make_temp_root();
    mka::content::ContentVolumeFailure content_failure;
    auto content = mka::content::NativeContentVolume::open(root, content_failure);
    check(bool(content), "native content volume open");

    NativeRetailStartupState startup(std::move(*content), {});
    StartupFailure sf;
    check(initialize_retail_memory_owners(startup, sf), "initialize_retail_memory_owners");
    check(initialize_native_retail_game_session(startup, sf), "initialize_native_retail_game_session");

    mka::input::ControllerInput samples;
    NativeRetailMainState main(startup, samples);

    std::thread foreign([&]() {
        StartupFailure thread_sf;
        bool res1 = initial_reset_required::reset_selected_resource_334700(main, thread_sf);
        check(!res1, "reset_selected_resource_334700 must fail on foreign thread");

        bool res2 = initial_reset_required::configure_selected_resource_zero_16fc70(main, thread_sf);
        check(!res2, "configure_selected_resource_zero_16fc70 must fail on foreign thread");

        bool res3 = initial_reset_required::clear_nonowning_references_18da30(main, thread_sf);
        check(!res3, "clear_nonowning_references_18da30 must fail on foreign thread");
    });
    foreign.join();
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    std::cout << "PASS: test_wrong_thread_refusal\n";
}

} // namespace

int main() {
    test_ladder_tables();
    test_selected_resource_lifecycle_and_bindings();
    test_wrong_thread_refusal();
    std::cout << "ALL SELECTED RESOURCE TESTS PASSED.\n";
    return 0;
}
