#include <doctest/doctest.h>

#include <cstdlib>

#include "lob/market_maker.hpp"
#include "lob/market_maker_simulator.hpp"

using namespace lob;

namespace {

SimConfig base_config() {
    SimConfig c;
    c.steps = 3000;
    c.seed = 7;
    c.initial_mid = Price::from_units(100);
    c.mid_volatility_ticks = 10;
    c.order_arrival_permille = 800;
    c.liquidity_reach_ticks = 80;
    c.max_aggressor_qty = 6;
    c.starting_cash_ticks = 1'000'000;
    return c;
}

MarketMakerConfig mm_config(std::int64_t max_inv = 50) {
    MarketMakerConfig m;
    m.spread = Price::from_ticks(100);
    m.max_inventory = max_inv;
    m.order_quantity = Quantity{5};
    return m;
}

}  // namespace

TEST_CASE("The simulation is deterministic for a given seed") {
    const SimConfig cfg = base_config();

    FixedSpreadMarketMaker mm1(mm_config());
    FixedSpreadMarketMaker mm2(mm_config());
    const SimResult a = MarketMakerSimulator(cfg).run(mm1);
    const SimResult b = MarketMakerSimulator(cfg).run(mm2);

    CHECK(a == b);
    CHECK(a.steps.size() == cfg.steps);
}

TEST_CASE("The strategy trades and gets filled on both sides over a run") {
    FixedSpreadMarketMaker mm(mm_config());
    const SimResult r = MarketMakerSimulator(base_config()).run(mm);

    CHECK(r.fills > 0);
    CHECK(r.filled_quantity > 0);

    bool bought = false;   // inventory rose on a fill step (our bid hit)
    bool sold = false;     // inventory fell on a fill step (our ask lifted)
    for (std::size_t i = 1; i < r.steps.size(); ++i) {
        if (r.steps[i].fills == 0) {
            continue;
        }
        if (r.steps[i].inventory > r.steps[i - 1].inventory) {
            bought = true;
        } else if (r.steps[i].inventory < r.steps[i - 1].inventory) {
            sold = true;
        }
    }
    CHECK(bought);
    CHECK(sold);
}

TEST_CASE("Inventory never breaches the configured limit") {
    const std::int64_t max_inv = 40;
    FixedSpreadMarketMaker mm(mm_config(max_inv));
    const SimResult r = MarketMakerSimulator(base_config()).run(mm);

    for (const StepRecord& s : r.steps) {
        CHECK(std::llabs(s.inventory) <= max_inv);
    }
}

TEST_CASE("Cash and starting cash are tracked, and the P&L identity holds") {
    SimConfig cfg = base_config();
    FixedSpreadMarketMaker mm(mm_config());
    const SimResult r = MarketMakerSimulator(cfg).run(mm);

    // Starting cash flows through to the reported cash figure.
    CHECK(r.final_cash_ticks != cfg.starting_cash_ticks);   // trading moved cash
    for (const StepRecord& s : r.steps) {
        CHECK((s.realized_ticks + s.unrealized_ticks) == s.total_pnl_ticks);
    }
    CHECK((r.final_realized_ticks + r.final_unrealized_ticks) ==
          r.final_total_pnl_ticks);
}

TEST_CASE("Fill rate is a fraction in [0, 1]") {
    FixedSpreadMarketMaker mm(mm_config());
    const SimResult r = MarketMakerSimulator(base_config()).run(mm);
    CHECK(r.fill_rate() >= 0.0);
    CHECK(r.fill_rate() <= 1.0);
    CHECK(r.quotes_placed > 0);
}

TEST_CASE("With no aggressor flow the strategy stays flat and flat P&L") {
    SimConfig cfg = base_config();
    cfg.order_arrival_permille = 0;   // no counterparties ever arrive
    FixedSpreadMarketMaker mm(mm_config());
    const SimResult r = MarketMakerSimulator(cfg).run(mm);

    CHECK(r.fills == 0);
    CHECK(r.final_inventory == 0);
    CHECK(r.final_realized_ticks == 0);
    CHECK(r.final_total_pnl_ticks == 0);
    for (const StepRecord& s : r.steps) {
        CHECK(s.inventory == 0);
    }
}

TEST_CASE("A tiny inventory limit forces one-sided quoting at the cap") {
    SimConfig cfg = base_config();
    FixedSpreadMarketMaker mm(mm_config(/*max_inv=*/5));
    const SimResult r = MarketMakerSimulator(cfg).run(mm);

    // A step quotes from the previous step's inventory. When that was pinned at
    // +/- the cap, the risk-increasing side must be suppressed.
    for (std::size_t i = 1; i < r.steps.size(); ++i) {
        const std::int64_t prev = r.steps[i - 1].inventory;
        if (prev == 5) {
            CHECK_FALSE(r.steps[i].quoted_bid);
        }
        if (prev == -5) {
            CHECK_FALSE(r.steps[i].quoted_ask);
        }
    }
}
