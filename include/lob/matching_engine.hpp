#pragma once

#include <vector>

#include "lob/order.hpp"
#include "lob/order_book.hpp"
#include "lob/quantity.hpp"
#include "lob/trade.hpp"

namespace lob {

// The outcome of submitting an order to the matching engine.
struct SubmitResult {
    OrderId order_id{};                 // the submitted order's id
    std::vector<Trade> trades;          // executions, in the order they occurred
    Quantity filled_quantity{};         // total quantity that traded
    Quantity remaining_quantity{};      // quantity left after matching
    bool resting = false;               // remainder was added to the book

    // True when the whole order traded and nothing was left to rest.
    bool fully_filled() const noexcept { return remaining_quantity.is_zero(); }
};

// A price-time-priority matching engine built on top of OrderBook.
//
// An incoming order is matched against the best levels of the opposite side,
// consuming resting orders in strict price-then-time priority. Each fill
// executes at the resting (maker) order's price. The two order types differ only
// in when matching stops and what happens to any unfilled quantity:
//
//   * Limit  - matches while the prices cross; any remainder rests on the book.
//   * Market - matches against whatever liquidity exists regardless of price and
//              never rests; if liquidity runs out, the remainder is dropped.
//
// The engine holds no other state, so behaviour is fully deterministic. Amend /
// replace and higher-level strategies are not implemented. Resting orders may be
// cancelled by id.
class MatchingEngine {
public:
    // Submits an order: matches it against the book and rests any remainder.
    // Returns the resulting trades and fill/remaining accounting.
    SubmitResult submit(Order order);

    // Cancels a resting order by id. See OrderBook::cancel for the semantics.
    CancelResult cancel(OrderId id) { return book_.cancel(id); }

    // Read-only access to the underlying book (for inspection and testing).
    const OrderBook& book() const noexcept { return book_; }

private:
    OrderBook book_;
};

}  // namespace lob
