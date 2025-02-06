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
#include "jax.hpp"
#include "ordermanager.hpp"
#include <limits>

namespace vengaboys {
    Settings settings;
    menu_category* menu = nullptr;
    bool initialized = false;
    bool debug_enabled = false;
    CombatState combat_state{};
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

        g_sdk->log_console("[Jax Debug] %s", buffer);
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

        if (!OrderManager::get_instance().can_cast(player, spell_slot, delay)) {
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

        const float base_damage[] = { 65.f, 105.f, 145.f, 185.f, 225.f };
        const float bonus_ad_ratio = 1.0f;        
        const float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
        const float total_damage = base_damage[spell->get_level() - 1] + (bonus_ad * bonus_ad_ratio);   

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, total_damage);
    }
    float calculate_w_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(1);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 50.f, 85.f, 120.f, 155.f, 190.f };
        const float ap_ratio = 0.6f;
        const float total_damage = base_damage[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float calculate_e_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(2);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 40.f, 70.f, 100.f, 130.f, 160.f };       
        const float ap_ratio = 0.7f;        
        const float max_health_ratio = 0.035f;         

        float total_damage = base_damage[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio);

        if (target->is_hero() || target->is_monster()) {          
            total_damage += target->get_max_hp() * max_health_ratio;
        }


        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);      
    }

    float calculate_full_combo_damage(game_object* target) {
        if (!target) return 0.f;

        float total_damage = 0.f;

        if (can_cast_spell(0, 0.f))
            total_damage += calculate_q_damage(target);
        if (can_cast_spell(1, 0.f))
            total_damage += calculate_w_damage(target);
        if (can_cast_spell(2, 0.f) && !combat_state.e_active)             
            total_damage += calculate_e_damage(target);

        auto player = g_sdk->object_manager->get_local_player();
        if (player)
            total_damage += sdk::damage->get_aa_damage(player, target);

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

    bool cast_q(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target || target->is_plant() || target->is_ward()) return false;

        if (!can_cast_spell(0, 0.f)) {     
            return false;
        }

        if (target->get_position().distance(player->get_position()) > settings.q_range) {
            log_debug("cast_q: Target out of range");
            return false;
        }

        if (!target->is_targetable()) {
            log_debug("cast_q: Target not targetable");
            return false;
        }

        try {
            log_debug("Casting Q on target %s", target->get_char_name().c_str());
            if (OrderManager::get_instance().cast_spell(0, target)) {
                spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                combat_state.last_q_time = g_sdk->clock_facade->get_game_time();
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_q: %s", e.what());
        }
        return false;
    }
    bool cast_w(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || target->is_plant() || target->is_ward()) return false;

        if (!can_cast_spell(1, 0.f)) {     
            return false;
        }

        if (!target->is_valid() || !target->is_targetable())
            return false;

        float game_time = g_sdk->clock_facade->get_game_time();
        if (game_time - combat_state.last_w_farm_cast_time < 0.2f)
        {
            log_debug("cast_w: W Farm on cooldown");
            return false;
        }

        try {
            log_debug("Casting W");
            if (OrderManager::get_instance().cast_spell(1, target)) {
                spell_tracker.update_cooldown(1, g_sdk->clock_facade->get_game_time() + player->get_spell(1)->get_cooldown());
                combat_state.last_w_time = g_sdk->clock_facade->get_game_time();
                combat_state.last_w_farm_cast_time = game_time;
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_w: %s", e.what());
        }
        return false;
    }

    bool cast_e(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || target->is_plant() || target->is_ward()) return false;

        float current_time = g_sdk->clock_facade->get_game_time();

        if (!can_cast_spell(2, 0.f)) {
            return false;
        }

        if (combat_state.e_active) {
            if (current_time - combat_state.e_start_time < settings.min_e_recast_delay) {
                log_debug("cast_e: Too soon to recast E, waiting %.2f seconds",
                    settings.min_e_recast_delay - (current_time - combat_state.e_start_time));
                return false;
            }
        }

        try {
            log_debug("Casting E");
            if (OrderManager::get_instance().cast_spell(2, target)) {
                spell_tracker.update_cooldown(2, current_time + player->get_spell(2)->get_cooldown());
                combat_state.e_active = !combat_state.e_active;

                if (combat_state.e_active) {
                    combat_state.e_start_time = current_time;
                }

                combat_state.last_e_time = current_time;
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_e: %s", e.what());
        }
        return false;
    }

    bool cast_r() {
        if (!can_cast_spell(3, 0.f)) return false;     

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        try {
            if (OrderManager::get_instance().cast_spell(3, player)) {
                spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + player->get_spell(3)->get_cooldown());
                combat_state.last_r_time = g_sdk->clock_facade->get_game_time();
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_r: %s", e.what());
        }
        return false;
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

        if (!spell_cast && settings.harass_use_q) {
            if (!settings.harass_under_turret || !is_under_turret(target->get_position())) {
                spell_cast = cast_q(target);
            }
        }

        if (!spell_cast && settings.harass_use_w && settings.w_auto_reset &&
            sdk::orbwalker->is_attack_ready_in(0.1f)) {
            if (sdk::orbwalker->is_in_auto_attack_range(player, target)) {
                spell_cast = cast_w(player);
            }
        }

        if (!spell_cast && settings.harass_use_e && !combat_state.e_active) {
            int nearby_enemies = 0;
            for (auto hero : g_sdk->object_manager->get_heroes()) {
                if (!hero->is_valid() || hero->is_dead() ||
                    hero->get_team_id() == player->get_team_id()) {
                    continue;
                }

                if (hero->get_position().distance(player->get_position()) <= settings.e_range) {
                    nearby_enemies++;
                }
            }

            if (nearby_enemies >= settings.min_e_targets) {
                spell_cast = cast_e(player);
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

        if (settings.farm_use_w && settings.w_auto_reset && sdk::orbwalker->is_attack_ready_in(0.2f)) {
            if (current_time - combat_state.last_w_farm_cast_time >= 0.2f) {
                cast_w(player);
                log_debug("handle_farming: Casting W for lane/jungle clear (always cast)");
            }
            else {
                log_debug("handle_farming: W Farm on cooldown - skipping cast");
            }
        }

        if (settings.farm_use_q && spell_farm_lasthit_enabled) {
            if (current_time - combat_state.last_farm_q_cast_time >= 0.2f) {
                for (auto object : g_sdk->object_manager->get_minions()) {
                    if (!object->is_valid() || object->is_dead() ||
                        object->get_team_id() == player->get_team_id())
                        continue;

                    if (!settings.farm_under_turret && is_under_turret(object->get_position()))
                        continue;

                    if ((object->is_lane_minion() || object->is_monster())) {
                        if (calculate_q_damage(object) >= object->get_hp()) {
                            log_debug("handle_farming: Trying to last hit %s with Q", object->get_char_name().c_str());
                            if (cast_q(object)) {
                                combat_state.last_farm_q_cast_time = current_time;
                                log_debug("handle_farming: Q casted successfully for last hit");
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (settings.farm_use_e && spell_farm_enabled &&
            !combat_state.e_active &&
            player->get_par() > settings.farm_min_mana / 100.f * player->get_max_par() * 1.5f) {

            int nearby_targets = 0;
            for (auto object : g_sdk->object_manager->get_minions()) {
                if (!object->is_valid() || object->is_dead() ||
                    object->get_team_id() == player->get_team_id())
                    continue;

                if ((object->is_lane_minion() || object->is_monster()) &&
                    object->get_position().distance(player->get_position()) <= settings.e_range)
                    nearby_targets++;
            }

            if (nearby_targets >= settings.farm_min_minions) {
                log_debug("handle_farming: Casting E for AoE farm, nearby targets: %d", nearby_targets);
                cast_e(player);
            }
        }
    }

    void handle_fast_clear() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.farm_min_mana / 100.f * player->get_max_par()) return;

        if (!sdk::orbwalker->is_spell_farm_enabled()) {
            return;
        }

        float current_time = g_sdk->clock_facade->get_game_time();

        if (settings.farm_use_q) {
            if (current_time - combat_state.last_farm_q_cast_time >= 0.1f) {
                game_object* target = sdk::target_selector->get_monster_target([&](game_object* monster) {
                    if (!monster) return false;
                    return (monster->is_lane_minion() || monster->is_monster()) &&
                        monster->is_valid() && !monster->is_dead() &&
                        monster->get_team_id() != player->get_team_id() && monster->is_targetable() &&
                        monster->is_visible() && monster->get_position().distance(player->get_position()) <= settings.q_range;
                    });
                if (target) {
                    log_debug("handle_fast_clear: Casting Q for clear on target: %s", target->get_char_name().c_str());
                    if (cast_q(target)) {
                        combat_state.last_farm_q_cast_time = current_time;
                    }
                }
            }
        }

        if (settings.farm_use_w && settings.w_auto_reset && sdk::orbwalker->is_attack_ready_in(0.2f)) {
            if (current_time - combat_state.last_w_farm_cast_time >= 0.2f) {
                cast_w(player);
                log_debug("handle_fast_clear: Casting W for clear (always cast)");
            }
            else {
                log_debug("handle_fast_clear: W Farm on cooldown - skipping cast");
            }
        }

        if (settings.farm_use_e && !combat_state.e_active &&
            player->get_par() > settings.farm_min_mana / 100.f * player->get_max_par() / 2.f) {

            int nearby_targets = 0;
            for (auto object : g_sdk->object_manager->get_minions()) {
                if (!object->is_valid() || object->is_dead() ||
                    object->get_team_id() == player->get_team_id())
                    continue;

                if ((object->is_lane_minion() || object->is_monster()) &&
                    object->get_position().distance(player->get_position()) <= settings.e_range)
                    nearby_targets++;
            }
            if (nearby_targets >= 1) {
                log_debug("handle_fast_clear: Casting E for AoE fast clear, nearby targets: %d", nearby_targets);
                cast_e(player);
            }
        }
    }
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

        log_debug("handle_combo: Player is valid, Current HP: %.2f", player->get_hp());

        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero) return false;
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;
            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.q_range;
            });

        cached_target.target = target;
        if (!cached_target.target) return;

        bool spell_cast = false;         

        if (!spell_cast && settings.use_e && !combat_state.e_active &&
            player->get_par() >= settings.e_min_mana / 100.f * player->get_max_par()) {

            if (cached_target.target->get_position().distance(player->get_position()) <= settings.e_range &&
                cached_target.target->is_targetable()) {

                float combo_damage_with_e = calculate_full_combo_damage(cached_target.target) + calculate_e_damage(cached_target.target);
                if (combo_damage_with_e >= cached_target.target->get_hp()) {
                    log_debug("handle_combo: Casting E for kill secure on target: %s, Combo Damage: %.2f, Target HP: %.2f",
                        cached_target.target->get_char_name().c_str(), combo_damage_with_e, cached_target.target->get_hp());
                    spell_cast = cast_e(player);
                }
            }
        }

        if (!spell_cast && settings.use_r && settings.r_use_defensive) {
            float hp_ratio = player->get_hp() / player->get_max_hp();

            if (hp_ratio <= settings.r_defensive_hp_threshold / 100.f) {
                int nearby_enemies = 0;
                for (auto hero : sdk::target_selector->get_sorted_heroes()) {
                    if (hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                        hero->is_visible() && hero->get_team_id() != player->get_team_id() &&
                        hero->get_position().distance(player->get_position()) <= settings.r_range)
                        nearby_enemies++;
                }
                if (nearby_enemies >= settings.r_min_enemies_defensive) {
                    log_debug("handle_combo: Casting R defensively - HP Ratio: %.2f, Nearby Enemies: %d",
                        hp_ratio, nearby_enemies);
                    spell_cast = cast_r();
                }
            }
        }

        if (!spell_cast && settings.use_e && !combat_state.e_active &&
            player->get_par() >= settings.e_min_mana / 100.f * player->get_max_par()) {

            bool should_cast_e = false;

            if (cached_target.target && sdk::orbwalker->is_in_auto_attack_range(player, cached_target.target)) {
                float incoming_aa_damage = sdk::damage->get_aa_damage(cached_target.target, player);
                float lethal_threshold_damage = player->get_hp() * (settings.e_lethal_aa_hp_threshold / 100.f);

                if (incoming_aa_damage >= lethal_threshold_damage) {
                    log_debug("handle_combo: Casting E to block lethal auto-attack");
                    should_cast_e = true;
                }
            }

            if (!should_cast_e) {
                int nearby_enemies = 0;
                for (auto hero : sdk::target_selector->get_sorted_heroes()) {
                    if (hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                        hero->is_visible() && hero->get_team_id() != player->get_team_id() &&
                        hero->get_position().distance(player->get_position()) <= settings.e_range) {
                        nearby_enemies++;
                    }
                }

                bool is_1v1 = nearby_enemies <= 1;
                bool can_cast_e_1v1 = is_1v1 && nearby_enemies == 1;

                if (nearby_enemies >= settings.min_e_targets || can_cast_e_1v1) {
                    log_debug("handle_combo: Casting E for AoE, nearby enemies: %d", nearby_enemies);
                    should_cast_e = true;
                }
            }

            if (should_cast_e) {
                spell_cast = cast_e(player);
            }
        }

        if (!spell_cast && settings.use_q &&
            player->get_par() >= settings.q_min_mana / 100.f * player->get_max_par()) {

            float target_distance = player->get_position().distance(cached_target.target->get_position());
            bool should_cast_q = false;

            if (!sdk::orbwalker->is_in_auto_attack_range(player, cached_target.target) &&
                target_distance <= settings.q_range) {
                log_debug("handle_combo: Casting Q for gap close");
                should_cast_q = true;
            }
            else if (is_under_turret(cached_target.target->get_position()) &&
                (calculate_q_damage(cached_target.target) + calculate_w_damage(cached_target.target) >=
                    cached_target.target->get_hp())) {
                log_debug("handle_combo: Casting Q under turret for kill");
                should_cast_q = true;
            }
            else if (!is_under_turret(cached_target.target->get_position())) {
                float hp_ratio = cached_target.target->get_hp() / cached_target.target->get_max_hp();
                if (!settings.q_prioritize_low_hp || hp_ratio <= settings.q_hp_threshold) {
                    log_debug("handle_combo: Casting Q for damage");
                    should_cast_q = true;
                }
            }

            if (should_cast_q) {
                spell_cast = cast_q(cached_target.target);
            }
        }

        if (!spell_cast && settings.use_w &&
            player->get_par() >= settings.w_min_mana / 100.f * player->get_max_par() &&
            cached_target.target && settings.w_auto_reset &&
            sdk::orbwalker->is_attack_ready_in(0.1f)) {

            spell_cast = cast_w(player);
        }

        if (spell_cast) {
            combat_state.last_combo_time = current_time;
        }
    }

    void ward_jump() {
        if (!settings.ward_jump_enabled) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast_spell(0, 0.f)) return;     

        auto cursor_pos = g_sdk->hud_manager->get_cursor_position();

        game_object* best_target = nullptr;
        float best_distance = FLT_MAX;

        if (settings.ward_jump_wards) {
            for (auto ward : g_sdk->object_manager->get_wards()) {
                if (!ward->is_valid() || ward->is_dead() || !ward->is_targetable() ||
                    ward->get_team_id() != player->get_team_id())
                    continue;

                float dist_to_cursor = ward->get_position().distance(cursor_pos);
                if (dist_to_cursor < best_distance &&
                    player->get_position().distance(ward->get_position()) <= settings.q_range) {
                    best_distance = dist_to_cursor;
                    best_target = ward;
                }
            }
        }

        if (settings.ward_jump_minions) {
            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead() || !minion->is_targetable() ||
                    minion->get_team_id() != player->get_team_id())
                    continue;

                float dist_to_cursor = minion->get_position().distance(cursor_pos);
                if (dist_to_cursor < best_distance &&
                    player->get_position().distance(minion->get_position()) <= settings.q_range) {
                    best_distance = dist_to_cursor;
                    best_target = minion;
                }
            }
        }

        if (settings.ward_jump_allies) {
            for (auto hero : g_sdk->object_manager->get_heroes()) {
                if (!hero->is_valid() || hero->is_dead() || !hero->is_targetable() ||
                    hero->get_team_id() != player->get_team_id() || hero == player)
                    continue;

                float dist_to_cursor = hero->get_position().distance(cursor_pos);
                if (dist_to_cursor < best_distance &&
                    player->get_position().distance(hero->get_position()) <= settings.q_range) {
                    best_distance = dist_to_cursor;
                    best_target = hero;
                }
            }
        }

        if (best_target) {
            cast_q(best_target);
            return;
        }

        int ward_slot = -1;
        for (int i = 6; i <= 7; i++) {
            if (player->get_item_id(i) == 3340 ||
                player->get_item_id(i) == 2055) {
                ward_slot = i;
                break;
            }
        }

        if (ward_slot != -1) {
            math::vector3 ward_pos = cursor_pos;
            float dist_to_cursor = player->get_position().distance(cursor_pos);

            if (dist_to_cursor > settings.ward_jump_range) {
                auto direction = (cursor_pos - player->get_position()).normalized();
                ward_pos = player->get_position() + direction * settings.ward_jump_range;
            }

            if (g_sdk->nav_mesh->is_pathable(ward_pos)) {
                OrderManager::get_instance().cast_spell(ward_slot, ward_pos);
            }
        }
    }
    void create_menu() {
        menu = g_sdk->menu_manager->add_category("jax", "Jax");
        if (!menu) return;

        auto combat = menu->add_sub_category("combat", "Combat Settings");
        if (combat) {
            combat->add_checkbox("use_q", "Use Q", settings.use_q, [&](bool value) { settings.use_q = value; });
            combat->add_checkbox("use_w", "Use W", settings.use_w, [&](bool value) { settings.use_w = value; });
            combat->add_checkbox("use_e", "Use E", settings.use_e, [&](bool value) { settings.use_e = value; });
            combat->add_checkbox("use_r", "Use R", settings.use_r, [&](bool value) { settings.use_r = value; });

            auto q_settings = combat->add_sub_category("q_settings", "Q Settings");
            if (q_settings) {
                q_settings->add_checkbox("q_prio_low_hp", "Prioritize Low HP", settings.q_prioritize_low_hp,
                    [&](bool value) { settings.q_prioritize_low_hp = value; });
                q_settings->add_slider_float("q_hp_threshold", "Low HP Threshold", 0.f, 1.f, 0.01f, settings.q_hp_threshold,
                    [&](float value) { settings.q_hp_threshold = value; });
                q_settings->add_slider_float("q_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.q_min_mana,
                    [&](float value) { settings.q_min_mana = value; });
                q_settings->add_checkbox("q_under_turret", "Use Under Turret", settings.q_under_turret,
                    [&](bool value) { settings.q_under_turret = value; });
            }

            auto w_settings = combat->add_sub_category("w_settings", "W Settings");
            if (w_settings) {
                w_settings->add_checkbox("w_auto_reset", "Auto Reset Attack", settings.w_auto_reset,
                    [&](bool value) { settings.w_auto_reset = value; });
                w_settings->add_slider_float("w_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.w_min_mana,
                    [&](float value) { settings.w_min_mana = value; });
            }

            auto e_settings = combat->add_sub_category("e_settings", "E Settings");
            if (e_settings) {
                e_settings->add_slider_float("e_recast_delay", "Recast Delay", 0.5f, 2.5f, 0.1f, settings.e_recast_time,
                    [&](float value) { settings.e_recast_time = value; });
                e_settings->add_slider_float("e_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.e_min_mana,
                    [&](float value) { settings.e_min_mana = value; });
                e_settings->add_slider_float("e_lethal_aa_hp_threshold", "Lethal AA % Threshold", 5.f, 50.f, 1.f, settings.e_lethal_aa_hp_threshold,
                    [&](float value) { settings.e_lethal_aa_hp_threshold = value; });
            }

            auto r_settings = combat->add_sub_category("r_settings", "R Settings");
            if (r_settings) {
                r_settings->add_checkbox("r_use_defensive", "Use Defensively", settings.r_use_defensive,
                    [&](bool value) { settings.r_use_defensive = value; });
                r_settings->add_slider_float("r_defensive_hp_threshold", "Defensive HP %", 0.f, 100.f, 1.f, settings.r_defensive_hp_threshold,
                    [&](float value) { settings.r_defensive_hp_threshold = value; });
                r_settings->add_slider_int("r_min_enemies_defensive", "Min Enemies", 0, 5, 1, settings.r_min_enemies_defensive,
                    [&](int value) { settings.r_min_enemies_defensive = value; });
                r_settings->add_slider_float("r_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.r_min_mana,
                    [&](float value) { settings.r_min_mana = value; });
                r_settings->add_label("Note: R casts if HP < Defensive HP % AND Nearby Enemies > Min Enemies");
            }
        }

        auto harass = menu->add_sub_category("harass", "Harass Settings");
        if (harass) {
            harass->add_checkbox("harass_use_q", "Use Q", settings.harass_use_q,
                [&](bool value) { settings.harass_use_q = value; });
            harass->add_checkbox("harass_use_w", "Use W", settings.harass_use_w,
                [&](bool value) { settings.harass_use_w = value; });
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
            farm->add_checkbox("farm_use_w", "Use W", settings.farm_use_w,
                [&](bool value) { settings.farm_use_w = value; });
            farm->add_checkbox("farm_use_e", "Use E", settings.farm_use_e,
                [&](bool value) { settings.farm_use_e = value; });
            farm->add_slider_float("farm_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.farm_min_mana,
                [&](float value) { settings.farm_min_mana = value; });
            farm->add_slider_int("farm_min_minions", "Min Minions for E", 1, 6, 1, settings.farm_min_minions,
                [&](int value) { settings.farm_min_minions = value; });
            farm->add_checkbox("farm_under_turret", "Farm Under Turret", settings.farm_under_turret,
                [&](bool value) { settings.farm_under_turret = value; });
        }

        auto ward_jump = menu->add_sub_category("ward_jump", "Ward Jump Settings");
        if (ward_jump) {
            ward_jump->add_checkbox("ward_jump_enabled", "Enable Ward Jump", settings.ward_jump_enabled,
                [&](bool value) { settings.ward_jump_enabled = value; });
            ward_jump->add_slider_float("ward_jump_range", "Range", 400.f, 700.f, 25.f, settings.ward_jump_range,
                [&](float value) { settings.ward_jump_range = value; });
            ward_jump->add_checkbox("ward_jump_wards", "Jump to Wards", settings.ward_jump_wards,
                [&](bool value) { settings.ward_jump_wards = value; });
            ward_jump->add_checkbox("ward_jump_minions", "Jump to Minions", settings.ward_jump_minions,
                [&](bool value) { settings.ward_jump_minions = value; });
            ward_jump->add_checkbox("ward_jump_allies", "Jump to Allies", settings.ward_jump_allies,
                [&](bool value) { settings.ward_jump_allies = value; });
            ward_jump->add_checkbox("draw_ward_spots", "Draw Ward Jump Range", settings.draw_ward_spots,
                [&](bool value) { settings.draw_ward_spots = value; });
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
    }

    void debug_buff_info(game_object* player) {
        if (!player) return;
        g_sdk->log_console("---- Buff Debug ----");
        auto buffs = player->get_buffs();
        for (auto&& buff : buffs) {
            std::string name = buff->get_name();
            uint32_t hash = buff->get_hash();
            g_sdk->log_console("Found buff: %s (0x%08X)", name.c_str(), hash);
        }
    }

    bool __fastcall before_attack(orb_sdk::event_data* data) {
        if (!initialized) return true;
        log_debug("before_attack: called");
        if (!data || !data->target) {
            log_debug("before_attack: data or target is null");
            return true;
        }
        log_debug("before_attack: is valid = %s",
            (!combat_state.e_active || (data->target && data->target->is_plant()) ||
                (data->target && data->target->is_ward())) ? "yes" : "no");

        return !combat_state.e_active || (data->target && data->target->is_plant()) ||
            (data->target && data->target->is_ward());
    }

    void __fastcall on_draw() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.draw_spell_ranges) {
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.q_range, 2.f, 0xFFFFFFFF);
            if (combat_state.e_active)
                g_sdk->renderer->add_circle_3d(player->get_position(), settings.e_range, 2.f, 0xFFFFFFFF);
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.r_range, 2.f, 0xFFFFFFFF);
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

        if (settings.draw_ward_spots && sdk::orbwalker->flee()) {
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.ward_jump_range, 2.f, 0x4000FF00);
        }
    }
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
            else if (sdk::orbwalker->fast_clear()) {
                log_debug("on_update: Fast Clear mode active");
                handle_fast_clear();
            }
            else if (sdk::orbwalker->flee()) {
                log_debug("on_update: Flee mode active");
                ward_jump();
            }
            else if (sdk::orbwalker->lasthit()) {
                log_debug("on_update: LastHit mode active");
                handle_farming();
            }
        }

        if (sdk::infotab && infotab_text_id != 0) {
            sdk::infotab->remove_text(infotab_text_id);
            infotab_text_id = 0;
        }

        if (sdk::infotab) {
            infotab_sdk::text_entry title = { "Jax", 0xFFFFFFFF };
            infotab_text_id = sdk::infotab->add_text(title, [&]() -> infotab_sdk::text_entry {
                infotab_sdk::text_entry current_entry;
                current_entry.color = 0xFFFFFFFF;
                std::string mode_str = "Mode: ";
                if (sdk::orbwalker->combo()) mode_str += "Combo";
                else if (sdk::orbwalker->harass()) mode_str += "Harass";
                else if (sdk::orbwalker->clear()) mode_str += "Clear";
                else if (sdk::orbwalker->fast_clear()) mode_str += "Fast Clear";
                else if (sdk::orbwalker->flee()) mode_str += "Flee";
                else if (sdk::orbwalker->lasthit()) mode_str += "Last Hit";
                else mode_str += "None";
                current_entry.text = mode_str;
                return current_entry;
                });
        }

        if (combat_state.e_active && !can_cast_spell(2, 0.f)) {     
            combat_state.e_active = false;
            log_debug("on_update: E state updated");
        }

        log_debug("on_update: Update completed");
    }

    void __fastcall on_buff_gain(game_object* source, buff_instance* buff) {
        if (!initialized) return;
    }

    void __fastcall on_buff_loss(game_object* source, buff_instance* buff) {    
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;
        if (!initialized) return;

        if (buff->get_name() == "JaxCounterStrike") {
            combat_state.e_active = false;
        }
    }

    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast) {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !sender || sender->get_team_id() == player->get_team_id()) return;

        if (cast->is_basic_attack() && settings.use_e && can_cast_spell(2, 0.f)) {     
            float incoming_aa_damage = sdk::damage->get_aa_damage(sender, player);
            float lethal_threshold_damage = player->get_hp() * (settings.e_lethal_aa_hp_threshold / 100.f);

            if (incoming_aa_damage >= lethal_threshold_damage) {
                log_debug("on_process_spell_cast: Casting E to block incoming lethal auto attack");
                cast_e(player);
            }
        }
    }

    void load() {
        try {
            g_sdk->log_console("[+] Loading Jax...");

            if (!sdk_init::orbwalker() || !sdk_init::target_selector() || !sdk_init::damage() || !sdk_init::infotab()) {
                g_sdk->log_console("[!] Failed to load dependencies!");
                return;
            }

            auto& order_manager = OrderManager::get_instance();
            order_manager.set_min_order_delay(settings.min_spell_delay);
            order_manager.set_max_queue_size(3);

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

            initialized = true;
            g_sdk->log_console("[+] Jax loaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in load: %s", e.what());
        }
    }

    void unload() {
        try {
            if (!initialized) return;

            g_sdk->log_console("[+] Unloading Jax...");

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
            g_sdk->log_console("[+] Jax unloaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in unload: %s", e.what());
        }
    }
}   