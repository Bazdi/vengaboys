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
#include "riven.hpp"
#include "ordermanager.hpp"
#include "champion_spell_names.hpp"

namespace riven {

    Settings settings;
    menu_category* menu = nullptr;
    bool initialized = false;
    bool debug_enabled = false; // Disabled debug by default
    ComboState combo_state{};
    SpellTracker spell_tracker{};
    CachedTarget cached_target{};

    uint32_t infotab_text_id = 0;

    void log_debug(const char* format, ...) {
        if (!debug_enabled) return;

        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        g_sdk->log_console("[Riven] %s", buffer);
    }

    bool can_cast_spell(int spell_slot, float delay) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        auto spell = player->get_spell(spell_slot);
        if (!spell) return false;

        if (spell->get_cooldown() > 0.01f) return false;

        return vengaboys::OrderManager::get_instance().can_cast(player, spell_slot, delay);
    }

    float calculate_q_damage(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target || !target->is_valid()) return 0.f;

        try {
            return sdk::damage->get_spell_damage(player, target, 0);
        }
        catch (const std::exception&) {
            return 0.f;
        }
    }

    float calculate_w_damage(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target || !target->is_valid()) return 0.f;

        try {
            return sdk::damage->get_spell_damage(player, target, 1);
        }
        catch (const std::exception&) {
            return 0.f;
        }
    }

    float get_total_damage(game_object* target, bool include_r) {
        if (!target || !target->is_valid()) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        try {
            float total = 0.f;

            total += sdk::damage->get_aa_damage(player, target);

            if (auto spell = player->get_spell(0)) {
                if (spell->get_level() > 0) {
                    total += sdk::damage->get_spell_damage(player, target, 0) * 3;
                }
            }

            if (auto spell = player->get_spell(1)) {
                if (spell->get_level() > 0) {
                    total += sdk::damage->get_spell_damage(player, target, 1);
                }
            }

            if (include_r && !combo_state.r_active) {
                total *= 1.2f;
            }

            return total;
        }
        catch (const std::exception&) {
            return 0.f;
        }
    }

    bool can_kill_with_r2(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target || !target->is_valid()) return false;

        try {
            int r_level = player->get_spell(3)->get_level();
            if (r_level == 0) return false;

            float physical_damage = sdk::damage->get_spell_damage(player, target, 3);

            float missing_hp_percent = 1.f - (target->get_hp() / target->get_max_hp());
            float damage_multiplier = 1.f + (2.0f * missing_hp_percent);

            float total_damage = physical_damage * damage_multiplier;

            return total_damage >= target->get_hp();
        }
        catch (const std::exception&) {
            return false;
        }
    }

    game_object* get_best_target() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return nullptr;

        const float MAX_TARGET_RANGE = settings.q_extend_range + 100.f;

        // Try to use target selector first
        auto target = sdk::target_selector->get_hero_target();
        if (target && target->is_valid() && !target->is_dead()) {
            float distance = player->get_position().distance(target->get_position());
            if (distance <= MAX_TARGET_RANGE) {
                return target;
            }
        }

        // Fall back to closest enemy
        game_object* best_target = nullptr;
        float best_distance = FLT_MAX;

        try {
            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || !enemy->is_valid() || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id()) {
                    continue;
                }

                float distance = player->get_position().distance(enemy->get_position());
                if (distance > MAX_TARGET_RANGE) {
                    continue;
                }

                if (distance < best_distance) {
                    best_distance = distance;
                    best_target = enemy;
                }
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }

        return best_target;
    }

    bool should_use_r1() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        try {
            if (!player->get_spell(3) || player->get_spell(3)->get_level() == 0) {
                return false;
            }

            int nearby_enemies = 0;
            int killable_enemies = 0;
            game_object* primary_target = nullptr;

            auto heroes = g_sdk->object_manager->get_heroes();
            for (auto enemy : heroes) {
                if (!enemy || !enemy->is_valid() || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id()) {
                    continue;
                }

                float distance = player->get_position().distance(enemy->get_position());
                if (distance <= 800.f) {
                    nearby_enemies++;
                    float damage = get_total_damage(enemy, true);
                    if (damage >= enemy->get_hp()) {
                        killable_enemies++;
                    }

                    if (!primary_target || distance < player->get_position().distance(primary_target->get_position())) {
                        primary_target = enemy;
                    }
                }
            }

            // Aggressive 1v1 R1 usage
            if (nearby_enemies == 1 && primary_target) {
                float target_hp_percent = (primary_target->get_hp() / primary_target->get_max_hp()) * 100.0f;
                if (target_hp_percent <= 85.0f) {
                    return true;
                }
            }

            return nearby_enemies >= settings.r_min_targets && killable_enemies > 0;
        }
        catch (const std::exception&) {
            return false;
        }
    }

    bool cast_q(game_object* target) {
        if (!target || !target->is_valid() || !settings.use_q) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(0, 0.f)) return false;

        try {
            // Always use exact cursor position for Q targeting
            math::vector3 cursor_pos = g_sdk->hud_manager->get_cursor_position();

            vengaboys::OrderPriority priority = combo_state.r_active ?
                vengaboys::OrderPriority::High : vengaboys::OrderPriority::Critical;

            bool result = vengaboys::OrderManager::get_instance().cast_spell(0, cursor_pos, priority);

            if (result) {
                spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                combo_state.last_q_time = g_sdk->clock_facade->get_game_time();

                if (settings.cancel_q) {
                    combo_state.waiting_for_aa = true;
                    combo_state.last_aa_time = g_sdk->clock_facade->get_game_time() + settings.cancel_delay;
                }

                return true;
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }

        return false;
    }

    bool cast_w(game_object* target) {
        if (!target || !target->is_valid() || !settings.use_w) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(1, 0.f)) return false;

        try {
            float distance = player->get_position().distance(target->get_position());
            if (distance <= settings.w_range) {
                vengaboys::OrderPriority priority = combo_state.r_active ?
                    vengaboys::OrderPriority::Normal : vengaboys::OrderPriority::High;
                if (vengaboys::OrderManager::get_instance().cast_spell(1, player->get_position(), priority)) {
                    spell_tracker.update_cooldown(1, g_sdk->clock_facade->get_game_time() + player->get_spell(1)->get_cooldown());
                    combo_state.last_w_time = g_sdk->clock_facade->get_game_time();

                    if (settings.cancel_w) {
                        combo_state.waiting_for_aa = true;
                        combo_state.last_aa_time = g_sdk->clock_facade->get_game_time() + settings.cancel_delay;
                    }

                    return true;
                }
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }

        return false;
    }

    bool cast_e(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target || !settings.use_e) return false;

        if (!can_cast_spell(2, 0.f)) return false;

        try {
            float distance = player->get_position().distance(target->get_position());

            if (settings.e_gap_close && distance > player->get_attack_range() &&
                distance <= settings.e_gap_close_range) {
                // Use cursor position for E dash direction
                math::vector3 cursor_pos = g_sdk->hud_manager->get_cursor_position();
                if (vengaboys::OrderManager::get_instance().cast_spell(2, cursor_pos, vengaboys::OrderPriority::High)) {
                    spell_tracker.update_cooldown(2, g_sdk->clock_facade->get_game_time() + player->get_spell(2)->get_cooldown());
                    combo_state.last_e_time = g_sdk->clock_facade->get_game_time();
                    return true;
                }
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }

        return false;
    }

    bool cast_r1(game_object* target) {
        if (!settings.use_r1) return false;

        if (combo_state.r_active) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!target) return false;

        try {
            auto spell = player->get_spell(3);
            if (!spell) return false;

            if (spell->get_level() == 0) return false;

            if (!can_cast_spell(3, 0.f)) return false;

            math::vector3 cast_pos = player->get_position();

            if (!combo_state.r_active) {
                vengaboys::OrderManager::get_instance().clear_queue();
            }

            bool cast_success = vengaboys::OrderManager::get_instance().cast_spell(3, cast_pos, vengaboys::OrderPriority::Critical);

            if (cast_success) {
                spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + player->get_spell(3)->get_cooldown());
                combo_state.r_active = true;
                combo_state.last_r_time = g_sdk->clock_facade->get_game_time();
                return true;
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }

        return false;
    }

    bool cast_r2(game_object* target) {
        if (!target || !target->is_valid() || !settings.use_r2 || !combo_state.r_active) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(3, 0.f)) return false;

        try {
            float hp_percent = (target->get_hp() / target->get_max_hp()) * 100.f;
            if (hp_percent <= settings.r_hp_threshold || can_kill_with_r2(target)) {
                vengaboys::OrderManager::get_instance().clear_queue();
                if (vengaboys::OrderManager::get_instance().cast_spell(3, target->get_position(), vengaboys::OrderPriority::Critical)) {
                    spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + player->get_spell(3)->get_cooldown());
                    combo_state.r_active = false;
                    combo_state.last_r_time = g_sdk->clock_facade->get_game_time();
                    return true;
                }
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }

        return false;
    }

    // Completely rewritten handle_combo function
    void handle_combo() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        if (!sdk::orbwalker->combo()) {
            // Reset combo if not in combo mode
            combo_state.combo_step = ComboStep::IDLE;
            combo_state.combo_in_progress = false;
            return;
        }

        auto target = get_best_target();
        if (!target || !target->is_valid() || target->is_dead()) {
            // Reset combo if no target
            combo_state.combo_step = ComboStep::IDLE;
            combo_state.combo_in_progress = false;
            combo_state.current_target = nullptr;
            return;
        }

        combo_state.current_target = target;
        float current_time = g_sdk->clock_facade->get_game_time();
        float distance = player->get_position().distance(target->get_position());

        // Handle R1/R2 separately from the main combo
        if (settings.use_r1 && !combo_state.r_active && should_use_r1()) {
            cast_r1(target);
        }
        else if (combo_state.r_active && settings.use_r2 &&
            (target->get_hp() / target->get_max_hp() <= settings.r_hp_threshold / 100.0f ||
                can_kill_with_r2(target))) {
            cast_r2(target);
        }

        // Check for timeout and reset if needed
        if (combo_state.combo_in_progress && current_time - combo_state.last_step_time > 2.0f) {
            combo_state.combo_step = ComboStep::IDLE;
            combo_state.combo_in_progress = false;
        }

        // Start a new combo if not in progress
        if (!combo_state.combo_in_progress) {
            if (settings.use_e && can_cast_spell(2, 0.0f) && distance <= settings.e_gap_close_range && distance > settings.w_range) {
                combo_state.combo_step = ComboStep::WAITING_FOR_E;
                combo_state.combo_in_progress = true;
            }
            else if (distance <= settings.w_range) {
                // Skip E if already in W range
                combo_state.combo_step = ComboStep::WAITING_FOR_W;
                combo_state.combo_in_progress = true;
            }
            else if (distance <= settings.q_extend_range) {
                // Skip to Q1 if out of W range but in Q range
                combo_state.combo_step = ComboStep::WAITING_FOR_Q1;
                combo_state.combo_in_progress = true;
            }
        }

        // Abort if no valid step
        if (!combo_state.combo_in_progress) return;

        // Execute the combo step by step
        switch (combo_state.combo_step) {
        case ComboStep::WAITING_FOR_E:
            if (settings.use_e && can_cast_spell(2, 0.0f) && distance <= settings.e_gap_close_range) {
                if (cast_e(target)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_E;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else {
                // Skip E if not available
                combo_state.combo_step = ComboStep::WAITING_FOR_W;
            }
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_E:
            // This state is handled in on_before_attack
            // If AA doesn't happen for too long, move on
            if (current_time - combo_state.last_step_time > 0.5f) {
                combo_state.combo_step = ComboStep::WAITING_FOR_W;
                combo_state.waiting_for_aa = false;
            }
            break;

        case ComboStep::WAITING_FOR_W:
            if (settings.use_w && can_cast_spell(1, 0.0f) && distance <= settings.w_range) {
                if (cast_w(target)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_W;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else {
                // Skip W if not available or out of range
                combo_state.combo_step = ComboStep::WAITING_FOR_Q1;
            }
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_W:
            // This state is handled in on_before_attack
            // If AA doesn't happen for too long, move on
            if (current_time - combo_state.last_step_time > 0.5f) {
                combo_state.combo_step = ComboStep::WAITING_FOR_Q1;
                combo_state.waiting_for_aa = false;
            }
            break;

        case ComboStep::WAITING_FOR_Q1:
            if (settings.use_q && can_cast_spell(0, 0.0f) && distance <= settings.q_extend_range) {
                if (cast_q(target)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_Q1;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else {
                // If we can't cast Q1, reset the combo
                combo_state.combo_step = ComboStep::IDLE;
                combo_state.combo_in_progress = false;
            }
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_Q1:
            // This state is handled in on_before_attack
            // If AA doesn't happen for too long, move on
            if (current_time - combo_state.last_step_time > 0.5f) {
                combo_state.combo_step = ComboStep::WAITING_FOR_Q2;
                combo_state.waiting_for_aa = false;
            }
            break;

        case ComboStep::WAITING_FOR_Q2:
            if (settings.use_q && can_cast_spell(0, 0.0f) && distance <= settings.q_extend_range) {
                if (cast_q(target)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_Q2;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else {
                // If we can't cast Q2, wait a bit longer
                if (current_time - combo_state.last_step_time > 1.0f) {
                    combo_state.combo_step = ComboStep::IDLE;
                    combo_state.combo_in_progress = false;
                }
            }
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_Q2:
            // This state is handled in on_before_attack
            // If AA doesn't happen for too long, move on
            if (current_time - combo_state.last_step_time > 0.5f) {
                combo_state.combo_step = ComboStep::WAITING_FOR_Q3;
                combo_state.waiting_for_aa = false;
            }
            break;

        case ComboStep::WAITING_FOR_Q3:
            if (settings.use_q && can_cast_spell(0, 0.0f) && distance <= settings.q_extend_range) {
                if (cast_q(target)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_Q3;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else {
                // If we can't cast Q3, wait a bit longer
                if (current_time - combo_state.last_step_time > 1.0f) {
                    combo_state.combo_step = ComboStep::IDLE;
                    combo_state.combo_in_progress = false;
                }
            }
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_Q3:
            // This state is handled in on_before_attack
            // If AA doesn't happen for too long, reset combo
            if (current_time - combo_state.last_step_time > 0.5f) {
                combo_state.combo_step = ComboStep::IDLE;
                combo_state.combo_in_progress = false;
                combo_state.waiting_for_aa = false;
            }
            break;

        default:
            // Reset if in an unknown state
            combo_state.combo_step = ComboStep::IDLE;
            combo_state.combo_in_progress = false;
            break;
        }
    }

    void handle_harass() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead() || !sdk::orbwalker->harass()) return;

        if (player->get_par() < settings.harass_min_energy / 100.f * player->get_max_par()) return;

        auto target = get_best_target();
        if (!target || !target->is_valid() || target->is_dead()) return;

        try {
            if (!combo_state.waiting_for_aa) {
                if (settings.harass_use_q) cast_q(target);
                if (settings.harass_use_w) cast_w(target);
            }
        }
        catch (const std::exception&) {
            combo_state.waiting_for_aa = false;
        }
    }

    void handle_lane_clear() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead() || !sdk::orbwalker->clear()) return;

        if (player->get_par() < settings.clear_min_energy / 100.f * player->get_max_par()) return;

        // Use the same state machine for lane clear
        float current_time = g_sdk->clock_facade->get_game_time();

        // Reset combo state if we haven't done anything in a while
        if (combo_state.combo_in_progress && current_time - combo_state.last_step_time > 2.0f) {
            combo_state.combo_step = ComboStep::IDLE;
            combo_state.combo_in_progress = false;
        }

        // Find best minion target
        // Find best minion target
        game_object* best_minion = nullptr;
        float best_distance = FLT_MAX;

        for (auto minion : g_sdk->object_manager->get_minions()) {
            if (minion && minion->is_valid() && !minion->is_dead() &&
                minion->get_team_id() != player->get_team_id() && minion->is_lane_minion()) {

                float distance = player->get_position().distance(minion->get_position());
                if (distance < best_distance && distance <= settings.q_extend_range) {
                    best_distance = distance;
                    best_minion = minion;
                }
            }
        }

        if (!best_minion) return;
        combo_state.current_target = best_minion;

        // Start a new farm sequence if not in progress
        if (!combo_state.combo_in_progress) {
            if (settings.clear_use_q && can_cast_spell(0, 0.0f)) {
                combo_state.combo_step = ComboStep::WAITING_FOR_Q1;
                combo_state.combo_in_progress = true;
            }
            else if (settings.clear_use_w && can_cast_spell(1, 0.0f) && best_distance <= settings.w_range) {
                combo_state.combo_step = ComboStep::WAITING_FOR_W;
                combo_state.combo_in_progress = true;
            }
        }

        // Execute the farm sequence step by step
        if (combo_state.waiting_for_aa) return;

        switch (combo_state.combo_step) {
        case ComboStep::WAITING_FOR_W:
            if (settings.clear_use_w && can_cast_spell(1, 0.0f) && best_distance <= settings.w_range) {
                if (cast_w(best_minion)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_W;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else {
                // Skip W if not available
                combo_state.combo_step = ComboStep::WAITING_FOR_Q1;
            }
            break;

        case ComboStep::WAITING_FOR_Q1:
            if (settings.clear_use_q && can_cast_spell(0, 0.0f)) {
                if (cast_q(best_minion)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_Q1;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else {
                // Reset if Q1 can't be cast
                combo_state.combo_step = ComboStep::IDLE;
                combo_state.combo_in_progress = false;
            }
            break;

        case ComboStep::WAITING_FOR_Q2:
            if (settings.clear_use_q && can_cast_spell(0, 0.0f)) {
                if (cast_q(best_minion)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_Q2;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else if (current_time - combo_state.last_step_time > 1.0f) {
                combo_state.combo_step = ComboStep::IDLE;
                combo_state.combo_in_progress = false;
            }
            break;

        case ComboStep::WAITING_FOR_Q3:
            if (settings.clear_use_q && can_cast_spell(0, 0.0f)) {
                if (cast_q(best_minion)) {
                    combo_state.combo_step = ComboStep::WAITING_FOR_AA_AFTER_Q3;
                    combo_state.last_step_time = current_time;
                    combo_state.waiting_for_aa = true;
                }
            }
            else if (current_time - combo_state.last_step_time > 1.0f) {
                combo_state.combo_step = ComboStep::IDLE;
                combo_state.combo_in_progress = false;
            }
            break;
        }
    }

    void handle_animation_cancel() {
        if (!combo_state.waiting_for_aa) return;

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time < combo_state.last_aa_time) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        try {
            math::vector3 cursor_pos = g_sdk->hud_manager->get_cursor_position();
            math::vector3 move_pos = player->get_position().extended(cursor_pos, 50.f);

            if (vengaboys::OrderManager::get_instance().move_to(move_pos, vengaboys::OrderPriority::Low)) {
                combo_state.waiting_for_aa = false;
            }
        }
        catch (const std::exception&) {
            combo_state.waiting_for_aa = false;
        }
    }

    void handle_q_keep_alive() {
        if (!settings.q_keep_alive) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        try {
            float current_time = g_sdk->clock_facade->get_game_time();

            // Only keep Q alive if we're not in combat and it's about to expire
            bool in_combat = false;
            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (enemy && enemy->is_valid() && !enemy->is_dead() &&
                    enemy->get_team_id() != player->get_team_id()) {
                    float distance = player->get_position().distance(enemy->get_position());
                    if (distance < 1000.f) {
                        in_combat = true;
                        break;
                    }
                }
            }

            // Only keep Q stacks if we're not in combat
            if (!in_combat && combo_state.q_count > 0 && current_time - combo_state.last_q_time > 3.5f) {
                if (can_cast_spell(0, 0.f)) {
                    // Get exact cursor position from HUD manager and use it directly
                    math::vector3 cursor_pos = g_sdk->hud_manager->get_cursor_position();

                    // Don't modify or extend the position, cast directly at cursor
                    if (vengaboys::OrderManager::get_instance().cast_spell(0, cursor_pos, vengaboys::OrderPriority::Low)) {
                        spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                        combo_state.last_q_time = current_time;
                    }
                }
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }
    }

    void handle_magnet_mode() {
        if (!settings.magnet_mode || !sdk::orbwalker->combo()) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        auto target = combo_state.current_target;
        if (!target || !target->is_valid() || target->is_dead()) {
            target = get_best_target();
            if (!target) return;
            combo_state.current_target = target;
        }

        float distance = player->get_position().distance(target->get_position());

        // If we're too far from target, move closer
        if (distance > settings.magnet_follow_distance) {
            // Calculate position closer to target
            math::vector3 move_pos = player->get_position().extended(target->get_position(),
                distance - settings.magnet_follow_distance);

            // Only move if we haven't issued a move command recently
            static float last_magnet_move_time = 0.f;
            float current_time = g_sdk->clock_facade->get_game_time();

            if (current_time - last_magnet_move_time > 0.25f) {
                vengaboys::OrderManager::get_instance().move_to(move_pos, vengaboys::OrderPriority::Normal);
                last_magnet_move_time = current_time;
            }
        }
    }

    void __fastcall on_update() {
        if (!g_sdk || !initialized) return;

        try {
            auto player = g_sdk->object_manager->get_local_player();
            if (!player || player->is_dead()) {
                combo_state.waiting_for_aa = false;
                combo_state.current_target = nullptr;
                return;
            }

            float current_time = g_sdk->clock_facade->get_game_time();

            static float last_throttle_reset_time = 0.0f;
            if (vengaboys::OrderManager::get_instance().is_throttled() &&
                (current_time - last_throttle_reset_time > 0.5f)) {
                vengaboys::OrderManager::get_instance().reset_throttle();
                last_throttle_reset_time = current_time;
                return;
            }

            static float last_queue_clear_time = 0.0f;
            if (!combo_state.r_active && !combo_state.waiting_for_aa &&
                (current_time - last_queue_clear_time > 0.5f)) {
                vengaboys::OrderManager::get_instance().clear_queue();
                last_queue_clear_time = current_time;
            }

            handle_animation_cancel();
            handle_q_keep_alive();

            static float last_mode_handle_time = 0.0f;
            if (current_time - last_mode_handle_time < 0.05f) {
                return;
            }
            last_mode_handle_time = current_time;

            if (sdk::orbwalker->combo()) {
                handle_combo();
                handle_magnet_mode();
            }
            else if (sdk::orbwalker->harass()) {
                handle_harass();
            }
            else if (sdk::orbwalker->clear()) {
                handle_lane_clear();
            }
        }
        catch (const std::exception&) {
            combo_state.waiting_for_aa = false;
            combo_state.current_target = nullptr;
        }
    }

    void __fastcall on_draw() {
        if (!g_sdk || !initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        try {
            if (settings.draw_ranges) {
                const auto& pos = player->get_position();
                g_sdk->renderer->add_circle_3d(pos, settings.w_range, 2.f, 0xFF00FFFF);
                g_sdk->renderer->add_circle_3d(pos, settings.q_extend_range, 2.f, 0xFFFFFF00);
            }

            if (settings.draw_combo_state) {
                math::vector2 pos = g_sdk->renderer->world_to_screen(player->get_position());

                std::string state_text = "Q: " + std::to_string(combo_state.q_count);
                if (combo_state.r_active) {
                    state_text += " R ACTIVE";
                }

                // Add combo step information
                std::string step_text = "Combo: ";
                switch (combo_state.combo_step) {
                case ComboStep::IDLE: step_text += "IDLE"; break;
                case ComboStep::WAITING_FOR_E: step_text += "E"; break;
                case ComboStep::WAITING_FOR_AA_AFTER_E: step_text += "AA after E"; break;
                case ComboStep::WAITING_FOR_W: step_text += "W"; break;
                case ComboStep::WAITING_FOR_AA_AFTER_W: step_text += "AA after W"; break;
                case ComboStep::WAITING_FOR_Q1: step_text += "Q1"; break;
                case ComboStep::WAITING_FOR_AA_AFTER_Q1: step_text += "AA after Q1"; break;
                case ComboStep::WAITING_FOR_Q2: step_text += "Q2"; break;
                case ComboStep::WAITING_FOR_AA_AFTER_Q2: step_text += "AA after Q2"; break;
                case ComboStep::WAITING_FOR_Q3: step_text += "Q3"; break;
                case ComboStep::WAITING_FOR_AA_AFTER_Q3: step_text += "AA after Q3"; break;
                }

                g_sdk->renderer->add_text(state_text, 14.f, math::vector2(pos.x, pos.y - 50.f), 1, 0xFF00FF00);
                g_sdk->renderer->add_text(step_text, 14.f, math::vector2(pos.x, pos.y - 70.f), 1, 0xFFFFFF00);
            }

            if (settings.draw_damage_prediction && combo_state.current_target) {
                float damage = get_total_damage(combo_state.current_target, true);
                g_sdk->renderer->add_damage_indicator(combo_state.current_target, damage);
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }
    }

    bool __fastcall on_before_attack(orb_sdk::event_data* args) {
        if (!args) return true;

        combo_state.waiting_for_aa = false;
        combo_state.last_aa_time = g_sdk->clock_facade->get_game_time();

        // Progress through combo steps when AA is performed
        switch (combo_state.combo_step) {
        case ComboStep::WAITING_FOR_AA_AFTER_E:
            combo_state.combo_step = ComboStep::WAITING_FOR_W;
            combo_state.last_step_time = g_sdk->clock_facade->get_game_time();
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_W:
            combo_state.combo_step = ComboStep::WAITING_FOR_Q1;
            combo_state.last_step_time = g_sdk->clock_facade->get_game_time();
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_Q1:
            combo_state.combo_step = ComboStep::WAITING_FOR_Q2;
            combo_state.last_step_time = g_sdk->clock_facade->get_game_time();
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_Q2:
            combo_state.combo_step = ComboStep::WAITING_FOR_Q3;
            combo_state.last_step_time = g_sdk->clock_facade->get_game_time();
            break;

        case ComboStep::WAITING_FOR_AA_AFTER_Q3:
            combo_state.combo_step = ComboStep::IDLE;
            combo_state.combo_in_progress = false;
            combo_state.last_step_time = g_sdk->clock_facade->get_game_time();
            break;
        }

        return true;
    }

    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast) {
        if (!sender || sender != g_sdk->object_manager->get_local_player() || !cast) return;

        try {
            if (cast->get_spell_slot() == 0) {
                combo_state.q_count = (combo_state.q_count + 1) % 3;
                combo_state.last_q_time = g_sdk->clock_facade->get_game_time();
            }
            else if (cast->get_spell_slot() == 3) {
                combo_state.r_active = !combo_state.r_active;
                if (combo_state.r_active) {
                    combo_state.r_start_time = g_sdk->clock_facade->get_game_time();
                }
            }
        }
        catch (const std::exception&) {
            // Handle exception silently
        }
    }

    void create_menu() {
        menu = g_sdk->menu_manager->add_category("rivenAIO", "Riven");
        if (!menu) return;

        auto combo_menu = menu->add_sub_category("combo_settings", "Combo Settings");
        if (combo_menu) {
            combo_menu->add_checkbox("use_q", "Use Q", settings.use_q, [&](bool value) { settings.use_q = value; });
            combo_menu->add_checkbox("use_w", "Use W", settings.use_w, [&](bool value) { settings.use_w = value; });
            combo_menu->add_checkbox("use_e", "Use E", settings.use_e, [&](bool value) { settings.use_e = value; });
            combo_menu->add_checkbox("use_r1", "Use R1", settings.use_r1, [&](bool value) { settings.use_r1 = value; });
            combo_menu->add_checkbox("use_r2", "Use R2", settings.use_r2, [&](bool value) { settings.use_r2 = value; });
            combo_menu->add_slider_float("r_execute", "R2 Execute Threshold %", 0.f, 100.f, 5.f, settings.r_hp_threshold,
                [&](float value) { settings.r_hp_threshold = value; });
            combo_menu->add_slider_int("r_min_targets", "Min R1 Targets", 1, 5, 1, settings.r_min_targets,
                [&](int value) { settings.r_min_targets = value; });
        }

        auto harass_menu = menu->add_sub_category("harass_settings", "Harass Settings");
        if (harass_menu) {
            harass_menu->add_checkbox("harass_use_q", "Use Q", settings.harass_use_q, [&](bool value) { settings.harass_use_q = value; });
            harass_menu->add_checkbox("harass_use_w", "Use W", settings.harass_use_w, [&](bool value) { settings.harass_use_w = value; });
        }

        auto clear_menu = menu->add_sub_category("clear_settings", "Lane Clear Settings");
        if (clear_menu) {
            clear_menu->add_checkbox("clear_use_q", "Use Q", settings.clear_use_q, [&](bool value) { settings.clear_use_q = value; });
            clear_menu->add_checkbox("clear_use_w", "Use W", settings.clear_use_w, [&](bool value) { settings.clear_use_w = value; });
        }

        auto q_settings = menu->add_sub_category("q_settings", ("Q - " + champion_spell_names::get_champion_q_name("Riven") + " Settings").c_str());
        if (q_settings) {
            q_settings->add_checkbox("q_keep_alive", "Keep Q Stack", settings.q_keep_alive,
                [&](bool value) { settings.q_keep_alive = value; });
            q_settings->add_slider_float("q_range", "Q Extended Range", 600.f, 1000.f, 25.f, settings.q_extend_range,
                [&](float value) { settings.q_extend_range = value; });
        }

        auto w_settings = menu->add_sub_category("w_settings", ("W - " + champion_spell_names::get_champion_w_name("Riven") + " Settings").c_str());
        if (w_settings) {
            w_settings->add_slider_float("w_range", "W Range", 200.f, 300.f, 10.f, settings.w_range,
                [&](float value) { settings.w_range = value; });
        }

        auto e_settings = menu->add_sub_category("e_settings", ("E - " + champion_spell_names::get_champion_e_name("Riven") + " Settings").c_str());
        if (e_settings) {
            e_settings->add_checkbox("e_gap_close", "Use E to Gap Close", settings.e_gap_close,
                [&](bool value) { settings.e_gap_close = value; });
            e_settings->add_slider_float("e_gap_close_range", "E Gap Close Range", 200.f, 400.f, 25.f, settings.e_gap_close_range,
                [&](float value) { settings.e_gap_close_range = value; });
        }

        auto r_settings = menu->add_sub_category("r_settings", ("R - " + champion_spell_names::get_champion_r_name("Riven") + " Settings").c_str());
        if (r_settings) {
            r_settings->add_slider_float("r_execute", "R2 Execute Threshold %", 0.f, 100.f, 5.f, settings.r_hp_threshold,
                [&](float value) { settings.r_hp_threshold = value; });
            r_settings->add_slider_int("r_min_targets", "Min R1 Targets", 1, 5, 1, settings.r_min_targets,
                [&](int value) { settings.r_min_targets = value; });
        }

        auto cancel_menu = menu->add_sub_category("cancel_settings", "Animation Cancel");
        if (cancel_menu) {
            cancel_menu->add_checkbox("cancel_q", "Cancel Q Animation", settings.cancel_q,
                [&](bool value) { settings.cancel_q = value; });
            cancel_menu->add_checkbox("cancel_w", "Cancel W Animation", settings.cancel_w,
                [&](bool value) { settings.cancel_w = value; });
            cancel_menu->add_slider_float("cancel_delay", "Cancel Delay", 0.f, 0.5f, 0.01f, settings.cancel_delay,
                [&](float value) { settings.cancel_delay = value; });
        }

        auto draw_menu = menu->add_sub_category("draw_settings", "Drawing Settings");
        if (draw_menu) {
            draw_menu->add_checkbox("draw_ranges", "Draw Ranges", settings.draw_ranges,
                [&](bool value) { settings.draw_ranges = value; });
            draw_menu->add_checkbox("draw_combo", "Draw Combo State", settings.draw_combo_state,
                [&](bool value) { settings.draw_combo_state = value; });
            draw_menu->add_checkbox("draw_damage", "Draw Damage Prediction", settings.draw_damage_prediction,
                [&](bool value) { settings.draw_damage_prediction = value; });
        }

        // Add magnet mode menu
        auto utility_menu = menu->add_sub_category("utility_settings", "Utility Settings");
        if (utility_menu) {
            settings.magnet_mode_key = new std::string("G");
            utility_menu->add_hotkey("magnet_mode", "Magnet Mode", 'G', settings.magnet_mode, true,
                [&](std::string* key, bool value) {
                    settings.magnet_mode = value;
                    settings.magnet_mode_key = key;
                    if (sdk::notification) {
                        if (value) {
                            sdk::notification->add("Riven", "Magnet Mode ON", 0xFF00FF00);
                        }
                        else {
                            sdk::notification->add("Riven", "Magnet Mode OFF", 0xFFFF0000);
                        }
                    }
                });
            utility_menu->add_slider_float("magnet_distance", "Magnet Follow Distance", 125.f, 250.f, 5.f, settings.magnet_follow_distance,
                [&](float value) { settings.magnet_follow_distance = value; });
        }
    }

    void load() {
        g_sdk->log_console("[+] Loading Riven...");

        try {
            g_sdk->set_package("rivenAIO");

            if (!sdk_init::orbwalker() || !sdk_init::target_selector() ||
                !sdk_init::damage() || !sdk_init::notification()) {
                g_sdk->log_console("[!] Failed to initialize required SDKs");
                return;
            }

            auto& order_manager = vengaboys::OrderManager::get_instance();
            order_manager.set_min_order_delay(0.05f);
            order_manager.set_cooldown_after_ult(0.1f);
            g_sdk->log_console("[+] OrderManager configured for Riven fast combos");

            create_menu();

            g_sdk->event_manager->register_callback(event_manager::event::game_update,
                reinterpret_cast<void*>(&riven::on_update));
            g_sdk->event_manager->register_callback(event_manager::event::draw_world,
                reinterpret_cast<void*>(&riven::on_draw));
            g_sdk->event_manager->register_callback(event_manager::event::process_cast,
                reinterpret_cast<void*>(&riven::on_process_spell_cast));
            sdk::orbwalker->register_callback(orb_sdk::before_attack,
                reinterpret_cast<void*>(&riven::on_before_attack));

            // Initialize settings to default values
            settings.q_extend_range = 800.f;
            settings.w_range = 250.f;
            settings.e_gap_close_range = 325.f;
            settings.r_hp_threshold = 25.f;
            settings.r_min_targets = 2;
            settings.cancel_delay = 0.2f; // Adjusted for more consistent animation canceling

            initialized = true;
            g_sdk->log_console("[+] Riven loaded successfully");

            // Send a notification to the user
            if (sdk::notification) {
                sdk::notification->add("Riven AIO", "Riven script loaded successfully", 0xFF00FF00);
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in load: %s", e.what());
        }
    }

    void unload() {
        try {
            combo_state = ComboState();

            g_sdk->event_manager->unregister_callback(event_manager::event::game_update,
                reinterpret_cast<void*>(&riven::on_update));
            g_sdk->event_manager->unregister_callback(event_manager::event::draw_world,
                reinterpret_cast<void*>(&riven::on_draw));
            g_sdk->event_manager->unregister_callback(event_manager::event::process_cast,
                reinterpret_cast<void*>(&riven::on_process_spell_cast));
            sdk::orbwalker->unregister_callback(orb_sdk::before_attack,
                reinterpret_cast<void*>(&riven::on_before_attack));

            if (sdk::infotab && infotab_text_id != 0) {
                sdk::infotab->remove_text(infotab_text_id);
                infotab_text_id = 0;
            }

            // Free allocated memory
            if (settings.magnet_mode_key) {
                delete settings.magnet_mode_key;
                settings.magnet_mode_key = nullptr;
            }

            initialized = false;
            g_sdk->log_console("[+] Riven unloaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in unload: %s", e.what());
        }
    }
}