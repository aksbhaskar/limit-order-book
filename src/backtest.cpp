#include "lob/backtest.hpp"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include "lob/price.hpp"

namespace lob {

namespace {

std::string json_number(double value) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6) << value;
    return out.str();
}

}  // namespace

BacktestMetrics Backtester::run(Strategy& strategy) const {
    const SimResult sim = MarketMakerSimulator(market_).run(strategy);

    BacktestMetrics m;
    m.strategy = strategy.name();

    const double scale = static_cast<double>(Price::kTicksPerUnit);
    m.total_pnl = static_cast<double>(sim.final_total_pnl_ticks) / scale;
    m.realized_pnl = static_cast<double>(sim.final_realized_ticks) / scale;
    m.unrealized_pnl = static_cast<double>(sim.final_unrealized_ticks) / scale;
    m.trade_count = sim.fills;
    m.fill_rate = sim.fill_rate();

    const double start_cash = static_cast<double>(market_.starting_cash_ticks) / scale;
    m.return_pct = (market_.starting_cash_ticks != 0) ? (m.total_pnl / start_cash) : 0.0;

    // Per-step returns are normalised by the (constant) starting cash so the
    // denominator can never be zero mid-curve; equity = starting cash + P&L.
    const double denom =
        (market_.starting_cash_ticks != 0)
            ? static_cast<double>(market_.starting_cash_ticks)
            : 1.0;

    std::int64_t prev_equity = market_.starting_cash_ticks;
    std::int64_t peak = market_.starting_cash_ticks;
    double max_dd = 0.0;
    double sum_r = 0.0;
    double sum_r2 = 0.0;
    std::size_t n_ret = 0;
    long double sum_inv = 0.0L;
    std::int64_t max_abs_inv = 0;

    for (const StepRecord& s : sim.steps) {
        const std::int64_t equity = market_.starting_cash_ticks + s.total_pnl_ticks;

        const double r = static_cast<double>(equity - prev_equity) / denom;
        sum_r += r;
        sum_r2 += r * r;
        ++n_ret;
        prev_equity = equity;

        if (equity > peak) {
            peak = equity;
        }
        if (peak > 0) {
            const double dd =
                static_cast<double>(peak - equity) / static_cast<double>(peak);
            if (dd > max_dd) {
                max_dd = dd;
            }
        }

        sum_inv += static_cast<long double>(s.inventory);
        const std::int64_t a = std::llabs(s.inventory);
        if (a > max_abs_inv) {
            max_abs_inv = a;
        }
    }

    m.max_drawdown = max_dd;
    m.max_abs_inventory = max_abs_inv;
    if (!sim.steps.empty()) {
        m.average_inventory =
            static_cast<double>(sum_inv / static_cast<long double>(sim.steps.size()));
    }
    if (n_ret > 1) {
        const double mean = sum_r / static_cast<double>(n_ret);
        const double var = (sum_r2 / static_cast<double>(n_ret)) - mean * mean;
        const double sd = var > 0.0 ? std::sqrt(var) : 0.0;
        m.volatility = sd;
        m.sharpe = sd > 0.0 ? (mean / sd) : 0.0;
    }
    return m;
}

std::string BacktestMetrics::to_json() const {
    std::ostringstream out;
    out << "{"
        << "\"strategy\":\"" << strategy << "\","
        << "\"total_pnl\":" << json_number(total_pnl) << ","
        << "\"realized_pnl\":" << json_number(realized_pnl) << ","
        << "\"unrealized_pnl\":" << json_number(unrealized_pnl) << ","
        << "\"return_pct\":" << json_number(return_pct) << ","
        << "\"max_drawdown\":" << json_number(max_drawdown) << ","
        << "\"volatility\":" << json_number(volatility) << ","
        << "\"sharpe\":" << json_number(sharpe) << ","
        << "\"trade_count\":" << trade_count << ","
        << "\"fill_rate\":" << json_number(fill_rate) << ","
        << "\"average_inventory\":" << json_number(average_inventory) << ","
        << "\"max_abs_inventory\":" << max_abs_inventory << "}";
    return out.str();
}

std::string results_to_json(const std::vector<BacktestMetrics>& results) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < results.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << results[i].to_json();
    }
    out << "]";
    return out.str();
}

}  // namespace lob
