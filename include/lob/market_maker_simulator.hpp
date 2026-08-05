#pragma once

#include <cstdint>
#include <vector>

#include "lob/price.hpp"
#include "lob/strategy.hpp"

namespace lob {

// Configuration of the synthetic market the simulator drives. All randomness is
// seeded, so a given config produces exactly one deterministic run.
struct SimConfig {
    std::uint64_t steps = 2000;
    std::uint64_t seed = 1;
    Price initial_mid = Price::from_units(100);
    std::int64_t mid_tick_step = 10;        // reference-mid random-walk step (ticks)
    unsigned trade_permille = 800;          // aggressor arrival prob, per mille (0..1000)
    std::uint64_t max_aggressor_qty = 6;    // aggressor size drawn from [1, this]
    std::int64_t starting_cash_ticks = 0;   // strategy starting cash
    std::int64_t transaction_cost_ticks = 0;  // fee per filled unit (ticks)
};

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
// The market is a single order book into which only the strategy's quotes rest.
// Each step: the reference mid takes a seeded random-walk step; previous quotes
// are cancelled; the strategy re-quotes from observed state; then, with the
// configured probability, a synthetic market order arrives on a random side and
// lifts/hits whichever quote it crosses. Fills against the strategy's quotes are
// booked through PnLAccount using the existing engine submit/cancel APIs — the
// simulator never touches book internals directly.
//
// The random stream depends only on the config, not on the strategy's choices,
// so two different strategies run on identical market conditions.
class MarketMakerSimulator {
public:
    explicit MarketMakerSimulator(SimConfig config) : config_(config) {}

    SimResult run(Strategy& strategy) const;

    const SimConfig& config() const noexcept { return config_; }

private:
    SimConfig config_;
};

}  // namespace lob
