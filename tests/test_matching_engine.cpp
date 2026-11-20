#include <doctest/doctest.h>

#include <stdexcept>

#include "lob/matching_engine.hpp"

using namespace lob;

namespace {

Order buy(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Buy, OrderType::Limit, price, qty, seq);
}

Order sell(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Sell, OrderType::Limit, price, qty, seq);
}

Price px(std::int64_t units) { return Price::from_units(units); }

}  // namespace

TEST_CASE("A non-crossing order rests without trading") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(101), Quantity{5}, Sequence{1}));

    // Bid below the ask: no cross.
    const SubmitResult r = engine.submit(buy(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    CHECK(r.trades.empty());
    CHECK(r.filled_quantity == Quantity{0});
    CHECK(r.remaining_quantity == Quantity{5});
    CHECK(r.resting);
    CHECK(engine.book().best_bid() != nullptr);
    CHECK(engine.book().best_ask() != nullptr);
}

TEST_CASE("An exactly-priced incoming order crosses and fully fills") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    const SubmitResult r = engine.submit(buy(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].maker_id == OrderId{1});
    CHECK(r.trades[0].taker_id == OrderId{2});
    CHECK(r.trades[0].price == px(100));
    CHECK(r.trades[0].quantity == Quantity{5});
    CHECK(r.fully_filled());
    CHECK_FALSE(r.resting);
    // Both sides now empty.
    CHECK(engine.book().empty());
}

TEST_CASE("A crossing buy trades at the resting ask's price, not its own") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    // Aggressive buy priced at 105 still executes at the maker's 100.
    const SubmitResult r = engine.submit(buy(OrderId{2}, px(105), Quantity{5}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].price == px(100));
    CHECK(r.fully_filled());
}

TEST_CASE("A crossing sell trades at the resting bid's price") {
    MatchingEngine engine;
    engine.submit(buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    const SubmitResult r = engine.submit(sell(OrderId{2}, px(95), Quantity{5}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].maker_id == OrderId{1});
    CHECK(r.trades[0].taker_id == OrderId{2});
    CHECK(r.trades[0].price == px(100));
    CHECK(engine.book().empty());
}

TEST_CASE("Partial fill: incoming larger than resting leaves a remainder resting") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{3}, Sequence{1}));

    const SubmitResult r = engine.submit(buy(OrderId{2}, px(100), Quantity{10}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].quantity == Quantity{3});
    CHECK(r.filled_quantity == Quantity{3});
    CHECK(r.remaining_quantity == Quantity{7});
    CHECK(r.resting);

    // Ask side consumed; the buy remainder now rests as the best bid.
    CHECK(engine.book().best_ask() == nullptr);
    REQUIRE(engine.book().best_bid() != nullptr);
    CHECK(engine.book().best_bid()->price() == px(100));
    CHECK(engine.book().best_bid()->total_quantity() == Quantity{7});
}

TEST_CASE("Partial fill: incoming smaller than resting leaves the maker resting") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{10}, Sequence{1}));

    const SubmitResult r = engine.submit(buy(OrderId{2}, px(100), Quantity{4}, Sequence{2}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].quantity == Quantity{4});
    CHECK(r.fully_filled());
    CHECK_FALSE(r.resting);

    // Maker remains with reduced quantity; it is still the front order.
    REQUIRE(engine.book().best_ask() != nullptr);
    CHECK(engine.book().best_ask()->total_quantity() == Quantity{6});
    CHECK(engine.book().best_ask()->front().id() == OrderId{1});
    CHECK(engine.book().best_ask()->front().remaining_quantity() == Quantity{6});
}

TEST_CASE("One incoming order consumes multiple resting orders at the same price") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{3}, Sequence{1}));
    engine.submit(sell(OrderId{2}, px(100), Quantity{4}, Sequence{2}));

    const SubmitResult r = engine.submit(buy(OrderId{3}, px(100), Quantity{7}, Sequence{3}));

    REQUIRE(r.trades.size() == 2);
    // FIFO: the older maker (seq 1) trades first.
    CHECK(r.trades[0].maker_id == OrderId{1});
    CHECK(r.trades[0].quantity == Quantity{3});
    CHECK(r.trades[1].maker_id == OrderId{2});
    CHECK(r.trades[1].quantity == Quantity{4});
    CHECK(r.fully_filled());
    CHECK(engine.book().empty());
}

