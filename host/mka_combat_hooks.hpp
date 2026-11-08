#pragma once

#include "../../../../include/armageddon/retail_pad_event.hpp"
#include "../../../../include/armageddon/retail_health.hpp"
#include "../../../../include/armageddon/stage_hazards.hpp"
#include "../../../../include/armageddon/video_stream_registry.hpp"
#include "../../../../include/armageddon/audio_sink_registry.hpp"
#include "../../../../include/armageddon/camera_registry.hpp"
#include "../../../../include/armageddon/special_move_state.hpp"
#include "../../../../include/armageddon/fatality_sequencer.hpp"
#include "../../../../include/armageddon/projectile_manager.hpp"
#include "../../../../include/armageddon/breaker_parry_engine.hpp"
#include "../../../../include/armageddon/air_combat_engine.hpp"
#include "../../../../include/armageddon/aggressor_weapon_matrix.hpp"
#include "../../../../include/armageddon/stage_transition_engine.hpp"
#include "../../../../include/armageddon/render_bridge.hpp"
#include "ps2/native_hook.hpp"
#include "ps2/generated.hpp"
#include "ps2/system.hpp"
#include "ps2/memory.hpp"
#include "ps2/cpu.hpp"
#include "ps2/os.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <string>

namespace mka::combat {

// Interception Activation State
inline bool g_combat_interception_active = false;

inline bool is_interception_enabled() {
    if (g_combat_interception_active) return true;
    const std::string env = ps2::os::env("MKA_INTERCEPT_COMBAT");
    if (!env.empty() && env != "0" && env != "off") {
        g_combat_interception_active = true;
        return true;
    }
    return false;
}

// Native Phase 2 Combo History Tracker (outside RDRAM)
inline armageddon::retail::ComboHistoryTracker g_combo_tracker;

// Native Phase 3 Subsystem Managers
inline armageddon::StageHazardManager g_stage_hazard_manager;
inline armageddon::VideoStreamRegistry g_video_stream_registry;

// Native Phase 4 Audio Sink Registry (outside RDRAM)
inline armageddon::audio::AudioSinkRegistry g_audio_registry;

// Native Phase 5 Director's Cut Camera Registry (outside RDRAM)
inline armageddon::camera::CameraRegistry g_camera_registry;

// Native Phase 6 Special Move & Stance Registry (outside RDRAM)
inline armageddon::special_moves::SpecialMoveRegistry g_stance_registry;

// Native Phase 7 Kreate-A-Fatality Sequencer (outside RDRAM)
inline armageddon::fatality::FatalitySequencer g_fatality_sequencer;

// Native Phase 8 Projectile Manager (outside RDRAM)
inline armageddon::projectile::ProjectileManager g_projectile_manager;

// Native Phase 9 Defense Matrix Registry (outside RDRAM)
inline armageddon::defense::DefenseMatrixRegistry g_defense_matrix;

// Native Phase 10 Air Combat Manager (outside RDRAM)
inline armageddon::air_combat::AirCombatManager g_air_combat_manager;

// Native Phase 11 Aggressor & Weapon Awakening Matrix (outside RDRAM)
inline armageddon::aggressor::AggressorMatrixRegistry g_aggressor_matrix;

// Native Phase 12 Stage Transition & Multi-Tier Engine (outside RDRAM)
inline armageddon::stage_transition::StageTransitionManager g_stage_transition_manager;

// Native Phase 13 Render Bridge Coordinator (outside RDRAM)
inline armageddon::render_bridge::RenderBridgeCoordinator g_render_bridge;

inline void process_audio(const std::int16_t *samples, std::size_t frames) {
    if (!is_interception_enabled()) return;
    g_audio_registry.on_audio_samples(samples, frames);
}

// Portable C++17 bit reinterpretation primitives
inline float from_bits(std::uint32_t bits) {
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

inline std::uint32_t to_bits(float value) {
    std::uint32_t result = 0;
    std::memcpy(&result, &value, sizeof(result));
    return result;
}

// Guest PC Constants: Cross-Unit Boundaries
constexpr std::uint32_t kPadScannerGuestPc   = 0x001637D0u; // From unit_0008 (entry 328u in unit_0018)
constexpr std::uint32_t kHealthScalarGuestPc = 0x00197BC0u; // From unit_0041 (Hit Dispatcher, RA 0x00207728)
constexpr std::uint32_t kVideoStreamGuestPc  = 0x00181F90u; // Video stream dispatcher (entry 126u in unit_0020)

// RDRAM Layout (Strict Authentic 30-entry Ring & 5-entry Recent Map)
constexpr std::uint32_t kRingBufferAddr       = 0x0065ed40u;
constexpr std::uint32_t kRecentRingAddr       = 0x0065e600u;
constexpr std::uint32_t kWriteIndexAddr       = 0x0064c3acu;
constexpr std::uint32_t kRecentIndexAddr      = 0x0064e0ecu;
constexpr std::uint32_t kLastTickAddr         = 0x0064e0e4u;

constexpr std::uint32_t kSubZeroHealthAddr   = 0x00660180u;
constexpr std::uint32_t kModeWordAddr        = 0x0064dea4u;
constexpr std::uint32_t kGatekeeperFlagsAddr = 0x00660080u;
constexpr std::uint32_t kPlayer1ObjPtr       = 0x0066015cu;
constexpr std::uint32_t kPlayer2ObjPtr       = 0x006601a4u;

// Helper: Checks if the engine is currently in an active 1v1 combat match
// Returns true only for Arcade 1v1, Versus 1v1, Practice 1v1, or Konquest 1v1 arena bouts.
// Returns false in Main Menus, Options, Character Select, and Konquest 3rd-person exploration.
inline bool is_active_combat_match() {
    const std::uint32_t mode        = ps2::ld32(kModeWordAddr);
    const std::uint8_t flags_660080 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr));

    // Valid combat modes: mode < 2 (Arcade, Versus, Practice)
    // and combat active gatekeeper flag bit 2 (0x04) in flags_660080.
    // In Practice Mode, bit 2 of flags_660084 is set by the engine to lock dummy health,
    // so we must not reject matches based on flags_660084!
    if (mode >= 2u || (flags_660080 & 4u) == 0u) {
        return false;
    }

    const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
    if (p2 < 0x00100000u || p2 >= 0x02000000u) {
        return false;
    }

    const float p2_health = from_bits(ps2::ld32(kSubZeroHealthAddr));
    if (!std::isfinite(p2_health) || p2_health <= 0.0f || p2_health > 1.5f) {
        return false;
    }

    return true;
}

