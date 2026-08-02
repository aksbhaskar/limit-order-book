#pragma once

#include <cstddef>
#include <deque>

#include "lob/order.hpp"
#include "lob/price.hpp"
#include "lob/quantity.hpp"

namespace lob {

// All resting orders that share a single price, held in first-in-first-out
// (time priority) order: the oldest order is at the front and has the highest
// priority. The order book guarantees that orders are appended in
// non-decreasing sequence order, so back-insertion preserves time priority.
//
// This type stores and reports orders. The mutation needed for matching is
// added in a later milestone; here a level is only grown, never reduced.
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

private:
    Price price_;
    std::deque<Order> orders_;
    Quantity total_quantity_{};
};

}  // namespace lob
