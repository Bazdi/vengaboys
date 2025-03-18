#pragma once
#include "sdk.hpp"

namespace venutilities {
    namespace fiddle_tracker {
        // Global variables
        extern uint32_t menu_draw_color;
        extern bool menu_draw_text_warning;
        extern uint32_t menu_text_color;
        extern float menu_text_size;
        extern int menu_text_pos_x;
        extern int menu_text_pos_y;
        extern bool enable_debug_log;
        extern menu_category* menu;
        extern bool is_fiddlesticks_ulting;
        extern math::vector3 last_ult_position;
        extern float last_ult_time;

        // Function declarations
        bool initialize_dependencies();
        void setup_menu(menu_category* parent_menu);
        void log_ally_buffs();
        void log_ally_buffs_periodic();
        void __fastcall on_buff_gain(game_object* target, buff_instance* buff);
        void __fastcall on_buff_loss(game_object* target, buff_instance* buff);
        void __fastcall on_draw_world();
        void __fastcall on_game_update();
        void load(menu_category* main_menu);
        void unload();
    }
}