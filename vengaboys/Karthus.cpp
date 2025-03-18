#include <Windows.h>
#include "sdk.hpp"
#include "karthus.hpp"
#include "ordermanager.hpp"
#include <limits>
#include <unordered_set>

namespace karthus {
    Settings settings;
    menu_category* menu = nullptr;
    bool initialized = false;
    bool debug_enabled = false;
    CombatState combat_state{};
    SpellTracker spell_tracker{};
    CachedTarget cached_target{};

    uint32_t infotab_text_id = 0;

    const float q_base_damage[6] = { 0.f, 45.f, 62.5f, 80.f, 97.5f, 115.f };
    const float q_ap_ratio = 0.35f;
    const float q_isolated_multiplier = 2.0f;

    const float e_base_damage[6] = { 0.f, 30.f, 45.f, 60.f, 75.f, 90.f };
    const float e_ap_ratio = 0.2f;

    const float r_base_damage[4] = { 0.f, 250.f, 400.f, 550.f };
    const float r_ap_ratio = 0.75f;

    void log_debug(const char* format, ...) {
        if (!debug_enabled) return;

        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        g_sdk->log_console("[Karthus Debug] %s", buffer);
    }

    bool can_cast_spell(int spell_slot, float delay) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            return false;
        }

        auto spell = player->get_spell(spell_slot);
        if (!spell) {
            return false;
        }

        if (spell->get_cooldown() > 0.01f) {
            return false;
        }

        float current_time = g_sdk->clock_facade->get_game_time();
        float last_cast_time = 0.f;

        switch (spell_slot) {
        case 0: last_cast_time = combat_state.last_q_time; break;
        case 1: last_cast_time = combat_state.last_w_time; break;
        case 2: last_cast_time = combat_state.last_e_time; break;
        case 3: last_cast_time = combat_state.last_r_time; break;
        }

        if (current_time - last_cast_time < 0.5f) {
            return false;
        }

        auto& order_manager = vengaboys::OrderManager::get_instance();
        if (order_manager.is_throttled() || order_manager.get_pending_orders() >= 2) {
            return false;
        }

        if (!order_manager.can_cast(player, spell_slot, delay)) {
            log_debug("can_cast_spell: OrderManager preventing cast (slot %d) - pending: %zu, throttled: %s",
                spell_slot,
                order_manager.get_pending_orders(),
                order_manager.is_throttled() ? "yes" : "no");
            return false;
        }

        return true;
    }

    float calculate_q_damage(game_object* target, bool is_isolated) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(0);
        if (!spell || spell->get_level() == 0) return 0.f;

        int spell_level = spell->get_level();
        float base_damage = q_base_damage[spell_level];
        float ratio = q_ap_ratio;

        if (is_isolated) {
            base_damage *= q_isolated_multiplier;
            ratio *= q_isolated_multiplier;
        }

        float total_damage = base_damage + (player->get_ability_power() * ratio);
        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float calculate_e_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(2);
        if (!spell || spell->get_level() == 0) return 0.f;

        int spell_level = spell->get_level();
        float base_damage = e_base_damage[spell_level];
        float total_damage = base_damage + (player->get_ability_power() * e_ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float calculate_r_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(3);
        if (!spell || spell->get_level() == 0) return 0.f;

        int spell_level = spell->get_level();
        float base_damage = r_base_damage[spell_level];
        float total_damage = base_damage + (player->get_ability_power() * r_ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float calculate_full_combo_damage(game_object* target) {
        if (!target) return 0.f;

        float total_damage = 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto q_spell = player->get_spell(0);
        if (q_spell && q_spell->get_level() > 0 && q_spell->get_cooldown() <= 0.f)
            total_damage += calculate_q_damage(target, true);

        auto e_spell = player->get_spell(2);
        if (e_spell && e_spell->get_level() > 0 && e_spell->get_cooldown() <= 0.f)
            total_damage += calculate_e_damage(target) * 2;

        auto r_spell = player->get_spell(3);
        if (r_spell && r_spell->get_level() > 0 && r_spell->get_cooldown() <= 0.f)
            total_damage += calculate_r_damage(target);

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

    int count_nearby_enemies(float range) {
        int count = 0;
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0;

        for (auto enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy->is_valid() || enemy->is_dead() || enemy->get_team_id() == player->get_team_id())
                continue;

            if (enemy->get_position().distance(player->get_position()) <= range) {
                count++;
            }
        }
        return count;
    }

    bool is_target_isolated(game_object* target) {
        if (!target) return false;

        for (auto other : g_sdk->object_manager->get_heroes()) {
            if (!other->is_valid() || other->is_dead() || other->get_team_id() == target->get_team_id() || other == target)
                continue;

            if (other->get_position().distance(target->get_position()) <= settings.q_radius * 2) {
                return false;
            }
        }

        for (auto minion : g_sdk->object_manager->get_minions()) {
            if (!minion->is_valid() || minion->is_dead() || minion->get_team_id() == target->get_team_id())
                continue;

            if (minion->get_position().distance(target->get_position()) <= settings.q_radius * 2) {
                return false;
            }
        }

        return true;
    }

    bool cast_q(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            return false;
        }

        auto spell = player->get_spell(0);
        if (!spell || spell->get_level() == 0 || spell->get_cooldown() > 0.f) {
            return false;
        }

        if (player->get_par() < settings.q_min_mana / 100.f * player->get_max_par()) {
            return false;
        }

        math::vector3 cast_position;
        if (target && target->is_valid() && target->is_targetable() && !target->is_ward() && !target->is_plant()) {
            pred_sdk::spell_data spell_data{};
            spell_data.spell_type = pred_sdk::spell_type::circular;
            spell_data.range = settings.q_range;
            spell_data.radius = settings.q_radius;
            spell_data.delay = settings.q_delay;
            spell_data.expected_hitchance = settings.q_min_hitchance;

            auto pred = sdk::prediction->predict(target, spell_data);
            if (pred.is_valid && pred.hitchance >= settings.q_min_hitchance) {
                cast_position = pred.cast_position;

                if (settings.q_multihit) {
                    bool potential_isolated_hit = is_target_isolated(target);

                    if (potential_isolated_hit) {
                        auto direction = (player->get_position() - pred.cast_position).normalized();

                        float edge_distance = settings.q_radius * 0.8f;     
                        math::vector3 edge_position = pred.cast_position + (direction * edge_distance);

                        if (edge_position.distance(player->get_position()) <= settings.q_range) {
                            cast_position = edge_position;
                            log_debug("Edge casting Q for isolated hit");
                        }
                    }
                }
            }
            else {
                return false;
            }
        }
        else {
            cast_position = g_sdk->hud_manager->get_cursor_position();
        }

        auto& order_manager = vengaboys::OrderManager::get_instance();

        if (order_manager.is_issue_order_passed(0, 0.25f)) {
            if (order_manager.cast_spell(0, cast_position)) {
                combat_state.last_q_time = g_sdk->clock_facade->get_game_time();
                log_debug("Cast Q at position %.1f, %.1f, %.1f",
                    cast_position.x, cast_position.y, cast_position.z);
                return true;
            }
        }

        return false;
    }

    bool cast_w(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            return false;
        }

        auto spell = player->get_spell(1);
        if (!spell || spell->get_level() == 0 || spell->get_cooldown() > 0.f) {
            return false;
        }

        if (player->get_par() < settings.w_min_mana / 100.f * player->get_max_par()) {
            return false;
        }

        if (!target || !target->is_valid() || !target->is_targetable()) {
            return false;
        }

        float distance = target->get_position().distance(player->get_position());
        if (distance < settings.w_min_distance) {
            return false;
        }

        pred_sdk::spell_data spell_data{};
        spell_data.spell_type = pred_sdk::spell_type::circular;
        spell_data.range = settings.w_range;
        spell_data.radius = settings.w_radius;
        spell_data.delay = settings.w_delay;
        spell_data.expected_hitchance = settings.w_min_hitchance;

        auto pred = sdk::prediction->predict(target, spell_data);
        if (!pred.is_valid || pred.hitchance < settings.w_min_hitchance) {
            return false;
        }

        auto& order_manager = vengaboys::OrderManager::get_instance();

        if (order_manager.is_issue_order_passed(1, 0.5f)) {
            if (order_manager.cast_spell(1, pred.cast_position)) {
                combat_state.last_w_time = g_sdk->clock_facade->get_game_time();
                log_debug("W cast successful at position %.1f, %.1f, %.1f",
                    pred.cast_position.x, pred.cast_position.y, pred.cast_position.z);
                return true;
            }
        }

        return false;
    }

    bool toggle_e(bool activate) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        auto spell = player->get_spell(2);
        if (!spell || spell->get_level() == 0 || spell->get_cooldown() > 0.f) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - combat_state.last_e_time < 0.5f) return false;

        if (combat_state.e_active == activate) return false;

        if (vengaboys::OrderManager::get_instance().is_issue_order_passed(2, 0.5f)) {
            player->cast_spell(2, player->get_position());
            combat_state.last_e_time = current_time;
            combat_state.e_active = activate;
            log_debug("E toggled %s successfully", activate ? "ON" : "OFF");
            return true;
        }

        return false;
    }

    bool cast_r() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        auto spell = player->get_spell(3);
        if (!spell || spell->get_level() == 0 || spell->get_cooldown() > 0.f) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - combat_state.last_r_time < 0.5f) return false;

        if (combat_state.r_potential_kills < settings.r_min_kills) return false;

        if (vengaboys::OrderManager::get_instance().is_issue_order_passed(3, 0.5f)) {
            player->cast_spell(3, player->get_position());
            combat_state.last_r_time = current_time;
            log_debug("R cast successful (potential kills: %d)", combat_state.r_potential_kills);
            return true;
        }

        return false;
    }

    void update_r_indicator() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto spell = player->get_spell(3);
        if (!spell || spell->get_level() == 0 || spell->get_cooldown() > 0.f) {
            combat_state.r_potential_kills = 0;
            return;
        }

        int kill_count = 0;
        combat_state.r_notified_targets.clear();

        for (auto enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy->is_valid() || enemy->is_dead() || enemy->get_team_id() == player->get_team_id())
                continue;

            float r_damage = calculate_r_damage(enemy);
            float predicted_health = enemy->get_hp();

            predicted_health += enemy->get_hp_regen_rate() * 3.0f;
            predicted_health += enemy->get_all_shield() + enemy->get_magical_shield();

            if (r_damage > predicted_health) {
                kill_count++;

                if (settings.r_notification) {
                    send_r_notification(enemy);
                }
            }
        }

        combat_state.r_potential_kills = kill_count;
    }

    void send_r_notification(game_object* target) {
        if (!target || !settings.r_notification) return;

        if (combat_state.r_notified_targets.find(target->get_network_id()) ==
            combat_state.r_notified_targets.end()) {

            std::string message = "R can kill " + target->get_char_name();
            if (sdk::notification) {
                sdk::notification->add("Karthus R", message, color(0xFF00FF00));     
                combat_state.r_notified_targets.insert(target->get_network_id());
            }
        }
    }

    void handle_combo() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            return;
        }

        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - combat_state.last_combo_time < settings.min_spell_delay) {
            return;
        }

        auto& order_manager = vengaboys::OrderManager::get_instance();
        if (order_manager.get_pending_orders() > 5) {
            order_manager.clear_queue();
            log_debug("Cleared OrderManager queue");
        }

        if (order_manager.is_throttled() && rand() % 10 == 0) {
            order_manager.reset_throttle();
            log_debug("Reset OrderManager throttle state");
            return;
        }

        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero) return false;
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;
            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                hero->is_visible();
            });

        cached_target.target = target;

        if (!cached_target.target) {
            return;
        }

        bool spell_cast = false;

        if (combat_state.passive_active) {
            if (settings.use_q && !spell_cast) {
                spell_cast = cast_q(target);
            }

            if (settings.use_e && !combat_state.e_active && !spell_cast) {
                spell_cast = toggle_e(true);
            }

            if (spell_cast) {
                combat_state.last_combo_time = current_time;
            }
            return;
        }

        if (settings.use_r && settings.r_use_lethal && !spell_cast) {
            update_r_indicator();
            if (combat_state.r_potential_kills >= settings.r_min_kills) {
                spell_cast = cast_r();
            }
        }

        if (settings.use_w && !spell_cast) {
            spell_cast = cast_w(target);
        }

        if (settings.use_q && !spell_cast) {
            spell_cast = cast_q(target);
        }

        if (settings.use_e && !spell_cast) {
            int nearby_enemies = count_nearby_enemies(settings.e_range);
            float mana_percent = player->get_par() / player->get_max_par() * 100.f;

            bool should_activate = nearby_enemies >= settings.e_min_enemies;

            if (mana_percent < settings.e_min_mana && settings.e_disable_under_min_mana) {
                should_activate = false;
            }

            if (combat_state.e_active != should_activate) {
                spell_cast = toggle_e(should_activate);
            }
        }

        if (spell_cast) {
            combat_state.last_combo_time = current_time;
        }
    }

    void handle_harass() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.harass_min_mana / 100.f * player->get_max_par()) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - combat_state.last_harass_time < settings.min_spell_delay * 3) {
            return;      
        }

        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero) return false;
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;
            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
                hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.q_range;
            });

        if (!target) {
            return;
        }

        bool spell_cast = false;

        if (settings.harass_use_q && !spell_cast) {
            if (!settings.harass_under_turret || !is_under_turret(target->get_position())) {
                spell_cast = cast_q(target);
            }
        }

        if (!spell_cast && settings.harass_use_w) {
            if (!settings.harass_under_turret || !is_under_turret(target->get_position())) {
                spell_cast = cast_w(target);
            }
        }

        if (!spell_cast && settings.harass_use_e) {
            int nearby_enemies = count_nearby_enemies(settings.e_range);

            if (nearby_enemies > 0 && !combat_state.e_active) {
                spell_cast = toggle_e(true);
            }
            else if (nearby_enemies == 0 && combat_state.e_active) {
                spell_cast = toggle_e(false);
            }
        }

        if (spell_cast) {
            combat_state.last_harass_time = current_time;
        }
    }

    void handle_farming() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.farm_min_mana / 100.f * player->get_max_par()) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        if (!settings.farm_use_q) {
            return;
        }

        if (current_time - combat_state.last_farm_q_time < 0.5f) {
            return;     
        }

        bool spell_farm_enabled = sdk::orbwalker->is_spell_farm_enabled();
        bool spell_farm_lasthit_enabled = sdk::orbwalker->is_spell_farm_lasthit_enabled();

        if (!spell_farm_enabled && !spell_farm_lasthit_enabled) {
            if (combat_state.e_active) {
                toggle_e(false);
            }
            return;
        }

        if (can_cast_spell(0, 0.f)) {
            std::vector<game_object*> valid_minions;

            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead())
                    continue;

                if (minion->get_team_id() == player->get_team_id() || minion->is_ward() || minion->is_plant())
                    continue;

                if (minion->get_position().distance(player->get_position()) <= settings.q_range) {
                    valid_minions.push_back(minion);
                }
            }

            if (!valid_minions.empty()) {
                std::vector<game_object*> last_hittable_minions;

                for (auto minion : valid_minions) {
                    float q_damage = calculate_q_damage(minion, is_target_isolated(minion));
                    float predicted_health = minion->get_hp();

                    if (q_damage >= predicted_health) {
                        last_hittable_minions.push_back(minion);
                    }
                }

                if (!last_hittable_minions.empty() && (spell_farm_lasthit_enabled || spell_farm_enabled)) {
                    math::vector3 best_position;
                    int max_hits = 0;

                    if (last_hittable_minions.size() > 1) {
                        for (auto& center_minion : last_hittable_minions) {
                            int hits = 0;
                            for (auto& minion : last_hittable_minions) {
                                if (minion->get_position().distance(center_minion->get_position()) <= settings.q_radius) {
                                    hits++;
                                }
                            }

                            if (hits > max_hits) {
                                max_hits = hits;
                                best_position = center_minion->get_position();
                            }
                        }
                    }
                    else {
                        best_position = last_hittable_minions[0]->get_position();
                        max_hits = 1;
                    }

                    if (max_hits == 1 && settings.q_multihit) {
                        auto minion_pos = best_position;

                        bool can_be_isolated = true;
                        for (auto& other_minion : valid_minions) {
                            if (other_minion->get_position() == minion_pos) continue;     

                            if (other_minion->get_position().distance(minion_pos) <= settings.q_radius * 2) {
                                auto direction = (other_minion->get_position() - minion_pos).normalized();

                                math::vector3 edge_pos = minion_pos - (direction * (settings.q_radius * 0.7f));

                                if (edge_pos.distance(player->get_position()) <= settings.q_range) {
                                    best_position = edge_pos;
                                    log_debug("Edge casting Q for isolated farm last hit");
                                }

                                can_be_isolated = false;
                                break;
                            }
                        }
                    }

                    if (best_position.distance(player->get_position()) > 50.f) {
                        log_debug("Q farm: Targeting position at %.1f distance with %d minions",
                            best_position.distance(player->get_position()), max_hits);

                        if (vengaboys::OrderManager::get_instance().cast_spell(0, best_position)) {
                            combat_state.last_farm_q_time = current_time;
                            log_debug("Q farm: Last hit cast successful, hitting %d minions", max_hits);
                        }
                    }
                }
                else if (spell_farm_enabled && valid_minions.size() >= settings.farm_min_minions_q) {
                    math::vector3 best_position;
                    int max_hits = 0;

                    for (auto& center_minion : valid_minions) {
                        int hits = 0;
                        for (auto& minion : valid_minions) {
                            if (minion->get_position().distance(center_minion->get_position()) <= settings.q_radius) {
                                hits++;
                            }
                        }

                        if (hits > max_hits) {
                            max_hits = hits;
                            best_position = center_minion->get_position();
                        }
                    }

                    if (max_hits >= settings.farm_min_minions_q &&
                        best_position.distance(player->get_position()) > 50.f) {

                        log_debug("Q farm: Wave clear targeting position at %.1f distance with %d minions",
                            best_position.distance(player->get_position()), max_hits);

                        if (vengaboys::OrderManager::get_instance().cast_spell(0, best_position)) {
                            combat_state.last_farm_q_time = current_time;
                            log_debug("Q farm: Wave clear cast successful, hitting %d minions", max_hits);
                        }
                    }
                }
            }
        }

        if (settings.use_e) {
            int nearby_enemy_minions = 0;

            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead())
                    continue;

                if (minion->get_team_id() == player->get_team_id() || minion->is_ward() || minion->is_plant())
                    continue;

                if (minion->get_position().distance(player->get_position()) <= settings.e_range) {
                    nearby_enemy_minions++;
                }
            }

            float mana_percent = player->get_par() / player->get_max_par() * 100.f;
            bool should_activate = nearby_enemy_minions >= settings.e_min_enemies && mana_percent >= settings.e_min_mana;

            if (combat_state.e_active != should_activate) {
                toggle_e(should_activate);
            }
        }
    }

    void urf_mode_logic() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        auto& order_manager = vengaboys::OrderManager::get_instance();
        if (order_manager.get_pending_orders() > 2) {
            order_manager.clear_queue();
            log_debug("Cleared OrderManager queue (URF mode)");
        }

        if (order_manager.is_throttled()) {
            order_manager.reset_throttle();
            log_debug("Reset OrderManager throttle state (URF mode)");
            return;        
        }

        update_r_indicator();

        if (settings.use_r && combat_state.r_potential_kills >= 1 && can_cast_spell(3, 0.f)) {
            if (cast_r()) {
                return;       
            }
        }

        game_object* target = sdk::target_selector->get_hero_target();

        if (settings.use_q && can_cast_spell(0, 0.f) && current_time - combat_state.last_q_time >= 0.2f) {
            if (target) {
                cast_q(target);
            }
            else {
                math::vector3 cursor_pos = g_sdk->hud_manager->get_cursor_position();
                vengaboys::OrderManager::get_instance().cast_spell(0, cursor_pos);
                combat_state.last_q_time = current_time;
            }
        }

        if (settings.use_w && can_cast_spell(1, 0.f) && current_time - combat_state.last_w_time >= 0.5f) {
            if (target) {
                cast_w(target);
            }
        }

        if (settings.use_e && can_cast_spell(2, 0.f) && !combat_state.e_active &&
            current_time - combat_state.last_e_time >= 0.5f) {
            toggle_e(true);
        }
    }

    void __fastcall on_update() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) {
            return;
        }

        float current_time = g_sdk->clock_facade->get_game_time();


        auto& order_manager = vengaboys::OrderManager::get_instance();
        if (order_manager.get_pending_orders() >= 3 || order_manager.is_throttled()) {
            order_manager.clear_queue();
            order_manager.reset_throttle();
            log_debug("OrderManager reset - queue cleared and throttle reset");
            return;         
        }
        if (combat_state.passive_active) {
            if (current_time - combat_state.passive_start_time > 7.0f) {
                combat_state.passive_active = false;
                g_sdk->log_console("[Karthus] Passive has ended");
            }
        }

        static float last_r_update_time = 0.f;
        if (current_time - last_r_update_time > 1.0f && player->get_spell(3) &&
            player->get_spell(3)->get_level() > 0 && player->get_spell(3)->get_cooldown() <= 0.f) {
            update_r_indicator();
            last_r_update_time = current_time;
        }

        if (settings.urf_mode) {
            urf_mode_logic();
            return;
        }

        if (sdk::orbwalker) {
            if (sdk::orbwalker->combo()) {
                handle_combo();
            }
            else if (sdk::orbwalker->harass()) {
                handle_harass();
            }
            else if (sdk::orbwalker->clear() || sdk::orbwalker->lasthit()) {
                handle_farming();
            }
            else {
                if (combat_state.e_active && can_cast_spell(2, 0.f) &&
                    current_time - combat_state.last_e_time >= 0.5f) {
                    toggle_e(false);
                }

                auto& order_manager = vengaboys::OrderManager::get_instance();
                if (order_manager.get_pending_orders() > 0 && rand() % 5 == 0) {
                    order_manager.clear_queue();
                    log_debug("Idle - cleared OrderManager queue");
                }
            }
        }

        if (sdk::infotab && infotab_text_id != 0) {
            sdk::infotab->remove_text(infotab_text_id);
            infotab_text_id = 0;
        }

        if (sdk::infotab) {
            infotab_sdk::text_entry title = { "Karthus", 0xFFFFFFFF };
            infotab_text_id = sdk::infotab->add_text(title, [&]() -> infotab_sdk::text_entry {
                infotab_sdk::text_entry current_entry;

                std::string status = "R kills: ";
                status += std::to_string(combat_state.r_potential_kills);

                if (combat_state.passive_active) {
                    status += " | PASSIVE";
                    current_entry.color = 0xFFFF0000;
                }
                else if (combat_state.e_active) {
                    status += " | E: ON";
                    current_entry.color = 0xFF00FFFF;
                }
                else {
                    current_entry.color = 0xFFFFFFFF;
                }

                if (settings.urf_mode) {
                    status += " | URF MODE";
                }

                if (settings.debug_enabled) {
                    status += " | DEBUG";
                }

                if (settings.debug_enabled) {
                    auto& order_manager = vengaboys::OrderManager::get_instance();
                    status += " | Queue: " + std::to_string(order_manager.get_pending_orders());
                    if (order_manager.is_throttled()) {
                        status += " (T)";
                    }
                }

                current_entry.text = status;
                return current_entry;
                });
        }
    }

    void __fastcall on_draw() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !settings.draw_ranges) return;

        if (settings.draw_q_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.q_range, 2.f, settings.q_range_color);
        }

        if (settings.draw_w_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.w_range, 2.f, settings.w_range_color);
        }

        if (settings.draw_e_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.e_range, 2.f, settings.e_range_color);
        }

        if (settings.draw_r_indicator) {
            math::vector2 screen_pos = g_sdk->renderer->world_to_screen(player->get_position());
            screen_pos.y += 20.f;

            std::string text = "R Kills: " + std::to_string(combat_state.r_potential_kills);
            uint32_t text_color = combat_state.r_potential_kills > 0 ? 0xFF00FF00 : 0xFFFFFFFF;

            g_sdk->renderer->add_text(text, 16.f, screen_pos, 1, text_color);
        }

        if (settings.draw_damage) {
            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy->is_valid() || enemy->is_dead() || enemy->get_team_id() == player->get_team_id())
                    continue;

                float r_damage = 0.f;
                if (player->get_spell(3) && player->get_spell(3)->get_level() > 0 &&
                    player->get_spell(3)->get_cooldown() <= 0.f) {
                    r_damage = calculate_r_damage(enemy);
                }

                if (r_damage > 0.f) {
                    g_sdk->renderer->add_damage_indicator(enemy, r_damage);
                }
            }
        }

        if (settings.urf_mode) {
            math::vector2 screen_pos = g_sdk->renderer->world_to_screen(player->get_position());
            screen_pos.y -= 20.f;

            g_sdk->renderer->add_text("URF MODE", 16.f, screen_pos, 1, 0xFFFF00FF);
        }

        if (settings.debug_enabled) {
            auto& order_manager = vengaboys::OrderManager::get_instance();
            math::vector2 debug_pos = g_sdk->renderer->world_to_screen(player->get_position());
            debug_pos.y -= 40.f;

            std::string debug_text = "Queue: " + std::to_string(order_manager.get_pending_orders());
            if (order_manager.is_throttled()) {
                debug_text += " (THROTTLED)";
            }

            g_sdk->renderer->add_text(debug_text, 14.f, debug_pos, 1, 0xFFFFFF00);
        }
    }

    bool __fastcall before_attack(orb_sdk::event_data* data) {
        if (!initialized) return true;

        if (combat_state.passive_active) {
            return false;
        }

        auto player = g_sdk->object_manager->get_local_player();
        if (player && settings.disable_aa_after_level && player->get_level() >= settings.disable_aa_level) {
            if (sdk::orbwalker && (sdk::orbwalker->combo() || sdk::orbwalker->harass())) {
                log_debug("Auto attack prevented (Level: %d >= %d)",
                    player->get_level(), settings.disable_aa_level);
                return false;
            }
        }

        return true;
    }


    void __fastcall on_buff_gain(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;

        std::string buff_name = buff->get_name();

        if (buff_name == "KarthusDeathDefiedBuff") {
            combat_state.passive_active = true;
            combat_state.passive_start_time = g_sdk->clock_facade->get_game_time();
            g_sdk->log_console("[Karthus] Passive activated");

            vengaboys::OrderManager::get_instance().clear_queue();
            vengaboys::OrderManager::get_instance().reset_throttle();
        }

        if (buff_name == "KarthusDefile") {
            combat_state.e_active = true;
            g_sdk->log_console("[Karthus] E activated");
        }
    }

    void __fastcall on_buff_loss(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;

        std::string buff_name = buff->get_name();

        if (buff_name == "KarthusDefile") {
            combat_state.e_active = false;
            g_sdk->log_console("[Karthus] E deactivated");
        }
    }

    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast) {
        if (!initialized || !sender || !cast) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || sender != player) return;

        int spell_slot = cast->get_spell_slot();
        float current_time = g_sdk->clock_facade->get_game_time();

        switch (spell_slot) {
        case 0:  
            combat_state.last_q_time = current_time;
            log_debug("Spell cast: Q");
            break;
        case 1:  
            combat_state.last_w_time = current_time;
            log_debug("Spell cast: W");
            break;
        case 2:  
            combat_state.last_e_time = current_time;
            log_debug("Spell cast: E toggle");
            break;
        case 3:  
            combat_state.last_r_time = current_time;
            log_debug("Spell cast: R");
            break;
        }
    }

    void create_menu() {
        menu = g_sdk->menu_manager->add_category("karthus", "Karthus");
        if (!menu) return;

        auto combat = menu->add_sub_category("combat", "Combat Settings");
        if (combat) {
            combat->add_checkbox("use_q", "Q - Lay Waste", settings.use_q, [&](bool value) { settings.use_q = value; });
            combat->add_checkbox("use_w", "W - Wall of Pain", settings.use_w, [&](bool value) { settings.use_w = value; });
            combat->add_checkbox("use_e", "E - Defile", settings.use_e, [&](bool value) { settings.use_e = value; });
            combat->add_checkbox("use_r", "R - Requiem", settings.use_r, [&](bool value) { settings.use_r = value; });
            combat->add_checkbox("disable_aa_after_level", "Disable AA After Level",
                settings.disable_aa_after_level,
                [&](bool value) { settings.disable_aa_after_level = value; });

            combat->add_slider_int("disable_aa_level", "Disable AA Level", 1, 18, 1,
                settings.disable_aa_level,
                [&](int value) { settings.disable_aa_level = value; });


            auto q_settings = combat->add_sub_category("q_settings", "Q - Lay Waste Settings");
            if (q_settings) {
                q_settings->add_slider_float("q_range", "Range", 500.f, 1000.f, 25.f, settings.q_range,
                    [&](float value) { settings.q_range = value; });
                q_settings->add_slider_float("q_radius", "Radius", 100.f, 200.f, 10.f, settings.q_radius,
                    [&](float value) { settings.q_radius = value; });
                q_settings->add_checkbox("q_multihit", "Try to Hit Multiple Targets", settings.q_multihit,
                    [&](bool value) { settings.q_multihit = value; });
                q_settings->add_slider_int("q_min_hitchance", "Minimum Hitchance", 0, 100, 5, settings.q_min_hitchance,
                    [&](int value) { settings.q_min_hitchance = value; });
                q_settings->add_slider_float("q_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.q_min_mana,
                    [&](float value) { settings.q_min_mana = value; });
            }

            auto w_settings = combat->add_sub_category("w_settings", "W - Wall of Pain Settings");
            if (w_settings) {
                w_settings->add_slider_float("w_range", "Range", 500.f, 1000.f, 25.f, settings.w_range,
                    [&](float value) { settings.w_range = value; });
                w_settings->add_slider_float("w_min_distance", "Minimum Cast Distance", 0.f, 500.f, 25.f, settings.w_min_distance,
                    [&](float value) { settings.w_min_distance = value; });
                w_settings->add_slider_int("w_min_hitchance", "Minimum Hitchance", 0, 100, 5, settings.w_min_hitchance,
                    [&](int value) { settings.w_min_hitchance = value; });
                w_settings->add_slider_float("w_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.w_min_mana,
                    [&](float value) { settings.w_min_mana = value; });
            }

            auto e_settings = combat->add_sub_category("e_settings", "E - Defile Settings");
            if (e_settings) {
                e_settings->add_slider_float("e_range", "Range", 300.f, 600.f, 25.f, settings.e_range,
                    [&](float value) { settings.e_range = value; });
                e_settings->add_slider_int("e_min_enemies", "Minimum Enemies", 1, 5, 1, settings.e_min_enemies,
                    [&](int value) { settings.e_min_enemies = value; });
                e_settings->add_slider_float("e_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.e_min_mana,
                    [&](float value) { settings.e_min_mana = value; });
                e_settings->add_checkbox("e_disable_under_min_mana", "Disable When Below Min Mana", settings.e_disable_under_min_mana,
                    [&](bool value) { settings.e_disable_under_min_mana = value; });
            }

            auto r_settings = combat->add_sub_category("r_settings", "R - Requiem Settings");
            if (r_settings) {
                r_settings->add_checkbox("r_use_lethal", "Use When Lethal", settings.r_use_lethal,
                    [&](bool value) { settings.r_use_lethal = value; });
                r_settings->add_slider_int("r_min_kills", "Minimum Kills to Cast", 1, 5, 1, settings.r_min_kills,
                    [&](int value) { settings.r_min_kills = value; });
                r_settings->add_checkbox("r_notification", "Kill Notifications", settings.r_notification,
                    [&](bool value) { settings.r_notification = value; });
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
            harass->add_slider_float("harass_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.harass_min_mana,
                [&](float value) { settings.harass_min_mana = value; });
            harass->add_checkbox("harass_under_turret", "Harass Under Turret", settings.harass_under_turret,
                [&](bool value) { settings.harass_under_turret = value; });
        }

        auto farm = menu->add_sub_category("farm", "Farm Settings");
        if (farm) {
            farm->add_checkbox("farm_use_q", "Use Q", settings.farm_use_q,
                [&](bool value) { settings.farm_use_q = value; });
            farm->add_slider_float("farm_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.farm_min_mana,
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
            drawings->add_checkbox("draw_q_range", "Draw Q Range", settings.draw_q_range,
                [&](bool value) { settings.draw_q_range = value; });
            drawings->add_checkbox("draw_w_range", "Draw W Range", settings.draw_w_range,
                [&](bool value) { settings.draw_w_range = value; });
            drawings->add_checkbox("draw_e_range", "Draw E Range", settings.draw_e_range,
                [&](bool value) { settings.draw_e_range = value; });
            drawings->add_checkbox("draw_r_indicator", "Draw R Kill Indicator", settings.draw_r_indicator,
                [&](bool value) { settings.draw_r_indicator = value; });
            drawings->add_colorpicker("q_range_color", "Q Range Color", settings.q_range_color,
                [&](uint32_t value) { settings.q_range_color = value; });
            drawings->add_colorpicker("w_range_color", "W Range Color", settings.w_range_color,
                [&](uint32_t value) { settings.w_range_color = value; });
            drawings->add_colorpicker("e_range_color", "E Range Color", settings.e_range_color,
                [&](uint32_t value) { settings.e_range_color = value; });
        }

        auto misc = menu->add_sub_category("misc", "Misc Settings");
        if (misc) {
            misc->add_checkbox("urf_mode", "URF Mode (Spam All Spells)", settings.urf_mode,
                [&](bool value) {
                    settings.urf_mode = value;
                    g_sdk->log_console("[Karthus] URF mode %s", value ? "ENABLED" : "DISABLED");

                    vengaboys::OrderManager::get_instance().clear_queue();
                    vengaboys::OrderManager::get_instance().reset_throttle();
                });

            misc->add_slider_float("min_spell_delay", "Minimum Spell Delay", 0.05f, 0.5f, 0.05f, settings.min_spell_delay,
                [&](float value) {
                    settings.min_spell_delay = value;
                    g_sdk->log_console("[Karthus] Minimum spell delay set to %.2f", value);
                });

            misc->add_checkbox("debug_mode", "Debug Mode", debug_enabled,
                [&](bool value) {
                    debug_enabled = value;
                    settings.debug_enabled = value;
                    g_sdk->log_console("[Karthus] Debug mode %s", value ? "ENABLED" : "DISABLED");
                });
        }
    }

    void load() {
        try {
            g_sdk->log_console("[+] Loading Karthus...");

            if (!sdk_init::orbwalker() || !sdk_init::target_selector() || !sdk_init::damage() || !sdk_init::infotab() || !sdk_init::prediction()) {
                g_sdk->log_console("[!] Failed to load dependencies!");
                return;
            }

            auto& order_manager = vengaboys::OrderManager::get_instance();
            order_manager.set_min_order_delay(0.05f);      
            order_manager.set_max_queue_size(4);          
            order_manager.set_max_orders_per_second(20);     

            order_manager.clear_queue();
            order_manager.reset_throttle();

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

            settings.q_range = 875.f;
            settings.q_radius = 160.f;
            settings.q_delay = 0.75f;
            settings.w_range = 1000.f;
            settings.w_radius = 250.f;
            settings.w_delay = 0.25f;
            settings.e_range = 550.f;
            settings.min_spell_delay = 0.2f;    

            g_sdk->log_console("[+] Karthus settings initialized:");
            g_sdk->log_console("    - Q Range: %.1f", settings.q_range);
            g_sdk->log_console("    - W Range: %.1f", settings.w_range);
            g_sdk->log_console("    - E Range: %.1f", settings.e_range);

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
            g_sdk->log_console("[+] Karthus loaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in load: %s", e.what());
        }
    }

    void unload() {
        try {
            if (!initialized) return;

            g_sdk->log_console("[+] Unloading Karthus...");

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

            vengaboys::OrderManager::get_instance().clear_queue();

            initialized = false;
            g_sdk->log_console("[+] Karthus unloaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in unload: %s", e.what());
        }
    }
}