#include <doctest/doctest.h>

#include "lob/price_level.hpp"

using namespace lob;

namespace {

Order limit_order(OrderId id, Quantity qty, Sequence seq, Price price) {
    return Order(id, Side::Buy, OrderType::Limit, price, qty, seq);
}

}  // namespace

TEST_CASE("A new price level is empty and remembers its price") {
    const PriceLevel level(Price::from_units(100));
    CHECK(level.price() == Price::from_units(100));
    CHECK(level.empty());
    CHECK(level.order_count() == 0);
    CHECK(level.total_quantity() == Quantity{0});
}

TEST_CASE("Adding orders accumulates count and aggregate quantity") {
    PriceLevel level(Price::from_units(100));
    level.add(limit_order(OrderId{1}, Quantity{5}, Sequence{1}, Price::from_units(100)));
    level.add(limit_order(OrderId{2}, Quantity{7}, Sequence{2}, Price::from_units(100)));

    CHECK_FALSE(level.empty());
    CHECK(level.order_count() == 2);
    CHECK(level.total_quantity() == Quantity{12});
}

TEST_CASE("Orders keep FIFO order: the front is the oldest") {
    PriceLevel level(Price::from_units(100));
    level.add(limit_order(OrderId{10}, Quantity{5}, Sequence{1}, Price::from_units(100)));
    level.add(limit_order(OrderId{11}, Quantity{5}, Sequence{2}, Price::from_units(100)));

    CHECK(level.front().id() == OrderId{10});
    CHECK(level.orders().front().id() == OrderId{10});
    CHECK(level.orders().back().id() == OrderId{11});
}
