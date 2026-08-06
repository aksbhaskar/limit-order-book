#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lob/market_maker_simulator.hpp"

namespace lob {

// Which market-making strategy a study cell used.
enum class StrategyKind {
    FixedSpread,
    InventoryAware,
};

std::string to_string(StrategyKind kind);

// One market-maker parameter configuration (a point in the grid).
struct ParamPoint {
    std::int64_t spread_ticks = 100;
    std::uint64_t order_quantity = 5;
    std::int64_t max_inventory = 50;
    std::int64_t skew_ticks_per_unit = 2;      // used by inventory-aware only
    std::int64_t transaction_cost_ticks = 0;

    friend bool operator==(const ParamPoint&, const ParamPoint&) = default;
};

// Axes to sweep; the grid is their Cartesian product.
struct ParamGridSpec {
    std::vector<std::int64_t> spreads{100};
    std::vector<std::uint64_t> order_quantities{5};
    std::vector<std::int64_t> max_inventories{50};
    std::vector<std::int64_t> skews{2};
    std::vector<std::int64_t> transaction_costs{0};
};

// Builds the Cartesian product of the axes, in a fixed, deterministic order.
std::vector<ParamPoint> generate_grid(const ParamGridSpec& spec);

// Distribution summary of a sample, including a 95% confidence interval of the
// mean (normal approximation, mean +/- 1.96 * stddev / sqrt(n)). The standard
// deviation is the sample (n-1) estimator.
struct Aggregate {
    std::size_t n = 0;
    double mean = 0.0;
    double median = 0.0;
    double stddev = 0.0;
    double ci95_low = 0.0;
    double ci95_high = 0.0;

    static Aggregate of(std::vector<double> samples);

    friend bool operator==(const Aggregate&, const Aggregate&) = default;
};

// One individual backtest run (a single seed for one strategy/parameter point).
// These raw samples back the per-seed distribution figures.
struct RunRecord {
    StrategyKind strategy = StrategyKind::FixedSpread;
    ParamPoint params;
    std::uint64_t seed = 0;

    double total_pnl = 0.0;
    double sharpe = 0.0;
    double max_drawdown = 0.0;
    double fill_rate = 0.0;
    double average_inventory = 0.0;
    std::int64_t max_abs_inventory = 0;
    std::uint64_t trade_count = 0;
};

// Aggregated results for one (strategy, parameter point) over all seeds.
struct StudyCell {
    StrategyKind strategy = StrategyKind::FixedSpread;
    ParamPoint params;
    std::size_t seeds = 0;

    Aggregate pnl;                // total P&L per run
    Aggregate sharpe;             // per-step Sharpe per run
    Aggregate max_drawdown;
    Aggregate fill_rate;
    Aggregate avg_inventory;
    Aggregate max_abs_inventory;
    Aggregate trade_count;
};

// The full experiment.
struct StudyConfig {
    SimConfig market;                        // base market (seed is overridden per run)
    std::vector<std::uint64_t> seeds{1, 2, 3, 4, 5};
    ParamGridSpec grid;
};

struct StudyResult {
    std::vector<StudyCell> cells;   // one per (parameter point, strategy)
    std::vector<RunRecord> runs;    // one per (parameter point, strategy, seed)

    std::string to_json() const;         // aggregated cells, JSON
    std::string to_csv() const;          // aggregated cells, CSV
    std::string runs_to_csv() const;     // raw per-seed runs, CSV
};

// Runs the experiment: for every parameter point, every strategy, and every seed,
// runs a backtest and aggregates the per-seed metrics into distributions. Both
// strategies use the *same* seed and market for each run, so the comparison is
// fair. Fully deterministic for a given StudyConfig.
StudyResult run_study(const StudyConfig& config);

}  // namespace lob
