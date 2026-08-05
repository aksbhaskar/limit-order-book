#include <doctest/doctest.h>

#include "lob/inventory_market_maker.hpp"

using namespace lob;

namespace {

InventoryAwareConfig cfg() {
    InventoryAwareConfig c;
    c.base_spread = Price::from_ticks(100);   // half 50
    c.max_inventory = 100;
    c.order_quantity = Quantity{10};
    c.skew_ticks_per_unit = 2;
    c.widen_ticks_per_unit = 1;
    return c;
}

MarketState state_with(std::int64_t inventory, Price mid) {
    MarketState s;
    s.mid = mid;
    s.inventory = inventory;
    return s;
}

}  // namespace

TEST_CASE("At zero inventory the inventory-aware maker quotes symmetrically") {
    InventoryAwareMarketMaker mm(cfg());
    const std::int64_t mid = Price::from_units(100).ticks();

    const QuoteDecision d = mm.quote(state_with(0, Price::from_units(100)));
    CHECK(d.bid_price == Price::from_ticks(mid - 50));
    CHECK(d.ask_price == Price::from_ticks(mid + 50));
}

TEST_CASE("A long position shifts both quotes down to lean against inventory") {
    InventoryAwareMarketMaker mm(cfg());
    const std::int64_t mid = Price::from_units(100).ticks();

    const QuoteDecision flat = mm.quote(state_with(0, Price::from_units(100)));
    const QuoteDecision longd = mm.quote(state_with(10, Price::from_units(100)));

    // Reservation shifts by -skew*inventory = -20 ticks; half widens by +10.
    const std::int64_t reservation = mid - 2 * 10;
    const std::int64_t half_eff = 50 + 10 * 1;
    CHECK(longd.bid_price == Price::from_ticks(reservation - half_eff));
    CHECK(longd.ask_price == Price::from_ticks(reservation + half_eff));

    // The whole quote is lower than when flat (leaning against the long).
    CHECK(longd.ask_price < flat.ask_price);
    CHECK(longd.bid_price < flat.bid_price);
}

TEST_CASE("A short position shifts both quotes up") {
    InventoryAwareMarketMaker mm(cfg());

    const QuoteDecision flat = mm.quote(state_with(0, Price::from_units(100)));
    const QuoteDecision shortd = mm.quote(state_with(-10, Price::from_units(100)));

    CHECK(shortd.bid_price > flat.bid_price);
    CHECK(shortd.ask_price > flat.ask_price);
}

TEST_CASE("The spread widens as absolute inventory grows") {
    InventoryAwareMarketMaker mm(cfg());

    const QuoteDecision flat = mm.quote(state_with(0, Price::from_units(100)));
    const QuoteDecision skewed = mm.quote(state_with(20, Price::from_units(100)));

    const std::int64_t flat_spread = flat.ask_price.ticks() - flat.bid_price.ticks();
    const std::int64_t wide_spread = skewed.ask_price.ticks() - skewed.bid_price.ticks();
    CHECK(wide_spread > flat_spread);
}

TEST_CASE("Inventory limits still clamp sizes and suppress a pinned side") {
    InventoryAwareConfig c = cfg();
    c.max_inventory = 10;
    c.order_quantity = Quantity{5};
    InventoryAwareMarketMaker mm(c);

    const QuoteDecision d = mm.quote(state_with(10, Price::from_units(100)));
    CHECK_FALSE(d.quote_bid);   // at the long cap, cannot buy more
    CHECK(d.quote_ask);
}
