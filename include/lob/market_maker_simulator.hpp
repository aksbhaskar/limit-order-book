#pragma once

#include <cstdint>
#include <vector>

#include "lob/price.hpp"
#include "lob/strategy.hpp"

namespace lob {

// Broad market conditions with sensible default parameters (see regime_config).
enum class MarketRegime {
    Calm,         // low volatility, moderate flow
    Volatile,     // high volatility, moderate flow
    HighVolume,   // heavy, deep-reaching flow
};

// Configuration of the synthetic market the simulator drives. All randomness is
// seeded, so a given config produces exactly one deterministic run.
//
// The market is intentionally simple and transparent (see docs/DESIGN.md):
//   * The reference mid follows a random walk whose per-step increment is uniform
//     on [-mid_volatility_ticks, +mid_volatility_ticks]; larger volatility means
//     a wider-ranging mid.
//   * Each step, with probability order_arrival_permille/1000, one aggressive
//     order arrives on a random side. Its willingness to "reach" away from the
//     mid is exponentially distributed with mean liquidity_reach_ticks, so a
//     quote at distance d from the mid is taken with probability exp(-d / mean).
//     Tighter quotes therefore fill more often; wider quotes fill less often.
struct SimConfig {
    std::uint64_t steps = 2000;
    std::uint64_t seed = 1;
    Price initial_mid = Price::from_units(100);
    std::int64_t mid_volatility_ticks = 10;      // half-range of the mid increment
    unsigned order_arrival_permille = 800;       // P(aggressor arrives)/step, per mille
    std::int64_t liquidity_reach_ticks = 60;     // mean aggressor reach from mid (ticks)
    std::uint64_t max_aggressor_qty = 6;         // aggressor size drawn from [1, this]
    std::int64_t starting_cash_ticks = 0;        // strategy starting cash
    std::int64_t transaction_cost_ticks = 0;     // fee per filled unit (ticks)
};

// Default market parameters for a named regime (steps / seed / cash are left at
// their defaults for the caller to set).
SimConfig regime_config(MarketRegime regime);

// One row of the simulation time series. Money is in ticks (Price scale).
struct StepRecord {
    std::uint64_t step = 0;
    std::int64_t mid_ticks = 0;
    bool quoted_bid = false;
    bool quoted_ask = false;
    std::int64_t bid_ticks = 0;
    std::int64_t ask_ticks = 0;
    std::uint64_t bid_quantity = 0;
    std::uint64_t ask_quantity = 0;
    std::int64_t inventory = 0;
    std::int64_t cash_ticks = 0;
    std::int64_t realized_ticks = 0;
    std::int64_t unrealized_ticks = 0;
    std::int64_t total_pnl_ticks = 0;
    std::uint64_t fills = 0;            // fill events this step
    std::uint64_t filled_quantity = 0;

    friend bool operator==(const StepRecord&, const StepRecord&) = default;
};

// Full result of a run: the time series plus end-of-run summary figures.
struct SimResult {
    std::vector<StepRecord> steps;

    std::int64_t final_inventory = 0;
    std::int64_t final_cash_ticks = 0;
    std::int64_t final_realized_ticks = 0;
    std::int64_t final_unrealized_ticks = 0;
    std::int64_t final_total_pnl_ticks = 0;

    std::uint64_t quotes_placed = 0;      // number of quote orders submitted
    std::uint64_t fills = 0;              // number of fill events
    std::uint64_t filled_quantity = 0;
    std::uint64_t aggressor_buys = 0;     // buy-side aggressors that arrived
    std::uint64_t aggressor_sells = 0;    // sell-side aggressors that arrived

    // Fills per quote order placed.
    double fill_rate() const noexcept {
        return quotes_placed == 0 ? 0.0
                                   : static_cast<double>(fills) /
                                         static_cast<double>(quotes_placed);
    }

    friend bool operator==(const SimResult&, const SimResult&) = default;
};

// Runs a market-making strategy against a deterministic synthetic market.
//
// Only the strategy's quotes rest in the book. Each step: the reference mid takes
// a volatility-scaled random-walk step; the previous quotes are cancelled; the
// strategy re-quotes; then, with the configured probability, one aggressive limit
// order arrives on a random side. Its exponential "reach" from the mid decides
// how far it will trade, so it lifts/hits a quote only if that quote lies within
// reach — making fill probability decrease with quote distance. The aggressor is
// immediate-or-cancel: any unfilled remainder is cancelled, never rested. All
// order handling goes through the public MatchingEngine submit/cancel APIs.
//
// The random stream depends only on the config, never on the strategy's choices,
// so two strategies run on identical market conditions (same mid path, arrivals,
// sides, sizes, and reaches) and differ only in which quotes get filled.
class MarketMakerSimulator {
public:
    explicit MarketMakerSimulator(SimConfig config) : config_(config) {}

    SimResult run(Strategy& strategy) const;

    const SimConfig& config() const noexcept { return config_; }

private:
    SimConfig config_;
};

}  // namespace lob
