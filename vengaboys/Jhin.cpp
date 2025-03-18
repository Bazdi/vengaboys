// jhin.cpp
#include "jhin.hpp"
#include <iomanip>
#include <cmath>

namespace jhin_damage_tracker {

    constexpr int INFINITY_EDGE_ID = 3031;
    constexpr int LORD_DOMINIKS_REGARDS_ID = 3036;
    constexpr int THE_COLLECTOR_ID = 6676;
    constexpr int MORTAL_REMINDER = 3033;
    constexpr float JHIN_BASE_AD_LEVEL_1 = 59.0f;


    constexpr uint32_t RUNE_PRESS_THE_ATTACK = 8005;   // No change - Still exists, but *adaptive* damage
    constexpr uint32_t RUNE_LETHAL_TEMPO = 8008;      // No change - Attack speed, *not directly* damage
    constexpr uint32_t RUNE_FLEET_FOOTWORK = 8021;    // No change - Heal/MS, not direct damage
    constexpr uint32_t RUNE_CONQUEROR = 8010;         // No change - Stacking *adaptive* force, not direct on-hit

    constexpr uint32_t RUNE_OVERHEAL = 9101;          // No change - Shield, not damage
    constexpr uint32_t RUNE_TRIUMPH = 9111;           // No change - Heal on takedown, not damage
    constexpr uint32_t RUNE_PRESENCE_OF_MIND = 8009;  // No change, but it *is* this ID. Mana/energy, not damage.
    constexpr uint32_t RUNE_LEGEND_ALACRITY = 9104;   // No change - Attack speed, not direct damage
    constexpr uint32_t RUNE_LEGEND_TENACITY = 9109;   // No change - Tenacity, not damage
    constexpr uint32_t RUNE_LEGEND_BLOODLINE = 9103;  // No change - Lifesteal, not direct damage
    constexpr uint32_t RUNE_COUP_DE_GRACE = 8014;     // No change - **Damage amplifier**, keep this!
    constexpr uint32_t RUNE_CUT_DOWN = 8017;          // No change - **Damage amplifier**, keep this!
    constexpr uint32_t RUNE_LAST_STAND = 8299;        // No change - **Damage amplifier**, keep this!

    // --- Domination Tree ---
    constexpr uint32_t RUNE_ELECTROCUTE = 8112;       // No change - Adaptive burst, after 3 hits
    constexpr uint32_t RUNE_PREDATOR = 8124;          // No change - Active MS and damage on *next* attack/ability
    constexpr uint32_t RUNE_DARK_HARVEST = 8128;      // No change - Adaptive damage on low health, *stacks*
    constexpr uint32_t RUNE_HAIL_OF_BLADES = 9923;    // No change - Attack speed, not direct damage
    constexpr uint32_t RUNE_CHEAP_SHOT = 8126;        // No change - True damage on impaired movement
    constexpr uint32_t RUNE_TASTE_OF_BLOOD = 8139;    // No change - Healing, not damage
    constexpr uint32_t RUNE_SUDDEN_IMPACT = 8143;     // No change - Lethality/Magic Pen after dash, not direct damage
    constexpr uint32_t RUNE_ZOMBIE_WARD = 8136;       // No change - Vision and AD, not direct damage
    constexpr uint32_t RUNE_GHOST_PORO = 8120;        // No change - Vision and AD, not direct damage
    constexpr uint32_t RUNE_EYEBALL_COLLECTION = 8138;// No change - Stacking AD/AP, not direct damage
    constexpr uint32_t RUNE_TREASURE_HUNTER = 8135;   // Replaced Bounty Hunter - Gold on unique takedowns, not damage
    constexpr uint32_t RUNE_INGENIOUS_HUNTER = 8134;  // No change - Item Haste, not damage
    constexpr uint32_t RUNE_RELENTLESS_HUNTER = 8105; // No change - Out of combat MS, not damage
    constexpr uint32_t RUNE_ULTIMATE_HUNTER = 8106;   // Ultimate Haste, not direct damage.

