#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "lob/parameter_study.hpp"
#include "lob/price.hpp"

using namespace lob;

namespace {

StudyConfig small_study() {
    StudyConfig s;
    s.market.steps = 800;
    s.market.initial_mid = Price::from_units(1000);
    s.market.mid_volatility_ticks = 8;
    s.market.order_arrival_permille = 800;
    s.market.liquidity_reach_ticks = 80;
    s.market.max_aggressor_qty = 5;
    s.market.starting_cash_ticks = 100'000 * Price::kTicksPerUnit;
    s.seeds = {1, 2, 3, 4};
    s.grid.spreads = {80, 140};
    s.grid.order_quantities = {5};
    s.grid.max_inventories = {40};
    s.grid.skews = {4};
    s.grid.transaction_costs = {0, 3};
    return s;
}

}  // namespace

TEST_CASE("The grid is the Cartesian product of the axes") {
    ParamGridSpec spec;
    spec.spreads = {60, 100, 160};
    spec.order_quantities = {5, 10};
    spec.max_inventories = {30, 60};
    spec.skews = {2, 6};
    spec.transaction_costs = {0, 2};

    const std::vector<ParamPoint> grid = generate_grid(spec);
    CHECK(grid.size() == 3u * 2u * 2u * 2u * 2u);   // 48

    // First and last points follow the nested iteration order.
    CHECK(grid.front() == ParamPoint{60, 5, 30, 2, 0});
    CHECK(grid.back() == ParamPoint{160, 10, 60, 6, 2});
}

TEST_CASE("Aggregate computes mean, median, stddev and a 95% CI") {
    const Aggregate a = Aggregate::of({1.0, 2.0, 3.0, 4.0, 5.0});
    CHECK(a.n == 5);
    CHECK(a.mean == doctest::Approx(3.0));
    CHECK(a.median == doctest::Approx(3.0));
    CHECK(a.stddev == doctest::Approx(1.5811388));   // sample (n-1) std dev
    // CI = mean +/- 1.96 * s/sqrt(n) = 3 +/- 1.386.
    CHECK(a.ci95_low == doctest::Approx(1.6142).epsilon(0.001));
    CHECK(a.ci95_high == doctest::Approx(4.3858).epsilon(0.001));
    CHECK(a.ci95_low < a.mean);
    CHECK(a.ci95_high > a.mean);
}

TEST_CASE("Aggregate handles even counts and degenerate samples") {
    CHECK(Aggregate::of({1.0, 2.0, 3.0, 4.0}).median == doctest::Approx(2.5));

    const Aggregate one = Aggregate::of({7.0});
    CHECK(one.mean == doctest::Approx(7.0));
    CHECK(one.stddev == doctest::Approx(0.0));
    CHECK(one.ci95_low == doctest::Approx(7.0));
    CHECK(one.ci95_high == doctest::Approx(7.0));

    const Aggregate none = Aggregate::of({});
    CHECK(none.n == 0);
    CHECK(none.mean == doctest::Approx(0.0));
}

TEST_CASE("A study produces one cell per (parameter point, strategy)") {
    const StudyResult r = run_study(small_study());
    // 2 spreads * 2 costs = 4 points, times 2 strategies.
    CHECK(r.cells.size() == 8);
    for (const StudyCell& c : r.cells) {
        CHECK(c.seeds == 4);
        CHECK(c.pnl.n == 4);
        CHECK(c.trade_count.mean > 0.0);
    }
}

TEST_CASE("Raw per-seed runs are recorded and serialized") {
    const StudyResult r = run_study(small_study());
    // 4 points * 2 strategies * 4 seeds.
    CHECK(r.runs.size() == r.cells.size() * 4);
    CHECK(r.runs.size() == 32);

    const std::string csv = r.runs_to_csv();
    CHECK(csv.find("strategy,spread_ticks,") == 0);
    CHECK(csv.find(",seed,total_pnl,sharpe,") != std::string::npos);
    std::size_t lines = 0;
    for (char ch : csv) {
        if (ch == '\n') {
            ++lines;
        }
    }
    CHECK(lines == r.runs.size() + 1);   // header + one row per run
}

TEST_CASE("A study is reproducible") {
    const StudyResult a = run_study(small_study());
    const StudyResult b = run_study(small_study());
    CHECK(a.to_csv() == b.to_csv());
    CHECK(a.to_json() == b.to_json());
}

TEST_CASE("Different seed sets change the results (seed independence)") {
    StudyConfig s1 = small_study();
    StudyConfig s2 = small_study();
    s2.seeds = {101, 102, 103, 104};

    const StudyResult r1 = run_study(s1);
    const StudyResult r2 = run_study(s2);
    CHECK(r1.to_json() != r2.to_json());
}

TEST_CASE("Both strategies are present and comparable at each point") {
    const StudyResult r = run_study(small_study());

    // Cells alternate fixed, inventory-aware per point.
    for (std::size_t i = 0; i + 1 < r.cells.size(); i += 2) {
        CHECK(r.cells[i].strategy == StrategyKind::FixedSpread);
        CHECK(r.cells[i + 1].strategy == StrategyKind::InventoryAware);
        CHECK(r.cells[i].params == r.cells[i + 1].params);   // same conditions
    }
}

TEST_CASE("JSON and CSV output are well-formed") {
    const StudyResult r = run_study(small_study());

    const std::string json = r.to_json();
    CHECK(json.front() == '[');
    CHECK(json.back() == ']');
    CHECK(json.find("\"strategy\":\"inventory-aware\"") != std::string::npos);
    CHECK(json.find("\"pnl_ci95_low\":") != std::string::npos);

    const std::string csv = r.to_csv();
    // Header line + one line per cell (+ trailing newline => cells+2 splits).
    std::size_t lines = 0;
    for (char ch : csv) {
        if (ch == '\n') {
            ++lines;
        }
    }
    CHECK(lines == r.cells.size() + 1);   // header + one row per cell
    CHECK(csv.find("strategy,spread_ticks,") == 0);
}