// 1. Health Scalar & Stage Hazard Interception (Linker Wrapped unit_0025 @ entry 204u)
inline void handle_subzero_health(ps2::Cpu &c, std::uint32_t depth,
                                  void (*real_unit_0025)(ps2::Cpu&, std::uint32_t, std::uint32_t)) {
    const std::uint32_t mode        = ps2::ld32(kModeWordAddr);
    const std::uint8_t flags_660080 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr));
    const std::uint8_t flags_660084 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr + 4));

    const float delta   = from_bits(c.f[12]);
    const float current = from_bits(ps2::ld32(kSubZeroHealthAddr));

    const float combo_mult = g_combo_tracker.get_damage_multiplier();
    const float effective_delta = delta * combo_mult;

    const bool is_practice = ((flags_660084 & 4u) != 0u);
    // Gatekeeper: Non-standard / knockout modes (health depletion <= 0) delegate explicitly to authentic unit_0025
    // In Practice Mode, dummy health is preserved, so allow hits without requiring depletion delta
    static std::uint64_t s_health_calls = 0;
    const bool is_target_combat = (mode < 2u && (flags_660080 & 4u) != 0u &&
                                   std::isfinite(current) && std::isfinite(effective_delta) &&
                                   effective_delta < 0.0f && current > 0.0f &&
                                   (is_practice || (current + effective_delta) > 0.0f));

    if (++s_health_calls <= 25 || is_target_combat) {
        std::fprintf(stderr, "[mka-health] invocation #%llu: mode=%u flags=0x%02X/0x%02X cur=%.4f delta=%.4f (mult=%.1fx, practice=%d) (%s)\n",
                     static_cast<unsigned long long>(s_health_calls), mode, flags_660080, flags_660084,
                     current, effective_delta, combo_mult, is_practice ? 1 : 0, is_target_combat ? "intercepting" : "delegating");
    }

    if (!is_target_combat) {
        g_audio_registry.evaluate_combat_audio_state(0.0f, effective_delta, false, true);
        g_camera_registry.on_combat_hit(effective_delta, true, false);
        g_camera_registry.advance_frame();
        g_fatality_sequencer.start_fatality(0u);
        real_unit_0025(c, 204u, depth);
        return;
    }

    try {
        // Phase 9 Defense Matrix: Parry Evaluation (Down + Block)
        float active_delta = effective_delta;
        float mitigated_dmg = 0.0f;
        const bool parried = (effective_delta < 0.0f &&
                              g_defense_matrix.evaluate_incoming_hit(-effective_delta, 0u, mitigated_dmg));
        if (parried) {
            std::fprintf(stderr, "[mka-defense] PARRY DEFLECT! Incoming strike of %.4ff mitigated to 0.0f (attacker stagger=18f)!\n",
                         -effective_delta);
            active_delta = 0.0f;
        }

        // Phase 11 Aggressor Light Armor Absorption
        float armor_absorbed_dmg = 0.0f;
        const bool armor_absorbed = (!parried && active_delta < 0.0f &&
                                     g_aggressor_matrix.try_absorb_damage(-active_delta, armor_absorbed_dmg, 0u));
        if (armor_absorbed) {
            std::fprintf(stderr, "[mka-aggressor] LIGHT ARMOR ABSORPTION! Incoming strike of %.4ff reduced to %.4ff (armor remaining=%u)\n",
                         -active_delta, armor_absorbed_dmg, g_aggressor_matrix.armor_hits_remaining());
            active_delta = -armor_absorbed_dmg;
        }

        // Standard Hit Path: Bit-cast delta and commit with IEEE-754 integrity
        float new_health = current;
        if (!is_practice) {
            new_health = armageddon::retail::apply_observed_hit_to_health(
                current, active_delta, mode, flags_660080, flags_660084);
            ps2::st32(kSubZeroHealthAddr, to_bits(new_health)); // Preserves 0x3f6d288d
        } else {
            // Authentic MKA Practice Mode preserves dummy health
            new_health = current;
        }

        const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
        if (p2 != 0) {
            const float old_acc = from_bits(ps2::ld32(p2 + 0x3dc));
            ps2::st32(p2 + 0x3dc, to_bits(old_acc - active_delta));

            // Stage Hazard Collision Check (Position at p2 + 0x50, 0x54, 0x58)
            const float p2_x = from_bits(ps2::ld32(p2 + 0x50));
            const float p2_y = from_bits(ps2::ld32(p2 + 0x54));
            const float p2_z = from_bits(ps2::ld32(p2 + 0x58));

            auto hazard_hit = g_stage_hazard_manager.check_point_collision(p2_x, p2_y, p2_z);
            if (hazard_hit.triggered) {
                std::fprintf(stderr, "[mka-hazard] Triggered %s at (%.1f, %.1f, %.1f) (damage=%.1f, lethal=%d)\n",
                             hazard_hit.hazard_name.c_str(), p2_x, p2_y, p2_z,
                             hazard_hit.total_damage, hazard_hit.is_lethal);

                if (hazard_hit.is_lethal) {
                    std::fprintf(stderr, "[mka-hazard] Lethal deathtrap fatality [%s]! Delegating to authentic unit_0025\n",
                                 hazard_hit.fatality_cinematic_name.c_str());
                    g_audio_registry.evaluate_combat_audio_state(0.0f, effective_delta, false, true);
                    g_camera_registry.on_combat_hit(effective_delta, true, false);
                    g_camera_registry.advance_frame();
                    g_fatality_sequencer.start_fatality(0u);
                    real_unit_0025(c, 204u, depth);
                    return;
                }
            }

            // Phase 8 Projectile Collision Evaluation (Target at p2 + 0x50, 0x54, 0x58)
            auto proj_hit = g_projectile_manager.update_and_collide(p2_x, p2_y, p2_z, 0u);
            if (proj_hit == armageddon::projectile::ProjectileHitResult::DirectHit) {
                const auto hit_type = g_projectile_manager.last_hit_type();
                const float hit_dmg = g_projectile_manager.last_hit_damage();
                if (hit_type == armageddon::projectile::ProjectileType::ScorpionSpear) {
                    std::fprintf(stderr, "[mka-proj] SCORPION'S SPEAR IMPALED OPPONENT! \"GET OVER HERE!\" (tether=%u frames, dmg=%.2f)\n",
                                 g_projectile_manager.target_tethered_ticks(), hit_dmg);
                    const std::uint32_t p1 = ps2::ld32(kPlayer1ObjPtr);
                    if (p1 != 0) {
                        const float p1_x = from_bits(ps2::ld32(p1 + 0x50));
                        const float pull_target_x = (p1_x < p2_x) ? (p1_x + 1.6f) : (p1_x - 1.6f);
                        ps2::st32(p2 + 0x50, to_bits(pull_target_x));
                        std::fprintf(stderr, "[mka-proj] Opponent reeled in to point-blank range -> new P2 X=%.2f\n", pull_target_x);
                    }
                } else if (hit_type == armageddon::projectile::ProjectileType::HellfirePillar) {
                    std::fprintf(stderr, "[mka-proj] HELLFIRE PILLAR ERUPTION! Opponent engulfed in flames (dmg=%.2f)\n", hit_dmg);
                    ps2::st32(p2 + 0x54, to_bits(2.5f));
                } else {
                    std::fprintf(stderr, "[mka-proj] P1's %s struck Opponent (P2)! (freeze=%u frames, dmg=%.2f)\n",
                                 armageddon::projectile::projectile_type_name(hit_type),
                                 g_projectile_manager.target_frozen_ticks(), hit_dmg);
                }
                g_camera_registry.on_combat_hit(-hit_dmg, false, false);
                g_camera_registry.advance_frame();
            }

            // Phase 10 Aerial Kinematics Synchronization (Y at p2 + 0x54)
            if (g_air_combat_manager.p2().is_airborne()) {
                ps2::st32(p2 + 0x54, to_bits(g_air_combat_manager.p2().y));
            }

            // Phase 12 Stage Transition & Multi-Tier Breach Evaluation
            const bool wall_breached = g_stage_transition_manager.evaluate_wall_impact(p2_x, 1.0f);
            if (wall_breached) {
                std::fprintf(stderr, "[mka-stage] ARENA PERIMETER BREACHED at X=%.2f! Transitioning to Freefall Tier 2\n", p2_x);
                g_stage_transition_manager.trigger_tier_fall(1u, p2_y);
                g_camera_registry.on_combat_hit(-0.25f, false, true);
                g_camera_registry.advance_frame();
            } else if (g_stage_transition_manager.is_in_transition()) {
                const bool landed_tier2 = g_stage_transition_manager.step_fall_physics();
                if (landed_tier2) {
                    bool is_fatal = false;
                    const float impact_dmg = g_stage_transition_manager.calculate_impact_damage(current, is_fatal);
                    std::fprintf(stderr, "[mka-stage] TIER 2 IMPACT LANDING! Impact dmg=%.2ff (lethal=%d, new Tier=2)\n",
                                 impact_dmg, is_fatal ? 1 : 0);
                    if (is_fatal) {
                        std::fprintf(stderr, "[mka-stage] Lethal tier drop fatality! Delegating to authentic unit_0025\n");
                        g_audio_registry.evaluate_combat_audio_state(0.0f, -impact_dmg, false, true);
                        g_camera_registry.on_combat_hit(-impact_dmg, true, false);
                        g_camera_registry.advance_frame();
                        g_fatality_sequencer.start_fatality(0u);
                        real_unit_0025(c, 204u, depth);
                        return;
                    }
                    g_camera_registry.on_combat_hit(-impact_dmg, false, true);
                    g_camera_registry.advance_frame();
                }
            }
        }

        const bool is_combo = g_combo_tracker.is_combo_active();
        const float combo_scaling = g_combo_tracker.get_combo_scaling();
        if (is_combo) {
            std::fprintf(stderr, "[mka-combo] Combat hit registered during %u-hit combo! (applied scaling=%.0f%%)\n",
                         g_combo_tracker.combo_hit_count(), combo_scaling * 100.0f);
        }
        if (parried) {
            g_audio_registry.evaluate_combat_audio_state(new_health, 0.0f, false, false);
            g_camera_registry.on_combat_hit(0.0f, false, true);
            g_camera_registry.advance_frame();
            g_defense_matrix.advance_frame();
            g_air_combat_manager.advance_frame(0u);
            g_aggressor_matrix.advance_frame();
            g_render_bridge.trigger_camera_zoom(38.0f, armageddon::render_bridge::CameraViewportBridge::Mode::Normal);
            g_render_bridge.advance_frame();
        } else {
            g_audio_registry.evaluate_combat_audio_state(new_health, active_delta, is_combo, false);
            g_camera_registry.on_combat_hit(active_delta, false, is_combo);
            g_camera_registry.advance_frame();
            g_defense_matrix.advance_frame();
            g_air_combat_manager.advance_frame(0u);
            g_aggressor_matrix.advance_frame();
            g_render_bridge.trigger_camera_zoom(55.0f, armageddon::render_bridge::CameraViewportBridge::Mode::ZoomedIn);
            g_render_bridge.advance_frame();
        }

        std::fprintf(stderr, "[health] delta %.4ff (mult=%.1fx, parried=%d) applied to current %.4ff -> new %.4ff (mode=%u)\n",
                     active_delta, combo_mult, parried ? 1 : 0, current, new_health, mode);
        std::fprintf(stderr, "[mka-camera] Hit-stop: %s (dilation=%.2f, shake=%u, fov=%.1f)\n",
                     armageddon::camera::hit_stop_state_name(g_camera_registry.hit_stop().state),
                     g_camera_registry.hit_stop().time_dilation(),
                     static_cast<unsigned>(g_camera_registry.shake().active_tier),
                     g_camera_registry.matrix_buffer().latest().fov);
        std::fprintf(stderr, "[mka-render] Viewport: %s (focal=%.1fmm), Actor: %s (lean=%.1f deg), Entities: %u\n",
                     armageddon::render_bridge::CameraViewportBridge::mode_name(g_render_bridge.camera().latest().mode_idx),
                     g_render_bridge.camera().latest().focal_length,
                     armageddon::render_bridge::ActorAnimationBridge::blend_mode_name(g_render_bridge.actor().latest().blend_idx),
                     g_render_bridge.actor().latest().lean_angle,
                     g_render_bridge.entities().frames_recorded());

        c.pc = c.r[31].w[0]; // Clean unwind to caller RA (0x00207728)
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[mka-health] delegated on exception (%s)\n", e.what());
        real_unit_0025(c, 204u, depth);
    }
}

