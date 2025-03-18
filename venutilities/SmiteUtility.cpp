#include "SmiteUtility.hpp"
#include "OrderManager.hpp"

namespace venutilities {
    namespace smite_utility {

        // Static instance
        SmiteUtility* g_smite_instance = nullptr;

        void SmiteUtility::initialize(menu_category* parent_menu) {
            g_smite_instance = this;

            menu = parent_menu->add_sub_category("smite_utility", "Smite Utility");
            if (!menu) return;

            // Core settings
            menu->add_checkbox("enabled", "Enable Smite Utility", settings.enabled,
                [this](bool val) { settings.enabled = val; });
            menu->add_checkbox("use_on_champions", "Smite Champions", settings.use_on_champions,
                [this](bool val) { settings.use_on_champions = val; });
            menu->add_checkbox("keep_one_charge", "Keep Charge for Objectives", settings.keep_one_charge,
                [this](bool val) { settings.keep_one_charge = val; });
            menu->add_checkbox("delay_void_grubs", "Delay Void Grub Smite", settings.delay_void_grubs,
                [this](bool val) { settings.delay_void_grubs = val; });

            // Champion smite settings
            auto champion_menu = menu->add_sub_category("champion_settings", "Champion Settings");
            if (champion_menu) {
                champion_menu->add_slider_float("champion_hp_threshold", "Champion HP Threshold %", 0.f, 100.f, 1.f,
                    settings.champion_hp_threshold, [this](float val) { settings.champion_hp_threshold = val; });
                champion_menu->add_checkbox("smite_in_combo", "Use in Combo Only", settings.smite_in_combo,
                    [this](bool val) { settings.smite_in_combo = val; });
            }

            // Register for game_update event
            if (g_sdk->event_manager) {
                g_sdk->event_manager->register_callback(event_manager::event::game_update, reinterpret_cast<void*>(&SmiteUtility::on_game_update));
                g_sdk->event_manager->register_callback(event_manager::event::draw_world, reinterpret_cast<void*>(&SmiteUtility::on_draw_world));
                g_sdk->log_console("[+] SmiteUtility: Event callbacks registered.");
            }
        }

        void SmiteUtility::cleanup() {
            if (g_sdk->event_manager) {
                g_sdk->event_manager->unregister_callback(event_manager::event::game_update, reinterpret_cast<void*>(&SmiteUtility::on_game_update));
                g_sdk->event_manager->unregister_callback(event_manager::event::draw_world, reinterpret_cast<void*>(&SmiteUtility::on_draw_world));
                g_sdk->log_console("[-] SmiteUtility: Event callbacks unregistered.");
            }
        }

        int SmiteUtility::get_smite_slot() {
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return -1;

            for (int slot = 4; slot <= 5; slot++) {
                auto spell = player->get_spell(slot);
                if (!spell) continue;

                auto spell_data = spell->get_data();
                if (!spell_data || !spell_data->get_static_data()) continue;

                const char* name = spell_data->get_static_data()->get_name();
                if (name && strcmp(name, "SummonerSmite") == 0) {
                    if (spell->get_cooldown() <= 0.f) {
                        g_sdk->log_console("[SmiteUtility] Found active smite in slot %d", slot);
                        return slot;
                    }
                }
            }
            return -1;
        }

        bool SmiteUtility::should_delay_void_grub(game_object* monster) {
            if (!settings.delay_void_grubs) return false;
            if (!monster || monster->get_name().find("VoidGrub") == std::string::npos) return false;

            auto it = monster_tracker.find(monster->get_network_id());
            if (it == monster_tracker.end()) {
                monster_tracker[monster->get_network_id()] = MonsterInfo{
                    .spawn_time = g_sdk->clock_facade->get_game_time()
                };
                return true;
            }

            if (!it->second.first_seen) return false;

            float current_time = g_sdk->clock_facade->get_game_time();
            if (current_time - it->second.spawn_time >= 1.0f) { // Delay by 1 second
                it->second.first_seen = false;
                return false;
            }
            return true;
        }

        bool SmiteUtility::is_being_contested(game_object* monster) {
            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return true;

            for (auto hero : g_sdk->object_manager->get_heroes()) {
                if (!hero->is_valid() || hero->is_dead() ||
                    hero->get_team_id() == player->get_team_id())
                    continue;

                if (hero->get_position().distance(monster->get_position()) <= 1000.f) {
                    return true;
                }
            }
            return false;
        }

        float SmiteUtility::calculate_monster_dps(game_object* monster) {
            auto it = monster_tracker.find(monster->get_network_id());
            if (it == monster_tracker.end()) {
                monster_tracker[monster->get_network_id()] = MonsterInfo{
                    .last_hp = monster->get_hp(),
                    .last_damage_calc_time = g_sdk->clock_facade->get_game_time()
                };
                return 0.f;
            }

            auto& info = it->second;
            float current_time = g_sdk->clock_facade->get_game_time();
            float time_diff = current_time - info.last_damage_calc_time;

            if (time_diff >= 0.25f) { // Update every 0.25 seconds
                float hp_diff = info.last_hp - monster->get_hp();
                info.damage_per_second = hp_diff / time_diff;
                info.last_hp = monster->get_hp();
                info.last_damage_calc_time = current_time;
                info.being_contested = is_being_contested(monster);
            }

            return info.damage_per_second;
        }

        float SmiteUtility::get_smite_damage(int level) {
            if (level >= 15) return 1200.f;
            if (level >= 10) return 900.f;
            return 600.f;
        }

