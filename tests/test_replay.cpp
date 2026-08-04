#include <doctest/doctest.h>

#include <variant>

#include "lob/book_snapshot.hpp"
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

// Builds a representative session: several limit orders across price levels, a
// market order that sweeps liquidity, a partial fill, and a cancellation.
RecordingEngine build_session() {
    RecordingEngine rec;
    rec.submit(limit_sell(OrderId{1}, px(101), Quantity{5}, Sequence{1}));
    rec.submit(limit_sell(OrderId{2}, px(102), Quantity{5}, Sequence{2}));
    rec.submit(limit_buy(OrderId{3}, px(99), Quantity{4}, Sequence{3}));    // rests
    rec.submit(limit_buy(OrderId{4}, px(101), Quantity{3}, Sequence{4}));   // partial vs #1
    rec.submit(market_buy(OrderId{5}, Quantity{4}, Sequence{5}));           // sweeps 101 then 102
    rec.cancel(OrderId{3});                                                 // cancel the resting bid
    return rec;
}

}  // namespace

TEST_CASE("A submission is recorded as a Submission event") {
    RecordingEngine rec;
    rec.submit(limit_buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    REQUIRE(rec.log().size() == 1);
    CHECK(rec.log()[0].type() == MarketEventType::Submission);
    CHECK(rec.log()[0].index == 0);
}

TEST_CASE("A trade is recorded as Submission then Execution, in order") {
    RecordingEngine rec;
    rec.submit(limit_sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    rec.submit(limit_buy(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    REQUIRE(rec.log().size() == 3);
    CHECK(rec.log()[0].type() == MarketEventType::Submission);   // sell #1
    CHECK(rec.log()[1].type() == MarketEventType::Submission);   // buy #2
    CHECK(rec.log()[2].type() == MarketEventType::Execution);    // the fill

    const auto& exec = std::get<ExecutionEvent>(rec.log()[2].payload);
    CHECK(exec.trade.maker_id == OrderId{1});
    CHECK(exec.trade.taker_id == OrderId{2});
    CHECK(exec.trade.price == px(100));
}

TEST_CASE("Only effective cancellations are recorded") {
    RecordingEngine rec;
    rec.submit(limit_buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    CHECK(rec.cancel(OrderId{999}).status == CancelStatus::NotFound);   // no event
    REQUIRE(rec.log().size() == 1);

    CHECK(rec.cancel(OrderId{1}).ok());                                 // records event
    REQUIRE(rec.log().size() == 2);
    CHECK(rec.log()[1].type() == MarketEventType::Cancellation);
}

TEST_CASE("Event indices are a strictly increasing, gap-free sequence") {
    RecordingEngine rec = build_session();
    std::uint64_t expected = 0;
    for (const MarketEvent& e : rec.log()) {
        CHECK(e.index == expected);
        ++expected;
    }
}

TEST_CASE("Replaying a plain sequence of limit orders reproduces the book") {
    RecordingEngine rec;
    rec.submit(limit_buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    rec.submit(limit_buy(OrderId{2}, px(99), Quantity{6}, Sequence{2}));
    rec.submit(limit_sell(OrderId{3}, px(103), Quantity{7}, Sequence{3}));

    const RecordingEngine replayed = replay(rec.log());
    CHECK(BookSnapshot::of(replayed.book()) == BookSnapshot::of(rec.book()));
}

TEST_CASE("Replaying cancellations reproduces the post-cancel book") {
    RecordingEngine rec;
    rec.submit(limit_buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    rec.submit(limit_buy(OrderId{2}, px(100), Quantity{6}, Sequence{2}));
    rec.cancel(OrderId{1});

    const RecordingEngine replayed = replay(rec.log());
    const BookSnapshot snap = BookSnapshot::of(replayed.book());
    REQUIRE(snap.bids.size() == 1);
    REQUIRE(snap.bids[0].orders.size() == 1);
    CHECK(snap.bids[0].orders[0].id == OrderId{2});   // survivor kept
    CHECK(snap == BookSnapshot::of(rec.book()));
}

TEST_CASE("Replaying market executions and partial fills matches the original") {
    RecordingEngine rec = build_session();
    const RecordingEngine replayed = replay(rec.log());
    CHECK(BookSnapshot::of(replayed.book()) == BookSnapshot::of(rec.book()));
}

TEST_CASE("Replay across multiple price levels matches the original") {
    RecordingEngine rec;
    for (int i = 0; i < 5; ++i) {
        rec.submit(limit_sell(OrderId{static_cast<std::uint64_t>(i + 1)},
                              px(100 + i), Quantity{2},
                              Sequence{static_cast<std::uint64_t>(i + 1)}));
    }
    rec.submit(market_buy(OrderId{99}, Quantity{5}, Sequence{99}));   // sweeps 3 levels

    const RecordingEngine replayed = replay(rec.log());
    CHECK(BookSnapshot::of(replayed.book()) == BookSnapshot::of(rec.book()));
    CHECK(replayed.book().level_count(Side::Sell) == rec.book().level_count(Side::Sell));
}

TEST_CASE("Replay is deterministic: it regenerates an identical event log") {
    RecordingEngine rec = build_session();

    const RecordingEngine once = replay(rec.log());
    const RecordingEngine twice = replay(rec.log());

    // Replaying the recorded commands regenerates the same events (submissions,
    // trades, cancellations) with the same indices.
    CHECK(once.log() == rec.log());
    CHECK(once.log() == twice.log());
    CHECK(BookSnapshot::of(once.book()) == BookSnapshot::of(twice.book()));
}

TEST_CASE("Replay survives a serialization round-trip and matches the original book") {
    RecordingEngine rec = build_session();

    const EventLog reloaded = EventLog::from_string(rec.log().to_string());
    const RecordingEngine replayed = replay(reloaded);

    CHECK(replayed.log() == rec.log());
    CHECK(BookSnapshot::of(replayed.book()) == BookSnapshot::of(rec.book()));
}

TEST_CASE("Replaying an empty event stream yields an empty book") {
    const EventLog empty;
    const RecordingEngine replayed = replay(empty);

    CHECK(replayed.book().empty());
    CHECK(replayed.log().empty());
    CHECK(BookSnapshot::of(replayed.book()) == BookSnapshot{});
}
