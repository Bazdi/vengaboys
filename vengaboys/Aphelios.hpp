#pragma once

#include "sdk.hpp"
#include "OrderManager.hpp"
#include <string>
#include <array>
#include <vector>
#include <memory>
#include <unordered_map>

namespace aphelios {
    // Constants for weapon identification and ranges
    constexpr float BASE_ATTACK_RANGE = 550.f;
    constexpr float CALIBRUM_BONUS_RANGE = 100.f;
    constexpr float CALIBRUM_Q_RANGE = 1450.f;
    constexpr float SEVERUM_Q_RANGE = 600.f;
    constexpr float GRAVITUM_Q_RANGE = 800.f;
    constexpr float INFERNUM_Q_RANGE = 600.f;
    constexpr float CRESCENDUM_Q_RANGE = 475.f;
    constexpr float R_RANGE = 1300.f;
    constexpr float R_WIDTH = 125.f;
    constexpr float R_RANGE_MIN = 700.f;
    constexpr float R_RANGE_MAX = 2000.f;

    // Buff hashes - directly initialized with values from logs
    namespace buff_hashes {
        extern uint32_t CALIBRUM_MANAGER;
        extern uint32_t SEVERUM_MANAGER;
        extern uint32_t GRAVITUM_MANAGER;
        extern uint32_t INFERNUM_MANAGER;
        extern uint32_t CRESCENDUM_MANAGER;

        extern uint32_t OFFHAND_CALIBRUM;
        extern uint32_t OFFHAND_SEVERUM;
        extern uint32_t OFFHAND_GRAVITUM;
        extern uint32_t OFFHAND_INFERNUM;
        extern uint32_t OFFHAND_CRESCENDUM;

        extern uint32_t CALIBRUM_MARK;
        extern uint32_t GRAVITUM_DEBUFF;
        extern uint32_t GRAVITUM_ROOT;
        extern uint32_t CRESCENDUM_TURRET;

        extern uint32_t CRESCENDUM_ORBIT;
        extern uint32_t SEVERUM_COUNT;
        extern uint32_t APHELIOS_CANNOT_MOVE;
    }

    extern std::unordered_map<uint32_t, float> damage_cache;
    extern float last_damage_cache_clear;
    extern const float DAMAGE_CACHE_DURATION;

    constexpr float MAX_AMMO = 50.f;
    constexpr float AMMO_DRAIN_RATE = 0.5f;

    enum class WeaponType {
        CALIBRUM = 0,
        SEVERUM = 1,
        GRAVITUM = 2,
        INFERNUM = 3,
        CRESCENDUM = 4,
        NONE = 5
    };

    struct WeaponData {
        WeaponType type = WeaponType::NONE;
        float ammo = 0.f;
        float last_ammo_update = 0.f;
        float ammo_drain_rate = 0.f;
        float time_equipped = 0.f;
        bool is_active = false;
        bool was_just_equipped = false;
        int stacks = 0;
    };

    struct WeaponState {
        WeaponData main_hand;
        WeaponData off_hand;
        std::array<WeaponType, 3> queue;
        float last_weapon_switch = 0.f;

        WeaponState() {
            queue.fill(WeaponType::NONE);
            last_weapon_switch = 0.f;
        }
    };

    struct Settings {
        bool auto_q = true;
        bool use_calibrum_q = true;
        bool calibrum_q_manual_target = false;
        float calibrum_q_min_range = 650.f;

        bool use_severum_q = true;
        bool severum_q_toggle = true;
        float severum_q_anti_melee_range = 250.f;
        bool severum_q_only_when_enemies_close = true;

        bool use_gravitum_q = true;
        bool gravitum_q_manual = false;
        bool gravitum_q_antigapcloser = true;

        bool use_infernum_q = true;
        int infernum_q_min_targets = 2;
        bool infernum_q_prioritize_heroes = true;

        bool use_crescendum_q = true;
        bool crescendum_q_turret_safety = true;
        float crescendum_q_min_distance = 250.f;

        bool auto_r = true;
        int r_min_enemies = 2;
        float r_range = R_RANGE;
        unsigned char manual_r_key{ 0x52 };
        bool manual_r_key_pressed = false;
        bool r_save_for_combo = true;
        float r_min_hp_ratio = 0.4f;

        bool auto_weapon_switch = true;
        unsigned char manual_switch_key{ 0x57 };
        bool manual_switch_key_pressed = false;
        bool instant_q_after_switch = true;
        bool smart_weapon_rotation = true;

        // Anti-magneting settings
        bool anti_magneting = true;
        bool show_magnet_warnings = true;

