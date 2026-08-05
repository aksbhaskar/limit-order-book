#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "lob/market_maker_simulator.hpp"
#include "lob/strategy.hpp"

namespace lob {

// Performance analytics for a single backtest run. Monetary fields are in
// currency units (ticks / Price::kTicksPerUnit); ratios are dimensionless.
//
// All figures are computed from the deterministic simulator's time series. They
// describe behaviour on the toy synthetic market only and must not be read as
// real-market performance. Return-based statistics are per simulation step
// (there is no wall-clock calendar), normalised by the starting cash; Sharpe is
// therefore a per-step, risk-free-zero ratio and is intentionally not annualised.
struct BacktestMetrics {
    std::string strategy;

    double total_pnl = 0.0;
    double realized_pnl = 0.0;
    double unrealized_pnl = 0.0;
    double return_pct = 0.0;         // total P&L / starting cash

    double max_drawdown = 0.0;       // largest peak-to-trough equity drop, fraction of peak
    double volatility = 0.0;         // std dev of per-step returns
    double sharpe = 0.0;             // mean / std dev of per-step returns (rf = 0)

    std::uint64_t trade_count = 0;
    double fill_rate = 0.0;
    double average_inventory = 0.0;  // mean signed inventory
    std::int64_t max_abs_inventory = 0;

    // Machine-readable single-object JSON.
    std::string to_json() const;
};

// Machine-readable JSON array combining several results (e.g. for comparison).
std::string results_to_json(const std::vector<BacktestMetrics>& results);

// Runs a strategy over a deterministic simulated market and reports analytics.
//
// The backtester wraps MarketMakerSimulator, so every strategy is executed
// through the same engine, accounting, and market generation. Because the market
// depends only on the SimConfig (including its seed and transaction cost), the
// same Backtester runs different strategies on identical conditions — the basis
// for a fair comparison.
class Backtester {
public:
    explicit Backtester(SimConfig market) : market_(market) {}

    BacktestMetrics run(Strategy& strategy) const;

    const SimConfig& market() const noexcept { return market_; }

private:
    SimConfig market_;
};

}  // namespace lob
