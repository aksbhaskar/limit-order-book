#include <doctest/doctest.h>

#include "lob/matching_engine.hpp"
#include "lob/order_book.hpp"

using namespace lob;

namespace {

Order buy(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Buy, OrderType::Limit, price, qty, seq);
}

Order sell(OrderId id, Price price, Quantity qty, Sequence seq) {
    return Order(id, Side::Sell, OrderType::Limit, price, qty, seq);
}

Price px(std::int64_t units) { return Price::from_units(units); }

}  // namespace

TEST_CASE("Cancelling the only order at a level removes the level") {
    OrderBook book;
    book.add_order(buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    const CancelResult r = book.cancel(OrderId{1});
    CHECK(r.ok());
    CHECK(r.status == CancelStatus::Cancelled);
    CHECK(r.cancelled_quantity == Quantity{5});

    CHECK(book.empty());
    CHECK(book.best_bid() == nullptr);
    CHECK(book.level_at(Side::Buy, px(100)) == nullptr);
    CHECK(book.level_count(Side::Buy) == 0);
}

TEST_CASE("Cancelling the first order in a FIFO queue promotes the next") {
    OrderBook book;
    const Price p = px(100);
    book.add_order(buy(OrderId{1}, p, Quantity{5}, Sequence{1}));
    book.add_order(buy(OrderId{2}, p, Quantity{6}, Sequence{2}));
    book.add_order(buy(OrderId{3}, p, Quantity{7}, Sequence{3}));

    CHECK(book.cancel(OrderId{1}).cancelled_quantity == Quantity{5});

    const PriceLevel* level = book.best_bid();
    REQUIRE(level != nullptr);
    CHECK(level->order_count() == 2);
    CHECK(level->front().id() == OrderId{2});          // next-oldest promoted
    CHECK(level->orders().back().id() == OrderId{3});  // order preserved
    CHECK(level->total_quantity() == Quantity{13});
}

TEST_CASE("Cancelling a middle order preserves the order of the rest") {
    OrderBook book;
    const Price p = px(100);
    book.add_order(buy(OrderId{1}, p, Quantity{5}, Sequence{1}));
    book.add_order(buy(OrderId{2}, p, Quantity{6}, Sequence{2}));
    book.add_order(buy(OrderId{3}, p, Quantity{7}, Sequence{3}));

    CHECK(book.cancel(OrderId{2}).ok());

    const PriceLevel* level = book.best_bid();
    REQUIRE(level != nullptr);
    CHECK(level->order_count() == 2);
    CHECK(level->front().id() == OrderId{1});          // still first
    CHECK(level->orders().back().id() == OrderId{3});  // still last, order intact
    CHECK(level->total_quantity() == Quantity{12});
}

TEST_CASE("Cancelling the last order in a queue leaves the rest intact") {
    OrderBook book;
    const Price p = px(100);
    book.add_order(buy(OrderId{1}, p, Quantity{5}, Sequence{1}));
    book.add_order(buy(OrderId{2}, p, Quantity{6}, Sequence{2}));

    CHECK(book.cancel(OrderId{2}).ok());

    const PriceLevel* level = book.best_bid();
    REQUIRE(level != nullptr);
    CHECK(level->order_count() == 1);
    CHECK(level->front().id() == OrderId{1});
    CHECK(level->total_quantity() == Quantity{5});
}

TEST_CASE("Cancelling an unknown order reports NotFound") {
    OrderBook book;
    book.add_order(buy(OrderId{1}, px(100), Quantity{5}, Sequence{1}));

    const CancelResult r = book.cancel(OrderId{999});
    CHECK_FALSE(r.ok());
    CHECK(r.status == CancelStatus::NotFound);
    CHECK(r.cancelled_quantity == Quantity{0});
    // The book is untouched.
    CHECK(book.best_bid()->total_quantity() == Quantity{5});
}

TEST_CASE("An order can be cancelled after a partial fill, removing what remains") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{10}, Sequence{1}));
    // Partially fill the resting sell: 4 of 10 trade, 6 remain resting.
    engine.submit(buy(OrderId{2}, px(100), Quantity{4}, Sequence{2}));
    REQUIRE(engine.book().best_ask() != nullptr);
    REQUIRE(engine.book().best_ask()->total_quantity() == Quantity{6});

    const CancelResult r = engine.cancel(OrderId{1});
    CHECK(r.ok());
    CHECK(r.cancelled_quantity == Quantity{6});   // only the remaining qty
    CHECK(engine.book().empty());
}

