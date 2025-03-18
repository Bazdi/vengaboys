#pragma once

#include "sdk.hpp"

namespace venutilities {
    namespace trynda_w_prevent_slow {

        extern bool enabled;
        extern float reaction_delay;

        void load(menu_category* main_menu);
        void unload();
        void setup_menu(menu_category* parent_menu);
        bool initialize_dependencies();
    } // namespace trynda_w_prevent_slow
} // namespace venutilities