    // --- Sorcery Tree ---
    constexpr uint32_t RUNE_SUMMON_AERY = 8214;       // No change - Single-target damage/shield
    constexpr uint32_t RUNE_ARCANE_COMET = 8229;      // No change - Skillshot damage
    constexpr uint32_t RUNE_PHASE_RUSH = 8230;        // No change - Move speed burst, not damage
    constexpr uint32_t RUNE_NULLIFYING_ORB = 8224;    // No change - Magic damage shield, not damage
    constexpr uint32_t RUNE_MANAFLOW_BAND = 8226;     // No change - Mana, not damage
    constexpr uint32_t RUNE_NIMBUS_CLOAK = 8275;      // No change - Move speed after summoner spell, not damage
    constexpr uint32_t RUNE_TRANSCENDENCE = 8210;     // No change - Ability Haste, not direct damage
    constexpr uint32_t RUNE_CELERITY = 8234;          // REMOVED - This rune was removed from the game.
    constexpr uint32_t RUNE_ABSOLUTE_FOCUS = 8233;    // No change - AD/AP above 70% HP, *affects base stats*
    constexpr uint32_t RUNE_SCORCH = 8237;            // No change - Bonus magic damage on ability hit
    constexpr uint32_t RUNE_WATERWALKING = 8232;      // No change - River stats, not direct damage
    constexpr uint32_t RUNE_GATHERING_STORM = 8236;   // No change - Scaling AD/AP, *affects base stats*

    // --- Resolve Tree ---
    //Resolve tree has no damage-modifying runes.

    // --- Inspiration Tree ---
    constexpr uint32_t RUNE_FIRST_STRIKE = 8437; // Gold and bonus TRUE damage

    // Define the extern variable declared in the header
    std::vector<JhinDamageData> damage_history;
    bool is_fourth_shot = false;


    void check_all_runes() {
        auto local_player = g_sdk->object_manager->get_local_player();
        if (!local_player) return;

        g_sdk->log_console("\n==== ACTIVE RUNES ====");

        // Check each rune slot
        for (int i = 0; i < 6; i++) {
            uint32_t rune_id = local_player->get_rune_id(i);
            if (rune_id != 0) {
                // Log all runes and their effects
                switch (rune_id) {
                    // Precision
                case RUNE_COUP_DE_GRACE:
                    g_sdk->log_console("Slot %d: Coup de Grace (8%% more damage to low health)", i);
                    break;
                case RUNE_CUT_DOWN:
                    g_sdk->log_console("Slot %d: Cut Down (more damage to higher health)", i);
                    break;
                case RUNE_LAST_STAND:
                    g_sdk->log_console("Slot %d: Last Stand (more damage when low)", i);
                    break;
                case RUNE_PRESENCE_OF_MIND:
                    g_sdk->log_console("Slot %d: Presence of Mind", i);
                    break;
                case RUNE_LEGEND_ALACRITY:
                    g_sdk->log_console("Slot %d: Legend: Alacrity (Attack Speed)", i);
                    break;

                    // Domination
                case RUNE_SUDDEN_IMPACT:
                    g_sdk->log_console("Slot %d: Sudden Impact (Lethality after dash)", i);
                    break;
                case RUNE_EYEBALL_COLLECTION:
                    g_sdk->log_console("Slot %d: Eyeball Collection (Stacking AD)", i);
                    break;

                    // Sorcery
                case RUNE_ABSOLUTE_FOCUS:
                    g_sdk->log_console("Slot %d: Absolute Focus (AD when >70%% HP)", i);
                    break;
                case RUNE_GATHERING_STORM:
                    g_sdk->log_console("Slot %d: Gathering Storm (Scaling AD)", i);
                    break;

                default:
                    g_sdk->log_console("Slot %d: Unknown Rune ID: %u", i, rune_id);
                    break;
                }
            }
        }
        g_sdk->log_console("===================\n");
    }

    bool has_item(game_object* entity, int item_id) {
        if (!entity) return false;
        int slot = 0;
        return entity->has_item(item_id, &slot);
    }

    bool is_attack_fourth_shot(game_object* jhin) {
        if (!jhin) return false;
        std::string buffName("jhinpassiveattackbuff");
        auto buff = jhin->get_buff_by_name(buffName);
        if (!buff) return false;
        return buff->is_active() && buff->get_count() == 0;
    }