TEST_CASE("Cancelling a fully filled order reports AlreadyFilled") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    engine.submit(buy(OrderId{2}, px(100), Quantity{5}, Sequence{2}));  // fully fills #1
    REQUIRE(engine.book().empty());

    const CancelResult r = engine.cancel(OrderId{1});
    CHECK_FALSE(r.ok());
    CHECK(r.status == CancelStatus::AlreadyFilled);
    CHECK(r.cancelled_quantity == Quantity{0});
}

TEST_CASE("Aggregate quantity is updated when one of several orders is cancelled") {
    OrderBook book;
    const Price p = px(100);
    book.add_order(sell(OrderId{1}, p, Quantity{5}, Sequence{1}));
    book.add_order(sell(OrderId{2}, p, Quantity{8}, Sequence{2}));
    REQUIRE(book.best_ask()->total_quantity() == Quantity{13});

    book.cancel(OrderId{1});
    CHECK(book.best_ask()->total_quantity() == Quantity{8});
    CHECK(book.best_ask()->order_count() == 1);
}

TEST_CASE("The best bid/ask is recomputed correctly after cancellation") {
    OrderBook book;
    book.add_order(buy(OrderId{1}, px(102), Quantity{5}, Sequence{1}));  // best bid
    book.add_order(buy(OrderId{2}, px(101), Quantity{5}, Sequence{2}));

    REQUIRE(book.best_bid()->price() == px(102));
    book.cancel(OrderId{1});   // removes the top of book

    REQUIRE(book.best_bid() != nullptr);
    CHECK(book.best_bid()->price() == px(101));   // next level becomes best
    CHECK(book.level_count(Side::Buy) == 1);
}

TEST_CASE("Cancellation interacts correctly with subsequent matching") {
    MatchingEngine engine;
    // Two asks at different prices; cancel the better one, then submit a buy.
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    engine.submit(sell(OrderId{2}, px(101), Quantity{5}, Sequence{2}));

    CHECK(engine.cancel(OrderId{1}).ok());

    // The 100 ask is gone; a buy at 100 no longer crosses anything and rests.
    const SubmitResult r = engine.submit(buy(OrderId{3}, px(100), Quantity{5}, Sequence{3}));
    CHECK(r.trades.empty());
    CHECK(r.resting);
    CHECK(engine.book().best_bid()->price() == px(100));
    CHECK(engine.book().best_ask()->price() == px(101));

    // Cancelling an already-cancelled order now reports NotFound.
    CHECK(engine.cancel(OrderId{1}).status == CancelStatus::NotFound);
}

TEST_CASE("A cancelled resting order no longer participates in matching") {
    MatchingEngine engine;
    engine.submit(sell(OrderId{1}, px(100), Quantity{5}, Sequence{1}));
    engine.submit(sell(OrderId{2}, px(100), Quantity{5}, Sequence{2}));

    // Cancel the FIFO-front maker; the second should now be first to trade.
    CHECK(engine.cancel(OrderId{1}).ok());

    const SubmitResult r = engine.submit(buy(OrderId{3}, px(100), Quantity{5}, Sequence{3}));
    REQUIRE(r.trades.size() == 1);
    CHECK(r.trades[0].maker_id == OrderId{2});
    CHECK(engine.book().empty());
}
