#pragma once

#include "sdk.hpp"
#include "ordermanager.hpp"
#include <array>
#include <string>
#include <functional>
#include <unordered_set>

namespace nocturne
{
    struct Settings
    {
        bool use_q = true;
        bool use_w = true;
        bool use_e = true;
        bool use_r = true;
        float e_big_monster_mana_threshold = 50.f; // Only use E on big monsters if mana is above this percentage
        bool use_e_on_big_monsters = true;        // Option to toggle this feature

        bool q_prediction = true;
        float q_range = 1200.f;
        float q_min_mana = 30.f;

        bool w_auto_cc = true;
        float w_low_health_threshold = 30.f;
        float w_min_mana = 0.f;

        float e_range = 425.f;
        float e_min_mana = 50.f;
        float e_fear_duration = 2.f;

        float r_range = 3250.f;
        bool r_use_lethal = true;
        bool r_use_if_escaping = true;
        float r_recast_delay = 0.25f;
        float r_min_mana = 100.f;

        bool harass_use_q = true;
        bool harass_use_e = false;
        float harass_min_mana = 60.f;
        bool harass_under_turret = false;

        bool farm_use_q = true;
        bool farm_use_w = true;
        bool farm_use_e = true;
        float farm_min_mana = 40.f;
        int farm_min_minions_q = 3;
        bool farm_under_turret = false;

        bool draw_ranges = true;
        bool draw_damage = true;
        bool draw_q_trail = true;
        bool draw_e_tether = true;

        float min_spell_delay = 0.05f;

        bool debug_enabled = false;
    };

    struct CombatState
    {
        bool r_active = false;
        float r_activation_time = 0.f;
        bool r_dashed = false;

        game_object* e_target = nullptr;
        bool passive_ready = false;

        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_e_time = 0.f;
        float last_r_time = 0.f;
        float last_farm_q_time = 0.f;

        float last_combo_time = 0.f;
        float last_harass_time = 0.f;

        void check_passive_status(game_object* player);
    };

    struct SpellTracker {
        std::array<float, 4> cooldowns;
        void update_cooldown(int spell_slot, float time) {
            cooldowns[spell_slot] = time;
        }
        bool is_ready(int spell_slot, float current_time) {
            return current_time >= cooldowns[spell_slot];
        }
        SpellTracker() : cooldowns{ 0.f, 0.f, 0.f, 0.f } {}
    };

    struct CachedTarget {
        game_object* target = nullptr;
        float distance = 0.f;
        float last_update_time = 0.f;
        bool is_valid = false;
    };

    extern Settings settings;
    extern menu_category* menu;
    extern bool initialized;
    extern bool debug_enabled;
    extern CombatState combat_state;
    extern SpellTracker spell_tracker;
    extern CachedTarget cached_target;

    extern uint32_t infotab_text_id;

    void log_debug(const char* format, ...);
    bool can_cast_spell(int spell_slot, float delay);

    float calculate_q_damage(game_object* target);
    float calculate_e_damage(game_object* target);
    float calculate_r_damage(game_object* target);
    float calculate_full_combo_damage(game_object* target);
    bool is_under_turret(const math::vector3& pos);
    bool check_dusk_trail(game_object* target);
    bool has_dusk_trail_buff(game_object* object);

    bool cast_q(game_object* target);
    bool cast_w(bool block_cc);
    bool cast_e(game_object* target);
    bool cast_r(game_object* target);
    bool cast_re_dash_to_target(game_object* target);

    void handle_combo();
    void handle_harass();
    void handle_farming();
    void handle_jungle_clear();

    void create_menu();

    void __fastcall on_update();
    void __fastcall on_draw();
    bool __fastcall before_attack(orb_sdk::event_data* data);
    void __fastcall on_buff_gain(game_object* source, buff_instance* buff);
    void __fastcall on_buff_loss(game_object* source, buff_instance* buff);
    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast);

    void load();
    void unload();

    bool force_cast_q(game_object* target);
}