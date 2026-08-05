#include <doctest/doctest.h>

#include "lob/pnl_account.hpp"

using namespace lob;

namespace {

Price t(std::int64_t ticks) { return Price::from_ticks(ticks); }

}  // namespace

TEST_CASE("A fresh account is flat with only its starting cash") {
    const PnLAccount acct(1000);
    CHECK(acct.position() == 0);
    CHECK(acct.cash_ticks() == 1000);
    CHECK(acct.realized_ticks() == 0);
    CHECK(acct.total_pnl_ticks(t(100)) == 0);
    CHECK(acct.unrealized_ticks(t(100)) == 0);
}

TEST_CASE("A buy fill builds a long position and unrealized P&L moves with the mark") {
    PnLAccount acct(0);
    acct.apply_fill(Side::Buy, t(100), Quantity{10});

    CHECK(acct.position() == 10);
    CHECK(acct.cash_ticks() == -1000);
    CHECK(acct.average_cost_ticks() == 100);
    CHECK(acct.realized_ticks() == 0);
    CHECK(acct.unrealized_ticks(t(110)) == 100);   // +10 ticks * 10 units
    CHECK(acct.total_pnl_ticks(t(110)) == 100);
}

TEST_CASE("Closing a long books realized P&L and flattens unrealized") {
    PnLAccount acct(0);
    acct.apply_fill(Side::Buy, t(100), Quantity{10});
    acct.apply_fill(Side::Sell, t(110), Quantity{10});

    CHECK(acct.position() == 0);
    CHECK(acct.realized_ticks() == 100);
    CHECK(acct.unrealized_ticks(t(110)) == 0);
    CHECK(acct.total_pnl_ticks(t(110)) == 100);
    CHECK((acct.realized_ticks() + acct.unrealized_ticks(t(110))) ==
          acct.total_pnl_ticks(t(110)));
}

TEST_CASE("A short position profits when the price falls") {
    PnLAccount acct(0);
    acct.apply_fill(Side::Sell, t(100), Quantity{10});   // open short
    CHECK(acct.position() == -10);
    CHECK(acct.cash_ticks() == 1000);
    CHECK(acct.unrealized_ticks(t(90)) == 100);          // short, price down 10

    acct.apply_fill(Side::Buy, t(90), Quantity{10});     // cover
    CHECK(acct.position() == 0);
    CHECK(acct.realized_ticks() == 100);
}

TEST_CASE("Increasing a position updates the weighted average cost") {
    PnLAccount acct(0);
    acct.apply_fill(Side::Buy, t(100), Quantity{10});
    acct.apply_fill(Side::Buy, t(120), Quantity{10});

    CHECK(acct.position() == 20);
    CHECK(acct.average_cost_ticks() == 110);
    CHECK(acct.unrealized_ticks(t(120)) == 200);   // 20 units * (120-110)
}

TEST_CASE("A partial close realizes only the closed portion") {
    PnLAccount acct(0);
    acct.apply_fill(Side::Buy, t(100), Quantity{10});
    acct.apply_fill(Side::Sell, t(130), Quantity{4});

    CHECK(acct.position() == 6);
    CHECK(acct.average_cost_ticks() == 100);       // basis of the remainder unchanged
    CHECK(acct.realized_ticks() == 120);           // 4 units * (130-100)
    CHECK(acct.unrealized_ticks(t(130)) == 180);   // 6 units * (130-100)
}

TEST_CASE("Flipping through zero realizes the old side and reopens at the new price") {
    PnLAccount acct(0);
    acct.apply_fill(Side::Buy, t(100), Quantity{10});    // long 10 @ 100
    acct.apply_fill(Side::Sell, t(130), Quantity{25});   // sell 25 -> short 15 @ 130

    CHECK(acct.position() == -15);
    CHECK(acct.average_cost_ticks() == 130);
    CHECK(acct.realized_ticks() == 300);                 // 10 units * (130-100)
    CHECK(acct.unrealized_ticks(t(130)) == 0);
}

TEST_CASE("A transaction fee is a realized cost") {
    PnLAccount acct(1000);
    acct.apply_fill(Side::Buy, t(100), Quantity{10});
    acct.charge_fee(20);

    CHECK(acct.cash_ticks() == 1000 - 1000 - 20);
    CHECK(acct.realized_ticks() == -20);
    // Identity preserved: realized + unrealized == total.
    CHECK((acct.realized_ticks() + acct.unrealized_ticks(t(100))) ==
          acct.total_pnl_ticks(t(100)));
}
