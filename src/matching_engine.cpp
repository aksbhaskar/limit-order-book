#include "lob/matching_engine.hpp"

#include <algorithm>

#include "lob/price_level.hpp"
#include "lob/types.hpp"

namespace lob {

namespace {

// Does a taker order priced at `taker_price` cross a resting level at
// `resting_price` on the opposite side?
bool crosses(Side taker_side, Price taker_price, Price resting_price) noexcept {
    return taker_side == Side::Buy ? taker_price >= resting_price
                                   : taker_price <= resting_price;
}

}  // namespace

SubmitResult MatchingEngine::submit(Order order) {
    SubmitResult result;
    result.order_id = order.id();

    const Quantity original = order.quantity();
    const Side opposite = order.side() == Side::Buy ? Side::Sell : Side::Buy;

    // Consume resting liquidity while the order still has quantity and the best
    // opposite level is priced such that the two cross.
    while (!order.is_filled()) {
        const PriceLevel* best =
            opposite == Side::Sell ? book_.best_ask() : book_.best_bid();
        if (best == nullptr || !crosses(order.side(), order.price(), best->price())) {
            break;
        }

        // Price priority is handled by best-level selection; time priority by
        // always taking the front (oldest) order at that level.
        const Order& maker = best->front();
        const Quantity exec =
            std::min(order.remaining_quantity(), maker.remaining_quantity());

        result.trades.push_back(Trade{
            /*maker_id=*/maker.id(),
            /*taker_id=*/order.id(),
            /*price=*/maker.price(),          // trade at the resting/maker price
            /*quantity=*/exec,
            /*maker_sequence=*/maker.sequence(),
            /*taker_sequence=*/order.sequence(),
        });

        order.fill(exec);
        book_.reduce_best(opposite, exec);    // may pop the maker / empty level
    }

    result.remaining_quantity = order.remaining_quantity();
    result.filled_quantity = original - order.remaining_quantity();

    // Whatever is left rests as a passive limit order.
    if (!order.is_filled()) {
        book_.add_order(order);
        result.resting = true;
    }

    return result;
}

}  // namespace lob
