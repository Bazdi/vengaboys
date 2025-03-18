#pragma once

#include "sdk.hpp"
#include <unordered_map>

namespace venutilities {
    namespace smite_utility {

        // In SmiteSettings (SmiteUtility.hpp)
        struct SmiteSettings {
            bool enabled = true;
            bool use_on_champions = true;
            bool keep_one_charge = true;
            bool delay_void_grubs = true;  // Delay smite on void grubs for better clear

            // Champion smite settings
            float champion_hp_threshold = 30.f;
            bool smite_in_combo = true;
        };

        class SmiteUtility {
        private:
            static constexpr float SMITE_RANGE = 500.f;
            menu_category* menu = nullptr;
            SmiteSettings settings;

            struct MonsterInfo {
                float spawn_time = 0.f;
                bool first_seen = true;
                bool being_contested = false;
                float last_hp = 0.f;
                float damage_per_second = 0.f;
                float last_damage_calc_time = 0.f;
            };

            std::unordered_map<uint32_t, MonsterInfo> monster_tracker;

            bool should_delay_void_grub(game_object* monster);
            bool is_being_contested(game_object* monster);
            float calculate_monster_dps(game_object* monster);
            float get_smite_damage(int level);
            bool should_keep_smite_charge();
            bool should_smite_monster(game_object* monster);
            bool should_smite_champion(game_object* target);
            int get_smite_slot();

        public:
            void initialize(menu_category* parent_menu);
            void cleanup();
            void update();
            void draw();

            // Static event handlers
            static void __fastcall on_game_update();
            static void __fastcall on_draw_world();
        };

        // Global instance pointer for static callbacks
        extern SmiteUtility* g_smite_instance;

    } // namespace smite_utility
} // namespace venutilities