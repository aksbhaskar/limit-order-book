#include <doctest/doctest.h>

#include <algorithm>
#include <cstdlib>
#include <string>

#include "lob/market_maker_simulator.hpp"
#include "lob/strategy.hpp"

using namespace lob;

namespace {

// A test double that always quotes both sides at a fixed distance from the mid,
// ignoring inventory. It isolates the effect of quote distance on fills.
class FixedDistanceQuoter : public Strategy {
public:
    FixedDistanceQuoter(std::int64_t distance, std::uint64_t qty)
        : distance_(distance), qty_(qty) {}

    QuoteDecision quote(const MarketState& state) override {
        QuoteDecision d;
        const std::int64_t mid = state.mid.ticks();
        if (mid - distance_ > 0) {
            d.quote_bid = true;
            d.bid_price = Price::from_ticks(mid - distance_);
            d.bid_quantity = Quantity{qty_};
        }
        if (mid + distance_ > 0) {
            d.quote_ask = true;
            d.ask_price = Price::from_ticks(mid + distance_);
            d.ask_quantity = Quantity{qty_};
        }
        return d;
    }
    std::string name() const override { return "fixed-distance"; }

private:
    std::int64_t distance_;
    std::uint64_t qty_;
};

SimConfig cfg() {
    SimConfig c;
    c.steps = 4000;
    c.seed = 11;
    c.initial_mid = Price::from_units(1000);   // high mid so wide quotes stay positive
    c.mid_volatility_ticks = 8;
    c.order_arrival_permille = 700;
    c.liquidity_reach_ticks = 80;
    c.max_aggressor_qty = 5;
    return c;
}

}  // namespace

TEST_CASE("Tighter quotes fill more often than wider quotes") {
    FixedDistanceQuoter tight(10, 1);
    FixedDistanceQuoter wide(220, 1);

    const SimResult tight_run = MarketMakerSimulator(cfg()).run(tight);
    const SimResult wide_run = MarketMakerSimulator(cfg()).run(wide);

    CHECK(tight_run.fills > wide_run.fills);
    CHECK(tight_run.fill_rate() > wide_run.fill_rate());
    CHECK(wide_run.fills > 0);   // very wide still fills occasionally, just rarely
}

TEST_CASE("Higher market activity produces more executions") {
    SimConfig quiet = cfg();
    quiet.order_arrival_permille = 150;
    SimConfig busy = cfg();
    busy.order_arrival_permille = 950;

    FixedDistanceQuoter q1(20, 1);
    FixedDistanceQuoter q2(20, 1);
    const SimResult quiet_run = MarketMakerSimulator(quiet).run(q1);
    const SimResult busy_run = MarketMakerSimulator(busy).run(q2);

    CHECK(busy_run.fills > quiet_run.fills);
}

TEST_CASE("Volatility widens the reference-price process") {
    auto mid_range = [](const SimResult& r) {
        std::int64_t lo = r.steps.front().mid_ticks;
        std::int64_t hi = lo;
        for (const StepRecord& s : r.steps) {
            lo = std::min(lo, s.mid_ticks);
            hi = std::max(hi, s.mid_ticks);
        }
        return hi - lo;
    };

    SimConfig calm = cfg();
    calm.mid_volatility_ticks = 2;
    SimConfig wild = cfg();
    wild.mid_volatility_ticks = 40;

    FixedDistanceQuoter q1(20, 1);
    FixedDistanceQuoter q2(20, 1);
    const SimResult calm_run = MarketMakerSimulator(calm).run(q1);
    const SimResult wild_run = MarketMakerSimulator(wild).run(q2);

    CHECK(mid_range(wild_run) > mid_range(calm_run));
}

TEST_CASE("The simulator generates both buy and sell aggressive flow") {
    FixedDistanceQuoter q(20, 1);
    const SimResult r = MarketMakerSimulator(cfg()).run(q);

    CHECK(r.aggressor_buys > 0);
    CHECK(r.aggressor_sells > 0);
}

TEST_CASE("Identical seeds produce identical simulations") {
    FixedDistanceQuoter a(20, 1);
    FixedDistanceQuoter b(20, 1);
    const SimResult ra = MarketMakerSimulator(cfg()).run(a);
    const SimResult rb = MarketMakerSimulator(cfg()).run(b);

    CHECK(ra == rb);
    CHECK(ra.aggressor_buys == rb.aggressor_buys);
    CHECK(ra.aggressor_sells == rb.aggressor_sells);
}

TEST_CASE("Different seeds produce different simulations") {
    SimConfig c1 = cfg();
    c1.seed = 100;
    SimConfig c2 = cfg();
    c2.seed = 200;

    FixedDistanceQuoter a(20, 1);
    FixedDistanceQuoter b(20, 1);
    const SimResult ra = MarketMakerSimulator(c1).run(a);
    const SimResult rb = MarketMakerSimulator(c2).run(b);

    CHECK_FALSE(ra == rb);
}

TEST_CASE("The market never generates impossible prices or quantities") {
    FixedDistanceQuoter q(20, 3);
    const SimResult r = MarketMakerSimulator(cfg()).run(q);

    for (const StepRecord& s : r.steps) {
        CHECK(s.mid_ticks >= 1);
        if (s.quoted_bid) {
            CHECK(s.bid_ticks > 0);
        }
        if (s.quoted_ask) {
            CHECK(s.ask_ticks > 0);
        }
        // A step can fill at most both quotes' worth; never a negative amount.
        CHECK(s.filled_quantity <= s.bid_quantity + s.ask_quantity);
    }
    // Every aggressor was a valid order (the run completed without throwing).
    CHECK((r.aggressor_buys + r.aggressor_sells) > 0);
}

TEST_CASE("Named regimes differ: high-volume executes more than calm") {
    SimConfig calm = regime_config(MarketRegime::Calm);
    calm.steps = 4000;
    calm.seed = 5;
    SimConfig heavy = regime_config(MarketRegime::HighVolume);
    heavy.steps = 4000;
    heavy.seed = 5;

    FixedDistanceQuoter q1(20, 1);
    FixedDistanceQuoter q2(20, 1);
    const SimResult calm_run = MarketMakerSimulator(calm).run(q1);
    const SimResult heavy_run = MarketMakerSimulator(heavy).run(q2);

    CHECK(heavy_run.fills > calm_run.fills);
}
