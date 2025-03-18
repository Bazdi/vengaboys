#pragma once
#include "sdk.hpp"
#include "OrderManager.hpp"
#include <vector>
#include <string>

namespace teemo {
    // Mushroom spot data structure
    struct MushroomSpot {
        std::string name;
        math::vector3 position;
        bool enabled = true;
    };

    // Q-Priority target system
    enum class QTargetPriority {
        AUTO = 0,            // Use default target selection
        LOWEST_HEALTH = 1,   // Target lowest health enemy
        HIGHEST_DAMAGE = 2,  // Target highest damage dealer
        MOST_DANGEROUS = 3,  // Target most dangerous (based on threat score)
        CLOSEST = 4,         // Target closest enemy
        CUSTOM = 5           // Custom priority (define in settings)
    };

    // Target evaluation scores
    struct TargetScore {
        game_object* target = nullptr;
        float hp_score = 0.f;
        float damage_score = 0.f;
        float distance_score = 0.f;
        float threat_score = 0.f;
        float total_score = 0.f;
    };

    // Main settings structure
    struct Settings {
        // Combat settings
        bool use_q_combo = true;
        bool use_w_combo = true;
        bool use_r_combo = true;

        // Harass settings
        bool use_q_harass = true;
        bool use_aa_harass = true;
        float harass_mana_threshold = 30.f; // Minimum mana % to use abilities in harass

        // Lane clear settings
        bool use_q_clear = false;
        bool use_r_clear = false;
        int r_clear_min_minions = 3;
        float clear_mana_threshold = 40.f;

        // Jungle clear settings
        bool use_q_jungle = true;
        bool use_r_jungle = true;
        bool use_r_objectives = true;

        // Flee settings
        bool use_w_flee = true;
        bool drop_r_when_fleeing = true;

        // Q settings
        QTargetPriority q_priority = QTargetPriority::AUTO;
        bool q_auto_harass = false;
        bool q_ks = true;
        bool save_q_for_blind = false;
        bool blind_adc_priority = true;

        // W settings
        bool w_auto_use = true;
        float w_hp_threshold = 60.f;
        bool w_chase = true;
        bool w_dodge_skillshot = true;

        // R settings
        bool r_auto_place = true;
        bool r_use_saved_spots = true;
        float r_min_distance = 300.f;
        bool r_in_bush_priority = true;
        int r_max_stacks = 0; // 0 means use all available
        int r_maintain_reserve = 1; // Keep this many mushrooms in reserve
        bool r_block_paths = true;
        bool r_on_immobile = true;

        // Drawing settings
        bool draw_q_range = true;
        bool draw_r_range = true;
        bool draw_r_spots = true;
        bool draw_damage_prediction = true;
        uint32_t draw_q_color = 0x8800FF00;
        uint32_t draw_r_color = 0x88FF0000;
        uint32_t draw_spot_color = 0xFFFFFF00;
        uint32_t draw_enabled_spot_color = 0xFF00FF00;
        uint32_t draw_disabled_spot_color = 0xFFFF0000;

        // Developer mode settings
        bool dev_mode = false;
        std::string* save_spot_key = nullptr;
        std::string* delete_spot_key = nullptr;
        std::string* toggle_spot_key = nullptr;
        std::string spot_name_prefix = "Spot";
        std::string mushroom_spots_file = "teemo_mushrooms.txt";

        // Debug settings
        bool debug_mode = false;
    };

    // Combo state tracking
    struct ComboState {
        game_object* current_target = nullptr;
        float last_q_time = 0.f;
        float last_w_time = 0.f;
        float last_r_time = 0.f;
        float last_state_update = 0.f;
        float danger_level = 0.f;
        bool fleeing = false;
        bool in_bush = false;
        std::vector<game_object*> nearby_enemies;
        std::vector<game_object*> nearby_allies;
        int q_target_network_id = 0;
    };

    // Developer mode state
    struct DevModeState {
        bool selecting_spot = false;
        int spot_index = 0;
        bool editing_spot = false;
        MushroomSpot current_edit_spot;
        game_object* hovered_mushroom = nullptr;
        float last_spot_save_time = 0.f;
    };

    // Main module class
    class TeemoModule {
    public:
        static TeemoModule& get_instance();

        void initialize();
        void load();
        void unload();

        void update();
        void draw();
        void process_spell_cast(game_object* sender, spell_cast* cast);
        bool before_attack(orb_sdk::event_data* data);

        bool can_cast_q();
        bool can_cast_w();
        bool can_cast_r();
        float get_q_damage(game_object* target);
        float get_r_damage(game_object* target);
        float get_e_damage(game_object* target);
        float calculate_total_damage(game_object* target);

        void handle_combo();
        void handle_harass();
        void handle_lane_clear();
        void handle_jungle_clear();
        void handle_flee();

        // R spot management
        void save_current_spot();
        void delete_spot(int index);
        void toggle_spot(int index);
        void load_mushroom_spots();
        void save_mushroom_spots();
        MushroomSpot* get_best_mushroom_spot();

        // Q priority system
        game_object* get_q_target();
        bool should_use_q_on(game_object* target);

        // Helper functions
        void log_debug(const char* format, ...);
        bool is_target_valid(game_object* target);
        bool is_in_danger();
        void update_combat_state();

    private:
        TeemoModule();
        TeemoModule(const TeemoModule&) = delete;
        TeemoModule& operator=(const TeemoModule&) = delete;

        void create_menu();

        bool initialized = false;
        menu_category* menu = nullptr;
        uint32_t infotab_text_id = 0;

        Settings settings;
        ComboState combat_state;
        DevModeState dev_state;

        std::vector<MushroomSpot> mushroom_spots;
        std::vector<TargetScore> target_scores;
    };

    void __fastcall on_update();
    void __fastcall on_draw();
    bool __fastcall on_before_attack(orb_sdk::event_data* data);
    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast);

    void load();
    void unload();
}