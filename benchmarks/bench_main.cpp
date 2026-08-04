// Benchmark suite for the order book and matching engine.
//
// All workloads are generated from fixed PRNG seeds, so runs are reproducible.
// Setup (workload generation, and any pre-populated book) happens outside the
// measured region; the timed lambda performs only the operation(s) under test.
// nanobench reports median ns per unit and units/s; `batch(n)` tells it how many
// logical operations each lambda call performs so the figures are per-operation.
//
// See benchmarks/README.md for the methodology and workload assumptions.

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include <nanobench.h>

#include "lob/matching_engine.hpp"
#include "lob/order.hpp"
#include "lob/order_book.hpp"
#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/types.hpp"

namespace nb = ankerl::nanobench;
using namespace lob;

namespace {

// Mid price = 1000.0000; one tick step = 0.0100. Bids sit below mid and asks
// above it, so a plain book never crosses during pure insertion workloads.
constexpr std::int64_t kMid = 10'000'000;
constexpr std::int64_t kStep = 100;
constexpr int kLevels = 128;

// A book's worth of non-crossing limit orders on both sides.
std::vector<Order> gen_book_orders(std::size_t n, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Order> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const bool buy = (rng() & 1u) != 0;
        const std::int64_t lvl = static_cast<std::int64_t>(rng() % kLevels) + 1;
        const std::int64_t ticks = buy ? kMid - lvl * kStep : kMid + lvl * kStep;
        const std::uint64_t qty = 1 + rng() % 100;
        out.push_back(Order(OrderId{i + 1}, buy ? Side::Buy : Side::Sell,
                            OrderType::Limit, Price::from_ticks(ticks),
                            Quantity{qty}, Sequence{i + 1}));
    }
    return out;
}

// A resting maker plus a taker that crosses and fully fills it (one trade each).
struct Pair {
    Order maker;
    Order taker;
};

std::vector<Pair> gen_match_pairs(std::size_t n, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Pair> out;
    out.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        const std::int64_t lvl = static_cast<std::int64_t>(rng() % kLevels);
        const std::int64_t ticks = kMid + lvl * kStep;
        const std::uint64_t qty = 1 + rng() % 50;
        const std::uint64_t mid_id = 2 * i + 1;
        Order maker(OrderId{mid_id}, Side::Sell, OrderType::Limit,
                    Price::from_ticks(ticks), Quantity{qty}, Sequence{mid_id});
        Order taker(OrderId{mid_id + 1}, Side::Buy, OrderType::Limit,
                    Price::from_ticks(ticks), Quantity{qty}, Sequence{mid_id + 1});
        out.push_back(Pair{maker, taker});
    }
    return out;
}

// K ascending ask levels and one taker (limit or market) that sweeps them all.
struct Sweep {
    std::vector<Order> makers;
    Order taker;
};

Sweep gen_sweep(int levels, bool market, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Order> makers;
    makers.reserve(static_cast<std::size_t>(levels));
    std::uint64_t qty_sum = 0;
    std::uint64_t id = 1;
    std::uint64_t seq = 1;
    for (int l = 0; l < levels; ++l) {
        const std::int64_t ticks = kMid + static_cast<std::int64_t>(l + 1) * kStep;
        const std::uint64_t q = 1 + rng() % 50;
        qty_sum += q;
        makers.push_back(Order(OrderId{id++}, Side::Sell, OrderType::Limit,
                               Price::from_ticks(ticks), Quantity{q}, Sequence{seq++}));
    }
    const std::int64_t top = kMid + static_cast<std::int64_t>(levels + 1) * kStep;
    Order taker =
        market ? Order(OrderId{id}, Side::Buy, OrderType::Market, Price{},
                       Quantity{qty_sum}, Sequence{seq})
               : Order(OrderId{id}, Side::Buy, OrderType::Limit,
                       Price::from_ticks(top), Quantity{qty_sum}, Sequence{seq});
    return Sweep{std::move(makers), taker};
}

// A deterministic mixed feed: mostly resting limit orders, some marketable
// takers, and occasional cancellations of earlier ids.
struct Action {
    bool cancel = false;
    std::optional<Order> order;   // for a submission
    OrderId cancel_id{};          // for a cancellation
};

std::vector<Action> gen_mixed(std::size_t n, std::uint64_t seed) {
    std::mt19937_64 rng(seed);
    std::vector<Action> out;
    out.reserve(n);
    std::vector<OrderId> submitted;
    std::uint64_t next_id = 1;
    std::uint64_t seq = 1;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t r = rng() % 100;
        if (r < 15 && !submitted.empty()) {
            Action a;
            a.cancel = true;
            a.cancel_id = submitted[rng() % submitted.size()];
            out.push_back(a);
        } else if (r < 35) {
            const bool buy = (rng() & 1u) != 0;
            const std::uint64_t qty = 1 + rng() % 10;
            Action a;
            a.order = Order(OrderId{next_id++}, buy ? Side::Buy : Side::Sell,
                            OrderType::Market, Price{}, Quantity{qty}, Sequence{seq++});
            out.push_back(std::move(a));
        } else {
            const bool buy = (rng() & 1u) != 0;
            const std::int64_t lvl = static_cast<std::int64_t>(rng() % kLevels) + 1;
            const std::int64_t ticks = buy ? kMid - lvl * kStep : kMid + lvl * kStep;
            const std::uint64_t qty = 1 + rng() % 50;
            const OrderId id{next_id++};
            Action a;
            a.order = Order(id, buy ? Side::Buy : Side::Sell, OrderType::Limit,
                            Price::from_ticks(ticks), Quantity{qty}, Sequence{seq++});
            submitted.push_back(id);
            out.push_back(std::move(a));
        }
    }
    return out;
}