// 2. PAD Event Interception Implementation (Linker Wrapped unit_0018 @ entry 309u)
inline void handle_pad_event(ps2::Cpu &c, std::uint32_t depth,
                             void (*real_unit_0018)(ps2::Cpu&, std::uint32_t, std::uint32_t)) {
    const std::uint32_t channel       = c.r[19].w[0]; // channel loop index (0 = P1)
    const std::uint32_t button_code   = c.r[5].w[0];  // button index (0..15)
    const std::uint32_t tick          = c.r[6].w[0];
    const std::uint32_t label_addr    = c.r[7].w[0];
    const std::uint32_t mapping_index = c.r[8].w[0];
    const std::uint32_t ret_addr      = c.r[31].w[0];

    // Gatekeeper 1: Non-combat context (menus, options, character select, Konquest exploration)
    // Pass all pad events completely untouched to authentic engine
    if (!is_active_combat_match()) {
        real_unit_0018(c, 309u, depth);
        return;
    }

    // Gatekeeper 2: Intercept Player 1 combat cluster:
    // 4 (Triangle), 5 (Circle), 6 (Cross), 7 (Square), 9 (R2 Block), 10 (L1 Style Change)
    // Also intercept directional buttons (0..3) if a Kreate-A-Fatality sequence is active
    // Direction mapping: 0 = Up, 1 = Right, 2 = Down, 3 = Left
    struct DirRecord {
        std::uint32_t code = 0xFFFFFFFFu;
        std::uint32_t tick = 0u;
    };
    static std::uint64_t s_pad_events = 0;
    static std::uint32_t s_last_dir_code = 0xFFFFFFFFu;
    static std::uint32_t s_last_dir_tick = 0u;
    static std::uint32_t s_last_fwd_tick = 0u;
    static std::uint32_t s_dash_active_until_tick = 0u;
    static std::array<DirRecord, 16> s_dir_history{};
    static std::size_t s_dir_total = 0;

    const std::uint32_t p1_ptr = ps2::ld32(kPlayer1ObjPtr);
    const std::uint32_t p2_ptr = ps2::ld32(kPlayer2ObjPtr);
    std::uint32_t fwd_code = 1u;  // default Right
    std::uint32_t back_code = 3u; // default Left
    float fwd_dir_sign = 1.0f;
    if (p1_ptr >= 0x00100000u && p2_ptr >= 0x00100000u) {
        const float p1_x = from_bits(ps2::ld32(p1_ptr + 0x50));
        const float p2_x = from_bits(ps2::ld32(p2_ptr + 0x50));
        if (p1_x <= p2_x) {
            fwd_code = 1u;  // Right
            back_code = 3u; // Left (away from opponent)
            fwd_dir_sign = 1.0f;
        } else {
            fwd_code = 3u;  // Left
            back_code = 1u; // Right (away from opponent)
            fwd_dir_sign = -1.0f;
        }
    }

    if (channel == 0 && button_code <= 3u) {
        s_last_dir_code = button_code;
        s_last_dir_tick = tick;
        s_dir_history[s_dir_total % s_dir_history.size()] = { button_code, tick };
        ++s_dir_total;

        // Forward double-tap dash tracking (Forward, Forward)
        if (button_code == fwd_code || button_code == 1u) {
            if (tick >= s_last_fwd_tick && (tick - s_last_fwd_tick) <= 25u && (tick - s_last_fwd_tick) > 0u) {
                s_dash_active_until_tick = tick + 35u;
                std::fprintf(stderr, "[mka-air] FORWARD DASH DETECTED at tick 0x%04X (Dash-Cancel Jump window armed for 35 frames)!\n", tick);
            }
            s_last_fwd_tick = tick;
        }
    }

    // Helper: detect 2-direction motion executed within timing window
    auto match_2dir_motion = [&](std::uint32_t d1, std::uint32_t d2, std::uint32_t cur_tick, std::uint32_t window = 45u) -> bool {
        if (s_dir_total < 2u) return false;
        const auto &last0 = s_dir_history[(s_dir_total - 1u) % s_dir_history.size()];
        const auto &last1 = s_dir_history[(s_dir_total - 2u) % s_dir_history.size()];
        if (cur_tick < last0.tick || (cur_tick - last0.tick) > window) return false;
        if (last0.tick < last1.tick || (last0.tick - last1.tick) > 35u) return false;
        return (last1.code == d1 && last0.code == d2);
    };

    // Check if Up is pressed during an active dash: Dash-Cancel Jump (Forward, Forward, Up)
    const bool is_dash_cancel_jump = (channel == 0 && button_code == 0u &&
                                      tick <= s_dash_active_until_tick && s_dash_active_until_tick > 0u);

    const bool is_target_combat = (channel == 0 &&
        (button_code == 4u || button_code == 5u || button_code == 6u || button_code == 7u ||
         button_code == 9u || button_code == 10u || is_dash_cancel_jump ||
         (g_fatality_sequencer.is_active() && button_code <= 3u)));

    if (++s_pad_events <= 25 || is_target_combat) {
        const auto btn_enum = armageddon::retail::to_combat_button(button_code);
        std::fprintf(stderr, "[mka-pad] P1 (ch=%u) event #%llu: btn=%u [%s] (target=%s)\n",
                     channel, static_cast<unsigned long long>(s_pad_events), button_code,
                     armageddon::retail::combat_button_name(btn_enum),
                     is_target_combat ? "COMBAT_INTERCEPT" : "delegate");
    }

    if (!is_target_combat) {
        real_unit_0018(c, 309u, depth);
        return;
    }

    try {
        // Native Path: Reconstruct channel state and commit atomically
        armageddon::retail::PadEventChannel0 pad_ch{};
        pad_ch.write_index     = ps2::ld32(kWriteIndexAddr);
        pad_ch.recent_index    = ps2::ld32(kRecentIndexAddr);
        pad_ch.last_event_tick = ps2::ld32(kLastTickAddr);

        if (pad_ch.write_index >= pad_ch.records.size() ||
            pad_ch.recent_index >= pad_ch.recent_mappings.size()) {
            real_unit_0018(c, 309u, depth);
            return;
        }

        for (std::size_t i = 0; i < pad_ch.records.size(); ++i) {
            const std::uint32_t at = kRingBufferAddr + static_cast<std::uint32_t>(i * 20);
            pad_ch.records[i] = {
                ps2::ld32(at), ps2::ld32(at + 4), ps2::ld32(at + 8), ps2::ld32(at + 12), ps2::ld32(at + 16)
            };
        }
        for (std::size_t i = 0; i < pad_ch.recent_mappings.size(); ++i) {
            pad_ch.recent_mappings[i] = ps2::ld32(kRecentRingAddr + static_cast<std::uint32_t>(i * 4));
        }

        const std::uint32_t prev_index = pad_ch.write_index;
        armageddon::retail::append_pad_event(pad_ch, button_code, tick, label_addr, 0u, mapping_index);
        g_combo_tracker.record(button_code, tick);

        // Combo string & 3+ hit combo detection with scaling and visual logs
        const std::uint32_t combo_hits = g_combo_tracker.combo_hit_count();
        const float combo_scaling = g_combo_tracker.get_combo_scaling();
        const bool is_hapkido = g_combo_tracker.detect_hapkido_string();
        const bool is_generic_combo = (combo_hits >= 3u);

        if (is_hapkido || is_generic_combo) {
            std::fprintf(stderr, "[mka-combo] %u-HIT COMBO DETECTED! (scaling=%.0f%%, mult=%.2fx%s) at tick 0x%04X!\n",
                         combo_hits, combo_scaling * 100.0f, combo_scaling,
                         is_hapkido ? " [Hapkido String]" : "", tick);
            g_audio_registry.evaluate_combat_audio_state(1.0f, 0.0f, true, false);
            g_camera_registry.on_combat_hit(-0.06f * combo_scaling, false, true);
            g_camera_registry.advance_frame();
            g_render_bridge.trigger_camera_zoom(56.0f, armageddon::render_bridge::CameraViewportBridge::Mode::ZoomedIn);
            std::fprintf(stderr, "[mka-camera] Combo string camera pulse: FOV=%.1f, shake=%u\n",
                         g_camera_registry.matrix_buffer().latest().fov,
                         static_cast<unsigned>(g_camera_registry.shake().active_tier));
        }

        // Phase 6 Stance & Special Move Processing
        if (button_code == 10u) { // L1 Style Change / Stance Switch
            const bool switched = g_stance_registry.on_button_press(button_code, tick);
            if (switched) {
                const auto st = g_stance_registry.current_stance();
                std::fprintf(stderr, "[mka-stance] Switched stance to %s (L1 at tick 0x%04X, cooldown=8f)\n",
                             armageddon::special_moves::stance_name(st), tick);
                if (st == armageddon::special_moves::StanceType::Dragon) {
                    g_render_bridge.trigger_stance_blend(armageddon::render_bridge::ActorAnimationBridge::BlendMode::Guarded, 42.0f);
                } else if (st == armageddon::special_moves::StanceType::KoriBlade || st == armageddon::special_moves::StanceType::MugaiRyu) {
                    g_render_bridge.trigger_stance_blend(armageddon::render_bridge::ActorAnimationBridge::BlendMode::Lungeing, 28.0f);
                } else {
                    g_render_bridge.trigger_stance_blend(armageddon::render_bridge::ActorAnimationBridge::BlendMode::Idle, 35.0f);
                }
            }
        }

        // Phase 7 Kreate-A-Fatality Processing
        if (g_fatality_sequencer.is_active()) {
            const bool matched = g_fatality_sequencer.on_pad_event(button_code, tick);
            if (matched && g_fatality_sequencer.phase() == armageddon::fatality::FatalityPhase::ExecutingMove) {
                std::fprintf(stderr, "[mka-fatality] Executed %s (Tier %u, Score=%u)\n",
                             armageddon::fatality::fatality_move_name(g_fatality_sequencer.active_move()),
                             g_fatality_sequencer.current_tier(), g_fatality_sequencer.total_score());
                g_camera_registry.on_combat_hit(-0.15f, false, true);
                g_camera_registry.advance_frame();
            } else if (matched && g_fatality_sequencer.phase() == armageddon::fatality::FatalityPhase::FinalFinisherExecuting) {
                std::fprintf(stderr, "[mka-fatality] FINAL FINISHER: %s! (Score=%u)\n",
                             armageddon::fatality::fatality_move_name(g_fatality_sequencer.active_move()),
                             g_fatality_sequencer.total_score());
                g_audio_registry.evaluate_combat_audio_state(0.0f, -0.30f, false, true);
                g_camera_registry.on_combat_hit(-0.30f, true, false);
                g_camera_registry.advance_frame();
            }
        }

        // Phase 8 Authentic Special Move & Projectile Spawning
        const float cur_p1_x = (p1_ptr >= 0x00100000u) ? from_bits(ps2::ld32(p1_ptr + 0x50)) : 0.0f;
        const float cur_p2_x = (p2_ptr >= 0x00100000u) ? from_bits(ps2::ld32(p2_ptr + 0x50)) : 0.0f;
        const std::uint32_t down_code = 2u;
        const std::uint32_t up_code = 0u;

        if (button_code == 7u) { // Square (Attack 1)
            if (match_2dir_motion(back_code, fwd_code, tick)) {
                // Scorpion Bloody Spear: Back, Forward + 1
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::BloodySpear, tick);
                g_projectile_manager.spawn_scorpion_spear(armageddon::projectile::Vec3::create(cur_p1_x, 1.2f, 0.0f), fwd_dir_sign);
                g_render_bridge.spawn_entity(armageddon::render_bridge::EntityPoolBridge::EntityType::ScorpionSpear, cur_p1_x, 1.2f, 0.0f);
                std::fprintf(stderr, "[mka-special] SCORPION: BLOODY SPEAR (Back, Forward + 1) unleashed at tick 0x%04X! \"GET OVER HERE!\"\n", tick);
                g_audio_registry.evaluate_combat_audio_state(1.0f, -0.09f, false, false);
                g_camera_registry.on_combat_hit(-0.09f, false, true);
                g_camera_registry.advance_frame();
            } else if (match_2dir_motion(down_code, back_code, tick)) {
                // Sub-Zero Ice Clone: Down, Back + 1
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::IceClone, tick);
                g_projectile_manager.spawn_ice_clone(armageddon::projectile::Vec3::create(cur_p1_x, 0.8f, 0.0f));
                g_render_bridge.spawn_entity(armageddon::render_bridge::EntityPoolBridge::EntityType::IceClone, cur_p1_x, 0.8f, 0.0f);
                std::fprintf(stderr, "[mka-special] SUB-ZERO: ICE CLONE (Down, Back + 1) deployed at tick 0x%04X!\n", tick);
            }
        } else if (button_code == 4u) { // Triangle (Attack 2)
            if (match_2dir_motion(down_code, back_code, tick)) {
                // Scorpion Hellfire: Down, Back + 2
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::Hellfire, tick);
                g_projectile_manager.spawn_hellfire_pillar(armageddon::projectile::Vec3::create(cur_p2_x, 0.0f, 0.0f));
                g_render_bridge.spawn_entity(armageddon::render_bridge::EntityPoolBridge::EntityType::HellfirePillar, cur_p2_x, 0.0f, 0.0f);
                std::fprintf(stderr, "[mka-special] SCORPION: HELLFIRE (Down, Back + 2) ignited at tick 0x%04X! Unblockable flame eruption beneath opponent!\n", tick);
                g_audio_registry.evaluate_combat_audio_state(1.0f, -0.11f, false, false);
                g_camera_registry.on_combat_hit(-0.11f, false, true);
                g_camera_registry.advance_frame();
            } else if (match_2dir_motion(down_code, up_code, tick)) {
                // Sub-Zero Icy Pillar / Tombstone: Down, Up + 2
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::TombstoneTeleport, tick);
                std::fprintf(stderr, "[mka-special] SUB-ZERO: ICY PILLAR / TOMBSTONE TELEPORT (Down, Up + 2) at tick 0x%04X!\n", tick);
            }
        } else if (button_code == 6u) { // Cross (Attack 3)
            if (match_2dir_motion(down_code, fwd_code, tick)) {
                // Sub-Zero Freeze Ball: Down, Forward + 3
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::FreezeBall, tick);
                g_projectile_manager.spawn_freeze_ball(armageddon::projectile::Vec3::create(cur_p1_x, 1.2f, 0.0f), fwd_dir_sign);
                g_render_bridge.spawn_entity(armageddon::render_bridge::EntityPoolBridge::EntityType::FreezeBall, cur_p1_x, 1.2f, 0.0f);
                std::fprintf(stderr, "[mka-special] SUB-ZERO: FREEZE BALL (Down, Forward + 3) hurled at tick 0x%04X!\n", tick);
            } else if (match_2dir_motion(fwd_code, back_code, tick)) {
                // Scorpion Backflip Kick: Forward, Back + 3
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::BackflipKick, tick);
                std::fprintf(stderr, "[mka-special] SCORPION: BACKFLIP KICK (Forward, Back + 3) executed at tick 0x%04X!\n", tick);
                g_air_combat_manager.p1_mutable().vy = 6.0f;
                g_camera_registry.on_combat_hit(-0.08f, false, true);
                g_camera_registry.advance_frame();
            }
        } else if (button_code == 5u) { // Circle (Attack 4)
            if (match_2dir_motion(back_code, fwd_code, tick)) {
                // Sub-Zero Cold Shoulder: Back, Forward + 4
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::ColdShoulder, tick);
                std::fprintf(stderr, "[mka-special] SUB-ZERO: COLD SHOULDER (Back, Forward + 4) executed at tick 0x%04X!\n", tick);
                g_camera_registry.on_combat_hit(-0.12f, false, true);
                g_camera_registry.advance_frame();
            } else if (match_2dir_motion(fwd_code, back_code, tick)) {
                // Scorpion Hellfire Punch: Forward, Back + 4
                g_stance_registry.trigger_special_move(armageddon::special_moves::SpecialMoveId::HellfirePunch, tick);
                if (p1_ptr >= 0x00100000u && p2_ptr >= 0x00100000u) {
                    const float new_p1_x = cur_p2_x - fwd_dir_sign * 1.2f;
                    ps2::st32(p1_ptr + 0x50, to_bits(new_p1_x));
                    std::fprintf(stderr, "[mka-special] SCORPION: HELLFIRE PUNCH (Forward, Back + 4) teleported behind P2 to X=%.2f!\n", new_p1_x);
                }
                g_camera_registry.on_combat_hit(-0.10f, false, true);
                g_camera_registry.advance_frame();
            }
        }

        // Phase 9 Defense Matrix Processing (R2 Block = button 9)
        if (button_code == 9u) {
            const bool recent_dir = (tick >= s_last_dir_tick && (tick - s_last_dir_tick) <= 30u);
            const bool is_back = recent_dir && (s_last_dir_code == back_code || s_last_dir_code == 3u);
            const bool is_forward = recent_dir && (s_last_dir_code == fwd_code || s_last_dir_code == 1u);

            if (is_back) { // Back (holding away from opponent) + Block -> Parry
                const bool opened = g_defense_matrix.try_parry_input(tick);
                if (opened) {
                    std::fprintf(stderr, "[mka-defense] Opened 4-frame Parry Window at tick 0x%04X (Back + R2 Block)\n", tick);
                }
            } else if (is_forward) { // Forward + R2 -> Breaker
                const bool broken = g_defense_matrix.try_combo_breaker(tick, true, true);
                if (broken) {
                    std::fprintf(stderr, "[mka-defense] COMBO BREAKER TRIGGERED at tick 0x%04X! Charges remaining=%u\n",
                                 tick, g_defense_matrix.breaker().charges_remaining);
                    g_combo_tracker.clear();
                    const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
                    if (p2 != 0) {
                        float p2_x = from_bits(ps2::ld32(p2 + 0x50));
                        p2_x += g_defense_matrix.breaker().knockback_impulse_x;
                        ps2::st32(p2 + 0x50, to_bits(p2_x));
                        std::fprintf(stderr, "[mka-defense] Applied Breaker knockback to Opponent (P2) (+%.1fm) -> new P2 X=%.2f\n",
                                     g_defense_matrix.breaker().knockback_impulse_x, p2_x);
                        if (g_stage_transition_manager.evaluate_wall_impact(p2_x, 1.5f, tick)) {
                            std::fprintf(stderr, "[mka-stage] BREAKER KNOCKBACK SHATTERED PERIMETER BARRIER! (P2 X=%.2f)\n", p2_x);
                            g_stage_transition_manager.trigger_tier_fall(1u, 0.0f, tick);
                        }
                    }
                    g_camera_registry.on_combat_hit(-0.05f, false, true);
                    g_camera_registry.advance_frame();
                }
            }
        }

        // Phase 10 Air Combat Launcher & Jump Pursuit Processing
        const bool recent_up = (tick >= s_last_dir_tick && (tick - s_last_dir_tick) <= 30u && s_last_dir_code == 0u);
        if (is_dash_cancel_jump) { // Dash-Cancel Super Jump (Forward, Forward, Up)
            s_dash_active_until_tick = 0u; // consume the dash
            g_air_combat_manager.on_jump_pursuit(tick);
            g_air_combat_manager.p1_mutable().vx = fwd_dir_sign * 5.5f;
            g_air_combat_manager.p1_mutable().vy = 9.5f; // massive forward-arching Super Jump
            std::fprintf(stderr, "[mka-air] DASH-CANCEL SUPER JUMP triggered (Forward, Forward, Up) at tick 0x%04X! P1 launched in forward arch (vy=%.1fm/s, vx=%.1fm/s)\n",
                         tick, g_air_combat_manager.p1().vy, g_air_combat_manager.p1().vx);
            g_camera_registry.on_combat_hit(-0.05f, false, true);
            g_camera_registry.advance_frame();
            g_render_bridge.trigger_camera_zoom(52.0f, armageddon::render_bridge::CameraViewportBridge::Mode::Normal);
        } else if (button_code == 7u && recent_up) { // Up + Square -> Launcher Strike
            g_air_combat_manager.on_launcher_strike(tick);
            std::fprintf(stderr, "[mka-air] LAUNCHER STRIKE at tick 0x%04X! Opponent (P2) launched into air (vy=%.1fm/s)\n",
                         tick, g_air_combat_manager.p2().vy);
            g_camera_registry.on_combat_hit(-0.08f, false, true);
            g_camera_registry.advance_frame();
        } else if (g_air_combat_manager.p2().is_airborne() &&
                   (button_code == 4u || button_code == 5u || button_code == 7u)) {
            float air_scale = 1.0f;
            if (g_air_combat_manager.on_aerial_combo_hit(tick, air_scale)) {
                std::fprintf(stderr, "[mka-air] AERIAL STRIKE connected at tick 0x%04X on Opponent (P2)! Juggle #%u (scaling=%.0f%%)\n",
                             tick, g_air_combat_manager.p2().juggle_hits, air_scale * 100.0f);
                g_camera_registry.on_combat_hit(-0.05f * air_scale, false, true);
                g_camera_registry.advance_frame();
            }
        }

        // Phase 11 Aggressor Super Meter & Awakening Activation
        if (button_code == 4u || button_code == 5u || button_code == 6u || button_code == 7u) {
            const float gain = (button_code == 4u || button_code == 5u) ? 8.0f : 5.0f;
            const bool reached_full = g_aggressor_matrix.add_meter(gain, tick);
            const float cur_meter = g_aggressor_matrix.meter();
            std::fprintf(stderr, "[mka-aggressor] Meter: %.1f%% (+%.1f%%) [state=%s]\n",
                         cur_meter, gain,
                         g_aggressor_matrix.is_awakened() ? "AWAKENED" : (reached_full ? "FULL" : "BUILDING"));
            if (reached_full) {
                std::fprintf(stderr, "[mka-aggressor] METER FULL at tick 0x%04X! Aggressor meter reached 100.0f (Awakening ready via L1)\n", tick);
            }
        }
        if (button_code == 10u && g_aggressor_matrix.is_full()) {
            const bool awakened = g_aggressor_matrix.trigger_awakening(tick);
            if (awakened) {
                const bool in_weapon = (g_stance_registry.current_stance() == armageddon::special_moves::StanceType::KoriBlade);
                const float boost = g_aggressor_matrix.evaluate_offensive_multiplier(in_weapon, tick);
                std::fprintf(stderr, "[mka-aggressor] AWAKENING ACTIVATED at tick 0x%04X! (duration=240f, armor=2 hits, boost=%.2fx, weapon_synergy=%d)\n",
                             tick, boost, in_weapon ? 1 : 0);
                g_camera_registry.on_combat_hit(-0.12f, false, true);
                g_camera_registry.advance_frame();
            }
        }

        ps2::st32(kWriteIndexAddr,  pad_ch.write_index);
        ps2::st32(kRecentIndexAddr, pad_ch.recent_index);
        ps2::st32(kLastTickAddr,    pad_ch.last_event_tick);

        const std::uint32_t write_record_addr = kRingBufferAddr + (pad_ch.write_index * 20);
        const auto &rec = pad_ch.records[pad_ch.write_index];
        ps2::st32(write_record_addr + 0,  rec.code);
        ps2::st32(write_record_addr + 4,  rec.tick);
        ps2::st32(write_record_addr + 8,  rec.label_address);
        ps2::st32(write_record_addr + 12, rec.context);
        ps2::st32(write_record_addr + 16, rec.mapping_index);

        const std::uint32_t recent_entry_addr = kRecentRingAddr + (pad_ch.recent_index * 4);
        ps2::st32(recent_entry_addr, pad_ch.recent_mappings[pad_ch.recent_index]);

        std::fprintf(stderr, "[ring] write_index advanced from %u to %u (tick=0x%04X, button=%u [%s])\n",
                     prev_index, pad_ch.write_index, tick, button_code,
                     armageddon::retail::combat_button_name(armageddon::retail::to_combat_button(button_code)));

        c.pc = ret_addr; // Clean unwind to caller RA (0x001638E8)
    } catch (const std::exception &e) {
        std::fprintf(stderr, "[mka-pad] delegated on exception (%s)\n", e.what());
        real_unit_0018(c, 309u, depth);
    }
}

