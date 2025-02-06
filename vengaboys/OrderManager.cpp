#include "ordermanager.hpp"
#include "jax.hpp"

namespace vengaboys
{
    OrderManager& OrderManager::get_instance()
    {
        static OrderManager instance;
        return instance;
    }

    OrderManager::OrderManager() : min_order_delay(0.05f), max_queue_size(3), last_order_time(0.f)
    {
        for (auto& time : last_cast_times) {
            time = 0.f;
        }
    }

    bool OrderManager::validate_spell_cast(game_object* source, int spell_slot, float delay) const
    {
        if (!source) {
            log_debug("validate_spell_cast: Invalid source");
            return false;
        }

        auto spell = source->get_spell(spell_slot);
        if (!spell) {
            log_debug("validate_spell_cast: Invalid spell for slot %d", spell_slot);
            return false;
        }

        if (spell->get_cooldown() > 0.f) {
            log_debug("validate_spell_cast: Spell %d on cooldown", spell_slot);
            return false;
        }

        float current_time = g_sdk->clock_facade->get_game_time();
        float elapsed_time = current_time - last_cast_times[spell_slot];

        if (elapsed_time < delay) {
            log_debug("validate_spell_cast: Delay preventing cast for spell %d, elapsed: %.2f, delay: %.2f",
                spell_slot, elapsed_time, delay);
            return false;
        }

        return true;
    }

    void OrderManager::cleanup_old_orders(float current_time)
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!order_queue.empty()) {
            const auto& front = order_queue.front();
            if (current_time - front.time_issued > 1.0f) {
                order_queue.pop();
            }
            else {
                break;
            }
        }
    }

    bool OrderManager::can_cast(game_object* source, int spell_slot, float delay)
    {
        float current_time = g_sdk->clock_facade->get_game_time();

        if (current_time - last_order_time < min_order_delay) {
            log_debug("can_cast: Rate limit exceeded, waiting %.3f seconds",
                min_order_delay - (current_time - last_order_time));
            return false;
        }

        cleanup_old_orders(current_time);

        if (is_queue_full()) {
            log_debug("can_cast: Order queue full");
            return false;
        }

        return validate_spell_cast(source, spell_slot, delay);
    }

    bool OrderManager::process_spell_order(const SpellOrder& order)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player) return false;

        try {
            if (order.is_targeted) {
                if (!order.target || !order.target->is_valid() || order.target->is_dead()) {
                    return false;
                }
                player->cast_spell(order.spell_slot, order.target);
            }
            else {
                player->cast_spell(order.spell_slot, order.position);
            }

            update_cast_time(order.spell_slot);
            return true;
        }
        catch (const std::exception& e) {
            g_sdk->log_console("[!] Exception in process_spell_order: %s", e.what());
            return false;
        }
    }

    void OrderManager::update_cast_time(int spell_slot)
    {
        if (spell_slot >= 0 && spell_slot < 4) {
            float current_time = g_sdk->clock_facade->get_game_time();
            last_cast_times[spell_slot] = current_time;
            last_order_time = current_time;
        }
    }

    bool OrderManager::cast_spell(int spell_slot, game_object* target)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast(player, spell_slot)) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        SpellOrder order(spell_slot, target, current_time);

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!is_queue_full()) {
                order_queue.push(order);
            }
            else {
                return false;
            }
        }

        return process_spell_order(order);
    }

    bool OrderManager::cast_spell(int spell_slot, const math::vector3& position)
    {
        auto player = g_sdk->object_manager->get_local_player();
        if (!player || !can_cast(player, spell_slot)) return false;

        float current_time = g_sdk->clock_facade->get_game_time();
        SpellOrder order(spell_slot, position, current_time);

        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            if (!is_queue_full()) {
                order_queue.push(order);
            }
            else {
                return false;
            }
        }

        return process_spell_order(order);
    }

    float OrderManager::get_last_cast_time(int spell_slot) const
    {
        if (spell_slot >= 0 && spell_slot < 4) {
            return last_cast_times[spell_slot];
        }
        return 0.f;
    }

    size_t OrderManager::get_pending_orders() const
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        return order_queue.size();
    }

    void OrderManager::clear_queue()
    {
        std::lock_guard<std::mutex> lock(queue_mutex);
        while (!order_queue.empty()) {
            order_queue.pop();
        }
    }
}