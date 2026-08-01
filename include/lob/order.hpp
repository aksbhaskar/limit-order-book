#pragma once

#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/types.hpp"

namespace lob {

// An immutable description of a single resting/incoming order.
//
// This is the foundational data structure the order book and matching engine
// will be built on. It only *represents* an order and enforces its invariants;
// it contains no matching, booking, or lifecycle logic (those arrive in later
// milestones). The remaining-quantity field and sequence number are carried now
// so that price-time priority and partial fills can be layered on without
// changing this type's shape.
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
