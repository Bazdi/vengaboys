#include "Velkoz.hpp"
#include "ordermanager.hpp"
#include <cmath>
#include <algorithm>

namespace velkoz {

    Velkoz velkoz_instance;
    Settings settings;

    void Velkoz::log_debug(const char* format, ...) {
        if (!settings.draw_debug) return;

        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        g_sdk->log_console("[Velkoz Debug] %s", buffer);
    }

    bool Velkoz::can_cast_spell(int spell_slot, float delay) {
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

        if (spell->get_level() <= 0) {
            log_debug("can_cast_spell: Spell %d not leveled", spell_slot);
            return false;
        }

        if (spell->get_cooldown() > 0.f) {
            log_debug("can_cast_spell: Spell on cooldown: %.2fs", spell->get_cooldown());
            return false;
        }

        if (!vengaboys::OrderManager::get_instance().can_cast(player, spell_slot, delay)) {
            log_debug("can_cast_spell: OrderManager preventing cast");
            return false;
        }

        return true;
    }

    float Velkoz::calculate_passive_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        const float base_damages[] = { 35.f, 35.f, 35.f, 55.f, 55.f, 55.f, 80.f, 80.f, 105.f, 105.f, 130.f, 130.f, 155.f, 155.f, 180.f, 180.f, 180.f, 180.f };
        float base_damage = base_damages[std::min(17, player->get_level())];
        float bonus_ap = player->get_ability_power();
        float damage = base_damage + (bonus_ap * 0.60f);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::truedamage, player, target, damage);
    }

    float Velkoz::calculate_q_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(0);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 80.f, 120.f, 160.f, 200.f, 240.f };
        const float ap_ratio = 0.90f;
        float damage = base_damage[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, damage);
    }

    float Velkoz::calculate_w_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(1);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 30.f, 50.f, 70.f, 90.f, 110.f };
        const float ap_ratio = 0.20f;
        float damage_instance = base_damage[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio);
        float total_damage = damage_instance * 2.0f;

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
    }

    float Velkoz::calculate_e_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(2);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage[] = { 70.f, 100.f, 130.f, 160.f, 190.f };
        const float ap_ratio = 0.30f;
        float damage = base_damage[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, damage);
    }

    float Velkoz::calculate_r_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(3);
        if (!spell || spell->get_level() == 0) return 0.f;

        const float base_damage_per_tick[] = { 34.62f, 48.08f, 61.54f };
        const float ap_ratio_per_tick = 0.0962f;
        float damage_per_tick = base_damage_per_tick[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio_per_tick);

        float total_damage = damage_per_tick * 20.0f;

        bool has_three_stacks = false;
        if (target->is_hero()) {
            for (auto buff : target->get_buffs()) {
                if (buff->get_name() == "velkozresearchstack" && buff->get_count() == 3) {
                    has_three_stacks = true;
                    break;
                }
            }
        }

        if (has_three_stacks) {
            return sdk::damage->calc_damage(dmg_sdk::damage_type::truedamage, player, target, total_damage);
        }
        else {
            return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
        }
    }

    float Velkoz::calculate_full_combo_damage(game_object* target) {
        if (!target) return 0.f;

        float total_damage = 0.f;

        total_damage += calculate_passive_damage(target);

        if (can_cast_spell(0, 0.f)) {
            total_damage += calculate_q_damage(target);
        }

        if (can_cast_spell(1, 0.f)) {
            total_damage += calculate_w_damage(target);
        }

        if (can_cast_spell(2, 0.f)) {
            total_damage += calculate_e_damage(target);
        }

        if (can_cast_spell(3, 0.f)) {
            total_damage += calculate_r_damage(target);
        }

        return total_damage;
    }

    bool Velkoz::collision_check(const math::vector3& start, const math::vector3& end) {
        pred_sdk::spell_data collision_data{};
        collision_data.spell_type = pred_sdk::spell_type::linear;
        collision_data.range = start.distance(end);
        collision_data.radius = 60.f;
        collision_data.forbidden_collisions = {
            pred_sdk::collision_type::hero,
            pred_sdk::collision_type::unit,
            pred_sdk::collision_type::yasuo_wall,
            pred_sdk::collision_type::braum_wall
        };

        auto collision_result = sdk::prediction->collides(end, collision_data, nullptr);
        return collision_result.collided;
    }

    bool Velkoz::cast_q(game_object* target) {
        if (!target || !can_cast_spell(0, 0.25f)) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        q_reactivate_available = false;
        q_reactivation_pending = false;

        const auto player_pos = player->get_position();
        const auto target_pos = target->get_position();
        float direct_distance = player_pos.distance(target_pos);

        if (direct_distance <= 1050.f) {         
            pred_sdk::spell_data q_data{};
            q_data.spell_type = pred_sdk::spell_type::linear;
            q_data.targetting_type = pred_sdk::targetting_type::center;
            q_data.range = 1100.f;
            q_data.radius = 60.f;
            q_data.delay = 0.25f;
            q_data.projectile_speed = 1300.f;
            q_data.forbidden_collisions = {
                pred_sdk::collision_type::hero,
                pred_sdk::collision_type::unit,
                pred_sdk::collision_type::yasuo_wall,
                pred_sdk::collision_type::braum_wall
            };

            const auto pred = sdk::prediction->predict(target, q_data);
            if (pred.is_valid && pred.hitchance >= pred_sdk::hitchance::high) {
                log_debug("Direct Q cast to target");
                if (vengaboys::OrderManager::get_instance().cast_spell(0, pred.cast_position)) {
                    spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                    q_cast_time = g_sdk->clock_facade->get_game_time();
                    return true;
                }
            }
        }

        if (settings.use_q_split) {
            return cast_q_split(target);
        }

        return false;
    }

    bool Velkoz::cast_q_split(game_object* target) {
        if (!target) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        const auto player_pos = player->get_position();
        const auto target_pos = target->get_position();

        const float total_potential_range = 1595.f;
        if (target_pos.distance(player_pos) > total_potential_range) {
            return false;
        }

        const auto dir_to_target = (target_pos - player_pos).normalized();
        const float PI = 3.14159265358979323846f;

        std::vector<float> split_angles;
        for (float angle = -75.f; angle <= 75.f; angle += 15.f) {
            split_angles.push_back(angle * (PI / 180.f));
        }

        std::vector<float> test_distances = {
            800.f, 850.f, 900.f, 950.f, 1000.f, 1050.f, 750.f, 700.f, 650.f, 600.f, 550.f
        };

        struct SplitOption {
            math::vector3 cast_pos;
            math::vector3 split_origin;
            float target_dist_from_split;
            float split_score;
        };

        std::vector<SplitOption> valid_splits;

        for (float angle : split_angles) {
            math::vector3 rotated_dir = dir_to_target.rotate(angle);

            for (float dist : test_distances) {
                math::vector3 cast_pos = player_pos + (rotated_dir * dist);

                if (!g_sdk->nav_mesh->is_pathable(cast_pos)) {
                    float min_dist = std::max(dist - 100.f, 0.f);
                    float max_dist = dist;
                    math::vector3 wall_pos = cast_pos;

                    for (int i = 0; i < 5; i++) {       
                        float mid_dist = (min_dist + max_dist) / 2.f;
                        math::vector3 test_pos = player_pos + (rotated_dir * mid_dist);

                        if (g_sdk->nav_mesh->is_pathable(test_pos)) {
                            min_dist = mid_dist;
                        }
                        else {
                            max_dist = mid_dist;
                            wall_pos = test_pos;
                        }
                    }

                    math::vector3 split_origin = player_pos + (rotated_dir * (std::max(min_dist - 55.f, 0.f)));

                    float target_dist_from_split = target_pos.distance(split_origin);

                    if (target_dist_from_split <= 850.f) {      
                        if (g_sdk->nav_mesh->is_pathable(split_origin) &&
                            !collision_check(split_origin, target_pos)) {

                            float angle_factor = 1.0f + std::abs(angle / PI);    
                            float dist_factor = target_dist_from_split / 850.f;    
                            float score = angle_factor * dist_factor;

                            valid_splits.push_back({ cast_pos, split_origin, target_dist_from_split, score });
                        }
                    }
                }
            }
        }

        if (!valid_splits.empty()) {
            std::sort(valid_splits.begin(), valid_splits.end(),
                [](const SplitOption& a, const SplitOption& b) {
                    return a.split_score < b.split_score;
                });

            auto best_split = valid_splits[0];

            log_debug("Q split cast: target dist from split %.1f, score %.2f",
                best_split.target_dist_from_split, best_split.split_score);

            if (vengaboys::OrderManager::get_instance().cast_spell(0, best_split.cast_pos)) {
                spell_tracker.update_cooldown(0, g_sdk->clock_facade->get_game_time() + player->get_spell(0)->get_cooldown());
                q_cast_time = g_sdk->clock_facade->get_game_time();
                return true;
            }
        }

        return false;
    }

    void Velkoz::track_q_missile() {
        if (!q_reactivate_available) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (!q_missile_object || !q_missile_object->is_valid()) {
            q_missile_object = nullptr;

            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || !minion->is_missile()) continue;
                if (minion->get_owner() != player) continue;

                auto spell_cast = minion->get_missile_spell_cast();
                if (!spell_cast) continue;

                auto spell_data = spell_cast->get_spell_data();
                if (!spell_data) continue;

                auto static_data = spell_data->get_static_data();
                if (!static_data) continue;

                std::string name = static_data->get_name();
                if (name.find("VelkozQ") != std::string::npos) {
                    q_missile_object = minion;

                    if (!q_missile_tracked) {
                        q_missile_start_pos = minion->get_missile_start_pos();
                        math::vector3 missile_end = minion->get_missile_end_pos();
                        q_missile_direction = (missile_end - q_missile_start_pos).normalized();
                        q_missile_tracked = true;

                        log_debug("Q missile found and tracked: start=%.1f,%.1f,%.1f end=%.1f,%.1f,%.1f",
                            q_missile_start_pos.x, q_missile_start_pos.y, q_missile_start_pos.z,
                            missile_end.x, missile_end.y, missile_end.z);
                    }
                    break;
                }
            }
        }

        if (q_missile_object && q_missile_object->is_valid()) {
            q_missile_current_pos = q_missile_object->get_position();
        }
        else {
            float current_time = g_sdk->clock_facade->get_game_time();
            float time_since_cast = current_time - q_cast_time;

            float estimated_distance = time_since_cast * 1300.f;

            if (estimated_distance > 1100.f) {
                estimated_distance = 1100.f;
            }

            q_missile_current_pos = q_missile_start_pos + (q_missile_direction * estimated_distance);
        }
    }

    bool Velkoz::is_point_near_line(const math::vector3& point, const math::vector3& line_start,
        const math::vector3& line_end, float max_distance) {
        math::vector3 line_dir = (line_end - line_start).normalized();
        math::vector3 point_dir = point - line_start;

        float projection = point_dir.dot(line_dir);

        if (projection <= 0.0f) {
            return point.distance(line_start) <= max_distance;
        }

        float line_length = line_start.distance(line_end);
        if (projection >= line_length) {
            return point.distance(line_end) <= max_distance;
        }

        math::vector3 closest_point = line_start + (line_dir * projection);

        return point.distance(closest_point) <= max_distance;
    }

    std::vector<game_object*> Velkoz::get_enemies_near_q_path(float range) {
        std::vector<game_object*> enemies;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return enemies;

        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!is_valid_target(hero, 0)) continue;

            if (hero->get_position().distance(q_missile_current_pos) <= range) {
                enemies.push_back(hero);
            }
        }

        for (auto minion : g_sdk->object_manager->get_minions()) {
            if (!is_valid_target(minion, 0) || minion->is_plant()) continue;

            if (minion->get_position().distance(q_missile_current_pos) <= range) {
                enemies.push_back(minion);
            }
        }

        for (auto monster : g_sdk->object_manager->get_monsters()) {
            if (!is_valid_target(monster, 0)) continue;

            if (monster->get_position().distance(q_missile_current_pos) <= range) {
                enemies.push_back(monster);
            }
        }

        return enemies;
    }

    bool Velkoz::is_any_target_in_q_split_path() {
        if (!q_reactivate_available) return false;

        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!is_valid_target(hero, 0)) continue;

            if (would_q_split_hit_target(hero)) {
                log_debug("Q split would hit champion: %s", hero->get_name().c_str());
                return true;
            }
        }

        auto potential_targets = get_enemies_near_q_path(850.f);
        for (auto target : potential_targets) {
            if (target->is_hero()) continue;    

            if (would_q_split_hit_target(target)) {
                log_debug("Q split would hit %s", target->is_hero() ? "champion" : "minion/monster");
                return true;
            }
        }

        return false;
    }

    bool Velkoz::would_q_split_hit_target(game_object* target) {
        if (!target || !q_reactivate_available) return false;

        float distance = q_missile_current_pos.distance(target->get_position());

        if (distance > 950.0f) {
            return false;
        }

        if (target->is_hero()) {
            log_debug("Champion %s potential Q split target at distance: %.1f",
                target->get_name().c_str(), distance);
            return true;
        }

        math::vector3 missile_to_target = (target->get_position() - q_missile_current_pos).normalized();
        float dot = std::abs(q_missile_direction.dot(missile_to_target));

        return (dot < 0.7f);
    }

    void Velkoz::handle_q_reactivation() {
        if (!q_reactivate_available || (!settings.auto_reactivate_q && !settings.auto_q_hotkey_pressed))
            return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        track_q_missile();

        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!is_valid_target(hero)) continue;

            float distance = q_missile_current_pos.distance(hero->get_position());
            if (distance <= 900.0f) {
                log_debug("FORCE Q ACTIVATION: Champion nearby (%.1f units)", distance);
                player->cast_spell(0);      

                q_reactivate_available = false;
                q_reactivation_pending = false;
                q_missile_tracked = false;
                q_missile_object = nullptr;
                return;
            }
        }

        float current_time = g_sdk->clock_facade->get_game_time();
        float time_since_cast = current_time - q_cast_time;
        bool q_expiring = time_since_cast > 2.5f;    

        if (q_expiring) {
            log_debug("FORCE Q ACTIVATION: About to expire");
            player->cast_spell(0);

            q_reactivate_available = false;
            q_reactivation_pending = false;
            q_missile_tracked = false;
            q_missile_object = nullptr;
        }
    }

    bool Velkoz::cast_w(game_object* target) {
        if (!target || !can_cast_spell(1, 0.25f)) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        pred_sdk::spell_data w_data{};
        w_data.spell_type = pred_sdk::spell_type::linear;
        w_data.targetting_type = pred_sdk::targetting_type::center;
        w_data.range = 1050.f;    
        w_data.radius = 87.5f;         
        w_data.delay = 0.25f;
        w_data.projectile_speed = FLT_MAX;   

        const auto pred = sdk::prediction->predict(target, w_data);
        if (pred.is_valid && pred.hitchance >= pred_sdk::hitchance::high) {
            if (vengaboys::OrderManager::get_instance().cast_spell(1, pred.cast_position)) {
                spell_tracker.update_cooldown(1, g_sdk->clock_facade->get_game_time() + player->get_spell(1)->get_cooldown());
                return true;
            }
        }
        return false;
    }

    bool Velkoz::cast_e(game_object* target) {
        if (!target || !can_cast_spell(2, 0.25f)) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        pred_sdk::spell_data e_data{};
        e_data.spell_type = pred_sdk::spell_type::circular;
        e_data.targetting_type = pred_sdk::targetting_type::center;
        e_data.range = 800.f;    
        e_data.radius = 225.f;     
        e_data.delay = 0.25f + 0.55f;       
        e_data.projectile_speed = FLT_MAX;      

        const auto pred = sdk::prediction->predict(target, e_data);
        if (pred.is_valid && pred.hitchance >= pred_sdk::hitchance::medium) {
            if (vengaboys::OrderManager::get_instance().cast_spell(2, pred.cast_position)) {
                spell_tracker.update_cooldown(2, g_sdk->clock_facade->get_game_time() + player->get_spell(2)->get_cooldown());
                return true;
            }
        }
        return false;
    }

    game_object* Velkoz::find_best_r_target() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return nullptr;

        game_object* best_target = nullptr;
        float highest_score = 0.f;

        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!is_valid_target(hero, 0)) continue;

            float dist = hero->get_position().distance(player->get_position());
            if (dist > 1550.f) continue;       

            float health_percent = hero->get_hp() / hero->get_max_hp();
            float health_score = (1.0f - health_percent) * 1.5f;   

            int research_stacks = 0;
            for (auto buff : hero->get_buffs()) {
                if (buff->get_name() == "velkozresearchstack") {
                    research_stacks = buff->get_count();
                    break;
                }
            }
            float stack_score = research_stacks * 0.5f;   

            float distance_factor = 1.0f - (dist / 1550.f);

            float damage_percent = calculate_r_damage(hero) / hero->get_hp();
            float damage_score = std::min(damage_percent * 2.0f, 3.0f);   

            float total_score = health_score + stack_score + distance_factor + damage_score;

            if (hero->has_buff_of_type(buff_type::stun) ||
                hero->has_buff_of_type(buff_type::snare) ||
                hero->has_buff_of_type(buff_type::suppression)) {
                total_score += 2.0f;
            }

            if (total_score > highest_score) {
                highest_score = total_score;
                best_target = hero;
            }
        }

        return best_target;
    }

    bool Velkoz::cast_r(game_object* target) {
        if (!target || is_ulting || !can_cast_spell(3, 0.f)) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!is_valid_target(target, 1500.f)) return false;

        if (count_enemies_in_range(1200.f) < settings.r_min_enemies) {
            log_debug("Not enough enemies for R: %d (need %d)",
                count_enemies_in_range(1200.f), settings.r_min_enemies);
            return false;
        }

        const auto cast_pos = target->get_position();

        if (vengaboys::OrderManager::get_instance().cast_spell(3, cast_pos)) {
            spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + player->get_spell(3)->get_cooldown());
            is_ulting = true;
            r_cast_time = g_sdk->clock_facade->get_game_time();
            current_r_target = target;
            last_r_follow_time = g_sdk->clock_facade->get_game_time();
            r_auto_follow_active = true;
            log_debug("R cast on %s, auto-follow activated", target->get_name().c_str());
            return true;
        }

        return false;
    }

    void Velkoz::follow_r_target() {
        if (!is_ulting || !r_auto_follow_active) {
            return;
        }

        if (!settings.r_ignore_cursor && !settings.r_auto_aim_key_pressed) {
            return;
        }

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - last_r_follow_time < 0.15f) return;

        if (!current_r_target || !current_r_target->is_valid() || current_r_target->is_dead() || !current_r_target->is_visible()) {
            game_object* new_target = find_best_r_target();

            if (new_target) {
                log_debug("R target lost, switching to new target: %s", new_target->get_name().c_str());
                current_r_target = new_target;
            }
            else {
                log_debug("R target lost, no new target found");
                return;
            }
        }

        log_debug("Following R target: %s", current_r_target->get_name().c_str());

        bool cast_successful = false;

        if (vengaboys::OrderManager::get_instance().cast_spell(3, current_r_target->get_position())) {
            cast_successful = true;
            log_debug("R follow through OrderManager succeeded");
        }

        if (!cast_successful) {
            player->cast_spell(3, current_r_target->get_position());
            log_debug("R follow through direct cast");
        }

        last_r_follow_time = current_time;
    }

    void Velkoz::handle_combo() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (is_ulting) {
            follow_r_target();
            return;
        }

        game_object* target = sdk::target_selector->get_hero_target(
            [&](game_object* hero) -> bool { return is_valid_target(hero); });

        if (!target) return;

        if (settings.use_e_combo && can_cast_spell(2, 0.f) && !is_ulting) {
            if (cast_e(target)) return;
        }

        if (settings.use_w_combo && can_cast_spell(1, 0.f) && !is_ulting) {
            if (cast_w(target)) return;
        }

        if (settings.use_q_combo && can_cast_spell(0, 0.f) && !is_ulting) {
            if (cast_q(target)) return;
        }

        if (settings.use_r_combo && can_cast_spell(3, 0.f) && !is_ulting) {
            bool has_three_stacks = false;
            for (auto buff : target->get_buffs()) {
                if (buff->get_name() == "velkozresearchstack" && buff->get_count() == 3) {
                    has_three_stacks = true;
                    break;
                }
            }

            float health_percent = target->get_hp() / target->get_max_hp();
            if (has_three_stacks || health_percent < 0.5f || count_enemies_in_range(1200.f) >= settings.r_min_enemies) {
                cast_r(target);
            }
        }
    }

    void Velkoz::handle_harass() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.harass_min_mana / 100.f * player->get_max_par()) return;

        if (is_ulting) {
            follow_r_target();
            return;
        }

        game_object* target = sdk::target_selector->get_hero_target(
            [&](game_object* hero) -> bool { return is_valid_target(hero); });

        if (!target) return;

        if (settings.use_w_harass && can_cast_spell(1, 0.f) && !is_ulting) {
            if (cast_w(target)) return;
        }

        if (settings.use_q_harass && can_cast_spell(0, 0.f) && !is_ulting) {
            cast_q(target);
        }
    }

    void Velkoz::handle_lane_clear() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.clear_min_mana / 100.f * player->get_max_par()) return;

        if (is_ulting) {
            follow_r_target();
            return;
        }

        bool spell_cast = false;

        if (settings.use_w_clear && can_cast_spell(1, 0.f) && !is_ulting) {
            game_object* best_minion_w = nullptr;
            int max_minions_hit_w = 0;

            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead() ||
                    minion->get_team_id() == player->get_team_id() || minion->is_plant()) continue;
                if (minion->get_position().distance(player->get_position()) > 1050.f) continue;

                pred_sdk::spell_data w_data{};
                w_data.spell_type = pred_sdk::spell_type::linear;
                w_data.targetting_type = pred_sdk::targetting_type::center;
                w_data.range = 1050.f;
                w_data.radius = 87.5f;
                w_data.delay = 0.25f;
                w_data.projectile_speed = FLT_MAX;

                auto pred = sdk::prediction->predict(minion, w_data);
                if (pred.is_valid) {
                    int num_hit = 0;
                    for (auto other_minion : g_sdk->object_manager->get_minions()) {
                        if (!other_minion->is_valid() || other_minion->is_dead() ||
                            other_minion->get_team_id() == player->get_team_id() || other_minion->is_plant()) continue;
                        if (is_point_near_line(other_minion->get_position(), player->get_position(), pred.cast_position, w_data.radius + other_minion->get_bounding_radius())) {
                            num_hit++;
                        }
                    }
                    if (num_hit > max_minions_hit_w) {
                        max_minions_hit_w = num_hit;
                        best_minion_w = minion;
                    }
                }
            }
            if (best_minion_w && max_minions_hit_w >= 2) {
                if (cast_w(best_minion_w)) spell_cast = true;
            }
        }

        if (settings.use_q_clear && can_cast_spell(0, 0.f) && !spell_cast && !is_ulting) {
            game_object* best_minion_q = nullptr;
            float lowest_health = FLT_MAX;

            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead() ||
                    minion->get_team_id() == player->get_team_id() || minion->is_plant()) continue;
                if (minion->get_position().distance(player->get_position()) > 1100.f) continue;

                float q_dmg = calculate_q_damage(minion);
                if (minion->get_hp() <= q_dmg && minion->get_hp() < lowest_health) {
                    lowest_health = minion->get_hp();
                    best_minion_q = minion;
                }
            }
            if (best_minion_q) {
                cast_q(best_minion_q);
            }
        }
    }

    void Velkoz::handle_jungle_clear() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.jungle_min_mana / 100.f * player->get_max_par()) return;

        if (is_ulting) {
            follow_r_target();
            return;
        }

        game_object* target = sdk::target_selector->get_monster_target(
            [&](game_object* monster) -> bool {
                return is_valid_target(monster, 1500.f);
            });

        if (!target) return;

        if (settings.use_e_jungle && can_cast_spell(2, 0.f) && !is_ulting) {
            if (cast_e(target)) return;
        }

        if (settings.use_w_jungle && can_cast_spell(1, 0.f) && !is_ulting) {
            if (cast_w(target)) return;
        }

        if (settings.use_q_jungle && can_cast_spell(0, 0.f) && !is_ulting) {
            cast_q(target);
        }
    }

    bool Velkoz::on_before_attack(orb_sdk::event_data* data) {
        if (is_ulting && settings.block_orbwalker_r) {
            return false;
        }
        return true;
    }

    bool Velkoz::on_before_move(orb_sdk::event_data* data) {
        if (is_ulting && settings.block_orbwalker_r) {
            return false;
        }
        return true;
    }

    void Velkoz::on_buff_gain(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;

        std::string buff_name = buff->get_name();
        if (buff_name == "VelkozR") {
            is_ulting = true;
            r_auto_follow_active = true;
            log_debug("Velkoz R buff gained - Ult started, follow enabled");
        }
    }

    void Velkoz::on_buff_loss(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;

        std::string buff_name = buff->get_name();
        if (buff_name == "VelkozR") {
            is_ulting = false;
            r_auto_follow_active = false;
            current_r_target = nullptr;
            log_debug("Velkoz R buff lost - Ult ended, follow disabled");
        }
    }

    void Velkoz::on_process_spell_cast(game_object* sender, spell_cast* cast) {
        if (!initialized || !sender || !cast) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (sender != player) return;

        if (cast->get_spell_slot() == 0) {
            std::string spell_name = cast->get_spell_data()->get_static_data()->get_name();
            if (spell_name.find("VelkozQ") != std::string::npos) {
                if (cast->get_start_pos() != math::vector3() && cast->get_end_pos() != math::vector3()) {
                    q_reactivate_available = true;
                    q_reactivation_pending = false;
                    q_cast_time = g_sdk->clock_facade->get_game_time();
                    q_missile_start_pos = cast->get_start_pos();
                    q_missile_direction = (cast->get_end_pos() - cast->get_start_pos()).normalized();
                    q_missile_current_pos = q_missile_start_pos;
                    q_missile_object = nullptr;
                    q_missile_tracked = false;

                    log_debug("Q cast detected: start=%.1f,%.1f,%.1f dir=%.2f,%.2f,%.2f",
                        q_missile_start_pos.x, q_missile_start_pos.y, q_missile_start_pos.z,
                        q_missile_direction.x, q_missile_direction.y, q_missile_direction.z);
                }
                else {
                    q_reactivate_available = false;
                    q_reactivation_pending = false;
                    q_missile_object = nullptr;
                    q_missile_tracked = false;
                    log_debug("Q reactivation detected");
                }
            }
        }
        else if (cast->get_spell_slot() == 3) {
            if (cast->get_spell_data()->get_static_data()->get_name() == "VelkozR") {
                is_ulting = true;
                r_auto_follow_active = true;
                r_cast_time = g_sdk->clock_facade->get_game_time();
                log_debug("Velkoz R cast processed - Ult started, follow enabled");

                const auto cast_pos = cast->get_end_pos();
                float closest_dist = FLT_MAX;
                current_r_target = nullptr;

                for (auto hero : g_sdk->object_manager->get_heroes()) {
                    if (!is_valid_target(hero, 0)) continue;

                    float dist = hero->get_position().distance(cast_pos);
                    if (dist < closest_dist && dist <= 300.f) {
                        closest_dist = dist;
                        current_r_target = hero;
                    }
                }

                if (current_r_target) {
                    log_debug("R initial target: %s", current_r_target->get_name().c_str());
                }

                last_r_follow_time = g_sdk->clock_facade->get_game_time();
            }
        }
    }

    int Velkoz::count_enemies_in_range(float range) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0;

        int count = 0;
        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!is_valid_target(hero, 0)) continue;

            if (hero->get_position().distance(player->get_position()) <= range) {
                count++;
            }
        }
        return count;
    }

    bool Velkoz::is_valid_target(game_object* target, float extra_range) {
        if (!target || !target->is_valid() || target->is_dead() ||
            !target->is_targetable() || !target->is_visible()) {
            return false;
        }

        if (target->is_plant()) {
            return false;
        }

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (target->is_hero() && target->get_team_id() == player->get_team_id()) {
            return false;
        }

        float max_range = 1550.f + extra_range;       
        return target->get_position().distance(player->get_position()) <= max_range;
    }

    void Velkoz::on_draw() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        if (settings.draw_q_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), 1100.f, 2.f, 0x30BBBBFF);
        }
        if (settings.draw_w_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), 1050.f, 2.f, 0x3000FF00);
        }
        if (settings.draw_e_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), 800.f, 2.f, 0x30FF00FF);
        }
        if (settings.draw_r_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), 1550.f, 2.f, 0x30FFFF00);
        }

        if (settings.draw_debug) {
            if (is_ulting) {
                math::vector2 screen_pos(100, 140);
                g_sdk->renderer->add_text("ULTING ACTIVE", 14.f, screen_pos, 0, 0xFFFF0000);

                math::vector2 aim_pos(100, 160);
                if (settings.r_ignore_cursor || settings.r_auto_aim_key_pressed) {
                    g_sdk->renderer->add_text("Auto-aim: ON", 14.f, aim_pos, 0, 0xFF00FF00);
                }
                else {
                    g_sdk->renderer->add_text("Auto-aim: OFF (Using cursor)", 14.f, aim_pos, 0, 0xFFFF0000);
                }

                if (current_r_target && current_r_target->is_valid()) {
                    math::vector2 target_screen_pos = g_sdk->renderer->world_to_screen(current_r_target->get_position());
                    g_sdk->renderer->add_text("R TARGET", 14.f, target_screen_pos, 1, 0xFFFF0000);
                    g_sdk->renderer->add_circle_3d(current_r_target->get_position(), 100.f, 3.f, 0xFFFF0000);

                    math::vector2 info_pos(100, 180);
                    g_sdk->renderer->add_text("Auto follow: " + std::string(r_auto_follow_active ? "ON" : "OFF"),
                        14.f, info_pos, 0, r_auto_follow_active ? 0xFF00FF00 : 0xFFFF0000);
                }
            }

            if (q_reactivate_available) {
                track_q_missile();

                float current_time = g_sdk->clock_facade->get_game_time();
                float time_since_cast = current_time - q_cast_time;
                float dist_traveled = q_missile_current_pos.distance(q_missile_start_pos);

                math::vector3 q_end = q_missile_start_pos + (q_missile_direction * 1100.f);
                g_sdk->renderer->add_line_2d(
                    g_sdk->renderer->world_to_screen(q_missile_start_pos),
                    g_sdk->renderer->world_to_screen(q_end),
                    1.f, 0x60FFFFFF
                );

                g_sdk->renderer->add_circle_3d(q_missile_current_pos, 60.f, 2.f, 0xFF00FFFF);

                math::vector3 split_position = q_missile_current_pos + (q_missile_direction * 55.f);
                g_sdk->renderer->add_circle_3d(split_position, 30.f, 2.f, 0xFFFF00FF);

                math::vector3 perpendicular_left = q_missile_direction.perpendicular_left().normalized();
                math::vector3 perpendicular_right = q_missile_direction.perpendicular_right().normalized();

                math::vector3 split_left_end = split_position + (perpendicular_left * 850.f);
                math::vector3 split_right_end = split_position + (perpendicular_right * 850.f);

                g_sdk->renderer->add_line_2d(
                    g_sdk->renderer->world_to_screen(split_position),
                    g_sdk->renderer->world_to_screen(split_left_end),
                    1.f, 0x80FFFF00
                );

                g_sdk->renderer->add_line_2d(
                    g_sdk->renderer->world_to_screen(split_position),
                    g_sdk->renderer->world_to_screen(split_right_end),
                    1.f, 0x80FFFF00
                );

                g_sdk->renderer->add_circle_3d(split_position, 200.f, 1.5f, 0x40FFFF00);

                for (auto hero : g_sdk->object_manager->get_heroes()) {
                    if (!is_valid_target(hero, 0)) continue;

                    if (would_q_split_hit_target(hero)) {
                        g_sdk->renderer->add_circle_3d(hero->get_position(), 100.f, 2.f, 0xFFFF0000);
                        math::vector2 target_pos = g_sdk->renderer->world_to_screen(hero->get_position());
                        g_sdk->renderer->add_text("SPLIT TARGET", 12.f, target_pos, 1, 0xFFFF0000);
                    }
                }

                math::vector2 screen_pos(400, 400);
                g_sdk->renderer->add_text("Q REACTIVATE READY", 14.f, screen_pos, 0, 0xFFFFFF00);

                math::vector2 time_pos(400, 420);
                char time_buf[64];
                snprintf(time_buf, sizeof(time_buf), "Time: %.1fs | Dist: %.1f", time_since_cast, dist_traveled);
                g_sdk->renderer->add_text(time_buf, 14.f, time_pos, 0, 0xFFFFFF00);

                bool would_hit = is_any_target_in_q_split_path();
                math::vector2 hit_pos(400, 440);
                g_sdk->renderer->add_text(would_hit ? "TARGET DETECTED" : "NO TARGET", 14.f, hit_pos, 0,
                    would_hit ? 0xFF00FF00 : 0xFFFF0000);

                if (q_reactivation_pending) {
                    math::vector2 pending_pos(400, 460);
                    g_sdk->renderer->add_text("REACTIVATION PENDING", 14.f, pending_pos, 0, 0xFFFF8800);
                }
            }
        }

        if (settings.draw_damage) {
            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (!is_valid_target(enemy, 0)) continue;
                g_sdk->renderer->add_damage_indicator(enemy, calculate_full_combo_damage(enemy));
            }
        }
    }

    void update_infotab() {
        if (!sdk::infotab) return;

        if (settings.infotab_id != 0) {
            sdk::infotab->remove_text(settings.infotab_id);
        }

        infotab_sdk::text_entry title = { "Vel'Koz", 0xFFFFFFFF };
        settings.infotab_id = sdk::infotab->add_hotkey_text(&settings.auto_q_hotkey_str, title, []() -> infotab_sdk::text_entry {
            infotab_sdk::text_entry entry;

            std::string status_text = "Auto Q: ";

            if (settings.auto_q_hotkey_pressed) {
                status_text += "ACTIVE (Holding)";
                entry.color = 0xFF00FFFF;    
            }
            else if (settings.auto_reactivate_q) {
                status_text += "ON";
                entry.color = 0xFF00FF00;  
            }
            else {
                status_text += "OFF";
                entry.color = 0xFFFF0000;  
            }

            status_text += " | R Auto-aim: ";
            if (settings.r_auto_aim_key_pressed) {
                status_text += "ACTIVE (Holding)";
            }
            else if (settings.r_ignore_cursor) {
                status_text += "ON";
            }
            else {
                status_text += "OFF";
            }

            entry.text = status_text;
            return entry;
            });
    }

    void Velkoz::create_menu() {
        menu = g_sdk->menu_manager->add_category("velkoz", "Vel'Koz");
        if (!menu) return;

        auto combo_menu = menu->add_sub_category("combo", "Combo Settings");
        if (combo_menu) {
            combo_menu->add_checkbox("use_q_combo", "Use Q", settings.use_q_combo,
                [&](bool val) { settings.use_q_combo = val; });
            combo_menu->add_checkbox("use_q_split", "Use Q Split", settings.use_q_split,
                [&](bool val) { settings.use_q_split = val; });

            combo_menu->add_hotkey("auto_q_hotkey", "Auto Reactivate Q Toggle/Hold", settings.auto_q_hotkey, false, true,
                [&](std::string* key, bool val) {
                    settings.auto_q_hotkey_str = *key;

                    if (val) {
                        settings.auto_reactivate_q = !settings.auto_reactivate_q;
                        g_sdk->log_console("[Velkoz] Auto Q Reactivate %s", settings.auto_reactivate_q ? "ON" : "OFF");
                    }

                    settings.auto_q_hotkey_pressed = val;

                    update_infotab();
                });

            combo_menu->add_checkbox("auto_reactivate_q", "Auto Reactivate Q", settings.auto_reactivate_q,
                [&](bool val) {
                    settings.auto_reactivate_q = val;
                    update_infotab();
                });

            combo_menu->add_checkbox("use_w_combo", "Use W", settings.use_w_combo,
                [&](bool val) { settings.use_w_combo = val; });
            combo_menu->add_checkbox("use_e_combo", "Use E", settings.use_e_combo,
                [&](bool val) { settings.use_e_combo = val; });
            combo_menu->add_checkbox("use_r_combo", "Use R", settings.use_r_combo,
                [&](bool val) { settings.use_r_combo = val; });
            combo_menu->add_slider_int("r_min_enemies", "Min Enemies for R", 1, 5, 1,
                settings.r_min_enemies, [&](int val) { settings.r_min_enemies = val; });
            combo_menu->add_checkbox("block_orbwalker_r", "Block Orbwalker in R",
                settings.block_orbwalker_r, [&](bool val) { settings.block_orbwalker_r = val; });

            combo_menu->add_checkbox("r_ignore_cursor", "Always Auto-Aim R", settings.r_ignore_cursor,
                [&](bool val) {
                    settings.r_ignore_cursor = val;
                    update_infotab();
                });

            combo_menu->add_hotkey("r_auto_aim_key", "Auto-Aim R While Held", settings.r_auto_aim_key, false, false,
                [&](std::string* key, bool val) {
                    settings.r_auto_aim_key_str = *key;
                    settings.r_auto_aim_key_pressed = val;
                    update_infotab();
                });

            combo_menu->add_checkbox("r_auto_follow", "Auto Follow with R",
                settings.r_auto_follow, [&](bool val) { settings.r_auto_follow = val; });
        }

        auto harass_menu = menu->add_sub_category("harass", "Harass Settings");
        if (harass_menu) {
            harass_menu->add_checkbox("use_q_harass", "Use Q", settings.use_q_harass,
                [&](bool val) { settings.use_q_harass = val; });
            harass_menu->add_checkbox("use_w_harass", "Use W", settings.use_w_harass,
                [&](bool val) { settings.use_w_harass = val; });
            harass_menu->add_slider_float("harass_min_mana", "Min Mana %", 0.f, 100.f, 5.f,
                settings.harass_min_mana, [&](float val) { settings.harass_min_mana = val; });
        }

        auto clear_menu = menu->add_sub_category("clear", "Lane Clear Settings");
        if (clear_menu) {
            clear_menu->add_checkbox("use_q_clear", "Use Q", settings.use_q_clear,
                [&](bool val) { settings.use_q_clear = val; });
            clear_menu->add_checkbox("use_w_clear", "Use W", settings.use_w_clear,
                [&](bool val) { settings.use_w_clear = val; });
            clear_menu->add_slider_float("clear_min_mana", "Min Mana %", 0.f, 100.f, 5.f,
                settings.clear_min_mana, [&](float val) { settings.clear_min_mana = val; });
        }

        auto jungle_menu = menu->add_sub_category("jungle", "Jungle Clear Settings");
        if (jungle_menu) {
            jungle_menu->add_checkbox("use_q_jungle", "Use Q", settings.use_q_jungle,
                [&](bool val) { settings.use_q_jungle = val; });
            jungle_menu->add_checkbox("use_w_jungle", "Use W", settings.use_w_jungle,
                [&](bool val) { settings.use_w_jungle = val; });
            jungle_menu->add_checkbox("use_e_jungle", "Use E", settings.use_e_jungle,
                [&](bool val) { settings.use_e_jungle = val; });
            jungle_menu->add_slider_float("jungle_min_mana", "Min Mana %", 0.f, 100.f, 5.f,
                settings.jungle_min_mana, [&](float val) { settings.jungle_min_mana = val; });
        }

        auto draw_menu = menu->add_sub_category("draw", "Draw Settings");
        if (draw_menu) {
            draw_menu->add_checkbox("draw_q_range", "Draw Q Range", settings.draw_q_range,
                [&](bool val) { settings.draw_q_range = val; });
            draw_menu->add_checkbox("draw_w_range", "Draw W Range", settings.draw_w_range,
                [&](bool val) { settings.draw_w_range = val; });
            draw_menu->add_checkbox("draw_e_range", "Draw E Range", settings.draw_e_range,
                [&](bool val) { settings.draw_e_range = val; });
            draw_menu->add_checkbox("draw_r_range", "Draw R Range", settings.draw_r_range,
                [&](bool val) { settings.draw_r_range = val; });
            draw_menu->add_checkbox("draw_damage", "Draw Damage Indicator", settings.draw_damage,
                [&](bool val) { settings.draw_damage = val; });
            draw_menu->add_checkbox("draw_debug", "Draw Debug Info", settings.draw_debug,
                [&](bool val) { settings.draw_debug = val; });
        }
    }

    void Velkoz::on_update() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        if (q_reactivate_available) {
            if (settings.auto_reactivate_q || settings.auto_q_hotkey_pressed) {
                handle_q_reactivation();
            }

            float current_time = g_sdk->clock_facade->get_game_time();
            if (current_time - q_cast_time > 3.0f) {
                q_reactivate_available = false;
                q_reactivation_pending = false;
                q_missile_object = nullptr;
                q_missile_tracked = false;
            }
        }

        if (is_ulting && settings.r_auto_follow &&
            (settings.r_ignore_cursor || settings.r_auto_aim_key_pressed)) {
            follow_r_target();
        }

        if (sdk::orbwalker->combo()) {
            handle_combo();
        }
        else if (sdk::orbwalker->harass()) {
            handle_harass();
        }
        else if (sdk::orbwalker->clear()) {
            handle_lane_clear();
        }
        else if (sdk::orbwalker->fast_clear()) {
            handle_jungle_clear();
        }
    }

    void Velkoz::load() {
        try {
            g_sdk->log_console("[+] Loading Vel'Koz...");

            if (!sdk_init::orbwalker() || !sdk_init::prediction() ||
                !sdk_init::damage() || !sdk_init::target_selector()) {
                g_sdk->log_console("[!] Failed to initialize required SDKs");
                return;
            }

            if (sdk_init::infotab()) {
                update_infotab();
            }

            create_menu();
            spell_tracker = SpellTracker{};
            q_reactivate_available = false;
            q_reactivation_pending = false;
            q_cast_time = 0.f;
            q_missile_tracked = false;
            q_missile_object = nullptr;
            is_ulting = false;
            r_auto_follow_active = false;
            current_r_target = nullptr;
            initialized = true;

            settings.draw_debug = true;

            g_sdk->event_manager->register_callback(event_manager::event::game_update,
                reinterpret_cast<void*>(&on_game_update));
            g_sdk->event_manager->register_callback(event_manager::event::draw_world,
                reinterpret_cast<void*>(&on_draw_world));
            g_sdk->event_manager->register_callback(event_manager::event::buff_gain,
                reinterpret_cast<void*>(&on_buff_gain_callback));
            g_sdk->event_manager->register_callback(event_manager::event::buff_loss,
                reinterpret_cast<void*>(&on_buff_loss_callback));
            g_sdk->event_manager->register_callback(event_manager::event::process_cast,
                reinterpret_cast<void*>(&on_process_cast_callback));

            if (sdk::orbwalker) {
                sdk::orbwalker->register_callback(orb_sdk::before_attack,
                    reinterpret_cast<void*>(&before_attack_callback));
                sdk::orbwalker->register_callback(orb_sdk::before_move,
                    reinterpret_cast<void*>(&before_move_callback));
            }

            g_sdk->log_console("[+] Vel'Koz loaded successfully");
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in Velkoz::load: %s", e.what());
        }
    }

    void Velkoz::unload() {
        if (!initialized) return;

        if (sdk::infotab && settings.infotab_id != 0) {
            sdk::infotab->remove_text(settings.infotab_id);
            settings.infotab_id = 0;
        }

        is_ulting = false;
        r_auto_follow_active = false;
        current_r_target = nullptr;
        q_reactivate_available = false;
        q_reactivation_pending = false;
        q_missile_tracked = false;
        q_missile_object = nullptr;
        spell_tracker = SpellTracker{};

        g_sdk->event_manager->unregister_callback(event_manager::event::game_update,
            reinterpret_cast<void*>(&on_game_update));
        g_sdk->event_manager->unregister_callback(event_manager::event::draw_world,
            reinterpret_cast<void*>(&on_draw_world));
        g_sdk->event_manager->unregister_callback(event_manager::event::buff_gain,
            reinterpret_cast<void*>(&on_buff_gain_callback));
        g_sdk->event_manager->unregister_callback(event_manager::event::buff_loss,
            reinterpret_cast<void*>(&on_buff_loss_callback));
        g_sdk->event_manager->unregister_callback(event_manager::event::process_cast,
            reinterpret_cast<void*>(&on_process_cast_callback));

        if (sdk::orbwalker) {
            sdk::orbwalker->unregister_callback(orb_sdk::before_attack,
                reinterpret_cast<void*>(&before_attack_callback));
            sdk::orbwalker->unregister_callback(orb_sdk::before_move,
                reinterpret_cast<void*>(&before_move_callback));
        }

        initialized = false;
        g_sdk->log_console("[-] Vel'Koz unloaded");
    }

    void __fastcall on_game_update() {
        velkoz_instance.on_update();
    }

    void __fastcall on_draw_world() {
        velkoz_instance.on_draw();
    }

    bool __fastcall before_attack_callback(orb_sdk::event_data* data) {
        return velkoz_instance.on_before_attack(data);
    }

    bool __fastcall before_move_callback(orb_sdk::event_data* data) {
        return velkoz_instance.on_before_move(data);
    }

    void __fastcall on_buff_gain_callback(game_object* source, buff_instance* buff) {
        velkoz_instance.on_buff_gain(source, buff);
    }

    void __fastcall on_buff_loss_callback(game_object* source, buff_instance* buff) {
        velkoz_instance.on_buff_loss(source, buff);
    }

    void __fastcall on_process_cast_callback(game_object* sender, spell_cast* cast) {
        velkoz_instance.on_process_spell_cast(sender, cast);
    }

    void load() {
        if (!sdk_init::orbwalker()) {
            g_sdk->log_console("[!] Failed to initialize orbwalker");
            return;
        }

        if (!sdk_init::prediction()) {
            g_sdk->log_console("[!] Failed to initialize prediction");
            return;
        }

        if (!sdk_init::damage()) {
            g_sdk->log_console("[!] Failed to initialize damage");
            return;
        }

        if (!sdk_init::target_selector()) {
            g_sdk->log_console("[!] Failed to initialize target selector");
            return;
        }

        velkoz_instance.load();
    }

    void unload() {
        velkoz_instance.unload();
    }

}   