#include "ordermanager.hpp"
#include <stdarg.h>

namespace vengaboys
{
    OrderManager& OrderManager::get_instance()
    {
        static OrderManager instance;
        return instance;
    }

    OrderManager::OrderManager()
    {
        for (auto& time : last_spell_call) {
            time = 0.f;
        }
        for (auto& time : last_issue_order_call) {
            time = 0.f;
        }
    }

    void OrderManager::log_debug(const char* format, ...)
    {
        if (!debug_mode) return;

        try {
            va_list args;
            va_start(args, format);
            char buffer[512];
            vsnprintf(buffer, sizeof(buffer), format, args);
            va_end(args);
            g_sdk->log_console("[OrderManager] %s", buffer);
        }
        catch (...) {
            g_sdk->log_console("[OrderManager] Failed to log message");
        }
    }

    float OrderManager::get_ping_delay() const
    {
        if (compensate_for_ping) {
            return static_cast<float>(g_sdk->net_client->get_ping()) / 1000.0f;
        }
        return 0.f;
    }

    bool OrderManager::is_issue_order_passed(int spell_slot, float value)
    {
        const auto time = g_sdk->clock_facade->get_game_time();
        return time - last_spell_call[spell_slot] >= value;
    }

    bool OrderManager::is_issue_order_passed(game_object_order order_type, float value)
    {
        const auto time = g_sdk->clock_facade->get_game_time();
        return time - last_issue_order_call[static_cast<int>(order_type)] >= value;
    }

    bool OrderManager::is_target_valid(game_object* target, bool check_targetable)
    {
        if (!target || !target->is_valid() || target->is_dead())
            return false;

        if (check_targetable && !target->is_targetable())
            return false;

        return true;
    }

    bool OrderManager::is_in_spell_range(game_object* source, game_object* target, int spell_slot, float extra_range)
    {
        if (!source || !target) return false;

        auto spell = source->get_spell(spell_slot);
        if (!spell) return false;

        float spell_range = spell->get_cast_range() + extra_range;

        float distance = source->get_position().distance(target->get_position()) -
            source->get_bounding_radius() - target->get_bounding_radius();

        return distance <= spell_range;
    }

    bool OrderManager::is_in_attack_range(game_object* source, game_object* target, float extra_range)
    {
        if (!source || !target) return false;

        if (sdk::orbwalker) {
            return sdk::orbwalker->is_in_auto_attack_range(source, target, extra_range);
        }

        float attack_range = source->get_attack_range() + extra_range;

        float distance = source->get_position().distance(target->get_position()) -
            source->get_bounding_radius() - target->get_bounding_radius();

        return distance <= attack_range;
    }

    bool OrderManager::can_cast(game_object* source, int spell_slot, float delay)
    {
        if (!source) {
            log_debug("Can't cast: source is null");
            return false;
        }

        auto spell = source->get_spell(spell_slot);
        if (!spell) {
            log_debug("Can't cast: spell %d not found", spell_slot);
            return false;
        }

        if (spell->get_level() <= 0) {
            log_debug("Can't cast: spell %d not leveled", spell_slot);
            return false;
        }

        if (spell->get_cooldown() > 0.01f) {
            log_debug("Can't cast: spell %d on cooldown (%.2f)", spell_slot, spell->get_cooldown());
            return false;
        }

        if (!is_issue_order_passed(spell_slot, order_delay + delay)) {
            log_debug("Can't cast: too soon after previous cast");
            return false;
        }

        if (spell_slot != 3 && !is_issue_order_passed(3, post_ult_cooldown)) {
            log_debug("Can't cast: too soon after ultimate");
            return false;
        }

        if (is_blocked()) {
            log_debug("Can't cast: orders are blocked");
            return false;
        }

        if (custom_spell_validator && !custom_spell_validator(source, spell_slot)) {
            log_debug("Can't cast: custom validator rejected");
            return false;
        }

        return true;
    }

