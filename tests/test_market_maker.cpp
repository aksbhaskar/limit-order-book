#include <doctest/doctest.h>

#include "lob/market_maker.hpp"

using namespace lob;

namespace {

MarketState state_with(std::int64_t inventory, Price mid) {
    MarketState s;
    s.mid = mid;
    s.inventory = inventory;
    return s;
}

}  // namespace

TEST_CASE("Fixed-spread maker quotes symmetrically around the mid") {
    MarketMakerConfig cfg;
    cfg.spread = Price::from_ticks(100);   // half-spread 50
    cfg.max_inventory = 100;
    cfg.order_quantity = Quantity{10};
    FixedSpreadMarketMaker mm(cfg);

    const QuoteDecision d = mm.quote(state_with(0, Price::from_units(100)));

    REQUIRE(d.quote_bid);
    REQUIRE(d.quote_ask);
    CHECK(d.bid_price == Price::from_ticks(Price::from_units(100).ticks() - 50));
    CHECK(d.ask_price == Price::from_ticks(Price::from_units(100).ticks() + 50));
    CHECK(d.bid_quantity == Quantity{10});
    CHECK(d.ask_quantity == Quantity{10});
}

TEST_CASE("Quote size is clamped to remaining inventory room") {
    MarketMakerConfig cfg;
    cfg.spread = Price::from_ticks(100);
    cfg.max_inventory = 12;
    cfg.order_quantity = Quantity{10};
    FixedSpreadMarketMaker mm(cfg);

    // Long 8: can still buy only 4 (12 - 8) but can sell a full 10 (12 + 8).
    const QuoteDecision d = mm.quote(state_with(8, Price::from_units(100)));
    CHECK(d.bid_quantity == Quantity{4});
    CHECK(d.ask_quantity == Quantity{10});
}

TEST_CASE("At the long inventory limit the bid is suppressed") {
    MarketMakerConfig cfg;
    cfg.spread = Price::from_ticks(100);
    cfg.max_inventory = 10;
    cfg.order_quantity = Quantity{5};
    FixedSpreadMarketMaker mm(cfg);

    const QuoteDecision d = mm.quote(state_with(10, Price::from_units(100)));
    CHECK_FALSE(d.quote_bid);   // cannot buy more
    CHECK(d.quote_ask);         // can still sell
    CHECK(d.ask_quantity == Quantity{5});
}

TEST_CASE("At the short inventory limit the ask is suppressed") {
    MarketMakerConfig cfg;
    cfg.spread = Price::from_ticks(100);
    cfg.max_inventory = 10;
    cfg.order_quantity = Quantity{5};
    FixedSpreadMarketMaker mm(cfg);

    const QuoteDecision d = mm.quote(state_with(-10, Price::from_units(100)));
    CHECK(d.quote_bid);
    CHECK_FALSE(d.quote_ask);
}
