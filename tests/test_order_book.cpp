#include <doctest/doctest.h>

#include <stdexcept>

#include "lob/order_book.hpp"

using namespace lob;

namespace {

Order make(OrderId id, Side side, Price price, Quantity qty, Sequence seq) {
    return Order(id, side, OrderType::Limit, price, qty, seq);
}

Order bid(OrderId id, Price price, Quantity qty, Sequence seq) {
    return make(id, Side::Buy, price, qty, seq);
}

Order ask(OrderId id, Price price, Quantity qty, Sequence seq) {
    return make(id, Side::Sell, price, qty, seq);
}

}  // namespace

TEST_CASE("An empty book reports no best levels and zero depth") {
    const OrderBook book;
    CHECK(book.empty());
    CHECK(book.best_bid() == nullptr);
    CHECK(book.best_ask() == nullptr);
    CHECK(book.level_count(Side::Buy) == 0);
    CHECK(book.level_count(Side::Sell) == 0);
    CHECK(book.depth(Side::Buy, 10).empty());
}

TEST_CASE("Inserting a bid and an ask populates each side") {
    OrderBook book;
    book.add_order(bid(OrderId{1}, Price::from_units(100), Quantity{5}, Sequence{1}));
    book.add_order(ask(OrderId{2}, Price::from_units(101), Quantity{7}, Sequence{2}));

    CHECK_FALSE(book.empty());
    REQUIRE(book.best_bid() != nullptr);
    REQUIRE(book.best_ask() != nullptr);
    CHECK(book.best_bid()->price() == Price::from_units(100));
    CHECK(book.best_ask()->price() == Price::from_units(101));
    CHECK(book.best_bid()->total_quantity() == Quantity{5});
    CHECK(book.best_ask()->total_quantity() == Quantity{7});
}

TEST_CASE("Bid price priority: the highest bid is the best bid") {
    OrderBook book;
    book.add_order(bid(OrderId{1}, Price::from_units(100), Quantity{5}, Sequence{1}));
    book.add_order(bid(OrderId{2}, Price::from_units(102), Quantity{5}, Sequence{2}));
    book.add_order(bid(OrderId{3}, Price::from_units(101), Quantity{5}, Sequence{3}));

    REQUIRE(book.best_bid() != nullptr);
    CHECK(book.best_bid()->price() == Price::from_units(102));
    CHECK(book.level_count(Side::Buy) == 3);
}

TEST_CASE("Ask price priority: the lowest ask is the best ask") {
    OrderBook book;
    book.add_order(ask(OrderId{1}, Price::from_units(103), Quantity{5}, Sequence{1}));
    book.add_order(ask(OrderId{2}, Price::from_units(101), Quantity{5}, Sequence{2}));
    book.add_order(ask(OrderId{3}, Price::from_units(102), Quantity{5}, Sequence{3}));

    REQUIRE(book.best_ask() != nullptr);
    CHECK(book.best_ask()->price() == Price::from_units(101));
    CHECK(book.level_count(Side::Sell) == 3);
}

TEST_CASE("FIFO priority is preserved among orders at the same price") {
    OrderBook book;
    const Price p = Price::from_units(100);
    book.add_order(bid(OrderId{10}, p, Quantity{5}, Sequence{1}));
    book.add_order(bid(OrderId{11}, p, Quantity{6}, Sequence{2}));
    book.add_order(bid(OrderId{12}, p, Quantity{7}, Sequence{3}));

    const PriceLevel* level = book.best_bid();
    REQUIRE(level != nullptr);
    CHECK(level->order_count() == 3);
    CHECK(level->total_quantity() == Quantity{18});
    // Oldest (smallest sequence) has priority.
    CHECK(level->front().id() == OrderId{10});
    CHECK(level->orders().back().id() == OrderId{12});
    CHECK(book.level_count(Side::Buy) == 1);
}

TEST_CASE("A specific price level can be queried directly") {
    OrderBook book;
    book.add_order(bid(OrderId{1}, Price::from_units(100), Quantity{5}, Sequence{1}));
    book.add_order(bid(OrderId{2}, Price::from_units(99), Quantity{4}, Sequence{2}));

    const PriceLevel* level = book.level_at(Side::Buy, Price::from_units(99));
    REQUIRE(level != nullptr);
    CHECK(level->price() == Price::from_units(99));
    CHECK(level->total_quantity() == Quantity{4});

    CHECK(book.level_at(Side::Buy, Price::from_units(1234)) == nullptr);
    CHECK(book.level_at(Side::Sell, Price::from_units(100)) == nullptr);
}

TEST_CASE("Depth reports levels best-first, bounded by max_levels") {
    OrderBook book;
    book.add_order(bid(OrderId{1}, Price::from_units(100), Quantity{5}, Sequence{1}));
    book.add_order(bid(OrderId{2}, Price::from_units(102), Quantity{6}, Sequence{2}));
    book.add_order(bid(OrderId{3}, Price::from_units(101), Quantity{7}, Sequence{3}));

    const std::vector<LevelView> depth = book.depth(Side::Buy, 2);
    REQUIRE(depth.size() == 2);
    CHECK(depth[0].price == Price::from_units(102));
    CHECK(depth[0].quantity == Quantity{6});
    CHECK(depth[0].order_count == 1);
    CHECK(depth[1].price == Price::from_units(101));

    // Asking for more levels than exist returns all of them.
    CHECK(book.depth(Side::Buy, 100).size() == 3);
}

TEST_CASE("Ask depth is ordered lowest price first") {
    OrderBook book;
    book.add_order(ask(OrderId{1}, Price::from_units(103), Quantity{5}, Sequence{1}));
    book.add_order(ask(OrderId{2}, Price::from_units(101), Quantity{6}, Sequence{2}));

    const std::vector<LevelView> depth = book.depth(Side::Sell, 10);
    REQUIRE(depth.size() == 2);
    CHECK(depth[0].price == Price::from_units(101));
    CHECK(depth[1].price == Price::from_units(103));
}

TEST_CASE("Orders at the same price aggregate into one level") {
    OrderBook book;
    const Price p = Price::from_units(100);
    book.add_order(ask(OrderId{1}, p, Quantity{5}, Sequence{1}));
    book.add_order(ask(OrderId{2}, p, Quantity{5}, Sequence{2}));

    CHECK(book.level_count(Side::Sell) == 1);
    CHECK(book.best_ask()->total_quantity() == Quantity{10});
}

TEST_CASE("Adding a duplicate order id is rejected") {
    OrderBook book;
    book.add_order(bid(OrderId{1}, Price::from_units(100), Quantity{5}, Sequence{1}));
    CHECK_THROWS_AS(
        book.add_order(bid(OrderId{1}, Price::from_units(99), Quantity{5}, Sequence{2})),
        std::invalid_argument);
}
