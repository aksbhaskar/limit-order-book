#include "lob/parameter_study.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

#include "lob/backtest.hpp"
#include "lob/inventory_market_maker.hpp"
#include "lob/market_maker.hpp"
#include "lob/price.hpp"
#include "lob/quantity.hpp"

namespace lob {

namespace {

std::string num(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6) << value;
    return out.str();
}

BacktestMetrics run_one(StrategyKind kind, const ParamPoint& p, const SimConfig& market) {
    if (kind == StrategyKind::FixedSpread) {
        MarketMakerConfig c;
        c.spread = Price::from_ticks(p.spread_ticks);
        c.max_inventory = p.max_inventory;
        c.order_quantity = Quantity{p.order_quantity};
        FixedSpreadMarketMaker mm(c);
        return Backtester(market).run(mm);
    }
    InventoryAwareConfig c;
    c.base_spread = Price::from_ticks(p.spread_ticks);
    c.max_inventory = p.max_inventory;
    c.order_quantity = Quantity{p.order_quantity};
    c.skew_ticks_per_unit = p.skew_ticks_per_unit;
    c.widen_ticks_per_unit = 1;
    InventoryAwareMarketMaker mm(c);
    return Backtester(market).run(mm);
}

}  // namespace

std::string to_string(StrategyKind kind) {
    return kind == StrategyKind::FixedSpread ? "fixed-spread" : "inventory-aware";
}

std::vector<ParamPoint> generate_grid(const ParamGridSpec& spec) {
    std::vector<ParamPoint> grid;
    for (std::int64_t spread : spec.spreads) {
        for (std::uint64_t qty : spec.order_quantities) {
            for (std::int64_t max_inv : spec.max_inventories) {
                for (std::int64_t skew : spec.skews) {
                    for (std::int64_t cost : spec.transaction_costs) {
                        grid.push_back(ParamPoint{spread, qty, max_inv, skew, cost});
                    }
                }
            }
        }
    }
    return grid;
}

Aggregate Aggregate::of(std::vector<double> samples) {
    Aggregate a;
    a.n = samples.size();
    if (samples.empty()) {
        return a;
    }
    std::sort(samples.begin(), samples.end());

    const double sum = std::accumulate(samples.begin(), samples.end(), 0.0);
    a.mean = sum / static_cast<double>(a.n);

    const std::size_t m = samples.size();
    a.median = (m % 2 == 1) ? samples[m / 2]
                            : 0.5 * (samples[m / 2 - 1] + samples[m / 2]);

    if (a.n > 1) {
        double ss = 0.0;
        for (double x : samples) {
            ss += (x - a.mean) * (x - a.mean);
        }
        a.stddev = std::sqrt(ss / static_cast<double>(a.n - 1));
        const double se = a.stddev / std::sqrt(static_cast<double>(a.n));
        a.ci95_low = a.mean - 1.96 * se;
        a.ci95_high = a.mean + 1.96 * se;
    } else {
        a.stddev = 0.0;
        a.ci95_low = a.mean;
        a.ci95_high = a.mean;
    }
    return a;
}

