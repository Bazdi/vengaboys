#include "BuffHashLogger.hpp"
#include <sstream>   // For string streams
#include <algorithm> // For std::replace
#include <iostream>
#include <iomanip>

namespace venutilities {
    namespace buff_hash_logger {

        // Define the maps and sets
        std::map<std::string, std::ofstream> champion_log_files;
        std::map<std::string, std::set<uint32_t>> champion_logged_buffs;
        std::map<std::string, std::ofstream> particle_log_files;
        std::map<std::string, std::set<uint32_t>> logged_particles;

        bool initialize_dependencies() {
            // Load any previously logged buffs to avoid duplication
            return true;
        }

        std::string sanitize_filename(const std::string& filename) {
            std::string sanitized_filename = filename;
            std::replace(sanitized_filename.begin(), sanitized_filename.end(), ' ', '_'); // Replace spaces with underscores
            std::replace(sanitized_filename.begin(), sanitized_filename.end(), ':', '_'); // Replace colons with underscores
            std::replace(sanitized_filename.begin(), sanitized_filename.end(), '/', '_'); // Replace slashes with underscores
            std::replace(sanitized_filename.begin(), sanitized_filename.end(), '\\', '_'); // Replace backslashes with underscores
            return sanitized_filename;
        }

        bool is_buff_already_logged(const std::string& champion_name, uint32_t buff_hash) {
            // Check if this buff hash has already been logged for this champion
            auto it = champion_logged_buffs.find(champion_name);
            if (it != champion_logged_buffs.end()) {
                return it->second.find(buff_hash) != it->second.end();
            }
            return false;
        }

        void log_buff(const std::string& champion_name, const std::string& buff_name, uint32_t buff_hash) {
            if (is_buff_already_logged(champion_name, buff_hash)) {
                return; // Skip if already logged
            }

            // Construct filename based on champion name (e.g., Annie_Buffs.log)
            std::string filename = sanitize_filename(champion_name) + "_Buffs.log";

            // Get the file stream for this champion, or create a new one if it doesn't exist
            std::ofstream& log_file = champion_log_files[champion_name];

            // Check if the file stream is open (it will be created on first access)
            if (!log_file.is_open()) {
                log_file.open(filename, std::ios::out | std::ios::app);
                if (!log_file.is_open()) {
                    //g_sdk->log_console("[ERROR] Buff Hash Logger: Failed to open log file for champion: %s, File: %s", champion_name.c_str(), filename.c_str());
                    return; // Couldn't open file for this champion, skip logging for this buff
                }
                //g_sdk->log_console("[+] Buff Hash Logger: Started logging buffs for champion: %s to file: %s", champion_name.c_str(), filename.c_str());
            }

            // Format the output for the log file (CSV-like)
            std::stringstream log_entry;
            log_entry << buff_name << "," << std::hex << buff_hash << std::endl;

            log_file << log_entry.str();
            log_file.flush();

            // Add to our set of logged buffs
            champion_logged_buffs[champion_name].insert(buff_hash);

            //g_sdk->log_console("[NEW BUFF] Champion: %s, Buff Name: %s, Hash: 0x%X (Logged to file: %s)",
                //champion_name.c_str(), buff_name.c_str(), buff_hash, filename.c_str());
        }

        void log_particle(game_object* owner, const std::string& particle_name, uint32_t particle_hash) {
            std::string owner_name = owner ? owner->get_char_name() : "Unknown";

            // Use a better hash approach - we could hash the particle name if network_id doesn't work
            if (particle_hash == 0) {
                // Simple string hash function if network_id returns 0
                std::hash<std::string> hasher;
                particle_hash = static_cast<uint32_t>(hasher(particle_name));
            }

            // Skip if already logged (using owner_name + particle_name as key)
            if (logged_particles[owner_name].find(particle_hash) != logged_particles[owner_name].end()) {
                return;
            }

            std::string spell_name = "Unknown";
            std::string spell_slot = "Unknown";

            if (owner) {
                // Attempt to determine which spell created this particle
                auto active_spell = owner->get_active_spell_cast();
                if (active_spell && active_spell->get_spell_cast()) {
                    auto spell_cast = active_spell->get_spell_cast();
                    auto spell_data = spell_cast->get_spell_data();
                    if (spell_data && spell_data->get_static_data()) {
                        spell_name = spell_data->get_static_data()->get_name();
                        spell_slot = std::to_string(spell_cast->get_spell_slot());
                    }
                }
            }

            // Construct filename based on champion name (e.g., Annie_Particles.log)
            std::string filename = sanitize_filename(owner_name) + "_Particles.log";

            // Get the file stream for this champion, or create a new one if it doesn't exist
            std::ofstream& log_file = particle_log_files[owner_name];

            // Check if the file stream is open (it will be created on first access)
            if (!log_file.is_open()) {
                log_file.open(filename, std::ios::out | std::ios::app);
                if (!log_file.is_open()) {
                    //g_sdk->log_console("[ERROR] Particle Logger: Failed to open log file for champion: %s, File: %s", owner_name.c_str(), filename.c_str());
                    return;
                }
                // Add a header line if this is a new file
                log_file << "particle_name,hash,spell_name,spell_slot,created_time" << std::endl;
                //g_sdk->log_console("[+] Particle Logger: Started logging particles for champion: %s to file: %s", owner_name.c_str(), filename.c_str());
            }

            // Get current game time for timestamp
            float game_time = g_sdk->clock_facade ? g_sdk->clock_facade->get_game_time() : 0.0f;

            // Format the output for the log file (CSV-like)
            std::stringstream log_entry;
            log_entry << particle_name << ","
                << std::hex << "0x" << particle_hash << ","
                << spell_name << ","
                << spell_slot << ","
                << std::fixed << std::setprecision(2) << game_time
                << std::endl;

            log_file << log_entry.str();
            log_file.flush();

            // Add to our set of logged particles
            logged_particles[owner_name].insert(particle_hash);

            //g_sdk->log_console("[NEW PARTICLE] Champion: %s, Spell: %s (Slot: %s), Particle: %s, Hash: 0x%X, Time: %.2f",
                //owner_name.c_str(), spell_name.c_str(), spell_slot.c_str(), particle_name.c_str(),
                //particle_hash, game_time);
        }

