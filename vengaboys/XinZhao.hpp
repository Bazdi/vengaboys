#pragma once

#include "sdk.hpp"
#include "ordermanager.hpp"
#include <array>

namespace xinzhao {
    // Forward declarations of namespace-level functions
    void load();
    void unload();

    struct Settings {
        // Combat settings
        bool use_q = true;
        bool use_w = true;
        bool use_e = true;
        bool use_r = true;
        float r_min_enemies = 2;
        float r_hp_threshold = 30.f;

        // Harass settings
        bool harass_use_q = true;
        bool harass_use_w = true;
        bool harass_use_e = false;
        float harass_min_mana = 30.f;

        // Farm settings
        bool farm_use_q = true;
        bool farm_use_w = true;
        bool farm_use_e = true;
        float farm_min_mana = 30.f;

        // Jungle settings
        bool jungle_use_q = true;
        bool jungle_use_w = true;
        bool jungle_use_e = true;
        bool save_e_after_camps = true;
        float jungle_min_mana = 20.f;
        bool jungle_smart_ability_order = true;

        // Clear settings
        bool clear_use_q = true;
        bool clear_use_w = true;
        bool clear_use_e = true;
        int clear_min_minions_w = 3;
        float clear_min_mana = 40.f;
        bool clear_wave_management = true;

        // Gap close settings
        float e_max_range = 650.f;
        bool e_turret_dive = false;

        // Flee settings
        bool flee_use_e = true;
        bool flee_use_w = true;
        bool flee_use_r = true;
        bool flee_r_only_surrounded = true;

        // Draw settings
        bool draw_w_range = true;
        bool draw_e_range = true;
        bool draw_r_range = true;
        bool draw_damage = true;
        bool draw_aa_reset_timing = false;
    };

    struct CombatState {
        bool q_active = false;
        int q_stacks = 0;
        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_e_time = 0.f;
        float last_r_time = 0.f;
        float last_aa_time = 0.f;
        bool recently_auto_attacked = false;
        float auto_attack_timer = 0.f;
        game_object* last_target = nullptr;

        struct PassiveState {
            int stacks = 0;
            float last_stack_time = 0.f;
            float stack_duration = 3.f;
            bool healing_ready = false;
            float healing_amount = 0.f;
        } passive;
    };

    struct SpellTracker {
        std::array<float, 4> cooldowns;

        void update_cooldown(int spell_slot, float time) {
            if (spell_slot >= 0 && spell_slot < 4) {
                cooldowns[spell_slot] = time;
            }
        }

        bool is_ready(int spell_slot, float current_time) {
            if (spell_slot >= 0 && spell_slot < 4) {
                return current_time >= cooldowns[spell_slot];
            }
            return false;
        }

        SpellTracker() : cooldowns{ 0.f, 0.f, 0.f, 0.f } {}
    };

    class XinZhao {
    public:
        void load();
        void unload();
        void on_update();
        void on_draw();
        bool on_before_attack(orb_sdk::event_data* data);
        void on_buff_gain(game_object* sender, buff_instance* buff);
        void on_buff_lose(game_object* sender, buff_instance* buff);
        void on_process_spell_cast(game_object* sender, spell_cast* cast);

        // Expose for AA reset implementation
        bool can_cast_spell(int spell_slot, float delay);
        bool cast_q(game_object* target);
        CombatState& get_combat_state() { return combat_state; }

    private:
        bool initialized = false;
        menu_category* menu = nullptr;
        CombatState combat_state{};
        SpellTracker spell_tracker{};
        uint32_t infotab_text_id = 0;

        // Core functionality
        void log_debug(const char* format, ...);
        int count_enemies_in_range(float range);
        bool is_under_turret(const math::vector3& pos);
        bool is_large_monster_nearby();
        bool is_valid_target(game_object* target, float extra_range = 0.f);
        bool should_push_wave();
        int count_allies_in_range(float range);
        game_object* get_best_flee_target();

        // Damage calculations
        float calculate_q_damage(game_object* target);
        float calculate_w_damage(game_object* target);
        float calculate_e_damage(game_object* target);
        float calculate_r_damage(game_object* target);
        float calculate_full_combo_damage(game_object* target);

        // Spell casts
        bool cast_w(game_object* target);
        bool cast_e(game_object* target);
        bool cast_r(bool force);
        bool should_use_r_defensive();

        // Combat handlers
        void handle_combo();
        void handle_harass();
        void handle_jungle_clear();
        void handle_lane_clear();
        void handle_flee();
        void create_menu();
        void update_passive();
        void update_aa_timer();
        bool is_aa_reset_window();
        void handle_aa_reset(game_object* target);
    };

    // Global event callbacks
    void __fastcall on_game_update();
    void __fastcall on_draw_world();
    bool __fastcall before_attack_callback(orb_sdk::event_data* data);
    void __fastcall on_buff_gain_callback(game_object* source, buff_instance* buff);
    void __fastcall on_buff_loss_callback(game_object* source, buff_instance* buff);
    void __fastcall on_process_cast_callback(game_object* sender, spell_cast* cast);

    // Global variables
    extern XinZhao xin_zhao;
    extern Settings settings;
    extern bool debug_enabled;

} // namespace xinzhao