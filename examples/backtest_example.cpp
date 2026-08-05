// Reproducible backtest comparing two market-making strategies on identical,
// deterministic simulated market conditions.
//
// The market is a toy synthetic (random-walk mid, coin-flip aggressor flow). The
// numbers below are simulated and say nothing about real-market performance.
// Running this program again with the same build reproduces the same results.

#include <iomanip>
#include <iostream>
#include <vector>

#include "lob/backtest.hpp"
#include "lob/inventory_market_maker.hpp"
#include "lob/market_maker.hpp"

using namespace lob;

namespace {

void print_row(const BacktestMetrics& m) {
    std::cout << std::left << std::setw(18) << m.strategy << std::right
              << std::fixed << std::setprecision(2)
              << std::setw(12) << m.total_pnl
              << std::setw(12) << m.realized_pnl
              << std::setw(12) << m.unrealized_pnl
              << std::setprecision(4)
              << std::setw(10) << m.return_pct
              << std::setw(10) << m.max_drawdown
              << std::setw(10) << m.sharpe
              << std::setw(9) << m.trade_count
              << std::setprecision(3)
              << std::setw(9) << m.fill_rate
              << std::setw(11) << m.average_inventory
              << std::setw(8) << m.max_abs_inventory << '\n';
}

}  // namespace

int main() {
    // Shared, deterministic market conditions for a fair comparison.
    SimConfig market;
    market.steps = 5000;
    market.seed = 42;
    market.initial_mid = Price::from_units(100);
    market.mid_tick_step = 10;
    market.trade_permille = 800;
    market.max_aggressor_qty = 6;
    market.starting_cash_ticks = 100'000 * Price::kTicksPerUnit;   // 100,000.0000
    market.transaction_cost_ticks = 2;                             // 0.0002 per unit

    const std::int64_t max_inventory = 50;
    const Quantity order_quantity{5};
    const Price spread = Price::from_ticks(100);   // 0.0100

    MarketMakerConfig fixed_cfg;
    fixed_cfg.spread = spread;
    fixed_cfg.max_inventory = max_inventory;
    fixed_cfg.order_quantity = order_quantity;

    InventoryAwareConfig inv_cfg;
    inv_cfg.base_spread = spread;
    inv_cfg.max_inventory = max_inventory;
    inv_cfg.order_quantity = order_quantity;
    inv_cfg.skew_ticks_per_unit = 2;
    inv_cfg.widen_ticks_per_unit = 1;

    FixedSpreadMarketMaker fixed_mm(fixed_cfg);
    InventoryAwareMarketMaker inventory_mm(inv_cfg);

    const Backtester backtester(market);
    std::vector<BacktestMetrics> results;
    results.push_back(backtester.run(fixed_mm));
    results.push_back(backtester.run(inventory_mm));

    std::cout << "SIMULATED results on a toy synthetic market -- not real-market "
                 "performance.\n";
    std::cout << "seed=" << market.seed << " steps=" << market.steps
              << " starting_cash=100000.0000 txn_cost=0.0002/unit\n\n";

    std::cout << std::left << std::setw(18) << "strategy" << std::right
              << std::setw(12) << "total" << std::setw(12) << "realized"
              << std::setw(12) << "unreal" << std::setw(10) << "return"
              << std::setw(10) << "maxDD" << std::setw(10) << "sharpe"
              << std::setw(9) << "trades" << std::setw(9) << "fillR"
              << std::setw(11) << "avgInv" << std::setw(8) << "maxInv" << '\n';
    for (const BacktestMetrics& m : results) {
        print_row(m);
    }

    std::cout << "\nJSON:\n" << results_to_json(results) << '\n';
    return 0;
}