TEST_CASE("Matching walks price levels from best to worst") {
    MatchingEngine engine;
    // Two ask levels; 100 is better than 101 and must be consumed first.
    engine.submit(sell(OrderId{1}, px(101), Quantity{5}, Sequence{1}));
    engine.submit(sell(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    const SubmitResult r = engine.submit(buy(OrderId{3}, px(101), Quantity{8}, Sequence{3}));

    REQUIRE(r.trades.size() == 2);
    CHECK(r.trades[0].price == px(100));   // best (lowest) ask first
    CHECK(r.trades[0].quantity == Quantity{5});
    CHECK(r.trades[1].price == px(101));   // then the next level
    CHECK(r.trades[1].quantity == Quantity{3});
    CHECK(r.fully_filled());

    // Level 100 fully consumed and removed; 101 partially remains.
    CHECK(engine.book().level_at(Side::Sell, px(100)) == nullptr);
    REQUIRE(engine.book().best_ask() != nullptr);
    CHECK(engine.book().best_ask()->price() == px(101));
    CHECK(engine.book().best_ask()->total_quantity() == Quantity{2});
}

TEST_CASE("FIFO priority holds when only part of a level is consumed") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    engine.submit(sell(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    // Consume 5 + part of the second maker.
    const SubmitResult r = engine.submit(buy(OrderId{3}, px(100), Quantity{7}, Sequence{3}));

    REQUIRE(r.trades.size() == 2);
    CHECK(r.trades[0].maker_id == OrderId{1});
    CHECK(r.trades[1].maker_id == OrderId{2});
    CHECK(r.trades[1].quantity == Quantity{2});

    // First maker gone; second is now the front with 3 remaining.
    REQUIRE(engine.book().best_ask() != nullptr);
    CHECK(engine.book().best_ask()->order_count() == 1);
    CHECK(engine.book().best_ask()->front().id() == OrderId{2});
    CHECK(engine.book().best_ask()->front().remaining_quantity() == Quantity{3});
}

TEST_CASE("A fully filled resting order and its empty level are removed") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    REQUIRE(engine.book().level_at(Side::Sell, px(100)) != nullptr);

    engine.submit(buy(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    CHECK(engine.book().level_at(Side::Sell, px(100)) == nullptr);
    CHECK(engine.book().level_count(Side::Sell) == 0);
    CHECK(engine.book().empty());
}

TEST_CASE("A partially crossing order fills what it can and rests the rest") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{4}, Sequence{1}));
    engine.submit(sell(OrderId{2}, px(102), Quantity{4}, Sequence{2}));

    // Buy limited to 100: only the 100 level is executable.
    const SubmitResult r = engine.submit(buy(OrderId{3}, px(100), Quantity{10}, Sequence{3}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].price == px(100));
    CHECK(r.filled_quantity == Quantity{4});
    CHECK(r.remaining_quantity == Quantity{6});
    CHECK(r.resting);

    // Remainder rests as a bid at 100; the 102 ask is untouched.
    REQUIRE(engine.book().best_bid() != nullptr);
    CHECK(engine.book().best_bid()->price() == px(100));
    CHECK(engine.book().best_bid()->total_quantity() == Quantity{6});
    REQUIRE(engine.book().best_ask() != nullptr);
    CHECK(engine.book().best_ask()->price() == px(102));
}

TEST_CASE("Trade records carry maker and taker sequence information") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{11}));

    const SubmitResult r = engine.submit(buy(OrderId{2}, px(100), Quantity{5}, Sequence{22}));

    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].maker_sequence == Sequence{11});
    CHECK(r.trades[0].taker_sequence == Sequence{22});
}

TEST_CASE("Caller-owned submission results match the owning API and reuse storage") {
    MatchingEngine reference, engine;
    SubmitResult result;
    result.trades.reserve(128);
    const std::size_t capacity = result.trades.capacity();
    const auto check = [&](Order order) {
        const SubmitResult expected = reference.submit(order);
        engine.submit(order, result);
        CHECK(result.order_id == expected.order_id);
        CHECK(result.trades == expected.trades);
        CHECK(result.filled_quantity == expected.filled_quantity);
        CHECK(result.remaining_quantity == expected.remaining_quantity);
        CHECK(result.resting == expected.resting);
        CHECK(result.trades.capacity() == capacity);
    };

    check(Order(OrderId{1}, Side::Buy, OrderType::Market, Price{}, Quantity{1}, Sequence{1}));
    for (std::uint64_t i = 2; i < 130; ++i) {
        check(sell(OrderId{i}, px(100), Quantity{2}, Sequence{i}));
    }
    check(buy(OrderId{130}, px(100), Quantity{1}, Sequence{130}));
    REQUIRE(result.trades.size() == 1);
    const Trade* storage = result.trades.data();
    check(Order(OrderId{131}, Side::Buy, OrderType::Market, Price{}, Quantity{300}, Sequence{131}));
    CHECK(result.trades.size() == 128);
    CHECK(result.trades.data() == storage);
    check(buy(OrderId{132}, px(99), Quantity{3}, Sequence{132}));
    CHECK(result.trades.empty());
    CHECK(result.resting);
    check(sell(OrderId{133}, px(99), Quantity{5}, Sequence{133}));
    CHECK(result.trades.size() == 1);
    CHECK(result.resting);
    CHECK(result.remaining_quantity == Quantity{2});
    CHECK(result.trades.data() == storage);
}

TEST_CASE("Submission results own their trades independently of later submissions and engine lifetime") {
    SubmitResult owned, reused;
    {
        MatchingEngine engine;
        engine.submit(sell(OrderId{1}, px(100), Quantity{1}, Sequence{1}));
        owned = engine.submit(buy(OrderId{2}, px(100), Quantity{1}, Sequence{2}));
        engine.submit(sell(OrderId{3}, px(101), Quantity{1}, Sequence{3}), reused);
        engine.submit(buy(OrderId{4}, px(101), Quantity{1}, Sequence{4}), reused);
    }
    REQUIRE(owned.trades.size() == 1);
    REQUIRE(reused.trades.size() == 1);
    CHECK(owned.trades[0].maker_id == OrderId{1});
    CHECK(owned.trades[0].price == px(100));
    CHECK(reused.trades[0].maker_id == OrderId{3});
    CHECK(reused.trades[0].price == px(101));
}

TEST_CASE("A caller-owned result remains reusable after a duplicate order error") {
    MatchingEngine engine;
    SubmitResult result;
    const Order maker = sell(OrderId{1}, px(100), Quantity{1}, Sequence{1});
    engine.submit(maker, result);
    CHECK_THROWS_AS(engine.submit(maker, result), std::invalid_argument);
    engine.submit(buy(OrderId{2}, px(100), Quantity{1}, Sequence{2}), result);
    REQUIRE(result.trades.size() == 1);
    CHECK(result.trades[0].maker_id == OrderId{1});
    CHECK_FALSE(result.resting);
    CHECK(result.remaining_quantity.is_zero());
}
