#include "FiddleTracker.hpp"
#include "sdk.hpp"
#include <cmath>
#include <limits>

namespace venutilities {
    namespace fiddle_tracker {
        // Define global variables (declared as extern in header)
        uint32_t menu_draw_color = 0xFF00FF00;
        bool menu_draw_text_warning = true;
        uint32_t menu_text_color = 0xFFFFFFFF;
        float menu_text_size = 20.f;
        int menu_text_pos_x = 200;
        int menu_text_pos_y = 100;
        bool enable_debug_log = false;
        menu_category* menu = nullptr;
        bool is_fiddlesticks_ulting = false;
        math::vector3 last_ult_position{};
        float last_ult_time = 0.f;

        bool initialize_dependencies() {
            if (!sdk_init::infotab()) {
                if (enable_debug_log) g_sdk->log_console("[!] FiddleTracker: Failed to initialize menu system");
                return false;
            }
            if (!sdk_init::notification()) {
                if (enable_debug_log) g_sdk->log_console("[!] FiddleTracker: Failed to initialize notification system");
                return false;
            }
            return true;
        }

        void setup_menu(menu_category* parent_menu) {
            if (!parent_menu) return;

            menu = parent_menu->add_sub_category("fiddle_tracker", "Fiddlesticks Tracker");
            if (!menu) return;

            menu->add_label("Fiddlesticks Ultimate Tracker (Text Only)");
            menu->add_checkbox("draw_text_warning", "Draw Text Warning", menu_draw_text_warning,
                [](bool val) { menu_draw_text_warning = val; });
            menu->add_colorpicker("text_color", "Text Color", menu_text_color,
                [](uint32_t color) { menu_text_color = color; });
            menu->add_slider_float("text_size", "Text Size", 10.f, 40.f, 1.f, menu_text_size,
                [](float val) { menu_text_size = val; });
            menu->add_slider_int("text_pos_x", "Text Position X", 0, 1920, 10, menu_text_pos_x,
                [](int val) { menu_text_pos_x = val; });
            menu->add_slider_int("text_pos_y", "Text Position Y", 0, 1080, 10, menu_text_pos_y,
                [](int val) { menu_text_pos_y = val; });
            menu->add_checkbox("enable_debug_log", "Enable Debug Logging", enable_debug_log,
                [](bool value) { enable_debug_log = value; });
        }

        void log_ally_buffs() {
            if (enable_debug_log) g_sdk->log_console("--- Ally Buffs (Start) ---");
            auto local_player = g_sdk->object_manager->get_local_player();
            if (!local_player) {
                if (enable_debug_log) g_sdk->log_console("[FiddleTracker Debug] log_ally_buffs: No local player found.");
                return;
            }

            for (auto hero : g_sdk->object_manager->get_heroes()) {
                if (!hero->is_valid() || hero->is_dead() || hero->get_team_id() != local_player->get_team_id()) {
                    continue;
                }

                if (enable_debug_log) g_sdk->log_console("--- Ally Hero: %s ---", hero->get_char_name().c_str());
                auto buffs = hero->get_buffs();
                if (buffs.empty()) {
                    if (enable_debug_log) g_sdk->log_console("No buffs on %s", hero->get_char_name().c_str());
                }
                else {
                    for (const auto& buff : buffs) {
                        if (enable_debug_log) g_sdk->log_console("Buff Name: %s, Type: %d, Count: %d, Stacks: %d, Caster: %s",
                            buff->get_name().c_str(),
                            static_cast<int>(buff->get_type()),
                            buff->get_count(),
                            buff->get_stacks(),
                            buff->get_caster() ? buff->get_caster()->get_char_name().c_str() : "Unknown Caster");
                    }
                }
            }
            if (enable_debug_log) g_sdk->log_console("--- Ally Buffs (End) ---");
        }

        void log_ally_buffs_periodic() {
            log_ally_buffs();
        }

        void __fastcall on_buff_gain(game_object* target, buff_instance* buff) {
            if (!target || !buff) return;

            if (buff->get_name() == "fiddlerevealedvfx") {
                if (enable_debug_log) g_sdk->log_console("[FiddleTracker Debug - Buff Event] Buff Gain: FiddleRevealedVFX on target: %s", target->get_char_name().c_str());
                is_fiddlesticks_ulting = true;
                last_ult_time = g_sdk->clock_facade->get_game_time();
                sdk::notification->add("Fiddlesticks Ultimate Alert", "Fiddlesticks Ultimate Detected!", color(menu_draw_color));
            }
        }

        void __fastcall on_buff_loss(game_object* target, buff_instance* buff) {
            if (!target || !buff) return;

            if (buff->get_name() == "fiddlerevealedvfx") {
                if (enable_debug_log) g_sdk->log_console("[FiddleTracker Debug - Buff Event] Buff Loss: FiddleRevealedVFX on target: %s", target->get_char_name().c_str());
                is_fiddlesticks_ulting = false;
            }
        }

        void __fastcall on_draw_world() {
            if (!is_fiddlesticks_ulting) return;

            if (menu_draw_text_warning) {
                math::vector2 text_position = { static_cast<float>(menu_text_pos_x), static_cast<float>(menu_text_pos_y) };
                g_sdk->renderer->add_text("FIDDLE ULT CALLED!", menu_text_size, text_position, 0, menu_text_color);
            }
        }

        void __fastcall on_game_update() {
            log_ally_buffs_periodic();
        }

        void load(menu_category* main_menu) {
            g_sdk->log_console("[+] Loading Fiddlesticks Tracker (Text Warning Only)...");

            if (!initialize_dependencies()) {
                g_sdk->log_console("[!] Fiddlesticks Tracker (Text Warning Only): Dependency initialization failed!");
                return;
            }
            //if (g_sdk->menu_manager && main_menu) {
            //    setup_menu(main_menu);
            //}
            if (g_sdk->event_manager) {
                g_sdk->event_manager->register_callback(event_manager::event::buff_gain, reinterpret_cast<void*>(&on_buff_gain));
                g_sdk->event_manager->register_callback(event_manager::event::buff_loss, reinterpret_cast<void*>(&on_buff_loss));
                g_sdk->event_manager->register_callback(event_manager::event::draw_world, reinterpret_cast<void*>(&on_draw_world));
                g_sdk->event_manager->register_callback(event_manager::event::game_update, reinterpret_cast<void*>(&on_game_update));
                g_sdk->log_console("[+] Fiddlesticks Tracker (Text Warning Only): Event callbacks registered.");
            }

            g_sdk->log_console("[+] Fiddlesticks Tracker (Text Warning Only) loaded successfully");
        }

        void unload() {
            g_sdk->log_console("[-] Unloading Fiddlesticks Tracker (Text Warning Only)...");
            if (g_sdk->event_manager) {
                g_sdk->event_manager->unregister_callback(event_manager::event::buff_gain, reinterpret_cast<void*>(&on_buff_gain));
                g_sdk->event_manager->unregister_callback(event_manager::event::buff_loss, reinterpret_cast<void*>(&on_buff_loss));
                g_sdk->event_manager->unregister_callback(event_manager::event::draw_world, reinterpret_cast<void*>(&on_draw_world));
                g_sdk->event_manager->unregister_callback(event_manager::event::game_update, reinterpret_cast<void*>(&on_game_update));
                g_sdk->log_console("[-] Fiddlesticks Tracker (Text Warning Only): Event callbacks unregistered.");
            }
            g_sdk->log_console("[-] Fiddlesticks Tracker (Text Warning Only) unloaded");
        }

    } // namespace fiddle_tracker
} // namespace venutilities