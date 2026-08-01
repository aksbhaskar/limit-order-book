#include <doctest/doctest.h>

#include "lob/version.hpp"

// A minimal sanity test proving the test harness is wired up and the library
// headers are reachable. Real behavioural tests arrive alongside the code they
// cover.
TEST_CASE("library version is exposed") {
    CHECK(lob::kVersionMajor == 0);
    CHECK(lob::kVersionMinor == 1);
    CHECK(lob::kVersionString == "0.1.0");
}
