#pragma once
#include "sdk.hpp"
#include <array>

namespace vengaboys
{
    enum class OrderPriority {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    class OrderManager
    {
    public:
        static OrderManager& get_instance();

        // Core functionality
        bool cast_spell(int spell_slot, game_object* target, OrderPriority priority = OrderPriority::Normal);
        bool cast_spell(int spell_slot, const math::vector3& position, OrderPriority priority = OrderPriority::Normal);
        bool cast_spell(int spell_slot, const math::vector3& start_position, const math::vector3& end_position, OrderPriority priority = OrderPriority::Normal);
        bool move_to(const math::vector3& position, OrderPriority priority = OrderPriority::Low);
        bool attack(game_object* target, OrderPriority priority = OrderPriority::Normal);

        // Time-based order management
        float get_last_cast_time(int spell_slot) const;
        bool is_issue_order_passed(const int spell_slot, const float value);
        bool can_cast(game_object* source, int spell_slot, float delay = 0.f);

        // Configuration methods
        void set_min_order_delay(float delay) { min_order_delay = delay; }
        void set_cooldown_after_ult(float cooldown) { post_ult_cooldown = cooldown; }
        void set_max_queue_size(size_t size) { /* No longer needed */ }
        void set_max_orders_per_second(int max_rate) { /* No longer needed */ }

        // For backward compatibility
        void clear_queue() { /* No-op */ }
        bool is_throttled() const { return false; }
        void reset_throttle() { /* No-op */ }
        size_t get_pending_orders() const { return 0; }

    private:
        OrderManager();
        OrderManager(const OrderManager&) = delete;
        OrderManager& operator=(const OrderManager&) = delete;

        std::array<float, 6> last_cast_times{}; // 0-3 for spells, 4-5 for summoners
        float last_move_time = 0.f;
        float last_attack_time = 0.f;
        float min_order_delay = 0.05f;
        float post_ult_cooldown = 0.5f;
    };
}