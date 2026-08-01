#include <doctest/doctest.h>

#include "lob/quantity.hpp"

using namespace lob;

TEST_CASE("Quantity exposes its underlying value") {
    CHECK(Quantity{100}.value() == 100u);
    CHECK(Quantity{}.value() == 0u);
}

TEST_CASE("Quantity knows when it is zero") {
    CHECK(Quantity{}.is_zero());
    CHECK(Quantity{0}.is_zero());
    CHECK_FALSE(Quantity{1}.is_zero());
}

TEST_CASE("Quantities compare by size") {
    CHECK(Quantity{10} < Quantity{20});
    CHECK(Quantity{20} > Quantity{10});
    CHECK(Quantity{10} == Quantity{10});
    CHECK(Quantity{10} != Quantity{11});
}

TEST_CASE("Quantity arithmetic adds and subtracts sizes") {
    CHECK((Quantity{30} + Quantity{12}) == Quantity{42});
    CHECK((Quantity{30} - Quantity{12}) == Quantity{18});
}
