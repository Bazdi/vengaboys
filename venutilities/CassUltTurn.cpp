#include "CassUltTurn.hpp"
#include "sdk.hpp"
#include <cmath>
#include <limits>
#include <string.h>

namespace venutilities {
    namespace cass_ult_turn {

        bool enabled = true;
        float last_ult_time = 0.0f;
        float ult_cone_angle = 80.0f; // Cassiopeia R cone angle

        menu_category* menu = nullptr;
        bool is_cassiopeia_ulting = false;
        game_object* ulting_cassiopeia = nullptr;

        bool initialize_dependencies() {
            if (!sdk_init::infotab()) {
                g_sdk->log_console("[!] CassUltTurn: Failed to initialize menu system");
                return false;
            }
            return true;
        }

        void setup_menu(menu_category* parent_menu) {
            if (!parent_menu) return;

            menu = parent_menu->add_sub_category("cass_ult_turn", "Cassiopeia Ult Turn");
            if (!menu) return;

            menu->add_checkbox("enabled", "Enabled", enabled, [](bool val) { enabled = val; });
        }

        bool is_facing_cassiopeia(const math::vector3& player_pos, const math::vector3& cass_pos, const math::vector3& cass_direction) {
            math::vector3 to_player = (player_pos - cass_pos).normalized();
            float dot_product = cass_direction.dot(to_player);
            float angle = std::acos(dot_product) * (180.0f / 3.14159f);
            return angle <= ult_cone_angle * 0.5f;
        }

        void __fastcall on_process_cast(game_object* caster, spell_cast* cast) {
            if (!enabled || !caster || !caster->is_hero()) return;

            // Check if it's Cassiopeia
            const char* char_name = caster->get_char_name().c_str();
            if (!char_name || strcmp(char_name, "Cassiopeia") != 0) return;

            // Verify it's her ultimate
            if (!cast || !cast->get_spell_data() || !cast->get_spell_data()->get_static_data()) return;

            const char* spell_name = cast->get_spell_data()->get_static_data()->get_name();
            if (!spell_name || strcmp(spell_name, "CassiopeiaR") != 0) return;

            auto player = g_sdk->object_manager->get_local_player();
            if (!player || player->is_dead()) return;

            // Check if we're in range and facing danger
            math::vector3 player_pos = player->get_position();
            math::vector3 cass_pos = caster->get_position();

            float ult_range = 850.0f; // Cassiopeia R range
            if (player_pos.distance(cass_pos) <= ult_range) {
                math::vector3 cass_direction = caster->get_direction();

                if (is_facing_cassiopeia(player_pos, cass_pos, cass_direction)) {
                    // Calculate turn position - 180 degrees from Cassiopeia
                    math::vector3 direction_to_cass = (cass_pos - player_pos).normalized();
                    math::vector3 opposite_direction = direction_to_cass * -1.0f;
                    math::vector3 turn_pos = player_pos + (opposite_direction * 100.0f);

                    // Validate position and issue move order
                    if (g_sdk->nav_mesh->is_pathable(turn_pos)) {
                        player->issue_order(game_object_order::move_to, turn_pos);
                        g_sdk->log_console("[+] CassUltTurn: Turned away from Cassiopeia Ult!");
                    }
                }
            }
        }

        void load(menu_category* main_menu) {
            g_sdk->log_console("[+] Loading Cassiopeia Ult Turn...");

            if (!initialize_dependencies()) {
                g_sdk->log_console("[!] CassUltTurn: Dependency initialization failed!");
                return;
            }

            //if (g_sdk->menu_manager && main_menu) {
            //    setup_menu(main_menu);
            //}

            if (g_sdk->event_manager) {
                g_sdk->event_manager->register_callback(event_manager::event::process_cast, reinterpret_cast<void*>(&on_process_cast));
                g_sdk->log_console("[+] CassUltTurn: Event callbacks registered.");
            }

            g_sdk->log_console("[+] Cassiopeia Ult Turn loaded successfully");
        }

        void unload() {
            g_sdk->log_console("[-] Unloading Cassiopeia Ult Turn...");

            if (g_sdk->event_manager) {
                g_sdk->event_manager->unregister_callback(event_manager::event::process_cast, reinterpret_cast<void*>(&on_process_cast));
                g_sdk->log_console("[-] CassUltTurn: Event callbacks unregistered.");
            }

            g_sdk->log_console("[-] Cassiopeia Ult Turn unloaded");
        }

    } // namespace cass_ult_turn
} // namespace venutilities