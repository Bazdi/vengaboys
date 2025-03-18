#pragma once
#include "sdk.hpp"
#include "OrderManager.hpp"
#include "champion_spell_names.hpp"

namespace riven {
    // New enum to track the current combo state
    enum class ComboStep {
        IDLE,
        WAITING_FOR_E,
        WAITING_FOR_AA_AFTER_E,
        WAITING_FOR_W,
        WAITING_FOR_AA_AFTER_W,
        WAITING_FOR_Q1,
        WAITING_FOR_AA_AFTER_Q1,
        WAITING_FOR_Q2,
        WAITING_FOR_AA_AFTER_Q2,
        WAITING_FOR_Q3,
        WAITING_FOR_AA_AFTER_Q3
    };

    struct SpellTracker {
        float cooldowns[4] = { 0.f, 0.f, 0.f, 0.f };

        void update_cooldown(int spell_slot, float end_time) {
            if (spell_slot >= 0 && spell_slot < 4) {
                cooldowns[spell_slot] = end_time;
            }
        }

        bool is_ready(int spell_slot, float current_time) {
            if (spell_slot >= 0 && spell_slot < 4) {
                return current_time >= cooldowns[spell_slot];
            }
            return false;
        }
    };

    struct CachedTarget {
        game_object* target = nullptr;
        float last_update_time = 0.f;

        void update(game_object* new_target) {
            target = new_target;
            last_update_time = g_sdk->clock_facade->get_game_time();
        }

        bool is_valid(float max_age = 1.0f) {
            if (!target || !target->is_valid()) return false;

            float current_time = g_sdk->clock_facade->get_game_time();
            return (current_time - last_update_time) <= max_age;
        }
    };

    struct Settings {
        bool use_q = true;
        bool use_w = true;
        bool use_e = true;
        bool use_r1 = true;
        bool use_r2 = true;
        float r_hp_threshold = 25.f;
        int r_min_targets = 2;

        bool harass_use_q = true;
        bool harass_use_w = true;

        bool clear_use_q = true;
        bool clear_use_w = true;

        bool q_keep_alive = true;
        float q_extend_range = 800.f;

        float w_range = 250.f;

        bool e_gap_close = true;
        float e_gap_close_range = 325.f;

        bool cancel_q = true;
        bool cancel_w = true;
        float cancel_delay = 0.25f;

        bool draw_ranges = true;
        bool draw_combo_state = true;
        bool draw_damage_prediction = true;

        float min_spell_delay = 0.05f;
        float harass_min_energy = 0.f;
        float clear_min_energy = 0.f;

        // Magnet mode settings
        bool magnet_mode = false;
        std::string* magnet_mode_key = nullptr;
        float magnet_follow_distance = 175.f;
    };

    struct ComboState {
        bool r_active = false;
        int q_count = 0;
        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_e_time = 0.f;
        float last_r_time = 0.f;
        float last_aa_time = 0.f;
        float r_start_time = 0.f;
        float last_aa_cancel_time = 0.f;
        bool waiting_for_aa = false;
        game_object* current_target = nullptr;

        // New combo state tracking
        ComboStep combo_step = ComboStep::IDLE;
        float last_step_time = 0.f;
        bool combo_in_progress = false;
    };

    extern Settings settings;
    extern menu_category* menu;
    extern bool initialized;
    extern bool debug_enabled;
    extern ComboState combo_state;
    extern SpellTracker spell_tracker;
    extern CachedTarget cached_target;

    extern uint32_t infotab_text_id;

    void log_debug(const char* format, ...);
    bool can_cast_spell(int spell_slot, float delay);

    float get_total_damage(game_object* target, bool include_r);
    bool can_kill_with_r2(game_object* target);
    game_object* get_best_target();
    bool should_use_r1();
    float calculate_q_damage(game_object* target);
    float calculate_w_damage(game_object* target);

    bool cast_q(game_object* target);
    bool cast_w(game_object* target);
    bool cast_e(game_object* target);
    bool cast_r1(game_object* target);
    bool cast_r2(game_object* target);

    void handle_combo();
    void handle_harass();
    void handle_lane_clear();
    void handle_animation_cancel();
    void handle_q_keep_alive();
    void handle_magnet_mode();

    void create_menu();
    void debug_buff_info(game_object* player);

    void __fastcall on_update();
    void __fastcall on_draw();
    bool __fastcall on_before_attack(orb_sdk::event_data* data);
    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast);

    void load();
    void unload();
}