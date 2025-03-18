// Aphelios.cpp - Adapted to use pre-defined hash values

#include "Aphelios.hpp"
#include "OrderManager.hpp"
#include <algorithm>
#include <numeric>
#include <stdarg.h>

namespace aphelios {

    // Pre-defined buff hash values - no need to extract at runtime
    namespace buff_hashes {
        // Main weapon buffs
        uint32_t CALIBRUM_MANAGER = 0xae20393a;
        uint32_t SEVERUM_MANAGER = 0x3d561ab0;
        uint32_t GRAVITUM_MANAGER = 0xdd3aa26e;
        uint32_t INFERNUM_MANAGER = 0x1c69625d;
        uint32_t CRESCENDUM_MANAGER = 0xe19d9152;

        // Off-hand weapon buffs
        uint32_t OFFHAND_CALIBRUM = 0xf836618a;
        uint32_t OFFHAND_SEVERUM = 0x0ebf7e32;
        uint32_t OFFHAND_GRAVITUM = 0xb3ed2352;
        uint32_t OFFHAND_INFERNUM = 0x06020d97;
        uint32_t OFFHAND_CRESCENDUM = 0x6769748e;

        // Special buff markers
        uint32_t CALIBRUM_MARK = 0x9f57b629;
        uint32_t GRAVITUM_DEBUFF = 0xf1142b3d;
        uint32_t GRAVITUM_ROOT = 0x1cdf09cb;
        uint32_t CRESCENDUM_TURRET = 0x48e99536; // Placeholder - update with actual value if needed

        // Other related buffs
        uint32_t CRESCENDUM_ORBIT = 0x7a118de5; // Placeholder - update with actual value if needed
        uint32_t SEVERUM_COUNT = 0x319b0705;
        uint32_t APHELIOS_CANNOT_MOVE = 0xa5dc0904;
    }

    Settings settings;
    bool initialized = false;
    menu_category* menu = nullptr;
    uint32_t infotab_text_id = 0;

    static WeaponState cached_weapon_state;
    static float last_weapon_check = 0.f;
    static const float WEAPON_CACHE_DURATION = 0.1f;

    std::unordered_map<uint32_t, float> damage_cache;
    float last_damage_cache_clear = 0.f;
    const float DAMAGE_CACHE_DURATION = 0.25f;

    // Anti-magneting feature variables
    static bool is_magneting_detected = false;
    static float last_magneting_check = 0.f;
    static const float MAGNETING_CHECK_INTERVAL = 0.2f;
    static int consecutive_magnetic_frames = 0;
    static const int MAGNETIC_THRESHOLD = 3;
    static math::vector3 last_cursor_pos;

    void log_debug(const char* format, ...) {
        if (!settings.debug_mode) return;

        va_list args;
        va_start(args, format);
        char buffer[512];
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        g_sdk->log_console("[Aphelios Debug] %s", buffer);
    }

    /*
    // These hash extraction functions are commented out but kept for reference
    void extract_aphelios_buff_hashes() {
        g_sdk->log_console("[Aphelios] ===== BUFF HASH EXTRACTOR =====");

        try {
            auto buff_map = g_sdk->get_buffs_hash_map();
            g_sdk->log_console("[Aphelios] Total buffs found: %d", buff_map.size());

            int aphelios_buffs_found = 0;

            for (const auto& pair : buff_map) {
                uint32_t hash = pair.first;
                const char* name = pair.second;

                // Look specifically for Aphelios-related buffs
                if (name && strstr(name, "Aphelios") != nullptr) {
                    g_sdk->log_console("[BUFF] %s = 0x%08x", name, hash);
                    aphelios_buffs_found++;
                }
            }

            if (aphelios_buffs_found == 0) {
                g_sdk->log_console("[WARNING] No Aphelios buffs found in hash map!");
                g_sdk->log_console("[INFO] Printing first 10 buffs from map as sample:");

                int count = 0;
                for (const auto& pair : buff_map) {
                    if (count++ >= 10) break;
                    if (pair.second) {
                        g_sdk->log_console("[SAMPLE] %s = 0x%08x", pair.second, pair.first);
                    }
                }
            }
            else {
                g_sdk->log_console("[SUCCESS] Found %d Aphelios-related buffs", aphelios_buffs_found);
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[ERROR] Exception in buff hash extraction: %s", e.what());
        }
        catch (...) {
            g_sdk->log_console("[ERROR] Unknown exception in buff hash extraction");
        }

        g_sdk->log_console("[Aphelios] ===== EXTRACTION COMPLETE =====");
    }

    void extract_critical_aphelios_hashes() {
        g_sdk->log_console("[HASH EXTRACTOR] Starting critical hash extraction");

        // Create a local player instance
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            g_sdk->log_console("[HASH EXTRACTOR] No local player found");
            return;
        }

        // Get all buffs directly from the player
        auto buffs = player->get_buffs();
        g_sdk->log_console("[HASH EXTRACTOR] Player has %d buffs", buffs.size());

        // Extract and print information from each buff
        for (auto* buff : buffs) {
            if (!buff) continue;

            std::string name = buff->get_name();
            uint32_t hash = buff->get_hash();

            // Print ALL buffs for debugging
            g_sdk->log_console("[HASH EXTRACTOR] Buff: %s, Hash: 0x%08x, Count: %d, Stacks: %d",
                name.c_str(), hash, buff->get_count(), buff->get_stacks());

            // Identify specific Aphelios buffs we need
            if (name.find("ApheliosCalibrum") != std::string::npos) {
                if (name.find("Mark") != std::string::npos) {
                    g_sdk->log_console("[CRITICAL HASH] CALIBRUM_MARK = 0x%08x;", hash);
                    buff_hashes::CALIBRUM_MARK = hash;
                }
                else if (name.find("Manager") != std::string::npos) {
                    if (name.find("Offhand") != std::string::npos) {
                        g_sdk->log_console("[CRITICAL HASH] OFFHAND_CALIBRUM = 0x%08x;", hash);
                        buff_hashes::OFFHAND_CALIBRUM = hash;
                    }
                    else {
                        g_sdk->log_console("[CRITICAL HASH] CALIBRUM_MANAGER = 0x%08x;", hash);
                        buff_hashes::CALIBRUM_MANAGER = hash;
                    }
                }
            }
            else if (name.find("ApheliosSeverum") != std::string::npos) {
                if (name.find("Manager") != std::string::npos) {
                    if (name.find("Offhand") != std::string::npos) {
                        g_sdk->log_console("[CRITICAL HASH] OFFHAND_SEVERUM = 0x%08x;", hash);
                        buff_hashes::OFFHAND_SEVERUM = hash;
                    }
                    else {
                        g_sdk->log_console("[CRITICAL HASH] SEVERUM_MANAGER = 0x%08x;", hash);
                        buff_hashes::SEVERUM_MANAGER = hash;
                    }
                }
                else if (name.find("Count") != std::string::npos) {
                    g_sdk->log_console("[CRITICAL HASH] SEVERUM_COUNT = 0x%08x;", hash);
                    buff_hashes::SEVERUM_COUNT = hash;
                }
            }
            else if (name.find("ApheliosGravitum") != std::string::npos) {
                if (name.find("Debuff") != std::string::npos) {
                    g_sdk->log_console("[CRITICAL HASH] GRAVITUM_DEBUFF = 0x%08x;", hash);
                    buff_hashes::GRAVITUM_DEBUFF = hash;
                }
                else if (name.find("Root") != std::string::npos) {
                    g_sdk->log_console("[CRITICAL HASH] GRAVITUM_ROOT = 0x%08x;", hash);
                    buff_hashes::GRAVITUM_ROOT = hash;
                }
                else if (name.find("Manager") != std::string::npos) {
                    if (name.find("Offhand") != std::string::npos) {
                        g_sdk->log_console("[CRITICAL HASH] OFFHAND_GRAVITUM = 0x%08x;", hash);
                        buff_hashes::OFFHAND_GRAVITUM = hash;
                    }
                    else {
                        g_sdk->log_console("[CRITICAL HASH] GRAVITUM_MANAGER = 0x%08x;", hash);
                        buff_hashes::GRAVITUM_MANAGER = hash;
                    }
                }
            }
            else if (name.find("ApheliosInfernum") != std::string::npos) {
                if (name.find("Manager") != std::string::npos) {
                    if (name.find("Offhand") != std::string::npos) {
                        g_sdk->log_console("[CRITICAL HASH] OFFHAND_INFERNUM = 0x%08x;", hash);
                        buff_hashes::OFFHAND_INFERNUM = hash;
                    }
                    else {
                        g_sdk->log_console("[CRITICAL HASH] INFERNUM_MANAGER = 0x%08x;", hash);
                        buff_hashes::INFERNUM_MANAGER = hash;
                    }
                }
            }
            else if (name.find("ApheliosCrescendum") != std::string::npos) {
                if (name.find("Turret") != std::string::npos) {
                    g_sdk->log_console("[CRITICAL HASH] CRESCENDUM_TURRET = 0x%08x;", hash);
                    buff_hashes::CRESCENDUM_TURRET = hash;
                }
                else if (name.find("Orbit") != std::string::npos) {
                    g_sdk->log_console("[CRITICAL HASH] CRESCENDUM_ORBIT = 0x%08x;", hash);
                    buff_hashes::CRESCENDUM_ORBIT = hash;
                }
                else if (name.find("Manager") != std::string::npos) {
                    if (name.find("Offhand") != std::string::npos) {
                        g_sdk->log_console("[CRITICAL HASH] OFFHAND_CRESCENDUM = 0x%08x;", hash);
                        buff_hashes::OFFHAND_CRESCENDUM = hash;
                    }
                    else {
                        g_sdk->log_console("[CRITICAL HASH] CRESCENDUM_MANAGER = 0x%08x;", hash);
                        buff_hashes::CRESCENDUM_MANAGER = hash;
                    }
                }
            }
        }

        g_sdk->log_console("[HASH EXTRACTOR] Hash extraction complete");
    }

    void update_buff_hashes() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        // Get all buffs from player
        auto buffs = player->get_buffs();

        // Now check for specific Aphelios buffs
        for (auto* buff : buffs) {
            if (!buff) continue;

            std::string name = buff->get_name();
            uint32_t hash = buff->get_hash();

            // Process all Aphelios-related buffs
            if (name.find("Aphelios") != std::string::npos) {
                if (settings.debug_mode) {
                    g_sdk->log_console("[Aphelios] Found buff: %s, Hash: 0x%08x", name.c_str(), hash);
                }

                // Main hand buffs
                if (name.find("Calibrum") != std::string::npos && name.find("Manager") != std::string::npos && name.find("Offhand") == std::string::npos && name.find("OffHand") == std::string::npos) {
                    buff_hashes::CALIBRUM_MANAGER = hash;
                    g_sdk->log_console("[HASH] CALIBRUM_MANAGER = 0x%08x;", hash);
                }
                else if (name.find("Severum") != std::string::npos && name.find("Manager") != std::string::npos && name.find("Offhand") == std::string::npos && name.find("OffHand") == std::string::npos) {
                    buff_hashes::SEVERUM_MANAGER = hash;
                    g_sdk->log_console("[HASH] SEVERUM_MANAGER = 0x%08x;", hash);
                }
                else if (name.find("Gravitum") != std::string::npos && name.find("Manager") != std::string::npos && name.find("Offhand") == std::string::npos && name.find("OffHand") == std::string::npos) {
                    buff_hashes::GRAVITUM_MANAGER = hash;
                    g_sdk->log_console("[HASH] GRAVITUM_MANAGER = 0x%08x;", hash);
                }
                else if (name.find("Infernum") != std::string::npos && name.find("Manager") != std::string::npos && name.find("Offhand") == std::string::npos && name.find("OffHand") == std::string::npos) {
                    buff_hashes::INFERNUM_MANAGER = hash;
                    g_sdk->log_console("[HASH] INFERNUM_MANAGER = 0x%08x;", hash);
                }
                else if (name.find("Crescendum") != std::string::npos && name.find("Manager") != std::string::npos && name.find("Offhand") == std::string::npos && name.find("OffHand") == std::string::npos) {
                    buff_hashes::CRESCENDUM_MANAGER = hash;
                    g_sdk->log_console("[HASH] CRESCENDUM_MANAGER = 0x%08x;", hash);
                }

                // Offhand buffs
                else if (name.find("Calibrum") != std::string::npos && (name.find("Offhand") != std::string::npos || name.find("OffHand") != std::string::npos)) {
                    buff_hashes::OFFHAND_CALIBRUM = hash;
                    g_sdk->log_console("[HASH] OFFHAND_CALIBRUM = 0x%08x;", hash);
                }
                else if (name.find("Severum") != std::string::npos && (name.find("Offhand") != std::string::npos || name.find("OffHand") != std::string::npos)) {
                    buff_hashes::OFFHAND_SEVERUM = hash;
                    g_sdk->log_console("[HASH] OFFHAND_SEVERUM = 0x%08x;", hash);
                }
                else if (name.find("Gravitum") != std::string::npos && (name.find("Offhand") != std::string::npos || name.find("OffHand") != std::string::npos)) {
                    buff_hashes::OFFHAND_GRAVITUM = hash;
                    g_sdk->log_console("[HASH] OFFHAND_GRAVITUM = 0x%08x;", hash);
                }
                else if (name.find("Infernum") != std::string::npos && (name.find("Offhand") != std::string::npos || name.find("OffHand") != std::string::npos)) {
                    buff_hashes::OFFHAND_INFERNUM = hash;
                    g_sdk->log_console("[HASH] OFFHAND_INFERNUM = 0x%08x;", hash);
                }
                else if (name.find("Crescendum") != std::string::npos && (name.find("Offhand") != std::string::npos || name.find("OffHand") != std::string::npos)) {
                    buff_hashes::OFFHAND_CRESCENDUM = hash;
                    g_sdk->log_console("[HASH] OFFHAND_CRESCENDUM = 0x%08x;", hash);
                }

                // Special buffs
                else if (name.find("CalibrumMark") != std::string::npos) {
                    buff_hashes::CALIBRUM_MARK = hash;
                    g_sdk->log_console("[HASH] CALIBRUM_MARK = 0x%08x;", hash);
                }
                else if (name.find("GravitumDebuff") != std::string::npos) {
                    buff_hashes::GRAVITUM_DEBUFF = hash;
                    g_sdk->log_console("[HASH] GRAVITUM_DEBUFF = 0x%08x;", hash);
                }
                else if (name.find("GravitumRoot") != std::string::npos) {
                    buff_hashes::GRAVITUM_ROOT = hash;
                    g_sdk->log_console("[HASH] GRAVITUM_ROOT = 0x%08x;", hash);
                }
                else if (name.find("CrescendumTurret") != std::string::npos) {
                    buff_hashes::CRESCENDUM_TURRET = hash;
                    g_sdk->log_console("[HASH] CRESCENDUM_TURRET = 0x%08x;", hash);
                }
                else if (name.find("CrescendumOrbit") != std::string::npos) {
                    buff_hashes::CRESCENDUM_ORBIT = hash;
                    g_sdk->log_console("[HASH] CRESCENDUM_ORBIT = 0x%08x;", hash);
                }
                else if (name.find("SeverumCount") != std::string::npos) {
                    buff_hashes::SEVERUM_COUNT = hash;
                    g_sdk->log_console("[HASH] SEVERUM_COUNT = 0x%08x;", hash);
                }
            }
        }
    }

    void init_buff_hashes() {
        g_sdk->log_console("[Aphelios] Using pre-defined buff hashes");
        // The hash values are now directly initialized in the buff_hashes namespace
    }
    */

