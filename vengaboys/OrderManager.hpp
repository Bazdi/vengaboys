#pragma once

#include "sdk.hpp"
#include <chrono>
#include <queue>
#include <mutex>

namespace vengaboys
{
    struct SpellOrder {
        int spell_slot;
        game_object* target;
        math::vector3 position;
        bool is_targeted;
        float time_issued;

        SpellOrder(int slot, game_object* tgt, float time)
            : spell_slot(slot), target(tgt), is_targeted(true), time_issued(time) {
        }

        SpellOrder(int slot, const math::vector3& pos, float time)
            : spell_slot(slot), position(pos), is_targeted(false), time_issued(time) {
        }
    };

    class OrderManager
    {
    public:
        static OrderManager& get_instance();

        bool can_cast(game_object* source, int spell_slot, float delay = 0.f);
        bool cast_spell(int spell_slot, game_object* target);
        bool cast_spell(int spell_slot, const math::vector3& position);

        void set_min_order_delay(float delay) { min_order_delay = delay; }
        void set_max_queue_size(size_t size) { max_queue_size = size; }

        float get_last_cast_time(int spell_slot) const;
        size_t get_pending_orders() const;
        void clear_queue();

    private:
        OrderManager();
        OrderManager(const OrderManager&) = delete;
        OrderManager& operator=(const OrderManager&) = delete;

        bool process_spell_order(const SpellOrder& order);
        void update_cast_time(int spell_slot);
        bool validate_spell_cast(game_object* source, int spell_slot, float delay) const;

        std::array<float, 4> last_cast_times{};
        float last_order_time = 0.f;
        float min_order_delay = 0.05f;      

        std::queue<SpellOrder> order_queue;
        size_t max_queue_size = 3;
        mutable std::mutex queue_mutex;

        bool is_queue_full() const { return order_queue.size() >= max_queue_size; }
        void cleanup_old_orders(float current_time);
    };
}