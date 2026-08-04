#include <doctest/doctest.h>

#include <stdexcept>
#include <string>

#include "lob/event_log.hpp"
#include "lob/recording_engine.hpp"

using namespace lob;

namespace {

Order limit_buy(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Buy, OrderType::Limit, price, qty, seq);
}

Order limit_sell(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Sell, OrderType::Limit, price, qty, seq);
}

Order market_buy(OrderId id, Quantity qty, Sequence seq) {
    return Order(id, Side::Buy, OrderType::Market, Price{}, qty, seq);
}

Price px(std::int64_t units) { return Price::from_units(units); }

}  // namespace

TEST_CASE("An empty log serializes to just a header and round-trips") {
    const EventLog log;
    CHECK(log.empty());

    const EventLog parsed = EventLog::from_string(log.to_string());
    CHECK(parsed.empty());
    CHECK(parsed == log);
}

TEST_CASE("A log of submissions, a trade, and a cancellation round-trips exactly") {
    RecordingEngine rec;
    rec.submit(limit_sell(OrderId{1}, px(101), Quantity{5}, Sequence{1}));
    rec.submit(limit_buy(OrderId{2}, px(100), Quantity{4}, Sequence{2}));   // rests
    rec.submit(market_buy(OrderId{3}, Quantity{5}, Sequence{3}));           // trades vs #1
    rec.cancel(OrderId{2});                                                 // cancels the resting bid

    const EventLog& original = rec.log();
    const EventLog parsed = EventLog::from_string(original.to_string());
    CHECK(parsed == original);
}

TEST_CASE("Serialization preserves fixed-point prices exactly via ticks") {
    RecordingEngine rec;
    rec.submit(limit_buy(OrderId{1}, Price::from_units(100, 2500), Quantity{7}, Sequence{1}));

    const EventLog parsed = EventLog::from_string(rec.log().to_string());
    REQUIRE(parsed.size() == 1);
    const auto& sub = std::get<SubmissionEvent>(parsed[0].payload);
    CHECK(sub.order.price() == Price::from_units(100, 2500));
    CHECK(sub.order.quantity() == Quantity{7});
}

TEST_CASE("Deserializing rejects a missing or wrong header") {
    CHECK_THROWS_AS(EventLog::from_string(""), std::runtime_error);
    CHECK_THROWS_AS(EventLog::from_string("NOT A LOG\n"), std::runtime_error);
}

TEST_CASE("Deserializing rejects an unknown event tag") {
    CHECK_THROWS_AS(EventLog::from_string("LOBLOG v1\n0 FROBNICATE 1\n"),
                    std::runtime_error);
}

TEST_CASE("Deserializing rejects a truncated event line") {
    // SUBMIT missing most of its fields.
    CHECK_THROWS_AS(EventLog::from_string("LOBLOG v1\n0 SUBMIT 1 B\n"),
                    std::runtime_error);
}

TEST_CASE("Deserializing rejects an event that violates order invariants") {
    // A limit order with a non-positive price is invalid.
    CHECK_THROWS_AS(EventLog::from_string("LOBLOG v1\n0 SUBMIT 1 B L 0 5 1\n"),
                    std::runtime_error);
}

TEST_CASE("Blank lines and comments are ignored on parse") {
    const std::string text =
        "LOBLOG v1\n"
        "\n"
        "# a comment\n"
        "0 CANCEL 42\n";
    const EventLog log = EventLog::from_string(text);
    REQUIRE(log.size() == 1);
    CHECK(log[0].type() == MarketEventType::Cancellation);
}
