#include <Windows.h>
#include "sdk.hpp"
#include "nocturne.hpp"
#include "ordermanager.hpp"
#include <limits>
#include <unordered_set>

namespace nocturne {
    Settings settings;
    menu_category* menu = nullptr;
    bool initialized = false;
    bool debug_enabled = false;
    CombatState combat_state{};
    SpellTracker spell_tracker{};
    CachedTarget cached_target{};

    uint32_t infotab_text_id = 0;

    // Tracking variables for preventing spell spam
    float last_successful_e_cast_time = 0.0f;
    uint32_t last_e_target_id = 0;

    // Store reference to OrderManager for better performance
    vengaboys::OrderManager& order_manager = vengaboys::OrderManager::get_instance();

    void log_debug(const char* format, ...) {
        if (!debug_enabled) return;

        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        g_sdk->log_console("[Nocturne Debug] %s", buffer);
    }

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

        // IMPORTANT: Check if the spell is actually leveled up
        if (spell->get_level() == 0) {
            log_debug("can_cast_spell: Spell not leveled yet for slot %d", spell_slot);
            return false;
        }

        // Check the actual cooldown
        if (spell->get_cooldown() > 0.1f) {
            log_debug("can_cast_spell: Spell on cooldown: %.2fs", spell->get_cooldown());
            return false;
        }

        if (!order_manager.can_cast(player, spell_slot, delay)) {
            log_debug("can_cast_spell: OrderManager preventing cast");
            return false;
        }

