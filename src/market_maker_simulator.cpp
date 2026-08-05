#include "lob/market_maker_simulator.hpp"

#include <cstdint>
#include <optional>
#include <random>

#include "lob/matching_engine.hpp"
#include "lob/order.hpp"
#include "lob/pnl_account.hpp"
#include "lob/quantity.hpp"
#include "lob/trade.hpp"
#include "lob/types.hpp"

namespace lob {

SimResult MarketMakerSimulator::run(Strategy& strategy) const {
    SimResult result;
    result.steps.reserve(config_.steps);

    MatchingEngine engine;
    PnLAccount account(config_.starting_cash_ticks);
    std::mt19937_64 rng(config_.seed);

    std::int64_t mid = config_.initial_mid.ticks();
    std::optional<OrderId> mm_bid;
    std::optional<OrderId> mm_ask;
    std::uint64_t next_id = 1;
    std::uint64_t next_seq = 1;

    for (std::uint64_t step = 0; step < config_.steps; ++step) {
        // 1. Reference mid takes a seeded random-walk step (kept positive).
        mid += ((rng() & 1u) != 0 ? 1 : -1) * config_.mid_tick_step;
        if (mid < config_.mid_tick_step) {
            mid = config_.mid_tick_step;
        }
        const Price mid_price = Price::from_ticks(mid);

        // 2. Cancel the previous step's quotes (no-op if already filled).
        if (mm_bid) {
            engine.cancel(*mm_bid);
            mm_bid.reset();
        }
        if (mm_ask) {
            engine.cancel(*mm_ask);
            mm_ask.reset();
        }

        // 3. Observe state and ask the strategy to quote.
        MarketState state;
        state.mid = mid_price;
        if (const PriceLevel* bb = engine.book().best_bid()) {
            state.best_bid = bb->price();
        }
        if (const PriceLevel* ba = engine.book().best_ask()) {
            state.best_ask = ba->price();
        }
        state.inventory = account.position();
        state.step = step;
        const QuoteDecision decision = strategy.quote(state);

        // 4. Place the new quotes.
        StepRecord record;
        record.step = step;
        record.mid_ticks = mid;
        if (decision.quote_bid && !decision.bid_quantity.is_zero()) {
            const OrderId id{next_id++};
            engine.submit(Order(id, Side::Buy, OrderType::Limit, decision.bid_price,
                                decision.bid_quantity, Sequence{next_seq++}));
            mm_bid = id;
            ++result.quotes_placed;
            record.quoted_bid = true;
            record.bid_ticks = decision.bid_price.ticks();
            record.bid_quantity = decision.bid_quantity.value();
        }
        if (decision.quote_ask && !decision.ask_quantity.is_zero()) {
            const OrderId id{next_id++};
            engine.submit(Order(id, Side::Sell, OrderType::Limit, decision.ask_price,
                                decision.ask_quantity, Sequence{next_seq++}));
            mm_ask = id;
            ++result.quotes_placed;
            record.quoted_ask = true;
            record.ask_ticks = decision.ask_price.ticks();
            record.ask_quantity = decision.ask_quantity.value();
        }

        // 5. A synthetic aggressor may arrive and cross one of the quotes.
        std::uint64_t step_fills = 0;
        std::uint64_t step_qty = 0;
        if ((rng() % 1000) < config_.trade_permille) {
            const bool aggressor_buys = (rng() & 1u) != 0;  // buy lifts the MM ask
            const std::uint64_t aq = 1 + rng() % config_.max_aggressor_qty;
            const Side aggr_side = aggressor_buys ? Side::Buy : Side::Sell;
            const OrderId id{next_id++};
            const SubmitResult sr = engine.submit(Order(
                id, aggr_side, OrderType::Market, Price{}, Quantity{aq}, Sequence{next_seq++}));

            for (const Trade& trade : sr.trades) {
                Side mm_side;
                if (mm_bid && trade.maker_id == *mm_bid) {
                    mm_side = Side::Buy;   // our bid was hit: we bought
                } else if (mm_ask && trade.maker_id == *mm_ask) {
                    mm_side = Side::Sell;  // our ask was lifted: we sold
                } else {
                    continue;              // not our order (should not happen)
                }
                account.apply_fill(mm_side, trade.price, trade.quantity);
                if (config_.transaction_cost_ticks != 0) {
                    account.charge_fee(config_.transaction_cost_ticks *
                                       static_cast<std::int64_t>(trade.quantity.value()));
                }
                ++step_fills;
                step_qty += trade.quantity.value();
                ++result.fills;
                result.filled_quantity += trade.quantity.value();
            }
        }

        // 6. Record the post-step portfolio state, marked at the current mid.
        record.inventory = account.position();
        record.cash_ticks = account.cash_ticks();
        record.realized_ticks = account.realized_ticks();
        record.unrealized_ticks = account.unrealized_ticks(mid_price);
        record.total_pnl_ticks = account.total_pnl_ticks(mid_price);
        record.fills = step_fills;
        record.filled_quantity = step_qty;
        result.steps.push_back(record);
    }

    const Price final_mid = Price::from_ticks(mid);
    result.final_inventory = account.position();
    result.final_cash_ticks = account.cash_ticks();
    result.final_realized_ticks = account.realized_ticks();
    result.final_unrealized_ticks = account.unrealized_ticks(final_mid);
    result.final_total_pnl_ticks = account.total_pnl_ticks(final_mid);
    return result;
}

}  // namespace lob
