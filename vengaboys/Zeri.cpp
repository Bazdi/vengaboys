#define NOMINMAX
#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <algorithm>
#include <cstdarg>
#include <chrono>
#include <unordered_set>
#include <limits>
#include <random>
#include <fstream>
#include "zeri.hpp"
#include "ordermanager.hpp"
#include "champion_spell_names.hpp"

namespace zeri {
    // Forward declarations for functions used before they're defined
    void record_wall_jump(const math::vector3& start_pos, const math::vector3& end_pos, bool successful);
    bool is_wall_between(const math::vector3& start, const math::vector3& end);

    Settings settings;
    menu_category* menu = nullptr;
    bool initialized = false;
    bool debug_enabled = false;
    CombatState combat_state{};
    SpellTracker spell_tracker{};
    std::vector<JumpSpot> jump_spots;
    std::vector<JumpSpotRecord> recorded_jumps;
    std::mt19937 rng(std::random_device{}());

    uint32_t infotab_text_id = 0;
    menu_category* jump_spots_menu = nullptr;

    // Tracking variables for E jump detection
    math::vector3 e_start_pos;
    float e_cast_time = 0.f;

    game_object* get_best_q_target(float max_range = 825.f) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return nullptr;

        // First priority: Enemy champions in range
        game_object* best_target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                hero->is_visible() && hero->get_position().distance(player->get_position()) <= max_range;
            });

        if (best_target) return best_target;

        // Second priority: Minions in range
        float closest_minion_dist = max_range;
        game_object* best_minion = nullptr;

        for (auto minion : g_sdk->object_manager->get_minions()) {
            if (!minion->is_valid() || minion->is_dead() ||
                minion->get_team_id() == player->get_team_id() ||
                !minion->is_targetable() || !minion->is_visible())
                continue;

            float dist = minion->get_position().distance(player->get_position());
            if (dist < closest_minion_dist) {
                closest_minion_dist = dist;
                best_minion = minion;
            }
        }

        if (best_minion) return best_minion;

        // Third priority: Jungle monsters (only if no minions are in range)
        float closest_monster_dist = max_range;
        game_object* best_monster = nullptr;

        for (auto monster : g_sdk->object_manager->get_monsters()) {
            if (!monster->is_valid() || monster->is_dead() ||
                monster->get_team_id() == player->get_team_id() ||
                !monster->is_targetable() || !monster->is_visible())
                continue;

            float dist = monster->get_position().distance(player->get_position());
            if (dist < closest_monster_dist) {
                closest_monster_dist = dist;
                best_monster = monster;
            }
        }

        return best_monster;
    }

    // Static function for tracking wall jumps
    static void track_wall_jump_fn() {
        if (e_cast_time > 0.f) {
            float current_time = g_sdk->clock_facade->get_game_time();
            static float next_check_time = 0.f;
            static float check_interval = 0.1f;

            if (current_time >= next_check_time) {
                next_check_time = current_time + check_interval;

                if (current_time - e_cast_time >= 0.5f) {
                    auto player = g_sdk->object_manager->get_local_player();
                    if (!player) return;

                    // Get the current position and see if it was a significant jump
                    math::vector3 end_pos = player->get_position();
                    float jump_distance = e_start_pos.distance(end_pos);

                    // If we moved significantly, record it as a wall jump
                    if (jump_distance > 200.f) {
                        // Check if this jump crossed a wall
                        bool crossed_wall = is_wall_between(e_start_pos, end_pos);

                        // Record this jump with success status
                        bool successful = crossed_wall && jump_distance > 200.f;
                        record_wall_jump(e_start_pos, end_pos, successful);

                        g_sdk->log_console("[Zeri] Wall jump recorded from (%.1f, %.1f, %.1f) to (%.1f, %.1f, %.1f)",
                            e_start_pos.x, e_start_pos.y, e_start_pos.z,
                            end_pos.x, end_pos.y, end_pos.z);
                    }

                    // Reset tracking variables
                    e_cast_time = 0.f;

                    // Unregister this callback
                    g_sdk->event_manager->unregister_callback(event_manager::event::game_update,
                        reinterpret_cast<void*>(track_wall_jump_fn));
                }
            }
        }
    }

    // Core debug logging function
    void log_debug(const char* format, ...) {
        if (!debug_enabled) return;

        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        g_sdk->log_console("[Zeri Debug] %s", buffer);
    }

    // Check if a spell can be cast
    bool can_cast_spell(int spell_slot, float delay) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("can_cast_spell: No player found");
            return false;
        }

        auto spell = player->get_spell(spell_slot);
        if (!spell) {
            log_debug("can_cast_spell: No spell found for slot %d", spell_slot);
            return false;
        }

        if (!vengaboys::OrderManager::get_instance().can_cast(player, spell_slot, delay)) {
            log_debug("can_cast_spell: OrderManager preventing cast");
            return false;
        }

        return true;
    }

    // Damage calculation functions
    float calculate_q_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(0);
        if (!spell || spell->get_level() == 0) return 0.f;

        // Base damage of Burst Fire (Q)
        const float base_damage[] = { 10.f, 15.f, 20.f, 25.f, 30.f };
        // AD scaling
        const float ad_ratio = 1.1f;
        // Total AD
        const float total_ad = player->get_attack_damage();

        float total_damage = base_damage[spell->get_level() - 1] + (total_ad * ad_ratio);

        // Check if R is active for additional damage
        if (combat_state.r_active) {
            total_damage *= 1.1f; // 10% increased damage during R
        }

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, total_damage);
    }

    float calculate_w_damage(game_object* target, bool charged) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(1);
        if (!spell || spell->get_level() == 0) return 0.f;

        // Base damage of Ultrashock Laser (W)
        const float base_damage[] = { 20.f, 55.f, 90.f, 125.f, 160.f };
        // AD scaling
        const float ad_ratio = 1.3f;
        // AP scaling
        const float ap_ratio = 0.6f;

        float total_damage = base_damage[spell->get_level() - 1] +
            (player->get_attack_damage() * ad_ratio) +
            (player->get_ability_power() * ap_ratio);

        // If fully charged, damage is increased
        if (charged) {
            total_damage *= 1.7f; // 70% bonus damage when charged
        }

        // Check if R is active for additional damage
        if (combat_state.r_active) {
            total_damage *= 1.1f; // 10% increased damage during R
        }

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, total_damage);
    }

    float calculate_e_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(2);
        if (!spell || spell->get_level() == 0) return 0.f;

        // Base damage of Spark Surge (E)
        const float base_damage[] = { 40.f, 65.f, 90.f, 115.f, 140.f };
        // AP scaling
        const float ap_ratio = 0.9f;

        float total_damage = base_damage[spell->get_level() - 1] +
            (player->get_ability_power() * ap_ratio);

        // Check if R is active for additional damage
        if (combat_state.r_active) {
            total_damage *= 1.1f; // 10% increased damage during R
        }

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float calculate_r_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(3);
        if (!spell || spell->get_level() == 0) return 0.f;

        // Base damage of Lightning Crash (R)
        const float base_damage[] = { 150.f, 250.f, 350.f };
        // AD scaling
        const float ad_ratio = 0.8f;
        // AP scaling
        const float ap_ratio = 0.8f;

        float total_damage = base_damage[spell->get_level() - 1] +
            (player->get_attack_damage() * ad_ratio) +
            (player->get_ability_power() * ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float calculate_full_combo_damage(game_object* target) {
        if (!target) return 0.f;

        float total_damage = 0.f;

        // Add Q damage (assume 3 hits)
        if (can_cast_spell(0, 0.f))
            total_damage += calculate_q_damage(target) * 3;

        // Add W damage (prefer charged)
        if (can_cast_spell(1, 0.f))
            total_damage += calculate_w_damage(target, settings.w_charged_preferred);

        // Add E damage
        if (can_cast_spell(2, 0.f))
            total_damage += calculate_e_damage(target);

        // Add R damage
        if (can_cast_spell(3, 0.f))
            total_damage += calculate_r_damage(target);

        return total_damage;
    }

    // Q humanization functions
    float get_humanized_q_delay() {
        if (!settings.q_humanization.enabled)
            return 0.f;

        float min_delay = settings.q_humanization.min_delay;
        float max_delay = settings.q_humanization.max_delay;

        // Adjust delay based on attack speed if enabled
        if (settings.q_humanization.dynamic_delay) {
            auto player = g_sdk->object_manager->get_local_player();
            if (player) {
                float attack_speed_mod = player->get_attack_speed_mod();
                min_delay = (std::max)(0.01f, min_delay / (attack_speed_mod * settings.q_humanization.delay_scale));
                max_delay = (std::max)(0.03f, max_delay / (attack_speed_mod * settings.q_humanization.delay_scale));
            }
        }

        // Generate random delay within range if randomization is enabled
        if (settings.q_humanization.randomize) {
            std::uniform_real_distribution<float> dist(min_delay, max_delay);
            return dist(rng);
        }

        return min_delay;
    }

    bool should_cast_q_now() {
        if (!settings.q_humanization.enabled)
            return true;

        float current_time = g_sdk->clock_facade->get_game_time();

        // Check if minimum delay has passed
        if (current_time - combat_state.last_q_time < combat_state.next_q_delay) {
            return false;
        }

        // If we're implementing patterns avoidance
        if (settings.q_humanization.avoid_patterns && combat_state.q_cast_count > 10) {
            // Occasionally add a slightly longer delay to break patterns (only 1 in 15 casts)
            if (combat_state.q_cast_count % 15 == 0) {
                std::uniform_real_distribution<float> extra_delay(0.01f, 0.08f);
                combat_state.next_q_delay = get_humanized_q_delay() + extra_delay(rng);
                return false;
            }
        }

        // Implement miss chance - but keep it subtle
        if (settings.q_humanization.miss_chance > 0) {
            std::uniform_int_distribution<int> miss_roll(1, 100);
            if (miss_roll(rng) <= settings.q_humanization.miss_chance) {
                combat_state.q_missed_count++;
                log_debug("Q humanization: Intentionally 'missed' Q input");
                // Add a very small delay before next attempt
                combat_state.next_q_delay = 0.03f;
                return false;
            }
        }

        // Calculate next delay for next cast
        combat_state.next_q_delay = get_humanized_q_delay();
        return true;
    }

    void update_q_humanization_stats() {
        if (!settings.q_humanization.enabled)
            return;

        float current_time = g_sdk->clock_facade->get_game_time();

        // Store current delay for pattern analysis
        if (combat_state.recent_cast_delays.size() >= 10) {
            combat_state.recent_cast_delays.erase(combat_state.recent_cast_delays.begin());
        }
        combat_state.recent_cast_delays.push_back(current_time - combat_state.last_q_time);

        // Increment cast counter
        combat_state.q_cast_count++;
        combat_state.last_q_time = current_time;
    }

    void render_q_humanization_stats() {
        if (!settings.draw_q_humanization_stats)
            return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        int window_height = g_sdk->renderer->get_window_height();
        int window_width = g_sdk->renderer->get_window_width();

        std::string stats_text = "Q Humanization Stats:";
        g_sdk->renderer->add_text(stats_text, 14.f, math::vector2(10.f, window_height - 120.f), 0, 0xFFFFFFFF);

        stats_text = "Cast Count: " + std::to_string(combat_state.q_cast_count);
        g_sdk->renderer->add_text(stats_text, 14.f, math::vector2(10.f, window_height - 100.f), 0, 0xFFFFFFFF);

        stats_text = "Missed Inputs: " + std::to_string(combat_state.q_missed_count);
        g_sdk->renderer->add_text(stats_text, 14.f, math::vector2(10.f, window_height - 80.f), 0, 0xFFFFFFFF);

        stats_text = "Current Delay: " + std::to_string(combat_state.next_q_delay).substr(0, 5) + "s";
        g_sdk->renderer->add_text(stats_text, 14.f, math::vector2(10.f, window_height - 60.f), 0, 0xFFFFFFFF);

        // Draw recent delays as bar graph
        if (!combat_state.recent_cast_delays.empty()) {
            const float bar_width = 20.f;
            const float max_height = 60.f;
            const float x_start = 10.f;
            const float y_baseline = window_height - 20.f;

            for (size_t i = 0; i < combat_state.recent_cast_delays.size(); i++) {
                float delay = combat_state.recent_cast_delays[i];
                float height = (std::min)(max_height, delay * 500.f); // Scale for visibility

                g_sdk->renderer->add_rectangle_filled(
                    math::rect(
                        y_baseline - height,
                        x_start + i * (bar_width + 2.f),
                        y_baseline,
                        x_start + (i + 1) * bar_width + i * 2.f
                    ),
                    0xFF00AAFF
                );
            }
        }
    }

    // Spell casting implementations
    bool cast_q(game_object* target, const math::vector3* position) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(0, 0.f)) {
            log_debug("cast_q: Q not ready");
            return false;
        }

        // Apply humanization (more subtle than before)
        if (!should_cast_q_now()) {
            log_debug("cast_q: Humanization prevented cast");
            return false;
        }

        try {
            // If no target/position provided, find the best target
            if (!target && !position) {
                target = get_best_q_target(settings.q_range);

                // If we still don't have a target, use cursor position
                if (!target) {
                    auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
                    log_debug("Casting Q at cursor (%.1f, %.1f, %.1f)", cursor_pos.x, cursor_pos.y, cursor_pos.z);
                    if (vengaboys::OrderManager::get_instance().cast_spell(0, cursor_pos)) {
                        update_q_humanization_stats();
                        spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                        return true;
                    }
                    return false;
                }
            }

            if (position) {
                log_debug("Casting Q at position (%.1f, %.1f, %.1f)", position->x, position->y, position->z);
                if (vengaboys::OrderManager::get_instance().cast_spell(0, *position)) {
                    update_q_humanization_stats();
                    spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                    return true;
                }
            }
            else if (target) {
                log_debug("Casting Q on target %s", target->get_char_name().c_str());
                // Get precise position for targeting
                math::vector3 target_pos = target->get_position();

                // Make sure the position is within range
                float distance = player->get_position().distance(target_pos);
                if (distance > settings.q_range) {
                    math::vector3 direction = (target_pos - player->get_position()).normalized();
                    target_pos = player->get_position() + direction * (settings.q_range * 0.95f);
                    log_debug("Target out of range, adjusting position");
                }

                // Count potential hits for debugging
                int potential_hits = 0;
                for (auto obj : g_sdk->object_manager->get_minions()) {
                    if (!obj->is_valid() || obj->is_dead() || obj->get_team_id() == player->get_team_id())
                        continue;

                    if (obj->get_position().distance(target_pos) < 225.f) {
                        potential_hits++;
                    }
                }

                log_debug("Q targeting: could hit %d targets", potential_hits);

                if (vengaboys::OrderManager::get_instance().cast_spell(0, target_pos)) {
                    update_q_humanization_stats();
                    spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                    return true;
                }
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_q: %s", e.what());
        }
        return false;
    }

    bool cast_w(game_object* target, const math::vector3* position, bool charge) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(1, 0.f)) {
            log_debug("cast_w: W not ready");
            return false;
        }

        if (player->get_par() < settings.w_min_mana / 100.f * player->get_max_par()) {
            log_debug("cast_w: Not enough mana");
            return false;
        }

        try {
            float current_time = g_sdk->clock_facade->get_game_time();

            if (charge) {
                // Begin charging W
                log_debug("Starting to charge W");
                if (vengaboys::OrderManager::get_instance().cast_spell(1, player)) {
                    combat_state.w_charging = true;
                    combat_state.w_charge_start_time = current_time;
                    return true;
                }
            }
            else {
                // Instant cast W
                if (position) {
                    log_debug("Casting W at position (%.1f, %.1f, %.1f)", position->x, position->y, position->z);
                    if (vengaboys::OrderManager::get_instance().cast_spell(1, *position)) {
                        spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                        combat_state.last_w_time = current_time;
                        return true;
                    }
                }
                else if (target) {
                    // For targetted W, use prediction
                    if (sdk::prediction) {
                        pred_sdk::spell_data spell_data;
                        spell_data.spell_type = pred_sdk::spell_type::linear;
                        spell_data.range = settings.w_range;
                        spell_data.radius = 40.f; // W width
                        spell_data.delay = 0.4f;  // W delay
                        spell_data.projectile_speed = 2600.f; // W speed
                        spell_data.expected_hitchance = settings.w_min_hitchance;

                        auto prediction = sdk::prediction->predict(target, spell_data);
                        if (prediction.is_valid && prediction.hitchance >= settings.w_min_hitchance) {
                            log_debug("Casting W at predicted position");
                            if (vengaboys::OrderManager::get_instance().cast_spell(1, prediction.cast_position)) {
                                spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                                combat_state.last_w_time = current_time;
                                return true;
                            }
                        }
                    }
                    else {
                        log_debug("Casting W on target %s", target->get_char_name().c_str());
                        if (vengaboys::OrderManager::get_instance().cast_spell(1, target->get_position())) {
                            spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                            combat_state.last_w_time = current_time;
                            return true;
                        }
                    }
                }
                else {
                    // Cast at cursor position if no target/position specified
                    auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
                    log_debug("Casting W at cursor (%.1f, %.1f, %.1f)", cursor_pos.x, cursor_pos.y, cursor_pos.z);
                    if (vengaboys::OrderManager::get_instance().cast_spell(1, cursor_pos)) {
                        spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                        combat_state.last_w_time = current_time;
                        return true;
                    }
                }
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_w: %s", e.what());
        }
        return false;
    }

    bool release_w(game_object* target, const math::vector3* position) {
        if (!combat_state.w_charging) {
            log_debug("release_w: W not charging");
            return false;
        }

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        float charge_time = current_time - combat_state.w_charge_start_time;

        // Ensure minimum charge time
        if (charge_time < settings.w_min_charge) {
            log_debug("release_w: Charge time too short (%.2f < %.2f)", charge_time, settings.w_min_charge);
            return false;
        }

        try {
            if (position) {
                log_debug("Releasing W at position");
                if (vengaboys::OrderManager::get_instance().cast_spell(1, *position)) {
                    combat_state.w_charging = false;
                    spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                    combat_state.last_w_time = current_time;
                    return true;
                }
            }
            else if (target) {
                // For targetted W release, use prediction
                if (sdk::prediction) {
                    pred_sdk::spell_data spell_data;
                    spell_data.spell_type = pred_sdk::spell_type::linear;
                    spell_data.range = settings.w_range;
                    spell_data.radius = 40.f; // W width
                    spell_data.delay = 0.4f;  // W delay
                    spell_data.projectile_speed = 2600.f; // W speed
                    spell_data.expected_hitchance = settings.w_min_hitchance;

                    auto prediction = sdk::prediction->predict(target, spell_data);
                    if (prediction.is_valid && prediction.hitchance >= settings.w_min_hitchance) {
                        log_debug("Releasing W at predicted position");
                        if (vengaboys::OrderManager::get_instance().cast_spell(1, prediction.cast_position)) {
                            combat_state.w_charging = false;
                            spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                            combat_state.last_w_time = current_time;
                            return true;
                        }
                    }
                }
                else {
                    log_debug("Releasing W on target %s", target->get_char_name().c_str());
                    if (vengaboys::OrderManager::get_instance().cast_spell(1, target->get_position())) {
                        combat_state.w_charging = false;
                        spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                        combat_state.last_w_time = current_time;
                        return true;
                    }
                }
            }
            else {
                // Release at cursor position if no target/position specified
                auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
                log_debug("Releasing W at cursor");
                if (vengaboys::OrderManager::get_instance().cast_spell(1, cursor_pos)) {
                    combat_state.w_charging = false;
                    spell_tracker.update_cooldown(1, current_time + player->get_spell(1)->get_cooldown());
                    combat_state.last_w_time = current_time;
                    return true;
                }
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in release_w: %s", e.what());
        }
        return false;
    }

    bool cast_e(game_object* target, const math::vector3* position) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(2, 0.f)) {
            log_debug("cast_e: E not ready");
            return false;
        }

        if (player->get_par() < settings.e_min_mana / 100.f * player->get_max_par()) {
            log_debug("cast_e: Not enough mana");
            return false;
        }

        try {
            if (position) {
                log_debug("Casting E to position (%.1f, %.1f, %.1f)", position->x, position->y, position->z);
                if (vengaboys::OrderManager::get_instance().cast_spell(2, *position)) {
                    spell_tracker.update_cooldown(2, g_sdk->clock_facade->get_game_time() + player->get_spell(2)->get_cooldown());
                    combat_state.last_e_time = g_sdk->clock_facade->get_game_time();
                    return true;
                }
            }
            else if (target) {
                log_debug("Casting E on target %s", target->get_char_name().c_str());
                if (vengaboys::OrderManager::get_instance().cast_spell(2, target->get_position())) {
                    spell_tracker.update_cooldown(2, g_sdk->clock_facade->get_game_time() + player->get_spell(2)->get_cooldown());
                    combat_state.last_e_time = g_sdk->clock_facade->get_game_time();
                    return true;
                }
            }
            else {
                // Cast at cursor position if no target/position specified
                auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
                log_debug("Casting E at cursor");
                if (vengaboys::OrderManager::get_instance().cast_spell(2, cursor_pos)) {
                    spell_tracker.update_cooldown(2, g_sdk->clock_facade->get_game_time() + player->get_spell(2)->get_cooldown());
                    combat_state.last_e_time = g_sdk->clock_facade->get_game_time();
                    return true;
                }
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_e: %s", e.what());
        }
        return false;
    }

    bool cast_r(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(3, 0.f)) {
            log_debug("cast_r: R not ready");
            return false;
        }

        if (player->get_par() < settings.r_min_mana / 100.f * player->get_max_par()) {
            log_debug("cast_r: Not enough mana");
            return false;
        }

        try {
            log_debug("Casting R");
            if (vengaboys::OrderManager::get_instance().cast_spell(3, player)) {
                spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + player->get_spell(3)->get_cooldown());
                combat_state.last_r_time = g_sdk->clock_facade->get_game_time();
                combat_state.r_active = true;
                combat_state.r_activation_time = g_sdk->clock_facade->get_game_time();
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_r: %s", e.what());
        }
        return false;
    }

    // Wall jump functionality
    void initialize_jump_spots() {
        jump_spots.clear();

        // These are example jump spots - you'd want to add actual map locations
        // Dragon pit to mid river
        jump_spots.push_back({ "Dragon Pit Exit", math::vector3(9800.f, 0.f, 4300.f), math::vector3(9400.f, 0.f, 4600.f) });

        // Baron pit to river
        jump_spots.push_back({ "Baron Pit Exit", math::vector3(4600.f, 0.f, 10500.f), math::vector3(4900.f, 0.f, 10800.f) });

        // Blue side blue buff wall
        jump_spots.push_back({ "Blue Side Blue", math::vector3(3800.f, 0.f, 7900.f), math::vector3(3500.f, 0.f, 8200.f) });

        // Red side blue buff wall
        jump_spots.push_back({ "Red Side Blue", math::vector3(11000.f, 0.f, 6500.f), math::vector3(11300.f, 0.f, 6200.f) });

        // Mid lane top wall
        jump_spots.push_back({ "Mid Top Wall", math::vector3(6500.f, 0.f, 8500.f), math::vector3(6700.f, 0.f, 8200.f) });

        // Mid lane bottom wall
        jump_spots.push_back({ "Mid Bot Wall", math::vector3(8500.f, 0.f, 6500.f), math::vector3(8200.f, 0.f, 6700.f) });

        // Raptor camp walls
        jump_spots.push_back({ "Blue Side Raptors", math::vector3(6800.f, 0.f, 5400.f), math::vector3(7100.f, 0.f, 5100.f) });
        jump_spots.push_back({ "Red Side Raptors", math::vector3(7900.f, 0.f, 9300.f), math::vector3(7600.f, 0.f, 9600.f) });

        log_debug("Initialized %zu jump spots", jump_spots.size());
    }

    bool is_wall_between(const math::vector3& start, const math::vector3& end) {
        const int num_samples = 10;
        for (int i = 1; i < num_samples; i++) {
            float t = static_cast<float>(i) / num_samples;
            math::vector3 sample_point = start + (end - start) * t;
            math::vector3 pathable_point = sample_point;
            if (!g_sdk->nav_mesh->is_pathable(pathable_point)) {
                return true;
            }
        }
        return false;
    }

    bool test_wall_jump(const math::vector3& start, const math::vector3& end) {
        // Tests if Zeri can jump from start to end
        if (!is_wall_between(start, end)) {
            return false; // No wall to jump over
        }

        float distance = start.distance(end);
        if (distance > settings.e_range) {
            return false; // Too far to jump
        }

        // Check if end point is pathable (can stand there)
        math::vector3 pathable_end = end; // Create a copy that can be modified
        if (!g_sdk->nav_mesh->is_pathable(pathable_end)) {
            return false;
        }

        return true;
    }

    bool find_nearest_jump_spot(const math::vector3& position, JumpSpot& result, float max_distance) {
        float closest_distance = max_distance;
        bool found = false;

        for (const auto& spot : jump_spots) {
            if (!spot.enabled) continue;

            float dist_to_start = position.distance(spot.start_pos);
            if (dist_to_start < closest_distance) {
                closest_distance = dist_to_start;
                result = spot;
                found = true;
            }
        }

        return found;
    }

    void record_wall_jump(const math::vector3& start_pos, const math::vector3& end_pos, bool successful) {
        JumpSpotRecord record;
        record.start_pos = start_pos;
        record.end_pos = end_pos;
        record.time_recorded = g_sdk->clock_facade->get_game_time();
        record.successful = successful;

        recorded_jumps.push_back(record);

        // Log to console for immediate feedback
        g_sdk->log_console("[Zeri] Wall jump recorded: %s", successful ? "SUCCESSFUL" : "FAILED");
        g_sdk->log_console("[Zeri] Start: (%.1f, %.1f, %.1f)", start_pos.x, start_pos.y, start_pos.z);
        g_sdk->log_console("[Zeri] End: (%.1f, %.1f, %.1f)", end_pos.x, end_pos.y, end_pos.z);
        g_sdk->log_console("[Zeri] Add to jump_spots with: {\"Jump Spot %d\", math::vector3(%.1ff, %.1ff, %.1ff), math::vector3(%.1ff, %.1ff, %.1ff)},",
            recorded_jumps.size(),
            start_pos.x, start_pos.y, start_pos.z,
            end_pos.x, end_pos.y, end_pos.z);
    }

    void save_jump_spots_to_file() {
        try {
            std::ofstream file("zeri_jump_spots.txt");
            if (!file.is_open()) {
                g_sdk->log_console("[Zeri] Failed to open file for saving jump spots");
                return;
            }

            file << "// Zeri jump spots recorded\n";
            for (size_t i = 0; i < recorded_jumps.size(); i++) {
                const auto& jump = recorded_jumps[i];
                file << "jump_spots.push_back({\"Jump Spot " << i << "\", ";
                file << "math::vector3(" << jump.start_pos.x << "f, " << jump.start_pos.y << "f, " << jump.start_pos.z << "f), ";
                file << "math::vector3(" << jump.end_pos.x << "f, " << jump.end_pos.y << "f, " << jump.end_pos.z << "f)});\n";
            }

            file.close();
            g_sdk->log_console("[Zeri] Saved %d jump spots to zeri_jump_spots.txt", recorded_jumps.size());
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[Zeri] Error saving jump spots: %s", e.what());
        }
    }

    void draw_jump_spots() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        float current_time = g_sdk->clock_facade->get_game_time();
        static float pulse_time = 0.f;
        pulse_time += 0.05f;
        if (pulse_time > 2.f * 3.14159f) pulse_time = 0.f;

        float pulse_factor = (std::sin(pulse_time) + 1.f) * 0.5f; // 0 to 1 pulsing

        for (const auto& spot : jump_spots) {
            if (!spot.enabled) continue;

            // Only draw spots within range
            float dist_to_spot = player->get_position().distance(spot.start_pos);
            if (dist_to_spot > settings.jump_spot_draw_range)
                continue;

            // Draw start position
            color spot_color = spot.color;
            spot_color.a = static_cast<uint8_t>(120 + 135 * pulse_factor); // Pulsing transparency

            g_sdk->renderer->add_circle_3d(spot.start_pos, spot.radius, 2.f, spot_color);

            // Draw end position
            color end_color = 0xFF0000FF; // Blue for end position
            end_color.a = spot_color.a;
            g_sdk->renderer->add_circle_3d(spot.end_pos, spot.radius * 0.7f, 2.f, end_color);

            // Draw line between them if debug is enabled
            if (settings.jump_spots_debug) {
                g_sdk->renderer->add_line_2d(
                    g_sdk->renderer->world_to_screen(spot.start_pos),
                    g_sdk->renderer->world_to_screen(spot.end_pos),
                    1.5f,
                    0xFFFFFFFF
                );

                math::vector2 name_pos = g_sdk->renderer->world_to_screen(spot.start_pos);
                name_pos.y -= 15.f;
                g_sdk->renderer->add_text(spot.name, 12.f, name_pos, 0, 0xFFFFFFFF);
            }
        }
    }

    void draw_recorded_jumps() {
        if (!settings.jump_spots_debug || recorded_jumps.empty())
            return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        for (size_t i = 0; i < recorded_jumps.size(); i++) {
            const auto& jump = recorded_jumps[i];

            // Only draw recent jumps or jumps near the player
            float time_since_recorded = g_sdk->clock_facade->get_game_time() - jump.time_recorded;
            float dist_to_jump = player->get_position().distance(jump.start_pos);

            if (time_since_recorded < 60.0f || dist_to_jump < settings.jump_spot_draw_range) {
                // Draw start position in green/red depending on success
                uint32_t start_color = jump.successful ? 0xFF00FF00 : 0xFF0000FF;
                g_sdk->renderer->add_circle_3d(jump.start_pos, 65.f, 2.f, start_color);

                // Draw end position in blue
                g_sdk->renderer->add_circle_3d(jump.end_pos, 45.f, 2.f, 0xFFFFAA00);

                // Draw line between them
                g_sdk->renderer->add_line_2d(
                    g_sdk->renderer->world_to_screen(jump.start_pos),
                    g_sdk->renderer->world_to_screen(jump.end_pos),
                    1.5f,
                    0xFFFFFFFF
                );

                // Draw index number for reference
                math::vector2 name_pos = g_sdk->renderer->world_to_screen(jump.start_pos);
                name_pos.y -= 15.f;
                std::string index_text = "Jump #" + std::to_string(i);
                g_sdk->renderer->add_text(index_text, 12.f, name_pos, 0, 0xFFFFFFFF);
            }
        }
    }

    void handle_wall_jump() {
        if (!settings.e_over_walls || !can_cast_spell(2, 0.f))
            return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto cursor_pos = g_sdk->hud_manager->get_cursor_position();

        // Find the nearest jump spot to player
        JumpSpot nearest_spot;
        if (find_nearest_jump_spot(player->get_position(), nearest_spot, 250.f)) {
            // Check if cursor is near the end position
            float cursor_to_end = cursor_pos.distance(nearest_spot.end_pos);
            if (cursor_to_end < 300.f) {
                log_debug("Attempting wall jump from %s to %s",
                    nearest_spot.start_pos.to_string().c_str(),
                    nearest_spot.end_pos.to_string().c_str());

                // Cast E toward the end position
                cast_e(nullptr, &nearest_spot.end_pos);
            }
        }
    }

    void debug_wall_jumps() {
        if (!settings.jump_spots_debug)
            return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto cursor_pos = g_sdk->hud_manager->get_cursor_position();

        // Display the testing status of cursor position
        math::vector2 text_pos(10.f, 200.f);

        // Check if cursor position is pathable
        math::vector3 pathable_cursor = cursor_pos; // Create a copy for is_pathable
        bool cursor_pathable = g_sdk->nav_mesh->is_pathable(pathable_cursor);
        std::string pathable_text = "Cursor position pathable: " + std::string(cursor_pathable ? "Yes" : "No");
        g_sdk->renderer->add_text(pathable_text, 14.f, text_pos, 0, cursor_pathable ? 0xFF00FF00 : 0xFFFF0000);
        text_pos.y += 20.f;

        // Display distance from player to cursor
        float dist_to_cursor = player->get_position().distance(cursor_pos);
        std::string dist_text = "Distance to cursor: " + std::to_string(dist_to_cursor).substr(0, 6);
        g_sdk->renderer->add_text(dist_text, 14.f, text_pos, 0, 0xFFFFFFFF);
        text_pos.y += 20.f;

        // Check if there's a wall between player and cursor
        bool wall_between = is_wall_between(player->get_position(), cursor_pos);
        std::string wall_text = "Wall between: " + std::string(wall_between ? "Yes" : "No");
        g_sdk->renderer->add_text(wall_text, 14.f, text_pos, 0, wall_between ? 0xFF00FF00 : 0xFFFF0000);
        text_pos.y += 20.f;

        // Display can jump status
        bool can_jump = test_wall_jump(player->get_position(), cursor_pos);
        std::string jump_text = "Can jump: " + std::string(can_jump ? "Yes" : "No");
        g_sdk->renderer->add_text(jump_text, 14.f, text_pos, 0, can_jump ? 0xFF00FF00 : 0xFFFF0000);
        text_pos.y += 20.f;

        // Display coordinates for easy recording
        std::string coords_text = "Cursor: " + std::to_string(cursor_pos.x).substr(0, 6) + ", " +
            std::to_string(cursor_pos.y).substr(0, 6) + ", " +
            std::to_string(cursor_pos.z).substr(0, 6);
        g_sdk->renderer->add_text(coords_text, 14.f, text_pos, 0, 0xFFFFFFFF);
        text_pos.y += 20.f;

        std::string player_coords = "Player: " + std::to_string(player->get_position().x).substr(0, 6) + ", " +
            std::to_string(player->get_position().y).substr(0, 6) + ", " +
            std::to_string(player->get_position().z).substr(0, 6);
        g_sdk->renderer->add_text(player_coords, 14.f, text_pos, 0, 0xFFFFFFFF);
    }

    // Combat mode implementations
    void handle_combo() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("handle_combo: No player found");
            return;
        }

        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - combat_state.last_combo_time < settings.min_spell_delay) {
            return;
        }

        log_debug("handle_combo: Starting combo logic");

        // Get target
        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero) return false;
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;
            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.q_range;
            });

        if (!target) {
            log_debug("handle_combo: No valid target found");
            return;
        }

        bool spell_cast = false;

        // R logic
        if (!spell_cast && settings.use_r && !combat_state.r_active &&
            player->get_par() >= settings.r_min_mana / 100.f * player->get_max_par()) {

            int nearby_enemies = 0;
            for (auto hero : g_sdk->object_manager->get_heroes()) {
                if (!hero->is_valid() || hero->is_dead() ||
                    hero->get_team_id() == player->get_team_id())
                    continue;

                if (hero->get_position().distance(player->get_position()) <= settings.r_range)
                    nearby_enemies++;
            }

            if (nearby_enemies >= settings.r_min_enemies) {
                log_debug("handle_combo: Casting R for multiple enemies (%d)", nearby_enemies);
                spell_cast = cast_r(target);
            }
            else if (player->get_hp() / player->get_max_hp() <= settings.r_min_health / 100.f) {
                log_debug("handle_combo: Casting R defensively, HP ratio: %.2f",
                    player->get_hp() / player->get_max_hp());
                spell_cast = cast_r(target);
            }
        }

        // W logic (charged version if preferred and appropriate)
        if (!spell_cast && settings.use_w &&
            player->get_par() >= settings.w_min_mana / 100.f * player->get_max_par()) {

            if (combat_state.w_charging) {
                float charge_time = current_time - combat_state.w_charge_start_time;

                // If fully charged or enemy is about to get out of range
                if (charge_time >= settings.w_charge_time ||
                    target->get_position().distance(player->get_position()) > settings.w_range - 50.f) {
                    log_debug("handle_combo: Releasing charged W, charge time: %.2f", charge_time);
                    spell_cast = release_w(target, nullptr);
                }
            }
            else if (settings.combo_prioritize_charged_w) {
                // Start charging W if appropriate
                bool should_charge = target->get_position().distance(player->get_position()) <= settings.w_range * 0.8f;

                if (should_charge) {
                    log_debug("handle_combo: Starting to charge W");
                    spell_cast = cast_w(nullptr, nullptr, true);
                }
            }
            else {
                // Regular W cast
                log_debug("handle_combo: Instant W cast");
                spell_cast = cast_w(target, nullptr, false);
            }
        }

        // E logic
        if (!spell_cast && settings.use_e && settings.combo_use_e_aggressive &&
            player->get_par() >= settings.e_min_mana / 100.f * player->get_max_par()) {

            float target_distance = player->get_position().distance(target->get_position());

            // Use E to get in range if needed
            if (target_distance > settings.q_range * 0.8f) {
                log_debug("handle_combo: Using E to close distance");
                spell_cast = cast_e(target, nullptr);
            }
            // Use E to kite away if in danger
            else if (settings.combo_use_e_kite && player->get_hp() / player->get_max_hp() < 0.3f) {
                auto player_pos = player->get_position();
                auto target_pos = target->get_position();
                auto kite_direction = (player_pos - target_pos).normalized();
                auto kite_position = player_pos + kite_direction * settings.e_range;

                log_debug("handle_combo: Using E to kite away");
                spell_cast = cast_e(nullptr, &kite_position);
            }
        }

        // Q spam (main damage source) - made more aggressive
        if (settings.use_q) {
            log_debug("handle_combo: Casting Q");
            spell_cast = cast_q(target, nullptr);
        }

        if (spell_cast) {
            combat_state.last_combo_time = current_time;
        }
    }

    void handle_harass() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.harass_min_mana / 100.f * player->get_max_par()) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - combat_state.last_combo_time < settings.min_spell_delay) {
            return;
        }

        bool spell_cast = false;

        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero) return false;
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;
            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.q_range;
            });

        if (!target) return;

        // Primarily focus on Q harass
        if (!spell_cast && settings.harass_use_q) {
            spell_cast = cast_q(target, nullptr);
        }

        // Occasionally use W for additional damage
        if (!spell_cast && settings.harass_use_w &&
            player->get_par() > settings.w_min_mana / 100.f * player->get_max_par()) {

            // Use prediction for W
            if (sdk::prediction) {
                pred_sdk::spell_data spell_data;
                spell_data.spell_type = pred_sdk::spell_type::linear;
                spell_data.range = settings.w_range;
                spell_data.radius = 40.f; // W width
                spell_data.delay = 0.4f;  // W delay
                spell_data.projectile_speed = 2600.f; // W speed
                spell_data.expected_hitchance = settings.w_min_hitchance;

                auto prediction = sdk::prediction->predict(target, spell_data);
                if (prediction.is_valid && prediction.hitchance >= settings.w_min_hitchance) {
                    log_debug("handle_harass: Casting W with prediction");
                    spell_cast = cast_w(nullptr, &prediction.cast_position, false);
                }
            }
        }

        if (spell_cast) {
            combat_state.last_combo_time = current_time;
        }
    }

    void handle_farming() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.farm_min_mana / 100.f * player->get_max_par()) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        bool spell_farm_enabled = sdk::orbwalker->is_spell_farm_enabled();
        bool spell_farm_lasthit_enabled = sdk::orbwalker->is_spell_farm_lasthit_enabled();

        if (!spell_farm_enabled && !spell_farm_lasthit_enabled) {
            return;
        }

        // Q farming
        if (settings.farm_use_q && current_time - combat_state.last_farm_q_cast_time >= 0.2f) {
            // Zeri Q is her primary farming tool
            auto minions = g_sdk->object_manager->get_minions();
            auto monsters = g_sdk->object_manager->get_monsters();

            // Find best position for Q
            math::vector3 best_position;
            int max_hit_count = 0;
            bool best_is_minion = false; // Track if the best target is a lane minion (for prioritization)

            // First, check lane minions
            for (auto minion : minions) {
                if (!minion->is_valid() || minion->is_dead() ||
                    minion->get_team_id() == player->get_team_id() ||
                    !minion->is_lane_minion() ||
                    minion->get_position().distance(player->get_position()) > settings.q_range)
                    continue;

                int hit_count = 0;

                // Count nearby minions
                for (auto other_minion : minions) {
                    if (!other_minion->is_valid() || other_minion->is_dead() ||
                        other_minion->get_team_id() == player->get_team_id())
                        continue;

                    if (other_minion->get_position().distance(minion->get_position()) <= 225.f) {
                        hit_count++;
                    }
                }

                // Only update if this position is better than our current best,
                // or if our current best is a monster and this is a minion
                if (hit_count > max_hit_count || (!best_is_minion && hit_count > 0)) {
                    max_hit_count = hit_count;
                    best_position = minion->get_position();
                    best_is_minion = true;

                    log_debug("Lane minion position with %d hits found", hit_count);
                }
            }

            // Check jungle monsters only if enabled and we haven't found a good group of minions
            if (settings.farm_use_jungle && (max_hit_count < 2 || !best_is_minion)) {
                for (auto monster : monsters) {
                    if (!monster->is_valid() || monster->is_dead() ||
                        monster->get_team_id() == player->get_team_id() ||
                        monster->get_position().distance(player->get_position()) > settings.q_range)
                        continue;

                    int hit_count = 0;

                    // Count nearby monsters
                    for (auto other_monster : monsters) {
                        if (!other_monster->is_valid() || other_monster->is_dead() ||
                            other_monster->get_team_id() == player->get_team_id())
                            continue;

                        if (other_monster->get_position().distance(monster->get_position()) <= 225.f) {
                            hit_count++;
                        }
                    }

                    // Prioritize large monsters
                    if (monster->is_large_monster()) {
                        hit_count += 2;
                    }

                    // Epic monsters (Baron, Dragon) get even higher priority
                    if (monster->is_epic_monster()) {
                        hit_count += 3;
                    }

                    // Only update if:
                    // 1. This monster position has more potential hits than our current best
                    // 2. OR our current best isn't a minion and this monster position is better
                    if (hit_count > max_hit_count || (!best_is_minion && hit_count > 0)) {
                        max_hit_count = hit_count;
                        best_position = monster->get_position();
                        best_is_minion = false;

                        log_debug("Jungle monster position with %d hits found", hit_count);
                    }
                }
            }

            // If we found a good position with multiple targets, cast Q there
            if (max_hit_count >= 1) {
                log_debug("handle_farming: Casting Q at best position, hits: %d, is_minion: %s",
                    max_hit_count, best_is_minion ? "Yes" : "No");

                if (cast_q(nullptr, &best_position)) {
                    combat_state.last_farm_q_cast_time = current_time;
                }
            }
            // Otherwise just cast at cursor
            else if (spell_farm_enabled) {
                auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
                log_debug("handle_farming: No good targets found, casting Q at cursor");

                if (cast_q(nullptr, &cursor_pos)) {
                    combat_state.last_farm_q_cast_time = current_time;
                }
            }
        }

        // W farming (for cannon minions or large waves)
        if (settings.farm_use_w && spell_farm_enabled &&
            current_time - combat_state.last_farm_w_cast_time >= 1.0f &&
            player->get_par() > settings.w_min_mana / 100.f * player->get_max_par()) {

            // Scan for clusters of minions
            auto minions = g_sdk->object_manager->get_minions();
            bool w_cast = false;

            // First priority: Siege/cannon minions with nearby minions
            for (auto minion : minions) {
                if (!minion->is_valid() || minion->is_dead() ||
                    minion->get_team_id() == player->get_team_id() ||
                    !minion->is_targetable() || !minion->is_visible() ||
                    minion->get_position().distance(player->get_position()) > settings.w_range)
                    continue;

                // Prioritize cannon minions
                if (minion->is_lane_minion_siege()) {
                    int nearby_count = 0;

                    for (auto other_minion : minions) {
                        if (!other_minion->is_valid() || other_minion->is_dead() ||
                            other_minion->get_team_id() == player->get_team_id())
                            continue;

                        if (other_minion->get_position().distance(minion->get_position()) <= 250.f) {
                            nearby_count++;
                        }
                    }

                    if (nearby_count >= settings.farm_min_minions_w) {
                        log_debug("handle_farming: Casting W for cannon minion with %d nearby minions", nearby_count);
                        if (cast_w(minion, nullptr, false)) {
                            combat_state.last_farm_w_cast_time = current_time;
                            w_cast = true;
                            break;
                        }
                    }
                }
            }

            // Second priority: Large clusters of any minions
            if (!w_cast) {
                for (auto minion : minions) {
                    if (!minion->is_valid() || minion->is_dead() ||
                        minion->get_team_id() == player->get_team_id() ||
                        !minion->is_targetable() || !minion->is_visible() ||
                        minion->get_position().distance(player->get_position()) > settings.w_range)
                        continue;

                    int nearby_count = 0;
                    for (auto other_minion : minions) {
                        if (!other_minion->is_valid() || other_minion->is_dead() ||
                            other_minion->get_team_id() == player->get_team_id())
                            continue;

                        if (other_minion->get_position().distance(minion->get_position()) <= 250.f) {
                            nearby_count++;
                        }
                    }

                    if (nearby_count >= settings.farm_min_minions_w + 1) {
                        log_debug("handle_farming: Casting W for large minion cluster, count: %d", nearby_count);
                        if (cast_w(minion, nullptr, false)) {
                            combat_state.last_farm_w_cast_time = current_time;
                            w_cast = true;
                            break;
                        }
                    }
                }
            }

            // Third priority (if enabled): Jungle monsters
            if (!w_cast && settings.farm_use_jungle) {
                auto monsters = g_sdk->object_manager->get_monsters();

                // First check for epic or large monsters
                for (auto monster : monsters) {
                    if (!monster->is_valid() || monster->is_dead() ||
                        monster->get_team_id() == player->get_team_id() ||
                        !monster->is_targetable() || !monster->is_visible() ||
                        monster->get_position().distance(player->get_position()) > settings.w_range)
                        continue;

                    // Focus on large or epic monsters
                    if (monster->is_large_monster() || monster->is_epic_monster()) {
                        int nearby_count = 0;

                        for (auto other_monster : monsters) {
                            if (!other_monster->is_valid() || other_monster->is_dead() ||
                                other_monster->get_team_id() == player->get_team_id())
                                continue;

                            if (other_monster->get_position().distance(monster->get_position()) <= 250.f) {
                                nearby_count++;
                            }
                        }

                        if (nearby_count >= 2) { // At least the large monster plus one more
                            log_debug("handle_farming: Casting W for jungle camp, count: %d", nearby_count);
                            if (cast_w(monster, nullptr, false)) {
                                combat_state.last_farm_w_cast_time = current_time;
                                break;
                            }
                        }
                    }
                }
            }
        }

        // E farming (mostly for jungle)
        if (settings.farm_use_e && settings.farm_use_jungle && spell_farm_enabled &&
            current_time - combat_state.last_e_time >= 3.0f &&
            player->get_par() > settings.e_min_mana / 100.f * player->get_max_par()) {

            auto monsters = g_sdk->object_manager->get_monsters();

            // Look for large monsters at a distance
            for (auto monster : monsters) {
                if (!monster->is_valid() || monster->is_dead() ||
                    monster->get_team_id() == player->get_team_id() ||
                    !monster->is_large_monster()) // Only consider large monsters
                    continue;

                float distance = monster->get_position().distance(player->get_position());

                // Only use E if monster is at medium range (not too close, not too far)
                if (distance > 450.f && distance < settings.e_range + 200.f) {
                    log_debug("handle_farming: Using E to reach large monster");
                    if (cast_e(monster, nullptr)) {
                        break;
                    }
                }
            }
        }
    }

    void handle_flee() {
        if (settings.jump_spots_enabled) {
            handle_wall_jump();
        }

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        // Use E to dash away in flee mode
        if (settings.use_e && can_cast_spell(2, 0.f)) {
            auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
            cast_e(nullptr, &cursor_pos);
        }
    }

    void create_menu() {
        menu = g_sdk->menu_manager->add_category("zeri", "Zeri");
        if (!menu) return;

        auto combat = menu->add_sub_category("combat", "Combat Settings");
        if (combat) {
            // Add "Q - ", "W - ", etc. here, in the menu code, *not* in the library.
            combat->add_checkbox("use_q", "Q - " + champion_spell_names::get_champion_q_name("Zeri"), settings.use_q, [&](bool value) { settings.use_q = value; });
            combat->add_checkbox("use_w", "W - " + champion_spell_names::get_champion_w_name("Zeri"), settings.use_w, [&](bool value) { settings.use_w = value; });
            combat->add_checkbox("use_e", "E - " + champion_spell_names::get_champion_e_name("Zeri"), settings.use_e, [&](bool value) { settings.use_e = value; });
            combat->add_checkbox("use_r", "R - " + champion_spell_names::get_champion_r_name("Zeri"), settings.use_r, [&](bool value) { settings.use_r = value; });

            auto q_settings = combat->add_sub_category("q_settings", ("Q - " + champion_spell_names::get_champion_q_name("Zeri") + " Settings").c_str());
            if (q_settings) {
                q_settings->add_checkbox("q_use_extended", "Extended Q Through Walls", settings.q_use_extended,
                    [&](bool value) { settings.q_use_extended = value; });
            }

            auto w_settings = combat->add_sub_category("w_settings", ("W - " + champion_spell_names::get_champion_w_name("Zeri") + " Settings").c_str());
            if (w_settings) {
                w_settings->add_checkbox("w_charged_preferred", "Prefer Charged W", settings.w_charged_preferred,
                    [&](bool value) { settings.w_charged_preferred = value; });
                w_settings->add_slider_float("w_charge_time", "Full Charge Time", 0.5f, 1.5f, 0.05f, settings.w_charge_time,
                    [&](float value) { settings.w_charge_time = value; });
                w_settings->add_slider_float("w_min_charge", "Min Charge Time", 0.1f, 0.5f, 0.05f, settings.w_min_charge,
                    [&](float value) { settings.w_min_charge = value; });
                w_settings->add_slider_float("w_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.w_min_mana,
                    [&](float value) { settings.w_min_mana = value; });
                w_settings->add_slider_int("w_min_hitchance", "Minimum Hitchance", 30, 100, 1, settings.w_min_hitchance,
                    [&](int value) { settings.w_min_hitchance = value; });
                w_settings->add_checkbox("w_wall_pierce", "Prioritize Wall Pierce", settings.w_wall_pierce,
                    [&](bool value) { settings.w_wall_pierce = value; });
            }

            auto e_settings = combat->add_sub_category("e_settings", ("E - " + champion_spell_names::get_champion_e_name("Zeri") + " Settings").c_str());
            if (e_settings) {
                e_settings->add_slider_float("e_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.e_min_mana,
                    [&](float value) { settings.e_min_mana = value; });
                e_settings->add_checkbox("e_over_walls", "Enable Wall Jumping", settings.e_over_walls,
                    [&](bool value) { settings.e_over_walls = value; });
                e_settings->add_checkbox("e_save_for_escape", "Save for Escape", settings.e_save_for_escape,
                    [&](bool value) { settings.e_save_for_escape = value; });
                e_settings->add_checkbox("combo_use_e_aggressive", "Use Aggressive in Combo", settings.combo_use_e_aggressive,
                    [&](bool value) { settings.combo_use_e_aggressive = value; });
                e_settings->add_checkbox("combo_use_e_kite", "Use for Kiting", settings.combo_use_e_kite,
                    [&](bool value) { settings.combo_use_e_kite = value; });
            }

            auto r_settings = combat->add_sub_category("r_settings", ("R - " + champion_spell_names::get_champion_r_name("Zeri") + " Settings").c_str());
            if (r_settings) {
                r_settings->add_slider_float("r_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.r_min_mana,
                    [&](float value) { settings.r_min_mana = value; });
                r_settings->add_slider_int("r_min_enemies", "Min Enemies for R", 1, 5, 1, settings.r_min_enemies,
                    [&](int value) { settings.r_min_enemies = value; });
                r_settings->add_slider_float("r_min_health", "Defensive HP Threshold %", 0.f, 100.f, 1.f, settings.r_min_health,
                    [&](float value) { settings.r_min_health = value; });
            }
        }

        auto q_humanization = menu->add_sub_category("q_humanization", "Q Humanization");
        if (q_humanization) {
            q_humanization->add_checkbox("enabled", "Enable Q Humanization", settings.q_humanization.enabled,
                [&](bool value) { settings.q_humanization.enabled = value; });
            q_humanization->add_slider_float("min_delay", "Minimum Delay", 0.01f, 0.2f, 0.01f, settings.q_humanization.min_delay,
                [&](float value) { settings.q_humanization.min_delay = value; });
            q_humanization->add_slider_float("max_delay", "Maximum Delay", 0.02f, 0.3f, 0.01f, settings.q_humanization.max_delay,
                [&](float value) { settings.q_humanization.max_delay = value; });
            q_humanization->add_checkbox("randomize", "Randomize Delay", settings.q_humanization.randomize,
                [&](bool value) { settings.q_humanization.randomize = value; });
            q_humanization->add_checkbox("dynamic_delay", "Adjust for Attack Speed", settings.q_humanization.dynamic_delay,
                [&](bool value) { settings.q_humanization.dynamic_delay = value; });
            q_humanization->add_slider_float("delay_scale", "Delay Scale Factor", 0.5f, 1.5f, 0.05f, settings.q_humanization.delay_scale,
                [&](float value) { settings.q_humanization.delay_scale = value; });
            q_humanization->add_checkbox("avoid_patterns", "Avoid Obvious Patterns", settings.q_humanization.avoid_patterns,
                [&](bool value) { settings.q_humanization.avoid_patterns = value; });
            q_humanization->add_slider_int("miss_chance", "Miss Chance %", 0, 10, 1, settings.q_humanization.miss_chance,
                [&](int value) { settings.q_humanization.miss_chance = value; });
            q_humanization->add_checkbox("draw_stats", "Draw Humanization Stats", settings.draw_q_humanization_stats,
                [&](bool value) { settings.draw_q_humanization_stats = value; });
        }

        auto harass = menu->add_sub_category("harass", "Harass Settings");
        if (harass) {
            harass->add_checkbox("harass_use_q", champion_spell_names::get_champion_q_name("Zeri"), settings.harass_use_q,
                [&](bool value) { settings.harass_use_q = value; });
            harass->add_checkbox("harass_use_w", champion_spell_names::get_champion_w_name("Zeri"), settings.harass_use_w,
                [&](bool value) { settings.harass_use_w = value; });
            harass->add_slider_float("harass_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.harass_min_mana,
                [&](float value) { settings.harass_min_mana = value; });
            harass->add_checkbox("harass_under_turret", "Harass Under Turret", settings.harass_under_turret,
                [&](bool value) { settings.harass_under_turret = value; });
        }

        auto farm = menu->add_sub_category("farm", "Farm Settings");
        if (farm) {
            farm->add_checkbox("farm_use_q", champion_spell_names::get_champion_q_name("Zeri"), settings.farm_use_q,
                [&](bool value) { settings.farm_use_q = value; });
            farm->add_checkbox("farm_use_w", champion_spell_names::get_champion_w_name("Zeri"), settings.farm_use_w,
                [&](bool value) { settings.farm_use_w = value; });
            farm->add_checkbox("farm_use_e", champion_spell_names::get_champion_e_name("Zeri"), settings.farm_use_e,
                [&](bool value) { settings.farm_use_e = value; });
            farm->add_checkbox("farm_use_jungle", "Farm Jungle Monsters", settings.farm_use_jungle,
                [&](bool value) { settings.farm_use_jungle = value; });
            farm->add_slider_float("farm_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.farm_min_mana,
                [&](float value) { settings.farm_min_mana = value; });
            farm->add_slider_int("farm_min_minions_w", "Min Minions for W", 2, 6, 1, settings.farm_min_minions_w,
                [&](int value) { settings.farm_min_minions_w = value; });
            farm->add_checkbox("farm_under_turret", "Farm Under Turret", settings.farm_under_turret,
                [&](bool value) { settings.farm_under_turret = value; });
        }

        // Change the variable name to jump_spots_submenu to avoid conflict
        auto jump_spots_submenu = menu->add_sub_category("jump_spots", "Wall Jump Settings");
        if (jump_spots_submenu) {
            jump_spots_submenu->add_checkbox("jump_spots_enabled", "Enable Jump Spots", settings.jump_spots_enabled,
                [&](bool value) { settings.jump_spots_enabled = value; });
            jump_spots_submenu->add_checkbox("jump_spots_debug", "Debug Mode", settings.jump_spots_debug,
                [&](bool value) { settings.jump_spots_debug = value; });
            jump_spots_submenu->add_slider_int("jump_spot_draw_range", "Draw Range", 500, 5000, 100, settings.jump_spot_draw_range,
                [&](int value) { settings.jump_spot_draw_range = value; });
            jump_spots_submenu->add_checkbox("manual_wall_jump", "Manual Wall Jump", settings.manual_wall_jump,
                [&](bool value) { settings.manual_wall_jump = value; });

            // Add recording functionality
            jump_spots_submenu->add_hotkey("save_jump_spots", "Save Recorded Jump Spots", 'J', false, true,
                [](std::string* hotkey, bool value) {
                    if (value) save_jump_spots_to_file();
                });

            // Fixed jump spots menu code - renamed from jump_spots to jump_spots_submenu
            auto js_list_menu = jump_spots_submenu->add_sub_category("jump_spots_list", "Jump Spots List");
            if (js_list_menu) {
                // Add a simple entry for each jump spot
                for (size_t i = 0; i < jump_spots.size(); i++) {
                    std::string spot_id = "spot_" + std::to_string(i);
                    std::string menu_text = jump_spots[i].name + " (Enabled: ";
                    menu_text += jump_spots[i].enabled ? "Yes)" : "No)";

                    // Just use add_label method
                    js_list_menu->add_label(menu_text.c_str());
                }

                // Add a note about editing
                js_list_menu->add_separator();
                js_list_menu->add_label("Note: Use Debug Mode and hotkey to add new spots");
            }
        }

        auto drawings = menu->add_sub_category("drawings", "Drawing Settings");
        if (drawings) {
            drawings->add_checkbox("draw_ranges", "Draw Ranges", settings.draw_ranges,
                [&](bool value) { settings.draw_ranges = value; });
            drawings->add_checkbox("draw_damage", "Draw Damage", settings.draw_damage,
                [&](bool value) { settings.draw_damage = value; });
            drawings->add_checkbox("draw_spell_ranges", "Draw Spell Ranges", settings.draw_spell_ranges,
                [&](bool value) { settings.draw_spell_ranges = value; });
        }

        auto debug = menu->add_sub_category("debug", "Debug Settings");
        if (debug) {
            debug->add_checkbox("debug_enabled", "Enable Debug Logging", debug_enabled,
                [&](bool value) { debug_enabled = value; });
        }
    }

    // Event handlers
    void __fastcall on_update() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) {
            log_debug("on_update: player is null or dead");
            return;
        }

        log_debug("on_update: Starting update");

        if (sdk::orbwalker) {
            if (sdk::orbwalker->combo()) {
                log_debug("on_update: Combo mode active");
                handle_combo();
            }
            else if (sdk::orbwalker->harass()) {
                log_debug("on_update: Harass mode active");
                handle_harass();
            }
            else if (sdk::orbwalker->clear()) {
                log_debug("on_update: Clear mode active");
                handle_farming();
            }
            else if (sdk::orbwalker->flee()) {
                log_debug("on_update: Flee mode active");
                handle_flee();
            }
            else if (sdk::orbwalker->lasthit()) {
                log_debug("on_update: LastHit mode active");
                handle_farming();
            }
        }

        // Handle W charging timeout
        if (combat_state.w_charging) {
            float current_time = g_sdk->clock_facade->get_game_time();
            float charge_time = current_time - combat_state.w_charge_start_time;

            // If charged for too long, release it
            if (charge_time >= settings.w_charge_time + 0.5f) {
                auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
                release_w(nullptr, &cursor_pos);
            }
        }

        // Update infotab
        if (sdk::infotab && infotab_text_id != 0) {
            sdk::infotab->remove_text(infotab_text_id);
            infotab_text_id = 0;
        }

        if (sdk::infotab) {
            infotab_sdk::text_entry title = { "Zeri", 0xFFFFFFFF };
            infotab_text_id = sdk::infotab->add_text(title, [&]() -> infotab_sdk::text_entry {
                infotab_sdk::text_entry current_entry;
                current_entry.color = 0xFFFFFFFF;
                std::string mode_str = "Mode: ";
                if (sdk::orbwalker->combo()) mode_str += "Combo";
                else if (sdk::orbwalker->harass()) mode_str += "Harass";
                else if (sdk::orbwalker->clear()) mode_str += "Clear";
                else if (sdk::orbwalker->flee()) mode_str += "Flee";
                else if (sdk::orbwalker->lasthit()) mode_str += "Last Hit";
                else mode_str += "None";

                if (combat_state.r_active) {
                    mode_str += " (R Active)";
                }
                if (combat_state.w_charging) {
                    float charge_time = g_sdk->clock_facade->get_game_time() - combat_state.w_charge_start_time;
                    mode_str += " (W Charging: " + std::to_string(charge_time).substr(0, 4) + "s)";
                }

                current_entry.text = mode_str;
                return current_entry;
                });
        }

        log_debug("on_update: Update completed");
    }

    void __fastcall on_draw() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.draw_spell_ranges) {
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.q_range, 2.f, 0xFF00AAFF);
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.w_range, 2.f, 0xFFFFAA00);
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.e_range, 2.f, 0xFF00FF00);
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.r_range, 2.f, 0xFFFF0055);
        }

        if (settings.draw_damage) {
            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy->is_valid() || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                float total_damage = calculate_full_combo_damage(enemy);
                g_sdk->renderer->add_damage_indicator(enemy, total_damage);
            }
        }

        // Draw jump spots
        if (settings.jump_spots_enabled) {
            draw_jump_spots();
        }

        // Draw recorded jump spots
        if (settings.jump_spots_debug) {
            draw_recorded_jumps();
        }

        // Draw Q humanization stats
        if (settings.draw_q_humanization_stats) {
            render_q_humanization_stats();
        }

        // Draw debug information for wall jumps
        if (settings.jump_spots_debug) {
            debug_wall_jumps();
        }
    }

    bool __fastcall before_attack(orb_sdk::event_data* data) {
        if (!initialized) return true;

        // Allow manual auto-attacks unless in an active combat mode
        if (!sdk::orbwalker->combo() && !sdk::orbwalker->harass() &&
            !sdk::orbwalker->clear() && !sdk::orbwalker->fast_clear()) {
            return true;
        }

        // In combat modes, use Q instead of auto-attacks for Zeri
        return false;
    }

    void __fastcall on_buff_gain(game_object* source, buff_instance* buff) {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;

        // Detect R activation
        if (buff->get_name() == "ZeriR") {
            combat_state.r_active = true;
            combat_state.r_activation_time = g_sdk->clock_facade->get_game_time();
            log_debug("on_buff_gain: R activated");
        }
    }

    void __fastcall on_buff_loss(game_object* source, buff_instance* buff) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;
        if (!initialized) return;

        // Detect R deactivation
        if (buff->get_name() == "ZeriR") {
            combat_state.r_active = false;
            log_debug("on_buff_loss: R deactivated");
        }
    }

    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast) {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || sender != player) return;

        // Track W charging state
        if (cast->get_spell_slot() == 1) {
            if (combat_state.w_charging) {
                combat_state.w_charging = false;
                log_debug("on_process_spell_cast: W released");
            }
            else {
                // Could be start of charge
                std::string spell_name = cast->get_spell_data()->get_static_data()->get_name();
                if (spell_name.find("ZeriW") != std::string::npos) {
                    log_debug("on_process_spell_cast: W started charging");
                }
            }
        }

        // Track E for wall jumps
        if (settings.jump_spots_debug && cast->get_spell_slot() == 2) {
            log_debug("on_process_spell_cast: E cast detected!");

            // Record the starting position and time
            e_start_pos = player->get_position();
            e_cast_time = g_sdk->clock_facade->get_game_time();

            // Register our properly defined tracking function
            g_sdk->event_manager->register_callback(event_manager::event::game_update,
                reinterpret_cast<void*>(track_wall_jump_fn));
        }
    }

    void __fastcall on_create_object(game_object* obj) {
        if (!initialized || !obj) return;

        // Could be used to detect new particles or effects for debugging
        if (settings.jump_spots_debug && obj->is_particle()) {
            log_debug("on_create_object: Particle created: %s", obj->get_name().c_str());
        }
    }

    void load() {
        try {
            g_sdk->log_console("[+] Loading Zeri...");

            if (!sdk_init::orbwalker() || !sdk_init::target_selector() || !sdk_init::damage() || !sdk_init::infotab()) {
                g_sdk->log_console("[!] Failed to load dependencies!");
                return;
            }

            auto& order_manager = vengaboys::OrderManager::get_instance();
            order_manager.set_min_order_delay(settings.min_spell_delay);
            order_manager.set_max_queue_size(3);

            g_sdk->log_console("[+] Dependencies loaded successfully.");

            if (sdk::orbwalker) {
                sdk::orbwalker->register_callback(orb_sdk::before_attack, reinterpret_cast<void*>(&before_attack));
                g_sdk->log_console("[+] Registered Orbwalker before_attack callback");
            }

            // Initialize jump spots
            initialize_jump_spots();

            create_menu();
            if (!menu) {
                g_sdk->log_console("[!] Failed to create menu");
                return;
            }

            if (g_sdk->event_manager) {
                g_sdk->event_manager->register_callback(event_manager::event::game_update,
                    reinterpret_cast<void*>(&on_update));
                g_sdk->event_manager->register_callback(event_manager::event::draw_world,
                    reinterpret_cast<void*>(&on_draw));
                g_sdk->event_manager->register_callback(event_manager::event::buff_gain,
                    reinterpret_cast<void*>(&on_buff_gain));
                g_sdk->event_manager->register_callback(event_manager::event::buff_loss,
                    reinterpret_cast<void*>(&on_buff_loss));
                g_sdk->event_manager->register_callback(event_manager::event::process_cast,
                    reinterpret_cast<void*>(&on_process_spell_cast));
                g_sdk->event_manager->register_callback(event_manager::event::create_object,
                    reinterpret_cast<void*>(&on_create_object));
                g_sdk->log_console("[+] Event callbacks registered successfully.");
            }

            initialized = true;
            g_sdk->log_console("[+] Zeri loaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in load: %s", e.what());
        }
    }

    void unload() {
        try {
            if (!initialized) return;

            g_sdk->log_console("[+] Unloading Zeri...");

            if (g_sdk->event_manager) {
                g_sdk->event_manager->unregister_callback(event_manager::event::game_update,
                    reinterpret_cast<void*>(&on_update));
                g_sdk->event_manager->unregister_callback(event_manager::event::draw_world,
                    reinterpret_cast<void*>(&on_draw));
                g_sdk->event_manager->unregister_callback(event_manager::event::buff_gain,
                    reinterpret_cast<void*>(&on_buff_gain));
                g_sdk->event_manager->unregister_callback(event_manager::event::buff_loss,
                    reinterpret_cast<void*>(&on_buff_loss));
                g_sdk->event_manager->unregister_callback(event_manager::event::process_cast,
                    reinterpret_cast<void*>(&on_process_spell_cast));
                g_sdk->event_manager->unregister_callback(event_manager::event::create_object,
                    reinterpret_cast<void*>(&on_create_object));
            }

            if (sdk::orbwalker) {
                sdk::orbwalker->unregister_callback(orb_sdk::before_attack,
                    reinterpret_cast<void*>(&before_attack));
            }

            if (sdk::infotab && infotab_text_id != 0) {
                sdk::infotab->remove_text(infotab_text_id);
                infotab_text_id = 0;
            }

            initialized = false;
            g_sdk->log_console("[+] Zeri unloaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in unload: %s", e.what());
        }
    }
}