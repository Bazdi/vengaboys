#pragma once

#include "sdk.hpp"
#include "ordermanager.hpp"
#include <array>
#include <string>
#include <functional>
#include <unordered_set>

namespace vengaboys
{
    struct Settings
    {
        bool use_q = true;
        bool use_w = true;
        bool use_e = true;
        bool use_r = true;

        bool q_prioritize_low_hp = true;
        float q_hp_threshold = 0.3f;
        float q_min_mana = 50.f;
        bool q_under_turret = false;
        float q_range = 700.f;

        bool w_auto_reset = true;
        float w_min_mana = 40.f;

        bool e_auto_dodge = true;
        int min_e_targets = 2;
        float e_recast_time = 2.0f;
        float e_min_mana = 60.f;
        float e_range = 300.f;
        float e_lethal_aa_hp_threshold = 20.f;          

        bool r_use_defensive = true;
        float r_min_health = 30.f;
        float r_defensive_hp_threshold = 40.f;          
        int r_min_enemies_defensive = 1;             
        float r_min_mana = 100.f;
        float r_range = 300.f;

        bool harass_use_q = true;
        bool harass_use_w = true;
        bool harass_use_e = true;
        float harass_min_mana = 60.f;
        bool harass_under_turret = false;

        bool farm_use_q = true;
        bool farm_use_w = true;
        bool farm_use_e = true;
        float farm_min_mana = 40.f;
        int farm_min_minions = 3;
        bool farm_under_turret = false;

        bool ward_jump_enabled = true;
        float ward_jump_range = 600.f;
        bool ward_jump_wards = true;
        bool ward_jump_minions = true;
        bool ward_jump_allies = true;
        bool draw_ward_spots = true;

        bool draw_ranges = false;      
        bool draw_damage = true;
        bool draw_spell_ranges = false;   

        float min_spell_delay = 0.05f;         
        float min_e_recast_delay = 0.25f;      
    };

    struct CombatState
    {
        bool e_active = false;
        float e_start_time = 0.f;
        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_e_time = 0.f;
        float last_r_time = 0.f;
        float last_farm_q_cast_time = 0.f;
        float last_w_farm_cast_time = 0.f;      

        float last_combo_time = 0.f;
        float combo_spell_delay = 0.05f;      
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
    float calculate_w_damage(game_object* target);
    float calculate_e_damage(game_object* target);
    float calculate_full_combo_damage(game_object* target);
    bool is_under_turret(const math::vector3& pos);

    bool cast_q(game_object* target);
    bool cast_w(game_object* target);
    bool cast_e(game_object* target);
    bool cast_r();

    void handle_combo();
    void handle_harass();
    void handle_farming();
    void handle_fast_clear();
    void ward_jump();

    void create_menu();
    void debug_buff_info(game_object* player);

    void __fastcall on_update();
    void __fastcall on_draw();
    bool __fastcall before_attack(orb_sdk::event_data* data);
    void __fastcall on_buff_gain(game_object* source, buff_instance* buff);
    void __fastcall on_buff_loss(game_object* source, buff_instance* buff);     
    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast);


    void load();
    void unload();
}   