    bool has_weapon_buff(game_object* player, const std::string& buff_name) {
        if (!player) return false;

        std::string name_copy = buff_name;
        auto buff = player->get_buff_by_name(name_copy);
        return buff && buff->is_active();
    }

    buff_instance* get_buff_by_hash_safe(game_object* obj, uint32_t hash) {
        if (!obj) return nullptr;
        return obj->get_buff_by_hash(hash);
    }

    int get_buff_stacks_safe(buff_instance* buff) {
        if (!buff || !buff->is_active()) return 0;
        return buff->get_stacks();
    }

    float get_buff_count_safe(buff_instance* buff) {
        if (!buff || !buff->is_active()) return 0.f;
        return static_cast<float>(buff->get_count());
    }

    WeaponState WeaponManager::get_current_state() {
        WeaponState state;
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return state;

        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - last_weapon_check < WEAPON_CACHE_DURATION &&
            cached_weapon_state.main_hand.is_active) {
            return cached_weapon_state;
        }

        if (settings.debug_mode) {
            for (auto* buff : player->get_buffs()) {
                if (!buff) continue;
                std::string name = buff->get_name();
                if (name.find("Aphelios") != std::string::npos) {
                    log_debug("Buff: %s Count: %d Stacks: %d", name.c_str(), buff->get_count(), buff->get_stacks());
                }
            }
        }

        // Check for main hand weapon type
        auto calibrum_manager = get_buff_by_hash_safe(player, buff_hashes::CALIBRUM_MANAGER);
        if (calibrum_manager && calibrum_manager->is_active()) {
            state.main_hand.type = WeaponType::CALIBRUM;
            state.main_hand.is_active = true;
            state.main_hand.ammo = get_buff_count_safe(calibrum_manager);
            state.main_hand.time_equipped = calibrum_manager->get_start_time();
            state.main_hand.was_just_equipped = (current_time - state.main_hand.time_equipped) < 0.5f;
        }

        auto severum_manager = get_buff_by_hash_safe(player, buff_hashes::SEVERUM_MANAGER);
        if (severum_manager && severum_manager->is_active()) {
            state.main_hand.type = WeaponType::SEVERUM;
            state.main_hand.is_active = true;
            state.main_hand.ammo = get_buff_count_safe(severum_manager);
            state.main_hand.time_equipped = severum_manager->get_start_time();
            state.main_hand.was_just_equipped = (current_time - state.main_hand.time_equipped) < 0.5f;
        }

        auto gravitum_manager = get_buff_by_hash_safe(player, buff_hashes::GRAVITUM_MANAGER);
        if (gravitum_manager && gravitum_manager->is_active()) {
            state.main_hand.type = WeaponType::GRAVITUM;
            state.main_hand.is_active = true;
            state.main_hand.ammo = get_buff_count_safe(gravitum_manager);
            state.main_hand.time_equipped = gravitum_manager->get_start_time();
            state.main_hand.was_just_equipped = (current_time - state.main_hand.time_equipped) < 0.5f;
        }

        auto infernum_manager = get_buff_by_hash_safe(player, buff_hashes::INFERNUM_MANAGER);
        if (infernum_manager && infernum_manager->is_active()) {
            state.main_hand.type = WeaponType::INFERNUM;
            state.main_hand.is_active = true;
            state.main_hand.ammo = get_buff_count_safe(infernum_manager);
            state.main_hand.time_equipped = infernum_manager->get_start_time();
            state.main_hand.was_just_equipped = (current_time - state.main_hand.time_equipped) < 0.5f;
        }

        auto crescendum_manager = get_buff_by_hash_safe(player, buff_hashes::CRESCENDUM_MANAGER);
        if (crescendum_manager && crescendum_manager->is_active()) {
            state.main_hand.type = WeaponType::CRESCENDUM;
            state.main_hand.is_active = true;
            state.main_hand.ammo = get_buff_count_safe(crescendum_manager);
            state.main_hand.time_equipped = crescendum_manager->get_start_time();
            state.main_hand.was_just_equipped = (current_time - state.main_hand.time_equipped) < 0.5f;
            state.main_hand.stacks = get_buff_stacks_safe(crescendum_manager);
        }

        // Check for off-hand weapon type
        auto offhand_calibrum = get_buff_by_hash_safe(player, buff_hashes::OFFHAND_CALIBRUM);
        if (offhand_calibrum && offhand_calibrum->is_active()) {
            state.off_hand.type = WeaponType::CALIBRUM;
            state.off_hand.is_active = true;
        }

        auto offhand_severum = get_buff_by_hash_safe(player, buff_hashes::OFFHAND_SEVERUM);
        if (offhand_severum && offhand_severum->is_active()) {
            state.off_hand.type = WeaponType::SEVERUM;
            state.off_hand.is_active = true;
        }

        auto offhand_gravitum = get_buff_by_hash_safe(player, buff_hashes::OFFHAND_GRAVITUM);
        if (offhand_gravitum && offhand_gravitum->is_active()) {
            state.off_hand.type = WeaponType::GRAVITUM;
            state.off_hand.is_active = true;
        }

        auto offhand_infernum = get_buff_by_hash_safe(player, buff_hashes::OFFHAND_INFERNUM);
        if (offhand_infernum && offhand_infernum->is_active()) {
            state.off_hand.type = WeaponType::INFERNUM;
            state.off_hand.is_active = true;
        }

        auto offhand_crescendum = get_buff_by_hash_safe(player, buff_hashes::OFFHAND_CRESCENDUM);
        if (offhand_crescendum && offhand_crescendum->is_active()) {
            state.off_hand.type = WeaponType::CRESCENDUM;
            state.off_hand.is_active = true;
        }

        update_weapon_queue(state);
        cached_weapon_state = state;
        last_weapon_check = current_time;