        return true;
    }

    float calculate_q_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(0);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 65.f, 110.f, 155.f, 200.f, 245.f };
        const float bonus_ad_ratio = 0.85f;
        const float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
        const float total_damage = base_damage[spell->get_level() - 1] + (bonus_ad * bonus_ad_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, total_damage);
    }

    float calculate_e_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(2);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 80.f, 125.f, 170.f, 215.f, 260.f };
        const float ap_ratio = 1.0f;
        const float total_damage = base_damage[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float calculate_r_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(3);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 150.f, 275.f, 400.f };
        const float bonus_ad_ratio = 1.2f;
        const float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
        const float total_damage = base_damage[spell->get_level() - 1] + (bonus_ad * bonus_ad_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, total_damage);
    }

    float calculate_full_combo_damage(game_object* target) {
        if (!target) return 0.f;

        float total_damage = 0.f;

        if (can_cast_spell(0, 0.f))
            total_damage += calculate_q_damage(target);
        if (can_cast_spell(2, 0.f))
            total_damage += calculate_e_damage(target);
        if (can_cast_spell(3, 0.f))
            total_damage += calculate_r_damage(target);

        auto player = g_sdk->object_manager->get_local_player();
        if (player) {
            // Add passive damage if active
            if (combat_state.passive_ready) {
                // Umbra Blades deals 20% total AD
                float passive_damage = player->get_attack_damage() * 0.2f;
                total_damage += sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, passive_damage);
            }

            // Add basic attack damage
            total_damage += sdk::damage->get_aa_damage(player, target);
        }

        return total_damage;
    }

    bool is_under_turret(const math::vector3& pos) {
        for (auto turret : g_sdk->object_manager->get_turrets()) {
            if (!turret->is_valid() || turret->is_dead())
                continue;

            auto player = g_sdk->object_manager->get_local_player();
            if (!player) continue;

            if (turret->get_team_id() != player->get_team_id() &&
                pos.distance(turret->get_position()) <= 775.f) {
                return true;
            }
        }
        return false;
    }

    bool check_dusk_trail(game_object* target) {
        if (!target) return false;

        // Check if target has dusk trail debuff
        for (auto buff : target->get_buffs()) {
            if (buff->get_name() == "NocturneDuskTrail" && buff->is_active()) {
                return true;
            }
        }
        return false;
    }

    // Helper to check if it's safe to cast regarding orbwalker
    bool is_safe_to_cast() {
        if (sdk::orbwalker->would_cancel_attack()) {
            return false;
        }
        return true;
    }

    bool cast_q(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_q: No player found");
            return false;
        }

        // Check if it's safe to cast
        if (!is_safe_to_cast()) {
            return false;
        }

        auto spell = player->get_spell(0);
        if (!spell) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_q: No spell found for slot 0");
            return false;
        }

        // Check if spell is leveled
        if (spell->get_level() == 0) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_q: Spell not leveled yet");
            return false;
        }

        if (spell->get_cooldown() > 0.f) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_q: Spell on cooldown: %.2fs", spell->get_cooldown());
            return false;
        }

        if (player->get_par() < 60.f) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_q: Not enough mana: %.2f", player->get_par());
            return false;
        }

        math::vector3 cast_position;

        if (target && target->is_valid() && target->is_targetable()) {
            cast_position = target->get_position();

            if (settings.q_prediction) {
                // Simple prediction
                if (target->is_moving()) {
                    float speed = target->get_move_speed();
                    float distance = player->get_position().distance(target->get_position());
                    float travel_time = distance / 1600.f; // Q projectile speed

                    auto direction = target->get_direction().normalized();
                    cast_position = cast_position + (direction * speed * travel_time);
                }
            }
        }
        else {
            // Use cursor position if no target
            cast_position = g_sdk->hud_manager->get_cursor_position();
        }

        try {
            if (debug_enabled) {
                g_sdk->log_console("[Nocturne] Casting Q toward position %.1f, %.1f, %.1f",
                    cast_position.x, cast_position.y, cast_position.z);
            }

            if (order_manager.cast_spell(0, cast_position)) {
                spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + spell->get_cooldown());
                combat_state.last_q_time = g_sdk->clock_facade->get_game_time();
                g_sdk->log_console("[Nocturne] Q cast successful");
                return true;
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] OrderManager Q cast returned false");
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_q: %s", e.what());
        }
        return false;
    }

    bool cast_w(bool block_cc) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: No player found");
            return false;
        }

        // Check if it's safe to cast
        if (!is_safe_to_cast()) {
            return false;
        }

        auto spell = player->get_spell(1);
        if (!spell) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: No spell found for slot 1");
            return false;
        }

        // Check if spell is leveled
        if (spell->get_level() == 0) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: Spell not leveled yet");
            return false;
        }

        if (spell->get_cooldown() > 0.f) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: Spell on cooldown: %.2fs", spell->get_cooldown());
            return false;
        }

        // Only cast W strategically - not as spam
        // Check for incoming CC, dangerous projectiles, or low health defensive use
        bool should_cast_w = false;

        // 1. Check if we're being targeted by CC
        if (block_cc) {
            bool cc_threat = false;
            for (auto hero : g_sdk->object_manager->get_heroes()) {
                if (!hero->is_valid() || hero->is_dead() || hero->get_team_id() == player->get_team_id())
                    continue;

                if (hero->get_position().distance(player->get_position()) <= 800.f) {
                    // Check for active spell casts targeting us
                    auto active_spell = hero->get_active_spell_cast();
                    if (active_spell && active_spell->get_spell_cast() &&
                        active_spell->get_spell_cast()->get_target() == player) {
                        cc_threat = true;
                        if (debug_enabled) g_sdk->log_console("[Nocturne] Detected spell being cast at player");
                        break;
                    }
                }
            }

            if (cc_threat) {
                should_cast_w = true;
                if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: Spell targeting detected, using W");
            }
        }

        // 2. Check if player is at low HP in combat
        float hp_ratio = player->get_hp() / player->get_max_hp();
        if (hp_ratio <= settings.w_low_health_threshold / 100.0f) {
            // Only use at low health if we're in combat with enemy champion nearby
            for (auto hero : g_sdk->object_manager->get_heroes()) {
                if (!hero->is_valid() || hero->is_dead() || hero->get_team_id() == player->get_team_id())
                    continue;

                if (hero->get_position().distance(player->get_position()) <= 600.f) {
                    should_cast_w = true;
                    if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: Low health defensive use, enemy nearby");
                    break;
                }
            }
        }

        // 3. Check for any CC effects currently on player
        if (player->has_buff_of_type(buff_type::stun) ||
            player->has_buff_of_type(buff_type::snare) ||
            player->has_buff_of_type(buff_type::charm) ||
            player->has_buff_of_type(buff_type::taunt) ||
            player->has_buff_of_type(buff_type::fear) ||
            player->has_buff_of_type(buff_type::polymorph)) {

            should_cast_w = true;
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: Player has CC, using W");
        }

        if (!should_cast_w) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_w: No strategic reason to use W now");
            return false;
        }

        try {
            if (debug_enabled) g_sdk->log_console("[Nocturne] Casting W defensively");

            if (order_manager.cast_spell(1, player->get_position())) {
                combat_state.last_w_time = g_sdk->clock_facade->get_game_time();
                spell_tracker.update_cooldown(1, g_sdk->clock_facade->get_game_time() + spell->get_cooldown());
                g_sdk->log_console("[Nocturne] W cast successful");
                return true;
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] OrderManager W cast returned false");
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_w: %s", e.what());
        }
        return false;
    }

    bool cast_e(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: No player found");
            return false;
        }

        // Check if it's safe to cast
        if (!is_safe_to_cast()) {
            return false;
        }

        auto spell = player->get_spell(2);
        if (!spell) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: No spell found for slot 2");
            return false;
        }

        // CRITICAL CHECK: Verify the spell is actually leveled
        if (spell->get_level() == 0) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: Spell not leveled yet");
            return false;
        }

        float current_time = g_sdk->clock_facade->get_game_time();

        // Check if E was cast recently (anti-spam)
        if (current_time - last_successful_e_cast_time < 1.0f) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: Anti-spam protection, last cast was %.2f seconds ago",
                current_time - last_successful_e_cast_time);
            return false;
        }

        // If no target provided, try to find one automatically
        if (!target) {
            target = sdk::target_selector->get_hero_target([&](game_object* hero) {
                if (!hero) return false;
                return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                    hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.e_range;
                });

            if (!target) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: No valid target found within range");
                return false;
            }
        }

        // Don't cast on the same target too often
        if (target->get_network_id() == last_e_target_id &&
            current_time - last_successful_e_cast_time < 10.0f) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: Already cast on this target recently");
            return false;
        }

        if (spell->get_cooldown() > 0.f) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: Spell on cooldown: %.2fs", spell->get_cooldown());
            return false;
        }

        if (target->get_position().distance(player->get_position()) > settings.e_range) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: Target out of range: %.1f > %.1f",
                target->get_position().distance(player->get_position()), settings.e_range);
            return false;
        }

        if (!target->is_targetable()) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_e: Target not targetable");
            return false;
        }

        try {
            g_sdk->log_console("[Nocturne] Casting E on %s - Distance: %.1f",
                target->get_char_name().c_str(),
                player->get_position().distance(target->get_position()));

            if (order_manager.cast_spell(2, target)) {
                combat_state.last_e_time = current_time;
                combat_state.e_target = target;
                last_successful_e_cast_time = current_time;
                last_e_target_id = target->get_network_id();
                spell_tracker.update_cooldown(2, current_time + spell->get_cooldown());
                g_sdk->log_console("[Nocturne] E cast successful on %s", target->get_char_name().c_str());
                return true;
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] OrderManager E cast returned false");
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_e: %s", e.what());
        }
        return false;
    }

    bool cast_r(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_r: Invalid player or target");
            return false;
        }

        // Check if it's safe to cast
        if (!is_safe_to_cast()) {
            return false;
        }

        auto spell = player->get_spell(3);
        if (!spell) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_r: No spell found for slot 3");
            return false;
        }

        // Check if spell is leveled
        if (spell->get_level() == 0) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_r: Spell not leveled yet");
            return false;
        }

        if (spell->get_cooldown() > 0.f) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_r: Spell on cooldown: %.2fs", spell->get_cooldown());
            return false;
        }

        if (target->get_position().distance(player->get_position()) > settings.r_range) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_r: Target out of range: %.1f > %.1f",
                target->get_position().distance(player->get_position()), settings.r_range);
            return false;
        }

        if (!target->is_targetable() || target->is_zombie()) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_r: Target not targetable");
            return false;
        }

        try {
            g_sdk->log_console("[Nocturne] Casting R on %s", target->get_char_name().c_str());

            if (order_manager.cast_spell(3, target, vengaboys::OrderPriority::Critical)) {
                combat_state.last_r_time = g_sdk->clock_facade->get_game_time();
                spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + spell->get_cooldown());
                return true;
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] OrderManager R cast returned false");
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_r: %s", e.what());
        }
        return false;
    }

    bool cast_re_dash_to_target(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_re_dash_to_target: Invalid player or target");
            return false;
        }

        if (!combat_state.r_active) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] cast_re_dash_to_target: R not active");
            return false;
        }

        // Check if it's safe to cast
        if (!is_safe_to_cast()) {
            return false;
        }

        try {
            g_sdk->log_console("[Nocturne] Re-casting R to dash to %s", target->get_char_name().c_str());

            if (order_manager.cast_spell(3, target, vengaboys::OrderPriority::Critical)) {
                combat_state.r_dashed = true;
                return true;
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] OrderManager R recast returned false");
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_re_dash_to_target: %s", e.what());
        }
        return false;
    }

    void handle_combo() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: No player found");
            return;
        }

        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - combat_state.last_combo_time < settings.min_spell_delay) {
            return;
        }

        // Get spell levels FIRST to avoid any casting attempts for unleveled spells
        int q_level = 0;
        int w_level = 0;
        int e_level = 0;
        int r_level = 0;

        auto q_spell = player->get_spell(0);
        auto w_spell = player->get_spell(1);
        auto e_spell = player->get_spell(2);
        auto r_spell = player->get_spell(3);

        if (q_spell) q_level = q_spell->get_level();
        if (w_spell) w_level = w_spell->get_level();
        if (e_spell) e_level = e_spell->get_level();
        if (r_spell) r_level = r_spell->get_level();

        if (debug_enabled) {
            g_sdk->log_console("[Nocturne] handle_combo: Spell levels - Q:%d, W:%d, E:%d, R:%d",
                q_level, w_level, e_level, r_level);
        }

        if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Player HP: %.2f", player->get_hp());

        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero) return false;
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;
            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                hero->is_visible();
            });

        cached_target.target = target;

        if (!cached_target.target) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: No target found");
            return;
        }

        if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Target found: %s, distance: %.1f, Target HP: %.1f/%.1f (%.1f%%)",
            target->get_char_name().c_str(),
            player->get_position().distance(target->get_position()),
            target->get_hp(),
            target->get_max_hp(),
            (target->get_hp() / target->get_max_hp()) * 100.0f);

        bool spell_cast = false;

        // R priority - execute when target is low enough for a kill
        if (settings.use_r && r_level > 0 && r_spell && r_spell->get_cooldown() <= 0.f &&
            target->get_position().distance(player->get_position()) <= settings.r_range &&
            !combat_state.r_active) {

            bool should_cast_r = false;

            // Calculate if we can kill with R + followup
            if (settings.r_use_lethal) {
                float r_damage = calculate_r_damage(target);
                float q_damage = calculate_q_damage(target);
                float e_damage = calculate_e_damage(target);
                float aa_damage = sdk::damage->get_aa_damage(player, target);

                // Calculate full combo damage
                float combo_damage = r_damage + q_damage + e_damage + aa_damage;

                // Calculate target's effective health (account for healing)
                float health_threshold = target->get_hp() * 1.15f; // Add 15% buffer for healing/shields

                // HP threshold for R
                float hp_percentage = target->get_hp() / target->get_max_hp() * 100.0f;

                if (debug_enabled) g_sdk->log_console("[Nocturne] R Lethal Check: Target HP: %.1f, Combo Damage: %.1f, HP%%: %.1f%%",
                    target->get_hp(), combo_damage, hp_percentage);

                if (combo_damage >= health_threshold || hp_percentage <= 30.0f) {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Target can be killed with R combo");
                    should_cast_r = true;
                }
            }

            if (!should_cast_r && settings.r_use_if_escaping) {
                if (target->is_moving() && target->get_position().distance(player->get_position()) > 450.f) {
                    // Check if target is running away
                    auto direction = (target->get_position() - player->get_position()).normalized();
                    auto target_direction = target->get_direction().normalized();

                    float dot_product = direction.dot(target_direction);
                    if (dot_product > 0.7f) {
                        if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Target is escaping, using R");
                        should_cast_r = true;
                    }
                }
            }

            if (should_cast_r) {
                spell_cast = cast_r(target);
                if (spell_cast) {
                    g_sdk->log_console("[Nocturne] handle_combo: R cast successful");
                    combat_state.r_active = true;
                    combat_state.r_activation_time = current_time;
                }
                else {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: R cast failed");
                }
            }
        }

        // R recast (dash to target) if R is active and we haven't dashed yet
        if (!spell_cast && combat_state.r_active && !combat_state.r_dashed &&
            (current_time - combat_state.r_activation_time) >= settings.r_recast_delay) {

            spell_cast = cast_re_dash_to_target(target);
            if (spell_cast) {
                g_sdk->log_console("[Nocturne] handle_combo: Dashed to target with R");
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: R dash failed");
            }
        }

        // Q has high priority to get the movement speed bonus
        if (settings.use_q && q_level > 0 && q_spell && q_spell->get_cooldown() <= 0.f) {
            if (target->get_position().distance(player->get_position()) <= settings.q_range) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Attempting Q cast");
                spell_cast = cast_q(target);
                if (spell_cast) {
                    g_sdk->log_console("[Nocturne] handle_combo: Q cast successful");
                }
                else {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Q cast failed");
                }
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Target out of Q range: %.1f > %.1f",
                    target->get_position().distance(player->get_position()), settings.q_range);
            }
        }

        // E if in range - ONLY if leveled
        if (settings.use_e && e_level > 0 && e_spell && e_spell->get_cooldown() <= 0.f) {
            if (target->get_position().distance(player->get_position()) <= settings.e_range) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Attempting E cast");
                spell_cast = cast_e(target);
                if (spell_cast) {
                    g_sdk->log_console("[Nocturne] handle_combo: E cast successful");
                }
                else {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: E cast failed");
                }
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Target out of E range: %.1f > %.1f",
                    target->get_position().distance(player->get_position()), settings.e_range);
            }
        }

        // W only for defensive purposes - ONLY if leveled
        if (settings.use_w && w_level > 0 && w_spell && w_spell->get_cooldown() <= 0.f) {
            // Only use W defensively in combo
            float hp_ratio = player->get_hp() / player->get_max_hp();
            bool nearby_enemy = target->get_position().distance(player->get_position()) < 400.f;

            if (hp_ratio <= settings.w_low_health_threshold / 100.0f && nearby_enemy) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: Low health, using W defensively");
                spell_cast = cast_w(true);
            }
        }

        if (spell_cast) {
            combat_state.last_combo_time = current_time;
        }
        else {
            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_combo: No spells were cast this cycle");
        }
    }

    void handle_harass() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.harass_min_mana / 100.f * player->get_max_par()) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - combat_state.last_harass_time < settings.min_spell_delay) {
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

        if (!target) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: No target found in range");
            return;
        }

        // For harass, we primarily use Q 
        if (settings.harass_use_q && can_cast_spell(0, 0.f)) {
            if (!settings.harass_under_turret || !is_under_turret(target->get_position())) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: Attempting Q cast on %s", target->get_char_name().c_str());
                spell_cast = cast_q(target);
                if (spell_cast) {
                    g_sdk->log_console("[Nocturne] handle_harass: Q cast successful");
                }
                else {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: Q cast failed");
                }
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: Target under turret, skipping Q");
            }
        }

        // Use E in harass if target is close enough and setting enabled
        if (!spell_cast && settings.harass_use_e && can_cast_spell(2, 0.f)) {
            if (target->get_position().distance(player->get_position()) <= settings.e_range) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: Attempting E cast on %s", target->get_char_name().c_str());
                spell_cast = cast_e(target);
                if (spell_cast) {
                    g_sdk->log_console("[Nocturne] handle_harass: E cast successful");
                }
                else {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: E cast failed");
                }
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: Target out of E range: %.1f > %.1f",
                    target->get_position().distance(player->get_position()), settings.e_range);
            }
        }

        if (spell_cast) {
            combat_state.last_harass_time = current_time;
        }
        else {
            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_harass: No spells cast this cycle");
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

        // Get spell levels FIRST to avoid any casting attempts for unleveled spells
        int q_level = 0;
        int w_level = 0;
        int e_level = 0;

        auto q_spell = player->get_spell(0);
        auto w_spell = player->get_spell(1);
        auto e_spell = player->get_spell(2);

        if (q_spell) q_level = q_spell->get_level();
        if (w_spell) w_level = w_spell->get_level();
        if (e_spell) e_level = e_spell->get_level();

        if (debug_enabled) {
            g_sdk->log_console("[Nocturne] handle_farming: Spell levels - Q:%d, W:%d, E:%d", q_level, w_level, e_level);
        }

        // E on large monsters - only if the spell is actually leveled
        if (settings.use_e_on_big_monsters && settings.farm_use_e && e_level > 0 && e_spell && e_spell->get_cooldown() <= 0.f &&
            (player->get_par() / player->get_max_par() > settings.e_big_monster_mana_threshold / 100.f)) {

            // Double check E level again to be extra safe
            if (e_level == 0) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: E not leveled yet (secondary check)");
                return;
            }

            // Check anti-spam timer
            if (current_time - last_successful_e_cast_time < 1.0f) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: Too soon to cast E again, waiting");
                return;
            }

            // Look for big monsters (red buff, blue buff, gromp, etc.)
            for (auto monster : g_sdk->object_manager->get_monsters()) {
                if (!monster->is_valid() || monster->is_dead())
                    continue;

                // Skip if we just cast on this monster recently
                if (monster->get_network_id() == last_e_target_id &&
                    current_time - last_successful_e_cast_time < 10.0f) {
                    continue;
                }

                if (monster->get_position().distance(player->get_position()) <= settings.e_range) {
                    // Check if it's a large monster (red buff, blue buff, gromp, etc.)
                    if (monster->is_large_monster() || monster->is_buff_monster()) {
                        if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: Found large monster in lane clear, attempting E cast");
                        if (cast_e(monster)) {
                            g_sdk->log_console("[Nocturne] handle_farming: E cast on large monster successful");
                            break;
                        }
                    }
                }
            }
        }

        // Q for wave clear - hit multiple minions in line
        if (settings.farm_use_q && q_level > 0 && q_spell && q_spell->get_cooldown() <= 0.f &&
            (current_time - combat_state.last_farm_q_time >= 0.2f)) {

            if (spell_farm_enabled) {
                int minions_in_q_range = 0;
                math::vector3 best_q_pos;
                int max_minions_hit = 0;
                bool valid_position_found = false;  // Track if we found a valid position

                for (auto minion : g_sdk->object_manager->get_minions()) {
                    if (!minion->is_valid() || minion->is_dead() || minion->get_team_id() == player->get_team_id())
                        continue;

                    if (minion->get_position().distance(player->get_position()) <= settings.q_range) {
                        minions_in_q_range++;

                        // Find position to hit multiple minions
                        int minions_hit = 0;
                        for (auto other_minion : g_sdk->object_manager->get_minions()) {
                            if (!other_minion->is_valid() || other_minion->is_dead() ||
                                other_minion->get_team_id() == player->get_team_id())
                                continue;

                            // Check if minions are roughly in a line
                            auto direction = (minion->get_position() - player->get_position()).normalized();
                            auto to_other = other_minion->get_position() - player->get_position();
                            auto projected = player->get_position() + direction * to_other.magnitude() * direction.dot(to_other.normalized());

                            if (projected.distance(other_minion->get_position()) <= 120.f) {
                                minions_hit++;
                            }
                        }

                        if (minions_hit > max_minions_hit) {
                            max_minions_hit = minions_hit;
                            best_q_pos = minion->get_position();
                            valid_position_found = true;  // We found a valid position
                        }
                    }
                }

                // Only cast if we have enough minions AND a valid position
                if (max_minions_hit >= settings.farm_min_minions_q && valid_position_found) {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: Attempting Q cast to hit %d minions", max_minions_hit);
                    if (order_manager.cast_spell(0, best_q_pos)) {
                        combat_state.last_farm_q_time = current_time;
                        g_sdk->log_console("[Nocturne] handle_farming: Q cast to hit %d minions", max_minions_hit);
                    }
                    else {
                        if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: Q cast failed");
                    }
                }
            }
            else if (spell_farm_lasthit_enabled) {
                // Use Q for last hits
                for (auto minion : g_sdk->object_manager->get_minions()) {
                    if (!minion->is_valid() || minion->is_dead() ||
                        minion->get_team_id() == player->get_team_id())
                        continue;

                    if (minion->get_position().distance(player->get_position()) <= settings.q_range) {
                        if (calculate_q_damage(minion) >= minion->get_hp()) {
                            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: Attempting Q cast for last hit");
                            if (order_manager.cast_spell(0, minion->get_position())) {
                                combat_state.last_farm_q_time = current_time;
                                g_sdk->log_console("[Nocturne] handle_farming: Q cast for last hit");
                                break;
                            }
                            else {
                                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: Q cast failed");
                            }
                        }
                    }
                }
            }
        }

        // Use passive effectively for farming
        combat_state.check_passive_status(player);
        if (combat_state.passive_ready && spell_farm_enabled) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_farming: Passive ready for AoE damage");
        }
    }

    void handle_jungle_clear() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.farm_min_mana / 100.f * player->get_max_par()) return;

        if (!sdk::orbwalker->is_spell_farm_enabled()) {
            return;
        }

        float current_time = g_sdk->clock_facade->get_game_time();

        // Get spell levels FIRST to avoid any casting attempts for unleveled spells
        int q_level = 0;
        int w_level = 0;
        int e_level = 0;

        auto q_spell = player->get_spell(0);
        auto w_spell = player->get_spell(1);
        auto e_spell = player->get_spell(2);

        if (q_spell) q_level = q_spell->get_level();
        if (w_spell) w_level = w_spell->get_level();
        if (e_spell) e_level = e_spell->get_level();

        if (debug_enabled) {
            g_sdk->log_console("[Nocturne] handle_jungle_clear: Spell levels - Q:%d, W:%d, E:%d", q_level, w_level, e_level);
        }

        // Q for jungle monsters - only if Q is leveled
        if (settings.farm_use_q && q_level > 0 && q_spell && q_spell->get_cooldown() <= 0.f &&
            (current_time - combat_state.last_farm_q_time >= 0.2f)) {

            game_object* largest_monster = nullptr;
            float max_hp = 0.f;

            for (auto monster : g_sdk->object_manager->get_monsters()) {
                if (!monster->is_valid() || monster->is_dead())
                    continue;

                if (monster->get_position().distance(player->get_position()) <= settings.q_range) {
                    if (monster->get_max_hp() > max_hp) {
                        max_hp = monster->get_max_hp();
                        largest_monster = monster;
                    }
                }
            }

            if (largest_monster) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: Attempting Q cast on monster");
                if (cast_q(largest_monster)) {
                    combat_state.last_farm_q_time = current_time;
                    g_sdk->log_console("[Nocturne] handle_jungle_clear: Q cast on monster successful");
                }
                else {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: Q cast on monster failed");
                }
            }
        }

        // E for big monsters - ONLY if E is actually leveled 
        if (settings.farm_use_e && e_level > 0 && e_spell && e_spell->get_cooldown() <= 0.f) {
            // CRITICAL: E must be leveled - double check
            if (e_level == 0) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: E not leveled yet (secondary check)");
                return;
            }

            // Check if E was cast recently (anti-spam)
            if (current_time - last_successful_e_cast_time < 1.0f) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: Too soon to cast E again, waiting");
                return;
            }

            for (auto monster : g_sdk->object_manager->get_monsters()) {
                if (!monster->is_valid() || monster->is_dead())
                    continue;

                // Skip if we just cast on this monster
                if (monster->get_network_id() == last_e_target_id &&
                    current_time - last_successful_e_cast_time < 10.0f) {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: Already cast E on this monster recently");
                    continue;
                }

                if (monster->get_position().distance(player->get_position()) <= settings.e_range) {
                    if (monster->is_epic_monster() || monster->is_large_monster()) {
                        if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: Attempting E cast on large monster");
                        if (cast_e(monster)) {
                            g_sdk->log_console("[Nocturne] handle_jungle_clear: E cast on large monster successful");
                            break;
                        }
                        else {
                            if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: E cast on large monster failed");
                        }
                    }
                }
            }
        }

        // W to mitigate damage from large monsters - only if leveled
        if (settings.farm_use_w && w_level > 0 && w_spell && w_spell->get_cooldown() <= 0.f) {
            float hp_ratio = player->get_hp() / player->get_max_hp();

            if (hp_ratio < 0.5f) {
                bool large_monster_nearby = false;

                for (auto monster : g_sdk->object_manager->get_monsters()) {
                    if (!monster->is_valid() || monster->is_dead())
                        continue;

                    if (monster->get_position().distance(player->get_position()) <= 500.f) {
                        if (monster->is_large_monster() || monster->is_epic_monster()) {
                            large_monster_nearby = true;
                            break;
                        }
                    }
                }

                if (large_monster_nearby) {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: Attempting to cast W defensively");
                    if (cast_w(false)) {
                        g_sdk->log_console("[Nocturne] handle_jungle_clear: W cast defensively successful");
                    }
                    else {
                        if (debug_enabled) g_sdk->log_console("[Nocturne] handle_jungle_clear: W cast failed");
                    }
                }
            }
        }

        // Use passive effectively for jungle
        combat_state.check_passive_status(player);
    }

    void create_menu() {
        menu = g_sdk->menu_manager->add_category("nocturne", "Nocturne");
        if (!menu) return;

        auto combat = menu->add_sub_category("combat", "Combat Settings");
        if (combat) {
            combat->add_checkbox("use_q", "Q - Duskbringer", settings.use_q, [&](bool value) { settings.use_q = value; });
            combat->add_checkbox("use_w", "W - Shroud of Darkness", settings.use_w, [&](bool value) { settings.use_w = value; });
            combat->add_checkbox("use_e", "E - Unspeakable Horror", settings.use_e, [&](bool value) { settings.use_e = value; });
            combat->add_checkbox("use_r", "R - Paranoia", settings.use_r, [&](bool value) { settings.use_r = value; });

            auto q_settings = combat->add_sub_category("q_settings", "Q - Duskbringer Settings");
            if (q_settings) {
                q_settings->add_slider_float("q_range", "Range", 500.f, 1200.f, 50.f, settings.q_range,
                    [&](float value) { settings.q_range = value; });
                q_settings->add_checkbox("q_prediction", "Use Prediction", settings.q_prediction,
                    [&](bool value) { settings.q_prediction = value; });
                q_settings->add_slider_float("q_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.q_min_mana,
                    [&](float value) { settings.q_min_mana = value; });
            }

            auto w_settings = combat->add_sub_category("w_settings", "W - Shroud of Darkness Settings");
            if (w_settings) {
                w_settings->add_checkbox("w_auto_cc", "Auto-Cast on CC", settings.w_auto_cc,
                    [&](bool value) { settings.w_auto_cc = value; });
                w_settings->add_slider_float("w_low_health_threshold", "Health Threshold %", 10.f, 50.f, 1.f, settings.w_low_health_threshold,
                    [&](float value) { settings.w_low_health_threshold = value; });
                w_settings->add_slider_float("w_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.w_min_mana,
                    [&](float value) { settings.w_min_mana = value; });
            }

            auto e_settings = combat->add_sub_category("e_settings", "E - Unspeakable Horror Settings");
            if (e_settings) {
                e_settings->add_slider_float("e_range", "Range", 300.f, 500.f, 25.f, settings.e_range,
                    [&](float value) { settings.e_range = value; });
                e_settings->add_slider_float("e_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.e_min_mana,
                    [&](float value) { settings.e_min_mana = value; });
                e_settings->add_slider_float("e_fear_duration", "Fear Duration", 1.25f, 2.25f, 0.25f, settings.e_fear_duration,
                    [&](float value) { settings.e_fear_duration = value; });
            }

            auto r_settings = combat->add_sub_category("r_settings", "R - Paranoia Settings");
            if (r_settings) {
                r_settings->add_slider_float("r_range", "Range", 2000.f, 4000.f, 500.f, settings.r_range,
                    [&](float value) { settings.r_range = value; });
                r_settings->add_checkbox("r_use_lethal", "Use for Lethal", settings.r_use_lethal,
                    [&](bool value) { settings.r_use_lethal = value; });
                r_settings->add_checkbox("r_use_if_escaping", "Use if Target Escaping", settings.r_use_if_escaping,
                    [&](bool value) { settings.r_use_if_escaping = value; });
                r_settings->add_slider_float("r_recast_delay", "Recast Delay", 0.25f, 1.f, 0.05f, settings.r_recast_delay,
                    [&](float value) { settings.r_recast_delay = value; });
                r_settings->add_slider_float("r_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.r_min_mana,
                    [&](float value) { settings.r_min_mana = value; });
            }
        }

        auto harass = menu->add_sub_category("harass", "Harass Settings");
        if (harass) {
            harass->add_checkbox("harass_use_q", "Use Q", settings.harass_use_q,
                [&](bool value) { settings.harass_use_q = value; });
            harass->add_checkbox("harass_use_e", "Use E", settings.harass_use_e,
                [&](bool value) { settings.harass_use_e = value; });
            harass->add_slider_float("harass_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.harass_min_mana,
                [&](float value) { settings.harass_min_mana = value; });
            harass->add_checkbox("harass_under_turret", "Harass Under Turret", settings.harass_under_turret,
                [&](bool value) { settings.harass_under_turret = value; });
        }

        auto farm = menu->add_sub_category("farm", "Farm Settings");
        if (farm) {
            farm->add_checkbox("farm_use_q", "Use Q", settings.farm_use_q,
                [&](bool value) { settings.farm_use_q = value; });
            farm->add_checkbox("farm_use_w", "Use W (Jungle)", settings.farm_use_w,
                [&](bool value) { settings.farm_use_w = value; });
            farm->add_checkbox("farm_use_e", "Use E (Jungle)", settings.farm_use_e,
                [&](bool value) { settings.farm_use_e = value; });

            // Add these new menu options
            farm->add_checkbox("use_e_on_big_monsters", "Use E on Big Monsters", settings.use_e_on_big_monsters,
                [&](bool value) { settings.use_e_on_big_monsters = value; });
            farm->add_slider_float("e_big_monster_mana_threshold", "Big Monster E Mana %", 0.f, 100.f, 5.f, settings.e_big_monster_mana_threshold,
                [&](float value) { settings.e_big_monster_mana_threshold = value; });

            farm->add_slider_float("farm_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.farm_min_mana,
                [&](float value) { settings.farm_min_mana = value; });
            farm->add_slider_int("farm_min_minions_q", "Min Minions for Q", 1, 6, 1, settings.farm_min_minions_q,
                [&](int value) { settings.farm_min_minions_q = value; });
            farm->add_checkbox("farm_under_turret", "Farm Under Turret", settings.farm_under_turret,
                [&](bool value) { settings.farm_under_turret = value; });
        }

        auto drawings = menu->add_sub_category("drawings", "Drawing Settings");
        if (drawings) {
            drawings->add_checkbox("draw_ranges", "Draw Ranges", settings.draw_ranges,
                [&](bool value) { settings.draw_ranges = value; });
            drawings->add_checkbox("draw_damage", "Draw Damage", settings.draw_damage,
                [&](bool value) { settings.draw_damage = value; });
            drawings->add_checkbox("draw_q_trail", "Draw Q Trail", settings.draw_q_trail,
                [&](bool value) { settings.draw_q_trail = value; });
            drawings->add_checkbox("draw_e_tether", "Draw E Tether", settings.draw_e_tether,
                [&](bool value) { settings.draw_e_tether = value; });
        }

        auto misc = menu->add_sub_category("misc", "Misc Settings");
        if (misc) {
            misc->add_checkbox("debug_mode", "Debug Mode", debug_enabled,
                [&](bool value) {
                    debug_enabled = value;
                    g_sdk->log_console("[Nocturne] Debug mode %s", value ? "ENABLED" : "DISABLED");
                });
        }
    }

    bool has_dusk_trail_buff(game_object* object) {
        if (!object) return false;

        for (auto buff : object->get_buffs()) {
            if (buff->get_name() == "NocturneDuskTrail" && buff->is_active()) {
                return true;
            }
        }
        return false;
    }

    void CombatState::check_passive_status(game_object* player) {
        if (!player) return;

        // Check if passive is ready by looking for the buff
        for (auto buff : player->get_buffs()) {
            if (buff->get_name() == "NocturneUmbraBlades" && buff->is_active()) {
                passive_ready = true;
                return;
            }
        }
        passive_ready = false;
    }

    bool __fastcall before_attack(orb_sdk::event_data* data) {
        if (!initialized) return true;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !data || !data->target) return true;

        // Check if we have passive ready
        combat_state.check_passive_status(player);

        // If attacking with passive, consider using it for multiple targets
        if (combat_state.passive_ready && data->target->is_lane_minion()) {
            int nearby_minions = 0;

            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead() ||
                    minion->get_team_id() == player->get_team_id())
                    continue;

                if (minion->get_position().distance(data->target->get_position()) <= 300.f) {
                    nearby_minions++;
                }
            }

            // If only 1 minion nearby, maybe save passive for better opportunity
            if (nearby_minions <= 1 && sdk::orbwalker->clear()) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] before_attack: Considering holding passive for better AoE opportunity");
                // Don't actually block the attack, just log the consideration
            }
        }

        return true;
    }

    void __fastcall on_draw() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.draw_ranges) {
            // Draw Q range
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.q_range, 2.f, 0x20BBBBFF);

            // Draw E range 
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.e_range, 2.f, 0x20BB00FF);

            // Draw R range if available
            if (can_cast_spell(3, 0.f)) {
                g_sdk->renderer->add_circle_3d(player->get_position(), settings.r_range, 2.f, 0x20FF00FF);
            }
        }

        // Draw Q trail path if enabled
        if (settings.draw_q_trail) {
            for (auto target : g_sdk->object_manager->get_heroes()) {
                if (!target->is_valid() || target->is_dead()) continue;

                if (has_dusk_trail_buff(target)) {
                    g_sdk->renderer->add_circle_3d(target->get_position(), 100.f, 3.f, 0x80BBBBFF);
                }
            }
        }

        // Draw E tether if active
        if (settings.draw_e_tether && combat_state.e_target) {
            if (combat_state.e_target->is_valid() && !combat_state.e_target->is_dead()) {
                g_sdk->renderer->add_line_2d(
                    g_sdk->renderer->world_to_screen(player->get_position()),
                    g_sdk->renderer->world_to_screen(combat_state.e_target->get_position()),
                    2.f, 0xA0BB00FF);
            }
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
    }

    void __fastcall on_update() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) {
            return;
        }

        // Get spell levels FIRST to avoid any casting attempts for unleveled spells
        int q_level = 0;
        int w_level = 0;
        int e_level = 0;
        int r_level = 0;

        auto q_spell = player->get_spell(0);
        auto w_spell = player->get_spell(1);
        auto e_spell = player->get_spell(2);
        auto r_spell = player->get_spell(3);

        if (q_spell) q_level = q_spell->get_level();
        if (w_spell) w_level = w_spell->get_level();
        if (e_spell) e_level = e_spell->get_level();
        if (r_spell) r_level = r_spell->get_level();

        // Limit logging frequency
        static float last_debug_log = 0.0f;
        float current_time = g_sdk->clock_facade->get_game_time();

        if (debug_enabled && (current_time - last_debug_log > 1.0f)) {
            g_sdk->log_console("[Nocturne] on_update: Frame update - Spell levels: Q:%d, W:%d, E:%d, R:%d",
                q_level, w_level, e_level, r_level);
            last_debug_log = current_time;
        }

        // Update R status
        if (combat_state.r_active) {
            if (current_time - combat_state.r_activation_time > 6.0f) {
                combat_state.r_active = false;
                combat_state.r_dashed = false;
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: R effect timed out after 6 seconds");
            }
        }

        // Update E target validity
        if (combat_state.e_target && (!combat_state.e_target->is_valid() || combat_state.e_target->is_dead())) {
            combat_state.e_target = nullptr;
            if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: E target is no longer valid");
        }

        if (sdk::orbwalker) {
            if (sdk::orbwalker->combo()) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: Combo mode active");
                handle_combo();
            }
            else if (sdk::orbwalker->harass()) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: Harass mode active");
                handle_harass();
            }
            else if (sdk::orbwalker->clear()) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: Clear mode active");
                // Check if we're in lane or jungle based on nearby minions/monsters
                bool jungle_nearby = false;

                for (auto monster : g_sdk->object_manager->get_monsters()) {
                    if (!monster->is_valid() || monster->is_dead()) continue;

                    if (monster->get_position().distance(player->get_position()) <= 1000.f) {
                        jungle_nearby = true;
                        break;
                    }
                }

                if (jungle_nearby) {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: Jungle clear detected");
                    handle_jungle_clear();
                }
                else {
                    if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: Lane clear detected");
                    handle_farming();
                }
            }
            else if (sdk::orbwalker->lasthit()) {
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_update: Last Hit mode active");
                handle_farming();
            }
        }

        // Auto W on CC - but only if the spell is leveled
        if (settings.w_auto_cc && settings.use_w && w_level > 0 && w_spell && w_spell->get_cooldown() <= 0.f) {
            if (player->has_buff_of_type(buff_type::stun) ||
                player->has_buff_of_type(buff_type::snare) ||
                player->has_buff_of_type(buff_type::charm) ||
                player->has_buff_of_type(buff_type::taunt) ||
                player->has_buff_of_type(buff_type::fear) ||
                player->has_buff_of_type(buff_type::polymorph)) {

                g_sdk->log_console("[Nocturne] on_update: Detecting CC on player, auto-casting W");
                cast_w(true);
            }
        }

        // Update infotab
        if (sdk::infotab && infotab_text_id != 0) {
            sdk::infotab->remove_text(infotab_text_id);
            infotab_text_id = 0;
        }

        if (sdk::infotab) {
            infotab_sdk::text_entry title = { "Nocturne", 0xFFFFFFFF };
            infotab_text_id = sdk::infotab->add_text(title, [&]() -> infotab_sdk::text_entry {
                infotab_sdk::text_entry current_entry;

                std::string status = "Passive: ";
                if (combat_state.passive_ready) {
                    status += "READY";
                    current_entry.color = 0xFF00FF00;
                }
                else {
                    status += "Not Ready";
                    current_entry.color = 0xFFFFFFFF;
                }

                if (combat_state.r_active) {
                    status += " | R: ACTIVE";
                }

                if (combat_state.e_target) {
                    status += " | E: Tethered";
                }

                // Show spell levels for debugging
                if (player) {
                    status += " | Levels:";
                    for (int i = 0; i < 4; i++) {
                        auto spell = player->get_spell(i);
                        if (spell) {
                            status += " " + std::to_string(i) + ":" + std::to_string(spell->get_level());
                        }
                    }
                }

                if (debug_enabled) {
                    status += " | DEBUG ON";
                }

                current_entry.text = status;
                return current_entry;
                });
        }
    }

    void __fastcall on_buff_gain(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        std::string buff_name = buff->get_name();

        if (source == player) {
            if (buff_name == "NocturneParanoia") {
                combat_state.r_active = true;
                combat_state.r_activation_time = g_sdk->clock_facade->get_game_time();
                combat_state.r_dashed = false;
                g_sdk->log_console("[Nocturne] on_buff_gain: R activated");
            }
            else if (buff_name == "NocturneUmbraBlades") {
                combat_state.passive_ready = true;
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_buff_gain: Passive ready");
            }
        }
    }

    void __fastcall on_buff_loss(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;

        std::string buff_name = buff->get_name();

        if (buff_name == "NocturneParanoia") {
            combat_state.r_active = false;
            g_sdk->log_console("[Nocturne] on_buff_loss: R deactivated");
        }
        else if (buff_name == "NocturneUmbraBlades") {
            combat_state.passive_ready = false;
            if (debug_enabled) g_sdk->log_console("[Nocturne] on_buff_loss: Passive used");
        }
    }

    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast) {
        if (!initialized || !sender || !cast) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        // If we're the one casting
        if (sender == player) {
            int spell_slot = cast->get_spell_slot();

            switch (spell_slot) {
            case 0: // Q
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_process_spell_cast: Cast Q");
                break;
            case 1: // W
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_process_spell_cast: Cast W");
                break;
            case 2: // E
                combat_state.e_target = cast->get_target();
                if (debug_enabled) g_sdk->log_console("[Nocturne] on_process_spell_cast: Cast E on target %p", combat_state.e_target);
                break;
            case 3: // R
                if (!combat_state.r_active) {
                    combat_state.r_active = true;
                    combat_state.r_activation_time = g_sdk->clock_facade->get_game_time();
                    combat_state.r_dashed = false;
                    g_sdk->log_console("[Nocturne] on_process_spell_cast: Cast R (activation)");
                }
                else {
                    combat_state.r_dashed = true;
                    g_sdk->log_console("[Nocturne] on_process_spell_cast: Cast R (dash)");
                }
                break;
            }
        }
        // If enemy is casting at us and we have W available
        else if (sender->get_team_id() != player->get_team_id() &&
            cast->get_target() == player &&
            settings.w_auto_cc &&
            settings.use_w &&
            can_cast_spell(1, 0.f)) {

            // Check if it's a dangerous spell (CC)
            bool dangerous_spell = false;
            auto spell_data = cast->get_spell_data();
            if (spell_data) {
                auto static_data = spell_data->get_static_data();
                if (static_data) {
                    auto tags = static_data->get_tags();
                    for (auto tag : tags) {
                        std::string tag_str(tag);
                        if (tag_str.find("Stun") != std::string::npos ||
                            tag_str.find("Root") != std::string::npos ||
                            tag_str.find("Taunt") != std::string::npos ||
                            tag_str.find("Fear") != std::string::npos ||
                            tag_str.find("Charm") != std::string::npos ||
                            tag_str.find("Suppression") != std::string::npos) {
                            dangerous_spell = true;
                            break;
                        }
                    }
                }
            }

            if (dangerous_spell) {
                g_sdk->log_console("[Nocturne] on_process_spell_cast: Detected CC spell targeting player, auto-casting W");
                cast_w(true);
            }
        }
    }

    bool force_cast_q(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        // Check if Q is leveled
        auto spell = player->get_spell(0);
        if (!spell || spell->get_level() == 0) {
            if (debug_enabled) g_sdk->log_console("[Nocturne] force_cast_q: Q not leveled yet");
            return false;
        }

        math::vector3 cast_position;

        if (target && target->is_valid() && target->is_targetable()) {
            cast_position = target->get_position();
        }
        else {
            // Use cursor position if no target
            cast_position = g_sdk->hud_manager->get_cursor_position();
        }

        try {
            g_sdk->log_console("[Nocturne] Force casting Q via OrderManager at position %.1f, %.1f, %.1f",
                cast_position.x, cast_position.y, cast_position.z);

            // Use OrderManager for consistent behavior
            if (order_manager.cast_spell(0, cast_position, vengaboys::OrderPriority::Critical)) {
                g_sdk->log_console("[Nocturne] Force cast Q via OrderManager successful");
                return true;
            }
            else {
                if (debug_enabled) g_sdk->log_console("[Nocturne] Force cast Q via OrderManager failed");
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in force_cast_q: %s", e.what());
        }
        return false;
    }

    void load() {
        try {
            g_sdk->log_console("[+] Loading Nocturne...");

            if (!sdk_init::orbwalker() || !sdk_init::target_selector() || !sdk_init::damage() || !sdk_init::infotab()) {
                g_sdk->log_console("[!] Failed to load dependencies!");
                return;
            }

            // Configure order manager with proper settings
            order_manager.set_min_order_delay(0.05f);   // Set to 50ms for stability
            order_manager.set_cooldown_after_ult(0.2f); // Slightly longer post-ult cooldown

            // Enable debug mode by default
            settings.debug_enabled = true;
            debug_enabled = true;

            g_sdk->log_console("[+] Dependencies loaded successfully.");

            if (sdk::orbwalker) {
                sdk::orbwalker->register_callback(orb_sdk::before_attack, reinterpret_cast<void*>(&before_attack));
                g_sdk->log_console("[+] Registered Orbwalker before_attack callback");
            }

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
                g_sdk->log_console("[+] Event callbacks registered successfully.");
            }

            // Initial settings
            settings.q_range = 1200.f;
            settings.e_range = 425.f;  // Confirmed range for E
            settings.r_range = 3250.f;
            settings.min_spell_delay = 0.05f;
            settings.draw_q_trail = true;
            settings.draw_e_tether = true;
            settings.draw_ranges = true;
            settings.draw_damage = true;
            settings.w_auto_cc = true;
            settings.w_low_health_threshold = 30.f;
            settings.r_use_lethal = true;
            settings.r_use_if_escaping = true;
            settings.r_recast_delay = 0.25f;

            // Print current settings to console
            g_sdk->log_console("[+] Nocturne settings initialized:");
            g_sdk->log_console("    - Q Range: %.1f", settings.q_range);
            g_sdk->log_console("    - E Range: %.1f", settings.e_range);
            g_sdk->log_console("    - R Range: %.1f", settings.r_range);

            // Test spell access and display levels
            auto player = g_sdk->object_manager->get_local_player();
            if (player) {
                for (int i = 0; i < 4; i++) {
                    auto spell = player->get_spell(i);
                    if (spell) {
                        g_sdk->log_console("    - Spell %d Level: %d, Cooldown: %.2f",
                            i, spell->get_level(), spell->get_cooldown());
                    }
                    else {
                        g_sdk->log_console("    - Spell %d: Not available", i);
                    }
                }
            }

            initialized = true;
            g_sdk->log_console("[+] Nocturne loaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in load: %s", e.what());
        }
    }

    void unload() {
        try {
            if (!initialized) return;

            g_sdk->log_console("[+] Unloading Nocturne...");

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
            g_sdk->log_console("[+] Nocturne unloaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in unload: %s", e.what());
        }
    }
}