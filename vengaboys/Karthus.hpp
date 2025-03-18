#pragma once

#include "sdk.hpp"
#include "ordermanager.hpp"
#include <array>
#include <string>
#include <functional>
#include <unordered_set>

namespace karthus
{
    struct Settings
    {
        bool use_q = true;
        bool use_w = true;
        bool use_e = true;
        bool use_r = true;

        bool q_prediction = true;
        float q_range = 875.f;
        float q_radius = 160.f;
        float q_delay = 0.75f;
        float q_min_mana = 30.f;
        int q_min_hitchance = 70;
        bool q_multihit = true;

        float w_range = 1000.f;
        float w_radius = 250.f;
        float w_delay = 0.25f;
        float w_min_mana = 30.f;
        int w_min_hitchance = 70;
        float w_min_distance = 400.f;

        float e_range = 550.f;
        float e_min_mana = 50.f;
        int e_min_enemies = 1;
        bool e_disable_under_min_mana = true;

        bool r_use_lethal = true;
        int r_min_kills = 1;
        bool r_notification = true;
        bool r_sound_notification = false;

        bool harass_use_q = true;
        bool harass_use_w = false;
        bool harass_use_e = false;
        float harass_min_mana = 60.f;
        bool harass_under_turret = false;

        bool farm_use_q = true;
        bool farm_min_mana = 40.f;
        int farm_min_minions_q = 3;
        bool farm_under_turret = false;

        bool draw_ranges = true;
        bool draw_damage = true;
        bool draw_q_range = true;
        bool draw_w_range = true;
        bool draw_e_range = true;
        bool draw_r_indicator = true;
        uint32_t q_range_color = 0x4000FFFF;
        uint32_t w_range_color = 0x4000FFFF;
        uint32_t e_range_color = 0x4000FFFF;

        bool disable_aa_after_level = true;
        int disable_aa_level = 6;

        // Special mode
        bool urf_mode = false;
        float min_spell_delay = 0.05f;

        bool debug_enabled = false;
    };

    struct CombatState
    {
        bool passive_active = false;
        float passive_start_time = 0.f;
        bool e_active = false;

        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_e_time = 0.f;
        float last_r_time = 0.f;
        float last_farm_q_time = 0.f;

        float last_combo_time = 0.f;
        float last_harass_time = 0.f;

        int r_potential_kills = 0;
        std::unordered_set<uint32_t> r_notified_targets;
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

    float calculate_q_damage(game_object* target, bool is_isolated = true);
    float calculate_e_damage(game_object* target);
    float calculate_r_damage(game_object* target);
    float calculate_full_combo_damage(game_object* target);
    bool is_under_turret(const math::vector3& pos);
    int count_nearby_enemies(float range);

    bool cast_q(game_object* target);
    bool cast_w(game_object* target);
    bool toggle_e(bool activate);
    bool cast_r();

    void handle_combo();
    void handle_harass();
    void handle_farming();
    void urf_mode_logic();
    void update_r_indicator();
    void send_r_notification(game_object* target);

    void create_menu();

    void __fastcall on_update();
    void __fastcall on_draw();
    bool __fastcall before_attack(orb_sdk::event_data* data);
    void __fastcall on_buff_gain(game_object* source, buff_instance* buff);
    void __fastcall on_buff_loss(game_object* source, buff_instance* buff);
    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast);

    void load();
    void unload();
}