        bool SmiteUtility::should_keep_smite_charge() {
            if (!settings.keep_one_charge) return false;

            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;

            for (auto monster : g_sdk->object_manager->get_monsters()) {
                if (!monster->is_valid() || monster->is_dead()) continue;

                if (monster->is_epic_monster() &&
                    monster->get_position().distance(player->get_position()) <= 2000.f) {
                    return true;
                }
            }

            return false;
        }

        bool SmiteUtility::should_smite_monster(game_object* monster) {
            if (!monster || !settings.enabled) return false;

            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return false;

            float smite_damage = get_smite_damage(player->get_level());

            // Check for void grub delay
            if (should_delay_void_grub(monster)) return false;

            // Simple but perfect execution - smite when we can kill
            if (monster->get_hp() <= smite_damage) {
                // For epic monsters, ensure we're the closest to avoid stealing
                if (monster->is_epic_monster()) {
                    float our_distance = player->get_position().distance(monster->get_position());

                    // Check if any enemy jungler is closer
                    for (auto hero : g_sdk->object_manager->get_heroes()) {
                        if (!hero->is_valid() || hero->is_dead() ||
                            hero->get_team_id() == player->get_team_id())
                            continue;

                        // Check if they have smite
                        bool has_smite = false;
                        for (int slot = 4; slot <= 5; slot++) {
                            auto spell = hero->get_spell(slot);
                            if (!spell) continue;
                            auto data = spell->get_data();
                            if (!data || !data->get_static_data()) continue;
                            const char* name = data->get_static_data()->get_name();
                            if (name && strcmp(name, "SummonerSmite") == 0) {
                                has_smite = true;
                                break;
                            }
                        }

                        if (has_smite) {
                            float their_distance = hero->get_position().distance(monster->get_position());
                            if (their_distance < our_distance) {
                                // Enemy jungler is closer, might be risky to smite
                                return false;
                            }
                        }
                    }
                }

                return true;
            }

            return false;
        }

        bool SmiteUtility::should_smite_champion(game_object* target) {
            if (!target || !settings.use_on_champions) return false;

            float hp_percent = (target->get_hp() / target->get_max_hp()) * 100.f;
            return hp_percent <= settings.champion_hp_threshold;
        }

        void __fastcall SmiteUtility::on_game_update() {
            if (!g_smite_instance || !g_smite_instance->settings.enabled) return;
            g_smite_instance->update();
        }

        void __fastcall SmiteUtility::on_draw_world() {
            if (!g_smite_instance || !g_smite_instance->settings.enabled) return;
            g_smite_instance->draw();
        }

        void SmiteUtility::update() {
            if (!settings.enabled) return;

            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return;

            int smite_slot = get_smite_slot();
            if (smite_slot == -1) return;

            // Handle epic monsters first
            for (auto monster : g_sdk->object_manager->get_monsters()) {
                if (!monster->is_valid() || monster->is_dead() ||
                    monster->get_position().distance(player->get_position()) > SMITE_RANGE)
                    continue;

                if (monster->is_epic_monster() && should_smite_monster(monster)) {
                    player->cast_spell(smite_slot, monster);
                    g_sdk->log_console("[SmiteUtility] Smited epic monster: %s", monster->get_name().c_str());
                    return;
                }
            }

            // Handle champions if in combo
            if (settings.smite_in_combo && sdk::orbwalker && sdk::orbwalker->combo()) {
                for (auto hero : g_sdk->object_manager->get_heroes()) {
                    if (!hero->is_valid() || hero->is_dead() ||
                        hero->get_team_id() == player->get_team_id() ||
                        hero->get_position().distance(player->get_position()) > SMITE_RANGE)
                        continue;

                    if (should_smite_champion(hero)) {
                        player->cast_spell(smite_slot, hero);
                        g_sdk->log_console("[SmiteUtility] Smited champion: %s", hero->get_char_name().c_str());
                        return;
                    }
                }
            }

            // Handle other monsters if we don't need to save charge
            if (!should_keep_smite_charge()) {
                for (auto monster : g_sdk->object_manager->get_monsters()) {
                    if (!monster->is_valid() || monster->is_dead() ||
                        monster->get_position().distance(player->get_position()) > SMITE_RANGE)
                        continue;

                    if (should_smite_monster(monster)) {
                        player->cast_spell(smite_slot, monster);
                        g_sdk->log_console("[SmiteUtility] Smited monster: %s", monster->get_name().c_str());
                        return;
                    }
                }
            }
        }

        void SmiteUtility::draw() {
            if (!settings.enabled) return;

            auto player = g_sdk->object_manager->get_local_player();
            if (!player) return;

            // Draw smite range
            g_sdk->renderer->add_circle_3d(player->get_position(), SMITE_RANGE, 2.f, 0x20FFFF00);

            // Draw damage indicators on monsters
            float smite_damage = get_smite_damage(player->get_level());
            for (auto monster : g_sdk->object_manager->get_monsters()) {
                if (!monster->is_valid() || monster->is_dead()) continue;

                if (monster->is_epic_monster() || monster->is_large_monster()) {
                    g_sdk->renderer->add_damage_indicator(monster, smite_damage);

                    // Draw contested status if being tracked
                    auto it = monster_tracker.find(monster->get_network_id());
                    if (it != monster_tracker.end() && it->second.being_contested) {
                        math::vector2 screen_pos = g_sdk->renderer->world_to_screen(monster->get_position());
                        g_sdk->renderer->add_text("CONTESTED", 14.f, screen_pos, 1, 0xFFFF0000);
                    }
                }
            }
        }

    } // namespace smite_utility
} // namespace venutilities