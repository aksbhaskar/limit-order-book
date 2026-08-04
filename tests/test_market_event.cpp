#include <doctest/doctest.h>

#include <variant>

#include "lob/market_event.hpp"

using namespace lob;

namespace {

Order some_order() {
    return Order(OrderId{1}, Side::Buy, OrderType::Limit,
                 Price::from_units(100), Quantity{5}, Sequence{1});
}

}  // namespace

TEST_CASE("MarketEvent reports the type matching its payload") {
    const MarketEvent submit{0, SubmissionEvent{some_order()}};
    const MarketEvent cancel{1, CancellationEvent{OrderId{1}}};
    const MarketEvent trade{2, ExecutionEvent{Trade{}}};

    CHECK(submit.type() == MarketEventType::Submission);
    CHECK(cancel.type() == MarketEventType::Cancellation);
    CHECK(trade.type() == MarketEventType::Execution);
}

TEST_CASE("Events carry the expected typed payload") {
    const MarketEvent submit{7, SubmissionEvent{some_order()}};
    REQUIRE(std::holds_alternative<SubmissionEvent>(submit.payload));
    CHECK(std::get<SubmissionEvent>(submit.payload).order == some_order());
    CHECK(submit.index == 7);
}

TEST_CASE("Market events compare by index and payload") {
    const MarketEvent a{0, SubmissionEvent{some_order()}};
    const MarketEvent b{0, SubmissionEvent{some_order()}};
    const MarketEvent c{1, SubmissionEvent{some_order()}};      // different index
    const MarketEvent d{0, CancellationEvent{OrderId{1}}};      // different payload

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a != d);
}
