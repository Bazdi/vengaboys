//#include <Windows.h>
//#include "sdk.hpp"
//#include "varus.hpp"
//#include "ordermanager.hpp"
//#include <limits>
//#include <unordered_set>
//#include <algorithm>
//
//namespace varus {
//    Settings settings;
//    menu_category* menu = nullptr;
//    bool initialized = false;
//    bool debug_enabled = false;
//    CombatState combat_state{};
//    SpellTracker spell_tracker{};
//    CachedTarget cached_target{};
//
//    uint32_t infotab_text_id = 0;
//
//    // Varus buff hashes from the game
//    constexpr uint32_t Q_CHARGE_BUFF_HASH = 0x5c587d15;  // VarusQ
//    constexpr uint32_t Q_LAUNCH_BUFF_HASH = 0xba065456;  // VarusQLaunch
//    constexpr uint32_t W_STACK_BUFF_HASH = 0x934f9061;   // VarusWDebuff
//    constexpr uint32_t W_EMPOWERED_BUFF_HASH = 0xaf659cef; // varuswempowered
//    constexpr uint32_t E_SLOW_BUFF_HASH = 0x84e33d84;    // varuseslow
//    constexpr uint32_t R_ROOT_BUFF_HASH = 0x44e29b6c;    // varusrroot
//
//    void log_debug(const char* format, ...) {
//        if (!debug_enabled) return;
//
//        char buffer[512];
//        va_list args;
//        va_start(args, format);
//        vsnprintf(buffer, sizeof(buffer), format, args);
//        va_end(args);
//
//        g_sdk->log_console("[Varus Debug] %s", buffer);
//    }
//
//    bool can_cast_spell(int spell_slot, float delay) {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) {
//            log_debug("can_cast_spell: No player found");
//            return false;
//        }
//
//        auto spell = player->get_spell(spell_slot);
//        if (!spell) {
//            log_debug("can_cast_spell: No spell found for slot %d", spell_slot);
//            return false;
//        }
//
//        if (spell->get_cooldown() > 0.01f) {
//            log_debug("can_cast_spell: Spell on cooldown: %.2fs", spell->get_cooldown());
//            return false;
//        }
//
//        // Check if enough mana
//        float mana_cost = 0.f;
//        switch (spell_slot) {
//        case 0: // Q
//            mana_cost = player->get_max_par() * (settings.q_min_mana / 100.f);
//            break;
//        case 1: // W
//            mana_cost = 50.f;
//            break;
//        case 2: // E
//            mana_cost = player->get_max_par() * (settings.e_min_mana / 100.f);
//            break;
//        case 3: // R
//            mana_cost = player->get_max_par() * (settings.r_min_mana / 100.f);
//            break;
//        }
//
//        if (player->get_par() < mana_cost) {
//            log_debug("can_cast_spell: Not enough mana for spell %d", spell_slot);
//            return false;
//        }
//
//        if (!vengaboys::OrderManager::get_instance().can_cast(player, spell_slot, delay)) {
//            log_debug("can_cast_spell: OrderManager preventing cast");
//            return false;
//        }
//
//        return true;
//    }
//
//    void check_q_charge_status(game_object* player, float current_time) {
//        if (!player) return;
//
//        try {
//            // Safely check for Q charge buff
//            auto buff = player->get_buff_by_hash(Q_CHARGE_BUFF_HASH);
//            if (buff && buff->is_active()) {
//                if (!combat_state.q_charging) {
//                    combat_state.q_charging = true;
//                    combat_state.q_charge_start_time = buff->get_start_time();
//                    log_debug("Q charge started at time %.2f", combat_state.q_charge_start_time);
//                }
//            }
//            else if (combat_state.q_charging) {
//                // Don't check for launch buff yet - simplify for debugging
//                combat_state.q_charging = false;
//                combat_state.q_launching = false;
//                log_debug("Q charge ended");
//            }
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in check_q_charge_status: %s", e.what());
//            // Reset states to be safe
//            combat_state.q_charging = false;
//            combat_state.q_launching = false;
//        }
//    }
//
//    bool is_q_fully_charged(float current_time) {
//        if (!combat_state.q_charging) return false;
//
//        float charge_time = current_time - combat_state.q_charge_start_time;
//        return charge_time >= settings.q_charge_time;
//    }
//
//    float get_current_q_charge_percent(float current_time) {
//        if (!combat_state.q_charging) return 0.f;
//
//        float charge_time = current_time - combat_state.q_charge_start_time;
//        float ratio = charge_time / settings.q_charge_time;
//        return (ratio > 1.0f) ? 1.0f : ratio;
//    }
//
//    float calculate_q_damage(game_object* target, float charge_time) {
//        if (!target) return 0.f;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return 0.f;
//
//        auto spell = player->get_spell(0);
//        if (!spell || spell->get_level() == 0) return 0.f;
//
//        // Base damage values per level
//        const float base_damage[] = { 10.f, 45.f, 80.f, 115.f, 150.f };
//
//        // Bonus AD scaling (increases with charge time)
//        const float bonus_ad_ratios[] = { 0.8667f, 0.9333f, 1.0f, 1.0667f, 1.1333f };
//
//        const int level = spell->get_level() - 1;
//        const float base = base_damage[level];
//        const float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
//        const float ad_ratio = bonus_ad_ratios[level];
//
//        // Damage increases with charge time (up to 50% more at full charge)
//        float ratio = charge_time / settings.q_charge_time;
//        float capped_ratio = (ratio > 1.0f) ? 1.0f : ratio;
//        float charge_multiplier = 1.0f + capped_ratio * 0.5f;
//
//        float total_damage = (base + (bonus_ad * ad_ratio)) * charge_multiplier;
//
//        // Check if W is active (empowered abilities)
//        bool w_empowered = player->get_buff_by_hash(W_EMPOWERED_BUFF_HASH) != nullptr;
//        if (w_empowered) {
//            // W empowers abilities to do additional damage based on target's missing health
//            float missing_health_percent = 1.0f - (target->get_hp() / target->get_max_hp());
//            float w_level = player->get_spell(1)->get_level();
//            float w_damage_percent = 0.0f;
//
//            // W damage percentage scales with level (approximate values)
//            if (w_level == 1) w_damage_percent = 0.06f;  // 6% missing health at level 1
//            else if (w_level == 2) w_damage_percent = 0.075f;
//            else if (w_level == 3) w_damage_percent = 0.09f;
//            else if (w_level == 4) w_damage_percent = 0.105f;
//            else if (w_level == 5) w_damage_percent = 0.12f; // 12% missing health at level 5
//
//            float w_bonus_damage = target->get_max_hp() * missing_health_percent * w_damage_percent;
//            total_damage += w_bonus_damage;
//            log_debug("Adding W empowered damage to Q: %.1f", w_bonus_damage);
//        }
//
//        // Add damage from W stacks if present
//        int w_stacks = get_w_stacks(target);
//        if (w_stacks > 0) {
//            float w_stack_damage = calculate_w_damage(target, w_stacks);
//            total_damage += w_stack_damage;
//            log_debug("Adding W stack damage to Q: %.1f (stacks: %d)", w_stack_damage, w_stacks);
//        }
//
//        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, total_damage);
//    }
//
//    float calculate_w_damage(game_object* target, int stacks) {
//        if (!target) return 0.f;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return 0.f;
//
//        auto spell = player->get_spell(1);
//        if (!spell || spell->get_level() == 0) return 0.f;
//
//        // W passive on-hit magic damage
//        const float base_damage[] = { 6.f, 12.f, 18.f, 24.f, 30.f };
//        const float ap_ratio = 0.35f;
//
//        // W stack detonation (% max health)
//        const float stack_percent[] = { 3.f, 3.5f, 4.f, 4.5f, 5.f };
//        const float stack_ap_ratio = 0.015f; // 1.5% per 100 AP
//
//        const int level = spell->get_level() - 1;
//        float damage = 0.f;
//
//        // Stack damage if applicable
//        if (stacks > 0) {
//            float percent_damage = stack_percent[level] + (player->get_ability_power() * stack_ap_ratio);
//            damage += (target->get_max_hp() * percent_damage / 100.f) * stacks;
//        }
//
//        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, damage);
//    }
//
//    float calculate_e_damage(game_object* target) {
//        if (!target) return 0.f;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return 0.f;
//
//        auto spell = player->get_spell(2);
//        if (!spell || spell->get_level() == 0) return 0.f;
//
//        const float base_damage[] = { 60.f, 100.f, 140.f, 180.f, 220.f };
//        const float bonus_ad_ratio = 1.0f;
//
//        const int level = spell->get_level() - 1;
//        const float bonus_ad = player->get_attack_damage() - player->get_base_attack_damage();
//
//        float total_damage = base_damage[level] + (bonus_ad * bonus_ad_ratio);
//
//        // Check if W is active (empowered abilities)
//        bool w_empowered = player->get_buff_by_hash(W_EMPOWERED_BUFF_HASH) != nullptr;
//        if (w_empowered) {
//            // W empowers abilities to do additional damage based on target's missing health
//            float missing_health_percent = 1.0f - (target->get_hp() / target->get_max_hp());
//            float w_level = player->get_spell(1)->get_level();
//            float w_damage_percent = 0.0f;
//
//            // W damage percentage scales with level (approximate values)
//            if (w_level == 1) w_damage_percent = 0.06f;  // 6% missing health at level 1
//            else if (w_level == 2) w_damage_percent = 0.075f;
//            else if (w_level == 3) w_damage_percent = 0.09f;
//            else if (w_level == 4) w_damage_percent = 0.105f;
//            else if (w_level == 5) w_damage_percent = 0.12f; // 12% missing health at level 5
//
//            float w_bonus_damage = target->get_max_hp() * missing_health_percent * w_damage_percent;
//            total_damage += w_bonus_damage;
//            log_debug("Adding W empowered damage to E: %.1f", w_bonus_damage);
//        }
//
//        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, total_damage);
//    }
//
//    float calculate_r_damage(game_object* target) {
//        if (!target) return 0.f;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return 0.f;
//
//        auto spell = player->get_spell(3);
//        if (!spell || spell->get_level() == 0) return 0.f;
//
//        const float base_damage[] = { 150.f, 250.f, 350.f };
//        const float ap_ratio = 1.0f;
//
//        const int level = spell->get_level() - 1;
//
//        float total_damage = base_damage[level] + (player->get_ability_power() * ap_ratio);
//
//        return sdk::damage->calc_damage(dmg_sdk::damage_type::magical, player, target, total_damage);
//    }
//
//    float calculate_full_combo_damage(game_object* target) {
//        if (!target) return 0.f;
//
//        float total_damage = 0.f;
//
//        if (can_cast_spell(0, 0.f))
//            total_damage += calculate_q_damage(target, settings.q_charge_time); // Assuming full charge
//
//        if (can_cast_spell(1, 0.f)) {
//            int stacks = get_w_stacks(target);
//            total_damage += calculate_w_damage(target, stacks);
//        }
//
//        if (can_cast_spell(2, 0.f))
//            total_damage += calculate_e_damage(target);
//
//        if (can_cast_spell(3, 0.f))
//            total_damage += calculate_r_damage(target);
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (player) {
//            // Add basic attack damage
//            total_damage += sdk::damage->get_aa_damage(player, target);
//        }
//
//        return total_damage;
//    }
//
//    bool is_under_turret(const math::vector3& pos) {
//        for (auto turret : g_sdk->object_manager->get_turrets()) {
//            if (!turret->is_valid() || turret->is_dead())
//                continue;
//
//            auto player = g_sdk->object_manager->get_local_player();
//            if (!player) continue;
//
//            if (turret->get_team_id() != player->get_team_id() &&
//                pos.distance(turret->get_position()) <= 775.f) {
//                return true;
//            }
//        }
//        return false;
//    }
//
//    int get_w_stacks(game_object* target) {
//        if (!target) return 0;
//
//        auto buff = target->get_buff_by_hash(W_STACK_BUFF_HASH);
//        if (buff && buff->is_active()) {
//            return buff->get_stacks();
//        }
//
//        return 0; // No stacks found
//    }
//
//    bool is_target_cc_affected(game_object* target) {
//        if (!target) return false;
//
//        // Check if target has Varus R root
//        if (target->get_buff_by_hash(R_ROOT_BUFF_HASH)) {
//            return true;
//        }
//
//        // Check for other CC types
//        return target->has_buff_of_type(buff_type::stun) ||
//            target->has_buff_of_type(buff_type::snare) ||
//            target->has_buff_of_type(buff_type::charm) ||
//            target->has_buff_of_type(buff_type::taunt) ||
//            target->has_buff_of_type(buff_type::fear) ||
//            target->has_buff_of_type(buff_type::polymorph) ||
//            target->has_buff_of_type(buff_type::knockup);
//    }
//
//    bool is_target_slowed(game_object* target) {
//        if (!target) return false;
//
//        // Check if target has Varus E slow
//        if (target->get_buff_by_hash(E_SLOW_BUFF_HASH)) {
//            return true;
//        }
//
//        // Check for general slow
//        return target->has_buff_of_type(buff_type::slow);
//    }
//
//    bool start_charging_q() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) {
//            g_sdk->log_console("[Varus] start_charging_q: No player found");
//            return false;
//        }
//
//        auto spell = player->get_spell(0);
//        if (!spell) {
//            g_sdk->log_console("[Varus] start_charging_q: No spell found for slot 0");
//            return false;
//        }
//
//        if (spell->get_cooldown() > 0.f) {
//            g_sdk->log_console("[Varus] start_charging_q: Spell on cooldown: %.2fs", spell->get_cooldown());
//            return false;
//        }
//
//        if (player->get_par() < 70.f) { // Minimum mana check
//            g_sdk->log_console("[Varus] start_charging_q: Not enough mana: %.2f", player->get_par());
//            return false;
//        }
//
//        if (combat_state.q_charging) {
//            g_sdk->log_console("[Varus] start_charging_q: Already charging");
//            return false;
//        }
//
//        try {
//            g_sdk->log_console("[Varus] Starting to charge Q");
//
//            if (vengaboys::OrderManager::get_instance().cast_spell(0, player->get_position())) {
//                combat_state.last_q_time = g_sdk->clock_facade->get_game_time();
//                g_sdk->log_console("[Varus] Q charge started");
//                return true;
//            }
//            else {
//                g_sdk->log_console("[Varus] OrderManager Q charge start returned false");
//            }
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in start_charging_q: %s", e.what());
//        }
//        return false;
//    }
//
//    bool release_q(const math::vector3& position) {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) {
//            g_sdk->log_console("[Varus] release_q: No player found");
//            return false;
//        }
//
//        if (!combat_state.q_charging) {
//            g_sdk->log_console("[Varus] release_q: Q is not charging");
//            return false;
//        }
//
//        try {
//            g_sdk->log_console("[Varus] Releasing Q toward position %.1f, %.1f, %.1f",
//                position.x, position.y, position.z);
//
//            // For Q release we use the same slot (0)
//            if (vengaboys::OrderManager::get_instance().cast_spell(0, position)) {
//                combat_state.last_q_time = g_sdk->clock_facade->get_game_time();
//                g_sdk->log_console("[Varus] Q release successful");
//                return true;
//            }
//            else {
//                g_sdk->log_console("[Varus] OrderManager Q release returned false");
//            }
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in release_q: %s", e.what());
//        }
//        return false;
//    }
//
//    bool cast_w() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) {
//            g_sdk->log_console("[Varus] cast_w: No player found");
//            return false;
//        }
//
//        auto spell = player->get_spell(1);
//        if (!spell) {
//            g_sdk->log_console("[Varus] cast_w: No spell found for slot 1");
//            return false;
//        }
//
//        if (spell->get_cooldown() > 0.f) {
//            g_sdk->log_console("[Varus] cast_w: Spell on cooldown: %.2fs", spell->get_cooldown());
//            return false;
//        }
//
//        // Check if W is already active
//        if (player->get_buff_by_hash(W_EMPOWERED_BUFF_HASH)) {
//            g_sdk->log_console("[Varus] cast_w: W is already active");
//            return false;
//        }
//
//        try {
//            g_sdk->log_console("[Varus] Activating W");
//
//            if (vengaboys::OrderManager::get_instance().cast_spell(1, player->get_position())) {
//                combat_state.last_w_time = g_sdk->clock_facade->get_game_time();
//                spell_tracker.update_cooldown(1, g_sdk->clock_facade->get_game_time() + spell->get_cooldown());
//                g_sdk->log_console("[Varus] W activation successful");
//                return true;
//            }
//            else {
//                g_sdk->log_console("[Varus] OrderManager W cast returned false");
//            }
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in cast_w: %s", e.what());
//        }
//        return false;
//    }
//
//    bool cast_e(game_object* target) {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) {
//            g_sdk->log_console("[Varus] cast_e: No player found");
//            return false;
//        }
//
//        // If no target provided, try to find one automatically
//        if (!target) {
//            target = sdk::target_selector->get_hero_target([&](game_object* hero) {
//                if (!hero) return false;
//                return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
//                    hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.e_range;
//                });
//
//            if (!target) {
//                g_sdk->log_console("[Varus] cast_e: No valid target found within range");
//                return false;
//            }
//        }
//
//        auto spell = player->get_spell(2);
//        if (!spell) {
//            g_sdk->log_console("[Varus] cast_e: No spell found for slot 2");
//            return false;
//        }
//
//        if (spell->get_cooldown() > 0.f) {
//            g_sdk->log_console("[Varus] cast_e: Spell on cooldown: %.2fs", spell->get_cooldown());
//            return false;
//        }
//
//        if (target->get_position().distance(player->get_position()) > settings.e_range) {
//            g_sdk->log_console("[Varus] cast_e: Target out of range: %.1f > %.1f",
//                target->get_position().distance(player->get_position()), settings.e_range);
//            return false;
//        }
//
//        try {
//            // Use prediction for better accuracy
//            pred_sdk::spell_data spell_data{};
//            spell_data.spell_type = pred_sdk::spell_type::circular;
//            spell_data.targetting_type = pred_sdk::targetting_type::center;
//            spell_data.expected_hitchance = 60; // Reasonable hitchance
//            spell_data.source = player;
//            spell_data.source_position = player->get_position();
//            spell_data.spell_slot = 2;
//            spell_data.range = settings.e_range;
//            spell_data.radius = settings.e_radius;
//            spell_data.delay = settings.e_delay;
//
//            auto pred = sdk::prediction->predict(target, spell_data);
//            if (!pred.is_valid) {
//                g_sdk->log_console("[Varus] cast_e: Invalid prediction");
//                return false;
//            }
//
//            math::vector3 cast_position = pred.cast_position;
//            g_sdk->log_console("[Varus] Casting E on %s at position %.1f, %.1f, %.1f",
//                target->get_char_name().c_str(), cast_position.x, cast_position.y, cast_position.z);
//
//            if (vengaboys::OrderManager::get_instance().cast_spell(2, cast_position)) {
//                combat_state.last_e_time = g_sdk->clock_facade->get_game_time();
//                spell_tracker.update_cooldown(2, g_sdk->clock_facade->get_game_time() + spell->get_cooldown());
//                g_sdk->log_console("[Varus] E cast successful");
//                return true;
//            }
//            else {
//                g_sdk->log_console("[Varus] OrderManager E cast returned false");
//            }
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in cast_e: %s", e.what());
//        }
//        return false;
//    }
//
//    bool cast_r(game_object* target) {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) {
//            g_sdk->log_console("[Varus] cast_r: No player found");
//            return false;
//        }
//
//        // If no target provided, try to find one automatically
//        if (!target) {
//            target = sdk::target_selector->get_hero_target([&](game_object* hero) {
//                if (!hero) return false;
//                return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
//                    hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.r_range;
//                });
//
//            if (!target) {
//                g_sdk->log_console("[Varus] cast_r: No valid target found within range");
//                return false;
//            }
//        }
//
//        auto spell = player->get_spell(3);
//        if (!spell) {
//            g_sdk->log_console("[Varus] cast_r: No spell found for slot 3");
//            return false;
//        }
//
//        if (spell->get_cooldown() > 0.f) {
//            g_sdk->log_console("[Varus] cast_r: Spell on cooldown: %.2fs", spell->get_cooldown());
//            return false;
//        }
//
//        if (target->get_position().distance(player->get_position()) > settings.r_range) {
//            g_sdk->log_console("[Varus] cast_r: Target out of range: %.1f > %.1f",
//                target->get_position().distance(player->get_position()), settings.r_range);
//            return false;
//        }
//
//        try {
//            // Use prediction for skillshot
//            pred_sdk::spell_data spell_data{};
//            spell_data.spell_type = pred_sdk::spell_type::linear;
//            spell_data.targetting_type = pred_sdk::targetting_type::edge_to_edge;
//            spell_data.expected_hitchance = 70; // Higher hitchance for ultimate
//            spell_data.source = player;
//            spell_data.source_position = player->get_position();
//            spell_data.spell_slot = 3;
//            spell_data.range = settings.r_range;
//            spell_data.radius = settings.r_radius;
//            spell_data.delay = settings.r_delay;
//            spell_data.projectile_speed = settings.r_speed;
//            spell_data.forbidden_collisions = { pred_sdk::collision_type::yasuo_wall };
//
//            auto pred = sdk::prediction->predict(target, spell_data);
//            if (!pred.is_valid) {
//                g_sdk->log_console("[Varus] cast_r: Invalid prediction");
//                return false;
//            }
//
//            g_sdk->log_console("[Varus] Casting R on %s at position %.1f, %.1f, %.1f",
//                target->get_char_name().c_str(), pred.cast_position.x, pred.cast_position.y, pred.cast_position.z);
//
//            if (vengaboys::OrderManager::get_instance().cast_spell(3, pred.cast_position)) {
//                combat_state.last_r_time = g_sdk->clock_facade->get_game_time();
//                combat_state.r_active = true;
//                spell_tracker.update_cooldown(3, g_sdk->clock_facade->get_game_time() + spell->get_cooldown());
//                g_sdk->log_console("[Varus] R cast successful");
//                return true;
//            }
//            else {
//                g_sdk->log_console("[Varus] OrderManager R cast returned false");
//            }
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in cast_r: %s", e.what());
//        }
//        return false;
//    }
//
//    void handle_combo() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) {
//            g_sdk->log_console("[Varus] handle_combo: No player found");
//            return;
//        }
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        if (current_time - combat_state.last_combo_time < settings.min_spell_delay) {
//            return;
//        }
//
//        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
//            if (!hero) return false;
//            auto player = g_sdk->object_manager->get_local_player();
//            if (!player) return false;
//            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
//                hero->is_visible();
//            });
//
//        cached_target.target = target;
//
//        if (!cached_target.target) {
//            g_sdk->log_console("[Varus] handle_combo: No target found");
//            return;
//        }
//
//        g_sdk->log_console("[Varus] handle_combo: Target found: %s, distance: %.1f, Target HP: %.1f/%.1f (%.1f%%)",
//            target->get_char_name().c_str(),
//            player->get_position().distance(target->get_position()),
//            target->get_hp(),
//            target->get_max_hp(),
//            (target->get_hp() / target->get_max_hp()) * 100.0f);
//
//        bool spell_cast = false;
//
//        // Check if target is already affected by our R
//        bool target_rooted_by_r = target->get_buff_by_hash(R_ROOT_BUFF_HASH) != nullptr;
//        bool target_slowed_by_e = target->get_buff_by_hash(E_SLOW_BUFF_HASH) != nullptr;
//        bool w_is_active = player->get_buff_by_hash(W_EMPOWERED_BUFF_HASH) != nullptr;
//        int w_stacks = get_w_stacks(target);
//
//        g_sdk->log_console("[Varus] handle_combo: Target CC state - R root: %d, E slow: %d, W stacks: %d, W active: %d",
//            target_rooted_by_r ? 1 : 0, target_slowed_by_e ? 1 : 0, w_stacks, w_is_active ? 1 : 0);
//
//        // R logic - use when target is low or has W stacks for burst
//        if (settings.use_r && can_cast_spell(3, 0.f) && !combat_state.q_charging) {
//            // Check if target is in range
//            if (target->get_position().distance(player->get_position()) <= settings.r_range) {
//                bool should_cast_r = false;
//
//                // Use R for lethal kill
//                if (settings.r_use_on_killable) {
//                    float r_damage = calculate_r_damage(target);
//                    float e_damage = calculate_e_damage(target);
//                    float w_damage = calculate_w_damage(target, w_stacks);
//                    float aa_damage = sdk::damage->get_aa_damage(player, target);
//
//                    float combo_damage = r_damage + e_damage + w_damage + aa_damage;
//                    float lethal_threshold = target->get_hp() * 1.1f; // 10% buffer for shields/healing
//
//                    g_sdk->log_console("[Varus] Combo damage: %.1f, Target HP threshold: %.1f",
//                        combo_damage, lethal_threshold);
//
//                    if (combo_damage >= lethal_threshold) {
//                        g_sdk->log_console("[Varus] handle_combo: Target can be killed with R combo");
//                        should_cast_r = true;
//                    }
//                }
//
//                // Use R on CC'd targets
//                if (settings.r_use_on_cc && !should_cast_r) {
//                    // Check if target is under CC
//                    if (is_target_cc_affected(target)) {
//                        g_sdk->log_console("[Varus] handle_combo: Target is CC'd, using R");
//                        should_cast_r = true;
//                    }
//                }
//
//                // Use R if target has high W stacks and low health
//                if (!should_cast_r && w_stacks >= 2 &&
//                    (target->get_hp() / target->get_max_hp()) < 0.40f) {
//                    g_sdk->log_console("[Varus] handle_combo: Target has %d W stacks and low health, using R", w_stacks);
//                    should_cast_r = true;
//                }
//
//                if (should_cast_r) {
//                    spell_cast = cast_r(target);
//                    if (spell_cast) {
//                        g_sdk->log_console("[Varus] handle_combo: R cast successful");
//                    }
//                    else {
//                        g_sdk->log_console("[Varus] handle_combo: R cast failed");
//                    }
//                }
//            }
//        }
//
//        // If target is rooted by R, prioritize W active + E or Q to detonate stacks
//        if (target_rooted_by_r && !spell_cast) {
//            // Activate W if not already active
//            if (settings.use_w && can_cast_spell(1, 0.f) && !w_is_active) {
//                spell_cast = cast_w();
//                if (spell_cast) {
//                    g_sdk->log_console("[Varus] handle_combo: W activated while target rooted by R");
//                }
//            }
//
//            // Follow with E for AoE
//            if (settings.use_e && can_cast_spell(2, 0.f) && !spell_cast) {
//                if (target->get_position().distance(player->get_position()) <= settings.e_range) {
//                    spell_cast = cast_e(target);
//                    if (spell_cast) {
//                        g_sdk->log_console("[Varus] handle_combo: E cast on R-rooted target");
//                    }
//                }
//            }
//
//            // Or follow with Q if E isn't available
//            if (settings.use_q && can_cast_spell(0, 0.f) && !spell_cast) {
//                if (!combat_state.q_charging) {
//                    spell_cast = start_charging_q();
//                    if (spell_cast) {
//                        g_sdk->log_console("[Varus] handle_combo: Started charging Q on R-rooted target");
//                    }
//                }
//                else if (is_q_fully_charged(current_time)) {
//                    // Use prediction for skillshot
//                    pred_sdk::spell_data spell_data{};
//                    spell_data.spell_type = pred_sdk::spell_type::linear;
//                    spell_data.targetting_type = pred_sdk::targetting_type::edge_to_edge;
//                    spell_data.expected_hitchance = 70; // Higher hitchance for rooted target
//                    spell_data.source = player;
//                    spell_data.source_position = player->get_position();
//                    spell_data.spell_slot = 0;
//
//                    // Get current charge range
//                    float charge_percent = get_current_q_charge_percent(current_time);
//                    float current_range = settings.q_min_range + (settings.q_range - settings.q_min_range) * charge_percent;
//
//                    spell_data.range = current_range;
//                    spell_data.radius = settings.q_radius;
//                    spell_data.delay = settings.q_delay;
//                    spell_data.projectile_speed = settings.q_speed;
//
//                    auto pred = sdk::prediction->predict(target, spell_data);
//                    if (pred.is_valid) {
//                        spell_cast = release_q(pred.cast_position);
//                        if (spell_cast) {
//                            g_sdk->log_console("[Varus] handle_combo: Released Q on R-rooted target");
//                        }
//                    }
//                }
//            }
//        }
//
//        // W active logic - use for additional damage with W stacks
//        if (settings.use_w && !spell_cast && can_cast_spell(1, 0.f) && !combat_state.q_charging && !w_is_active) {
//            // Use W active when target has W stacks and is low
//            float target_hp_percent = (target->get_hp() / target->get_max_hp()) * 100.0f;
//
//            // Activate W if target has stacks or we're about to follow up with abilities
//            if ((w_stacks > 0 && target_hp_percent < settings.w_min_hp) ||
//                (can_cast_spell(0, 0.f) && can_cast_spell(2, 0.f))) {
//
//                g_sdk->log_console("[Varus] handle_combo: Target has %d W stacks and HP is below %.1f%%, activating W",
//                    w_stacks, settings.w_min_hp);
//
//                spell_cast = cast_w();
//                if (spell_cast) {
//                    g_sdk->log_console("[Varus] handle_combo: W activation successful");
//                    w_is_active = true; // Update for further decisions
//                }
//            }
//        }
//
//        // E logic - use when in range
//        if (settings.use_e && !spell_cast && can_cast_spell(2, 0.f) && !combat_state.q_charging) {
//            if (target->get_position().distance(player->get_position()) <= settings.e_range) {
//                // Prioritize E if W is active or target has enough stacks
//                if (w_is_active || w_stacks >= settings.e_min_stacks || !can_cast_spell(0, 0.f)) {
//                    g_sdk->log_console("[Varus] handle_combo: Target has %d W stacks, attempting E cast", w_stacks);
//                    spell_cast = cast_e(target);
//                    if (spell_cast) {
//                        g_sdk->log_console("[Varus] handle_combo: E cast successful");
//                    }
//                }
//            }
//        }
//
//        // Q logic - use for long range poke or to proc W stacks
//        if (settings.use_q && !spell_cast && can_cast_spell(0, 0.f)) {
//            if (combat_state.q_charging) {
//                // If we're charging, check if charge is full or target is in danger of escaping
//                bool should_release = is_q_fully_charged(current_time) ||
//                    target->get_position().distance(player->get_position()) > settings.q_range * 0.8f;
//
//                // Release earlier if target is CC'd for guaranteed hit
//                if (!should_release && (target_rooted_by_r || target_slowed_by_e || is_target_cc_affected(target))) {
//                    float charge_percent = get_current_q_charge_percent(current_time);
//                    if (charge_percent > 0.7f) { // 70% charged is good enough for CC'd targets
//                        should_release = true;
//                    }
//                }
//
//                if (should_release) {
//                    // Use prediction for skillshot
//                    pred_sdk::spell_data spell_data{};
//                    spell_data.spell_type = pred_sdk::spell_type::linear;
//                    spell_data.targetting_type = pred_sdk::targetting_type::edge_to_edge;
//
//                    // Adjust hitchance based on target state
//                    if (target_rooted_by_r || is_target_cc_affected(target)) {
//                        spell_data.expected_hitchance = pred_sdk::hitchance::very_high;
//                    }
//                    else if (target_slowed_by_e) {
//                        spell_data.expected_hitchance = pred_sdk::hitchance::high;
//                    }
//                    else {
//                        spell_data.expected_hitchance = pred_sdk::hitchance::medium;
//                    }
//
//                    spell_data.source = player;
//                    spell_data.source_position = player->get_position();
//                    spell_data.spell_slot = 0;
//
//                    // Get current charge range
//                    float charge_percent = get_current_q_charge_percent(current_time);
//                    float current_range = settings.q_min_range + (settings.q_range - settings.q_min_range) * charge_percent;
//
//                    spell_data.range = current_range;
//                    spell_data.radius = settings.q_radius;
//                    spell_data.delay = settings.q_delay;
//                    spell_data.projectile_speed = settings.q_speed;
//                    spell_data.forbidden_collisions = { pred_sdk::collision_type::yasuo_wall, pred_sdk::collision_type::hero };
//
//                    auto pred = sdk::prediction->predict(target, spell_data);
//                    if (pred.is_valid) {
//                        spell_cast = release_q(pred.cast_position);
//                        if (spell_cast) {
//                            g_sdk->log_console("[Varus] handle_combo: Q release successful");
//                        }
//                    }
//                }
//            }
//            else {
//                // Start charging Q if target has enough W stacks, W is active, or we're in a good position
//                bool should_charge = w_stacks >= settings.q_min_stacks ||
//                    w_is_active ||
//                    target->get_position().distance(player->get_position()) > player->get_attack_range();
//
//                // Also charge if E was just applied (for combo) or target is CC'd
//                if (!should_charge && (target_slowed_by_e || is_target_cc_affected(target))) {
//                    should_charge = true;
//                }
//
//                if (should_charge) {
//                    g_sdk->log_console("[Varus] handle_combo: Target has %d W stacks, W active: %d, starting Q charge",
//                        w_stacks, w_is_active ? 1 : 0);
//
//                    spell_cast = start_charging_q();
//                    if (spell_cast) {
//                        g_sdk->log_console("[Varus] handle_combo: Q charge started");
//                    }
//                }
//            }
//        }
//
//        if (spell_cast) {
//            combat_state.last_combo_time = current_time;
//        }
//    }
//
//    void handle_harass() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->get_par() < settings.harass_min_mana / 100.f * player->get_max_par()) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        if (current_time - combat_state.last_harass_time < settings.min_spell_delay) {
//            return;
//        }
//
//        bool spell_cast = false;
//
//        game_object* target = sdk::target_selector->get_hero_target([&](game_object* hero) {
//            if (!hero) return false;
//            auto player = g_sdk->object_manager->get_local_player();
//            if (!player) return false;
//            return hero->is_valid() && !hero->is_dead() && hero->is_targetable() &&
//                hero->is_visible() && hero->get_position().distance(player->get_position()) <= settings.q_range;
//            });
//
//        if (!target) {
//            g_sdk->log_console("[Varus] handle_harass: No target found in range");
//            return;
//        }
//
//        // Check target state
//        bool target_slowed_by_e = target->get_buff_by_hash(E_SLOW_BUFF_HASH) != nullptr;
//        int w_stacks = get_w_stacks(target);
//
//        // For harass, we primarily use Q
//        if (settings.harass_use_q && can_cast_spell(0, 0.f)) {
//            if (!settings.harass_under_turret || !is_under_turret(target->get_position())) {
//                if (combat_state.q_charging) {
//                    // If we're charging, check if charge is full or target is about to escape
//                    bool should_release = is_q_fully_charged(current_time) ||
//                        target->get_position().distance(player->get_position()) > settings.q_range * 0.8f;
//
//                    // Release earlier if target is slowed by E
//                    if (!should_release && target_slowed_by_e) {
//                        float charge_percent = get_current_q_charge_percent(current_time);
//                        if (charge_percent > 0.6f) { // 60% charged is good enough for slowed targets
//                            should_release = true;
//                        }
//                    }
//
//                    if (should_release) {
//                        // Use prediction for skillshot
//                        pred_sdk::spell_data spell_data{};
//                        spell_data.spell_type = pred_sdk::spell_type::linear;
//                        spell_data.targetting_type = pred_sdk::targetting_type::edge_to_edge;
//                        spell_data.expected_hitchance = target_slowed_by_e ?
//                            pred_sdk::hitchance::high : pred_sdk::hitchance::medium;
//                        spell_data.source = player;
//                        spell_data.source_position = player->get_position();
//                        spell_data.spell_slot = 0;
//
//                        // Get current charge range
//                        float charge_percent = get_current_q_charge_percent(current_time);
//                        float current_range = settings.q_min_range + (settings.q_range - settings.q_min_range) * charge_percent;
//
//                        spell_data.range = current_range;
//                        spell_data.radius = settings.q_radius;
//                        spell_data.delay = settings.q_delay;
//                        spell_data.projectile_speed = settings.q_speed;
//                        spell_data.forbidden_collisions = { pred_sdk::collision_type::yasuo_wall, pred_sdk::collision_type::hero };
//
//                        auto pred = sdk::prediction->predict(target, spell_data);
//                        if (pred.is_valid) {
//                            spell_cast = release_q(pred.cast_position);
//                        }
//                    }
//                }
//                else {
//                    // Start charging Q - check mana more conservatively in harass mode
//                    if (player->get_par() > player->get_max_par() * 0.35f) {
//                        spell_cast = start_charging_q();
//                    }
//                }
//
//                if (spell_cast) {
//                    g_sdk->log_console("[Varus] handle_harass: Q action successful");
//                }
//            }
//            else {
//                g_sdk->log_console("[Varus] handle_harass: Target under turret, skipping Q");
//            }
//        }
//
//        // Use E in harass if target is close enough and setting enabled
//        if (!spell_cast && settings.harass_use_e && can_cast_spell(2, 0.f) && !combat_state.q_charging) {
//            if (target->get_position().distance(player->get_position()) <= settings.e_range) {
//                if (!settings.harass_under_turret || !is_under_turret(target->get_position())) {
//                    // Prioritize E if target has W stacks for additional damage
//                    if (w_stacks > 0) {
//                        g_sdk->log_console("[Varus] handle_harass: Target has %d W stacks, prioritizing E", w_stacks);
//                    }
//
//                    g_sdk->log_console("[Varus] handle_harass: Attempting E cast on %s", target->get_char_name().c_str());
//                    spell_cast = cast_e(target);
//                    if (spell_cast) {
//                        g_sdk->log_console("[Varus] handle_harass: E cast successful");
//                    }
//                }
//                else {
//                    g_sdk->log_console("[Varus] handle_harass: Target under turret, skipping E");
//                }
//            }
//        }
//
//        if (spell_cast) {
//            combat_state.last_harass_time = current_time;
//        }
//    }
//
//    void handle_farming() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->get_par() < settings.farm_min_mana / 100.f * player->get_max_par()) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        bool spell_farm_enabled = sdk::orbwalker->is_spell_farm_enabled();
//        bool spell_farm_lasthit_enabled = sdk::orbwalker->is_spell_farm_lasthit_enabled();
//
//        if (!spell_farm_enabled && !spell_farm_lasthit_enabled) {
//            return;
//        }
//
//        // Release Q if it's charging and we're farming
//        if (combat_state.q_charging) {
//            // Find best position to hit multiple minions
//            math::vector3 best_pos;
//            int max_hits = 0;
//
//            for (auto minion : g_sdk->object_manager->get_minions()) {
//                if (!minion->is_valid() || minion->is_dead() || minion->get_team_id() == player->get_team_id())
//                    continue;
//
//                if (minion->get_position().distance(player->get_position()) <= settings.q_range) {
//                    // Count minions in a line
//                    int hits = 0;
//                    math::vector3 direction = (minion->get_position() - player->get_position()).normalized();
//
//                    for (auto other_minion : g_sdk->object_manager->get_minions()) {
//                        if (!other_minion->is_valid() || other_minion->is_dead() ||
//                            other_minion->get_team_id() == player->get_team_id())
//                            continue;
//
//                        // Check if minion is in line with the direction
//                        math::vector3 to_other = other_minion->get_position() - player->get_position();
//                        float dot = direction.dot(to_other.normalized());
//
//                        if (dot > 0.8f && to_other.magnitude() <= settings.q_range) {
//                            hits++;
//                        }
//                    }
//
//                    if (hits > max_hits) {
//                        max_hits = hits;
//                        best_pos = minion->get_position();
//                    }
//                }
//            }
//
//            if (max_hits >= settings.farm_min_minions_q || is_q_fully_charged(current_time)) {
//                g_sdk->log_console("[Varus] handle_farming: Releasing Q to hit %d minions", max_hits);
//                release_q(best_pos);
//                return;
//            }
//        }
//
//        // Q for wave clear - hit multiple minions in line
//        if (settings.farm_use_q && can_cast_spell(0, 0.f) &&
//            (current_time - combat_state.last_farm_q_time >= 0.2f) &&
//            !combat_state.q_charging) {
//
//            if (spell_farm_enabled) {
//                int minions_in_q_range = 0;
//
//                for (auto minion : g_sdk->object_manager->get_minions()) {
//                    if (!minion->is_valid() || minion->is_dead() || minion->get_team_id() == player->get_team_id())
//                        continue;
//
//                    if (minion->get_position().distance(player->get_position()) <= settings.q_range) {
//                        minions_in_q_range++;
//                    }
//                }
//
//                if (minions_in_q_range >= settings.farm_min_minions_q) {
//                    g_sdk->log_console("[Varus] handle_farming: Starting Q charge for farming");
//                    if (start_charging_q()) {
//                        combat_state.last_farm_q_time = current_time;
//                    }
//                }
//            }
//            else if (spell_farm_lasthit_enabled) {
//                // Use Q for last hits of out-of-range minions
//                for (auto minion : g_sdk->object_manager->get_minions()) {
//                    if (!minion->is_valid() || minion->is_dead() ||
//                        minion->get_team_id() == player->get_team_id())
//                        continue;
//
//                    float distance = minion->get_position().distance(player->get_position());
//                    if (distance > player->get_attack_range() && distance <= settings.q_range) {
//                        // Check if Q can kill the minion
//                        float q_damage = calculate_q_damage(minion, 0.5f); // Half charge for quick lasthit
//                        float predicted_health = sdk::health_prediction ?
//                            sdk::health_prediction->get_predicted_health(minion, 1.0f) : minion->get_hp();
//
//                        if (q_damage >= predicted_health) {
//                            g_sdk->log_console("[Varus] handle_farming: Starting Q charge for last hit");
//                            if (start_charging_q()) {
//                                combat_state.last_farm_q_time = current_time;
//                                break;
//                            }
//                        }
//                    }
//                }
//            }
//        }
//
//        // E for AoE farming
//        if (settings.farm_use_e && can_cast_spell(2, 0.f) && !combat_state.q_charging) {
//            if (spell_farm_enabled) {
//                // Find best position to hit multiple minions with E
//                math::vector3 best_pos;
//                int max_hits = 0;
//
//                for (auto minion : g_sdk->object_manager->get_minions()) {
//                    if (!minion->is_valid() || minion->is_dead() || minion->get_team_id() == player->get_team_id())
//                        continue;
//
//                    if (minion->get_position().distance(player->get_position()) <= settings.e_range) {
//                        // Count minions in E radius
//                        int hits = 0;
//
//                        for (auto other_minion : g_sdk->object_manager->get_minions()) {
//                            if (!other_minion->is_valid() || other_minion->is_dead() ||
//                                other_minion->get_team_id() == player->get_team_id())
//                                continue;
//
//                            if (other_minion->get_position().distance(minion->get_position()) <= settings.e_radius) {
//                                hits++;
//                            }
//                        }
//
//                        if (hits > max_hits) {
//                            max_hits = hits;
//                            best_pos = minion->get_position();
//                        }
//                    }
//                }
//
//                if (max_hits >= settings.farm_min_minions_q) {
//                    g_sdk->log_console("[Varus] handle_farming: Casting E to hit %d minions", max_hits);
//                    if (vengaboys::OrderManager::get_instance().cast_spell(2, best_pos)) {
//                        combat_state.last_e_time = current_time;
//                    }
//                }
//            }
//        }
//    }
//
//    void create_menu() {
//        menu = g_sdk->menu_manager->add_category("varus", "Varus");
//        if (!menu) return;
//
//        auto combat = menu->add_sub_category("combat", "Combat Settings");
//        if (combat) {
//            combat->add_checkbox("use_q", "Q - Piercing Arrow", settings.use_q, [&](bool value) { settings.use_q = value; });
//            combat->add_checkbox("use_w", "W - Blighted Quiver", settings.use_w, [&](bool value) { settings.use_w = value; });
//            combat->add_checkbox("use_e", "E - Hail of Arrows", settings.use_e, [&](bool value) { settings.use_e = value; });
//            combat->add_checkbox("use_r", "R - Chain of Corruption", settings.use_r, [&](bool value) { settings.use_r = value; });
//
//            auto q_settings = combat->add_sub_category("q_settings", "Q - Piercing Arrow Settings");
//            if (q_settings) {
//                q_settings->add_slider_float("q_range", "Range", 825.f, 1600.f, 25.f, settings.q_range,
//                    [&](float value) { settings.q_range = value; });
//                q_settings->add_slider_float("q_min_range", "Min Range", 500.f, 1000.f, 25.f, settings.q_min_range,
//                    [&](float value) { settings.q_min_range = value; });
//                q_settings->add_checkbox("q_prediction", "Use Prediction", settings.q_prediction,
//                    [&](bool value) { settings.q_prediction = value; });
//                q_settings->add_slider_float("q_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.q_min_mana,
//                    [&](float value) { settings.q_min_mana = value; });
//                q_settings->add_slider_int("q_min_stacks", "Min W Stacks", 0, 3, 1, settings.q_min_stacks,
//                    [&](int value) { settings.q_min_stacks = value; });
//            }
//
//            auto w_settings = combat->add_sub_category("w_settings", "W - Blighted Quiver Settings");
//            if (w_settings) {
//                w_settings->add_combo("w_mode", "Active Usage",
//                    { "When Target HP < x%", "Never" }, settings.w_mode,
//                    [&](int value) { settings.w_mode = value; });
//                w_settings->add_slider_float("w_min_hp", "Target HP Threshold %", 10.f, 80.f, 1.f, settings.w_min_hp,
//                    [&](float value) { settings.w_min_hp = value; });
//            }
//
//            auto e_settings = combat->add_sub_category("e_settings", "E - Hail of Arrows Settings");
//            if (e_settings) {
//                e_settings->add_slider_float("e_range", "Range", 500.f, 1000.f, 25.f, settings.e_range,
//                    [&](float value) { settings.e_range = value; });
//                e_settings->add_slider_float("e_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.e_min_mana,
//                    [&](float value) { settings.e_min_mana = value; });
//                e_settings->add_slider_int("e_min_stacks", "Min W Stacks", 0, 3, 1, settings.e_min_stacks,
//                    [&](int value) { settings.e_min_stacks = value; });
//            }
//
//            auto r_settings = combat->add_sub_category("r_settings", "R - Chain of Corruption Settings");
//            if (r_settings) {
//                r_settings->add_slider_float("r_range", "Range", 500.f, 1500.f, 25.f, settings.r_range,
//                    [&](float value) { settings.r_range = value; });
//                r_settings->add_checkbox("r_use_on_cc", "Use on CC'd Target", settings.r_use_on_cc,
//                    [&](bool value) { settings.r_use_on_cc = value; });
//                r_settings->add_checkbox("r_use_on_killable", "Use on Killable Target", settings.r_use_on_killable,
//                    [&](bool value) { settings.r_use_on_killable = value; });
//                r_settings->add_slider_float("r_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.r_min_mana,
//                    [&](float value) { settings.r_min_mana = value; });
//            }
//        }
//
//        auto harass = menu->add_sub_category("harass", "Harass Settings");
//        if (harass) {
//            harass->add_checkbox("harass_use_q", "Use Q", settings.harass_use_q,
//                [&](bool value) { settings.harass_use_q = value; });
//            harass->add_checkbox("harass_use_e", "Use E", settings.harass_use_e,
//                [&](bool value) { settings.harass_use_e = value; });
//            harass->add_slider_float("harass_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.harass_min_mana,
//                [&](float value) { settings.harass_min_mana = value; });
//            harass->add_checkbox("harass_under_turret", "Harass Under Turret", settings.harass_under_turret,
//                [&](bool value) { settings.harass_under_turret = value; });
//        }
//
//        auto farm = menu->add_sub_category("farm", "Farm Settings");
//        if (farm) {
//            farm->add_checkbox("farm_use_q", "Use Q", settings.farm_use_q,
//                [&](bool value) { settings.farm_use_q = value; });
//            farm->add_checkbox("farm_use_e", "Use E", settings.farm_use_e,
//                [&](bool value) { settings.farm_use_e = value; });
//            farm->add_slider_float("farm_min_mana", "Minimum Mana %", 0.f, 100.f, 1.f, settings.farm_min_mana,
//                [&](float value) { settings.farm_min_mana = value; });
//            farm->add_slider_int("farm_min_minions_q", "Min Minions for Q", 1, 6, 1, settings.farm_min_minions_q,
//                [&](int value) { settings.farm_min_minions_q = value; });
//            farm->add_checkbox("farm_under_turret", "Farm Under Turret", settings.farm_under_turret,
//                [&](bool value) { settings.farm_under_turret = value; });
//        }
//
//        auto drawings = menu->add_sub_category("drawings", "Drawing Settings");
//        if (drawings) {
//            drawings->add_checkbox("draw_ranges", "Draw Ranges", settings.draw_ranges,
//                [&](bool value) { settings.draw_ranges = value; });
//            drawings->add_checkbox("draw_damage", "Draw Damage", settings.draw_damage,
//                [&](bool value) { settings.draw_damage = value; });
//            drawings->add_checkbox("draw_q_charge", "Draw Q Charge", settings.draw_q_charge,
//                [&](bool value) { settings.draw_q_charge = value; });
//            drawings->add_checkbox("draw_w_stacks", "Draw W Stacks", settings.draw_w_stacks,
//                [&](bool value) { settings.draw_w_stacks = value; });
//        }
//
//        auto misc = menu->add_sub_category("misc", "Misc Settings");
//        if (misc) {
//            misc->add_checkbox("debug_mode", "Debug Mode", debug_enabled,
//                [&](bool value) {
//                    debug_enabled = value;
//                    settings.debug_enabled = value;
//                    g_sdk->log_console("[Varus] Debug mode %s", value ? "ENABLED" : "DISABLED");
//                });
//        }
//    }
//
//    bool __fastcall before_attack(orb_sdk::event_data* data) {
//        if (!initialized) return true;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || !data || !data->target) return true;
//
//        // Don't attack while Q is charging
//        if (combat_state.q_charging) {
//            return false;
//        }
//
//        // Add W stack tracking - prioritize heroes based on stacks
//        if (data->target->is_hero()) {
//            game_object* target = data->target;
//            int current_stacks = get_w_stacks(target);
//
//            // If target already has 3 stacks, consider using an ability instead
//            if (current_stacks >= 3 && settings.use_w) {
//                // If W is active, a spell would be better to proc the W stacks
//                bool w_is_active = player->get_buff_by_hash(W_EMPOWERED_BUFF_HASH) != nullptr;
//                if (w_is_active && (can_cast_spell(0, 0.f) || can_cast_spell(2, 0.f))) {
//                    g_sdk->log_console("[Varus] Target has max W stacks and W is active, prefer ability over AA");
//                    return false;
//                }
//            }
//        }
//
//        return true;
//    }
//
//    void __fastcall on_draw() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return;
//
//        if (settings.draw_ranges) {
//            // Draw Q range (use min and max based on charge)
//            if (combat_state.q_charging) {
//                float current_time = g_sdk->clock_facade->get_game_time();
//                float charge_percent = get_current_q_charge_percent(current_time);
//                float current_range = settings.q_min_range + (settings.q_range - settings.q_min_range) * charge_percent;
//
//                g_sdk->renderer->add_circle_3d(player->get_position(), current_range, 2.f, 0x60BBBBFF);
//            }
//            else {
//                g_sdk->renderer->add_circle_3d(player->get_position(), settings.q_range, 2.f, 0x20BBBBFF);
//                g_sdk->renderer->add_circle_3d(player->get_position(), settings.q_min_range, 1.f, 0x20FFFFFF);
//            }
//
//            // Draw E range
//            g_sdk->renderer->add_circle_3d(player->get_position(), settings.e_range, 2.f, 0x20BB00FF);
//
//            // Draw R range
//            g_sdk->renderer->add_circle_3d(player->get_position(), settings.r_range, 2.f, 0x20FF00FF);
//        }
//
//        // Draw Q charge state
//        if (settings.draw_q_charge && combat_state.q_charging) {
//            float current_time = g_sdk->clock_facade->get_game_time();
//            float charge_percent = get_current_q_charge_percent(current_time);
//
//            math::vector2 screen_pos = {
//                static_cast<float>(g_sdk->renderer->get_window_width()) * 0.5f,
//                static_cast<float>(g_sdk->renderer->get_window_height()) * 0.85f
//            };
//
//            char buffer[100];
//            sprintf(buffer, "Q Charge: %.0f%%", charge_percent * 100.0f);
//
//            g_sdk->renderer->add_text(buffer, 20.f, screen_pos, 1, 0xFFFFFFFF);
//        }
//
//        // Draw W stacks and empowered state on enemies
//        if (settings.draw_w_stacks) {
//            bool player_w_active = player->get_buff_by_hash(W_EMPOWERED_BUFF_HASH) != nullptr;
//
//            for (auto enemy : g_sdk->object_manager->get_heroes()) {
//                if (!enemy->is_valid() || enemy->is_dead() ||
//                    enemy->get_team_id() == player->get_team_id())
//                    continue;
//
//                int stacks = get_w_stacks(enemy);
//                if (stacks > 0) {
//                    math::vector2 hp_pos = enemy->get_health_bar_position();
//
//                    char buffer[10];
//                    sprintf(buffer, "W:%d", stacks);
//
//                    // Color based on stack count and W active status
//                    uint32_t stack_color = 0xFFFFFF00; // Yellow default
//                    if (stacks == 3) stack_color = 0xFFFF0000; // Red for max stacks
//                    if (player_w_active && stacks > 0) stack_color = 0xFF00FF00; // Green when W is active
//
//                    g_sdk->renderer->add_text(buffer, 14.f,
//                        math::vector2(hp_pos.x + 100, hp_pos.y), 0, stack_color);
//                }
//
//                // Show if target is rooted by R
//                if (enemy->get_buff_by_hash(R_ROOT_BUFF_HASH)) {
//                    math::vector2 hp_pos = enemy->get_health_bar_position();
//                    g_sdk->renderer->add_text("R", 14.f,
//                        math::vector2(hp_pos.x + 70, hp_pos.y), 0, 0xFFFF00FF); // Purple for R root
//                }
//
//                // Show if target is slowed by E
//                if (enemy->get_buff_by_hash(E_SLOW_BUFF_HASH)) {
//                    math::vector2 hp_pos = enemy->get_health_bar_position();
//                    g_sdk->renderer->add_text("E", 14.f,
//                        math::vector2(hp_pos.x + 85, hp_pos.y), 0, 0xFF00FFFF); // Cyan for E slow
//                }
//            }
//        }
//
//        if (settings.draw_damage) {
//            for (auto enemy : g_sdk->object_manager->get_heroes()) {
//                if (!enemy->is_valid() || enemy->is_dead() ||
//                    enemy->get_team_id() == player->get_team_id())
//                    continue;
//
//                float total_damage = calculate_full_combo_damage(enemy);
//                g_sdk->renderer->add_damage_indicator(enemy, total_damage);
//            }
//        }
//    }
//
//    void __fastcall on_update() {
//        g_sdk->log_console("[Varus] on_update - START"); // Function start log
//
//        if (!initialized) {
//            g_sdk->log_console("[Varus] on_update - Not Initialized - END"); // Early exit log
//            g_sdk->log_console("[Varus] on_update - END"); // Function end log
//            return;
//        }
//
//        g_sdk->log_console("[Varus] on_update - After Initialized Check"); // After init check
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) {
//            g_sdk->log_console("[Varus] on_update - No Player or Dead - END"); // Player dead/null exit log
//            g_sdk->log_console("[Varus] on_update - END"); // Function end log
//            return;
//        }
//        g_sdk->log_console("[Varus] on_update - After Player Check"); // After player check
//
//
//        float current_time = 0.f;
//        try {
//            current_time = g_sdk->clock_facade->get_game_time();
//            g_sdk->log_console("[Varus] on_update - After get_game_time"); // After get_game_time
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception getting game time: %s", e.what());
//            g_sdk->log_console("[Varus] on_update - Exception get_game_time - END"); // Exception log
//            g_sdk->log_console("[Varus] on_update - END"); // Function end log
//            return;
//        }
//
//        g_sdk->log_console("[Varus] on_update - Before check_q_charge_status"); // Before check_q_charge_status
//        check_q_charge_status(player, current_time);
//        g_sdk->log_console("[Varus] on_update - After check_q_charge_status"); // After check_q_charge_status
//
//        if (settings.debug_enabled) {
//            g_sdk->log_console("[Varus] on_update - Before Debug Section"); // Before debug section
//            try {
//                g_sdk->log_console("[Varus] on_update - Inside Debug Try"); // Inside debug try
//                // Debug logging code - removed for now to simplify
//                g_sdk->log_console("[Varus] on_update - Inside Debug Try - After Debug code"); // Inside debug try - after debug code
//
//            }
//            catch (const std::exception& e) {
//                g_sdk->log_console("[!] Exception in debug logging: %s", e.what());
//            }
//            g_sdk->log_console("[Varus] on_update - After Debug Section"); // After debug section
//        }
//
//        if (sdk::orbwalker) {
//            g_sdk->log_console("[Varus] on_update - Before Orbwalker Section"); // Before orbwalker section
//            try {
//                g_sdk->log_console("[Varus] on_update - Inside Orbwalker Try"); // Inside orbwalker try
//                // Orbwalker mode handling code - removed for now to simplify
//                g_sdk->log_console("[Varus] on_update - Inside Orbwalker Try - After Orbwalker code"); // Inside orbwalker try - after orbwalker code
//
//            }
//            catch (const std::exception& e) {
//                g_sdk->log_console("[!] Exception checking orbwalker modes: %s", e.what());
//            }
//            g_sdk->log_console("[Varus] on_update - After Orbwalker Section"); // After orbwalker section
//        }
//        else {
//            g_sdk->log_console("[Varus] on_update - Orbwalker SDK Null - Skipped Section"); // Orbwalker sdk null
//        }
//
//
//        // Infotab update section - removed for now to simplify
//
//        g_sdk->log_console("[Varus] on_update - END"); // Function end log
//    }
//
//    void __fastcall on_buff_gain(game_object* source, buff_instance* buff) {
//        if (!initialized || !source || !buff) return;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return;
//
//        std::string buff_name = buff->get_name();
//        uint32_t buff_hash = buff->get_hash();
//
//        // Handle Q charge buff
//        if (source == player && buff_hash == Q_CHARGE_BUFF_HASH) {
//            combat_state.q_charging = true;
//            combat_state.q_charge_start_time = buff->get_start_time();
//            g_sdk->log_console("[Varus] on_buff_gain: Q charge started");
//        }
//
//        // Handle Q launch buff
//        if (source == player && buff_hash == Q_LAUNCH_BUFF_HASH) {
//            combat_state.q_launching = true;
//            g_sdk->log_console("[Varus] on_buff_gain: Q launched");
//        }
//
//        // Handle W active buff
//        if (source == player && buff_hash == W_EMPOWERED_BUFF_HASH) {
//            g_sdk->log_console("[Varus] on_buff_gain: W activated (abilities empowered)");
//        }
//
//        // Track W stacks on enemies
//        if (source->get_team_id() != player->get_team_id() && buff_hash == W_STACK_BUFF_HASH) {
//            combat_state.hero_w_stacks[source->get_id()] = buff->get_stacks();
//            g_sdk->log_console("[Varus] on_buff_gain: Enemy %s has %d W stacks",
//                source->get_char_name().c_str(), buff->get_stacks());
//        }
//
//        // Track R roots on enemies
//        if (source->get_team_id() != player->get_team_id() && buff_hash == R_ROOT_BUFF_HASH) {
//            g_sdk->log_console("[Varus] on_buff_gain: Enemy %s is rooted by R",
//                source->get_char_name().c_str());
//        }
//
//        // Track E slows on enemies
//        if (source->get_team_id() != player->get_team_id() && buff_hash == E_SLOW_BUFF_HASH) {
//            g_sdk->log_console("[Varus] on_buff_gain: Enemy %s is slowed by E",
//                source->get_char_name().c_str());
//        }
//    }
//
//    void __fastcall on_buff_loss(game_object* source, buff_instance* buff) {
//        if (!initialized || !source || !buff) return;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return;
//
//        uint32_t buff_hash = buff->get_hash();
//
//        // Handle Q charge end
//        if (source == player && buff_hash == Q_CHARGE_BUFF_HASH) {
//            combat_state.q_charging = false;
//            g_sdk->log_console("[Varus] on_buff_loss: Q charge ended");
//        }
//
//        // Handle Q launch end
//        if (source == player && buff_hash == Q_LAUNCH_BUFF_HASH) {
//            combat_state.q_launching = false;
//            g_sdk->log_console("[Varus] on_buff_loss: Q launch ended");
//        }
//
//        // Handle W active end
//        if (source == player && buff_hash == W_EMPOWERED_BUFF_HASH) {
//            g_sdk->log_console("[Varus] on_buff_loss: W deactivated");
//        }
//
//        // Track W stacks on enemies
//        if (source->get_team_id() != player->get_team_id() && buff_hash == W_STACK_BUFF_HASH) {
//            combat_state.hero_w_stacks[source->get_id()] = 0;
//            g_sdk->log_console("[Varus] on_buff_loss: Enemy %s lost W stacks",
//                source->get_char_name().c_str());
//        }
//
//        // Track R roots on enemies
//        if (source->get_team_id() != player->get_team_id() && buff_hash == R_ROOT_BUFF_HASH) {
//            g_sdk->log_console("[Varus] on_buff_loss: Enemy %s is no longer rooted by R",
//                source->get_char_name().c_str());
//        }
//
//        // Track E slows on enemies
//        if (source->get_team_id() != player->get_team_id() && buff_hash == E_SLOW_BUFF_HASH) {
//            g_sdk->log_console("[Varus] on_buff_loss: Enemy %s is no longer slowed by E",
//                source->get_char_name().c_str());
//        }
//    }
//
//    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast) {
//        if (!initialized || !sender || !cast) return;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return;
//
//        // If we're the one casting
//        if (sender == player) {
//            int spell_slot = cast->get_spell_slot();
//
//            switch (spell_slot) {
//            case 0: // Q
//                g_sdk->log_console("[Varus] on_process_spell_cast: Cast Q");
//                break;
//            case 1: // W
//                g_sdk->log_console("[Varus] on_process_spell_cast: Cast W");
//                break;
//            case 2: // E
//                g_sdk->log_console("[Varus] on_process_spell_cast: Cast E");
//                break;
//            case 3: // R
//                g_sdk->log_console("[Varus] on_process_spell_cast: Cast R");
//                combat_state.r_active = true;
//                break;
//            }
//        }
//    }
//
//    void load() {
//        try {
//            g_sdk->log_console("[+] Loading Varus...");
//
//            if (!sdk_init::orbwalker() || !sdk_init::target_selector() ||
//                !sdk_init::damage() || !sdk_init::infotab() || !sdk_init::prediction()) {
//                g_sdk->log_console("[!] Failed to load dependencies!");
//                return;
//            }
//
//            // Try to load optional dependencies
//            sdk_init::health_prediction();
//
//            auto& order_manager = vengaboys::OrderManager::get_instance();
//            order_manager.set_min_order_delay(0.0f);
//            order_manager.set_max_queue_size(8);
//            order_manager.set_cooldown_after_ult(0.1f);
//
//            // Enable debug mode by default
//            settings.debug_enabled = true;
//            debug_enabled = true;
//
//            g_sdk->log_console("[+] Dependencies loaded successfully.");
//
//            if (sdk::orbwalker) {
//                sdk::orbwalker->register_callback(orb_sdk::before_attack, reinterpret_cast<void*>(&before_attack));
//                g_sdk->log_console("[+] Registered Orbwalker before_attack callback");
//            }
//
//            create_menu();
//            if (!menu) {
//                g_sdk->log_console("[!] Failed to create menu");
//                return;
//            }
//
//            if (g_sdk->event_manager) {
//                g_sdk->event_manager->register_callback(event_manager::event::game_update,
//                    reinterpret_cast<void*>(&on_update));
//                g_sdk->event_manager->register_callback(event_manager::event::draw_world,
//                    reinterpret_cast<void*>(&on_draw));
//                g_sdk->event_manager->register_callback(event_manager::event::buff_gain,
//                    reinterpret_cast<void*>(&on_buff_gain));
//                g_sdk->event_manager->register_callback(event_manager::event::buff_loss,
//                    reinterpret_cast<void*>(&on_buff_loss));
//                g_sdk->event_manager->register_callback(event_manager::event::process_cast,
//                    reinterpret_cast<void*>(&on_process_spell_cast));
//                g_sdk->log_console("[+] Event callbacks registered successfully.");
//            }
//
//            // Initial settings
//            settings.q_range = 1595.f;
//            settings.q_min_range = 825.f;
//            settings.q_charge_time = 1.5f;
//            settings.e_range = 925.f;
//            settings.e_radius = 235.f;
//            settings.r_range = 1370.f;
//            settings.r_radius = 120.f;
//            settings.r_speed = 1500.f;
//            settings.min_spell_delay = 0.05f;
//            settings.draw_q_charge = true;
//            settings.draw_w_stacks = true;
//            settings.draw_ranges = true;
//            settings.draw_damage = true;
//
//            // Q settings
//            settings.q_radius = 70.f;
//            settings.q_speed = 1900.f;
//            settings.q_delay = 0.25f;
//
//            // E settings
//            settings.e_delay = 0.25f;
//
//            // R settings
//            settings.r_delay = 0.25f;
//            settings.r_use_on_cc = true;
//            settings.r_use_on_killable = true;
//
//            g_sdk->log_console("[+] Varus settings initialized");
//
//            initialized = true;
//            g_sdk->log_console("[+] Varus loaded successfully");
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in load: %s", e.what());
//        }
//    }
//
//    void unload() {
//        try {
//            if (!initialized) return;
//
//            g_sdk->log_console("[+] Unloading Varus...");
//
//            if (g_sdk->event_manager) {
//                g_sdk->event_manager->unregister_callback(event_manager::event::game_update,
//                    reinterpret_cast<void*>(&on_update));
//                g_sdk->event_manager->unregister_callback(event_manager::event::draw_world,
//                    reinterpret_cast<void*>(&on_draw));
//                g_sdk->event_manager->unregister_callback(event_manager::event::buff_gain,
//                    reinterpret_cast<void*>(&on_buff_gain));
//                g_sdk->event_manager->unregister_callback(event_manager::event::buff_loss,
//                    reinterpret_cast<void*>(&on_buff_loss));
//                g_sdk->event_manager->unregister_callback(event_manager::event::process_cast,
//                    reinterpret_cast<void*>(&on_process_spell_cast));
//            }
//
//            if (sdk::orbwalker) {
//                sdk::orbwalker->unregister_callback(orb_sdk::before_attack,
//                    reinterpret_cast<void*>(&before_attack));
//            }
//
//            if (sdk::infotab && infotab_text_id != 0) {
//                sdk::infotab->remove_text(infotab_text_id);
//                infotab_text_id = 0;
//            }
//
//            initialized = false;
//            g_sdk->log_console("[+] Varus unloaded successfully");
//        }
//        catch (const std::exception& e) {
//            g_sdk->log_console("[!] Exception in unload: %s", e.what());
//        }
//    }
//}