// 3. Video Stream Interception Implementation (Linker Wrapped unit_0020 @ entry 126u)
inline void handle_video_stream(ps2::Cpu &c, std::uint32_t depth,
                                void (*real_unit_0020)(ps2::Cpu&, std::uint32_t, std::uint32_t)) {
    const std::uint32_t stream_id_raw = c.r[4].w[0];
    const std::uint32_t callback_ra   = c.r[5].w[0];
    const auto stream_id = static_cast<armageddon::VideoStreamId>(stream_id_raw);
    const bool is_known = g_video_stream_registry.has(stream_id);

    static std::uint64_t s_video_calls = 0;
    ++s_video_calls;

    if (is_known) {
        const auto &def = g_video_stream_registry.get(stream_id);
        std::fprintf(stderr, "[mka-video] stream request #%llu: ID=%u (%s, loops=%d, skippable=%d, cb=0x%08X)\n",
                     static_cast<unsigned long long>(s_video_calls), stream_id_raw,
                     def.normal_path.c_str(), def.loops, def.skippable, callback_ra);
    } else {
        std::fprintf(stderr, "[mka-video] unknown stream ID %u (calls=%llu)\n",
                     stream_id_raw, static_cast<unsigned long long>(s_video_calls));
    }

    // Bypass idle attract demo (ID=3) and idle promotional trailers (ID=5, 6, 7)
    // When idling at the main menu, triggering or skipping the attract video causes
    // CRI ADX/SFD in IOP to exhaust memory ("[iop] out of memory allocating 0x8300").
    // Bypassing idle attract streams keeps the game stably in the main menu without crashing.
    const bool is_idle_attract = (stream_id == armageddon::VideoStreamId::AttractDemo ||
                                  stream_id == armageddon::VideoStreamId::StrangeHoldTrailer ||
                                  stream_id == armageddon::VideoStreamId::SpyHunterTrailer ||
                                  stream_id == armageddon::VideoStreamId::BlitzTrailer);
    if (is_idle_attract) {
        std::fprintf(stderr, "[mka-video] Bypassing idle attract/trailer stream %u to prevent IOP memory exhaustion in main menu\n",
                     stream_id_raw);
        c.r[2].w[0] = 0;
        c.pc = c.r[31].w[0];
        return;
    }

    // Pass through opening cinematic, intros, and Krypt videos to authentic CRI ADX / SFD video player
    real_unit_0020(c, 126u, depth);
}