StudyResult run_study(const StudyConfig& config) {
    StudyResult out;
    const std::vector<ParamPoint> grid = generate_grid(config.grid);

    for (const ParamPoint& p : grid) {
        for (const StrategyKind kind :
             {StrategyKind::FixedSpread, StrategyKind::InventoryAware}) {
            std::vector<double> pnl, sharpe, mdd, fill, avg_inv, max_inv, trades;
            pnl.reserve(config.seeds.size());

            for (std::uint64_t seed : config.seeds) {
                SimConfig market = config.market;
                market.seed = seed;
                market.transaction_cost_ticks = p.transaction_cost_ticks;

                const BacktestMetrics m = run_one(kind, p, market);
                pnl.push_back(m.total_pnl);
                sharpe.push_back(m.sharpe);
                mdd.push_back(m.max_drawdown);
                fill.push_back(m.fill_rate);
                avg_inv.push_back(m.average_inventory);
                max_inv.push_back(static_cast<double>(m.max_abs_inventory));
                trades.push_back(static_cast<double>(m.trade_count));
            }

            StudyCell cell;
            cell.strategy = kind;
            cell.params = p;
            cell.seeds = config.seeds.size();
            cell.pnl = Aggregate::of(pnl);
            cell.sharpe = Aggregate::of(sharpe);
            cell.max_drawdown = Aggregate::of(mdd);
            cell.fill_rate = Aggregate::of(fill);
            cell.avg_inventory = Aggregate::of(avg_inv);
            cell.max_abs_inventory = Aggregate::of(max_inv);
            cell.trade_count = Aggregate::of(trades);
            out.cells.push_back(cell);
        }
    }
    return out;
}

std::string StudyResult::to_json() const {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const StudyCell& c = cells[i];
        if (i != 0) {
            out << ",";
        }
        out << "{"
            << "\"strategy\":\"" << to_string(c.strategy) << "\","
            << "\"spread_ticks\":" << c.params.spread_ticks << ","
            << "\"order_quantity\":" << c.params.order_quantity << ","
            << "\"max_inventory\":" << c.params.max_inventory << ","
            << "\"skew_ticks_per_unit\":" << c.params.skew_ticks_per_unit << ","
            << "\"transaction_cost_ticks\":" << c.params.transaction_cost_ticks << ","
            << "\"seeds\":" << c.seeds << ","
            << "\"pnl_mean\":" << num(c.pnl.mean) << ","
            << "\"pnl_median\":" << num(c.pnl.median) << ","
            << "\"pnl_std\":" << num(c.pnl.stddev) << ","
            << "\"pnl_ci95_low\":" << num(c.pnl.ci95_low) << ","
            << "\"pnl_ci95_high\":" << num(c.pnl.ci95_high) << ","
            << "\"sharpe_mean\":" << num(c.sharpe.mean) << ","
            << "\"max_drawdown_mean\":" << num(c.max_drawdown.mean) << ","
            << "\"fill_rate_mean\":" << num(c.fill_rate.mean) << ","
            << "\"avg_inventory_mean\":" << num(c.avg_inventory.mean) << ","
            << "\"max_abs_inventory_mean\":" << num(c.max_abs_inventory.mean) << ","
            << "\"trade_count_mean\":" << num(c.trade_count.mean) << "}";
    }
    out << "]";
    return out.str();
}

std::string StudyResult::to_csv() const {
    std::ostringstream out;
    out << "strategy,spread_ticks,order_quantity,max_inventory,skew_ticks_per_unit,"
           "transaction_cost_ticks,seeds,pnl_mean,pnl_median,pnl_std,pnl_ci95_low,"
           "pnl_ci95_high,sharpe_mean,max_drawdown_mean,fill_rate_mean,"
           "avg_inventory_mean,max_abs_inventory_mean,trade_count_mean\n";
    for (const StudyCell& c : cells) {
        out << to_string(c.strategy) << ',' << c.params.spread_ticks << ','
            << c.params.order_quantity << ',' << c.params.max_inventory << ','
            << c.params.skew_ticks_per_unit << ',' << c.params.transaction_cost_ticks
            << ',' << c.seeds << ',' << num(c.pnl.mean) << ',' << num(c.pnl.median)
            << ',' << num(c.pnl.stddev) << ',' << num(c.pnl.ci95_low) << ','
            << num(c.pnl.ci95_high) << ',' << num(c.sharpe.mean) << ','
            << num(c.max_drawdown.mean) << ',' << num(c.fill_rate.mean) << ','
            << num(c.avg_inventory.mean) << ',' << num(c.max_abs_inventory.mean) << ','
            << num(c.trade_count.mean) << '\n';
    }
    return out.str();
}

}  // namespace lob
