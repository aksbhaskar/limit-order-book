// Reproducible market-making parameter study.
//
// Sweeps market-maker parameters over a grid, runs many seeds per configuration,
// and compares the fixed-spread and inventory-aware strategies on identical,
// deterministic simulated markets. Writes machine-readable JSON and a CSV summary
// and prints a short comparison.
//
// All numbers are produced by the simulator at run time -- nothing is hard-coded.
// The market is a deliberately simple synthetic model; results describe the
// simulation only and are not evidence about real markets.

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "lob/parameter_study.hpp"
#include "lob/price.hpp"

using namespace lob;

namespace {

const StudyCell* cell_for(const StudyResult& r, StrategyKind kind,
                          const ParamPoint& p) {
    for (const StudyCell& c : r.cells) {
        if (c.strategy == kind && c.params == p) {
            return &c;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    StudyConfig study;

    // Base synthetic market (seed is overridden per run; cost comes from the grid).
    study.market.steps = 3000;
    study.market.initial_mid = Price::from_units(1000);
    study.market.mid_volatility_ticks = 8;
    study.market.order_arrival_permille = 800;
    study.market.liquidity_reach_ticks = 80;
    study.market.max_aggressor_qty = 5;
    study.market.starting_cash_ticks = 100'000 * Price::kTicksPerUnit;

    // Independent seeds per configuration -> distributions, not single points.
    study.seeds.clear();
    for (std::uint64_t s = 1; s <= 20; ++s) {
        study.seeds.push_back(s);
    }

    // Parameter grid.
    study.grid.spreads = {60, 100, 160};
    study.grid.order_quantities = {5, 10};
    study.grid.max_inventories = {30, 60};
    study.grid.skews = {2, 6};
    study.grid.transaction_costs = {0, 2};

    const StudyResult result = run_study(study);

    // Machine-readable artifacts.
    {
        std::ofstream json("study_results.json");
        json << result.to_json() << '\n';
        std::ofstream csv("study_results.csv");
        csv << result.to_csv();
        std::ofstream runs("study_runs.csv");
        runs << result.runs_to_csv();
    }

    std::cout << "SIMULATED parameter study -- results describe the synthetic "
                 "market only, not real markets.\n";
    std::cout << "grid points=" << (result.cells.size() / 2)
              << "  strategies=2  seeds=" << study.seeds.size()
              << "  steps=" << study.market.steps << '\n';
    std::cout << "wrote study_results.json, study_results.csv, and study_runs.csv\n\n";

    // Headline comparison at a representative configuration.
    const ParamPoint rep{100, 5, 60, 6, 2};
    const StudyCell* fixed = cell_for(result, StrategyKind::FixedSpread, rep);
    const StudyCell* inv = cell_for(result, StrategyKind::InventoryAware, rep);
    if (fixed != nullptr && inv != nullptr) {
        std::cout << "Representative config: spread=100 qty=5 max_inv=60 skew=6 cost=2\n";
        std::cout << std::left << std::setw(18) << "strategy" << std::right
                  << std::setw(12) << "P&L mean" << std::setw(22) << "P&L 95% CI"
                  << std::setw(12) << "Sharpe" << std::setw(10) << "avgInv"
                  << std::setw(9) << "maxInv" << '\n';
        for (const StudyCell* c : {fixed, inv}) {
            std::ostringstream ci;
            ci << std::fixed << std::setprecision(2) << "[" << c->pnl.ci95_low << ", "
               << c->pnl.ci95_high << "]";
            std::cout << std::left << std::setw(18) << to_string(c->strategy) << std::right
                      << std::fixed << std::setprecision(2) << std::setw(12) << c->pnl.mean
                      << std::setw(22) << ci.str() << std::setprecision(3)
                      << std::setw(12) << c->sharpe.mean << std::setprecision(2)
                      << std::setw(10) << c->avg_inventory.mean << std::setprecision(0)
                      << std::setw(9) << c->max_abs_inventory.mean << '\n';
        }
    }

    // Overall: across the whole grid, how often does inventory-aware win?
    std::size_t points = 0;
    std::size_t inv_higher_sharpe = 0;
    std::size_t inv_lower_inventory = 0;
    double fixed_sharpe_sum = 0.0;
    double inv_sharpe_sum = 0.0;
    for (std::size_t i = 0; i + 1 < result.cells.size(); i += 2) {
        const StudyCell& f = result.cells[i];       // fixed for this point
        const StudyCell& a = result.cells[i + 1];   // inventory-aware for this point
        ++points;
        fixed_sharpe_sum += f.sharpe.mean;
        inv_sharpe_sum += a.sharpe.mean;
        if (a.sharpe.mean > f.sharpe.mean) {
            ++inv_higher_sharpe;
        }
        if (a.avg_inventory.mean * (a.avg_inventory.mean < 0 ? -1 : 1) <
            f.avg_inventory.mean * (f.avg_inventory.mean < 0 ? -1 : 1)) {
            ++inv_lower_inventory;
        }
    }

    std::cout << "\nAcross " << points << " grid points (each averaged over "
              << study.seeds.size() << " seeds):\n";
    std::cout << std::fixed << std::setprecision(3);
    const double n_points = static_cast<double>(points);
    std::cout << "  mean Sharpe  fixed-spread=" << (fixed_sharpe_sum / n_points)
              << "  inventory-aware=" << (inv_sharpe_sum / n_points) << '\n';
    std::cout << "  inventory-aware had higher Sharpe in " << inv_higher_sharpe << "/"
              << points << " configs\n";
    std::cout << "  inventory-aware held lower |avg inventory| in " << inv_lower_inventory
              << "/" << points << " configs\n";

    return 0;
}