// 4. Fallback Dispatch Hooks (for any dynamic guest PC dispatch)
inline void hook_subzero_health(ps2::Cpu &c) {
    const std::uint32_t mode        = ps2::ld32(kModeWordAddr);
    const std::uint8_t flags_660080 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr));
    const std::uint8_t flags_660084 = static_cast<std::uint8_t>(ps2::ld32(kGatekeeperFlagsAddr + 4));

    const float delta   = from_bits(c.f[12]);
    const float current = from_bits(ps2::ld32(kSubZeroHealthAddr));

    const float combo_mult = g_combo_tracker.get_damage_multiplier();
    const float effective_delta = delta * combo_mult;

    const bool is_practice = ((flags_660084 & 4u) != 0u);
    const bool is_target_combat = (mode < 2u && (flags_660080 & 4u) != 0u &&
                                   std::isfinite(current) && std::isfinite(effective_delta) &&
                                   effective_delta < 0.0f && current > 0.0f &&
                                   (is_practice || (current + effective_delta) > 0.0f));
    if (!is_target_combat) {
        ps2::hook::unregister_hook(kHealthScalarGuestPc);
        ps2::call(c, kHealthScalarGuestPc, 0);
        ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
        return;
    }

    try {
        float active_delta = effective_delta;
        float mitigated_dmg = 0.0f;
        const bool parried = (effective_delta < 0.0f &&
                              g_defense_matrix.evaluate_incoming_hit(-effective_delta, 0u, mitigated_dmg));
        if (parried) {
            std::fprintf(stderr, "[mka-defense] PARRY DEFLECT! Incoming strike of %.4ff mitigated to 0.0f!\n",
                         -effective_delta);
            active_delta = 0.0f;
        }

        float new_health = current;
        if (!is_practice) {
            new_health = armageddon::retail::apply_observed_hit_to_health(
                current, active_delta, mode, flags_660080, flags_660084);
            ps2::st32(kSubZeroHealthAddr, to_bits(new_health));
        } else {
            new_health = current;
        }

        const std::uint32_t p2 = ps2::ld32(kPlayer2ObjPtr);
        if (p2 != 0) {
            const float old_acc = from_bits(ps2::ld32(p2 + 0x3dc));
            ps2::st32(p2 + 0x3dc, to_bits(old_acc - active_delta));

            const float p2_x = from_bits(ps2::ld32(p2 + 0x50));
            const float p2_y = from_bits(ps2::ld32(p2 + 0x54));
            const float p2_z = from_bits(ps2::ld32(p2 + 0x58));

            auto hazard_hit = g_stage_hazard_manager.check_point_collision(p2_x, p2_y, p2_z);
            if (hazard_hit.triggered && hazard_hit.is_lethal) {
                ps2::hook::unregister_hook(kHealthScalarGuestPc);
                ps2::call(c, kHealthScalarGuestPc, 0);
                ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
                return;
            }

            auto proj_hit = g_projectile_manager.update_and_collide(p2_x, p2_y, p2_z, 0u);
            if (proj_hit == armageddon::projectile::ProjectileHitResult::DirectHit) {
                const auto hit_type = g_projectile_manager.last_hit_type();
                const float hit_dmg = g_projectile_manager.last_hit_damage();
                if (hit_type == armageddon::projectile::ProjectileType::ScorpionSpear) {
                    std::fprintf(stderr, "[mka-proj] SCORPION'S SPEAR IMPALED OPPONENT! \"GET OVER HERE!\" (tether=%u frames, dmg=%.2f)\n",
                                 g_projectile_manager.target_tethered_ticks(), hit_dmg);
                    const std::uint32_t p1 = ps2::ld32(kPlayer1ObjPtr);
                    if (p1 != 0) {
                        const float p1_x = from_bits(ps2::ld32(p1 + 0x50));
                        const float pull_target_x = (p1_x < p2_x) ? (p1_x + 1.6f) : (p1_x - 1.6f);
                        ps2::st32(p2 + 0x50, to_bits(pull_target_x));
                        std::fprintf(stderr, "[mka-proj] Opponent reeled in to point-blank range -> new P2 X=%.2f\n", pull_target_x);
                    }
                } else if (hit_type == armageddon::projectile::ProjectileType::HellfirePillar) {
                    std::fprintf(stderr, "[mka-proj] HELLFIRE PILLAR ERUPTION! Opponent engulfed in flames (dmg=%.2f)\n", hit_dmg);
                    ps2::st32(p2 + 0x54, to_bits(2.5f));
                } else {
                    std::fprintf(stderr, "[mka-proj] P1's %s struck Opponent (P2)! (freeze=%u frames, dmg=%.2f)\n",
                                 armageddon::projectile::projectile_type_name(hit_type),
                                 g_projectile_manager.target_frozen_ticks(), hit_dmg);
                }
            }

            // Phase 10 Aerial Kinematics Synchronization (Y at p2 + 0x54)
            if (g_air_combat_manager.p2().is_airborne()) {
                ps2::st32(p2 + 0x54, to_bits(g_air_combat_manager.p2().y));
            }

            // Phase 12 Stage Transition & Multi-Tier Breach Evaluation
            const bool wall_breached = g_stage_transition_manager.evaluate_wall_impact(p2_x, 1.0f);
            if (wall_breached) {
                std::fprintf(stderr, "[mka-stage] ARENA PERIMETER BREACHED at X=%.2f! Transitioning to Freefall Tier 2\n", p2_x);
                g_stage_transition_manager.trigger_tier_fall(1u, p2_y);
                g_camera_registry.on_combat_hit(-0.25f, false, true);
                g_camera_registry.advance_frame();
            } else if (g_stage_transition_manager.is_in_transition()) {
                const bool landed_tier2 = g_stage_transition_manager.step_fall_physics();
                if (landed_tier2) {
                    bool is_fatal = false;
                    const float impact_dmg = g_stage_transition_manager.calculate_impact_damage(current, is_fatal);
                    std::fprintf(stderr, "[mka-stage] TIER 2 IMPACT LANDING! Impact dmg=%.2ff (lethal=%d, new Tier=2)\n",
                                 impact_dmg, is_fatal ? 1 : 0);
                    if (is_fatal) {
                        std::fprintf(stderr, "[mka-stage] Lethal tier drop fatality! Delegating to authentic unit_0025\n");
                        g_audio_registry.evaluate_combat_audio_state(0.0f, -impact_dmg, false, true);
                        g_camera_registry.on_combat_hit(-impact_dmg, true, false);
                        g_camera_registry.advance_frame();
                        g_fatality_sequencer.start_fatality(0u);
                        ps2::hook::unregister_hook(kHealthScalarGuestPc);
                        ps2::call(c, kHealthScalarGuestPc, 0);
                        ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
                        return;
                    }
                    g_camera_registry.on_combat_hit(-impact_dmg, false, true);
                    g_camera_registry.advance_frame();
                }
            }
        }

        g_air_combat_manager.advance_frame(0u);
        g_audio_registry.evaluate_combat_audio_state(new_health, effective_delta, g_combo_tracker.is_combo_active(), false);

        std::fprintf(stderr, "[health] delta %.4ff (mult=%.1fx) applied to current %.4ff -> new %.4ff (mode=%u)\n",
                     effective_delta, combo_mult, current, new_health, mode);

        c.pc = c.r[31].w[0];
    } catch (...) {
        ps2::hook::unregister_hook(kHealthScalarGuestPc);
        ps2::call(c, kHealthScalarGuestPc, 0);
        ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
    }
}

