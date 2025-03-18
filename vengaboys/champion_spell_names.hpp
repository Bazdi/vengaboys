#pragma once

#include <string>
#include <map>
#include <algorithm>

namespace champion_spell_names {
    // Only declaration, not definition
    extern std::map<std::string, std::map<char, std::string>> spell_names_data;

    // Function declarations
    void initialize_spell_names();
    std::string get_champion_spell_name(const std::string& champion_name, char spell_slot);
    std::string get_champion_q_name(const std::string& champion_name);
    std::string get_champion_w_name(const std::string& champion_name);
    std::string get_champion_e_name(const std::string& champion_name);
    std::string get_champion_r_name(const std::string& champion_name);
    std::string get_champion_p_name(const std::string& champion_name);
}