    bool OrderManager::cast_spell(int spell_slot, game_object* target, OrderPriority priority)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("Cast spell failed: player is null");
            return false;
        }

        if (!can_cast(player, spell_slot, 0.f)) {
            return false;
        }

        if (!is_target_valid(target)) {
            log_debug("Cast spell failed: invalid target");
            return false;
        }

        if (!is_in_spell_range(player, target, spell_slot)) {
            log_debug("Cast spell failed: target out of range");
            return false;
        }

        try {
            if (!g_sdk->is_replay_mode()) {
                player->cast_spell(spell_slot, target);
            }

            last_spell_call[spell_slot] = g_sdk->clock_facade->get_game_time();
            log_debug("Cast spell %d on target success", spell_slot);
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    bool OrderManager::cast_spell(int spell_slot, const math::vector3& position, OrderPriority priority)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("Cast spell failed: player is null");
            return false;
        }

        if (!can_cast(player, spell_slot, 0.f)) {
            return false;
        }

        auto spell = player->get_spell(spell_slot);
        if (spell && spell->get_cast_range() > 0) {
            float distance = player->get_position().distance(position);
            if (distance > spell->get_cast_range()) {
                log_debug("Cast spell failed: position out of range (%.1f > %.1f)",
                    distance, spell->get_cast_range());
                return false;
            }
        }

        try {
            if (!g_sdk->is_replay_mode()) {
                player->cast_spell(spell_slot, position);
            }

            last_spell_call[spell_slot] = g_sdk->clock_facade->get_game_time();
            log_debug("Cast spell %d at position success", spell_slot);
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    bool OrderManager::cast_spell(int spell_slot, const math::vector3& start_position, const math::vector3& end_position, OrderPriority priority)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("Cast spell failed: player is null");
            return false;
        }

        if (!can_cast(player, spell_slot, 0.f)) {
            return false;
        }

        try {
            if (!g_sdk->is_replay_mode()) {
                player->cast_spell(spell_slot, start_position, end_position);
            }

            last_spell_call[spell_slot] = g_sdk->clock_facade->get_game_time();
            log_debug("Cast spell %d with vector targeting success", spell_slot);
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    bool OrderManager::update_chargeable_spell(int spell_slot, const math::vector3& position, bool release_cast)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("Update chargeable spell failed: player is null");
            return false;
        }

        const auto game_time = g_sdk->clock_facade->get_game_time();
        if (game_time - last_spell_call[spell_slot] < order_delay) {
            log_debug("Update chargeable spell failed: too soon after previous call");
            return false;
        }

        try {
            if (!g_sdk->is_replay_mode()) {
                player->update_chargeable_spell(spell_slot, position, release_cast);
            }

            last_spell_call[spell_slot] = game_time;
            log_debug("Update chargeable spell %d success", spell_slot);
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    bool OrderManager::use_object(game_object* target)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("Use object failed: player is null");
            return false;
        }

        if (!is_target_valid(target)) {
            log_debug("Use object failed: invalid target");
            return false;
        }

        const auto game_time = g_sdk->clock_facade->get_game_time();
        if (game_time - last_use_object_call < order_delay) {
            log_debug("Use object failed: too soon after previous call");
            return false;
        }

        try {
            if (!g_sdk->is_replay_mode()) {
                player->use_object(target);
            }

            last_use_object_call = game_time;
            log_debug("Use object success");
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    bool OrderManager::move_to(const math::vector3& position, OrderPriority priority)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("Move failed: player is null");
            return false;
        }

        const auto game_time = g_sdk->clock_facade->get_game_time();
        if (game_time - last_issue_order_call[static_cast<int>(game_object_order::move_to)] < order_delay) {
            log_debug("Move failed: too soon after previous move");
            return false;
        }

        if (is_blocked()) {
            log_debug("Move failed: orders are blocked");
            return false;
        }

        try {
            if (!g_sdk->is_replay_mode()) {
                player->issue_order(game_object_order::move_to, position);
            }

            last_issue_order_call[static_cast<int>(game_object_order::move_to)] = game_time;
            log_debug("Move to position success");
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    bool OrderManager::attack(game_object* target, OrderPriority priority)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) {
            log_debug("Attack failed: player is null");
            return false;
        }

        if (!is_target_valid(target)) {
            log_debug("Attack failed: invalid target");
            return false;
        }

        if (target->get_team_id() == player->get_team_id()) {
            log_debug("Attack failed: trying to attack ally");
            return false;
        }

        if (!is_in_attack_range(player, target)) {
            log_debug("Attack failed: target out of range");
            return false;
        }

        const auto game_time = g_sdk->clock_facade->get_game_time();
        if (game_time - last_issue_order_call[static_cast<int>(game_object_order::attack_unit)] < order_delay) {
            log_debug("Attack failed: too soon after previous attack");
            return false;
        }

        if (is_blocked()) {
            log_debug("Attack failed: orders are blocked");
            return false;
        }

        try {
            if (!g_sdk->is_replay_mode()) {
                player->issue_order(game_object_order::attack_unit, target);
            }

            last_issue_order_call[static_cast<int>(game_object_order::attack_unit)] = game_time;
            log_debug("Attack success");
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    bool OrderManager::smart_attack(game_object* target, OrderPriority priority)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !target) return false;

        if (!is_target_valid(target)) return false;

        try {
            float attack_delay = player->get_attack_cast_delay();
            float ping_delay = get_ping_delay();
            float total_delay = attack_delay + ping_delay;

            math::vector3 predicted_pos;
            if (total_delay > 0.05f && sdk::prediction) {
                predicted_pos = sdk::prediction->predict_on_path(target, total_delay);
            }
            else {
                predicted_pos = target->get_position();
            }

            float attack_range = player->get_attack_range();
            float predicted_distance = player->get_position().distance(predicted_pos) -
                player->get_bounding_radius() - target->get_bounding_radius();

            if (predicted_distance <= attack_range) {
                log_debug("Smart attack with ping+delay prediction: %.1f+%.1f=%.1f ms",
                    attack_delay * 1000, ping_delay * 1000, total_delay * 1000);

                const auto game_time = g_sdk->clock_facade->get_game_time();
                if (game_time - last_issue_order_call[static_cast<int>(game_object_order::attack_unit)] < order_delay) {
                    return false;
                }

                if (!g_sdk->is_replay_mode()) {
                    player->issue_order(game_object_order::attack_unit, target);
                }

                last_issue_order_call[static_cast<int>(game_object_order::attack_unit)] = game_time;
                return true;
            }
            else {
                log_debug("Smart attack failed: target predicted to be out of range by %.1f units",
                    predicted_distance - attack_range);
                return false;
            }
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception in smart_attack: %s", e.what());
            return false;
        }
    }

    float OrderManager::get_last_cast_time(int spell_slot) const
    {
        if (spell_slot >= 0 && spell_slot < last_spell_call.size()) {
            return last_spell_call[spell_slot];
        }
        return 0.f;
    }

    void OrderManager::block_orders(float duration)
    {
        block_until_time = g_sdk->clock_facade->get_game_time() + duration;
        log_debug("Orders blocked for %.2f seconds", duration);
    }

    bool OrderManager::is_blocked() const
    {
        return g_sdk->clock_facade->get_game_time() < block_until_time;
    }

    void OrderManager::allow_orders()
    {
        block_until_time = 0.f;
        log_debug("Orders unblocked");
    }
}