    float calculate_additional_ad_from_passive(game_object* jhin) {
        if (!jhin) return 0.0f;

        // Level-based AD bonus (4% - 44%)
        int level = jhin->get_level();
        float level_bonus_percent = 0.04f + (0.40f * ((float)level - 1.0f) / 17.0f);

        // Bonus from crit chance (0.3% per 1% crit)
        float crit_chance = jhin->get_crit();
        float crit_bonus_percent = crit_chance * 0.003f;

        // Bonus from attack speed (0.25% per 1% bonus AS)
        float attack_speed_bonus = jhin->get_percent_attack_speed_mod();
        float as_bonus_percent = attack_speed_bonus * 0.0025f;

        // Total bonus percentage
        float total_bonus_percent = level_bonus_percent + crit_bonus_percent + as_bonus_percent;

        // Apply to base AD
        float base_ad = jhin->get_base_attack_damage();
        return base_ad * total_bonus_percent;
    }


    float calculate_base_crit_damage(game_object* jhin, game_object* target) {
        if (!jhin || !target) return 0.0f;

        // Get total AD including passive bonuses
        float base_ad = jhin->get_base_attack_damage();
        float bonus_ad = jhin->get_attack_damage() - base_ad;
        float passive_bonus = calculate_additional_ad_from_passive(jhin);
        float total_ad = base_ad + bonus_ad + passive_bonus;

        // Base fourth shot crit damage (150.5% + 34.4%)
        float base_crit_ratio = 1.505f + 0.344f;  // 184.9%

        // Add Infinity Edge's 40% bonus if present
        if (has_item(jhin, INFINITY_EDGE_ID)) {
            base_crit_ratio += 0.40f;
        }

        float crit_damage = total_ad * base_crit_ratio;

        // Debug output
        g_sdk->log_console("Damage Calculation Details:");
        g_sdk->log_console("  Base AD: %.2f", base_ad);
        g_sdk->log_console("  Bonus AD: %.2f", bonus_ad);
        g_sdk->log_console("  Passive Bonus AD: %.2f", passive_bonus);
        g_sdk->log_console("  Total AD: %.2f", total_ad);
        g_sdk->log_console("  Crit Ratio: %.3f", base_crit_ratio);
        g_sdk->log_console("  Raw Crit Damage: %.2f", crit_damage);

        return crit_damage;
    }

    float calculate_bonus_damage(game_object* jhin, game_object* target) {
        if (!jhin || !target) return 0.0f;

        float missing_health = target->get_max_hp() - target->get_hp();
        int jhin_level = jhin->get_level();

        // Get level-based missing health percentage
        float bonus_damage_percent;
        if (jhin_level >= 13) {
            bonus_damage_percent = 0.25f;
        }
        else if (jhin_level >= 7) {
            bonus_damage_percent = 0.20f;
        }
        else {
            bonus_damage_percent = 0.15f;
        }

        return missing_health * bonus_damage_percent;
    }

    float calculate_total_damage_before_armor(float base_crit_damage, float bonus_damage) {
        return base_crit_damage + bonus_damage;
    }

    float calculate_armor_reduction(float total_damage, game_object* target, game_object* source) {
        if (!target || !source) return 0.0f;
        float armor = target->get_armor();
        float lethality = source->get_physical_lethality();

        // Apply lethality
        float level_scaling = (0.6f + 0.4f * source->get_level() / 18.0f);
        armor -= (lethality * level_scaling);
        armor = std::max(0.0f, armor);

        return total_damage * (100.0f / (100.0f + armor));
    }

    float calculate_jhin_4th_shot_damage(game_object* jhin, game_object* target) {
        if (!jhin || !jhin->is_valid() || !target || !target->is_valid()) {
            return 0.0f;
        }

        // Calculate base crit damage
        float base_crit_damage = calculate_base_crit_damage(jhin, target);
        float bonus_damage = calculate_bonus_damage(jhin, target);
        float total_raw = base_crit_damage + bonus_damage;
        float after_armor = calculate_armor_reduction(total_raw, target, jhin);

        // Floor the final damage value
        float final_damage = std::floor(after_armor);

        // Detailed debug output
        g_sdk->log_console("\n==== DETAILED DAMAGE CALCULATION ====");
        g_sdk->log_console("Raw Values:");
        g_sdk->log_console("  Base Crit: %.2f", base_crit_damage);
        g_sdk->log_console("  Bonus: %.2f", bonus_damage);
        g_sdk->log_console("  After Armor: %.2f", after_armor);
        g_sdk->log_console("Final (Floored): %.0f", final_damage);
        g_sdk->log_console("================================\n");

        return final_damage;
    }