        void __fastcall on_buff_gain(game_object* object, buff_instance* buff)
        {
            if (!object || !buff)
                return;

            std::string champion_name = object->get_char_name();
            if (champion_name.empty()) {
                champion_name = "UnknownObject_" + std::to_string(object->get_id());
            }

            std::string buff_name = buff->get_name();
            uint32_t buff_hash = buff->get_hash();

            // Log the buff if it hasn't been logged yet
            log_buff(champion_name, buff_name, buff_hash);

            // Check if this buff came from another champion (for enemy buff tracking)
            game_object* caster = buff->get_caster();
            if (caster && caster != object && caster->is_hero()) {
                std::string caster_name = caster->get_char_name();
                if (!caster_name.empty()) {
                    std::string category = "Applied_Buffs";
                    log_buff(caster_name + "_" + category, buff_name, buff_hash);
                    
                    // Also log with target information
                    std::stringstream detailed_buff;
                    detailed_buff << buff_name << " (on " << champion_name << ")";
                    log_buff(caster_name + "_" + category + "_Detailed", detailed_buff.str(), buff_hash);
                }
            }
        }

        void __fastcall on_create_particle(game_object* particle, game_object* owner, math::vector3 start_pos, math::vector3 end_pos)
        {
            if (!particle || !owner)
                return;

            std::string particle_name = particle->get_name();
            uint32_t particle_hash = particle->get_network_id(); // Using network ID as a basic hash

            log_particle(owner, particle_name, particle_hash);
        }

        void load() {
            g_sdk->log_console("[+] Loading Enhanced Buff Hash Logger...");

            if (!initialize_dependencies()) {
                g_sdk->log_console("[!] Enhanced Buff Hash Logger: Dependency initialization failed!");
                return;
            }

            if (g_sdk->event_manager) {
                g_sdk->event_manager->register_callback(event_manager::event::buff_gain, (void*)on_buff_gain);
                g_sdk->event_manager->register_callback(event_manager::event::create_particle, (void*)on_create_particle);
                g_sdk->log_console("[+] Enhanced Buff Hash Logger: Event callbacks registered.");
            }

            g_sdk->log_console("[+] Enhanced Buff Hash Logger loaded successfully.");
        }

        void unload() {
            g_sdk->log_console("[-] Unloading Enhanced Buff Hash Logger...");
            if (g_sdk->event_manager) {
                g_sdk->event_manager->unregister_callback(event_manager::event::buff_gain, (void*)on_buff_gain);
                g_sdk->event_manager->unregister_callback(event_manager::event::create_particle, (void*)on_create_particle);
                g_sdk->log_console("[-] Enhanced Buff Hash Logger: Event callbacks unregistered.");
            }

            // Close all open champion log files
            for (auto& [champion_name, file_stream] : champion_log_files) {
                if (file_stream.is_open()) {
                    file_stream.close();
                    g_sdk->log_console("[-] Buff Hash Logger: Closed log file for champion: %s", champion_name.c_str());
                }
            }
            champion_log_files.clear();

            // Close all open particle log files
            for (auto& [champion_name, file_stream] : particle_log_files) {
                if (file_stream.is_open()) {
                    file_stream.close();
                    g_sdk->log_console("[-] Particle Logger: Closed log file for champion: %s", champion_name.c_str());
                }
            }
            particle_log_files.clear();

            // Clear the sets
            champion_logged_buffs.clear();
            logged_particles.clear();

            g_sdk->log_console("[-] Enhanced Buff Hash Logger unloaded");
        }

    } // namespace buff_hash_logger
} // namespace venutilities