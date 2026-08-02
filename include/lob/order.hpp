#pragma once

#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/types.hpp"

namespace lob {

// A single resting/incoming order.
//
// This is the foundational data structure the order book and matching engine are
// built on. Its identifying fields (id, side, type, price, quantity, sequence)
// are fixed at construction; only the remaining quantity changes, and only
// through fill(), as the order executes. It enforces its own invariants but
// contains no matching or booking policy — that lives in OrderBook and
// MatchingEngine.
//
// Invariants enforced at construction (a violation throws std::invalid_argument):
//   * id       != OrderId{0}          (0 is the reserved "invalid" id)
//   * price    is strictly positive   (limit orders must name a positive price)
//   * quantity is strictly positive   (an order for nothing is meaningless)
// After construction the following always hold:
//   * remaining_quantity() <= quantity()
//   * filled_quantity() + remaining_quantity() == quantity()
class Order {
public:
    // Constructs a fully-open order (remaining == quantity). Throws
    // std::invalid_argument if any invariant above is violated.
    Order(OrderId id,
          Side side,
          OrderType type,
          Price price,
          Quantity quantity,
          Sequence sequence);

    constexpr OrderId id() const noexcept { return id_; }
    constexpr Side side() const noexcept { return side_; }
    constexpr OrderType type() const noexcept { return type_; }
    constexpr Price price() const noexcept { return price_; }
    constexpr Sequence sequence() const noexcept { return sequence_; }

    // The original order size.
    constexpr Quantity quantity() const noexcept { return quantity_; }

    // The as-yet-unfilled size. Equal to quantity() for a freshly created order.
    constexpr Quantity remaining_quantity() const noexcept { return remaining_; }

    // The size filled so far: quantity() - remaining_quantity().
    constexpr Quantity filled_quantity() const noexcept {
        return quantity_ - remaining_;
    }

    // True once nothing is left to fill.
    constexpr bool is_filled() const noexcept { return remaining_.is_zero(); }

    // Applies a (partial or full) fill, reducing the remaining quantity by
    // `quantity`. Throws std::invalid_argument if `quantity` is zero or exceeds
    // the remaining quantity. The original quantity() is unchanged, preserving
    // the invariant filled_quantity() + remaining_quantity() == quantity().
    void fill(Quantity quantity);

private:
    OrderId id_{};
    Side side_{};
    OrderType type_{};
    Price price_{};
    Quantity quantity_{};
    Quantity remaining_{};
    Sequence sequence_{};
};

}  // namespace lob
