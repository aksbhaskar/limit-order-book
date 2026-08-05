#include <doctest/doctest.h>

#include <string>

#include "lob/backtest.hpp"
#include "lob/inventory_market_maker.hpp"
#include "lob/market_maker.hpp"

using namespace lob;

namespace {

SimConfig market(std::int64_t txn_cost = 0) {
    SimConfig c;
    c.steps = 4000;
    c.seed = 123;
    c.initial_mid = Price::from_units(100);
    c.mid_tick_step = 10;
    c.trade_permille = 800;
    c.max_aggressor_qty = 6;
    c.starting_cash_ticks = 100'000 * Price::kTicksPerUnit;
    c.transaction_cost_ticks = txn_cost;
    return c;
}

MarketMakerConfig fixed_cfg(std::int64_t max_inv = 50) {
    MarketMakerConfig m;
    m.spread = Price::from_ticks(100);
    m.max_inventory = max_inv;
    m.order_quantity = Quantity{5};
    return m;
}

InventoryAwareConfig inv_cfg(std::int64_t max_inv = 50) {
    InventoryAwareConfig c;
    c.base_spread = Price::from_ticks(100);
    c.max_inventory = max_inv;
    c.order_quantity = Quantity{5};
    c.skew_ticks_per_unit = 2;
    c.widen_ticks_per_unit = 1;
    return c;
}

}  // namespace

TEST_CASE("Backtest metrics are internally consistent") {
    FixedSpreadMarketMaker mm(fixed_cfg());
    const BacktestMetrics m = Backtester(market()).run(mm);

    CHECK(m.strategy == "fixed-spread");
    CHECK(m.total_pnl == doctest::Approx(m.realized_pnl + m.unrealized_pnl));
    CHECK(m.trade_count > 0);
    CHECK(m.fill_rate >= 0.0);
    CHECK(m.fill_rate <= 1.0);
    CHECK(m.max_drawdown >= 0.0);
    CHECK(m.max_drawdown <= 1.0);
    CHECK(m.volatility >= 0.0);
    CHECK(m.max_abs_inventory <= 50);
}

TEST_CASE("A flat, tradeless run has zero P&L, volatility, Sharpe and drawdown") {
    SimConfig cfg = market();
    cfg.trade_permille = 0;   // no trades ever
    FixedSpreadMarketMaker mm(fixed_cfg());
    const BacktestMetrics m = Backtester(cfg).run(mm);

    CHECK(m.trade_count == 0);
    CHECK(m.total_pnl == doctest::Approx(0.0));
    CHECK(m.realized_pnl == doctest::Approx(0.0));
    CHECK(m.volatility == doctest::Approx(0.0));
    CHECK(m.sharpe == doctest::Approx(0.0));
    CHECK(m.max_drawdown == doctest::Approx(0.0));
    CHECK(m.average_inventory == doctest::Approx(0.0));
    CHECK(m.max_abs_inventory == 0);
}

TEST_CASE("Transaction costs reduce total P&L") {
    FixedSpreadMarketMaker mm_free(fixed_cfg());
    FixedSpreadMarketMaker mm_paid(fixed_cfg());

    const BacktestMetrics free_run = Backtester(market(/*txn_cost=*/0)).run(mm_free);
    const BacktestMetrics paid_run = Backtester(market(/*txn_cost=*/5)).run(mm_paid);

    REQUIRE(free_run.trade_count > 0);
    // Same market and quotes, but fees are charged: P&L must be strictly lower.
    CHECK(paid_run.total_pnl < free_run.total_pnl);
    CHECK(paid_run.trade_count == free_run.trade_count);   // costs do not change fills
}

TEST_CASE("Backtests are deterministic") {
    FixedSpreadMarketMaker a(fixed_cfg());
    FixedSpreadMarketMaker b(fixed_cfg());
    const BacktestMetrics ra = Backtester(market()).run(a);
    const BacktestMetrics rb = Backtester(market()).run(b);

    CHECK(ra.total_pnl == doctest::Approx(rb.total_pnl));
    CHECK(ra.sharpe == doctest::Approx(rb.sharpe));
    CHECK(ra.trade_count == rb.trade_count);
    CHECK(ra.max_abs_inventory == rb.max_abs_inventory);
}

TEST_CASE("Two strategies can be compared on identical market conditions") {
    const SimConfig cfg = market();

    FixedSpreadMarketMaker fixed(fixed_cfg());
    InventoryAwareMarketMaker inventory(inv_cfg());

    const Backtester bt(cfg);
    const BacktestMetrics fixed_m = bt.run(fixed);
    const BacktestMetrics inv_m = bt.run(inventory);

    CHECK(fixed_m.strategy == "fixed-spread");
    CHECK(inv_m.strategy == "inventory-aware");
    CHECK(fixed_m.trade_count > 0);
    CHECK(inv_m.trade_count > 0);

    // The inventory-aware maker is designed to hold less inventory; on this
    // market it should not carry a larger maximum absolute inventory.
    CHECK(inv_m.max_abs_inventory <= fixed_m.max_abs_inventory);
}

TEST_CASE("Results serialize to machine-readable JSON") {
    FixedSpreadMarketMaker mm(fixed_cfg());
    const BacktestMetrics m = Backtester(market()).run(mm);

    const std::string json = m.to_json();
    CHECK(json.front() == '{');
    CHECK(json.back() == '}');
    CHECK(json.find("\"strategy\":\"fixed-spread\"") != std::string::npos);
    CHECK(json.find("\"sharpe\":") != std::string::npos);
    CHECK(json.find("\"max_drawdown\":") != std::string::npos);

    const std::string arr = results_to_json({m, m});
    CHECK(arr.front() == '[');
    CHECK(arr.back() == ']');
}
