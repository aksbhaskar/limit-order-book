#pragma once

#include <vector>

#include "lob/order_book.hpp"
#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/types.hpp"

namespace lob {

// A deterministic, comparable snapshot of full order-book state.
//
// It captures every level (in priority order) and, within each level, every
// resting order (in FIFO order) with the identity that matters for equivalence:
// id, remaining quantity, and arrival sequence. Two books that are equal in
// price priority, time priority, and resting quantity produce equal snapshots,
// which is exactly what "the book after replay matches the book before" means.
struct OrderSnapshot {
    OrderId id{};
    Quantity remaining{};
    Sequence sequence{};

    friend bool operator==(const OrderSnapshot&, const OrderSnapshot&) = default;
};

struct LevelSnapshot {
    Price price{};
    Quantity total_quantity{};
    std::vector<OrderSnapshot> orders;

    friend bool operator==(const LevelSnapshot&, const LevelSnapshot&) = default;
};

struct BookSnapshot {
    std::vector<LevelSnapshot> bids;   // best (highest) first
    std::vector<LevelSnapshot> asks;   // best (lowest) first

    friend bool operator==(const BookSnapshot&, const BookSnapshot&) = default;

    // Captures the current state of `book`.
    static BookSnapshot of(const OrderBook& book);
};

}  // namespace lob
