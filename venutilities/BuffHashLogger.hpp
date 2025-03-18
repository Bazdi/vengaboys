#pragma once

#include "sdk.hpp"
#include <fstream>
#include <map>
#include <set>
#include <string>

namespace venutilities {
    namespace buff_hash_logger {

        // Forward declarations
        void load();
        void unload();
        bool initialize_dependencies();

        // Maps and sets for tracking
        extern std::map<std::string, std::ofstream> champion_log_files;
        extern std::map<std::string, std::set<uint32_t>> champion_logged_buffs;
        extern std::map<std::string, std::ofstream> particle_log_files;
        extern std::map<std::string, std::set<uint32_t>> logged_particles;

        // Helper functions
        std::string sanitize_filename(const std::string& filename);
        bool is_buff_already_logged(const std::string& champion_name, uint32_t buff_hash);
        void log_buff(const std::string& champion_name, const std::string& buff_name, uint32_t buff_hash);
        void log_particle(game_object* owner, const std::string& particle_name, uint32_t particle_hash);

        // Event handlers
        void __fastcall on_buff_gain(game_object* object, buff_instance* buff);
        void __fastcall on_create_particle(game_object* particle, game_object* owner, math::vector3 start_pos, math::vector3 end_pos);

    } // namespace buff_hash_logger
} // namespace venutilities