        return state;
    }

    bool has_calibrum_mark(game_object* target) {
        if (!target) return false;
        auto mark = get_buff_by_hash_safe(target, buff_hashes::CALIBRUM_MARK);
        return mark && mark->is_active();
    }

    bool has_gravitum_debuff(game_object* target) {
        if (!target) return false;
        auto debuff = get_buff_by_hash_safe(target, buff_hashes::GRAVITUM_DEBUFF);
        return debuff && debuff->is_active();
    }

    bool is_rooted_by_gravitum(game_object* target) {
        if (!target) return false;
        auto root = get_buff_by_hash_safe(target, buff_hashes::GRAVITUM_ROOT);
        return root && root->is_active();
    }

    // Anti-magneting detection and prevention
    bool check_for_magneting() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - last_magneting_check < MAGNETING_CHECK_INTERVAL)
            return is_magneting_detected;

        last_magneting_check = current_time;

        // Get current cursor position
        math::vector3 cursor_pos = g_sdk->hud_manager->get_cursor_position();

        // Get player position and movement vector
        math::vector3 player_pos = player->get_position();
        auto path = player->get_path();

        if (path.size() <= 1) {
            // Not moving significantly, reset detection
            consecutive_magnetic_frames = 0;
            is_magneting_detected = false;
            last_cursor_pos = cursor_pos;
            return false;
        }

        // Check if player is moving toward an enemy when cursor is elsewhere
        math::vector3 move_direction = path[path.size() > 1 ? 1 : 0] - player_pos;
        math::vector3 cursor_direction = cursor_pos - player_pos;

        // Calculate 2D angles to compare directions
        float move_angle = atan2f(move_direction.z, move_direction.x);
        float cursor_angle = atan2f(cursor_direction.z, cursor_direction.x);

        // Calculate angle difference
        float angle_diff = fabsf(move_angle - cursor_angle);
        if (angle_diff > 3.14159f) // Greater than 180 degrees, take complementary angle
            angle_diff = 2 * 3.14159f - angle_diff;

        // Check if there's a significant difference between cursor direction and movement
        if (angle_diff > 0.7854f) { // ~45 degrees threshold
            // Check if there's an enemy nearby in the movement direction
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() || enemy->get_team_id() == player->get_team_id())
                    continue;

                math::vector3 enemy_direction = enemy->get_position() - player_pos;
                float enemy_angle = atan2f(enemy_direction.z, enemy_direction.x);

                float enemy_angle_diff = fabsf(move_angle - enemy_angle);
                if (enemy_angle_diff > 3.14159f)
                    enemy_angle_diff = 2 * 3.14159f - enemy_angle_diff;

                // If moving toward enemy but cursor is elsewhere
                if (enemy_angle_diff < 0.3927f && enemy->get_position().distance(player_pos) < 800.f) {
                    consecutive_magnetic_frames++;

                    if (consecutive_magnetic_frames >= MAGNETIC_THRESHOLD) {
                        if (settings.debug_mode)
                            log_debug("Magneting detected! Moving toward enemy but cursor is elsewhere");
                        is_magneting_detected = true;
                        return true;
                    }

                    break;
                }
            }
        }
        else {
            // Reset detection counter if directions align
            consecutive_magnetic_frames = 0;
            is_magneting_detected = false;
        }

        last_cursor_pos = cursor_pos;
        return is_magneting_detected;
    }

    void handle_anti_magneting() {
        if (!settings.anti_magneting || !is_magneting_detected)
            return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        // Get cursor position
        math::vector3 cursor_pos = g_sdk->hud_manager->get_cursor_position();

        // Override movement to follow cursor instead of magnetic targeting
        vengaboys::OrderManager::get_instance().move_to(cursor_pos, vengaboys::OrderPriority::High);

        if (settings.debug_mode)
            log_debug("Anti-magneting: Overriding movement to follow cursor");

        // Reset consecutive frames after handling
        consecutive_magnetic_frames = 0;
        is_magneting_detected = false;
    }

    float DamageCalculator::get_auto_attack_damage(game_object* target, bool include_calibrum_mark) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        float damage = sdk::damage->get_aa_damage(player, target);
        if (settings.debug_mode) log_debug("Base AA damage: %.1f", damage);

        if (include_calibrum_mark && has_calibrum_mark(target)) {
            float mark_damage = 15.f + (player->get_level() * 5.f) + (0.2f * player->get_attack_damage());
            damage += mark_damage;
            if (settings.debug_mode) log_debug("Added Calibrum mark damage: %.1f", mark_damage);
        }

        return damage;
    }

    float DamageCalculator::get_q_damage(WeaponType weapon, game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        float base_damage = 0.f;
        float scaling = 0.f;
        auto spell = player->get_spell(0);
        if (!spell) return 0.f;

        int spell_level = spell->get_level();

        // Get weapon state once outside the switch to avoid the initialization error
        auto state = WeaponManager::get_current_state();

        switch (weapon) {
        case WeaponType::CALIBRUM:
            base_damage = 60.f + 40.f * spell_level;
            scaling = 0.6f * player->get_ability_power() + player->get_attack_damage();
            break;
        case WeaponType::SEVERUM:
            base_damage = 10.f + 15.f * spell_level;
            scaling = 0.2f * player->get_attack_damage();
            break;
        case WeaponType::GRAVITUM:
            base_damage = 50.f + 30.f * spell_level;
            scaling = 0.7f * player->get_ability_power();
            break;
        case WeaponType::INFERNUM:
            base_damage = 45.f + 35.f * spell_level;
            scaling = 0.8f * player->get_attack_damage();
            break;
        case WeaponType::CRESCENDUM:
            base_damage = 40.f + 35.f * spell_level;
            scaling = 0.5f * player->get_attack_damage();
            scaling += 0.1f * player->get_attack_damage() * state.main_hand.stacks;
            break;
        default:
            return 0.f;
        }

        float total_damage = sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target, base_damage + scaling);

        if (settings.debug_mode) {
            log_debug("Q damage calculation:");
            log_debug("- Base damage: %.1f", base_damage);
            log_debug("- Scaling: %.1f", scaling);
            log_debug("- Total damage: %.1f", total_damage);
        }

        return total_damage;
    }

    float DamageCalculator::get_r_damage(WeaponType weapon, game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        auto spell = player->get_spell(3);
        if (!spell) return 0.f;

        int r_level = spell->get_level();
        float base_damage = 125.f + 75.f * r_level;
        float ad_scaling = 1.0f * player->get_attack_damage();
        float ap_scaling = 0.7f * player->get_ability_power();

        auto state = WeaponManager::get_current_state();

        float weapon_bonus = 0.f;
        switch (weapon) {
        case WeaponType::CALIBRUM:
            weapon_bonus = 0.2f * (base_damage + ad_scaling);
            break;
        case WeaponType::SEVERUM:
            weapon_bonus = 0.25f * (player->get_max_hp() - player->get_hp());
            break;
        case WeaponType::GRAVITUM:
            weapon_bonus = 0.3f * target->get_move_speed();
            break;
        case WeaponType::INFERNUM: {
            weapon_bonus = 0.25f * (base_damage + ad_scaling + ap_scaling);
            int nearby_enemies = 0;
            for (auto* nearby : g_sdk->object_manager->get_heroes()) {
                if (!nearby || nearby->is_dead() || nearby->get_team_id() == player->get_team_id())
                    continue;
                if (nearby->get_position().distance(target->get_position()) <= 300.f)
                    nearby_enemies++;
            }
            weapon_bonus *= (1.f + 0.25f * nearby_enemies);
            break;
        }
        case WeaponType::CRESCENDUM: {
            weapon_bonus = 0.2f * (base_damage + ad_scaling) * state.main_hand.stacks;
            break;
        }
        default:
            break;
        }

        return sdk::damage->calc_damage(dmg_sdk::damage_type::physical, player, target,
            base_damage + ad_scaling + ap_scaling + weapon_bonus);
    }

    float DamageCalculator::get_mark_damage(game_object* target, WeaponType weapon) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        switch (weapon) {
        case WeaponType::CALIBRUM: {
            if (has_calibrum_mark(target)) {
                return 15.f + (player->get_level() * 5.f) + (0.2f * player->get_attack_damage());
            }
            break;
        }
        case WeaponType::INFERNUM: {
            std::string mark_name = "ApheliosInfernumMark";
            auto mark = player->get_buff_by_name(mark_name);
            if (mark && mark->is_active()) {
                return player->get_attack_damage() * 0.1f;
            }
            break;
        }
        case WeaponType::CRESCENDUM: {
            std::string mark_name = "ApheliosCrescendumMark";
            auto mark = player->get_buff_by_name(mark_name);
            if (mark && mark->is_active()) {
                auto state = WeaponManager::get_current_state();
                return player->get_attack_damage() * (0.05f * state.main_hand.stacks);
            }
            break;
        }
        default:
            break;
        }
        return 0.f;
    }

    void cast_calibrum_q() {
        if (!settings.use_calibrum_q) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast_spell(0)) return;

        if (settings.debug_mode) log_debug("Attempting to cast Calibrum Q");

        pred_sdk::spell_data q_data;
        q_data.spell_type = pred_sdk::spell_type::linear;
        q_data.range = CALIBRUM_Q_RANGE;
        q_data.radius = 60.f;
        q_data.delay = 0.25f;
        q_data.projectile_speed = 1800.f;
        q_data.source = player;
        q_data.spell_slot = 0;
        q_data.targetting_type = pred_sdk::targetting_type::edge_to_edge;
        q_data.forbidden_collisions = {
            pred_sdk::collision_type::yasuo_wall,
            pred_sdk::collision_type::braum_wall,
            pred_sdk::collision_type::unit,
            pred_sdk::collision_type::hero
        };

        auto target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero || !hero->is_valid() || hero->is_dead() ||
                !hero->is_targetable() || !hero->is_visible())
                return false;

            float dist = hero->get_position().distance(player->get_position());
            bool in_range = dist >= settings.calibrum_q_min_range && dist <= CALIBRUM_Q_RANGE;

            if (settings.debug_mode && in_range) {
                log_debug("Found potential target: %s at distance %.1f",
                    hero->get_name().c_str(), dist);
            }

            return in_range;
            });

        if (!target) {
            if (settings.debug_mode) log_debug("No valid target found for Calibrum Q");
            return;
        }

        auto pred = sdk::prediction->predict(target, q_data);
        if (!pred.is_valid) {
            if (settings.debug_mode) log_debug("Calibrum Q prediction invalid!");
            return;
        }
        if (pred.hitchance < pred_sdk::hitchance::high) {
            if (settings.debug_mode) log_debug("Low hitchance (%.1f) for Calibrum Q", pred.hitchance);
            return;
        }

        if (settings.debug_mode) {
            log_debug("Casting Calibrum Q:");
            log_debug("- Target: %s", target->get_name().c_str());
            log_debug("- Hitchance: %.1f", pred.hitchance);
            log_debug("- Cast Position: %.1f, %.1f, %.1f",
                pred.cast_position.x, pred.cast_position.y, pred.cast_position.z);
        }

        // Check for magneting before weave attack
        if (check_for_magneting()) {
            handle_anti_magneting();
            return;
        }

        weave_attack(target);
        vengaboys::OrderManager::get_instance().cast_spell(0, pred.cast_position);
    }

    void cast_severum_q() {
        if (!settings.use_severum_q || !settings.severum_q_toggle) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast_spell(0)) return;

        if (settings.debug_mode) log_debug("Attempting to cast Severum Q");

        game_object* nearest_enemy = nullptr;
        float nearest_distance = FLT_MAX;

        for (auto* enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy || !enemy->is_valid() || !enemy->is_visible() ||
                enemy->is_dead() || !enemy->is_targetable() ||
                enemy->get_team_id() == player->get_team_id())
                continue;

            float dist = enemy->get_position().distance(player->get_position());
            if (dist < nearest_distance && dist <= SEVERUM_Q_RANGE) {
                nearest_enemy = enemy;
                nearest_distance = dist;
                if (settings.debug_mode) log_debug("Found closest enemy: %s at %f distance",
                    enemy->get_name().c_str(), dist);
            }
        }

        if (nearest_enemy) {
            // Check for magneting before casting
            if (check_for_magneting()) {
                handle_anti_magneting();
                return;
            }

            if (settings.debug_mode) log_debug("Using OrderManager to cast Severum Q");
            vengaboys::OrderManager::get_instance().cast_spell(0, nearest_enemy,
                vengaboys::OrderPriority::High);
        }
        else {
            if (settings.debug_mode) log_debug("No valid target found for Severum Q");
        }
    }

    void cast_gravitum_q() {
        if (!settings.use_gravitum_q) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast_spell(0)) return;

        if (settings.debug_mode) log_debug("Attempting to cast Gravitum Q");

        auto target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero || !hero->is_valid() || hero->is_dead() ||
                !hero->is_targetable() || !hero->is_visible())
                return false;

            if (settings.gravitum_q_manual) return false;

            if (!has_gravitum_debuff(hero)) return false;

            bool in_range = hero->get_position().distance(player->get_position()) <= GRAVITUM_Q_RANGE;

            if (settings.debug_mode && in_range) {
                log_debug("Found Gravitum marked target: %s", hero->get_name().c_str());
            }

            return in_range;
            });

        if (!target) {
            if (settings.debug_mode) log_debug("No valid target for Gravitum Q");
            return;
        }

        if (target->get_hp() <= DamageCalculator::get_auto_attack_damage(target, false)) {
            if (settings.debug_mode) log_debug("Target will die to AA, saving Gravitum Q");
            return;
        }

        bool should_cast = false;

        if (target->is_moving()) {
            auto path = target->get_path();
            if (!path.empty()) {
                math::vector3 enemy_direction = path.back();
                if (enemy_direction.distance(player->get_position()) >
                    target->get_position().distance(player->get_position())) {
                    should_cast = true;
                    if (settings.debug_mode) log_debug("Target moving away - casting Gravitum Q");
                }
            }
        }

        if (settings.gravitum_q_antigapcloser && target->is_dashing()) {
            should_cast = true;
            if (settings.debug_mode) log_debug("Target dashing - casting Gravitum Q");
        }

        float dist = target->get_position().distance(player->get_position());
        if (dist > GRAVITUM_Q_RANGE - 100.f) {
            should_cast = true;
            if (settings.debug_mode) log_debug("Target escaping range - casting Gravitum Q");
        }

        for (auto* ally : g_sdk->object_manager->get_heroes()) {
            if (!ally || ally->is_dead() ||
                ally->get_team_id() != player->get_team_id() ||
                ally == player)
                continue;

            float ally_dist = ally->get_position().distance(target->get_position());
            if (ally_dist <= 300.f && ally->get_hp() / ally->get_max_hp() <= 0.3f) {
                should_cast = true;
                if (settings.debug_mode) log_debug("Protecting low HP ally - casting Gravitum Q");
                break;
            }
        }

        if (should_cast) {
            // Check for magneting before weave attack
            if (check_for_magneting()) {
                handle_anti_magneting();
                return;
            }

            weave_attack(target);
            vengaboys::OrderManager::get_instance().cast_spell(0, target);
        }
    }

    void cast_infernum_q() {
        if (!settings.use_infernum_q) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast_spell(0)) return;

        if (settings.debug_mode) log_debug("Attempting to cast Infernum Q");

        struct TargetInfo {
            game_object* target = nullptr;
            int heroes_hit = 0;
            int minions_hit = 0;
            pred_sdk::pred_data prediction{};
            float total_damage = 0.0f;
        };

        std::vector<TargetInfo> potential_targets;

        pred_sdk::spell_data q_data;
        q_data.spell_type = pred_sdk::spell_type::circular;
        q_data.range = INFERNUM_Q_RANGE;
        q_data.radius = 100.f;
        q_data.delay = 0.25f;
        q_data.projectile_speed = 2200.f;
        q_data.source = player;
        q_data.spell_slot = 0;
        q_data.targetting_type = pred_sdk::targetting_type::center_to_edge;

        for (auto* hero : g_sdk->object_manager->get_heroes()) {
            if (!hero || !hero->is_valid() || hero->is_dead() ||
                !hero->is_targetable() || !hero->is_visible() ||
                hero->get_team_id() == player->get_team_id())
                continue;

            auto pred = sdk::prediction->predict(hero, q_data);
            if (!pred.is_valid) {
                if (settings.debug_mode) log_debug("Infernum Q prediction invalid!");
                continue;
            }
            if (pred.hitchance < pred_sdk::hitchance::high)
                continue;

            TargetInfo info;
            info.target = hero;
            info.heroes_hit = 0;
            info.minions_hit = 0;
            info.prediction = pred;
            info.total_damage = 0.f;

            for (auto* other_hero : g_sdk->object_manager->get_heroes()) {
                if (!other_hero || other_hero->is_dead() ||
                    other_hero->get_team_id() == player->get_team_id())
                    continue;

                if (other_hero->get_position().distance(pred.cast_position) <= q_data.radius) {
                    info.heroes_hit++;
                    info.total_damage += DamageCalculator::get_q_damage(WeaponType::INFERNUM, other_hero);
                }
            }

            for (auto* minion : g_sdk->object_manager->get_minions()) {
                if (!minion || minion->is_dead() ||
                    minion->get_team_id() == player->get_team_id())
                    continue;

                if (minion->get_position().distance(pred.cast_position) <= q_data.radius) {
                    info.minions_hit++;
                    info.total_damage += DamageCalculator::get_q_damage(WeaponType::INFERNUM, minion);
                }
            }

            potential_targets.push_back(info);

            if (settings.debug_mode) {
                log_debug("Potential Infernum Q target: %s", hero->get_name().c_str());
                log_debug("- Heroes hit: %d", info.heroes_hit);
                log_debug("- Minions hit: %d", info.minions_hit);
                log_debug("- Total damage: %.1f", info.total_damage);
            }
        }

        TargetInfo* best_target = nullptr;
        float best_score = 0.f;

        for (auto& info : potential_targets) {
            float score = 0.f;

            if (settings.infernum_q_prioritize_heroes) {
                score += info.heroes_hit * 100.f;
                score += info.minions_hit;
            }
            else {
                score += info.heroes_hit * 10.f + info.minions_hit;
            }

            if (info.target->get_hp() / info.target->get_max_hp() < 0.3f) {
                score *= 1.5f;
            }

            score *= (1.f + info.total_damage / 1000.f);

            if (score > best_score) {
                best_score = score;
                best_target = &info;
            }
        }

        if (best_target && best_target->heroes_hit >= settings.infernum_q_min_targets) {
            if (settings.debug_mode) {
                log_debug("Casting Infernum Q:");
                log_debug("- Target: %s", best_target->target->get_name().c_str());
                log_debug("- Total targets: %d", best_target->heroes_hit + best_target->minions_hit);
                log_debug("- Total damage: %.1f", best_target->total_damage);
            }

            // Check for magneting before weave attack
            if (check_for_magneting()) {
                handle_anti_magneting();
                return;
            }

            weave_attack(best_target->target);
            vengaboys::OrderManager::get_instance().cast_spell(0, best_target->prediction.cast_position);
        }
    }

    void cast_crescendum_q() {
        if (!settings.use_crescendum_q) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast_spell(0)) return;

        if (settings.debug_mode) log_debug("Attempting to cast Crescendum Q");

        auto target = sdk::target_selector->get_hero_target();
        if (!target) {
            if (settings.debug_mode) log_debug("No target found for Crescendum Q");
            return;
        }

        math::vector3 optimal_position;
        bool position_found = false;

        if (settings.crescendum_q_turret_safety) {
            math::vector3 player_pos = player->get_position();
            math::vector3 target_pos = target->get_position();

            math::vector3 direction = (target_pos - player_pos).normalized();

            pred_sdk::spell_data pred_data;
            pred_data.spell_type = pred_sdk::spell_type::linear;
            pred_data.range = CRESCENDUM_Q_RANGE;
            pred_data.delay = 0.25f;
            pred_data.projectile_speed = 1500.f;
            auto prediction = sdk::prediction->predict(target, pred_data);

            math::vector3 aim_pos = prediction.is_valid ? prediction.predicted_position : target_pos;

            if (settings.debug_mode) {
                log_debug("Crescendum Q prediction:");
                log_debug("- Target: %s", target->get_name().c_str());
                log_debug("- Prediction valid: %d", prediction.is_valid ? 1 : 0);
            }

            for (float distance = settings.crescendum_q_min_distance;
                distance <= CRESCENDUM_Q_RANGE;
                distance += 50.f) {

                math::vector3 potential_pos = player_pos + (direction * distance);

                if (is_safe_position(potential_pos) &&
                    !is_under_enemy_turret(potential_pos) &&
                    potential_pos.distance(aim_pos) >= settings.crescendum_q_min_distance) {

                    bool blocks_escape = false;
                    for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                        if (!enemy || enemy->is_dead() ||
                            enemy->get_team_id() == player->get_team_id())
                            continue;

                        auto path = enemy->get_path();
                        if (!path.empty()) {
                            math::vector3 escape_direction = path.back();
                            if (potential_pos.distance(escape_direction) <= 300.f) {
                                blocks_escape = true;
                                if (settings.debug_mode) log_debug("Found position blocking escape routes");
                                break;
                            }
                        }
                    }

                    if (blocks_escape) {
                        optimal_position = potential_pos;
                        position_found = true;
                        break;
                    }

                    if (!position_found) {
                        optimal_position = potential_pos;
                        position_found = true;
                    }
                }
            }
        }
        else {
            optimal_position = player->get_position().extended(target->get_position(), CRESCENDUM_Q_RANGE);
            position_found = true;
        }

        if (position_found) {
            if (settings.debug_mode) {
                log_debug("Casting Crescendum Q:");
                log_debug("- Position: %.1f, %.1f, %.1f",
                    optimal_position.x, optimal_position.y, optimal_position.z);
            }

            // Check for magneting before weave attack
            if (check_for_magneting()) {
                handle_anti_magneting();
                return;
            }

            weave_attack(target);
            vengaboys::OrderManager::get_instance().cast_spell(0, optimal_position);
        }
    }

    void cast_r() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || (!settings.auto_r && !settings.manual_r_key_pressed)) return;

        if (!can_cast_spell(3)) return;

        if (settings.debug_mode) log_debug("Attempting to cast ultimate");

        pred_sdk::spell_data r_data;
        r_data.spell_type = pred_sdk::spell_type::linear;
        r_data.range = settings.r_range;
        r_data.radius = R_WIDTH;
        r_data.delay = 0.4f;
        r_data.projectile_speed = 2000.f;
        r_data.source = player;
        r_data.spell_slot = 3;
        r_data.targetting_type = pred_sdk::targetting_type::center_to_edge;
        r_data.forbidden_collisions = {
            pred_sdk::collision_type::yasuo_wall,
            pred_sdk::collision_type::braum_wall,
        };

        auto target = sdk::target_selector->get_hero_target([&](game_object* hero) {
            if (!hero) return false;

            if (!hero->is_valid() || hero->is_dead() ||
                !hero->is_targetable() || !hero->is_visible())
                return false;

            if (settings.r_save_for_combo &&
                hero->get_hp() / hero->get_max_hp() > settings.r_min_hp_ratio) {
                return false;
            }

            int potential_targets = 0;
            float total_damage = 0.f;

            for (auto* nearby : g_sdk->object_manager->get_heroes()) {
                if (!nearby || nearby->is_dead() ||
                    nearby->get_team_id() == player->get_team_id())
                    continue;

                if (nearby->get_position().distance(hero->get_position()) <= R_WIDTH) {
                    potential_targets++;
                    auto weapon_state = WeaponManager::get_current_state();
                    total_damage += DamageCalculator::get_r_damage(weapon_state.main_hand.type, nearby);
                }
            }

            if (settings.debug_mode && (potential_targets >= settings.r_min_enemies ||
                total_damage >= hero->get_max_hp() * 0.5f)) {
                log_debug("Found potential R target: %s", hero->get_name().c_str());
                log_debug("- Potential targets: %d", potential_targets);
                log_debug("- Total damage: %.1f", total_damage);
            }

            return potential_targets >= settings.r_min_enemies ||
                total_damage >= hero->get_max_hp() * 0.5f;
            });

        if (!target) {
            if (settings.debug_mode) log_debug("No suitable target for R");
            return;
        }

        auto pred = sdk::prediction->predict(target, r_data);
        if (!pred.is_valid) {
            if (settings.debug_mode) log_debug("R prediction invalid!");
            return;
        }
        if (pred.hitchance < pred_sdk::hitchance::high) {
            if (settings.debug_mode) log_debug("Low hitchance for R: %.1f", pred.hitchance);
            return;
        }

        int enemies_hit = 0;
        float total_damage = 0.f;
        for (auto* enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy || enemy->is_dead() ||
                !enemy->is_targetable() ||
                enemy->get_team_id() == player->get_team_id())
                continue;

            if (enemy->get_position().distance(pred.cast_position) <= r_data.radius) {
                enemies_hit++;
                auto weapon_state = WeaponManager::get_current_state();
                total_damage += DamageCalculator::get_r_damage(weapon_state.main_hand.type, enemy);
            }
        }

        bool should_cast = settings.manual_r_key_pressed ||
            enemies_hit >= settings.r_min_enemies ||
            total_damage >= target->get_max_hp() * 0.5f;

        if (!settings.manual_r_key_pressed && should_cast) {
            bool target_dying = false;
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead()) continue;

                if (enemy->get_team_id() == player->get_team_id() &&
                    enemy->get_position().distance(target->get_position()) <= 500.f) {
                    float potential_damage = DamageCalculator::calculate_dps(WeaponType::NONE, target);
                    if (potential_damage >= target->get_hp()) {
                        target_dying = true;
                        if (settings.debug_mode) log_debug("Target already dying, saving R");
                        break;
                    }
                }
            }

            if (target_dying) return;
        }

        if (should_cast) {
            if (settings.debug_mode) {
                log_debug("Casting R:");
                log_debug("- Target: %s", target->get_name().c_str());
                log_debug("- Enemies hit: %d", enemies_hit);
                log_debug("- Total damage: %.1f", total_damage);
            }

            // Check for magneting before weave attack
            if (check_for_magneting()) {
                handle_anti_magneting();
                return;
            }

            weave_attack(target);
            vengaboys::OrderManager::get_instance().cast_spell(3, pred.cast_position);
        }
    }

    void cast_w() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (!can_cast_spell(1)) return;

        if (settings.debug_mode) log_debug("Attempting weapon swap");

        if (settings.manual_switch_key_pressed) {
            if (settings.debug_mode) log_debug("Manual weapon swap triggered");
            bool cast_result = vengaboys::OrderManager::get_instance().cast_spell(1, player);

            if (cast_result) {
                // Force immediate hash checking after weapon swap
                //force_hash_extraction = true;
                g_sdk->log_console("[Aphelios] Manual weapon switch completed");
            }

            if (settings.instant_q_after_switch) {
                auto state = WeaponManager::get_current_state();
                if (settings.debug_mode) log_debug("Instant Q after swap with weapon: %s",
                    get_weapon_name(state.off_hand.type).c_str());

                switch (state.off_hand.type) {
                case WeaponType::CALIBRUM: cast_calibrum_q(); break;
                case WeaponType::SEVERUM: cast_severum_q(); break;
                case WeaponType::GRAVITUM: cast_gravitum_q(); break;
                case WeaponType::INFERNUM: cast_infernum_q(); break;
                case WeaponType::CRESCENDUM: cast_crescendum_q(); break;
                default: break;
                }
            }
            return;
        }

        if (settings.auto_weapon_switch) {
            auto state = WeaponManager::get_current_state();

            if (WeaponManager::should_switch_weapons(state)) {
                WeaponType desired = WeaponManager::get_best_weapon_for_situation();

                if (desired != state.main_hand.type &&
                    desired == state.off_hand.type) {

                    if (settings.debug_mode) {
                        log_debug("Auto weapon swap:");
                        log_debug("- Current: %s", get_weapon_name(state.main_hand.type).c_str());
                        log_debug("- Desired: %s", get_weapon_name(desired).c_str());
                    }

                    bool cast_result = vengaboys::OrderManager::get_instance().cast_spell(1, player);

                    if (cast_result) {
                        // Force immediate hash checking after weapon swap
                        //force_hash_extraction = true;
                        g_sdk->log_console("[Aphelios] Auto weapon switch completed");
                    }

                    if (settings.instant_q_after_switch) {
                        if (settings.debug_mode) log_debug("Casting instant Q after auto swap");
                        switch (desired) {
                        case WeaponType::CALIBRUM: cast_calibrum_q(); break;
                        case WeaponType::SEVERUM: cast_severum_q(); break;
                        case WeaponType::GRAVITUM: cast_gravitum_q(); break;
                        case WeaponType::INFERNUM: cast_infernum_q(); break;
                        case WeaponType::CRESCENDUM: cast_crescendum_q(); break;
                        default: break;
                        }
                    }
                }
            }
        }
    }

    void handle_combo() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.debug_mode) log_debug("Handling combo mode");

        auto state = WeaponManager::get_current_state();
        if (settings.debug_mode) log_debug("Main hand weapon: %s", get_weapon_name(state.main_hand.type).c_str());

        auto q_spell = player->get_spell(0);
        if (q_spell && settings.debug_mode) {
            log_debug("Q cooldown: %.1f", q_spell->get_cooldown());
        }

        // Check for magneting and handle it if needed
        if (check_for_magneting()) {
            handle_anti_magneting();
            if (settings.debug_mode) log_debug("Anti-magneting handling applied in combo");
        }

        if (handle_anti_gapcloser()) {
            if (settings.debug_mode) log_debug("Anti-gapcloser handled in combo");
            return;
        }

        if (settings.auto_weapon_switch) {
            if (settings.debug_mode) log_debug("Checking weapon swap in combo");
            cast_w();
        }

        if (settings.auto_r || settings.manual_r_key_pressed) {
            if (settings.debug_mode) log_debug("Checking ultimate conditions in combo");
            cast_r();
        }

        if (settings.auto_q) {
            if (settings.debug_mode) log_debug("Attempting to cast Q with weapon: %s",
                get_weapon_name(state.main_hand.type).c_str());

            switch (state.main_hand.type) {
            case WeaponType::CALIBRUM:
                if (settings.use_calibrum_q) {
                    if (settings.debug_mode) log_debug("Trying Calibrum Q");
                    cast_calibrum_q();
                }
                break;
            case WeaponType::SEVERUM:
                if (settings.use_severum_q) {
                    if (settings.debug_mode) log_debug("Trying Severum Q");
                    cast_severum_q();
                }
                break;
            case WeaponType::GRAVITUM:
                if (settings.use_gravitum_q) {
                    if (settings.debug_mode) log_debug("Trying Gravitum Q");
                    cast_gravitum_q();
                }
                break;
            case WeaponType::INFERNUM:
                if (settings.use_infernum_q) {
                    if (settings.debug_mode) log_debug("Trying Infernum Q");
                    cast_infernum_q();
                }
                break;
            case WeaponType::CRESCENDUM:
                if (settings.use_crescendum_q) {
                    if (settings.debug_mode) log_debug("Trying Crescendum Q");
                    cast_crescendum_q();
                }
                break;
            default:
                break;
            }
        }
    }

    void handle_harass() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.debug_mode) log_debug("Handling harass mode");

        // Check for magneting and handle it if needed
        if (check_for_magneting()) {
            handle_anti_magneting();
            if (settings.debug_mode) log_debug("Anti-magneting handling applied in harass");
            return;
        }

        auto state = WeaponManager::get_current_state();

        if (state.off_hand.type == WeaponType::CALIBRUM && can_cast_spell(1)) {
            if (settings.debug_mode) log_debug("Switching to Calibrum for harass");
            cast_w();
        }

        if (settings.auto_q) {
            switch (state.main_hand.type) {
            case WeaponType::CALIBRUM:
                if (settings.debug_mode) log_debug("Casting Calibrum Q in harass");
                cast_calibrum_q();
                break;

            case WeaponType::INFERNUM:
                if (settings.infernum_q_min_targets <= 2) {
                    if (settings.debug_mode) log_debug("Casting Infernum Q in harass");
                    cast_infernum_q();
                }
                break;

            case WeaponType::GRAVITUM: {
                auto target = sdk::target_selector->get_hero_target([&](game_object* hero) {
                    if (!hero || !hero->is_valid() || hero->is_dead()) return false;

                    if (!has_gravitum_debuff(hero)) return false;

                    bool is_priority = hero->get_hp() / hero->get_max_hp() <= 0.4f ||
                        hero->get_position().distance(player->get_position()) <= 400.f;

                    if (settings.debug_mode && is_priority) {
                        log_debug("Found priority Gravitum target in harass: %s", hero->get_name().c_str());
                    }

                    return is_priority;
                    });

                if (target) {
                    if (settings.debug_mode) log_debug("Casting Gravitum Q on priority target in harass");
                    cast_gravitum_q();
                }
                break;
            }
            default:
                break;
            }
        }
    }

    void handle_farming() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.debug_mode) log_debug("Handling farming mode");

        // Check for magneting and handle it if needed
        if (check_for_magneting()) {
            handle_anti_magneting();
            if (settings.debug_mode) log_debug("Anti-magneting handling applied in farming");
            return;
        }

        auto state = WeaponManager::get_current_state();

        bool epic_monster_nearby = false;
        game_object* epic_monster = nullptr;
        for (auto* monster : g_sdk->object_manager->get_monsters()) {
            if (!monster || monster->is_dead()) continue;

            if (monster->is_epic_monster() &&
                monster->get_position().distance(player->get_position()) <= 1000.f) {
                epic_monster_nearby = true;
                epic_monster = monster;
                if (settings.debug_mode) log_debug("Epic monster detected: %s", monster->get_name().c_str());
                break;
            }
        }

        if (settings.auto_weapon_switch) {
            if (epic_monster_nearby) {
                if (state.off_hand.type == WeaponType::CRESCENDUM) {
                    if (settings.debug_mode) log_debug("Switching to Crescendum for epic monster");
                    cast_w();
                }
            }
            else {
                int nearby_minions = 0;
                for (auto* minion : g_sdk->object_manager->get_minions()) {
                    if (!minion || minion->is_dead() ||
                        minion->get_team_id() == player->get_team_id())
                        continue;

                    if (minion->get_position().distance(player->get_position()) <= 600.f) {
                        nearby_minions++;
                    }
                }

                if (settings.debug_mode) log_debug("Nearby minions count: %d", nearby_minions);

                if (nearby_minions >= settings.min_minions_for_aoe &&
                    state.off_hand.type == WeaponType::INFERNUM) {
                    if (settings.debug_mode) log_debug("Switching to Infernum for waveclear");
                    cast_w();
                }
                else if (nearby_minions < settings.min_minions_for_aoe &&
                    state.off_hand.type == WeaponType::CRESCENDUM) {
                    if (settings.debug_mode) log_debug("Switching to Crescendum for single target farm");
                    cast_w();
                }
            }
        }

        if (settings.auto_q) {
            switch (state.main_hand.type) {
            case WeaponType::INFERNUM: {
                if (settings.debug_mode) log_debug("Checking Infernum Q farm opportunity");
                cast_infernum_q();
                break;
            }
            case WeaponType::CRESCENDUM: {
                if (epic_monster_nearby) {
                    if (settings.debug_mode) log_debug("Using Crescendum Q on epic monster");
                    cast_crescendum_q();
                }
                break;
            }
            case WeaponType::CALIBRUM: {
                if (!settings.farm_with_abilities) break;

                auto target = sdk::target_selector->get_monster_target([&](game_object* obj) {
                    if (!obj || !obj->is_monster() || obj->is_dead()) return false;

                    if (settings.prioritize_cannon_minions && obj->is_lane_minion_siege()) {
                        return false;
                    }

                    float predicted_hp = sdk::health_prediction->get_predicted_health(obj, 0.25f);
                    bool can_execute = predicted_hp > 0 &&
                        predicted_hp <= DamageCalculator::get_q_damage(WeaponType::CALIBRUM, obj);

                    if (settings.debug_mode && can_execute) {
                        log_debug("Found Calibrum Q execute target - HP: %.1f", predicted_hp);
                    }

                    return can_execute;
                    });

                if (target) {
                    if (settings.debug_mode) log_debug("Executing monster with Calibrum Q");
                    cast_calibrum_q();
                }
                break;
            }
            default:
                break;
            }
        }
    }

    void handle_flee() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.debug_mode) log_debug("Handling flee mode");

        // Check for magneting and handle it if needed
        if (check_for_magneting()) {
            handle_anti_magneting();
            if (settings.debug_mode) log_debug("Anti-magneting handling applied in flee");
            return;
        }

        auto state = WeaponManager::get_current_state();

        if (settings.auto_weapon_switch && state.off_hand.type == WeaponType::SEVERUM) {
            if (settings.debug_mode) log_debug("Switching to Severum for flee");
            cast_w();
        }

        if (state.main_hand.type == WeaponType::SEVERUM) {
            auto nearest_enemy = sdk::target_selector->get_hero_target([&](game_object* hero) {
                if (!hero || !hero->is_valid() || hero->is_dead()) return false;
                return hero->get_position().distance(player->get_position()) <= SEVERUM_Q_RANGE;
                });

            if (nearest_enemy) {
                if (settings.debug_mode) log_debug("Using Severum Q for movement speed");
                cast_severum_q();
            }
        }

        if (state.main_hand.type == WeaponType::GRAVITUM) {
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                if (has_gravitum_debuff(enemy)) {
                    if (settings.debug_mode) log_debug("Using Gravitum Q to slow pursuer: %s",
                        enemy->get_name().c_str());
                    cast_gravitum_q();
                    break;
                }
            }
        }

        auto mouse_pos = g_sdk->hud_manager->get_cursor_position();
        auto path = player->calculate_path(mouse_pos);

        if (settings.debug_mode) log_debug("Calculating escape path");

        for (const auto& path_point : path) {
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                if (enemy->get_position().distance(path_point) <= 500.f) {
                    if (settings.debug_mode) log_debug("Enemy %s near escape path",
                        enemy->get_name().c_str());

                    math::vector3 perpendicular = (path_point - player->get_position()).perpendicular_right();
                    auto alt_path = player->calculate_path(path_point + perpendicular);

                    if (!alt_path.empty() && is_safe_position(alt_path.back())) {
                        if (settings.debug_mode) log_debug("Found alternative escape path");
                        vengaboys::OrderManager::get_instance().move_to(alt_path.back(), vengaboys::OrderPriority::High);
                        return;
                    }
                }
            }
        }
    }

    bool handle_anti_gapcloser() {
        if (!settings.anti_gapcloser) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (settings.debug_mode) log_debug("Checking anti-gapcloser");

        auto state = WeaponManager::get_current_state();

        for (auto* enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy || enemy->is_dead() ||
                enemy->get_team_id() == player->get_team_id())
                continue;

            if (enemy->is_dashing()) {
                float dash_end_dist = enemy->get_position().distance(player->get_position());

                if (dash_end_dist <= 400.f) {
                    if (settings.debug_mode) log_debug("Detecting dash from %s", enemy->get_name().c_str());

                    if (state.main_hand.type == WeaponType::GRAVITUM) {
                        if (can_cast_spell(0) && has_gravitum_debuff(enemy)) {
                            if (settings.debug_mode) log_debug("Using Gravitum Q anti-gapcloser");
                            vengaboys::OrderManager::get_instance().cast_spell(0, enemy);
                            return true;
                        }
                    }
                    else if (state.main_hand.type == WeaponType::SEVERUM) {
                        if (can_cast_spell(0)) {
                            if (settings.debug_mode) log_debug("Using Severum Q anti-gapcloser");
                            vengaboys::OrderManager::get_instance().cast_spell(0, enemy);
                            return true;
                        }
                    }

                    if (state.off_hand.type == WeaponType::GRAVITUM && can_cast_spell(1)) {
                        if (settings.debug_mode) log_debug("Swapping to Gravitum for anti-gapcloser");
                        cast_w();
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool is_safe_position(const math::vector3& pos) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (settings.tower_dive_prevention && is_under_enemy_turret(pos)) {
            if (settings.debug_mode) log_debug("Position unsafe: under enemy turret");
            return false;
        }

        int nearby_enemies = 0;
        float total_enemy_threat = 0.f;
        float total_ally_strength = 0.f;

        for (auto* hero : g_sdk->object_manager->get_heroes()) {
            if (!hero || hero->is_dead() || !hero->is_visible()) continue;

            float dist = pos.distance(hero->get_position());
            if (dist > 900.f) continue;

            if (hero->get_team_id() != player->get_team_id()) {
                nearby_enemies++;
                total_enemy_threat += hero->get_hp() * (1.f + hero->get_armor() * 0.01f);

                if (hero->has_buff_of_type(buff_type::stun) ||
                    hero->has_buff_of_type(buff_type::snare) ||
                    hero->has_buff_of_type(buff_type::suppression)) {
                    total_enemy_threat *= 0.5f;
                }
            }
            else {
                total_ally_strength += hero->get_hp() * (1.f + hero->get_armor() * 0.01f);
            }
        }

        bool is_safe = nearby_enemies < 3 || total_enemy_threat <= total_ally_strength * 1.5f;

        if (settings.debug_mode && !is_safe) {
            log_debug("Position unsafe: %d enemies nearby, threat ratio: %.2f",
                nearby_enemies, total_enemy_threat / (total_ally_strength > 0 ? total_ally_strength : 1));
        }

        return is_safe;
    }

    bool is_under_enemy_turret(const math::vector3& pos) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return true;

        for (auto* turret : g_sdk->object_manager->get_turrets()) {
            if (!turret || turret->is_dead() ||
                turret->get_team_id() == player->get_team_id())
                continue;

            if (pos.distance(turret->get_position()) <= 900.f) {
                if (settings.debug_mode) log_debug("Position is under enemy turret");
                return true;
            }
        }
        return false;
    }

    void WeaponManager::update_weapon_queue(WeaponState& state) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        // Handle different weapon queue modes
        if (settings.use_standard_weapon_queue) {
            // Use the predefined standard queue from settings
            state.queue[0] = settings.standard_queue[0];
            state.queue[1] = settings.standard_queue[1];
            state.queue[2] = settings.standard_queue[2];

            if (settings.debug_mode) {
                log_debug("Using standard weapon queue: %s > %s > %s",
                    get_weapon_name(state.queue[0]).c_str(),
                    get_weapon_name(state.queue[1]).c_str(),
                    get_weapon_name(state.queue[2]).c_str());
            }
        }
        else {
            // Calculate adaptive queue based on available weapons
            std::vector<WeaponType> all_weapons = {
                WeaponType::CALIBRUM,
                WeaponType::SEVERUM,
                WeaponType::GRAVITUM,
                WeaponType::INFERNUM,
                WeaponType::CRESCENDUM
            };

            // Remove current main and offhand weapons from the available weapons
            all_weapons.erase(
                std::remove_if(all_weapons.begin(), all_weapons.end(),
                    [&](WeaponType w) {
                        return w == state.main_hand.type || w == state.off_hand.type;
                    }),
                all_weapons.end()
            );

            // Fill the queue with remaining weapons
            state.queue.fill(WeaponType::NONE);
            for (size_t i = 0; i < all_weapons.size() && i < state.queue.size(); i++) {
                state.queue[i] = all_weapons[i];
            }

            if (settings.debug_mode) {
                log_debug("Using adaptive weapon queue");
            }
        }

        if (settings.debug_mode) {
            log_debug("Main hand: %s", get_weapon_name(state.main_hand.type).c_str());
            log_debug("Off hand: %s", get_weapon_name(state.off_hand.type).c_str());
            std::string queue_str = "Queue: ";
            for (auto w : state.queue) {
                if (w != WeaponType::NONE) {
                    queue_str += get_weapon_name(w) + " > ";
                }
            }
            log_debug("%s", queue_str.c_str());
        }
    }

    float WeaponManager::get_ammo_drain_multiplier(WeaponType weapon) {
        switch (weapon) {
        case WeaponType::SEVERUM:
            return 0.85f; // Drains faster due to healing
        case WeaponType::CRESCENDUM:
            return 0.9f; // Drains slightly faster with stacks
        case WeaponType::INFERNUM:
            return 1.1f; // Drains slower due to AoE
        case WeaponType::GRAVITUM:
            return 1.15f; // Drains slower due to utility
        case WeaponType::CALIBRUM:
            return 1.05f; // Standard drain rate
        default:
            return 1.0f;
        }
    }

    void WeaponManager::handle_weapon_stacks(WeaponData& weapon) {
        if (weapon.type != WeaponType::CRESCENDUM) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        // Get Crescendum buff to update stacks
        auto crescendum_buff = player->get_buff_by_hash(buff_hashes::CRESCENDUM_MANAGER);
        if (crescendum_buff && crescendum_buff->is_active()) {
            weapon.stacks = crescendum_buff->get_stacks();
        }
    }

    bool WeaponManager::should_switch_weapons(const WeaponState& state) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (settings.debug_mode) log_debug("Checking if weapon switch is needed");

        // Don't switch if we just switched recently (avoid double-swapping)
        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - state.last_weapon_switch < 0.5f) {
            if (settings.debug_mode) log_debug("Recent swap, waiting");
            return false;
        }

        // Low ammo check
        if (state.main_hand.ammo < 5.f) {
            if (settings.debug_mode) log_debug("Low ammo (%.1f), considering swap", state.main_hand.ammo);
            return true;
        }

        // Combat situation check - is the current weapon situation optimal?
        WeaponType desired = get_best_weapon_for_situation();
        if (desired != state.main_hand.type && desired == state.off_hand.type) {
            if (settings.debug_mode) {
                log_debug("Suboptimal weapon setup. Current: %s, Desired: %s",
                    get_weapon_name(state.main_hand.type).c_str(),
                    get_weapon_name(desired).c_str());
            }
            return true;
        }

        return false;
    }

    float WeaponManager::evaluate_weapon_combination(WeaponType main, WeaponType off) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        // Base scores for each weapon type
        float score = 0.f;

        // Evaluate main weapon
        switch (main) {
        case WeaponType::CALIBRUM: {
            score += 20.f; // Good poke

            // Better score at long range
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                float dist = enemy->get_position().distance(player->get_position());
                if (dist > 650.f && dist <= CALIBRUM_Q_RANGE) {
                    score += 10.f;
                    break;
                }
            }
            break;
        }

        case WeaponType::SEVERUM: {
            score += 15.f; // Decent sustain

            // Better score at low health
            if (player->get_hp() / player->get_max_hp() < 0.4f) {
                score += 15.f;
            }

            // Better against melee threats
            int nearby_enemies_severum = 0;
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                if (enemy->get_position().distance(player->get_position()) <= 350.f) {
                    nearby_enemies_severum++;
                }
            }

            if (nearby_enemies_severum > 0) {
                score += nearby_enemies_severum * 5.f;
            }
            break;
        }

        case WeaponType::GRAVITUM: {
            score += 18.f; // Good utility

            // Better score with enemies that need kiting
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                if (has_gravitum_debuff(enemy)) {
                    score += 12.f;
                    break;
                }

                // Good against high mobility targets
                if (enemy->is_melee() &&
                    enemy->get_position().distance(player->get_position()) <= 600.f) {
                    score += 8.f;
                }
            }
            break;
        }

        case WeaponType::INFERNUM: {
            score += 22.f; // Great waveclear/AoE

            // Better score with grouped enemies
            int grouped_enemies_infernum = 0;
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                if (enemy->get_position().distance(player->get_position()) <= 500.f) {
                    grouped_enemies_infernum++;
                }
            }

            if (grouped_enemies_infernum >= 2) {
                score += grouped_enemies_infernum * 7.f;
            }
            break;
        }

        case WeaponType::CRESCENDUM: {
            score += 25.f; // Best single target damage

            // Check if we have stacks
            auto crescendum_state = get_current_state();
            if (crescendum_state.main_hand.stacks >= 3) {
                score += crescendum_state.main_hand.stacks * 3.f;
            }

            // Better score with fewer enemies (single target situations)
            int nearby_enemies_crescendum = 0;
            for (auto* enemy : g_sdk->object_manager->get_heroes()) {
                if (!enemy || enemy->is_dead() ||
                    enemy->get_team_id() == player->get_team_id())
                    continue;

                if (enemy->get_position().distance(player->get_position()) <= 600.f) {
                    nearby_enemies_crescendum++;
                }
            }

            if (nearby_enemies_crescendum <= 1) {
                score += 10.f;
            }
            else {
                score -= (nearby_enemies_crescendum - 1) * 5.f;
            }
            break;
        }

        default:
            break;
        }

        // Evaluate off-hand weapon (synergy bonus)
        if (is_good_combo(main, off)) {
            score += 15.f;
        }

        return score;
    }

    bool WeaponManager::is_good_combo(WeaponType main, WeaponType off) {
        // Classic good combos
        if (main == WeaponType::CALIBRUM && off == WeaponType::SEVERUM) return true;
        if (main == WeaponType::CALIBRUM && off == WeaponType::GRAVITUM) return true;
        if (main == WeaponType::SEVERUM && off == WeaponType::CRESCENDUM) return true;
        if (main == WeaponType::INFERNUM && off == WeaponType::GRAVITUM) return true;
        if (main == WeaponType::INFERNUM && off == WeaponType::CRESCENDUM) return true;
        if (main == WeaponType::CRESCENDUM && off == WeaponType::CALIBRUM) return true;

        return false;
    }

    WeaponType WeaponManager::get_best_weapon_for_situation() {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return WeaponType::NONE;

        auto state = get_current_state();

        if (settings.debug_mode) {
            log_debug("Getting best weapon for current situation");
            log_debug("Main: %s, Off: %s",
                get_weapon_name(state.main_hand.type).c_str(),
                get_weapon_name(state.off_hand.type).c_str());
        }

        // Special situations override

        // Low HP, switch to Severum if available
        if (player->get_hp() / player->get_max_hp() < 0.3f) {
            if (state.off_hand.type == WeaponType::SEVERUM) {
                if (settings.debug_mode) log_debug("Low HP, preferring Severum");
                return WeaponType::SEVERUM;
            }
        }

        // Anti-dive situation, switch to Gravitum if available
        int nearby_melee_enemies = 0;
        for (auto* enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy || enemy->is_dead() ||
                enemy->get_team_id() == player->get_team_id())
                continue;

            if (enemy->is_melee() &&
                enemy->get_position().distance(player->get_position()) <= 400.f) {
                nearby_melee_enemies++;
            }
        }

        if (nearby_melee_enemies >= 1 && state.off_hand.type == WeaponType::GRAVITUM) {
            if (settings.debug_mode) log_debug("Anti-dive situation, preferring Gravitum");
            return WeaponType::GRAVITUM;
        }

        // Teamfight situation with multiple enemies, switch to Infernum if available
        int grouped_enemies = 0;
        for (auto* enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy || enemy->is_dead() ||
                enemy->get_team_id() == player->get_team_id())
                continue;

            if (enemy->get_position().distance(player->get_position()) <= 500.f) {
                grouped_enemies++;
            }
        }

        if (grouped_enemies >= 2 && state.off_hand.type == WeaponType::INFERNUM) {
            if (settings.debug_mode) log_debug("Multiple grouped enemies, preferring Infernum");
            return WeaponType::INFERNUM;
        }

        // If no special situation, evaluate based on scores
        float main_score = evaluate_weapon_combination(state.main_hand.type, state.off_hand.type);
        float off_score = evaluate_weapon_combination(state.off_hand.type, state.main_hand.type);

        if (settings.debug_mode) {
            log_debug("Weapon scores - Main: %.1f, Off: %.1f", main_score, off_score);
        }

        // Prefer current weapon unless offhand is significantly better
        if (off_score > main_score * 1.2f) {
            return state.off_hand.type;
        }

        return state.main_hand.type;
    }

    bool DamageCalculator::can_kill_with_combo(game_object* target) {
        if (!target) return false;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        auto state = WeaponManager::get_current_state();

        // Calculate total potential damage
        float total_damage = 0.f;

        // Auto attack damage
        total_damage += get_auto_attack_damage(target, true);

        // Q damage if available
        if (can_cast_spell(0)) {
            total_damage += get_q_damage(state.main_hand.type, target);
        }

        // R damage if available
        if (can_cast_spell(3)) {
            total_damage += get_r_damage(state.main_hand.type, target);
        }

        // Weapon swap and follow-up damage if possible
        if (can_cast_spell(1)) {
            total_damage += get_auto_attack_damage(target, false);
            total_damage += get_q_damage(state.off_hand.type, target);
        }

        if (settings.debug_mode) {
            log_debug("Kill combo calculation:");
            log_debug("- Target HP: %.1f", target->get_hp());
            log_debug("- Total potential damage: %.1f", total_damage);
        }

        return total_damage >= target->get_hp();
    }

    float DamageCalculator::predict_incoming_damage(game_object* target, float time_window) {
        if (!target) return 0.f;

        // Get target key for cache lookup
        uint32_t target_key = target->get_network_id();
        float current_time = g_sdk->clock_facade->get_game_time();

        // Check if we have a recent calculation
        auto it = damage_cache.find(target_key);
        if (it != damage_cache.end()) {
            return it->second;
        }

        // Calculate incoming damage
        float incoming_damage = 0.f;
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        for (auto* hero : g_sdk->object_manager->get_heroes()) {
            if (!hero || hero->is_dead() ||
                hero->get_team_id() != player->get_team_id())
                continue;

            // Only consider heroes that can reach the target
            float dist = hero->get_position().distance(target->get_position());
            if (dist > 1000.f) continue;

            // Estimate basic attack damage
            float attack_speed = hero->get_attack_speed_mod();
            float attacks_in_window = attack_speed * time_window;
            float aa_damage = sdk::damage->get_aa_damage(hero, target);

            incoming_damage += aa_damage * attacks_in_window;

            // Add estimated spell damage
            if (hero->get_spell(0) && hero->get_spell(0)->get_cooldown() < time_window) {
                incoming_damage += 0.7f * hero->get_attack_damage();
            }
        }

        // Cache the result
        damage_cache[target_key] = incoming_damage;

        return incoming_damage;
    }

    void __fastcall on_wndproc(uint32_t msg, uint32_t wparam, uint32_t lparam) {
        if (!initialized) return;

        // Extract x and y coordinates without Windows API
        int x = static_cast<int>(lparam & 0xFFFF);
        int y = static_cast<int>((lparam >> 16) & 0xFFFF);
        math::vector2 mouse_pos(static_cast<float>(x), static_cast<float>(y));

        if (settings.draw_weapon_queue) {
            if (msg == 0x0201) { // WM_LBUTTONDOWN
                if (mouse_pos.x >= settings.weapon_queue_drag_area.left &&
                    mouse_pos.x <= settings.weapon_queue_drag_area.right &&
                    mouse_pos.y >= settings.weapon_queue_drag_area.top &&
                    mouse_pos.y <= settings.weapon_queue_drag_area.bottom) {
                    settings.is_dragging_weapon_queue = true;
                    if (settings.debug_mode) log_debug("Started dragging weapon queue");
                }
            }
            else if (msg == 0x0202) { // WM_LBUTTONUP
                settings.is_dragging_weapon_queue = false;
            }
            else if (msg == 0x0200 && settings.is_dragging_weapon_queue) { // WM_MOUSEMOVE
                settings.weapon_queue_position = mouse_pos;
            }

            if (settings.draw_weapon_info) {
                if (msg == 0x0201) { // WM_LBUTTONDOWN
                    if (mouse_pos.x >= settings.weapon_info_drag_area.left &&
                        mouse_pos.x <= settings.weapon_info_drag_area.right &&
                        mouse_pos.y >= settings.weapon_info_drag_area.top &&
                        mouse_pos.y <= settings.weapon_info_drag_area.bottom) {
                        settings.is_dragging_weapon_info = true;
                        if (settings.debug_mode) log_debug("Started dragging weapon info");
                    }
                }
                else if (msg == 0x0202) { // WM_LBUTTONUP
                    settings.is_dragging_weapon_info = false;
                }
                else if (msg == 0x0200 && settings.is_dragging_weapon_info) { // WM_MOUSEMOVE
                    settings.weapon_info_position = mouse_pos;
                }
            }
        }
    }

    void __fastcall on_update() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        // Check for magneting and handle it if needed
        check_for_magneting();

        if (settings.smart_weapon_rotation && !settings.use_standard_weapon_queue) {
            if (settings.debug_mode) log_debug("Starting weapon rotation update");
            optimize_weapon_rotation();
        }

        if (settings.anti_gapcloser && handle_anti_gapcloser()) {
            if (settings.debug_mode) log_debug("Anti-gapcloser handled, skipping other actions");
            return;
        }

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - last_damage_cache_clear > DAMAGE_CACHE_DURATION) {
            if (settings.debug_mode) log_debug("Clearing damage cache");
            damage_cache.clear();
            last_damage_cache_clear = current_time;
        }

        if (sdk::orbwalker->combo()) {
            if (settings.debug_mode) log_debug("Handling combo mode");
            handle_combo();
        }
        else if (sdk::orbwalker->harass()) {
            if (settings.debug_mode) log_debug("Handling harass mode");
            handle_harass();
        }
        else if (sdk::orbwalker->clear() || sdk::orbwalker->lasthit()) {
            if (settings.debug_mode) log_debug("Handling farming mode");
            handle_farming();
        }
        else if (sdk::orbwalker->flee()) {
            if (settings.debug_mode) log_debug("Handling flee mode");
            handle_flee();
        }

        settings.manual_switch_key_pressed = false;
        settings.manual_r_key_pressed = false;

        update_infotab();
    }

    void __fastcall on_draw() {
        if (!initialized) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player || player->is_dead()) return;

        if (settings.debug_mode) log_debug("Starting draw calls");

        draw_ranges();
        draw_weapon_info();
        draw_damage_indicators();
        draw_ammo_bars();
        draw_weapon_queue();

        if (settings.debug_mode) log_debug("Draw calls completed");
    }

    void draw_weapon_info() {
        if (!settings.draw_weapon_info) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto state = WeaponManager::get_current_state();

        int window_width = g_sdk->renderer->get_window_width();
        int window_height = g_sdk->renderer->get_window_height();

        math::vector2 pos;

        // If position not set, initialize in a good location
        if (settings.weapon_info_position.x == 0.f && settings.weapon_info_position.y == 0.f) {
            pos = { window_width * 0.01f, window_height * 0.3f };
            settings.weapon_info_position = pos;
        }
        else {
            pos = settings.weapon_info_position;
        }

        // Background
        math::rect bg_rect(pos.y, pos.x, pos.y + 120.f, pos.x + 250.f);
        g_sdk->renderer->add_rectangle_filled(bg_rect, 0x80000000);
        g_sdk->renderer->add_rectangle(bg_rect, 0xFFFFFFFF, 1.f);

        // Update drag area
        settings.weapon_info_drag_area = bg_rect;

        // Title
        math::vector2 title_pos = { pos.x + 5.f, pos.y + 5.f };
        g_sdk->renderer->add_text("Aphelios Weapons", 16.f, title_pos, 0, 0xFFFFFFFF);

        // Main and offhand info
        math::vector2 main_pos = { pos.x + 5.f, pos.y + 30.f };
        std::string main_text = "Main: " + get_weapon_name(state.main_hand.type);
        main_text += " (" + std::to_string(static_cast<int>(state.main_hand.ammo)) + ")";
        g_sdk->renderer->add_text(main_text, 14.f, main_pos, 0, 0xFFFFFFFF);

        math::vector2 off_pos = { pos.x + 5.f, pos.y + 50.f };
        std::string off_text = "Off: " + get_weapon_name(state.off_hand.type);
        g_sdk->renderer->add_text(off_text, 14.f, off_pos, 0, 0xFFFFFFFF);

        // Queue info
        math::vector2 queue_pos = { pos.x + 5.f, pos.y + 75.f };
        std::string queue_text = "Next: ";
        bool first = true;
        for (auto weapon : state.queue) {
            if (weapon != WeaponType::NONE) {
                if (!first) {
                    queue_text += " > ";
                }
                queue_text += get_weapon_name(weapon);
                first = false;
            }
        }
        g_sdk->renderer->add_text(queue_text, 14.f, queue_pos, 0, 0xFFFFFFFF);
    }

    void draw_damage_indicators() {
        if (!settings.draw_damage_indicators) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto state = WeaponManager::get_current_state();

        for (auto* enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy || enemy->is_dead() ||
                enemy->get_team_id() == player->get_team_id() ||
                !enemy->is_visible())
                continue;

            float hp = enemy->get_hp();
            float max_hp = enemy->get_max_hp();

            // Get different damage sources
            float aa_damage = DamageCalculator::get_auto_attack_damage(enemy, true);
            float q_damage = 0.f;
            float r_damage = 0.f;

            if (can_cast_spell(0)) {
                q_damage = DamageCalculator::get_q_damage(state.main_hand.type, enemy);
            }

            if (can_cast_spell(3)) {
                r_damage = DamageCalculator::get_r_damage(state.main_hand.type, enemy);
            }

            // Calculate total damage and percentage of health
            float total_damage = aa_damage + q_damage + r_damage;
            float damage_percent = std::min(1.f, total_damage / max_hp);

            // Get enemy health bar position
            math::vector2 hp_pos = enemy->get_health_bar_position();

            // HP bar size
            float bar_width = 100.f;
            float bar_height = 10.f;

            // Draw damage indicator
            math::rect damage_rect(
                hp_pos.y,
                hp_pos.x + (bar_width * (1.f - damage_percent)),
                hp_pos.y + bar_height,
                hp_pos.x + bar_width
            );

            // Adjust colors based on kill potential
            uint32_t damage_color = settings.damage_indicator_color;
            if (total_damage >= hp) {
                damage_color = 0xFF00FF00; // Green if lethal
            }

            g_sdk->renderer->add_rectangle_filled(damage_rect, damage_color & 0x80FFFFFF);
            g_sdk->renderer->add_rectangle(damage_rect, damage_color, 1.f);

            // Add text for exact damage
            std::string damage_text = std::to_string(static_cast<int>(total_damage));
            math::vector2 text_pos = { hp_pos.x + bar_width + 5.f, hp_pos.y };
            g_sdk->renderer->add_text(damage_text, 14.f, text_pos, 0, damage_color);
        }
    }

    void draw_ranges() {
        if (!settings.draw_ranges) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto state = WeaponManager::get_current_state();

        // Draw auto attack range
        float aa_range = get_weapon_range(state.main_hand.type);
        g_sdk->renderer->add_circle_3d(
            player->get_position(),
            aa_range,
            1.5f,
            settings.range_color
        );

        // Draw Q range based on weapon
        float q_range = 0.f;
        switch (state.main_hand.type) {
        case WeaponType::CALIBRUM:
            q_range = CALIBRUM_Q_RANGE;
            break;
        case WeaponType::SEVERUM:
            q_range = SEVERUM_Q_RANGE;
            break;
        case WeaponType::GRAVITUM:
            q_range = GRAVITUM_Q_RANGE;
            break;
        case WeaponType::INFERNUM:
            q_range = INFERNUM_Q_RANGE;
            break;
        case WeaponType::CRESCENDUM:
            q_range = CRESCENDUM_Q_RANGE;
            break;
        default:
            break;
        }

        if (q_range > 0.f && can_cast_spell(0)) {
            g_sdk->renderer->add_circle_3d(
                player->get_position(),
                q_range,
                1.5f,
                0xFF00FF00
            );
        }

        // Draw ultimate range if enabled
        if (settings.auto_r && can_cast_spell(3)) {
            g_sdk->renderer->add_circle_3d(
                player->get_position(),
                settings.r_range,
                1.5f,
                0xFF0000FF
            );
        }
    }

    void draw_ammo_bars() {
        if (!settings.draw_ammo_bars) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto state = WeaponManager::get_current_state();

        // Get position over player
        math::vector3 pos = player->get_position();
        pos.y += 150.f; // Position above the player
        math::vector2 screen_pos = g_sdk->renderer->world_to_screen(pos);

        // Bar size
        float bar_width = 100.f;
        float bar_height = 8.f;
        float spacing = 12.f;

        // Main hand ammo bar
        float main_ammo_percent = state.main_hand.ammo / MAX_AMMO;
        math::rect main_bar_bg(
            screen_pos.y,
            screen_pos.x - bar_width / 2.f,
            screen_pos.y + bar_height,
            screen_pos.x + bar_width / 2.f
        );

        math::rect main_bar_fill(
            screen_pos.y,
            screen_pos.x - bar_width / 2.f,
            screen_pos.y + bar_height,
            screen_pos.x - bar_width / 2.f + bar_width * main_ammo_percent
        );

        // Background
        g_sdk->renderer->add_rectangle_filled(main_bar_bg, 0x80000000);
        g_sdk->renderer->add_rectangle(main_bar_bg, 0xFFFFFFFF, 1.f);

        // Colored fill based on weapon
        uint32_t main_color = 0xFFFFFFFF;
        switch (state.main_hand.type) {
        case WeaponType::CALIBRUM: main_color = 0xFF1E90FF; break; // Blue
        case WeaponType::SEVERUM: main_color = 0xFFFF4500; break; // Red
        case WeaponType::GRAVITUM: main_color = 0xFF800080; break; // Purple
        case WeaponType::INFERNUM: main_color = 0xFF32CD32; break; // Green
        case WeaponType::CRESCENDUM: main_color = 0xFFFFD700; break; // Gold
        default: break;
        }

        g_sdk->renderer->add_rectangle_filled(main_bar_fill, main_color);

        // Add weapon name and ammo count
        std::string main_text = get_weapon_name(state.main_hand.type) + ": " +
            std::to_string(static_cast<int>(state.main_hand.ammo));
        math::vector2 main_text_pos = {
            screen_pos.x - bar_width / 2.f,
            screen_pos.y - 16.f
        };
        g_sdk->renderer->add_text(main_text, 14.f, main_text_pos, 0, main_color);
    }

    void draw_weapon_queue() {
        if (!settings.draw_weapon_queue) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        auto state = WeaponManager::get_current_state();

        int window_width = g_sdk->renderer->get_window_width();
        int window_height = g_sdk->renderer->get_window_height();

        math::vector2 pos;

        // If position not set, initialize in a good location
        if (settings.weapon_queue_position.x == 0.f) {
            pos = { window_width * 0.01f, window_height * 0.1f }; // Position it at the top
            settings.weapon_queue_position = pos;
        }
        else {
            pos = settings.weapon_queue_position;
        }

        // Background
        math::rect bg_rect(pos.y, pos.x, pos.y + 40.f, pos.x + 250.f);
        g_sdk->renderer->add_rectangle_filled(bg_rect, 0x80000000);
        g_sdk->renderer->add_rectangle(bg_rect, 0xFFFFFFFF, 1.f);

        // Update drag area
        settings.weapon_queue_drag_area = bg_rect;

        // Title
        math::vector2 title_pos = { pos.x + 10.f, pos.y + 5.f };
        g_sdk->renderer->add_text("Weapon Queue", 14.f, title_pos, 0, 0xFFFFFFFF);

        // Draw weapon icons in queue
        float icon_size = 30.f;
        float spacing = 5.f;
        float start_x = pos.x + 20.f;
        float start_y = pos.y + 20.f;

        // Draw main hand first
        draw_weapon_icon(state.main_hand.type, { start_x, start_y }, icon_size, true);

        // Draw offhand
        draw_weapon_icon(state.off_hand.type, { start_x + icon_size + spacing, start_y }, icon_size, false);

        // Draw queued weapons
        for (size_t i = 0; i < state.queue.size(); i++) {
            if (state.queue[i] != WeaponType::NONE) {
                draw_weapon_icon(
                    state.queue[i],
                    { start_x + (i + 2) * (icon_size + spacing), start_y },
                    icon_size,
                    false
                );
            }
        }
    }

    void draw_weapon_icon(WeaponType weapon, math::vector2 pos, float size, bool is_main) {
        // Get color based on weapon
        uint32_t color = 0xFFFFFFFF;
        std::string label = "";

        switch (weapon) {
        case WeaponType::CALIBRUM:
            color = 0xFF1E90FF; // Blue
            label = "C";
            break;
        case WeaponType::SEVERUM:
            color = 0xFFFF4500; // Red
            label = "S";
            break;
        case WeaponType::GRAVITUM:
            color = 0xFF800080; // Purple
            label = "G";
            break;
        case WeaponType::INFERNUM:
            color = 0xFF32CD32; // Green
            label = "I";
            break;
        case WeaponType::CRESCENDUM:
            color = 0xFFFFD700; // Gold
            label = "R";
            break;
        default:
            label = "?";
            break;
        }

        // Draw circle background
        if (is_main) {
            // Solid circle with border for main weapon
            g_sdk->renderer->add_circle_filled_2d(pos, size / 2.f, color & 0xCFFFFFFF);
            g_sdk->renderer->add_circle_2d(pos, size / 2.f, 2.f, 0xFFFFFFFF);
        }
        else {
            // Semi-transparent circle for other weapons
            g_sdk->renderer->add_circle_filled_2d(pos, size / 2.f, color & 0xAFFFFFFF);
            g_sdk->renderer->add_circle_2d(pos, size / 2.f, 1.f, 0xFFFFFFFF);
        }

        // Add weapon label
        math::vector2 text_pos = {
            pos.x - 4.f,
            pos.y - 8.f
        };
        g_sdk->renderer->add_text(label, 16.f, text_pos, 0, 0xFFFFFFFF);
    }


    void create_menu() {
        menu = g_sdk->menu_manager->add_category("aphelios", "Aphelios");
        if (!menu) return;

        // Main Settings
        menu->add_checkbox("auto_q", "Auto Use Q", true, [](bool value) {
            settings.auto_q = value;
            });

        menu->add_checkbox("auto_r", "Auto Ultimate", true, [](bool value) {
            settings.auto_r = value;
            });

        menu->add_hotkey("manual_r_key", "Manual R Key", settings.manual_r_key, false, true,
            [](std::string* key, bool value) {
                settings.manual_r_key = (unsigned char)key->at(0);
                settings.manual_r_key_pressed = value;
            });

        menu->add_hotkey("manual_switch_key", "Manual W Key", settings.manual_switch_key, false, true,
            [](std::string* key, bool value) {
                settings.manual_switch_key = (unsigned char)key->at(0);
                settings.manual_switch_key_pressed = value;
            });

        // Weapon Settings
        auto weapon_category = menu->add_sub_category("weapons", "Weapon Settings");

        weapon_category->add_checkbox("auto_weapon_switch", "Auto Weapon Switch", true,
            [](bool value) {
                settings.auto_weapon_switch = value;
            });

        weapon_category->add_checkbox("instant_q_after_switch", "Instant Q After Switch", true,
            [](bool value) {
                settings.instant_q_after_switch = value;
            });

        weapon_category->add_checkbox("smart_weapon_rotation", "Smart Weapon Rotation", true,
            [](bool value) {
                settings.smart_weapon_rotation = value;
            });

        weapon_category->add_checkbox("use_standard_weapon_queue", "Use Standard Queue", false,
            [](bool value) {
                settings.use_standard_weapon_queue = value;
            });

        // Anti-magneting (add this section)
        auto anti_magneting_category = menu->add_sub_category("anti_magneting", "Anti-Magneting");

        anti_magneting_category->add_checkbox("anti_magneting", "Prevent Auto-Magnet", true,
            [](bool value) {
                settings.anti_magneting = value;
            });

        anti_magneting_category->add_checkbox("show_magnet_warnings", "Show Magnet Warnings", true,
            [](bool value) {
                settings.show_magnet_warnings = value;
            });

        // Calibrum Settings
        auto calibrum_category = menu->add_sub_category("calibrum", "Calibrum (Blue)");

        calibrum_category->add_checkbox("use_calibrum_q", "Use Calibrum Q", true,
            [](bool value) {
                settings.use_calibrum_q = value;
            });

        calibrum_category->add_slider_float("calibrum_q_min_range", "Min Range", 0.f, 650.f, 50.f, 650.f,
            [](float value) {
                settings.calibrum_q_min_range = value;
            });

        // Severum Settings
        auto severum_category = menu->add_sub_category("severum", "Severum (Red)");

        severum_category->add_checkbox("use_severum_q", "Use Severum Q", true,
            [](bool value) {
                settings.use_severum_q = value;
            });

        severum_category->add_checkbox("severum_q_toggle", "Toggle Severum Q", true,
            [](bool value) {
                settings.severum_q_toggle = value;
            });

        // Gravitum Settings
        auto gravitum_category = menu->add_sub_category("gravitum", "Gravitum (Purple)");

        gravitum_category->add_checkbox("use_gravitum_q", "Use Gravitum Q", true,
            [](bool value) {
                settings.use_gravitum_q = value;
            });

        gravitum_category->add_checkbox("gravitum_q_antigapcloser", "Anti-Gapcloser Q", true,
            [](bool value) {
                settings.gravitum_q_antigapcloser = value;
            });

        gravitum_category->add_checkbox("gravitum_q_manual", "Manual Q Only", false,
            [](bool value) {
                settings.gravitum_q_manual = value;
            });

        // Infernum Settings
        auto infernum_category = menu->add_sub_category("infernum", "Infernum (Green)");

        infernum_category->add_checkbox("use_infernum_q", "Use Infernum Q", true,
            [](bool value) {
                settings.use_infernum_q = value;
            });

        infernum_category->add_slider_int("infernum_q_min_targets", "Min Targets", 1, 5, 1, 2,
            [](int value) {
                settings.infernum_q_min_targets = value;
            });

        infernum_category->add_checkbox("infernum_q_prioritize_heroes", "Prioritize Heroes", true,
            [](bool value) {
                settings.infernum_q_prioritize_heroes = value;
            });

        // Crescendum Settings
        auto crescendum_category = menu->add_sub_category("crescendum", "Crescendum (White)");

        crescendum_category->add_checkbox("use_crescendum_q", "Use Crescendum Q", true,
            [](bool value) {
                settings.use_crescendum_q = value;
            });

        crescendum_category->add_checkbox("crescendum_q_turret_safety", "Turret Safety", true,
            [](bool value) {
                settings.crescendum_q_turret_safety = value;
            });

        crescendum_category->add_slider_float("crescendum_q_min_distance", "Min Distance", 100.f, 400.f, 10.f, 250.f,
            [](float value) {
                settings.crescendum_q_min_distance = value;
            });

        // Ultimate Settings
        auto ult_category = menu->add_sub_category("ultimate", "Ultimate Settings");

        ult_category->add_slider_int("r_min_enemies", "Min Enemies Hit", 1, 5, 1, 2,
            [](int value) {
                settings.r_min_enemies = value;
            });

        ult_category->add_slider_float("r_range", "Ultimate Range", R_RANGE_MIN, R_RANGE_MAX, 100.f, R_RANGE,
            [](float value) {
                settings.r_range = value;
            });

        ult_category->add_checkbox("r_save_for_combo", "Save for Lethal", true,
            [](bool value) {
                settings.r_save_for_combo = value;
            });

        ult_category->add_slider_float("r_min_hp_ratio", "Min Enemy HP %", 0.1f, 1.0f, 0.05f, 0.4f,
            [](float value) {
                settings.r_min_hp_ratio = value;
            });

        // Farm Settings
        auto farm_category = menu->add_sub_category("farm", "Farm Settings");

        farm_category->add_checkbox("farm_with_abilities", "Farm with Abilities", true,
            [](bool value) {
                settings.farm_with_abilities = value;
            });

        farm_category->add_checkbox("prioritize_cannon_minions", "Prioritize Cannons", true,
            [](bool value) {
                settings.prioritize_cannon_minions = value;
            });

        farm_category->add_slider_int("min_minions_for_aoe", "Min Minions for AoE", 1, 6, 1, 3,
            [](int value) {
                settings.min_minions_for_aoe = value;
            });

        // Safety Settings
        auto safety_category = menu->add_sub_category("safety", "Safety Settings");

        safety_category->add_checkbox("tower_dive_prevention", "Prevent Tower Diving", true,
            [](bool value) {
                settings.tower_dive_prevention = value;
            });

        safety_category->add_checkbox("anti_gapcloser", "Anti-Gapcloser", true,
            [](bool value) {
                settings.anti_gapcloser = value;
            });

        safety_category->add_checkbox("kite_melee_champions", "Kite Melee Champions", true,
            [](bool value) {
                settings.kite_melee_champions = value;
            });

        // Drawing Settings
        auto draw_category = menu->add_sub_category("drawing", "Drawing Settings");

        draw_category->add_checkbox("draw_ranges", "Draw Ranges", true,
            [](bool value) {
                settings.draw_ranges = value;
            });

        draw_category->add_checkbox("draw_weapon_info", "Draw Weapon Info", true,
            [](bool value) {
                settings.draw_weapon_info = value;
            });

        draw_category->add_checkbox("draw_damage_indicators", "Draw Damage Indicators", true,
            [](bool value) {
                settings.draw_damage_indicators = value;
            });

        draw_category->add_checkbox("draw_ammo_bars", "Draw Ammo Bars", true,
            [](bool value) {
                settings.draw_ammo_bars = value;
            });

        draw_category->add_checkbox("draw_weapon_queue", "Draw Weapon Queue", true,
            [](bool value) {
                settings.draw_weapon_queue = value;
            });

        draw_category->add_colorpicker("range_color", "Range Color", 0xFF00FF00,
            [](uint32_t value) {
                settings.range_color = value;
            });

        draw_category->add_colorpicker("weapon_info_color", "Info Text Color", 0xFFFFFFFF,
            [](uint32_t value) {
                settings.weapon_info_color = value;
            });

        // Debug Settings
        auto debug_category = menu->add_sub_category("debug", "Debug Settings");

        debug_category->add_checkbox("debug_mode", "Debug Mode", false,
            [](bool value) {
                settings.debug_mode = value;
            });

        debug_category->add_checkbox("debug_weapon_switches", "Debug Weapon Switches", false,
            [](bool value) {
                settings.debug_weapon_switches = value;
            });

        debug_category->add_checkbox("debug_damage_calcs", "Debug Damage Calculations", false,
            [](bool value) {
                settings.debug_damage_calcs = value;
            });

        debug_category->add_slider_float("min_spell_delay", "Min Spell Delay", 0.01f, 0.25f, 0.01f, 0.05f,
            [](float value) {
                settings.min_spell_delay = value;
                auto& order_manager = vengaboys::OrderManager::get_instance();
                order_manager.set_min_order_delay(value);
            });
    }

    void optimize_weapon_rotation() {
        auto state = WeaponManager::get_current_state();
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (settings.debug_mode) log_debug("Optimizing weapon rotation");

        bool teamfight_imminent = false;
        bool objective_coming = false;
        bool siege_situation = false;
        int nearby_enemies = 0;

        for (auto* enemy : g_sdk->object_manager->get_heroes()) {
            if (!enemy || enemy->is_dead() ||
                enemy->get_team_id() == player->get_team_id())
                continue;

            float dist = enemy->get_position().distance(player->get_position());
            if (dist <= 1200.f) {
                nearby_enemies++;
                if (enemy->get_hp() / enemy->get_max_hp() <= 0.3f) {
                    teamfight_imminent = true;
                    if (settings.debug_mode) log_debug("Teamfight imminent - low HP enemy nearby");
                }
            }
        }

        float game_time = g_sdk->clock_facade->get_game_time();
        if (game_time >= 1200.f && game_time <= 1260.f) {
            objective_coming = true;
            if (settings.debug_mode) log_debug("Objective spawning soon");
        }

        for (auto* turret : g_sdk->object_manager->get_turrets()) {
            if (!turret || turret->is_dead()) continue;
            if (turret->get_position().distance(player->get_position()) <= 1000.f) {
                siege_situation = true;
                if (settings.debug_mode) log_debug("Siege situation detected");
                break;
            }
        }

        std::vector<WeaponType> optimal_order;

        if (teamfight_imminent) {
            if (settings.debug_mode) log_debug("Setting teamfight weapon order");
            optimal_order = {
                WeaponType::INFERNUM,
                WeaponType::CRESCENDUM,
                WeaponType::GRAVITUM,
                WeaponType::SEVERUM,
                WeaponType::CALIBRUM
            };
        }
        else if (objective_coming) {
            if (settings.debug_mode) log_debug("Setting objective weapon order");
            optimal_order = {
                WeaponType::CRESCENDUM,
                WeaponType::SEVERUM,
                WeaponType::INFERNUM,
                WeaponType::CALIBRUM,
                WeaponType::GRAVITUM
            };
        }
        else if (siege_situation) {
            if (settings.debug_mode) log_debug("Setting siege weapon order");
            optimal_order = {
                WeaponType::CALIBRUM,
                WeaponType::INFERNUM,
                WeaponType::GRAVITUM,
                WeaponType::CRESCENDUM,
                WeaponType::SEVERUM
            };
        }
        else {
            if (settings.debug_mode) log_debug("Setting default weapon order");
            optimal_order = {
                WeaponType::CALIBRUM,
                WeaponType::SEVERUM,
                WeaponType::INFERNUM,
                WeaponType::GRAVITUM,
                WeaponType::CRESCENDUM
            };
        }

        bool needs_update = false;
        for (size_t i = 0; i < std::min(size_t(3), optimal_order.size()); i++) {
            if (state.queue[i] != optimal_order[i]) {
                needs_update = true;
                state.queue[i] = optimal_order[i];
            }
        }

        if (needs_update && settings.debug_mode) {
            std::string situation = teamfight_imminent ? "Teamfight" :
                objective_coming ? "Objective" :
                siege_situation ? "Siege" : "Default";
            log_debug("Updated weapon rotation for: %s", situation.c_str());
        }
    }

    float get_weapon_range(WeaponType weapon) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return BASE_ATTACK_RANGE;

        switch (weapon) {
        case WeaponType::CALIBRUM:
            return BASE_ATTACK_RANGE + CALIBRUM_BONUS_RANGE;
        case WeaponType::CRESCENDUM: {
            auto state = WeaponManager::get_current_state();
            return BASE_ATTACK_RANGE - (50.f * std::max(0.f, state.main_hand.stacks - 1.f));
        }
        default:
            return BASE_ATTACK_RANGE;
        }
    }

    float DamageCalculator::calculate_dps(WeaponType weapon, game_object* target) {
        if (!target) return 0.f;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return 0.f;

        float attack_speed = player->get_attack_speed_mod() * player->get_percent_attack_speed_mod();
        float base_dps = get_auto_attack_damage(target, true) * attack_speed;

        switch (weapon) {
        case WeaponType::SEVERUM:
            base_dps *= 1.3f;
            break;
        case WeaponType::INFERNUM:
            base_dps *= 1.2f;
            break;
        case WeaponType::CRESCENDUM: {
            auto state = WeaponManager::get_current_state();
            base_dps *= (1.0f + 0.05f * state.main_hand.stacks);
            break;
        }
        default:
            break;
        }

        return base_dps;
    }

    bool can_cast_spell(int spell_slot, float delay) {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        auto spell = player->get_spell(spell_slot);
        if (!spell) {
            if (settings.debug_mode) log_debug("Spell slot %d is invalid", spell_slot);
            return false;
        }

        if (spell->get_cooldown() > 0.f) {
            if (settings.debug_mode) log_debug("Spell %d on cooldown: %.1f",
                spell_slot, spell->get_cooldown());
            return false;
        }

        if (player->get_active_spell_cast()) {
            if (settings.debug_mode) log_debug("Currently casting another spell");
            return false;
        }

        bool order_manager_can_cast = vengaboys::OrderManager::get_instance().can_cast(player, spell_slot, delay);
        if (settings.debug_mode) log_debug("OrderManager can_cast(%d): %d", spell_slot, order_manager_can_cast ? 1 : 0);
        return order_manager_can_cast;
    }

    void weave_attack(game_object* target) {
        if (!target) return;

        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return;

        if (sdk::orbwalker->is_attack_ready_in(0.f)) {
            float attack_delay = player->get_attack_cast_delay();
            float ping_delay = g_sdk->net_client->get_ping() / 1000.0f;
            float total_delay = attack_delay + ping_delay;

            math::vector3 predicted_pos = sdk::prediction->predict_on_path(target, total_delay);

            float attack_range = player->get_attack_range();
            float predicted_distance = player->get_position().distance(predicted_pos) -
                player->get_bounding_radius() - target->get_bounding_radius();

            if (predicted_distance <= attack_range) {
                if (settings.debug_mode) log_debug("Attacking with ping+delay prediction: %.1f+%.1f=%.1f ms",
                    attack_delay * 1000, ping_delay * 1000, total_delay * 1000);
                sdk::orbwalker->attack(target);
            }
            else if (predicted_distance <= attack_range + 200.f) {
                if (settings.debug_mode) log_debug("Target predicted to leave range soon, not attacking");
            }
            else {
                if (settings.debug_mode) log_debug("Target predicted to be out of range by %.1f units",
                    predicted_distance - attack_range);
            }
        }
    }

    void update_infotab() {
        if (!sdk::infotab || !settings.draw_weapon_info) return;

        if (infotab_text_id != 0) {
            sdk::infotab->remove_text(infotab_text_id);
            infotab_text_id = 0;
        }

        auto state = WeaponManager::get_current_state();

        infotab_sdk::text_entry title = { "Aphelios", settings.weapon_info_color };
        infotab_text_id = sdk::infotab->add_text(title, [state]() -> infotab_sdk::text_entry {
            infotab_sdk::text_entry entry;
            entry.color = settings.weapon_info_color;

            std::string current_weapons = get_weapon_name(state.main_hand.type) + " | " + get_weapon_name(state.off_hand.type);

            std::string next_weapons = "\nNext: ";
            bool first = true;
            for (auto weapon : state.queue) {
                if (weapon != WeaponType::NONE) {
                    if (!first) {
                        next_weapons += " > ";
                    }
                    next_weapons += get_weapon_name(weapon);
                    first = false;
                }
            }

            entry.text = current_weapons + next_weapons;
            return entry;
            });
    }

    std::string get_weapon_name(WeaponType weapon) {
        switch (weapon) {
        case WeaponType::CALIBRUM: return "Calibrum";
        case WeaponType::SEVERUM: return "Severum";
        case WeaponType::GRAVITUM: return "Gravitum";
        case WeaponType::INFERNUM: return "Infernum";
        case WeaponType::CRESCENDUM: return "Crescendum";
        default: return "None";
        }
    }

    void load() {
        g_sdk->log_console("[+] Loading Aphelios AIO...");

        if (!sdk_init::orbwalker() ||
            !sdk_init::target_selector() ||
            !sdk_init::prediction() ||
            !sdk_init::damage() ||
            !sdk_init::infotab()) {
            g_sdk->log_console("[!] Failed to load dependencies!");
            return;
        }

        g_sdk->log_console("[Aphelios] Using pre-defined buff hashes");

        auto& order_manager = vengaboys::OrderManager::get_instance();
        order_manager.set_min_order_delay(settings.min_spell_delay);
        order_manager.set_max_queue_size(3);

        create_menu();
        if (!menu) {
            g_sdk->log_console("[!] Failed to create menu");
            return;
        }

        g_sdk->event_manager->register_callback(event_manager::event::game_update,
            reinterpret_cast<void*>(&on_update));
        g_sdk->event_manager->register_callback(event_manager::event::draw_world,
            reinterpret_cast<void*>(&on_draw));
        g_sdk->event_manager->register_callback(event_manager::event::wndproc,
            reinterpret_cast<void*>(&on_wndproc));

        damage_cache.clear();
        last_damage_cache_clear = g_sdk->clock_facade->get_game_time();

        // Initialize anti-magneting
        is_magneting_detected = false;
        consecutive_magnetic_frames = 0;
        last_magneting_check = 0.f;

        initialized = true;
        if (settings.debug_mode) log_debug("Initialization completed successfully");
        g_sdk->log_console("[+] Aphelios AIO loaded successfully");
    }

    void unload() {
        if (!initialized) return;

        g_sdk->log_console("[-] Unloading Aphelios AIO...");

        if (g_sdk->event_manager) {
            g_sdk->event_manager->unregister_callback(event_manager::event::game_update,
                reinterpret_cast<void*>(&on_update));
            g_sdk->event_manager->unregister_callback(event_manager::event::draw_world,
                reinterpret_cast<void*>(&on_draw));
            g_sdk->event_manager->unregister_callback(event_manager::event::wndproc,
                reinterpret_cast<void*>(&on_wndproc));
        }

        if (sdk::infotab && infotab_text_id != 0) {
            sdk::infotab->remove_text(infotab_text_id);
            infotab_text_id = 0;
        }

        damage_cache.clear();

        initialized = false;
        if (settings.debug_mode) log_debug("Cleanup completed successfully");
        g_sdk->log_console("[-] Aphelios AIO unloaded successfully");
    }
}