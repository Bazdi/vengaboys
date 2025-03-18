// jhin.hpp
#pragma once
#include "sdk.hpp"
#include <vector>

namespace jhin_damage_tracker {

    // Fully define the struct in the header
    struct JhinDamageData {
        game_object* target = nullptr;
        float expected_damage = 0.f;
        bool is_fourth_shot = false;
        bool is_crit = false;
        float time = 0.f;
    };

    // Function declarations
    void on_basic_attack(game_object* attacker, game_object* target, spell_cast* cast);
    void on_spell_hit(game_object* attacker, spell_cast* cast);
    void on_draw();
    void on_update();
    bool has_item(game_object* entity, int item_id);
    bool is_attack_fourth_shot(game_object* jhin);
    float calculate_base_crit_damage(game_object* jhin, game_object* target);
    float calculate_bonus_damage(game_object* jhin, game_object* target);
    float calculate_total_damage_before_armor(float base_crit_damage, float bonus_damage);
    float calculate_armor_reduction(float total_damage, game_object* target, game_object* source);
    float calculate_jhin_4th_shot_damage(game_object* jhin, game_object* target);
    void load();
    void unload();

    // Define this as extern so it can be used across translation units
    extern std::vector<JhinDamageData> damage_history;

} // namespace jhin_damage_tracker