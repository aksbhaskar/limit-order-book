#include "lob/market_maker.hpp"

namespace lob {

QuoteDecision FixedSpreadMarketMaker::quote(const MarketState& state) {
    QuoteDecision decision;

    const std::int64_t half = config_.spread.ticks() / 2;
    const std::int64_t mid = state.mid.ticks();
    const std::int64_t bid_ticks = mid - half;
    const std::int64_t ask_ticks = mid + half;

    const QuoteRoom room =
        inventory_room(state.inventory, config_.max_inventory, config_.order_quantity);

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
