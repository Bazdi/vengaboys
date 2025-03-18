#pragma once

#include "sdk.hpp"
#include "ordermanager.hpp"
#include <array>
#include <string>
#include <functional>
#include <unordered_set>
#include <vector>
#include <random>

namespace zeri
{
    struct JumpSpot {
        std::string name;
        math::vector3 start_pos;
        math::vector3 end_pos;
        bool enabled = true;
        uint32_t color = 0xFF00FF00;  // Default: Green
        float radius = 65.f;          // Default radius for visualization
    };

    struct JumpSpotRecord {
        math::vector3 start_pos;
        math::vector3 end_pos;
        float time_recorded;
        bool successful;
    };

    struct QHumanization {
        bool enabled = true;           // Enable Q humanization
        float min_delay = 0.01f;       // Minimum delay between Q casts (lowered for more aggression)
        float max_delay = 0.08f;       // Maximum delay between Q casts (lowered for more aggression)
        bool randomize = true;         // Add randomization to timings
        bool dynamic_delay = true;     // Adjust delay based on attack speed
        float delay_scale = 0.6f;      // Scale factor for delay (lowered for faster attacks)
        bool movement_aware = true;    // Adjust based on movement
        bool avoid_patterns = true;    // Avoid obvious casting patterns
        int miss_chance = 2;           // % chance to "miss" a Q cast (lowered for more consistency)
    };

    struct Settings
    {
        // General settings
        bool use_q = true;
        bool use_w = true;
        bool use_e = true;
        bool use_r = true;
        float min_spell_delay = 0.02f; // Reduced for more responsive spells

        // Q settings
        QHumanization q_humanization;
        float q_range = 825.f;
        float q_min_mana = 0.f;        // Q has no mana cost in-game
        bool q_use_extended = true;    // Use extended Q through terrain

        // W settings
        bool w_charged_preferred = true;
        float w_charge_time = 0.75f;   // Time to fully charge W
        float w_min_charge = 0.3f;     // Minimum charge time before releasing
        float w_range = 1200.f;
        float w_min_mana = 50.f;
        int w_min_hitchance = 60;      // Minimum hitchance percent
        bool w_wall_pierce = true;     // Prioritize wall-piercing shots

        // E settings
        float e_range = 300.f;
        float e_min_mana = 40.f;
        bool e_over_walls = true;      // Enable wall-jumping with E
        float e_wall_check_range = 350.f; // Range to check for jumpable walls
        bool e_save_for_escape = false; // Save E for escaping

        // R settings
        float r_range = 825.f;
        float r_min_mana = 100.f;
        int r_min_enemies = 2;         // Min enemies to use R in combo
        float r_min_health = 60.f;     // Min own health% to use R defensively

        // Combat settings
        bool combo_prioritize_charged_w = true;
        bool combo_use_e_aggressive = true;
        bool combo_use_e_kite = true;

        // Harass settings
        bool harass_use_q = true;
        bool harass_use_w = false;
        float harass_min_mana = 60.f;
        bool harass_under_turret = false;

        // Farm settings
        bool farm_use_q = true;
        bool farm_use_w = false;
        bool farm_use_e = false;
        bool farm_use_jungle = false;
        float farm_min_mana = 40.f;
        int farm_min_minions_w = 3;    // Min minions hit by W
        bool farm_under_turret = true;

        // Wall jump settings
        bool jump_spots_enabled = true;
        bool jump_spots_debug = false; // Show all possible jump spots
        int jump_spot_draw_range = 2000; // Range to draw jump spots
        bool manual_wall_jump = true;  // Enable manual wall jumping

        // Drawing settings
        bool draw_ranges = true;
        bool draw_damage = true;
        bool draw_spell_ranges = true;
        bool draw_q_humanization_stats = false;
    };

    struct CombatState
    {
        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_e_time = 0.f;
        float last_r_time = 0.f;

        bool w_charging = false;
        float w_charge_start_time = 0.f;

        bool r_active = false;
        float r_activation_time = 0.f;

        float last_auto_attack_time = 0.f;
        float next_q_delay = 0.f;      // Dynamic delay for Q humanization

        float last_combo_time = 0.f;
        float last_farm_q_cast_time = 0.f;
        float last_farm_w_cast_time = 0.f;

        int q_cast_count = 0;          // Track consecutive Q casts for humanization
        int q_missed_count = 0;        // Track "missed" Q inputs for humanization
        std::vector<float> recent_cast_delays; // Store recent delays for pattern detection
    };

    struct SpellTracker {
        std::array<float, 4> cooldowns{ 0.f, 0.f, 0.f, 0.f };

        void update_cooldown(int spell_slot, float time) {
            cooldowns[spell_slot] = time;
        }

        bool is_ready(int spell_slot, float current_time) {
            return current_time >= cooldowns[spell_slot];
        }
    };

    extern Settings settings;
    extern menu_category* menu;
    extern bool initialized;
    extern bool debug_enabled;
    extern CombatState combat_state;
    extern SpellTracker spell_tracker;
    extern std::vector<JumpSpot> jump_spots;
    extern std::vector<JumpSpotRecord> recorded_jumps;
    extern std::mt19937 rng;

    extern uint32_t infotab_text_id;

    // Core functions
    void log_debug(const char* format, ...);
    bool can_cast_spell(int spell_slot, float delay);

    // Damage calculations
    float calculate_q_damage(game_object* target);
    float calculate_w_damage(game_object* target, bool charged = false);
    float calculate_e_damage(game_object* target);
    float calculate_r_damage(game_object* target);
    float calculate_full_combo_damage(game_object* target);

    // Spell casting
    bool cast_q(game_object* target = nullptr, const math::vector3* position = nullptr);
    bool cast_w(game_object* target = nullptr, const math::vector3* position = nullptr, bool charge = false);
    bool release_w(game_object* target = nullptr, const math::vector3* position = nullptr);
    bool cast_e(game_object* target = nullptr, const math::vector3* position = nullptr);
    bool cast_r(game_object* target = nullptr);

    // Combat modes
    void handle_combo();
    void handle_harass();
    void handle_farming();
    void handle_flee();

    // Q humanization
    float get_humanized_q_delay();
    bool should_cast_q_now();
    void update_q_humanization_stats();

    // Wall-jump functionality
    void initialize_jump_spots();
    void handle_wall_jump();
    bool find_nearest_jump_spot(const math::vector3& position, JumpSpot& result, float max_distance = 500.f);
    void debug_wall_jumps();
    bool is_wall_between(const math::vector3& start, const math::vector3& end);
    bool test_wall_jump(const math::vector3& start, const math::vector3& end);
    void record_wall_jump(const math::vector3& start_pos, const math::vector3& end_pos, bool successful);
    void save_jump_spots_to_file();
    void draw_recorded_jumps();

    // UI and visuals
    void create_menu();
    void render_q_humanization_stats();
    void draw_jump_spots();

    // Event handlers
    void __fastcall on_update();
    void __fastcall on_draw();
    bool __fastcall before_attack(orb_sdk::event_data* data);
    void __fastcall on_buff_gain(game_object* source, buff_instance* buff);
    void __fastcall on_buff_loss(game_object* source, buff_instance* buff);
    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast);
    void __fastcall on_create_object(game_object* obj);

    // Module lifecycle
    void load();
    void unload();
};