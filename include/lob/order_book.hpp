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

// Outcome of a cancellation request.
enum class CancelStatus : std::uint8_t {
    Cancelled,      // the order was resting and has been removed
    NotFound,       // no such order is (or ever was) resting in the book
    AlreadyFilled,  // the order rested but has since been fully filled
};

struct CancelResult {
    OrderId order_id{};
    CancelStatus status = CancelStatus::NotFound;
    Quantity cancelled_quantity{};   // remaining qty removed, valid iff Cancelled

    bool ok() const noexcept { return status == CancelStatus::Cancelled; }

    friend bool operator==(const CancelResult&, const CancelResult&) = default;
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
// This class stores and reports resting orders, cancels them by id, and exposes
// to the matching engine (a friend) the reduction primitive matching needs.
// Matching policy itself lives in MatchingEngine; amendment is absent.
//
// An id index maps each live order to the (side, price) of the level holding it,
// so cancellation locates an order without scanning the whole book. Ids of
// orders that have been fully filled are remembered so that a cancel for them
// can be reported distinctly from a cancel for an id that was never seen.
class OrderBook {
public:
    // Adds a resting limit order. Throws std::invalid_argument if the order is
    // not a limit order, or if its id is already present in the book. Price,
    // quantity, and id validity are already guaranteed by Order itself.
    void add_order(const Order& order);

    // Cancels a resting order by id, removing it from its level (and removing
    // the level if it becomes empty) and updating aggregate quantity. Reports
    // Cancelled with the removed quantity, or NotFound / AlreadyFilled. Never
    // throws for an absent order. Time priority of remaining orders is preserved.
    CancelResult cancel(OrderId id);

    bool empty() const noexcept { return bids_.empty() && asks_.empty(); }

    // Best level on each side, or nullptr when that side is empty.
    const PriceLevel* best_bid() const noexcept;
    const PriceLevel* best_ask() const noexcept;

    // The level at an exact price on the given side, or nullptr if none exists.
    const PriceLevel* level_at(Side side, Price price) const;

    // Number of distinct price levels on a side.
    std::size_t level_count(Side side) const noexcept;

    // All price levels on a side, in priority order (best first). Pointers are
    // valid until the book is next modified. Useful for full-state inspection
    // and snapshotting.
    std::vector<const PriceLevel*> levels(Side side) const;

    // Depth snapshot: up to max_levels levels from the best price outward, in
    // priority order (best first).
    std::vector<LevelView> depth(Side side, std::size_t max_levels) const;

private:
    // Bids: highest price first. Asks: lowest price first.
    using BidMap = std::map<Price, PriceLevel, std::greater<Price>>;
    using AskMap = std::map<Price, PriceLevel, std::less<Price>>;

    // Where a live order rests: which side and at which price level.
    struct Locator {
        Side side{};
        Price price{};
    };

    // Fills the FIFO-front order at the best level of `side` by `qty`, removing
    // the order if it becomes fully filled and the level if it becomes empty.
    // Precondition: that side is non-empty and qty <= front's remaining. Used by
    // MatchingEngine to consume resting liquidity.
    void reduce_best(Side side, Quantity qty);

    friend class MatchingEngine;

    BidMap bids_;
    AskMap asks_;
    std::map<OrderId, Locator> live_;   // resting order id -> its level location
    std::set<OrderId> filled_;          // ids of orders fully filled while resting
};

}  // namespace lob
