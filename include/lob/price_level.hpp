#pragma once

#include <cstddef>
#include <deque>
#include <stdexcept>

#include "lob/order.hpp"
#include "lob/price.hpp"
#include "lob/quantity.hpp"

namespace lob {

// All resting orders that share a single price, held in first-in-first-out
// (time priority) order: the oldest order is at the front and has the highest
// priority. The order book guarantees that orders are appended in
// non-decreasing sequence order, so back-insertion preserves time priority.
//
// This type stores and reports orders, fills the front (oldest) order for the
// matching engine, and removes an arbitrary order by id for cancellation.
class PriceLevel {
public:
    explicit PriceLevel(Price price) noexcept : price_(price) {}

    Price price() const noexcept { return price_; }
    bool empty() const noexcept { return orders_.empty(); }
    std::size_t order_count() const noexcept { return orders_.size(); }

    // Aggregate remaining quantity resting at this level.
    Quantity total_quantity() const noexcept { return total_quantity_; }

    // The oldest order at this level (highest time priority).
    // Precondition: !empty().
    const Order& front() const { return orders_.front(); }

    // All resting orders in priority order (oldest first).
    const std::deque<Order>& orders() const noexcept { return orders_; }

    // Appends an order to the back of the FIFO queue.
    void add(const Order& order) {
        total_quantity_ = total_quantity_ + order.remaining_quantity();
        orders_.push_back(order);
    }

    // Outcome of reducing the front order.
    struct ReduceResult {
        bool order_completed = false;   // front order was fully filled & removed
        OrderId completed_id{};         // its id, valid iff order_completed
    };

    // Fills the front (oldest) order by `quantity`, updating the level's
    // aggregate quantity. If that order becomes fully filled it is popped and
    // its id is reported so callers can drop it from any index.
    // Precondition: !empty() and quantity <= front().remaining_quantity().
    ReduceResult reduce_front(Quantity quantity) {
        Order& head = orders_.front();
        total_quantity_ = total_quantity_ - quantity;
        head.fill(quantity);
        if (head.is_filled()) {
            const OrderId id = head.id();
            orders_.pop_front();
            return {true, id};
        }
        return {false, OrderId{}};
    }

    // Removes the order with `id` (a cancellation), subtracting its remaining
    // quantity from the level's aggregate. Returns that removed remaining
    // quantity on success. The relative FIFO order — and therefore the time
    // priority and sequence numbers — of every other order is preserved.
    // Throws std::logic_error if the id is not present at this level.
    Quantity remove(OrderId id) {
        for (auto it = orders_.begin(); it != orders_.end(); ++it) {
            if (it->id() == id) {
                const Quantity removed = it->remaining_quantity();
                total_quantity_ = total_quantity_ - removed;
                orders_.erase(it);
                return removed;
            }
        }
        throw std::logic_error("PriceLevel::remove: order id not at this level");
    }

private:
    Price price_;
    std::deque<Order> orders_;
    Quantity total_quantity_{};
};

}  // namespace lob
