//#include "teemo.hpp"
//#include "teemo_spots.hpp"
//#include <cstdarg>
//#include <algorithm>
//#include <fstream>
//
//namespace teemo {
//
//    Settings settings;
//    menu_category* menu = nullptr;
//    bool initialized = false;
//    ComboState combat_state;
//    DevModeState dev_state;
//    std::vector<MushroomSpot> mushroom_spots;
//    std::vector<TargetScore> target_scores;
//    uint32_t infotab_text_id = 0;
//
//    TeemoModule& TeemoModule::get_instance() {
//        static TeemoModule instance;
//        return instance;
//    }
//
//    TeemoModule::TeemoModule() {
//        // Default settings initialization
//        settings.use_q_combo = true;
//        settings.use_w_combo = true;
//        settings.use_r_combo = true;
//
//        settings.use_q_harass = true;
//        settings.use_aa_harass = true;
//        settings.harass_mana_threshold = 30.f;
//
//        settings.use_q_clear = false;
//        settings.use_r_clear = false;
//        settings.r_clear_min_minions = 3;
//        settings.clear_mana_threshold = 40.f;
//
//        settings.use_q_jungle = true;
//        settings.use_r_jungle = true;
//        settings.use_r_objectives = true;
//
//        settings.use_w_flee = true;
//        settings.drop_r_when_fleeing = true;
//
//        settings.q_priority = QTargetPriority::AUTO;
//        settings.q_auto_harass = false;
//        settings.q_ks = true;
//        settings.save_q_for_blind = false;
//        settings.blind_adc_priority = true;
//
//        settings.w_auto_use = true;
//        settings.w_hp_threshold = 60.f;
//        settings.w_chase = true;
//        settings.w_dodge_skillshot = true;
//
//        settings.r_auto_place = true;
//        settings.r_use_saved_spots = true;
//        settings.r_min_distance = 300.f;
//        settings.r_in_bush_priority = true;
//        settings.r_max_stacks = 0;
//        settings.r_maintain_reserve = 1;
//        settings.r_block_paths = true;
//        settings.r_on_immobile = true;
//
//        settings.draw_q_range = true;
//        settings.draw_r_range = true;
//        settings.draw_r_spots = true;
//        settings.draw_damage_prediction = true;
//        settings.draw_q_color = 0x8800FF00;
//        settings.draw_r_color = 0x88FF0000;
//        settings.draw_spot_color = 0xFFFFFF00;
//        settings.draw_enabled_spot_color = 0xFF00FF00;
//        settings.draw_disabled_spot_color = 0xFFFF0000;
//
//        settings.dev_mode = false;
//        settings.save_spot_key = nullptr;
//        settings.delete_spot_key = nullptr;
//        settings.toggle_spot_key = nullptr;
//        settings.spot_name_prefix = "Spot";
//        settings.mushroom_spots_file = "teemo_mushrooms.txt";
//
//        settings.debug_mode = false;
//    }
//
//    void TeemoModule::initialize() {
//        if (initialized) return;
//
//        // Load default mushroom spots
//        mushroom_spots = default_mushroom_spots;
//
//        // Try to load custom spots if file exists
//        this->load_mushroom_spots();
//
//        // Create and configure the menu
//        this->create_menu();
//
//        // Register callbacks
//        g_sdk->event_manager->register_callback(event_manager::event::game_update,
//            reinterpret_cast<void*>(&teemo::on_update));
//        g_sdk->event_manager->register_callback(event_manager::event::draw_world,
//            reinterpret_cast<void*>(&teemo::on_draw));
//        g_sdk->event_manager->register_callback(event_manager::event::process_cast,
//            reinterpret_cast<void*>(&teemo::on_process_spell_cast));
//
//        if (sdk::orbwalker) {
//            sdk::orbwalker->register_callback(orb_sdk::before_attack,
//                reinterpret_cast<void*>(&teemo::on_before_attack));
//        }
//
//        // Configure order manager for Teemo's needs
//        auto& order_manager = vengaboys::OrderManager::get_instance();
//        order_manager.set_order_delay(0.05f);
//        order_manager.set_debug_mode(settings.debug_mode);
//
//        initialized = true;
//        this->log_debug("Teemo module initialized");
//
//        // Notify user
//        if (sdk::notification) {
//            sdk::notification->add("Teemo", "Teemo script loaded successfully", 0xFF00FF00);
//
//            if (settings.dev_mode) {
//                sdk::notification->add("Teemo", "Developer mode active", 0xFFFFFF00);
//            }
//        }
//    }
//
//    void TeemoModule::load() {
//        g_sdk->log_console("[+] Loading Teemo module...");
//
//        // Set package name for the SDK
//        g_sdk->set_package("TeemoAIO");
//
//        // Initialize required SDKs
//        if (!sdk_init::orbwalker() || !sdk_init::target_selector() ||
//            !sdk_init::damage() || !sdk_init::notification() || !sdk_init::prediction()) {
//            g_sdk->log_console("[!] Failed to initialize required SDKs");
//            return;
//        }
//
//        // Full initialization
//        this->initialize();
//
//        g_sdk->log_console("[+] Teemo module loaded successfully");
//    }
//
//    void TeemoModule::unload() {
//        if (!initialized) return;
//
//        // Save mushroom spots if in dev mode
//        if (settings.dev_mode) {
//            this->save_mushroom_spots();
//        }
//
//        // Unregister callbacks
//        g_sdk->event_manager->unregister_callback(event_manager::event::game_update,
//            reinterpret_cast<void*>(&teemo::on_update));
//        g_sdk->event_manager->unregister_callback(event_manager::event::draw_world,
//            reinterpret_cast<void*>(&teemo::on_draw));
//        g_sdk->event_manager->unregister_callback(event_manager::event::process_cast,
//            reinterpret_cast<void*>(&teemo::on_process_spell_cast));
//
//        if (sdk::orbwalker) {
//            sdk::orbwalker->unregister_callback(orb_sdk::before_attack,
//                reinterpret_cast<void*>(&teemo::on_before_attack));
//        }
//
//        // Remove InfoTab if active
//        if (sdk::infotab && infotab_text_id != 0) {
//            sdk::infotab->remove_text(infotab_text_id);
//            infotab_text_id = 0;
//        }
//
//        // Clean up allocated memory
//        if (settings.save_spot_key) {
//            delete settings.save_spot_key;
//            settings.save_spot_key = nullptr;
//        }
//
//        if (settings.delete_spot_key) {
//            delete settings.delete_spot_key;
//            settings.delete_spot_key = nullptr;
//        }
//
//        if (settings.toggle_spot_key) {
//            delete settings.toggle_spot_key;
//            settings.toggle_spot_key = nullptr;
//        }
//
//        initialized = false;
//        g_sdk->log_console("[+] Teemo module unloaded");
//    }
//
//    void TeemoModule::log_debug(const char* format, ...) {
//        if (!settings.debug_mode) return;
//
//        va_list args;
//        va_start(args, format);
//        char buffer[512];
//        vsnprintf(buffer, sizeof(buffer), format, args);
//        va_end(args);
//        g_sdk->log_console("[Teemo] %s", buffer);
//    }
//
//    bool TeemoModule::is_target_valid(game_object* target) {
//        if (!target || !target->is_valid() || target->is_dead() || !target->is_targetable()) {
//            return false;
//        }
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return false;
//
//        if (target->get_team_id() == player->get_team_id()) {
//            return false; // Don't target allies
//        }
//
//        return true;
//    }
//
//    bool TeemoModule::can_cast_q() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return false;
//
//        auto spell = player->get_spell(0);
//        if (!spell || spell->get_level() <= 0) return false;
//
//        if (spell->get_cooldown() > 0.01f) return false;
//
//        float mana_percent = (player->get_par() / player->get_max_par()) * 100.f;
//        if (sdk::orbwalker->harass() && mana_percent < settings.harass_mana_threshold) {
//            return false;
//        }
//
//        if (sdk::orbwalker->clear() && mana_percent < settings.clear_mana_threshold) {
//            return false;
//        }
//
//        return vengaboys::OrderManager::get_instance().can_cast(player, 0);
//    }
//
//    bool TeemoModule::can_cast_w() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return false;
//
//        auto spell = player->get_spell(1);
//        if (!spell || spell->get_level() <= 0) return false;
//
//        if (spell->get_cooldown() > 0.01f) return false;
//
//        float mana_percent = (player->get_par() / player->get_max_par()) * 100.f;
//        if (sdk::orbwalker->harass() && mana_percent < settings.harass_mana_threshold) {
//            return false;
//        }
//
//        if (sdk::orbwalker->clear() && mana_percent < settings.clear_mana_threshold) {
//            return false;
//        }
//
//        return vengaboys::OrderManager::get_instance().can_cast(player, 1);
//    }
//
//    bool TeemoModule::can_cast_r() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return false;
//
//        auto spell = player->get_spell(3);
//        if (!spell || spell->get_level() <= 0) return false;
//
//        if (spell->get_cooldown() > 0.01f) return false;
//
//        // Check if we want to maintain a reserve of mushrooms
//        if (settings.r_maintain_reserve > 0) {
//            uint32_t current_charges = spell->get_charges();
//            if (current_charges <= static_cast<uint32_t>(settings.r_maintain_reserve)) {
//                return false;
//            }
//        }
//
//        float mana_percent = (player->get_par() / player->get_max_par()) * 100.f;
//        if (sdk::orbwalker->harass() && mana_percent < settings.harass_mana_threshold) {
//            return false;
//        }
//
//        if (sdk::orbwalker->clear() && mana_percent < settings.clear_mana_threshold) {
//            return false;
//        }
//
//        return vengaboys::OrderManager::get_instance().can_cast(player, 3);
//    }
//
//    float TeemoModule::get_q_damage(game_object* target) {
//        if (!target || !target->is_valid()) return 0.f;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return 0.f;
//
//        // Use the damage SDK if available
//        if (sdk::damage) {
//            return sdk::damage->get_spell_damage(player, target, 0);
//        }
//
//        // Fallback manual calculation if no damage SDK
//        auto spell = player->get_spell(0);
//        if (!spell || spell->get_level() == 0) return 0.f;
//
//        // Base damage values for Q
//        const float base_damage[] = { 80.f, 125.f, 170.f, 215.f, 260.f };
//        // AP ratio for Q
//        const float ap_ratio = 0.8f;
//
//        int spell_level = spell->get_level() - 1;
//        if (spell_level < 0 || spell_level > 4) spell_level = 0;
//
//        float ap = player->get_ability_power();
//        float total_damage = base_damage[spell_level] + (ap * ap_ratio);
//
//        // Apply magic resist calculation (simplified)
//        float magic_resist = target->get_magic_resist();
//        if (magic_resist >= 0) {
//            total_damage *= (100.0f / (100.0f + magic_resist));
//        }
//        else {
//            total_damage *= (2.0f - (100.0f / (100.0f - magic_resist)));
//        }
//
//        return total_damage;
//    }
//
//    float TeemoModule::get_e_damage(game_object* target) {
//        if (!target || !target->is_valid()) return 0.f;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return 0.f;
//
//        auto spell = player->get_spell(2);
//        if (!spell || spell->get_level() == 0) return 0.f;
//
//        // Base damage per tick
//        const float base_damage[] = { 14.f, 25.f, 36.f, 47.f, 58.f };
//        // AP ratio per tick
//        const float ap_ratio = 0.3f;
//
//        int spell_level = spell->get_level() - 1;
//        if (spell_level < 0 || spell_level > 4) spell_level = 0;
//
//        float ap = player->get_ability_power();
//
//        // Damage on hit
//        float on_hit_damage = base_damage[spell_level] + (ap * ap_ratio);
//
//        // Calculate DoT damage (4 seconds of poison)
//        float dot_damage = on_hit_damage * 4.0f;
//
//        // Total damage
//        float total_damage = on_hit_damage + dot_damage;
//
//        // Apply magic resist calculation (simplified)
//        float magic_resist = target->get_magic_resist();
//        if (magic_resist >= 0) {
//            total_damage *= (100.0f / (100.0f + magic_resist));
//        }
//        else {
//            total_damage *= (2.0f - (100.0f / (100.0f - magic_resist)));
//        }
//
//        return total_damage;
//    }
//
//    float TeemoModule::get_r_damage(game_object* target) {
//        if (!target || !target->is_valid()) return 0.f;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return 0.f;
//
//        // Use the damage SDK if available
//        if (sdk::damage) {
//            return sdk::damage->get_spell_damage(player, target, 3);
//        }
//
//        // Fallback manual calculation if no damage SDK
//        auto spell = player->get_spell(3);
//        if (!spell || spell->get_level() == 0) return 0.f;
//
//        // Base damage values for R
//        const float base_damage[] = { 200.f, 325.f, 450.f };
//        // AP ratio for R
//        const float ap_ratio = 0.5f;
//
//        int spell_level = spell->get_level() - 1;
//        if (spell_level < 0 || spell_level > 2) spell_level = 0;
//
//        float ap = player->get_ability_power();
//        float total_damage = base_damage[spell_level] + (ap * ap_ratio);
//
//        // Apply magic resist calculation (simplified)
//        float magic_resist = target->get_magic_resist();
//        if (magic_resist >= 0) {
//            total_damage *= (100.0f / (100.0f + magic_resist));
//        }
//        else {
//            total_damage *= (2.0f - (100.0f / (100.0f - magic_resist)));
//        }
//
//        return total_damage;
//    }
//
//    float TeemoModule::calculate_total_damage(game_object* target) {
//        if (!target || !target->is_valid()) return 0.f;
//
//        float total = 0.f;
//
//        // Add Q damage if ready
//        if (this->can_cast_q()) {
//            total += this->get_q_damage(target);
//        }
//
//        // Add E damage
//        total += this->get_e_damage(target);
//
//        // Add R damage if ready
//        if (this->can_cast_r()) {
//            total += this->get_r_damage(target);
//        }
//
//        // Add auto attack damage
//        auto player = g_sdk->object_manager->get_local_player();
//        if (player && sdk::damage) {
//            total += sdk::damage->get_aa_damage(player, target);
//        }
//
//        return total;
//    }
//
//    bool TeemoModule::is_in_danger() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return false;
//
//        // Check player health
//        float hp_percent = (player->get_hp() / player->get_max_hp()) * 100.f;
//        if (hp_percent <= settings.w_hp_threshold) {
//            return true;
//        }
//
//        // Check for nearby enemies and their positioning
//        int enemy_count = 0;
//        int ally_count = 0;
//
//        for (auto hero : g_sdk->object_manager->get_heroes()) {
//            if (!hero || !hero->is_valid() || hero->is_dead()) continue;
//
//            float distance = player->get_position().distance(hero->get_position());
//
//            if (distance <= 900.f) { // Consider enemies within this range threatening
//                if (hero->get_team_id() != player->get_team_id()) {
//                    enemy_count++;
//
//                    // Extra danger if enemy is closer than 400 units
//                    if (distance <= 400.f) {
//                        return true;
//                    }
//                }
//                else if (hero != player) {
//                    ally_count++;
//                }
//            }
//        }
//
//        // If more enemies than allies (including self), consider it dangerous
//        return enemy_count > (ally_count + 1);
//    }
//
//    void TeemoModule::update_combat_state() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        // Only update state every 0.25 seconds to reduce overhead
//        if (current_time - combat_state.last_state_update < 0.25f) {
//            return;
//        }
//
//        // Update in-bush status
//        combat_state.in_bush = player->is_in_bush();
//
//        // Update nearby enemies and allies
//        combat_state.nearby_enemies.clear();
//        combat_state.nearby_allies.clear();
//
//        for (auto hero : g_sdk->object_manager->get_heroes()) {
//            if (!hero || !hero->is_valid() || hero->is_dead()) continue;
//
//            float distance = player->get_position().distance(hero->get_position());
//            if (distance <= 1000.f) {
//                if (hero->get_team_id() != player->get_team_id()) {
//                    combat_state.nearby_enemies.push_back(hero);
//                }
//                else if (hero != player) {
//                    combat_state.nearby_allies.push_back(hero);
//                }
//            }
//        }
//
//        // Calculate danger level
//        combat_state.danger_level = 0.f;
//        float hp_percent = (player->get_hp() / player->get_max_hp()) * 100.f;
//
//        // Low HP = higher danger
//        if (hp_percent < 30.f) {
//            combat_state.danger_level += 0.5f;
//        }
//        else if (hp_percent < 50.f) {
//            combat_state.danger_level += 0.3f;
//        }
//        else if (hp_percent < 70.f) {
//            combat_state.danger_level += 0.1f;
//        }
//
//        // More enemies = higher danger
//        combat_state.danger_level += 0.2f * static_cast<float>(combat_state.nearby_enemies.size());
//
//        // Fewer allies = higher danger
//        if (combat_state.nearby_allies.size() < combat_state.nearby_enemies.size()) {
//            combat_state.danger_level += 0.2f * static_cast<float>(combat_state.nearby_enemies.size() - combat_state.nearby_allies.size());
//        }
//
//        // Under enemy turret = high danger
//        for (auto turret : g_sdk->object_manager->get_turrets()) {
//            if (!turret || !turret->is_valid() || turret->is_dead() ||
//                turret->get_team_id() == player->get_team_id()) continue;
//
//            float distance = player->get_position().distance(turret->get_position());
//            if (distance < 900.f) {
//                combat_state.danger_level += 0.6f;
//                break;
//            }
//        }
//
//        // Update fleeing status
//        combat_state.fleeing = combat_state.danger_level >= 0.7f;
//
//        // Set last update time
//        combat_state.last_state_update = current_time;
//    }
//
//    void TeemoModule::save_current_spot() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return;
//
//        MushroomSpot spot;
//        spot.position = player->get_position();
//        spot.name = settings.spot_name_prefix + std::to_string(mushroom_spots.size() + 1);
//        spot.enabled = true;
//
//        mushroom_spots.push_back(spot);
//
//        // Notify user
//        if (sdk::notification) {
//            sdk::notification->add("Teemo", "Mushroom spot saved: " + spot.name, 0xFF00FF00);
//        }
//
//        this->log_debug("Saved mushroom spot at %.1f, %.1f, %.1f as %s",
//            spot.position.x, spot.position.y, spot.position.z, spot.name.c_str());
//
//        // Also log the coordinates in a format ready for pasting into the spots list
//        g_sdk->log_console("[Teemo] New mushroom spot: {\"%s\", math::vector3(%.2ff, %.2ff, %.2ff), true},",
//            spot.name.c_str(), spot.position.x, spot.position.y, spot.position.z);
//
//        dev_state.last_spot_save_time = g_sdk->clock_facade->get_game_time();
//
//        // Save to spots file immediately
//        this->save_mushroom_spots();
//    }
//
//    void TeemoModule::delete_spot(int index) {
//        if (index < 0 || index >= static_cast<int>(mushroom_spots.size())) {
//            return;
//        }
//
//        std::string name = mushroom_spots[index].name;
//        mushroom_spots.erase(mushroom_spots.begin() + index);
//
//        // Notify user
//        if (sdk::notification) {
//            sdk::notification->add("Teemo", "Mushroom spot deleted: " + name, 0xFFFF0000);
//        }
//
//        this->log_debug("Deleted mushroom spot %s", name.c_str());
//
//        // Save changes
//        this->save_mushroom_spots();
//    }
//
//    void TeemoModule::toggle_spot(int index) {
//        if (index < 0 || index >= static_cast<int>(mushroom_spots.size())) {
//            return;
//        }
//
//        mushroom_spots[index].enabled = !mushroom_spots[index].enabled;
//
//        // Notify user
//        if (sdk::notification) {
//            std::string status = mushroom_spots[index].enabled ? "enabled" : "disabled";
//            sdk::notification->add("Teemo", "Mushroom spot " + status + ": " + mushroom_spots[index].name,
//                mushroom_spots[index].enabled ? 0xFF00FF00 : 0xFFFF0000);
//        }
//
//        this->log_debug("Toggled mushroom spot %s to %s",
//            mushroom_spots[index].name.c_str(),
//            mushroom_spots[index].enabled ? "enabled" : "disabled");
//
//        // Save changes
//        this->save_mushroom_spots();
//    }
//
//    void TeemoModule::load_mushroom_spots() {
//        std::ifstream file(settings.mushroom_spots_file);
//        if (!file.is_open()) {
//            this->log_debug("Could not open mushroom spots file. Using defaults.");
//            return;
//        }
//
//        // Clear existing spots and load from file
//        std::vector<MushroomSpot> loaded_spots;
//
//        std::string line;
//        while (std::getline(file, line)) {
//            // Skip comments and empty lines
//            if (line.empty() || line[0] == '#' || line[0] == '/') {
//                continue;
//            }
//
//            // Expected format: Name,X,Y,Z,Enabled
//            size_t pos = 0;
//            std::string name;
//            float x = 0.f, y = 0.f, z = 0.f;
//            bool enabled = true;
//
//            // Parse name (up to comma)
//            pos = line.find(',');
//            if (pos == std::string::npos) continue;
//
//            name = line.substr(0, pos);
//            line.erase(0, pos + 1);
//
//            // Parse X coordinate
//            pos = line.find(',');
//            if (pos == std::string::npos) continue;
//
//            x = std::stof(line.substr(0, pos));
//            line.erase(0, pos + 1);
//
//            // Parse Y coordinate
//            pos = line.find(',');
//            if (pos == std::string::npos) continue;
//
//            y = std::stof(line.substr(0, pos));
//            line.erase(0, pos + 1);
//
//            // Parse Z coordinate
//            pos = line.find(',');
//            if (pos == std::string::npos) {
//                z = std::stof(line);
//            }
//            else {
//                z = std::stof(line.substr(0, pos));
//                line.erase(0, pos + 1);
//
//                // Parse enabled state if present
//                enabled = (line == "1" || line == "true" || line == "yes" || line == "enabled");
//            }
//
//            // Create and add spot
//            MushroomSpot spot;
//            spot.name = name;
//            spot.position = math::vector3(x, y, z);
//            spot.enabled = enabled;
//
//            loaded_spots.push_back(spot);
//        }
//
//        file.close();
//
//        // Only replace the spots if we loaded some
//        if (!loaded_spots.empty()) {
//            mushroom_spots = loaded_spots;
//            this->log_debug("Loaded %d mushroom spots from %s",
//                static_cast<int>(mushroom_spots.size()),
//                settings.mushroom_spots_file.c_str());
//        }
//    }
//
//    void TeemoModule::save_mushroom_spots() {
//        std::ofstream file(settings.mushroom_spots_file);
//        if (!file.is_open()) {
//            this->log_debug("Could not open file for saving mushroom spots");
//            return;
//        }
//
//        file << "# Teemo mushroom spots - Format: Name,X,Y,Z,Enabled\n";
//        file << "# Generated on game time: " << g_sdk->clock_facade->get_game_time() << "\n";
//
//        for (const auto& spot : mushroom_spots) {
//            file << spot.name << ","
//                << spot.position.x << ","
//                << spot.position.y << ","
//                << spot.position.z << ","
//                << (spot.enabled ? "1" : "0") << "\n";
//
//            // Also log to console in code-compatible format for easy copying
//            g_sdk->log_console("{\"%s\", math::vector3(%.2ff, %.2ff, %.2ff), %s},",
//                spot.name.c_str(),
//                spot.position.x, spot.position.y, spot.position.z,
//                spot.enabled ? "true" : "false");
//        }
//
//        file.close();
//        this->log_debug("Saved %d mushroom spots to %s",
//            static_cast<int>(mushroom_spots.size()),
//            settings.mushroom_spots_file.c_str());
//    }
//
//    MushroomSpot* TeemoModule::get_best_mushroom_spot() {
//        if (mushroom_spots.empty()) {
//            return nullptr;
//        }
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return nullptr;
//
//        float r_range = player->get_spell(3) ? player->get_spell(3)->get_cast_range() : 600.f;
//        float best_score = -1.f;
//        MushroomSpot* best_spot = nullptr;
//
//        for (auto& spot : mushroom_spots) {
//            if (!spot.enabled) continue;
//
//            float distance = player->get_position().distance(spot.position);
//            if (distance > r_range) continue;
//
//            // Calculate spot score based on various factors
//            float score = 1000.f - distance; // Closer spots preferred
//
//            // Check if spot is in bush - using an alternative approach since is_in_bush isn't available
//            bool likely_in_bush = false;
//
//            // Bush detection approximation based on known map areas
//            // This is a simplified check - in a real implementation we would need map coordinates
//            if (settings.r_in_bush_priority) {
//                // Common bush coordinates check (very simplified)
//                // In a full implementation, we would check against actual bush coordinates
//                if ((spot.position.x > 2000.f && spot.position.x < 12000.f &&
//                    spot.position.z > 2000.f && spot.position.z < 12000.f) &&
//                    // Using the spot name as a hint - if it contains "Bush"
//                    spot.name.find("Bush") != std::string::npos) {
//                    score += 500.f;
//                    likely_in_bush = true;
//                }
//            }
//
//            // Check for nearby enemies
//            int nearby_enemies = 0;
//            for (auto hero : g_sdk->object_manager->get_heroes()) {
//                if (!hero || !hero->is_valid() || hero->is_dead() ||
//                    hero->get_team_id() == player->get_team_id()) continue;
//
//                float enemy_dist = hero->get_position().distance(spot.position);
//                if (enemy_dist <= 1000.f) {
//                    nearby_enemies++;
//
//                    // If enemy is very close, reduce score (don't place mushroom right next to them)
//                    if (enemy_dist < 400.f) {
//                        score -= 300.f;
//                    }
//                    else {
//                        // Otherwise, prioritize based on distance
//                        score += (1000.f - enemy_dist) * 0.3f;
//                    }
//                }
//            }
//
//            // If no enemies nearby and not in bush, reduce priority
//            if (nearby_enemies == 0 && !likely_in_bush) {
//                score -= 200.f;
//            }
//
//            // Find if we already have a mushroom near this spot
//            bool mushroom_exists = false;
//            for (auto obj : g_sdk->object_manager->get_traps()) {
//                if (!obj || !obj->is_valid() || obj->is_dead() ||
//                    obj->get_team_id() != player->get_team_id()) continue;
//
//                // Check if object name contains "Mushroom" or "TeemoMushroom"
//                std::string obj_name = obj->get_name();
//                if (obj_name.find("Mushroom") != std::string::npos ||
//                    obj_name.find("TeemoMushroom") != std::string::npos) {
//                    if (obj->get_position().distance(spot.position) < 300.f) {
//                        mushroom_exists = true;
//                        break;
//                    }
//                }
//            }
//
//            // If mushroom already exists at this spot, greatly reduce score
//            if (mushroom_exists) {
//                score -= 1000.f;
//            }
//
//            if (score > best_score) {
//                best_score = score;
//                best_spot = &spot;
//            }
//        }
//
//        return best_spot;
//    }
//
//    game_object* TeemoModule::get_q_target() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player) return nullptr;
//
//        float q_range = player->get_spell(0) ? player->get_spell(0)->get_cast_range() : 680.f;
//
//        // Use target selector if using AUTO priority
//        if (settings.q_priority == QTargetPriority::AUTO) {
//            return sdk::target_selector->get_hero_target([&](game_object* obj) {
//                if (!this->is_target_valid(obj)) return false;
//                float distance = player->get_position().distance(obj->get_position());
//                return distance <= q_range;
//                });
//        }
//
//        // Custom prioritization logic
//        std::vector<TargetScore> targets;
//
//        for (auto hero : g_sdk->object_manager->get_heroes()) {
//            if (!this->is_target_valid(hero)) continue;
//
//            float distance = player->get_position().distance(hero->get_position());
//            if (distance > q_range) continue;
//
//            TargetScore score;
//            score.target = hero;
//
//            // Health score - lower health = higher score
//            score.hp_score = 1.0f - (hero->get_hp() / hero->get_max_hp());
//
//            // Damage score - higher attack damage = higher score
//            score.damage_score = hero->get_attack_damage() / 200.f; // Normalize around 200 AD
//
//            // Distance score - closer = higher score
//            score.distance_score = 1.0f - (distance / q_range);
//
//            // Threat score calculation
//            score.threat_score = 0.0f;
//
//            // Check if target is an ADC
//            std::string char_name = hero->get_char_name();
//            bool is_adc = (
//                char_name == "Ashe" || char_name == "Caitlyn" || char_name == "Draven" ||
//                char_name == "Ezreal" || char_name == "Jhin" || char_name == "Jinx" ||
//                char_name == "Kaisa" || char_name == "Kalista" || char_name == "Kogmaw" ||
//                char_name == "Lucian" || char_name == "MissFortune" || char_name == "Samira" ||
//                char_name == "Senna" || char_name == "Sivir" || char_name == "Tristana" ||
//                char_name == "Twitch" || char_name == "Varus" || char_name == "Vayne" ||
//                char_name == "Xayah" || char_name == "Zeri" || char_name == "Aphelios"
//                );
//
//            if (is_adc && settings.blind_adc_priority) {
//                score.threat_score += 0.5f;
//            }
//
//            // Determine final score based on priority setting
//            switch (settings.q_priority) {
//            case QTargetPriority::LOWEST_HEALTH:
//                score.total_score = score.hp_score * 3.0f + score.distance_score;
//                break;
//
//            case QTargetPriority::HIGHEST_DAMAGE:
//                score.total_score = score.damage_score * 3.0f + score.distance_score;
//                break;
//
//            case QTargetPriority::MOST_DANGEROUS:
//                score.total_score = score.threat_score * 3.0f + score.hp_score + score.distance_score;
//                break;
//
//            case QTargetPriority::CLOSEST:
//                score.total_score = score.distance_score * 3.0f;
//                break;
//
//            case QTargetPriority::CUSTOM:
//                score.total_score = score.hp_score * 0.5f +
//                    score.damage_score * 0.3f +
//                    score.distance_score * 0.3f +
//                    score.threat_score * 0.4f;
//                break;
//
//            default:
//                score.total_score = score.hp_score + score.distance_score;
//                break;
//            }
//
//            // If we can kill with Q, prioritize
//            if (this->get_q_damage(hero) >= hero->get_hp()) {
//                score.total_score += 10.0f;
//            }
//
//            targets.push_back(score);
//        }
//
//        // Find best target
//        if (targets.empty()) {
//            return nullptr;
//        }
//
//        // Sort by total score (descending)
//        std::sort(targets.begin(), targets.end(), [](const TargetScore& a, const TargetScore& b) {
//            return a.total_score > b.total_score;
//            });
//
//        return targets[0].target;
//    }
//
//    bool TeemoModule::should_use_q_on(game_object* target) {
//        if (!this->is_target_valid(target)) return false;
//
//        // Always use Q if it will kill the target
//        if (settings.q_ks && this->get_q_damage(target) >= target->get_hp()) {
//            return true;
//        }
//
//        // If saving Q for blind, only use on auto-attackers
//        if (settings.save_q_for_blind) {
//            std::string char_name = target->get_char_name();
//            bool is_aa_reliant = (
//                char_name == "Yasuo" || char_name == "MasterYi" ||
//                char_name == "Tryndamere" || char_name == "Fiora" ||
//                char_name == "Jax" || char_name == "Irelia" ||
//                char_name == "Vayne" || char_name == "Draven" ||
//                char_name == "Twitch" || char_name == "Kaisa" ||
//                char_name == "Kalista" || char_name == "Jinx" ||
//                char_name == "Lucian" || char_name == "Viego" ||
//                char_name == "Yone"
//                );
//
//            return is_aa_reliant;
//        }
//
//        return true;
//    }
//
//    void TeemoModule::update() {
//        if (!initialized) return;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) return;
//
//        // Update combat state 
//        float current_time = g_sdk->clock_facade->get_game_time();
//        this->update_combat_state();
//
//        // Check for hotkey presses for dev mode
//        if (settings.dev_mode && settings.save_spot_key &&
//            current_time - dev_state.last_spot_save_time > 0.5f) {
//            // This is handled via the hotkey callback in the menu
//        }
//
//        // Handle orbwalker modes
//        if (sdk::orbwalker->combo()) {
//            this->handle_combo();
//        }
//        else if (sdk::orbwalker->harass()) {
//            this->handle_harass();
//        }
//        else if (sdk::orbwalker->clear()) {
//            // Check if we're in jungle or lane
//            bool hitting_monsters = false;
//            for (auto monster : g_sdk->object_manager->get_monsters()) {
//                if (!monster || !monster->is_valid() || monster->is_dead()) continue;
//
//                float distance = player->get_position().distance(monster->get_position());
//                if (distance <= player->get_attack_range() + 200.f) {
//                    hitting_monsters = true;
//                    break;
//                }
//            }
//
//            if (hitting_monsters) {
//                this->handle_jungle_clear();
//            }
//            else {
//                this->handle_lane_clear();
//            }
//        }
//        else if (sdk::orbwalker->flee()) {
//            this->handle_flee();
//        }
//
//        // Auto-use W when in danger if enabled
//        if (settings.w_auto_use && this->can_cast_w() && this->is_in_danger()) {
//            vengaboys::OrderManager::get_instance().cast_spell(1, player->get_position());
//            this->log_debug("Auto-used W due to danger");
//        }
//
//        // Auto-place mushrooms in saved spots if enabled
//        if (settings.r_use_saved_spots && settings.r_auto_place && this->can_cast_r()) {
//            MushroomSpot* best_spot = this->get_best_mushroom_spot();
//            if (best_spot) {
//                float distance = player->get_position().distance(best_spot->position);
//                float r_range = player->get_spell(3) ? player->get_spell(3)->get_cast_range() : 600.f;
//
//                if (distance <= r_range) {
//                    vengaboys::OrderManager::get_instance().cast_spell(3, best_spot->position);
//                    this->log_debug("Automatically placed mushroom at saved spot: %s", best_spot->name.c_str());
//                }
//            }
//        }
//    }
//
//    void TeemoModule::draw() {
//        if (!initialized) return;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) return;
//
//        // Draw ability ranges
//        if (settings.draw_q_range) {
//            float q_range = player->get_spell(0) ? player->get_spell(0)->get_cast_range() : 680.f;
//            g_sdk->renderer->add_circle_3d(player->get_position(), q_range, 2.f, settings.draw_q_color);
//        }
//
//        if (settings.draw_r_range) {
//            float r_range = player->get_spell(3) ? player->get_spell(3)->get_cast_range() : 600.f;
//            g_sdk->renderer->add_circle_3d(player->get_position(), r_range, 2.f, settings.draw_r_color);
//        }
//
//        // Draw mushroom spots
//        if (settings.draw_r_spots && (settings.dev_mode || settings.r_use_saved_spots)) {
//            for (size_t i = 0; i < mushroom_spots.size(); i++) {
//                const auto& spot = mushroom_spots[i];
//
//                // Don't draw spots too far away
//                float distance = player->get_position().distance(spot.position);
//                if (distance > 3000.f) continue;
//
//                // Draw spot indicator
//                uint32_t color = spot.enabled ? settings.draw_enabled_spot_color : settings.draw_disabled_spot_color;
//                g_sdk->renderer->add_circle_3d(spot.position, 50.f, 2.f, color);
//
//                // Draw spot name
//                math::vector2 screen_pos = g_sdk->renderer->world_to_screen(spot.position);
//                g_sdk->renderer->add_text(spot.name, 14.f, math::vector2(screen_pos.x, screen_pos.y - 15.f), 1, color);
//
//                // Draw spot number
//                std::string idx_text = std::to_string(i + 1);
//                g_sdk->renderer->add_text(idx_text, 16.f, screen_pos, 1, 0xFFFFFFFF);
//            }
//        }
//
//        // Draw damage prediction
//        if (settings.draw_damage_prediction) {
//            for (auto enemy : combat_state.nearby_enemies) {
//                if (!enemy || !enemy->is_valid() || enemy->is_dead()) continue;
//
//                float damage = this->calculate_total_damage(enemy);
//                g_sdk->renderer->add_damage_indicator(enemy, damage);
//            }
//        }
//
//        // Draw dev mode info
//        if (settings.dev_mode) {
//            math::vector2 screen_pos = { 100.f, 100.f };
//            g_sdk->renderer->add_text("Teemo Dev Mode", 16.f, screen_pos, 0, 0xFFFFFF00);
//
//            screen_pos.y += 20.f;
//            g_sdk->renderer->add_text("Press " + *settings.save_spot_key + " to save spot", 14.f, screen_pos, 0, 0xFFFFFFFF);
//
//            screen_pos.y += 20.f;
//            g_sdk->renderer->add_text("Total mushroom spots: " + std::to_string(mushroom_spots.size()), 14.f, screen_pos, 0, 0xFFFFFFFF);
//
//            // Current position
//            auto player_pos = player->get_position();
//            screen_pos.y += 20.f;
//            std::string pos_text = "Current position: " +
//                std::to_string(player_pos.x).substr(0, 6) + ", " +
//                std::to_string(player_pos.y).substr(0, 6) + ", " +
//                std::to_string(player_pos.z).substr(0, 6);
//            g_sdk->renderer->add_text(pos_text, 14.f, screen_pos, 0, 0xFFFFFFFF);
//
//            screen_pos.y += 20.f;
//            std::string combat_text;
//            uint32_t text_color;
//
//            if (combat_state.fleeing == true) {
//                combat_text = "Combat state: FLEEING";
//                text_color = 0xFFFF6666;
//            }
//            else {
//                combat_text = "Combat state: COMBAT";
//                text_color = 0xFF66FF66;
//            }
//
//            combat_text += " (Danger: " + std::to_string(combat_state.danger_level).substr(0, 4) + ")";
//            g_sdk->renderer->add_text(combat_text, 14.f, screen_pos, 0, text_color);
//
//            // Bush status - also fix this ternary
//            screen_pos.y += 20.f;
//            std::string bush_text = "In bush: " + std::string(combat_state.in_bush ? "YES" : "NO");
//            uint32_t bush_color = combat_state.in_bush ? 0xFF66FF66 : 0xFFFFFFFF;
//            g_sdk->renderer->add_text(bush_text, 14.f, screen_pos, 0, bush_color);
//
//            // Nearby counts
//            screen_pos.y += 20.f;
//            std::string nearby_text = "Nearby: " +
//                std::to_string(combat_state.nearby_enemies.size()) + " enemies, " +
//                std::to_string(combat_state.nearby_allies.size()) + " allies";
//            g_sdk->renderer->add_text(nearby_text, 14.f, screen_pos, 0, 0xFFFFFFFF);
//        }
//    }
//
//    void TeemoModule::handle_combo() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) return;
//
//        // Get target
//        game_object* target = this->get_q_target();
//        if (!target) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        // Q usage
//        if (settings.use_q_combo && this->can_cast_q() && this->should_use_q_on(target)) {
//            float q_range = player->get_spell(0) ? player->get_spell(0)->get_cast_range() : 680.f;
//            float distance = player->get_position().distance(target->get_position());
//
//            if (distance <= q_range) {
//                if (vengaboys::OrderManager::get_instance().cast_spell(0, target)) {
//                    combat_state.last_q_time = current_time;
//                    combat_state.q_target_network_id = target->get_network_id();
//                    this->log_debug("Cast Q on %s", target->get_char_name().c_str());
//                }
//            }
//        }
//
//        // W usage
//        if (settings.use_w_combo && this->can_cast_w()) {
//            bool should_use_w = false;
//
//            // Use for chase if target is getting away
//            if (settings.w_chase) {
//                float distance = player->get_position().distance(target->get_position());
//                float attack_range = player->get_attack_range() + player->get_bounding_radius() + target->get_bounding_radius();
//
//                if (distance > attack_range * 0.9f && distance < attack_range * 2.0f) {
//                    should_use_w = true;
//                }
//            }
//
//            // Use defensively if in danger
//            if (this->is_in_danger()) {
//                should_use_w = true;
//            }
//
//            if (should_use_w) {
//                if (vengaboys::OrderManager::get_instance().cast_spell(1, player->get_position())) {
//                    combat_state.last_w_time = current_time;
//                    this->log_debug("Cast W in combo");
//                }
//            }
//        }
//
//        // R usage in combat
//        if (settings.use_r_combo && this->can_cast_r()) {
//            float r_range = player->get_spell(3) ? player->get_spell(3)->get_cast_range() : 600.f;
//
//            // First priority: Place mushroom on immobile target
//            if (settings.r_on_immobile) {
//                bool target_immobile = target->has_buff_of_type(buff_type::stun) ||
//                    target->has_buff_of_type(buff_type::snare) ||
//                    target->has_buff_of_type(buff_type::suppression);
//
//                if (target_immobile) {
//                    float distance = player->get_position().distance(target->get_position());
//                    if (distance <= r_range) {
//                        if (vengaboys::OrderManager::get_instance().cast_spell(3, target->get_position())) {
//                            combat_state.last_r_time = current_time;
//                            this->log_debug("Cast R on immobile target %s", target->get_char_name().c_str());
//                            return; // Don't try other R placements
//                        }
//                    }
//                }
//            }
//
//            // Second priority: Place mushroom in target's path
//            if (target->is_moving()) {
//                // Calculate predicted position
//                auto path = target->get_path();
//                if (!path.empty() && path.size() > 1) {
//                    // Get the target's movement direction
//                    math::vector3 direction = (path[path.size() - 1] - path[0]).normalized();
//
//                    // Place mushroom ahead of the target
//                    math::vector3 mushroom_pos = target->get_position() + direction * 100.f;
//
//                    float distance = player->get_position().distance(mushroom_pos);
//                    if (distance <= r_range) {
//                        if (vengaboys::OrderManager::get_instance().cast_spell(3, mushroom_pos)) {
//                            combat_state.last_r_time = current_time;
//                            this->log_debug("Cast R in target's path");
//                            return;
//                        }
//                    }
//                }
//            }
//
//            // Third priority: Place mushroom at choke point or saved spot
//            MushroomSpot* best_spot = this->get_best_mushroom_spot();
//            if (best_spot) {
//                float distance = player->get_position().distance(best_spot->position);
//                if (distance <= r_range) {
//                    if (vengaboys::OrderManager::get_instance().cast_spell(3, best_spot->position)) {
//                        combat_state.last_r_time = current_time;
//                        this->log_debug("Cast R at saved spot %s during combat", best_spot->name.c_str());
//                    }
//                }
//            }
//        }
//    }
//
//    void TeemoModule::handle_harass() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) return;
//
//        // Only harass if we have enough mana
//        float mana_percent = (player->get_par() / player->get_max_par()) * 100.f;
//        if (mana_percent < settings.harass_mana_threshold) return;
//
//        // Get target
//        game_object* target = this->get_q_target();
//        if (!target) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        // Q usage
//        if (settings.use_q_harass && this->can_cast_q() && this->should_use_q_on(target)) {
//            float q_range = player->get_spell(0) ? player->get_spell(0)->get_cast_range() : 680.f;
//            float distance = player->get_position().distance(target->get_position());
//
//            if (distance <= q_range) {
//                if (vengaboys::OrderManager::get_instance().cast_spell(0, target)) {
//                    combat_state.last_q_time = current_time;
//                    combat_state.q_target_network_id = target->get_network_id();
//                    this->log_debug("Cast Q harass on %s", target->get_char_name().c_str());
//                }
//            }
//        }
//    }
//
//    void TeemoModule::handle_lane_clear() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) return;
//
//        // Only farm with abilities if we have enough mana
//        float mana_percent = (player->get_par() / player->get_max_par()) * 100.f;
//        if (mana_percent < settings.clear_mana_threshold) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        // Q usage for last hitting
//        if (settings.use_q_clear && this->can_cast_q()) {
//            float q_range = player->get_spell(0) ? player->get_spell(0)->get_cast_range() : 680.f;
//
//            // Find minion that can be last hit with Q
//            for (auto minion : g_sdk->object_manager->get_minions()) {
//                if (!minion || !minion->is_valid() || minion->is_dead() ||
//                    minion->get_team_id() == player->get_team_id()) continue;
//
//                float distance = player->get_position().distance(minion->get_position());
//                if (distance <= q_range) {
//                    // Check if we can kill with Q
//                    float q_damage = this->get_q_damage(minion);
//                    if (q_damage >= minion->get_hp()) {
//                        if (vengaboys::OrderManager::get_instance().cast_spell(0, minion)) {
//                            combat_state.last_q_time = current_time;
//                            this->log_debug("Cast Q to last hit minion");
//                            break;
//                        }
//                    }
//                }
//            }
//        }
//
//        // R usage for minion clusters
//        if (settings.use_r_clear && this->can_cast_r()) {
//            // Only if we have a good amount of mushrooms stacked
//            auto spell = player->get_spell(3);
//            if (spell && spell->get_charges() >= 2 + settings.r_maintain_reserve) {
//                float r_range = spell->get_cast_range();
//
//                // Find clusters of minions
//                std::vector<std::pair<math::vector3, int>> minion_clusters;
//
//                for (auto minion : g_sdk->object_manager->get_minions()) {
//                    if (!minion || !minion->is_valid() || minion->is_dead() ||
//                        minion->get_team_id() == player->get_team_id()) continue;
//
//                    float distance = player->get_position().distance(minion->get_position());
//                    if (distance <= r_range) {
//                        // Count minions in explosion radius
//                        int nearby_count = 0;
//                        for (auto other : g_sdk->object_manager->get_minions()) {
//                            if (!other || !other->is_valid() || !other->is_dead() ||
//                                other->get_team_id() == player->get_team_id()) continue;
//
//                            if (other->get_position().distance(minion->get_position()) <= 250.f) {
//                                nearby_count++;
//                            }
//                        }
//
//                        if (nearby_count >= settings.r_clear_min_minions) {
//                            minion_clusters.push_back({ minion->get_position(), nearby_count });
//                        }
//                    }
//                }
//
//                // Place mushroom at best cluster
//                if (!minion_clusters.empty()) {
//                    // Sort by minion count
//                    std::sort(minion_clusters.begin(), minion_clusters.end(),
//                        [](const auto& a, const auto& b) { return a.second > b.second; });
//
//                    if (vengaboys::OrderManager::get_instance().cast_spell(3, minion_clusters[0].first)) {
//                        combat_state.last_r_time = current_time;
//                        this->log_debug("Cast R on minion cluster with %d minions", minion_clusters[0].second);
//                    }
//                }
//            }
//        }
//    }
//
//    void TeemoModule::handle_jungle_clear() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) return;
//
//        // Only use abilities if we have enough mana
//        float mana_percent = (player->get_par() / player->get_max_par()) * 100.f;
//        if (mana_percent < settings.clear_mana_threshold) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        // Find best monster target
//        game_object* best_target = nullptr;
//        float best_priority = 0.f;
//
//        for (auto monster : g_sdk->object_manager->get_monsters()) {
//            if (!monster || !monster->is_valid() || monster->is_dead()) continue;
//
//            float distance = player->get_position().distance(monster->get_position());
//            float priority = 0.f;
//
//            // Prioritize by distance and monster type
//            if (monster->is_epic_monster()) {
//                priority = 100.f; // Epic monsters (Baron, Dragon)
//            }
//            else if (monster->is_large_monster()) {
//                priority = 50.f;  // Large monsters (Blue, Red, etc.)
//            }
//            else {
//                priority = 10.f;  // Small monsters
//            }
//
//            // Closer monsters get higher priority
//            priority += (1000.f - distance) * 0.05f;
//
//            if (priority > best_priority) {
//                best_priority = priority;
//                best_target = monster;
//            }
//        }
//
//        if (!best_target) return;
//
//        // Q usage
//        if (settings.use_q_jungle && this->can_cast_q()) {
//            float q_range = player->get_spell(0) ? player->get_spell(0)->get_cast_range() : 680.f;
//            float distance = player->get_position().distance(best_target->get_position());
//
//            if (distance <= q_range) {
//                if (vengaboys::OrderManager::get_instance().cast_spell(0, best_target)) {
//                    combat_state.last_q_time = current_time;
//                    this->log_debug("Cast Q on jungle monster");
//                }
//            }
//        }
//
//        // R usage for epic monsters
//        if (settings.use_r_jungle && this->can_cast_r() && settings.use_r_objectives &&
//            best_target->is_epic_monster()) {
//
//            float r_range = player->get_spell(3) ? player->get_spell(3)->get_cast_range() : 600.f;
//            float distance = player->get_position().distance(best_target->get_position());
//
//            if (distance <= r_range) {
//                if (vengaboys::OrderManager::get_instance().cast_spell(3, best_target->get_position())) {
//                    combat_state.last_r_time = current_time;
//                    this->log_debug("Cast R on epic monster");
//                }
//            }
//        }
//    }
//
//    void TeemoModule::handle_flee() {
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || player->is_dead()) return;
//
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        // W usage for movement speed
//        if (settings.use_w_flee && this->can_cast_w()) {
//            if (vengaboys::OrderManager::get_instance().cast_spell(1, player->get_position())) {
//                combat_state.last_w_time = current_time;
//                this->log_debug("Cast W for fleeing");
//            }
//        }
//
//        // R usage for escape
//        if (settings.drop_r_when_fleeing && this->can_cast_r()) {
//            // Find pursuing enemy
//            game_object* closest_enemy = nullptr;
//            float closest_distance = FLT_MAX;
//
//            for (auto enemy : g_sdk->object_manager->get_heroes()) {
//                if (!enemy || !enemy->is_valid() || enemy->is_dead() ||
//                    enemy->get_team_id() == player->get_team_id()) continue;
//
//                float distance = player->get_position().distance(enemy->get_position());
//                if (distance < closest_distance && distance < 1000.f) {
//                    closest_distance = distance;
//                    closest_enemy = enemy;
//                }
//            }
//
//            if (closest_enemy) {
//                // Place mushroom between player and enemy
//                math::vector3 direction = (player->get_position() - closest_enemy->get_position()).normalized();
//                math::vector3 mushroom_pos = player->get_position() - direction * 100.f;
//
//                float r_range = player->get_spell(3) ? player->get_spell(3)->get_cast_range() : 600.f;
//                float distance = player->get_position().distance(mushroom_pos);
//
//                if (distance <= r_range) {
//                    if (vengaboys::OrderManager::get_instance().cast_spell(3, mushroom_pos)) {
//                        combat_state.last_r_time = current_time;
//                        this->log_debug("Dropped R while fleeing");
//                    }
//                }
//            }
//        }
//    }
//
//    bool TeemoModule::before_attack(orb_sdk::event_data* data) {
//        if (!initialized || !data || !data->target) return true;
//
//        // Kill secure with Q
//        if (settings.q_ks && this->can_cast_q() &&
//            data->target->is_hero() && this->is_target_valid(data->target)) {
//
//            float q_damage = this->get_q_damage(data->target);
//            if (q_damage >= data->target->get_hp()) {
//                auto player = g_sdk->object_manager->get_local_player();
//                if (!player) return true;
//
//                float q_range = player->get_spell(0) ? player->get_spell(0)->get_cast_range() : 680.f;
//                float distance = player->get_position().distance(data->target->get_position());
//
//                if (distance <= q_range) {
//                    vengaboys::OrderManager::get_instance().cast_spell(0, data->target);
//                    combat_state.last_q_time = g_sdk->clock_facade->get_game_time();
//                    this->log_debug("Used Q to kill secure instead of auto attack");
//                    return false; // Cancel the auto attack
//                }
//            }
//        }
//
//        return true; // Allow the attack
//    }
//
//    void TeemoModule::process_spell_cast(game_object* sender, spell_cast* cast) {
//        if (!initialized || !sender || !cast) return;
//
//        auto player = g_sdk->object_manager->get_local_player();
//        if (!player || sender != player) return;
//
//        int spell_slot = cast->get_spell_slot();
//        float current_time = g_sdk->clock_facade->get_game_time();
//
//        // Track spell usage
//        if (spell_slot == 0) { // Q
//            combat_state.last_q_time = current_time;
//            if (cast->get_target()) {
//                combat_state.q_target_network_id = cast->get_target()->get_network_id();
//            }
//        }
//        else if (spell_slot == 1) { // W
//            combat_state.last_w_time = current_time;
//        }
//        else if (spell_slot == 3) { // R
//            combat_state.last_r_time = current_time;
//        }
//    }
//
//    void TeemoModule::create_menu() {
//        menu = g_sdk->menu_manager->add_category("teemoAIO", "Teemo");
//        if (!menu) return;
//
//        auto combo_menu = menu->add_sub_category("combo_settings", "Combo Settings");
//        if (combo_menu) {
//            combo_menu->add_checkbox("use_q_combo", "Use Q", settings.use_q_combo,
//                [&](bool value) { settings.use_q_combo = value; });
//            combo_menu->add_checkbox("use_w_combo", "Use W", settings.use_w_combo,
//                [&](bool value) { settings.use_w_combo = value; });
//            combo_menu->add_checkbox("use_r_combo", "Use R", settings.use_r_combo,
//                [&](bool value) { settings.use_r_combo = value; });
//        }
//
//        auto harass_menu = menu->add_sub_category("harass_settings", "Harass Settings");
//        if (harass_menu) {
//            harass_menu->add_checkbox("use_q_harass", "Use Q", settings.use_q_harass,
//                [&](bool value) { settings.use_q_harass = value; });
//            harass_menu->add_checkbox("use_aa_harass", "Use Auto Attacks", settings.use_aa_harass,
//                [&](bool value) { settings.use_aa_harass = value; });
//            harass_menu->add_slider_float("harass_mana", "Min Mana %", 0.f, 100.f, 5.f, settings.harass_mana_threshold,
//                [&](float value) { settings.harass_mana_threshold = value; });
//        }
//
//        auto clear_menu = menu->add_sub_category("clear_settings", "Lane Clear Settings");
//        if (clear_menu) {
//            clear_menu->add_checkbox("use_q_clear", "Use Q for Last Hit", settings.use_q_clear,
//                [&](bool value) { settings.use_q_clear = value; });
//            clear_menu->add_checkbox("use_r_clear", "Use R for Wave Clear", settings.use_r_clear,
//                [&](bool value) { settings.use_r_clear = value; });
//            clear_menu->add_slider_int("r_clear_minions", "Min Minions for R", 2, 6, 1, settings.r_clear_min_minions,
//                [&](int value) { settings.r_clear_min_minions = value; });
//            clear_menu->add_slider_float("clear_mana", "Min Mana %", 0.f, 100.f, 5.f, settings.clear_mana_threshold,
//                [&](float value) { settings.clear_mana_threshold = value; });
//        }
//
//        auto jungle_menu = menu->add_sub_category("jungle_settings", "Jungle Clear Settings");
//        if (jungle_menu) {
//            jungle_menu->add_checkbox("use_q_jungle", "Use Q on Monsters", settings.use_q_jungle,
//                [&](bool value) { settings.use_q_jungle = value; });
//            jungle_menu->add_checkbox("use_r_jungle", "Use R on Camp", settings.use_r_jungle,
//                [&](bool value) { settings.use_r_jungle = value; });
//            jungle_menu->add_checkbox("use_r_objectives", "Use R on Objectives", settings.use_r_objectives,
//                [&](bool value) { settings.use_r_objectives = value; });
//        }
//
//        auto flee_menu = menu->add_sub_category("flee_settings", "Flee Settings");
//        if (flee_menu) {
//            flee_menu->add_checkbox("use_w_flee", "Use W When Fleeing", settings.use_w_flee,
//                [&](bool value) { settings.use_w_flee = value; });
//            flee_menu->add_checkbox("drop_r_flee", "Drop R When Fleeing", settings.drop_r_when_fleeing,
//                [&](bool value) { settings.drop_r_when_fleeing = value; });
//        }
//
//        auto q_menu = menu->add_sub_category("q_settings", "Q Settings");
//        if (q_menu) {
//            const char* priority_names[] = {
//                "Auto (Default TS)",
//                "Lowest Health",
//                "Highest Damage",
//                "Most Dangerous",
//                "Closest",
//                "Custom"
//            };
//
//            std::vector<std::string> priority_options;
//            for (int i = 0; i < 6; i++) {
//                priority_options.push_back(priority_names[i]);
//            }
//
//            q_menu->add_combo("q_priority", "Q Target Priority", priority_options,
//                static_cast<int>(settings.q_priority),
//                [&](int value) {
//                    settings.q_priority = static_cast<QTargetPriority>(value);
//                });
//
//            q_menu->add_checkbox("q_auto_harass", "Auto Harass with Q", settings.q_auto_harass,
//                [&](bool value) { settings.q_auto_harass = value; });
//            q_menu->add_checkbox("q_ks", "Use Q for Kill Secure", settings.q_ks,
//                [&](bool value) { settings.q_ks = value; });
//            q_menu->add_checkbox("save_q_blind", "Save Q for Blinding ADCs", settings.save_q_for_blind,
//                [&](bool value) { settings.save_q_for_blind = value; });
//            q_menu->add_checkbox("blind_adc", "Prioritize Blinding ADCs", settings.blind_adc_priority,
//                [&](bool value) { settings.blind_adc_priority = value; });
//        }
//
//        auto w_menu = menu->add_sub_category("w_settings", "W Settings");
//        if (w_menu) {
//            w_menu->add_checkbox("w_auto", "Auto Use W in Danger", settings.w_auto_use,
//                [&](bool value) { settings.w_auto_use = value; });
//            w_menu->add_slider_float("w_hp", "Auto W HP Threshold %", 0.f, 100.f, 5.f, settings.w_hp_threshold,
//                [&](float value) { settings.w_hp_threshold = value; });
//            w_menu->add_checkbox("w_chase", "Use W to Chase", settings.w_chase,
//                [&](bool value) { settings.w_chase = value; });
//            w_menu->add_checkbox("w_dodge", "Use W to Dodge Skillshots", settings.w_dodge_skillshot,
//                [&](bool value) { settings.w_dodge_skillshot = value; });
//        }
//
//        auto r_menu = menu->add_sub_category("r_settings", "R Settings");
//        if (r_menu) {
//            r_menu->add_checkbox("r_auto", "Auto Place Mushrooms", settings.r_auto_place,
//                [&](bool value) { settings.r_auto_place = value; });
//            r_menu->add_checkbox("r_use_spots", "Use Saved Mushroom Spots", settings.r_use_saved_spots,
//                [&](bool value) { settings.r_use_saved_spots = value; });
//            r_menu->add_checkbox("r_bush", "Prioritize Bushes", settings.r_in_bush_priority,
//                [&](bool value) { settings.r_in_bush_priority = value; });
//            r_menu->add_slider_int("r_reserve", "Keep Mushroom Reserve", 0, 3, 1, settings.r_maintain_reserve,
//                [&](int value) { settings.r_maintain_reserve = value; });
//            r_menu->add_checkbox("r_block_paths", "Block Common Paths", settings.r_block_paths,
//                [&](bool value) { settings.r_block_paths = value; });
//            r_menu->add_checkbox("r_immobile", "Mushroom Immobile Enemies", settings.r_on_immobile,
//                [&](bool value) { settings.r_on_immobile = value; });
//        }
//
//        auto draw_menu = menu->add_sub_category("draw_settings", "Drawing Settings");
//        if (draw_menu) {
//            draw_menu->add_checkbox("draw_q", "Draw Q Range", settings.draw_q_range,
//                [&](bool value) { settings.draw_q_range = value; });
//            draw_menu->add_checkbox("draw_r", "Draw R Range", settings.draw_r_range,
//                [&](bool value) { settings.draw_r_range = value; });
//            draw_menu->add_checkbox("draw_spots", "Draw Mushroom Spots", settings.draw_r_spots,
//                [&](bool value) { settings.draw_r_spots = value; });
//            draw_menu->add_checkbox("draw_damage", "Draw Damage Prediction", settings.draw_damage_prediction,
//                [&](bool value) { settings.draw_damage_prediction = value; });
//
//            draw_menu->add_colorpicker("q_color", "Q Range Color", settings.draw_q_color,
//                [&](uint32_t color) { settings.draw_q_color = color; });
//            draw_menu->add_colorpicker("r_color", "R Range Color", settings.draw_r_color,
//                [&](uint32_t color) { settings.draw_r_color = color; });
//        }
//
//        auto spot_menu = menu->add_sub_category("spot_settings", "Mushroom Spots");
//        if (spot_menu) {
//            spot_menu->add_checkbox("r_use_saved_spots", "Use Saved Spots", settings.r_use_saved_spots,
//                [&](bool value) { settings.r_use_saved_spots = value; });
//
//            spot_menu->add_checkbox("r_auto_place", "Auto Place Mushrooms", settings.r_auto_place,
//                [&](bool value) { settings.r_auto_place = value; });
//
//            spot_menu->add_checkbox("draw_r_spots", "Draw Mushroom Spots", settings.draw_r_spots,
//                [&](bool value) { settings.draw_r_spots = value; });
//
//            // Add list of spots
//            for (size_t i = 0; i < mushroom_spots.size() && i < 10; i++) {
//                std::string spot_id = "spot_" + std::to_string(i);
//                std::string menu_text = mushroom_spots[i].name + " (Enabled: ";
//                menu_text += mushroom_spots[i].enabled ? "Yes)" : "No)";
//
//                spot_menu->add_checkbox(spot_id.c_str(), menu_text.c_str(), mushroom_spots[i].enabled,
//                    [this, i](bool value) {
//                        if (i < mushroom_spots.size()) {
//                            mushroom_spots[i].enabled = value;
//                            this->save_mushroom_spots();
//                        }
//                    });
//            }
//        }
//
//        auto dev_menu = menu->add_sub_category("dev_settings", "Developer Settings");
//        if (dev_menu) {
//            dev_menu->add_checkbox("dev_mode", "Developer Mode", settings.dev_mode,
//                [&](bool value) {
//                    settings.dev_mode = value;
//                    if (value) {
//                        g_sdk->log_console("[Teemo] Developer mode activated");
//                    }
//                });
//
//            settings.save_spot_key = new std::string("F5");
//            dev_menu->add_hotkey("save_spot", "Save Mushroom Spot", 'F', false, false,
//                [&](std::string* key, bool value) {
//                    if (value && settings.dev_mode) {
//                        this->save_current_spot();
//                    }
//                    settings.save_spot_key = key;
//                });
//
//            dev_menu->add_checkbox("debug_mode", "Debug Mode", settings.debug_mode,
//                [&](bool value) {
//                    settings.debug_mode = value;
//                    // Also update OrderManager debug mode
//                    vengaboys::OrderManager::get_instance().set_debug_mode(value);
//                });
//        }
//    }
//
//    void __fastcall on_update() {
//        TeemoModule::get_instance().update();
//    }
//
//    void __fastcall on_draw() {
//        TeemoModule::get_instance().draw();
//    }
//
//    bool __fastcall on_before_attack(orb_sdk::event_data* data) {
//        return TeemoModule::get_instance().before_attack(data);
//    }
//
//    void __fastcall on_process_spell_cast(game_object* sender, spell_cast* cast) {
//        TeemoModule::get_instance().process_spell_cast(sender, cast);
//    }
//
//    void load() {
//        TeemoModule::get_instance().load();
//    }
//
//    void unload() {
//        TeemoModule::get_instance().unload();
//    }
//}