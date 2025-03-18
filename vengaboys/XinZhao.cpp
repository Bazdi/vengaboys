#include "XinZhao.hpp"
#include "ordermanager.hpp"

namespace xinzhao {

    XinZhao xin_zhao;
    Settings settings;
    bool debug_enabled = false;

    void XinZhao::log_debug(const char* format, ...) {
        if (!debug_enabled) return;

        char buffer[512];
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);

        g_sdk->log_console("[XinZhao Debug] %s", buffer);
    }

    bool XinZhao::can_cast_spell(int spell_slot, float delay) {
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

    void XinZhao::update_aa_timer() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        // Reset the auto attack flag after a short duration
        if (combat_state.recently_auto_attacked &&
            current_time > combat_state.last_aa_time + 0.25f) {
            combat_state.recently_auto_attacked = false;
        }

        // Update auto attack timer
        float attack_time = 1.0f / player->get_attack_speed_mod();
        combat_state.auto_attack_timer = combat_state.last_aa_time + attack_time;
    }

    bool XinZhao::is_aa_reset_window() {
        // Check if we're in the window where an AA reset would be most effective
        float current_time = g_sdk->clock_facade->get_game_time();
        return combat_state.recently_auto_attacked &&
            current_time < combat_state.last_aa_time + 0.25f;
    }

    void XinZhao::handle_aa_reset(game_object* target) {
        if (!target || !target->is_valid()) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        // Handle AA reset with Q if appropriate
        if (is_aa_reset_window() && can_cast_spell(0, 0.f) && !combat_state.q_active) {
            // In combo mode
            if (sdk::orbwalker->combo() && settings.use_q) {
                if (target->is_hero() && target->get_team_id() != player->get_team_id()) {
                    if (cast_q(target)) {
                        log_debug("AA-reset with Q in combo mode");
                    }
                }
            }
            // In jungle clear mode
            else if (sdk::orbwalker->clear() && settings.jungle_use_q) {
                if (target->is_monster() &&
                    (target->is_large_monster() || target->is_epic_monster())) {
                    if (cast_q(target)) {
                        log_debug("AA-reset with Q on large monster");
                    }
                }
            }
            // In lane clear mode
            else if (sdk::orbwalker->lasthit() && settings.clear_use_q) {
                if (target->is_minion() && target->is_lane_minion_siege()) {
                    if (cast_q(target)) {
                        log_debug("AA-reset with Q on cannon minion");
                    }
                }
            }
        }
    }

    void XinZhao::update_passive() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        float current_time = g_sdk->clock_facade->get_game_time();

        // Check for stack timeout
        if (current_time - combat_state.passive.last_stack_time > combat_state.passive.stack_duration) {
            combat_state.passive.stacks = 0;
            combat_state.passive.healing_ready = false;
        }

        // Calculate healing if needed (3 stacks)
        if (combat_state.passive.stacks == 3 && !combat_state.passive.healing_ready) {
            // Healing scales with level: 3/3.5/4% max health (+65% AP)
            float heal_percent = 3.f;
            if (player->get_level() >= 11) heal_percent = 3.5f;
            if (player->get_level() >= 16) heal_percent = 4.f;

            float ap_bonus = player->get_ability_power() * 0.65f;
            float base_heal = (player->get_max_hp() * (heal_percent / 100.f));
            combat_state.passive.healing_amount = base_heal + ap_bonus;
            combat_state.passive.healing_ready = true;
        }
    }

    float XinZhao::calculate_q_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(0);
        if (!spell || spell->get_level() == 0) return 0.f;

        // Q damage per hit: 16/29/42/55/68 (+40% bonus AD)
        const float base_damage[] = { 16.f, 29.f, 42.f, 55.f, 68.f };
        const float bonus_ad_ratio = 0.4f;
        const float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
        float damage_per_hit = base_damage[spell->get_level() - 1] + (bonus_ad * bonus_ad_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, damage_per_hit);
    }

    float XinZhao::calculate_w_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(1);
        if (!spell || spell->get_level() == 0) return 0.f;

        // First hit (slash): 30/40/50/60/70 (+30% AD)
        const float slash_base[] = { 30.f, 40.f, 50.f, 60.f, 70.f };
        const float slash_ad_ratio = 0.3f;

        // Second hit (thrust): 50/85/120/155/190 (+90% AD + 65% AP)
        const float thrust_base[] = { 50.f, 85.f, 120.f, 155.f, 190.f };
        const float thrust_ad_ratio = 0.9f;
        const float thrust_ap_ratio = 0.65f;

        float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
        float ap = player->get_ability_power();

        float slash_damage = slash_base[spell->get_level() - 1] + (bonus_ad * slash_ad_ratio);
        float thrust_damage = thrust_base[spell->get_level() - 1] + (bonus_ad * thrust_ad_ratio) + (ap * thrust_ap_ratio);

        // Adjust for minions
        if (target->is_minion()) {
            float scaling = 0.5f + (0.5f * spell->get_level() / 5.0f);
            slash_damage *= scaling;
            thrust_damage *= scaling;
        }

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, slash_damage + thrust_damage);
    }

    float XinZhao::calculate_e_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(2);
        if (!spell || spell->get_level() == 0) return 0.f;

        // E damage: 50/75/100/125/150 (+60% AP)
        const float base_damage[] = { 50.f, 75.f, 100.f, 125.f, 150.f };
        const float ap_ratio = 0.6f;
        float damage = base_damage[spell->get_level() - 1] + (player->get_ability_power() * ap_ratio);

        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, damage);
    }

    float XinZhao::calculate_r_damage(game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(3);
        if (!spell || spell->get_level() == 0) return 0.f;

        // R damage: 75/175/275 (+100% bonus AD + 15% target's current health)
        const float base_damage[] = { 75.f, 175.f, 275.f };
        const float bonus_ad_ratio = 1.0f;
        const float health_ratio = 0.15f;

        float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
        float flat_damage = base_damage[spell->get_level() - 1] + (bonus_ad * bonus_ad_ratio);
        float health_damage = target->get_hp() * health_ratio;

        // Cap health damage against monsters
        if (target->is_monster()) {
            health_damage = std::min(health_damage, 600.f);
        }

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, flat_damage + health_damage);
    }

    float XinZhao::calculate_full_combo_damage(game_object* target) {
        if (!target) return 0.f;

        float total_damage = 0.f;

        // Q damage (3 hits)
        if (can_cast_spell(0, 0.f)) {
            total_damage += calculate_q_damage(target) * 3;
        }

        // W damage (both parts)
        if (can_cast_spell(1, 0.f)) {
            total_damage += calculate_w_damage(target);
        }

        // E damage
        if (can_cast_spell(2, 0.f)) {
            total_damage += calculate_e_damage(target);
        }

        // R damage
        if (can_cast_spell(3, 0.f)) {
            total_damage += calculate_r_damage(target);
        }

        // Add 2-3 auto attacks
        auto player = g_sdk->object_manager->get_local_player();
        if (player) {
            total_damage += sdk::damage->get_aa_damage(player, target) * 3;
        }

        return total_damage;
    }

    bool XinZhao::cast_q(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(0, 0.f)) return false;

        try {
            if (vengaboys::OrderManager::get_instance().cast_spell(0, player->get_position())) {
                combat_state.last_q_time = g_sdk->clock_facade->get_game_time();
                combat_state.q_active = true;
                combat_state.q_stacks = 0;

                // Reduce other cooldowns by 1s
                for (int i = 1; i < 4; i++) {
                    auto spell = player->get_spell(i);
                    if (spell && spell->get_cooldown() > 0.f) {
                        spell_tracker.update_cooldown(i, spell_tracker.cooldowns[i] - 1.f);
                    }
                }

                log_debug("Q activated successfully");
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_q: %s", e.what());
        }
        return false;
    }

    bool XinZhao::cast_w(game_object* target) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(1, 0.5f)) return false;

        math::vector3 cast_position;
        if (target && target->is_valid() && target->is_targetable()) {
            pred_sdk::spell_data w_data{};
            w_data.spell_type = pred_sdk::spell_type::linear;
            w_data.targetting_type = pred_sdk::targetting_type::center;
            w_data.range = 1000.f;
            w_data.radius = 40.f;
            w_data.delay = 0.5f;
            w_data.projectile_speed = 6250.f;

            const auto pred = sdk::prediction->predict(target, w_data);
            if (pred.is_valid && pred.hitchance >= pred_sdk::hitchance::high) {
                cast_position = pred.cast_position;
            }
            else {
                cast_position = target->get_position();
            }
        }
        else {
            cast_position = g_sdk->hud_manager->get_cursor_position();
        }

        try {
            if (vengaboys::OrderManager::get_instance().cast_spell(1, cast_position)) {
                combat_state.last_w_time = g_sdk->clock_facade->get_game_time();
                spell_tracker.update_cooldown(1, g_sdk->clock_facade->get_game_time() + player->get_spell(1)->get_cooldown());

                // Add passive stack
                if (combat_state.passive.stacks < 3) {
                    combat_state.passive.stacks++;
                    combat_state.passive.last_stack_time = g_sdk->clock_facade->get_game_time();
                }

                log_debug("W cast successfully");
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_w: %s", e.what());
        }
        return false;
    }

    bool XinZhao::cast_e(game_object* target) {
        if (!target) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(2, 0.f)) return false;

        // Check range (extended for challenged targets)
        float e_range = settings.e_max_range;
        bool is_challenged = false;

        for (auto buff : target->get_buffs()) {
            if (buff->get_name() == "XinZhaoPTarget" && buff->is_active()) {
                e_range = 1100.f;
                is_challenged = true;
                break;
            }
        }

        if (target->get_position().distance(player->get_position()) > e_range) {
            log_debug("Target out of E range: %.1f > %.1f",
                target->get_position().distance(player->get_position()), e_range);
            return false;
        }

        // Check turret dive safety
        if (!settings.e_turret_dive && is_under_turret(target->get_position())) {
            log_debug("E cancelled - target under turret");
            return false;
        }

        try {
            if (vengaboys::OrderManager::get_instance().cast_spell(2, target)) {
                combat_state.last_e_time = g_sdk->clock_facade->get_game_time();
                spell_tracker.update_cooldown(2, g_sdk->clock_facade->get_game_time() + player->get_spell(2)->get_cooldown());
                log_debug("E cast successfully on %s", target->get_char_name().c_str());
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_e: %s", e.what());
        }
        return false;
    }

    bool XinZhao::cast_r(bool force) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast_spell(3, 0.35f)) return false;

        // Check enemy count unless forced
        if (!force) {
            int enemies_in_range = count_enemies_in_range(500.f);
            if (enemies_in_range < settings.r_min_enemies) {
                log_debug("R cancelled - not enough enemies (%d < %d)",
                    enemies_in_range, (int)settings.r_min_enemies);
                return false;
            }
        }

        try {
            if (vengaboys::OrderManager::get_instance().cast_spell(3, player->get_position())) {
                combat_state.last_r_time = g_sdk->clock_facade->get_game_time();
                spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + player->get_spell(3)->get_cooldown());
                log_debug("R cast successfully");
                return true;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in cast_r: %s", e.what());
        }
        return false;
    }

    void XinZhao::handle_combo() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        game_object* target = sdk::target_selector->get_hero_target(
            [this](game_object* hero) -> bool {
                return is_valid_target(hero);
            });

        if (!target) return;

        float distance = player->get_position().distance(target->get_position());
        bool spell_cast = false;

        // Enhanced combo sequence with proper AA reset:
        // 1. E to gap close
        // 2. AA 
        // 3. Q immediately after AA (reset)
        // 4. W during Q attacks
        // 5. R when needed

        // Use E to gap close if not in AA range
        if (settings.use_e && can_cast_spell(2, 0.f) && !spell_cast) {
            if (distance > player->get_attack_range() && distance <= settings.e_max_range) {
                spell_cast = cast_e(target);
                if (spell_cast) {
                    log_debug("Combo: E cast successful on %s", target->get_char_name().c_str());
                }
            }
        }

        // AA reset with Q after auto attack
        if (settings.use_q && can_cast_spell(0, 0.f) && !combat_state.q_active && !spell_cast) {
            // Immediate AA reset if we just attacked
            if (is_aa_reset_window()) {
                spell_cast = cast_q(target);
                if (spell_cast) {
                    log_debug("Combo: Q activated as AA reset");
                }
            }
            // Normal Q use if within range and not in immediate AA reset window
            else if (distance <= player->get_attack_range() + 50.f) {
                spell_cast = cast_q(target);
                if (spell_cast) {
                    log_debug("Combo: Q activated in range");
                }
            }
        }

        // W when close, ideally during Q attacks
        if (settings.use_w && can_cast_spell(1, 0.f) && !spell_cast) {
            // Prioritize using W during Q's enhanced attacks
            bool should_use_w = distance <= 900.f;

            // Prioritize if we're actively using Q's enhanced attacks
            if (combat_state.q_active && combat_state.q_stacks > 0) {
                should_use_w = true;
            }

            if (should_use_w) {
                spell_cast = cast_w(target);
                if (spell_cast) {
                    log_debug("Combo: W cast on %s", target->get_char_name().c_str());
                }
            }
        }

        // R logic
        if (settings.use_r && can_cast_spell(3, 0.f) && !spell_cast) {
            bool should_cast = false;

            // Offensive use - multiple enemies
            int enemies_in_range = count_enemies_in_range(450.f);
            if (enemies_in_range >= settings.r_min_enemies) {
                should_cast = true;
                log_debug("Combo: R - Multiple enemies in range (%d)", enemies_in_range);
            }
            // Defensive use - low HP
            else if (should_use_r_defensive()) {
                should_cast = true;
                log_debug("Combo: R - Defensive cast (low HP)");
            }
            // Execute use - if target will die from R
            else if (target->get_hp() <= calculate_r_damage(target)) {
                should_cast = true;
                log_debug("Combo: R - Execute cast (target low HP)");
            }

            if (should_cast) {
                spell_cast = cast_r(false);
            }
        }
    }

    void XinZhao::handle_harass() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.harass_min_mana / 100.f * player->get_max_par()) return;

        game_object* target = sdk::target_selector->get_hero_target(
            [this](game_object* hero) -> bool {
                auto player = g_sdk->object_manager->get_local_player();
                if (!player) return false;
                return is_valid_target(hero) &&
                    hero->get_position().distance(player->get_position()) <= 900.f;
            });

        if (!target) return;

        bool spell_cast = false;
        float distance = player->get_position().distance(target->get_position());

        // In harass, prefer W for poke
        if (settings.harass_use_w && can_cast_spell(1, 0.f) && !spell_cast) {
            if (distance <= 900.f) {
                spell_cast = cast_w(target);
                if (spell_cast) {
                    log_debug("Harass: W cast on %s", target->get_char_name().c_str());
                }
            }
        }

        // Use Q if already in range or just auto attacked
        if (settings.harass_use_q && can_cast_spell(0, 0.f) && !combat_state.q_active && !spell_cast) {
            if (is_aa_reset_window() || distance <= player->get_attack_range()) {
                spell_cast = cast_q(target);
                if (spell_cast) {
                    log_debug("Harass: Q activated");
                }
            }
        }

        // E only if safe
        if (settings.harass_use_e && can_cast_spell(2, 0.f) && !spell_cast) {
            if (!is_under_turret(target->get_position()) && distance <= settings.e_max_range) {
                // Only use E in harass if we have HP advantage or they're low
                float player_hp_percent = player->get_hp() / player->get_max_hp();
                float target_hp_percent = target->get_hp() / target->get_max_hp();

                if (target_hp_percent < 0.3f ||
                    target_hp_percent < player_hp_percent - 0.2f) {
                    spell_cast = cast_e(target);
                    if (spell_cast) {
                        log_debug("Harass: E cast on %s", target->get_char_name().c_str());
                    }
                }
            }
        }
    }

    void XinZhao::handle_jungle_clear() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.jungle_min_mana / 100.f * player->get_max_par()) return;

        // Find best target
        game_object* best_target = nullptr;
        game_object* large_monster = nullptr;
        float closest_distance = FLT_MAX;

        for (auto monster : g_sdk->object_manager->get_monsters()) {
            if (!monster->is_valid() || monster->is_dead()) continue;

            float distance = player->get_position().distance(monster->get_position());

            // Track large monsters separately
            if (monster->is_large_monster() || monster->is_epic_monster()) {
                if (!large_monster ||
                    monster->get_max_hp() > large_monster->get_max_hp()) {
                    large_monster = monster;
                }
            }

            if (distance < closest_distance && distance <= settings.e_max_range) {
                closest_distance = distance;
                best_target = monster;
            }
        }

        // Prioritize large monsters
        if (large_monster) {
            best_target = large_monster;
            closest_distance = player->get_position().distance(large_monster->get_position());
        }

        if (!best_target) return;

        bool is_epic = best_target->is_epic_monster();
        bool is_large = best_target->is_large_monster();
        float distance = closest_distance;
        bool spell_cast = false;

        // Smart jungle clear sequence based on target type:

        // For large/epic monsters: E → AA → Q (reset) → W (during Q)
        if (is_epic || is_large) {
            // E to reach camp if needed
            if (settings.jungle_use_e && can_cast_spell(2, 0.f) && !spell_cast) {
                if (distance > 200.f) {
                    spell_cast = cast_e(best_target);
                    if (spell_cast) {
                        log_debug("Jungle: E cast on large/epic monster");
                    }
                }
            }

            // Q for AA reset and enhanced damage
            if (settings.jungle_use_q && can_cast_spell(0, 0.f) && !combat_state.q_active && !spell_cast) {
                if (is_aa_reset_window() || distance <= player->get_attack_range() + 25.f) {
                    spell_cast = cast_q(best_target);
                    if (spell_cast) {
                        log_debug("Jungle: Q activated on large/epic monster");
                    }
                }
            }

            // W for damage during Q
            if (settings.jungle_use_w && can_cast_spell(1, 0.f) && !spell_cast) {
                if (combat_state.q_active || distance <= 600.f) {
                    spell_cast = cast_w(best_target);
                    if (spell_cast) {
                        log_debug("Jungle: W cast on large/epic monster");
                    }
                }
            }
        }
        // For small monsters: W (AOE) → E (if multiple) → Q (reset if needed)
        else {
            // W first for AOE damage
            if (settings.jungle_use_w && can_cast_spell(1, 0.f) && !spell_cast) {
                if (distance <= 600.f) {
                    spell_cast = cast_w(best_target);
                    if (spell_cast) {
                        log_debug("Jungle: W cast on small monsters");
                    }
                }
            }

            // E if multiple monsters or need to close gap
            if (settings.jungle_use_e && can_cast_spell(2, 0.f) && !spell_cast) {
                bool should_use_e = true;

                // Save E logic
                if (settings.save_e_after_camps && !is_large_monster_nearby()) {
                    should_use_e = false;
                    log_debug("Jungle: Saving E for next camp");
                }

                if (should_use_e && distance > 175.f) {
                    spell_cast = cast_e(best_target);
                    if (spell_cast) {
                        log_debug("Jungle: E cast on small monsters");
                    }
                }
            }

            // Q for faster clear
            if (settings.jungle_use_q && can_cast_spell(0, 0.f) && !combat_state.q_active && !spell_cast) {
                if (is_aa_reset_window() || distance <= player->get_attack_range() + 25.f) {
                    spell_cast = cast_q(best_target);
                    if (spell_cast) {
                        log_debug("Jungle: Q activated on small monsters");
                    }
                }
            }
        }
    }

    void XinZhao::handle_lane_clear() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->get_par() < settings.clear_min_mana / 100.f * player->get_max_par()) return;

        bool spell_cast = false;
        bool should_push = should_push_wave();

        log_debug("Lane clear: should_push = %s", should_push ? "true" : "false");

        // W for multiple minions
        if (settings.clear_use_w && can_cast_spell(1, 0.f) && !spell_cast) {
            // Only use W for pushing or if we have plenty of mana
            if (should_push || player->get_par() / player->get_max_par() > 0.6f) {
                math::vector3 best_pos;
                int max_hits = 0;

                for (auto minion : g_sdk->object_manager->get_minions()) {
                    if (!minion->is_valid() || minion->is_dead() ||
                        minion->get_team_id() == player->get_team_id())
                        continue;

                    if (minion->get_position().distance(player->get_position()) > 900.f)
                        continue;

                    int hits = 0;
                    math::vector3 test_pos = minion->get_position();

                    for (auto other_minion : g_sdk->object_manager->get_minions()) {
                        if (!other_minion->is_valid() || other_minion->is_dead() ||
                            other_minion->get_team_id() == player->get_team_id())
                            continue;

                        if (test_pos.distance(other_minion->get_position()) <= 140.f) {
                            hits++;
                        }
                    }

                    if (hits > max_hits) {
                        max_hits = hits;
                        best_pos = test_pos;
                    }
                }

                if (max_hits >= settings.clear_min_minions_w) {
                    if (vengaboys::OrderManager::get_instance().cast_spell(1, best_pos)) {
                        spell_cast = true;
                        log_debug("Lane clear: W cast on %d minions", max_hits);
                    }
                }
            }
        }

        // Q for last hits with AA reset
        if (settings.clear_use_q && can_cast_spell(0, 0.f) && !combat_state.q_active && !spell_cast) {
            // Use Q as AA reset if we just attacked
            if (is_aa_reset_window()) {
                // Find a minion that will die from Q-enhanced attack
                for (auto minion : g_sdk->object_manager->get_minions()) {
                    if (!minion->is_valid() || minion->is_dead() ||
                        minion->get_team_id() == player->get_team_id())
                        continue;

                    if (minion->get_position().distance(player->get_position()) <= player->get_attack_range()) {
                        float q_damage = calculate_q_damage(minion);
                        float aa_damage = sdk::damage->get_aa_damage(player, minion);

                        // Check if minion can be killed with Q-enhanced AA
                        if (minion->get_hp() <= q_damage + aa_damage) {
                            spell_cast = cast_q(minion);
                            if (spell_cast) {
                                log_debug("Lane clear: Q AA-reset on low HP minion");
                                break;
                            }
                        }
                    }
                }
            }
            // Use Q on cannons or if pushing
            else if (should_push) {
                for (auto minion : g_sdk->object_manager->get_minions()) {
                    if (!minion->is_valid() || minion->is_dead() ||
                        minion->get_team_id() == player->get_team_id())
                        continue;

                    if (minion->get_position().distance(player->get_position()) <= player->get_attack_range()) {
                        bool should_q = minion->is_lane_minion_siege();

                        // Always use on cannons, otherwise only if pushing
                        if (should_q || should_push) {
                            spell_cast = cast_q(minion);
                            if (spell_cast) {
                                log_debug("Lane clear: Q activated for pushing/cannon");
                                break;
                            }
                        }
                    }
                }
            }
        }

        // E on cannons or low HP minions
        if (settings.clear_use_e && can_cast_spell(2, 0.f) && !spell_cast) {
            // Only use E for last hits unless pushing
            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead() ||
                    minion->get_team_id() == player->get_team_id())
                    continue;

                if (minion->get_position().distance(player->get_position()) > settings.e_max_range)
                    continue;

                bool should_e = false;
                if (minion->is_lane_minion_siege()) {
                    // Only E to cannons if they're low or we're pushing
                    should_e = should_push || minion->get_hp() / minion->get_max_hp() < 0.3f;
                }
                else {
                    float e_damage = calculate_e_damage(minion);
                    if (minion->get_hp() <= e_damage) {
                        should_e = true;
                    }
                    else if (should_push && minion->get_hp() / minion->get_max_hp() < 0.5f) {
                        should_e = true;
                    }
                }

                if (should_e) {
                    spell_cast = cast_e(minion);
                    if (spell_cast) {
                        log_debug("Lane clear: E cast on minion");
                        break;
                    }
                }
            }
        }
    }

    void XinZhao::handle_flee() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        bool spell_cast = false;
        float current_time = g_sdk->clock_facade->get_game_time();

        // First priority: Use E to dash to a safe target
        if (settings.flee_use_e && can_cast_spell(2, 0.f) && !spell_cast) {
            game_object* flee_target = get_best_flee_target();

            if (flee_target) {
                spell_cast = cast_e(flee_target);
                if (spell_cast) {
                    log_debug("Flee: E used to escape to %s",
                        flee_target->is_hero() ? flee_target->get_char_name().c_str() : "minion/monster");
                }
            }
        }

        // Second: Use W to slow pursuers
        if (settings.flee_use_w && can_cast_spell(1, 0.f) && !spell_cast) {
            // Get closest enemy
            game_object* closest_enemy = nullptr;
            float min_distance = FLT_MAX;

            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy->is_valid() || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                float dist = enemy->get_position().distance(player->get_position());
                if (dist < min_distance && dist <= 900.f) {
                    min_distance = dist;
                    closest_enemy = enemy;
                }
            }

            if (closest_enemy) {
                // Cast W toward pursuing enemy to slow them
                if (vengaboys::OrderManager::get_instance().cast_spell(1, closest_enemy->get_position())) {
                    combat_state.last_w_time = current_time;
                    spell_cast = true;
                    log_debug("Flee: W used to slow %s", closest_enemy->get_char_name().c_str());
                }
            }
        }

        // Last resort: Use R if surrounded and in danger
        if (settings.flee_use_r && can_cast_spell(3, 0.f) && !spell_cast) {
            int enemies_in_range = count_enemies_in_range(450.f);

            // Use R if surrounded by multiple enemies or low HP
            bool should_use_r = false;

            if (settings.flee_r_only_surrounded && enemies_in_range >= 2) {
                should_use_r = true;
            }
            else if (player->get_hp() / player->get_max_hp() < 0.3f && enemies_in_range >= 1) {
                should_use_r = true;
            }

            if (should_use_r) {
                spell_cast = cast_r(true);
                if (spell_cast) {
                    log_debug("Flee: R used defensively against %d enemies", enemies_in_range);
                }
            }
        }

        // Move toward cursor position
        if (!spell_cast) {
            auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
            vengaboys::OrderManager::get_instance().move_to(cursor_pos);
        }
    }

    game_object* XinZhao::get_best_flee_target() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return nullptr;

        auto cursor_pos = g_sdk->hud_manager->get_cursor_position();
        game_object* best_target = nullptr;
        float best_score = -1.0f;

        // Check allied heroes first
        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!hero->is_valid() || hero->is_dead() ||
                hero->get_team_id() != player->get_team_id() ||
                hero == player)
                continue;

            float dist_to_hero = player->get_position().distance(hero->get_position());
            if (dist_to_hero > settings.e_max_range) continue;

            // Score based on distance to cursor and safety
            float dist_to_cursor = hero->get_position().distance(cursor_pos);
            float safety_score = is_under_turret(hero->get_position()) ? 0.0f : 1.0f;

            // Check enemies around potential target
            int enemies_near_target = 0;
            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy->is_valid() || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                if (enemy->get_position().distance(hero->get_position()) < 500.f) {
                    enemies_near_target++;
                }
            }

            // If target has too many enemies nearby, reduce score
            if (enemies_near_target > 0) {
                safety_score *= (1.0f - 0.2f * enemies_near_target);
            }

            // Calculate final score (higher is better)
            float score = safety_score * (1000.0f - dist_to_cursor) / 1000.0f;

            if (score > best_score) {
                best_score = score;
                best_target = hero;
            }
        }

        // Check ally minions if no good hero target found
        if (best_score < 0.5f) {
            for (auto minion : g_sdk->object_manager->get_minions()) {
                if (!minion->is_valid() || minion->is_dead() ||
                    minion->get_team_id() != player->get_team_id())
                    continue;

                float dist_to_minion = player->get_position().distance(minion->get_position());
                if (dist_to_minion > settings.e_max_range) continue;

                // Score based on distance to cursor and safety
                float dist_to_cursor = minion->get_position().distance(cursor_pos);
                float safety_score = is_under_turret(minion->get_position()) ? 0.0f : 0.8f;

                // Check enemies around potential target
                int enemies_near_target = 0;
                for (auto enemy : g_sdk->object_manager->get_heroes()) {
                    if (!enemy->is_valid() || enemy->is_dead() ||
                        enemy->get_team_id() == player->get_team_id())
                        continue;

                    if (enemy->get_position().distance(minion->get_position()) < 500.f) {
                        enemies_near_target++;
                    }
                }

                // If target has too many enemies nearby, reduce score
                if (enemies_near_target > 0) {
                    safety_score *= (1.0f - 0.25f * enemies_near_target);
                }

                // Calculate final score (higher is better)
                float score = safety_score * (1000.0f - dist_to_cursor) / 1000.0f;

                if (score > best_score) {
                    best_score = score;
                    best_target = minion;
                }
            }
        }

        // Check monsters as a last resort
        if (best_score < 0.3f) {
            for (auto monster : g_sdk->object_manager->get_monsters()) {
                if (!monster->is_valid() || monster->is_dead()) continue;

                float dist_to_monster = player->get_position().distance(monster->get_position());
                if (dist_to_monster > settings.e_max_range) continue;

                // Score based on distance to cursor
                float dist_to_cursor = monster->get_position().distance(cursor_pos);
                float safety_score = 0.6f;  // Lower base score for monsters

                // Calculate final score (higher is better)
                float score = safety_score * (1000.0f - dist_to_cursor) / 1000.0f;

                if (score > best_score) {
                    best_score = score;
                    best_target = monster;
                }
            }
        }

        return best_target;
    }

    bool XinZhao::on_before_attack(orb_sdk::event_data* data) {
        if (!initialized) return true;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !data || !data->target) return true;

        // Track that we're about to attack
        combat_state.last_target = data->target;

        return true;
    }

    void XinZhao::on_buff_gain(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (source == player) {
            std::string buff_name = buff->get_name();
            if (buff_name == "XinZhaoQ") {
                combat_state.q_active = true;
                combat_state.q_stacks = 0;
                log_debug("Q buff gained");
            }
        }
    }

    void XinZhao::on_buff_lose(game_object* source, buff_instance* buff) {
        if (!initialized || !source || !buff) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || source != player) return;

        std::string buff_name = buff->get_name();
        if (buff_name == "XinZhaoQ") {
            combat_state.q_active = false;
            combat_state.q_stacks = 0;
            log_debug("Q buff lost");
        }
    }

    void XinZhao::on_process_spell_cast(game_object* sender, spell_cast* cast) {
        if (!initialized || !sender || !cast) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (sender == player) {
            int spell_slot = cast->get_spell_slot();

            // Track auto attacks for AA reset timing
            if (cast->is_basic_attack()) {
                combat_state.last_aa_time = g_sdk->clock_facade->get_game_time();
                combat_state.recently_auto_attacked = true;

                // Add passive stack from auto attack
                if (combat_state.passive.stacks < 3) {
                    combat_state.passive.stacks++;
                    combat_state.passive.last_stack_time = g_sdk->clock_facade->get_game_time();
                }

                // Track Q hits
                if (combat_state.q_active) {
                    combat_state.q_stacks++;

                    // Third Q hit
                    if (combat_state.q_stacks == 3) {
                        combat_state.q_active = false;
                        combat_state.q_stacks = 0;
                        log_debug("Third Q hit completed");
                    }
                }

                // Handle AA reset
                handle_aa_reset(cast->get_target());

                log_debug("Auto attack detected");
            }
            // Track other spells
            else {
                switch (spell_slot) {
                case 0: // Q
                    log_debug("Q cast detected");
                    break;
                case 1: // W
                    log_debug("W cast detected");
                    break;
                case 2: // E
                    log_debug("E cast detected");
                    break;
                case 3: // R
                    log_debug("R cast detected");
                    break;
                }
            }
        }
    }

    int XinZhao::count_enemies_in_range(float range) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0;

        int count = 0;
        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!hero->is_valid() || hero->is_dead() ||
                hero->get_team_id() == player->get_team_id())
                continue;

            if (hero->get_position().distance(player->get_position()) <= range) {
                count++;
            }
        }
        return count;
    }

    int XinZhao::count_allies_in_range(float range) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0;

        int count = 0;
        for (auto hero : g_sdk->object_manager->get_heroes()) {
            if (!hero->is_valid() || hero->is_dead() ||
                hero->get_team_id() != player->get_team_id() ||
                hero == player)
                continue;

            if (hero->get_position().distance(player->get_position()) <= range) {
                count++;
            }
        }
        return count;
    }

    bool XinZhao::is_under_turret(const math::vector3& pos) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        for (auto turret : g_sdk->object_manager->get_turrets()) {
            if (!turret->is_valid() || turret->is_dead() ||
                turret->get_team_id() == player->get_team_id())
                continue;

            if (pos.distance(turret->get_position()) <= 775.f) {
                return true;
            }
        }
        return false;
    }

    bool XinZhao::is_large_monster_nearby() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        for (auto monster : g_sdk->object_manager->get_monsters()) {
            if (!monster->is_valid() || monster->is_dead()) continue;

            if ((monster->is_large_monster() || monster->is_epic_monster()) &&
                monster->get_position().distance(player->get_position()) <= 1000.f) {
                return true;
            }
        }
        return false;
    }

    bool XinZhao::should_push_wave() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return true; // Default to pushing

        if (!settings.clear_wave_management) return true;

        // Don't push if we're under our turret
        bool under_ally_turret = false;
        for (auto turret : g_sdk->object_manager->get_turrets()) {
            if (!turret->is_valid() || turret->is_dead() ||
                turret->get_team_id() != player->get_team_id())
                continue;

            if (player->get_position().distance(turret->get_position()) <= 900.f) {
                under_ally_turret = true;
                break;
            }
        }

        if (under_ally_turret) return false;

        // Count enemy heroes nearby
        int nearby_enemies = count_enemies_in_range(1200.f);
        int nearby_allies = count_allies_in_range(1200.f);

        // Don't push if outnumbered
        if (nearby_enemies > nearby_allies + 1) {
            return false;
        }

        // Check if our allies are pushing
        int ally_minions = 0;
        int enemy_minions = 0;

        for (auto minion : g_sdk->object_manager->get_minions()) {
            if (!minion->is_valid() || minion->is_dead()) continue;

            if (minion->get_position().distance(player->get_position()) <= 800.f) {
                if (minion->get_team_id() == player->get_team_id())
                    ally_minions++;
                else
                    enemy_minions++;
            }
        }

        // Push if we have minion advantage
        if (ally_minions > enemy_minions + 3) {
            return true;
        }

        return false; // Default to not pushing if uncertain
    }

    void XinZhao::load() {
        try {
            g_sdk->log_console("[+] Loading Xin Zhao...");
            create_menu();
            initialized = true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in XinZhao::load: %s", e.what());
        }
        catch (...) {
            g_sdk->log_console("[!] Unknown exception in XinZhao::load");
        }
    }

    void XinZhao::unload() {
        initialized = false;
    }

    void XinZhao::on_update() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        update_passive();
        update_aa_timer();

        if (sdk::orbwalker) {
            if (sdk::orbwalker->combo()) {
                handle_combo();
            }
            else if (sdk::orbwalker->harass()) {
                handle_harass();
            }
            else if (sdk::orbwalker->clear()) {
                handle_jungle_clear();
            }
            else if (sdk::orbwalker->lasthit()) {
                handle_lane_clear();
            }
            else if (sdk::orbwalker->flee()) {
                handle_flee();
            }
        }
    }

    void XinZhao::on_draw() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.draw_w_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), 900.f, 2.f, 0x20BBBBFF);
        }

        if (settings.draw_e_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), settings.e_max_range, 2.f, 0x20BB00FF);
        }

        if (settings.draw_r_range) {
            g_sdk->renderer->add_circle_3d(player->get_position(), 450.f, 2.f, 0x20FF00FF);
        }

        if (settings.draw_damage) {
            for (auto enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy->is_valid() || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                g_sdk->renderer->add_damage_indicator(enemy, calculate_full_combo_damage(enemy));
            }
        }

        // AA reset timing indicator
        if (settings.draw_aa_reset_timing && combat_state.recently_auto_attacked) {
            float current_time = g_sdk->clock_facade->get_game_time();
            float reset_window = 0.25f;
            float time_since_aa = current_time - combat_state.last_aa_time;

            if (time_since_aa < reset_window) {
                // Draw circle at feet that shrinks as the reset window expires
                float radius = 50.f * (1.0f - time_since_aa / reset_window);
                uint32_t color = 0x90FF00FF; // Purple with alpha
                g_sdk->renderer->add_circle_3d(player->get_position(), radius, 2.f, color);

                // Draw text to indicate AA reset is available
                math::vector2 screen_pos = g_sdk->renderer->world_to_screen(
                    player->get_position() + math::vector3(0, 50.f, 0));
                g_sdk->renderer->add_text("AA Reset!", 14.f, screen_pos, 1, 0xFFFFFFFF);
            }
        }
    }

    void XinZhao::create_menu() {
        menu = g_sdk->menu_manager->add_category("xinzhao", "Xin Zhao");
        if (!menu) return;

        // Combat settings
        auto combat = menu->add_sub_category("combat", "Combat Settings");
        if (combat) {
            combat->add_checkbox("use_q", "Use Q", settings.use_q, [](bool value) { settings.use_q = value; });
            combat->add_checkbox("use_w", "Use W", settings.use_w, [](bool value) { settings.use_w = value; });
            combat->add_checkbox("use_e", "Use E", settings.use_e, [](bool value) { settings.use_e = value; });
            combat->add_checkbox("use_r", "Use R", settings.use_r, [](bool value) { settings.use_r = value; });
            combat->add_slider_float("r_min_enemies", "Min Enemies for R", 1.f, 5.f, 1.f, settings.r_min_enemies,
                [](float value) { settings.r_min_enemies = value; });
            combat->add_slider_float("r_hp_threshold", "R Defensive HP %", 5.f, 50.f, 5.f, settings.r_hp_threshold,
                [](float value) { settings.r_hp_threshold = value; });
        }

        // Harass settings
        auto harass = menu->add_sub_category("harass", "Harass Settings");
        if (harass) {
            harass->add_checkbox("harass_use_q", "Use Q", settings.harass_use_q,
                [](bool value) { settings.harass_use_q = value; });
            harass->add_checkbox("harass_use_w", "Use W", settings.harass_use_w,
                [](bool value) { settings.harass_use_w = value; });
            harass->add_checkbox("harass_use_e", "Use E", settings.harass_use_e,
                [](bool value) { settings.harass_use_e = value; });
            harass->add_slider_float("harass_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.harass_min_mana,
                [](float value) { settings.harass_min_mana = value; });
        }

        // Farm settings
        auto farm = menu->add_sub_category("farm", "Farm Settings");
        if (farm) {
            farm->add_checkbox("farm_use_q", "Use Q", settings.farm_use_q,
                [](bool value) { settings.farm_use_q = value; });
            farm->add_checkbox("farm_use_w", "Use W", settings.farm_use_w,
                [](bool value) { settings.farm_use_w = value; });
            farm->add_checkbox("farm_use_e", "Use E", settings.farm_use_e,
                [](bool value) { settings.farm_use_e = value; });
            farm->add_slider_float("farm_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.farm_min_mana,
                [](float value) { settings.farm_min_mana = value; });
        }

        // Jungle settings
        auto jungle = menu->add_sub_category("jungle", "Jungle Settings");
        if (jungle) {
            jungle->add_checkbox("jungle_use_q", "Use Q", settings.jungle_use_q,
                [](bool value) { settings.jungle_use_q = value; });
            jungle->add_checkbox("jungle_use_w", "Use W", settings.jungle_use_w,
                [](bool value) { settings.jungle_use_w = value; });
            jungle->add_checkbox("jungle_use_e", "Use E", settings.jungle_use_e,
                [](bool value) { settings.jungle_use_e = value; });
            jungle->add_checkbox("save_e_after_camps", "Save E For Next Camp", settings.save_e_after_camps,
                [](bool value) { settings.save_e_after_camps = value; });
            jungle->add_checkbox("jungle_smart_ability_order", "Smart Ability Order", settings.jungle_smart_ability_order,
                [](bool value) { settings.jungle_smart_ability_order = value; });
            jungle->add_slider_float("jungle_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.jungle_min_mana,
                [](float value) { settings.jungle_min_mana = value; });
        }

        // Clear settings
        auto clear = menu->add_sub_category("clear", "Clear Settings");
        if (clear) {
            clear->add_checkbox("clear_use_q", "Use Q", settings.clear_use_q,
                [](bool value) { settings.clear_use_q = value; });
            clear->add_checkbox("clear_use_w", "Use W", settings.clear_use_w,
                [](bool value) { settings.clear_use_w = value; });
            clear->add_checkbox("clear_use_e", "Use E", settings.clear_use_e,
                [](bool value) { settings.clear_use_e = value; });
            clear->add_checkbox("clear_wave_management", "Smart Wave Management", settings.clear_wave_management,
                [](bool value) { settings.clear_wave_management = value; });
            clear->add_slider_int("clear_min_minions_w", "Min Minions for W", 1, 6, 1, settings.clear_min_minions_w,
                [](int value) { settings.clear_min_minions_w = value; });
            clear->add_slider_float("clear_min_mana", "Minimum Mana %", 0.f, 100.f, 5.f, settings.clear_min_mana,
                [](float value) { settings.clear_min_mana = value; });
        }

        // Gap close settings
        auto gapclose = menu->add_sub_category("gapclose", "Gap Close Settings");
        if (gapclose) {
            gapclose->add_slider_float("e_max_range", "E Max Range", 500.f, 800.f, 25.f, settings.e_max_range,
                [](float value) { settings.e_max_range = value; });
            gapclose->add_checkbox("e_turret_dive", "Allow E Under Turret", settings.e_turret_dive,
                [](bool value) { settings.e_turret_dive = value; });
        }

        // Flee settings
        auto flee = menu->add_sub_category("flee", "Flee Settings");
        if (flee) {
            flee->add_checkbox("flee_use_e", "Use E to Escape", settings.flee_use_e,
                [](bool value) { settings.flee_use_e = value; });
            flee->add_checkbox("flee_use_w", "Use W to Slow", settings.flee_use_w,
                [](bool value) { settings.flee_use_w = value; });
            flee->add_checkbox("flee_use_r", "Use R Defensively", settings.flee_use_r,
                [](bool value) { settings.flee_use_r = value; });
            flee->add_checkbox("flee_r_only_surrounded", "R Only When Surrounded", settings.flee_r_only_surrounded,
                [](bool value) { settings.flee_r_only_surrounded = value; });
        }

        // Draw settings
        auto draw = menu->add_sub_category("draw", "Draw Settings");
        if (draw) {
            draw->add_checkbox("draw_w_range", "Draw W Range", settings.draw_w_range,
                [](bool value) { settings.draw_w_range = value; });
            draw->add_checkbox("draw_e_range", "Draw E Range", settings.draw_e_range,
                [](bool value) { settings.draw_e_range = value; });
            draw->add_checkbox("draw_r_range", "Draw R Range", settings.draw_r_range,
                [](bool value) { settings.draw_r_range = value; });
            draw->add_checkbox("draw_damage", "Draw Damage Indicator", settings.draw_damage,
                [](bool value) { settings.draw_damage = value; });
            draw->add_checkbox("draw_aa_reset_timing", "Draw AA Reset Timing", settings.draw_aa_reset_timing,
                [](bool value) { settings.draw_aa_reset_timing = value; });
        }

        // Debug option
        auto debug = menu->add_sub_category("debug", "Debug Settings");
        if (debug) {
            debug->add_checkbox("debug_enabled", "Enable Debug Logging", debug_enabled,
                [](bool value) { debug_enabled = value; });
        }
    }

    bool XinZhao::should_use_r_defensive() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        // Check if health is below threshold and enemies are nearby
        float health_percent = (player->get_hp() / player->get_max_hp()) * 100.f;
        if (health_percent <= settings.r_hp_threshold) {
            // Only use defensive R if there are enemies nearby
            if (count_enemies_in_range(450.f) > 0) {
                return true;
            }
        }
        return false;
    }

    bool XinZhao::is_valid_target(game_object* target, float extra_range) {
        if (!target || !target->is_valid() || target->is_dead() ||
            !target->is_targetable() || !target->is_visible()) {
            return false;
        }

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        float max_range = settings.e_max_range + extra_range;
        return target->get_position().distance(player->get_position()) <= max_range;
    }


    // Global event callbacks
    void __fastcall on_game_update() {
        xin_zhao.on_update();
    }

    void __fastcall on_draw_world() {
        xin_zhao.on_draw();
    }

    bool __fastcall before_attack_callback(orb_sdk::event_data* data) {
        return xin_zhao.on_before_attack(data);
    }

    void __fastcall on_buff_gain_callback(game_object* source, buff_instance* buff) {
        xin_zhao.on_buff_gain(source, buff);
    }

    void __fastcall on_buff_loss_callback(game_object* source, buff_instance* buff) {
        xin_zhao.on_buff_lose(source, buff);
    }

    void __fastcall on_process_cast_callback(game_object* sender, spell_cast* cast) {
        xin_zhao.on_process_spell_cast(sender, cast);
    }

    void load() {
        // Initialize required SDKs first
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

        // Register callbacks
        g_sdk->event_manager->register_callback(event_manager::event::game_update, on_game_update);
        g_sdk->event_manager->register_callback(event_manager::event::draw_world, on_draw_world);

        if (sdk::orbwalker) {
            sdk::orbwalker->register_callback(orb_sdk::before_attack, reinterpret_cast<void*>(before_attack_callback));
        }

        g_sdk->event_manager->register_callback(event_manager::event::buff_gain, reinterpret_cast<void*>(on_buff_gain_callback));
        g_sdk->event_manager->register_callback(event_manager::event::buff_loss, reinterpret_cast<void*>(on_buff_loss_callback));
        g_sdk->event_manager->register_callback(event_manager::event::process_cast, reinterpret_cast<void*>(on_process_cast_callback));

        // Initialize XinZhao instance
        xin_zhao.load();
    }

    void unload() {
        g_sdk->event_manager->unregister_callback(event_manager::event::game_update, on_game_update);
        g_sdk->event_manager->unregister_callback(event_manager::event::draw_world, on_draw_world);

        if (sdk::orbwalker) {
            sdk::orbwalker->unregister_callback(orb_sdk::before_attack, reinterpret_cast<void*>(before_attack_callback));
        }

        g_sdk->event_manager->unregister_callback(event_manager::event::buff_gain, reinterpret_cast<void*>(on_buff_gain_callback));
        g_sdk->event_manager->unregister_callback(event_manager::event::buff_loss, reinterpret_cast<void*>(on_buff_loss_callback));
        g_sdk->event_manager->unregister_callback(event_manager::event::process_cast, reinterpret_cast<void*>(on_process_cast_callback));

        xin_zhao.unload();
    }
} // namespace xinzhao