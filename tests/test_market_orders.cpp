#include <doctest/doctest.h>

#include "lob/matching_engine.hpp"

using namespace lob;

namespace {

Order limit_buy(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Buy, OrderType::Limit, price, qty, seq);
}

Order limit_sell(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Sell, OrderType::Limit, price, qty, seq);
}

// Market orders carry no price; the price field is unused (pass a zero Price).
Order market_buy(OrderId id, Quantity qty, Sequence seq) {
    return Order(id, Side::Buy, OrderType::Market, Price{}, qty, seq);
}

Order market_sell(OrderId id, Quantity qty, Sequence seq) {
    return Order(id, Side::Sell, OrderType::Market, Price{}, qty, seq);
}

Price px(std::int64_t units) { return Price::from_units(units); }

}  // namespace

TEST_CASE("A market order can be constructed with no price") {
    CHECK_NOTHROW(market_buy(OrderId{1}, Quantity{5}, Sequence{1}));
    const Order m = market_sell(OrderId{2}, Quantity{5}, Sequence{2});
    CHECK(m.type() == OrderType::Market);
}

TEST_CASE("Market buy executes against one resting ask at the ask price") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(101), Quantity{5}, Sequence{1}));

    const SubmitResult r = engine.submit(market_buy(OrderId{2}, Quantity{5}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].maker_id == OrderId{1});
    CHECK(r.trades[0].taker_id == OrderId{2});
    CHECK(r.trades[0].price == px(101));   // maker/resting price
    CHECK(r.trades[0].quantity == Quantity{5});
    CHECK(r.fully_filled());
    CHECK_FALSE(r.resting);
    CHECK(engine.book().empty());
}

TEST_CASE("Market sell executes against one resting bid at the bid price") {
    MatchingEngine engine;
    engine.submit(limit_buy(OrderId{1}, px(99), Quantity{5}, Sequence{1}));

    const SubmitResult r = engine.submit(market_sell(OrderId{2}, Quantity{5}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].price == px(99));
    CHECK(r.fully_filled());
    CHECK(engine.book().empty());
}

TEST_CASE("A market order consumes multiple resting orders at one level (FIFO)") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(100), Quantity{3}, Sequence{1}));
    engine.submit(limit_sell(OrderId{2}, px(100), Quantity{4}, Sequence{2}));

    const SubmitResult r = engine.submit(market_buy(OrderId{3}, Quantity{7}, Sequence{3}));

    REQUIRE(r.trades.size() == 2);
    CHECK(r.trades[0].maker_id == OrderId{1});   // oldest first
    CHECK(r.trades[0].quantity == Quantity{3});
    CHECK(r.trades[1].maker_id == OrderId{2});
    CHECK(r.trades[1].quantity == Quantity{4});
    CHECK(r.fully_filled());
    CHECK(engine.book().empty());
}

TEST_CASE("A market order walks multiple price levels best-first") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(101), Quantity{5}, Sequence{1}));
    engine.submit(limit_sell(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    const SubmitResult r = engine.submit(market_buy(OrderId{3}, Quantity{8}, Sequence{3}));

    REQUIRE(r.trades.size() == 2);
    CHECK(r.trades[0].price == px(100));   // best (lowest) ask first
    CHECK(r.trades[0].quantity == Quantity{5});
    CHECK(r.trades[1].price == px(101));   // then next level
    CHECK(r.trades[1].quantity == Quantity{3});
    CHECK(r.fully_filled());

    CHECK(engine.book().level_at(Side::Sell, px(100)) == nullptr);
    REQUIRE(engine.book().best_ask() != nullptr);
    CHECK(engine.book().best_ask()->price() == px(101));
    CHECK(engine.book().best_ask()->total_quantity() == Quantity{2});
}

TEST_CASE("A market order with insufficient liquidity partially fills and drops the rest") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(100), Quantity{4}, Sequence{1}));

    const SubmitResult r = engine.submit(market_buy(OrderId{2}, Quantity{10}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.filled_quantity == Quantity{4});
    CHECK(r.remaining_quantity == Quantity{6});   // unfilled remainder
    CHECK_FALSE(r.resting);                        // never rests
    CHECK(engine.book().empty());                  // nothing added to the book
}

