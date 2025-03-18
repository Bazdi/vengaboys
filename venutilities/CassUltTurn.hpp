#pragma once

#include "sdk.hpp"

namespace venutilities {
    namespace cass_ult_turn {

        extern bool enabled;
        extern float reaction_delay;
        extern float last_ult_time;

        void load(menu_category* main_menu);
        void unload();
        void setup_menu(menu_category* parent_menu);
        bool initialize_dependencies();
    } // namespace cass_ult_turn
} // namespace venutilities