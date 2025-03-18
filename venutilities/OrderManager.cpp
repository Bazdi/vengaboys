#include "ordermanager.hpp"

namespace vengaboys
{
    OrderManager& OrderManager::get_instance()
    {
        static OrderManager instance;
        return instance;
    }

    OrderManager::OrderManager()
    {
        for (auto& time : last_cast_times) {
            time = 0.f;
        }
    }

    bool OrderManager::is_issue_order_passed(const int spell_slot, const float value)
    {
        const auto& time = g_sdk->clock_facade->get_game_time();
        return get_last_cast_time(spell_slot) + value < time;
    }

    bool OrderManager::can_cast(game_object* source, int spell_slot, float delay)
    {
        if (!source) return false;

        auto spell = source->get_spell(spell_slot);
        if (!spell) return false;

        if (spell->get_cooldown() > 0.01f) return false;

        if (!is_issue_order_passed(spell_slot, min_order_delay + delay)) {
            return false;
        }

        if (spell_slot != 3 && !is_issue_order_passed(3, post_ult_cooldown)) {
            return false;
        }

        return true;
    }

    bool OrderManager::cast_spell(int spell_slot, game_object* target, OrderPriority priority)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        if (!can_cast(player, spell_slot, 0.f)) return false;

        try {
            float current_time = g_sdk->clock_facade->get_game_time();
            player->cast_spell(spell_slot, target);
            last_cast_times[spell_slot] = current_time;
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
        if (!player) return false;

        if (!can_cast(player, spell_slot, 0.f)) return false;

        try {
            float current_time = g_sdk->clock_facade->get_game_time();
            player->cast_spell(spell_slot, position);
            last_cast_times[spell_slot] = current_time;
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
        if (!player) return false;

        if (!can_cast(player, spell_slot, 0.f)) return false;

        try {
            float current_time = g_sdk->clock_facade->get_game_time();
            player->cast_spell(spell_slot, start_position, end_position);
            last_cast_times[spell_slot] = current_time;
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
        if (!player) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - last_move_time < min_order_delay) return false;

        try {
            player->issue_order(game_object_order::move_to, position);
            last_move_time = current_time;
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
        if (!player || !target || !target->is_valid() || target->is_dead()) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        if (current_time - last_attack_time < min_order_delay) return false;

        try {
            player->issue_order(game_object_order::attack_unit, target);
            last_attack_time = current_time;
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[OrderManager] Exception: %s", e.what());
            return false;
        }
    }

    float OrderManager::get_last_cast_time(int spell_slot) const
    {
        if (spell_slot >= 0 && spell_slot < last_cast_times.size()) {
            return last_cast_times[spell_slot];
        }
        return 0.f;
    }
}