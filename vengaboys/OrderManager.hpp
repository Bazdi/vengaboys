#pragma once
#include "sdk.hpp"
#include <array>
#include <functional>

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

        bool cast_spell(int spell_slot, game_object* target, OrderPriority priority = OrderPriority::Normal);
        bool cast_spell(int spell_slot, const math::vector3& position, OrderPriority priority = OrderPriority::Normal);
        bool cast_spell(int spell_slot, const math::vector3& start_position, const math::vector3& end_position, OrderPriority priority = OrderPriority::Normal);
        bool move_to(const math::vector3& position, OrderPriority priority = OrderPriority::Low);
        bool attack(game_object* target, OrderPriority priority = OrderPriority::Normal);
        bool update_chargeable_spell(int spell_slot, const math::vector3& position, bool release_cast = false);
        bool use_object(game_object* target);

        bool smart_attack(game_object* target, OrderPriority priority = OrderPriority::Normal);

        bool can_cast(game_object* source, int spell_slot, float delay = 0.f);
        bool is_target_valid(game_object* target, bool check_targetable = true);
        bool is_in_spell_range(game_object* source, game_object* target, int spell_slot, float extra_range = 0.f);
        bool is_in_attack_range(game_object* source, game_object* target, float extra_range = 0.f);

        float get_last_cast_time(int spell_slot) const;
        bool is_issue_order_passed(int spell_slot, float value);
        bool is_issue_order_passed(game_object_order order_type, float value);

        void block_orders(float duration);
        bool is_blocked() const;
        void allow_orders();

        void set_order_delay(float delay) { order_delay = delay; }
        void set_cooldown_after_ult(float cooldown) { post_ult_cooldown = cooldown; }
        void set_ping_compensation(bool enabled) { compensate_for_ping = enabled; }
        void set_debug_mode(bool enabled) { debug_mode = enabled; }

        void set_custom_validator(std::function<bool(game_object*, int)> validator) {
            custom_spell_validator = validator;
        }

        // Legacy methods (no longer used)
        void set_min_order_delay(float delay) { order_delay = delay; }
        void set_max_queue_size(size_t size) { /* no-op */ }
        void set_max_orders_per_second(int max_rate) { /* no-op */ }
        void clear_queue() { /* no-op */ }
        bool is_throttled() const { return false; }
        void reset_throttle() { /* no-op */ }
        size_t get_pending_orders() const { return 0; }

    private:
        OrderManager();
        OrderManager(const OrderManager&) = delete;
        OrderManager& operator=(const OrderManager&) = delete;

        void log_debug(const char* format, ...);
        float get_ping_delay() const;

        std::array<float, 64> last_spell_call{};
        std::array<float, 11> last_issue_order_call{};
        float last_use_object_call = 0.f;
        float order_delay = 0.066f;
        float post_ult_cooldown = 0.5f;
        float block_until_time = 0.f;
        bool compensate_for_ping = true;
        bool debug_mode = false;

        std::function<bool(game_object*, int)> custom_spell_validator = nullptr;
    };
}