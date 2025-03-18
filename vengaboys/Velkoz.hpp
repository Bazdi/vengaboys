#pragma once

#include "sdk.hpp"
#include "ordermanager.hpp"
#include <array>

namespace velkoz {
    // Module functions
    void load();
    void unload();

    struct Settings {
        // Combo settings
        bool use_q_combo = true;
        bool use_w_combo = true;
        bool use_e_combo = true;
        bool use_r_combo = true;
        int r_min_enemies = 2;
        bool block_orbwalker_r = true;
        bool use_q_split = true;      // Option to use Q split mechanic
        float q_split_angle = 45.0f;   // Angle for Q split (degrees)
        bool r_auto_follow = true;     // Auto-follow targets with R
        bool r_ignore_cursor = true;   // Auto-aim R (ignoring cursor position)
        unsigned char r_auto_aim_key = 0;  // Hotkey to temporarily enable auto-aim
        bool r_auto_aim_key_pressed = false; // Track if R auto-aim key is pressed
        std::string r_auto_aim_key_str = "";
        uint32_t infotab_id = 0;

        // Q second cast
        bool auto_reactivate_q = true;    // Auto reactivate Q when it will hit
        unsigned char auto_q_hotkey = 0;  // Hotkey to toggle auto Q
        bool auto_q_hotkey_pressed = false; // Track if hotkey is pressed
        std::string auto_q_hotkey_str = "";

        // Harass settings
        bool use_q_harass = true;
        bool use_w_harass = true;
        float harass_min_mana = 30.f;

        // Lane Clear settings
        bool use_q_clear = true;
        bool use_w_clear = true;
        float clear_min_mana = 40.f;

        // Jungle Clear settings
        bool use_q_jungle = true;
        bool use_w_jungle = true;
        bool use_e_jungle = true;
        float jungle_min_mana = 20.f;

        // Draw settings
        bool draw_q_range = true;
        bool draw_w_range = true;
        bool draw_e_range = true;
        bool draw_r_range = true;
        bool draw_damage = true;
        bool draw_debug = false;       // Draw debug information
    };

    struct SpellTracker {
        struct SpellInfo {
            float last_cast_time = 0.f;
            float cooldown = 0.f;
            bool is_ready(float current_time) const {
                return current_time >= last_cast_time + cooldown;
            }
        };
        std::array<SpellInfo, 4> spells{};

        void update_cooldown(int slot, float cooldown_end_time) {
            if (slot >= 0 && slot < 4) {
                spells[slot].last_cast_time = g_sdk->clock_facade->get_game_time();
                spells[slot].cooldown = cooldown_end_time - spells[slot].last_cast_time;
            }
        }
    };

    class Velkoz {
    public:
        void load();
        void unload();
        void on_update();
        void on_draw();
        bool on_before_attack(orb_sdk::event_data* data);
        bool on_before_move(orb_sdk::event_data* data);
        void on_buff_gain(game_object* sender, buff_instance* buff);
        void on_buff_loss(game_object* sender, buff_instance* buff);
        void on_process_spell_cast(game_object* sender, spell_cast* cast);

    private:
        bool initialized = false;
        bool is_ulting = false;
        game_object* current_r_target = nullptr;
        float last_r_follow_time = 0.f;
        menu_category* menu = nullptr;
        SpellTracker spell_tracker{};

        // Q tracking data
        bool q_reactivate_available = false;
        float q_cast_time = 0.f;
        math::vector3 q_missile_start_pos{};
        math::vector3 q_missile_direction{};
        math::vector3 q_missile_current_pos{};
        game_object* q_missile_object = nullptr;
        bool q_missile_tracked = false;
        bool q_reactivation_pending = false;

        // R tracking data
        float r_cast_time = 0.f;
        bool r_auto_follow_active = false;

        // Core functionality
        bool can_cast_spell(int spell_slot, float delay = 0.f);
        void log_debug(const char* format, ...);
        bool is_valid_target(game_object* target, float extra_range = 0.f);
        int count_enemies_in_range(float range);
        void follow_r_target();
        game_object* find_best_r_target();
        bool collision_check(const math::vector3& start, const math::vector3& end);

        // Q reactivation and missile tracking
        void handle_q_reactivation();
        void track_q_missile();
        bool would_q_split_hit_target(game_object* target);
        bool is_any_target_in_q_split_path();
        std::vector<game_object*> get_enemies_near_q_path(float range);
        bool is_point_near_line(const math::vector3& point, const math::vector3& line_start,
            const math::vector3& line_end, float max_distance);

        // Damage calculations
        float calculate_passive_damage(game_object* target);
        float calculate_q_damage(game_object* target);
        float calculate_w_damage(game_object* target);
        float calculate_e_damage(game_object* target);
        float calculate_r_damage(game_object* target);
        float calculate_full_combo_damage(game_object* target);

        // Spell casting
        bool cast_q(game_object* target);
        bool cast_q_split(game_object* target);
        bool cast_w(game_object* target);
        bool cast_e(game_object* target);
        bool cast_r(game_object* target);

        // Combat handlers
        void handle_combo();
        void handle_harass();
        void handle_lane_clear();
        void handle_jungle_clear();

        void create_menu();
    };

    // Globals
    extern Velkoz velkoz_instance;
    extern Settings settings;

    // Event callbacks
    void __fastcall on_game_update();
    void __fastcall on_draw_world();
    bool __fastcall before_attack_callback(orb_sdk::event_data* data);
    bool __fastcall before_move_callback(orb_sdk::event_data* data);
    void __fastcall on_buff_gain_callback(game_object* source, buff_instance* buff);
    void __fastcall on_buff_loss_callback(game_object* source, buff_instance* buff);
    void __fastcall on_process_cast_callback(game_object* sender, spell_cast* cast);

    // Global function
    void update_infotab();
}