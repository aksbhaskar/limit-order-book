#include "lob/order.hpp"

#include <stdexcept>

namespace lob {

Order::Order(OrderId id,
             Side side,
             OrderType type,
             Price price,
             Quantity quantity,
             Sequence sequence)
    : id_(id),
      side_(side),
      type_(type),
      price_(price),
      quantity_(quantity),
      remaining_(quantity),
      sequence_(sequence) {
    if (id_ == OrderId{0}) {
        throw std::invalid_argument("Order id must be non-zero");
    }
    if (!price_.is_positive()) {
        throw std::invalid_argument("Order price must be strictly positive");
    }
    if (quantity_.is_zero()) {
        throw std::invalid_argument("Order quantity must be strictly positive");
    }
}

void Order::fill(Quantity quantity) {
    if (quantity.is_zero()) {
        throw std::invalid_argument("fill quantity must be strictly positive");
    }
    if (quantity > remaining_) {
        throw std::invalid_argument("fill quantity exceeds remaining quantity");
    }
    remaining_ = remaining_ - quantity;
}

}  // namespace lob