TEST_CASE("A market order against an empty book does nothing and never rests") {
    MatchingEngine engine;

    const SubmitResult r = engine.submit(market_buy(OrderId{1}, Quantity{5}, Sequence{1}));

    CHECK(r.trades.empty());
    CHECK(r.filled_quantity == Quantity{0});
    CHECK(r.remaining_quantity == Quantity{5});
    CHECK_FALSE(r.resting);
    CHECK(engine.book().empty());
}

TEST_CASE("A market order never lands on the same side of the book") {
    MatchingEngine engine;
    // Only bids exist; a market *buy* has no opposing asks to hit.
    engine.submit(limit_buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    const SubmitResult r = engine.submit(market_buy(OrderId{2}, Quantity{5}, Sequence{2}));

    CHECK(r.trades.empty());
    CHECK_FALSE(r.resting);
    // The resting bid is untouched and no market order was added anywhere.
    CHECK(engine.book().best_bid()->total_quantity() == Quantity{5});
    CHECK(engine.book().best_ask() == nullptr);
    CHECK(engine.book().level_count(Side::Buy) == 1);
}

TEST_CASE("Execution prices come from the makers when a market order sweeps levels") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(100), Quantity{2}, Sequence{1}));
    engine.submit(limit_sell(OrderId{2}, px(102), Quantity{2}, Sequence{2}));
    engine.submit(limit_sell(OrderId{3}, px(105), Quantity{2}, Sequence{3}));

    const SubmitResult r = engine.submit(market_buy(OrderId{4}, Quantity{6}, Sequence{4}));

    REQUIRE(r.trades.size() == 3);
    CHECK(r.trades[0].price == px(100));
    CHECK(r.trades[1].price == px(102));
    CHECK(r.trades[2].price == px(105));
    CHECK(r.fully_filled());
    CHECK(engine.book().empty());
}

TEST_CASE("A market order leaves a partially consumed maker resting with FIFO intact") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    engine.submit(limit_sell(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    const SubmitResult r = engine.submit(market_buy(OrderId{3}, Quantity{7}, Sequence{3}));

    REQUIRE(r.trades.size() == 2);
    CHECK(r.trades[1].maker_id == OrderId{2});
    CHECK(r.trades[1].quantity == Quantity{2});

    REQUIRE(engine.book().best_ask() != nullptr);
    CHECK(engine.book().best_ask()->order_count() == 1);
    CHECK(engine.book().best_ask()->front().id() == OrderId{2});
    CHECK(engine.book().best_ask()->front().remaining_quantity() == Quantity{3});
}

TEST_CASE("Limit-order behaviour is unchanged by market-order support") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(101), Quantity{5}, Sequence{1}));

    // A limit buy below the ask still does not cross and rests, exactly as before.
    const SubmitResult r = engine.submit(limit_buy(OrderId{2}, px(100), Quantity{5}, Sequence{2}));
    CHECK(r.trades.empty());
    CHECK(r.resting);
    CHECK(engine.book().best_bid()->price() == px(100));
    CHECK(engine.book().best_ask()->price() == px(101));
}

TEST_CASE("Market orders interact correctly with cancellation") {
    MatchingEngine engine;
    engine.submit(limit_sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    engine.submit(limit_sell(OrderId{2}, px(101), Quantity{5}, Sequence{2}));

    // Cancel the best ask, then a market buy should hit the remaining level.
    REQUIRE(engine.cancel(OrderId{1}).ok());

    const SubmitResult r = engine.submit(market_buy(OrderId{3}, Quantity{5}, Sequence{3}));
    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].maker_id == OrderId{2});
    CHECK(r.trades[0].price == px(101));
    CHECK(engine.book().empty());
}
