# Design notes & roadmap

This document records the design intent and the planned milestone roadmap. It is
aspirational: sections describe where the project is heading, and each milestone
is only marked complete once the corresponding code and tests land.

## Design principles

- **Correct money representation.** Prices are stored as fixed-point integers
  (ticks), never as floating-point values, to avoid rounding error in the
  monetary path.
- **Strong typing.** Distinct concepts (order ids, sides, order types, prices,
  quantities, sequence numbers) use dedicated types rather than bare integers or
  strings, so that the compiler catches category errors.
- **Determinism.** Given the same ordered input, the engine must produce exactly
  the same trades. A monotonic sequence number establishes time priority.
- **Testability first.** Every layer ships with unit tests before the next layer
  is built on top of it.

## Roadmap

| Milestone | Description                                             | Status      |
|-----------|---------------------------------------------------------|-------------|
| 1         | Project initialisation (build, tests, layout)           | Done        |
| 2         | Core order data structures and supporting value types   | Done        |
| 3         | Price-time-priority limit order book                    | Done        |
| 4         | Matching engine producing trades / executions           | Planned     |
| 5         | Order lifecycle: amend and cancel                       | Planned     |
| 6         | Additional order types (market, IOC, FOK, ...)          | Planned     |
| 7         | Benchmarks: throughput and latency characterisation     | Planned     |

Only milestones 1–3 are implemented at present. Everything from milestone 4
onward is a plan, not a promise of existing functionality.

## Order book architecture

The order book (`lob::OrderBook`) keeps the two sides of the market separate and
maintains strict **price-time priority**.

### Price priority

Each side is a `std::map` keyed by `Price`, with a side-specific comparator:

- **Bids** use `std::greater<Price>`, so iteration runs from the highest
  (most aggressive) buy price downward. The best bid is always `begin()`.
- **Asks** use `std::less<Price>`, so iteration runs from the lowest
  (most aggressive) sell price upward. The best ask is always `begin()`.

Because the maps are ordered containers, the best level is retrieved in constant
time and the full book can be walked in price order without any sorting step.

### Time priority

All orders resting at one price are held in a `lob::PriceLevel`, which stores
them in a `std::deque<Order>` in arrival order. The order book appends orders as
they arrive (monotonically increasing sequence numbers), so the front of the
deque is always the oldest order at that price — the one with the highest time
priority. Each level also caches its aggregate remaining quantity so that depth
queries do not need to re-sum the orders.

### Ordering guarantees

1. `best_bid()` / `best_ask()` return the level that any incoming order would
   trade against first.
2. Walking a side (e.g. via `depth`) visits levels strictly from best to worst.
3. Within a level, `front()` is the highest-priority resting order and
   `orders()` is ordered oldest-first.

These guarantees are what the matching engine (milestone 4) relies on to apply
price-time priority correctly.

### Scope

The book supports insertion (`add_order`), best-level access, exact-level lookup
(`level_at`), level counts, and bounded depth snapshots (`depth`). It rejects
non-limit orders and duplicate order ids. Matching, cancellation, and amendment
are intentionally out of scope at this milestone.
