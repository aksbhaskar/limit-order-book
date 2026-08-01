#include <doctest/doctest.h>

#include "lob/price.hpp"

using namespace lob;

TEST_CASE("Price is constructed from ticks and whole units") {
    CHECK(Price::from_units(101).ticks() == 101 * Price::kTicksPerUnit);
    CHECK(Price::from_units(101, 2500).ticks() == 101 * Price::kTicksPerUnit + 2500);
    CHECK(Price::from_ticks(1'012'500).ticks() == 1'012'500);
}

TEST_CASE("Price default constructs to zero and is not positive") {
    CHECK(Price{}.ticks() == 0);
    CHECK_FALSE(Price{}.is_positive());
    CHECK(Price::from_ticks(1).is_positive());
    CHECK_FALSE(Price::from_ticks(-1).is_positive());
}

TEST_CASE("Prices compare exactly via their tick representation") {
    const Price a = Price::from_units(100);
    const Price b = Price::from_units(100, 100);  // 100.0100

    CHECK(a < b);
    CHECK(b > a);
    CHECK(a == Price::from_units(100));
    CHECK(a != b);
}

TEST_CASE("Price arithmetic operates on ticks without drift") {
    const Price a = Price::from_units(100, 1000);  // 100.1000
    const Price b = Price::from_units(0, 5000);    // 0.5000

    CHECK((a + b).ticks() == Price::from_units(100, 6000).ticks());
    CHECK((a - b).ticks() == Price::from_units(99, 6000).ticks());
}

TEST_CASE("to_double is a faithful, display-only conversion") {
    CHECK(Price::from_units(101, 2500).to_double() == doctest::Approx(101.25));
}