        // Standard weapon queue settings
        bool use_standard_weapon_queue = false;
        std::array<WeaponType, 3> standard_queue = {
            WeaponType::CALIBRUM,
            WeaponType::SEVERUM,
            WeaponType::INFERNUM
        };

        bool harass_with_q = false;

        bool farm_with_abilities = true;
        bool prioritize_cannon_minions = true;
        int min_minions_for_aoe = 3;

        bool tower_dive_prevention = true;
        float safety_range_buffer = 100.f;
        bool anti_gapcloser = true;
        bool kite_melee_champions = true;

        bool draw_ranges = true;
        bool draw_weapon_info = true;
        bool draw_damage_indicators = true;
        bool draw_ammo_bars = true;
        bool draw_weapon_queue = true;
        uint32_t range_color = 0xFF00FF00;
        uint32_t weapon_info_color = 0xFFFFFFFF;
        uint32_t damage_indicator_color = 0xFFFF0000;
        uint32_t ammo_bar_color = 0xFF00FFFF;

        bool debug_mode = false;
        bool debug_weapon_switches = false;
        bool debug_damage_calcs = false;

        float min_spell_delay = 0.05f;

        math::vector2 weapon_queue_position = { 50.f, 200.f };
        bool is_dragging_weapon_queue = false;
        math::rect weapon_queue_drag_area = math::rect(0.f, 0.f, 0.f, 0.f);

        math::vector2 weapon_info_position = { 0.f, 0.f };
        bool is_dragging_weapon_info = false;
        math::rect weapon_info_drag_area = math::rect(0.f, 0.f, 0.f, 0.f);
    };

    class WeaponManager {
    public:
        static WeaponState get_current_state();
        static bool should_switch_weapons(const WeaponState& state);
        static WeaponType get_best_weapon_for_situation();
        static float evaluate_weapon_combination(WeaponType main, WeaponType off);
        static bool is_good_combo(WeaponType main, WeaponType off);
        static void update_weapon_queue(WeaponState& state);
        static float get_ammo_drain_multiplier(WeaponType weapon);
        static void handle_weapon_stacks(WeaponData& weapon);
    };

    class DamageCalculator {
    public:
        static float get_auto_attack_damage(game_object* target, bool include_calibrum_mark);
        static float get_q_damage(WeaponType weapon, game_object* target);
        static float get_r_damage(WeaponType weapon, game_object* target);
        static float get_mark_damage(game_object* target, WeaponType weapon);
        static bool can_kill_with_combo(game_object* target);
        static float calculate_dps(WeaponType weapon, game_object* target);
        static float predict_incoming_damage(game_object* target, float time_window);
    };

    // Anti-magneting functions
    bool check_for_magneting();
    void handle_anti_magneting();

    // Safe buff handling functions
    bool has_weapon_buff(game_object* player, const std::string& buff_name);
    buff_instance* get_buff_by_hash_safe(game_object* obj, uint32_t hash);
    int get_buff_stacks_safe(buff_instance* buff);
    float get_buff_count_safe(buff_instance* buff);
    bool has_calibrum_mark(game_object* target);
    bool has_gravitum_debuff(game_object* target);
    bool is_rooted_by_gravitum(game_object* target);
    void draw_weapon_icon(WeaponType weapon, math::vector2 pos, float size, bool is_main);

    // Core module functions
    void load();
    void unload();
    void create_menu();
    void __fastcall on_update();
    void __fastcall on_draw();
    void __fastcall on_wndproc(uint32_t msg, uint32_t wparam, uint32_t lparam);
    void log_debug(const char* format, ...);

    // Ability casting functions
    void cast_calibrum_q();
    void cast_severum_q();
    void cast_gravitum_q();
    void cast_infernum_q();
    void cast_crescendum_q();
    void cast_r();
    void cast_w();
    bool handle_anti_gapcloser();

    // Orbwalker mode handlers
    void handle_combo();
    void handle_harass();
    void handle_farming();
    void handle_flee();

    // Utility functions
    void optimize_weapon_rotation();
    float get_weapon_range(WeaponType weapon);
    bool can_cast_spell(int spell_slot, float delay = 0.f);
    void weave_attack(game_object* target);
    bool is_safe_position(const math::vector3& pos);
    bool is_under_enemy_turret(const math::vector3& pos);
    void update_infotab();

    // Drawing functions
    void draw_weapon_info();
    void draw_damage_indicators();
    void draw_ranges();
    void draw_ammo_bars();
    void draw_weapon_queue();
    std::string get_weapon_name(WeaponType weapon);

    extern Settings settings;
    extern bool initialized;
    extern menu_category* menu;
    extern uint32_t infotab_text_id;
};