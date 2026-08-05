#include "lob/inventory_market_maker.hpp"

#include <cstdlib>

namespace lob {

QuoteDecision InventoryAwareMarketMaker::quote(const MarketState& state) {
    QuoteDecision decision;

    const std::int64_t inv = state.inventory;
    const std::int64_t half = config_.base_spread.ticks() / 2;
    // Widen as risk grows; shift the centre against the inventory.
    const std::int64_t half_eff = half + std::llabs(inv) * config_.widen_ticks_per_unit;
    const std::int64_t reservation = state.mid.ticks() - inv * config_.skew_ticks_per_unit;
    const std::int64_t bid_ticks = reservation - half_eff;
    const std::int64_t ask_ticks = reservation + half_eff;

    const QuoteRoom room =
        inventory_room(inv, config_.max_inventory, config_.order_quantity);

    if (room.bid > 0 && bid_ticks > 0) {
        decision.quote_bid = true;
        decision.bid_price = Price::from_ticks(bid_ticks);
        decision.bid_quantity = Quantity{room.bid};
    }
    if (room.ask > 0 && ask_ticks > 0) {
        decision.quote_ask = true;
        decision.ask_price = Price::from_ticks(ask_ticks);
        decision.ask_quantity = Quantity{room.ask};
    }
    return decision;
}

}  // namespace lob
