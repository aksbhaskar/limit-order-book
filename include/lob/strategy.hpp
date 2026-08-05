#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

#include "lob/price.hpp"
#include "lob/quantity.hpp"

namespace lob {

// What a strategy observes each step before deciding how to quote.
struct MarketState {
    Price mid;                        // reference mid price
    std::optional<Price> best_bid;    // current book best bid, if any
    std::optional<Price> best_ask;    // current book best ask, if any
    std::int64_t inventory = 0;       // the strategy's signed inventory
    std::uint64_t step = 0;           // simulation step index
};

// A strategy's desired two-sided quote for this step. Either side may be
// disabled (e.g. suppressed by a risk limit).
struct QuoteDecision {
    bool quote_bid = false;
    Price bid_price{};
    Quantity bid_quantity{};
    bool quote_ask = false;
    Price ask_price{};
    Quantity ask_quantity{};
};

// The interface every market-making strategy implements. It only decides how to
// quote from observed state; order submission, matching, and accounting are done
// by the simulator/backtester via the existing engine APIs.
class Strategy {
public:
    virtual ~Strategy() = default;

    virtual QuoteDecision quote(const MarketState& state) = 0;
    virtual std::string name() const = 0;
};

// Shared helper: how much a strategy may still buy / sell without breaching a
// symmetric inventory limit of +/- max_inventory, capped at order_quantity.
// Returns quantities (possibly zero) for the bid and ask sides.
struct QuoteRoom {
    std::uint64_t bid;
    std::uint64_t ask;
};

inline QuoteRoom inventory_room(std::int64_t inventory,
                                std::int64_t max_inventory,
                                Quantity order_quantity) noexcept {
    const std::int64_t oq = static_cast<std::int64_t>(order_quantity.value());
    const std::int64_t bid_room = std::max<std::int64_t>(0, max_inventory - inventory);
    const std::int64_t ask_room = std::max<std::int64_t>(0, max_inventory + inventory);
    return QuoteRoom{static_cast<std::uint64_t>(std::min(oq, bid_room)),
                     static_cast<std::uint64_t>(std::min(oq, ask_room))};
}

}  // namespace lob
