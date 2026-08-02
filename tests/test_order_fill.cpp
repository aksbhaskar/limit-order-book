#include <doctest/doctest.h>

#include <stdexcept>

#include "lob/order.hpp"

using namespace lob;

namespace {

Order fresh(Quantity qty) {
    return Order(OrderId{1}, Side::Buy, OrderType::Limit,
                 Price::from_units(100), qty, Sequence{1});
}

}  // namespace

TEST_CASE("A partial fill reduces remaining but keeps original quantity") {
    Order order = fresh(Quantity{10});
    order.fill(Quantity{3});

    CHECK(order.quantity() == Quantity{10});
    CHECK(order.remaining_quantity() == Quantity{7});
    CHECK(order.filled_quantity() == Quantity{3});
    CHECK_FALSE(order.is_filled());
}

TEST_CASE("Successive fills accumulate and can fully fill the order") {
    Order order = fresh(Quantity{10});
    order.fill(Quantity{4});
    order.fill(Quantity{6});

    CHECK(order.remaining_quantity() == Quantity{0});
    CHECK(order.filled_quantity() == Quantity{10});
    CHECK(order.is_filled());
}

TEST_CASE("Filling more than remaining throws and leaves the order unchanged") {
    Order order = fresh(Quantity{5});
    CHECK_THROWS_AS(order.fill(Quantity{6}), std::invalid_argument);
    CHECK(order.remaining_quantity() == Quantity{5});
}

TEST_CASE("A zero fill is rejected") {
    Order order = fresh(Quantity{5});
    CHECK_THROWS_AS(order.fill(Quantity{0}), std::invalid_argument);
}
