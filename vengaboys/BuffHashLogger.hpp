#pragma once
#include "sdk.hpp"
#include <fstream>

namespace venutilities {
    namespace buff_hash_logger {

        extern std::ofstream log_file; // Declare global log file stream

        // Function declarations
        bool initialize_dependencies();
        void __fastcall on_buff_gain(game_object* object, buff_instance* buff);
        void load(); // No menu_category* argument needed as it's auto-running
        void unload();
    }
}