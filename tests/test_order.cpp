#include <doctest/doctest.h>

#include <stdexcept>

#include "lob/order.hpp"

using namespace lob;

namespace {

// A conventional, valid limit order used as the basis for the tests below.
Order make_order() {
    return Order(OrderId{1},
                 Side::Buy,
                 OrderType::Limit,
                 Price::from_units(100, 5000),  // 100.5000
                 Quantity{50},
                 Sequence{7});
}

}  // namespace

TEST_CASE("Order construction stores every field") {
    const Order order = make_order();

    CHECK(order.id() == OrderId{1});
    CHECK(order.side() == Side::Buy);
    CHECK(order.type() == OrderType::Limit);
    CHECK(order.price() == Price::from_units(100, 5000));
    CHECK(order.quantity() == Quantity{50});
    CHECK(order.sequence() == Sequence{7});
}

TEST_CASE("A fresh order is fully unfilled") {
    const Order order = make_order();

    CHECK(order.remaining_quantity() == Quantity{50});
    CHECK(order.filled_quantity() == Quantity{0});
    CHECK_FALSE(order.is_filled());
}

TEST_CASE("Order invariant: quantity == filled + remaining") {
    const Order order = make_order();
    CHECK(order.filled_quantity() + order.remaining_quantity() == order.quantity());
}

TEST_CASE("Sell orders are representable") {
    const Order order(OrderId{2},
                      Side::Sell,
                      OrderType::Limit,
                      Price::from_units(101),
                      Quantity{10},
                      Sequence{8});
    CHECK(order.side() == Side::Sell);
}

TEST_CASE("Construction rejects a zero (invalid) order id") {
    CHECK_THROWS_AS(Order(OrderId{0},
                          Side::Buy,
                          OrderType::Limit,
                          Price::from_units(100),
                          Quantity{10},
                          Sequence{1}),
                    std::invalid_argument);
}

TEST_CASE("Construction rejects a non-positive price") {
    CHECK_THROWS_AS(Order(OrderId{1},
                          Side::Buy,
                          OrderType::Limit,
                          Price::from_ticks(0),
                          Quantity{10},
                          Sequence{1}),
                    std::invalid_argument);

    CHECK_THROWS_AS(Order(OrderId{1},
                          Side::Buy,
                          OrderType::Limit,
                          Price::from_ticks(-5),
                          Quantity{10},
                          Sequence{1}),
                    std::invalid_argument);
}

TEST_CASE("Construction rejects a zero quantity") {
    CHECK_THROWS_AS(Order(OrderId{1},
                          Side::Buy,
                          OrderType::Limit,
                          Price::from_units(100),
                          Quantity{0},
                          Sequence{1}),
                    std::invalid_argument);
}

TEST_CASE("A valid order does not throw") {
    CHECK_NOTHROW(make_order());
}