void apply(MatchingEngine& engine, const Action& action) {
    if (action.cancel) {
        engine.cancel(action.cancel_id);
    } else {
        engine.submit(*action.order);
    }
}

}  // namespace

int main() {
    nb::Bench bench;
    bench.title("limit-order-book").unit("op").relative(false);
    // Heavy batch lambdas otherwise run too few iterations to be stable; force a
    // floor so the reported medians are reproducible.
    bench.minEpochIterations(20).warmup(3);

    // 1. Limit-order insertion into a fresh book (empty-book construction is
    //    negligible relative to N insertions).
    for (std::size_t n : {std::size_t{1000}, std::size_t{10000}}) {
        const auto orders = gen_book_orders(n, 0xA11CEu);
        bench.batch(static_cast<double>(n))
            .run("limit insertion (N=" + std::to_string(n) + ")", [&] {
                OrderBook book;
                for (const Order& o : orders) {
                    book.add_order(o);
                }
                nb::doNotOptimizeAway(book);
            });
    }

    // 2. Best bid/ask lookup on a pre-populated book (pure const reads).
    {
        const auto orders = gen_book_orders(10000, 0xB0Bu);
        OrderBook book;
        for (const Order& o : orders) {
            book.add_order(o);
        }
        bench.batch(2.0).run("best bid/ask lookup", [&] {
            const PriceLevel* bb = book.best_bid();
            const PriceLevel* ba = book.best_ask();
            nb::doNotOptimizeAway(bb);
            nb::doNotOptimizeAway(ba);
        });
    }

    // 3. Cancellation, measured as an insert+cancel cycle (you cannot cancel N
    //    orders without first inserting them; subtract the insertion baseline
    //    above to isolate the cancel cost).
    for (std::size_t n : {std::size_t{1000}, std::size_t{10000}}) {
        const auto orders = gen_book_orders(n, 0xCA11u);
        bench.batch(static_cast<double>(n))
            .run("insert+cancel cycle (N=" + std::to_string(n) + ")", [&] {
                OrderBook book;
                for (const Order& o : orders) {
                    book.add_order(o);
                }
                for (const Order& o : orders) {
                    book.cancel(o.id());
                }
                nb::doNotOptimizeAway(book);
            });
    }

    // 4. Single-order matching: rest a maker, then fully fill it (one trade).
    {
        const std::size_t n = 2000;
        const auto pairs = gen_match_pairs(n, 0xD00Du);
        bench.batch(static_cast<double>(n))
            .run("single-order matching (N=" + std::to_string(n) + ")", [&] {
                MatchingEngine engine;
                for (const Pair& p : pairs) {
                    engine.submit(p.maker);
                    engine.submit(p.taker);
                }
                nb::doNotOptimizeAway(engine);
            });
    }

    // 5. Multi-level matching: a single limit taker sweeps K price levels.
    {
        const int k = 64;
        const Sweep sweep = gen_sweep(k, /*market=*/false, 0xE11Eu);
        bench.batch(static_cast<double>(k))
            .run("multi-level matching (K=" + std::to_string(k) + ")", [&] {
                MatchingEngine engine;
                for (const Order& m : sweep.makers) {
                    engine.submit(m);
                }
                engine.submit(sweep.taker);
                nb::doNotOptimizeAway(engine);
            });
    }

    // 6. Market-order execution: a single market taker sweeps K price levels.
    {
        const int k = 64;
        const Sweep sweep = gen_sweep(k, /*market=*/true, 0xF00Fu);
        bench.batch(static_cast<double>(k))
            .run("market-order execution (K=" + std::to_string(k) + ")", [&] {
                MatchingEngine engine;
                for (const Order& m : sweep.makers) {
                    engine.submit(m);
                }
                engine.submit(sweep.taker);
                nb::doNotOptimizeAway(engine);
            });
    }

    // 7. Mixed order flow: a realistic blend of limit / market / cancel actions.
    {
        const std::size_t n = 20000;
        const auto script = gen_mixed(n, 0x5EEDu);
        bench.batch(static_cast<double>(n))
            .run("mixed order flow (N=" + std::to_string(n) + ")", [&] {
                MatchingEngine engine;
                for (const Action& a : script) {
                    apply(engine, a);
                }
                nb::doNotOptimizeAway(engine);
            });
    }

    return 0;
}