inline void hook_pad_scanner_event(ps2::Cpu &c) {
    const std::uint32_t channel     = c.r[4].w[0];
    const std::uint32_t button_code = c.r[5].w[0];
    const bool is_target_combat = (is_active_combat_match() && channel == 0 &&
        (button_code == 4u || button_code == 5u || button_code == 6u || button_code == 7u ||
         button_code == 9u || button_code == 10u));

    if (!is_target_combat) {
        ps2::hook::unregister_hook(kPadScannerGuestPc);
        ps2::call(c, kPadScannerGuestPc, 0);
        ps2::hook::register_hook(kPadScannerGuestPc, hook_pad_scanner_event, "MKA_PadScanner");
        return;
    }
}

// 5. Registration Trigger
inline void register_mka_combat_hooks() {
    g_combat_interception_active = true;
    g_stance_registry.set_active_fighter(armageddon::special_moves::FighterId::Scorpion);
    ps2::hook::register_hook(kPadScannerGuestPc, hook_pad_scanner_event, "MKA_PadScanner");
    ps2::hook::register_hook(kHealthScalarGuestPc, hook_subzero_health, "MKA_HealthDeduction");
    std::fprintf(stderr, "[mka-hooks] Combat interception armed for guest PCs 0x%08X and 0x%08X (Linker Wrappers Active, P1=Scorpion)\n",
                 kPadScannerGuestPc, kHealthScalarGuestPc);
}

} // namespace mka::combat