    void on_basic_attack(game_object* attacker, game_object* target, spell_cast* cast) {
        if (!attacker || !target || !cast) return;
        auto local_player = g_sdk->object_manager->get_local_player();
        if (!local_player || !local_player->is_valid()) return;

        if (attacker->get_network_id() == local_player->get_network_id() &&
            attacker->get_char_name() == "Jhin") {
            is_fourth_shot = is_attack_fourth_shot(attacker);
        }
    }

    void on_spell_hit(game_object* attacker, spell_cast* cast) {
        if (!attacker || !cast || !cast->is_basic_attack()) return;

        auto local_player = g_sdk->object_manager->get_local_player();
        if (!local_player || !local_player->is_valid()) return;

        if (attacker->get_network_id() == local_player->get_network_id() &&
            attacker->get_char_name() == "Jhin" &&
            is_fourth_shot)
        {
            auto target = cast->get_target();
            if (!target || !target->is_valid()) return;

            float expected_damage = calculate_jhin_4th_shot_damage(attacker, target);

            JhinDamageData data;
            data.target = target;
            data.expected_damage = expected_damage;
            data.is_fourth_shot = true;
            data.is_crit = true;
            data.time = g_sdk->clock_facade->get_game_time();
            damage_history.push_back(data);

            // Debug logging
            g_sdk->log_console("---- Jhin 4th Shot Damage Calculation ----");
            g_sdk->log_console("Base AD: %.2f", attacker->get_base_attack_damage_sans_percent_scale());
            g_sdk->log_console("Total AD: %.2f", attacker->get_attack_damage());
            g_sdk->log_console("Expected Damage: %.2f", expected_damage);
        }
    }

    void on_draw() {
        auto it = damage_history.begin();
        while (it != damage_history.end()) {
            if (!it->target || !it->target->is_valid() || it->target->is_dead() ||
                g_sdk->clock_facade->get_game_time() - it->time > 5.0f) {
                it = damage_history.erase(it);
                continue;
            }

            math::vector2 screen_pos = g_sdk->renderer->world_to_screen(it->target->get_position());
            screen_pos.y -= 20.0f;  // Offset above target

            std::string damage_text = "Damage: " +
                std::to_string(static_cast<int>(it->expected_damage)) +
                (it->is_fourth_shot ? " (4th)" : "");

            g_sdk->renderer->add_text(damage_text, 16.0f, screen_pos, 1, 0xFFFFFFFF);
            ++it;
        }
    }

    void on_update() {
        // Implement if needed, or leave empty
    }

    void load() {
        if (!sdk_init::target_selector()) {
            g_sdk->log_console("[!] Failed to init target selector");
            return;
        }
        if (!sdk_init::damage()) {
            g_sdk->log_console("[!] Failed to init damage calculator");
            return;
        }

        // Check runes at load
        check_all_runes();

        g_sdk->event_manager->register_callback(event_manager::event::spell_hit,
            reinterpret_cast<void*>(&on_spell_hit));
        g_sdk->event_manager->register_callback(event_manager::event::draw_world,
            reinterpret_cast<void*>(&on_draw));
        g_sdk->event_manager->register_callback(event_manager::event::basic_attack,
            reinterpret_cast<void*>(&on_basic_attack));

        g_sdk->log_console("[+] Jhin Damage Tracker Loaded!");
    }

    void unload() {
        g_sdk->event_manager->unregister_callback(event_manager::event::spell_hit,
            reinterpret_cast<void*>(&on_spell_hit));
        g_sdk->event_manager->unregister_callback(event_manager::event::draw_world,
            reinterpret_cast<void*>(&on_draw));
        g_sdk->event_manager->unregister_callback(event_manager::event::basic_attack,
            reinterpret_cast<void*>(&on_basic_attack));

        damage_history.clear();
        g_sdk->log_console("[-] Jhin Damage Tracker Unloaded!");
    }

} // namespace jhin_damage_tracker