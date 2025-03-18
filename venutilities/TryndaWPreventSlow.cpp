#include "TryndaWPreventSlow.hpp"
#include "sdk.hpp"
#include <cmath>

namespace venutilities {
    namespace trynda_w_prevent_slow {

        bool enabled = true;
        menu_category* menu = nullptr;

        bool initialize_dependencies() {
            if (!sdk_init::infotab()) {
                g_sdk->log_console("[!] TryndaWPreventSlow: Failed to initialize menu system");
                return false;
            }
            return true;
        }

        void setup_menu(menu_category* parent_menu) {
            if (!parent_menu) return;

            menu = parent_menu->add_sub_category("trynda_w_prevent", "Tryndamere W Prevent Slow");
            if (!menu) return;

            menu->add_checkbox("enabled", "Enabled", enabled, [](bool val) { enabled = val; });
        }

        void __fastcall on_process_cast(game_object* caster, spell_cast* cast) {
            if (!enabled) return;
            if (!caster || !caster->is_hero() || caster->get_char_name() != "Tryndamere") return;

            if (!cast || !cast->get_spell_data() || !cast->get_spell_data()->get_static_data()) return;

            const char* spell_name = cast->get_spell_data()->get_static_data()->get_name();
            if (!spell_name || strcmp(spell_name, "TryndamereW") != 0) return;

            auto player = g_sdk->object_manager->get_local_player();
            if (!player || player->is_dead()) return;

            // Calculate the direction vector from player to Tryndamere
            math::vector3 player_pos = player->get_position();
            math::vector3 trynda_pos = caster->get_position();

            // We want to face directly toward Tryndamere
            math::vector3 direction_to_trynda = (trynda_pos - player_pos).normalized();
            math::vector3 face_position = player_pos + (direction_to_trynda * 100.0f);

            if (g_sdk->nav_mesh->is_pathable(face_position)) {
                player->issue_order(game_object_order::move_to, face_position);
                g_sdk->log_console("[+] TryndaWPreventSlow: Immediately facing Tryndamere!");
            }
        }

        void load(menu_category* main_menu) {
            g_sdk->log_console("[+] Loading Tryndamere W Prevent Slow...");

            if (!initialize_dependencies()) {
                g_sdk->log_console("[!] TryndaWPreventSlow: Dependency initialization failed!");
                return;
            }
            //if (g_sdk->menu_manager && main_menu) {
            //    setup_menu(main_menu);
            //}
            if (g_sdk->event_manager) {
                g_sdk->event_manager->register_callback(event_manager::event::process_cast, reinterpret_cast<void*>(&on_process_cast));
                g_sdk->log_console("[+] TryndaWPreventSlow: Event callbacks registered.");
            }

            g_sdk->log_console("[+] Tryndamere W Prevent Slow loaded successfully");
        }

        void unload() {
            g_sdk->log_console("[-] Unloading Tryndamere W Prevent Slow...");
            if (g_sdk->event_manager) {
                g_sdk->event_manager->unregister_callback(event_manager::event::process_cast, reinterpret_cast<void*>(&on_process_cast));
                g_sdk->log_console("[-] TryndaWPreventSlow: Event callbacks unregistered.");
            }
            g_sdk->log_console("[-] Tryndamere W Prevent Slow unloaded");
        }

    } // namespace trynda_w_prevent_slow
} // namespace venutilities