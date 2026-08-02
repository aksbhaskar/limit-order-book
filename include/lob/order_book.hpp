#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <set>
#include <vector>

#include "lob/order.hpp"
#include "lob/price.hpp"
#include "lob/price_level.hpp"
#include "lob/quantity.hpp"
#include "lob/types.hpp"

namespace lob {

// A read-only snapshot of a single price level, used for depth inspection.
struct LevelView {
    Price price;
    Quantity quantity;         // aggregate remaining quantity at this level
    std::size_t order_count;   // number of resting orders at this level

    friend bool operator==(const LevelView&, const LevelView&) = default;
};

// A price-time-priority limit order book.
//
// Bids and asks are kept on separate sides. Price priority: the best bid is the
// highest-priced buy, the best ask is the lowest-priced sell. Within one price
// level orders keep first-in-first-out (time) priority by arrival sequence.
//
// Each side is a std::map keyed by price with a side-specific comparator, so
// begin() is always the best level and iteration walks levels from best to
// worst in O(1) / in-order time. This makes the structure fully deterministic.
//
// This class stores and reports resting orders, and exposes to the matching
// engine (a friend) the single reduction primitive matching needs. Matching
// policy itself lives in MatchingEngine; cancellation and amendment are absent.
class OrderBook {
public:
    // Adds a resting limit order. Throws std::invalid_argument if the order is
    // not a limit order, or if its id is already present in the book. Price,
    // quantity, and id validity are already guaranteed by Order itself.
    void add_order(const Order& order);

    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

    // Best level on each side, or nullptr when that side is empty.
    const PriceLevel* best_bid() const noexcept;
    const PriceLevel* best_ask() const noexcept;

    // The level at an exact price on the given side, or nullptr if none exists.
    const PriceLevel* level_at(Side side, Price price) const;

    // Number of distinct price levels on a side.
    std::size_t level_count(Side side) const noexcept;

    // Depth snapshot: up to max_levels levels from the best price outward, in
    // priority order (best first).
    std::vector<LevelView> depth(Side side, std::size_t max_levels) const;

private:
    // Bids: highest price first. Asks: lowest price first.
    using BidMap = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskMap = std::map<Price, PriceLevel, std::less<Price>>;

    // Fills the FIFO-front order at the best level of `side` by `qty`, removing
    // the order if it becomes fully filled and the level if it becomes empty.
    // Precondition: that side is non-empty and qty <= front's remaining. Used by
    // MatchingEngine to consume resting liquidity.
    void reduce_best(Side side, Quantity qty);

    friend class MatchingEngine;

    BidMap bids_;
    AskMap asks_;
    std::set<OrderId> ids_;   // resting order ids, for duplicate detection
};

}  // namespace lob
