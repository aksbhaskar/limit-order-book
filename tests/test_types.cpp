#include <doctest/doctest.h>

#include <cstdint>

#include "lob/types.hpp"

using namespace lob;

TEST_CASE("Side renders to a readable string") {
    CHECK(to_string(Side::Buy) == "Buy");
    CHECK(to_string(Side::Sell) == "Sell");
}

TEST_CASE("OrderType renders to a readable string") {
    CHECK(to_string(OrderType::Limit) == "Limit");
}

TEST_CASE("OrderId carries identity and ordering but not arithmetic") {
    CHECK(OrderId{1} == OrderId{1});
    CHECK(OrderId{1} != OrderId{2});
    CHECK(OrderId{1} < OrderId{2});
    // Underlying representation is a 64-bit integer.
    CHECK(static_cast<std::uint64_t>(OrderId{42}) == 42u);
}

TEST_CASE("Sequence numbers order by arrival for time priority") {
    CHECK(Sequence{10} < Sequence{11});
    CHECK(Sequence{11} > Sequence{10});
    CHECK(Sequence{5} == Sequence{5});
}
