#pragma once

#include "sdk.hpp"
#include "ordermanager.hpp"
#include <array>
#include <string>
#include <functional>
#include <unordered_map>

namespace varus
{
    struct Settings
    {
        // Spell usage flags
        bool use_q = true;
        bool use_w = true;
        bool use_e = true;
        bool use_r = true;

        // Q settings
        bool q_prediction = true;
        float q_range = 1595.f;
        float q_min_range = 825.f;
        float q_charge_time = 1.5f;
        float q_min_mana = 30.f;
        int q_min_stacks = 3;
        float q_radius = 70.f;
        float q_speed = 1900.f;
        float q_delay = 0.25f;

        // W settings
        int w_mode = 0; // 0 = Min HP < x%, 1 = Never
        float w_min_hp = 50.f;

        // E settings
        float e_range = 925.f;
        float e_min_mana = 50.f;
        int e_min_stacks = 3;
        float e_radius = 235.f;
        float e_delay = 0.25f;
        float e_proc_delay = 0.5f;

        // R settings
        float r_range = 1370.f;
        float r_min_mana = 100.f;
        bool r_use_on_cc = true;
        bool r_use_on_killable = true;
        float r_radius = 120.f;
        float r_speed = 1500.f;
        float r_delay = 0.25f;

        // Harass settings
        bool harass_use_q = true;
        bool harass_use_e = false;
        float harass_min_mana = 60.f;
        bool harass_under_turret = false;

        // Farm settings
        bool farm_use_q = true;
        bool farm_use_e = false;
        float farm_min_mana = 40.f;
        int farm_min_minions_q = 3;
        bool farm_under_turret = false;

        // Drawing settings
        bool draw_ranges = true;
        bool draw_damage = true;
        bool draw_q_charge = true;
        bool draw_w_stacks = true;

        float min_spell_delay = 0.05f;
        bool debug_enabled = false;
    };

    struct CombatState
    {
        bool q_charging = false;
        bool q_launching = false;
        float q_charge_start_time = 0.f;

        bool r_active = false;

        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_e_time = 0.f;
        float last_r_time = 0.f;
        float last_farm_q_time = 0.f;

        float last_combo_time = 0.f;
        float last_harass_time = 0.f;

        std::unordered_map<uint32_t, int> hero_w_stacks;
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

    // Utility functions
    void log_debug(const char* format, ...);
    bool can_cast_spell(int spell_slot, float delay);

    // Spell state tracking
    void check_q_charge_status(game_object* player, float current_time);
    bool is_q_fully_charged(float current_time);
    float get_current_q_charge_percent(float current_time);

    // Target state tracking
    int get_w_stacks(game_object* target);
    bool is_target_cc_affected(game_object* target);
    bool is_target_slowed(game_object* target);
    bool is_under_turret(const math::vector3& pos);

    // Damage calculation
    float calculate_q_damage(game_object* target, float charge_time = 1.0f);
    float calculate_w_damage(game_object* target, int stacks = 3);
    float calculate_e_damage(game_object* target);
    float calculate_r_damage(game_object* target);
    float calculate_full_combo_damage(game_object* target);

    // Spell casting
    bool start_charging_q();
    bool release_q(const math::vector3& position);
    bool cast_w();
    bool cast_e(game_object* target);
    bool cast_r(game_object* target);

    // Mode handling
    void handle_combo();
    void handle_harass();
    void handle_farming();

    // Create menu and UI
    void create_menu();

    // Events
    void __fastcall on_update();
    void __fastcall on_draw();
    bool __fastcall before_attack(orb_sdk::event_data* data);
    void __fastcall on_buff_gain(game_object* source, buff_instance* buff);
    void __fastcall on_buff_loss(game_object* source, buff_instance* buff);
    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast);

    // Module functions
    void load();
    void unload();
}