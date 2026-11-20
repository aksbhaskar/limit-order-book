#include <doctest/doctest.h>

#include <algorithm>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

#include "lob/book_snapshot.hpp"
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

TEST_CASE("Cancellation handles survive index growth and unrelated level removal") {
    Side side = Side::Buy;
    SUBCASE("bids") { side = Side::Buy; }
    SUBCASE("asks") { side = Side::Sell; }

    OrderBook book;
    constexpr std::size_t n = 2048;
    const Price p = px(100);
    for (std::size_t i = 1; i <= n; ++i) {
        book.add_order(Order(OrderId{i}, side, OrderType::Limit, p,
                             Quantity{3}, Sequence{i}));
    }
    for (std::size_t i = n + 1; i <= n + 128; ++i) {
        book.add_order(Order(OrderId{i}, side, OrderType::Limit,
                             Price::from_ticks(static_cast<std::int64_t>(i)),
                             Quantity{1}, Sequence{i}));
    }
    for (std::size_t i = n + 1; i <= n + 128; ++i) {
        CHECK(book.cancel(OrderId{i}).ok());
    }

    std::vector<std::size_t> ids(n);
    std::iota(ids.begin(), ids.end(), std::size_t{1});
    std::mt19937_64 rng(42);
    std::shuffle(ids.begin(), ids.end(), rng);
    std::vector<bool> cancelled(n + 1, false);
    for (std::size_t i = 0; i < n / 2; ++i) {
        CHECK(book.cancel(OrderId{ids[i]}).cancelled_quantity == Quantity{3});
        CHECK(book.cancel(OrderId{ids[i]}).status == CancelStatus::NotFound);
        cancelled[ids[i]] = true;
    }

    const PriceLevel* level = book.level_at(side, p);
    REQUIRE(level != nullptr);
    CHECK(level->total_quantity() == Quantity{3 * n / 2});
    CHECK(level->order_count() == n / 2);
    std::size_t expected = 1;
    for (const Order& order : level->orders()) {
        while (expected <= n && cancelled[expected]) {
            ++expected;
        }
        CHECK(order.id() == OrderId{expected});
        CHECK(order.sequence() == Sequence{expected});
        ++expected;
    }
    for (std::size_t i = n / 2; i < n; ++i) {
        CHECK(book.cancel(OrderId{ids[i]}).ok());
    }
    CHECK(book.empty());
    book.add_order(Order(OrderId{1}, side, OrderType::Limit, p, Quantity{7}, Sequence{n + 129}));
    CHECK(book.cancel(OrderId{1}).cancelled_quantity == Quantity{7});
    CHECK(book.empty());
}

TEST_CASE("Copied and moved books own their cancellation handles") {
    MatchingEngine original;
    original.submit(sell(OrderId{1}, px(101), Quantity{10}, Sequence{1}));
    original.submit(sell(OrderId{2}, px(102), Quantity{20}, Sequence{2}));
    original.submit(buy(OrderId{3}, px(99), Quantity{30}, Sequence{3}));
    original.submit(buy(OrderId{4}, px(101), Quantity{10}, Sequence{4}));
    original.submit(sell(OrderId{5}, px(103), Quantity{40}, Sequence{5}));
    original.submit(buy(OrderId{6}, px(102), Quantity{5}, Sequence{6}));
    const BookSnapshot before = BookSnapshot::of(original.book());

    const auto check_copy = [&](MatchingEngine& copy) {
        CHECK(BookSnapshot::of(copy.book()) == before);
        CHECK(copy.cancel(OrderId{1}).status == CancelStatus::AlreadyFilled);
        CHECK(copy.cancel(OrderId{999}).status == CancelStatus::NotFound);
        CHECK(copy.cancel(OrderId{2}).cancelled_quantity == Quantity{15});
        CHECK(copy.cancel(OrderId{3}).cancelled_quantity == Quantity{30});
        CHECK(copy.cancel(OrderId{5}).cancelled_quantity == Quantity{40});
        CHECK(copy.book().empty());
        CHECK(BookSnapshot::of(original.book()) == before);
    };

    SUBCASE("copy construction") {
        MatchingEngine copy(original);
        check_copy(copy);
    }
    SUBCASE("copy assignment replaces a nonempty book") {
        MatchingEngine copy;
        copy.submit(buy(OrderId{100}, px(90), Quantity{1}, Sequence{100}));
        copy = original;
        CHECK(copy.cancel(OrderId{100}).status == CancelStatus::NotFound);
        check_copy(copy);
    }
    SUBCASE("self copy assignment") {
        MatchingEngine copy(original);
        const MatchingEngine& alias = copy;
        copy = alias;
        check_copy(copy);
    }
    SUBCASE("move construction") {
        MatchingEngine source(original);
        MatchingEngine copy(std::move(source));
        check_copy(copy);
    }
    SUBCASE("move assignment replaces a nonempty book") {
        MatchingEngine source(original);
        MatchingEngine copy;
        copy.submit(buy(OrderId{100}, px(90), Quantity{1}, Sequence{100}));
        copy = std::move(source);
        CHECK(copy.cancel(OrderId{100}).status == CancelStatus::NotFound);
        check_copy(copy);
    }
    SUBCASE("copy survives source destruction") {
        const auto make_copy = [&] {
            const MatchingEngine source(original);
            return MatchingEngine(source);
        };
        MatchingEngine copy = make_copy();
        check_copy(copy);
    }
    SUBCASE("empty copy and assignment") {
        const MatchingEngine empty;
        MatchingEngine copy(empty);
        CHECK(copy.book().empty());
        copy = original;
        copy = empty;
        CHECK(copy.book().empty());
        CHECK(copy.cancel(OrderId{1}).status == CancelStatus::NotFound);
    }
}
