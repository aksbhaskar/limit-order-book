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
| 4         | Matching engine producing trades / executions           | Done        |
| 5         | Order cancellation                                      | Done        |
| 6         | Market orders                                           | Done        |
| 7         | Market-data event recording and deterministic replay    | Done        |
| 8         | Benchmarks: throughput and latency characterisation     | Done        |
| 9         | Market-making simulator                                 | Done        |
| 10        | Strategy backtesting and performance analytics          | Planned     |
| 11        | Order amend / replace                                   | Planned     |
| 12        | Further order types (IOC, FOK, ...)                     | Planned     |

Only milestones 1–9 are implemented at present. Everything from milestone 10
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
non-limit orders and duplicate order ids. Cancellation and amendment are out of
scope; matching is layered on top by the engine below.

## Matching engine

The matching engine (`lob::MatchingEngine`) owns an `OrderBook` and turns an
incoming order into executions plus, possibly, a new resting order. It
implements continuous **price-time priority** matching for limit orders.

### Submission algorithm

`submit(order)` repeats the following while the order still has quantity:

1. Look at the best level of the **opposite** side (`best_ask` for a buy,
   `best_bid` for a sell). If that side is empty, stop.
2. Check whether the prices **cross**: a buy crosses when its limit price is
   `>=` the best ask; a sell crosses when its limit price is `<=` the best bid.
   If they do not cross, stop.
3. Take the **front** (oldest) order at that level — this is the highest-priority
   maker by time. Execute `min(taker_remaining, maker_remaining)`.
4. Record a `Trade` at the **maker's price**, reduce both orders, and remove the
   maker (and the level, if now empty) when it is fully filled.

When the loop ends, any remaining quantity is inserted into the book as a passive
limit order, and a `SubmitResult` reports the trades, the filled and remaining
quantities, and whether a remainder is now resting.

### Execution semantics

- **Price improvement goes to the taker.** Trades always execute at the resting
  maker's displayed price, never at the incoming order's (possibly more
  aggressive) price.
- **Priority order.** Because the book yields levels best-first and each level
  yields orders oldest-first, an incoming order consumes liquidity in strict
  price-then-time order.
- **Partial fills.** Either side may be partially filled: a large incoming order
  walks multiple makers and levels; a large maker is left resting with a reduced
  remaining quantity while preserving its original quantity and time priority.
- **Full fills and cleanup.** Fully filled makers are popped from their level,
  emptied levels are erased, and their ids are released from the book's index.
- **Determinism.** The engine holds no hidden state; identical ordered input
  always produces identical trades.

### Scope

Only limit-order matching is implemented. Market orders, amend/replace, matching
strategies, market making, and historical replay are out of scope at this
milestone. (Cancellation is covered in the next section.)

## Cancellation

A resting order can be withdrawn by id via `OrderBook::cancel` (exposed on the
engine as `MatchingEngine::cancel`).

### Locating an order

The book keeps an id index, `std::map<OrderId, Locator>`, mapping every live
order to the `(side, price)` of the level that holds it. Cancellation looks the
id up in this index, jumps straight to the level, and removes the order — no
scan of the whole book is needed. Within a level the order is found by a linear
walk of that level's queue (levels are typically shallow); a future optimisation
could store an iterator/handle to make this O(1).

### Data-structure implications

- **Aggregate quantity.** `PriceLevel::remove` subtracts the cancelled order's
  *remaining* quantity from the level's cached aggregate, so depth stays exact.
- **Empty-level removal.** If the level becomes empty it is erased from the side
  map, so it no longer appears in best-of-book or depth queries.
- **Time priority preserved.** Removing an order from the middle of the level's
  `std::deque` keeps the relative order — and therefore the sequence numbers and
  FIFO priority — of all remaining orders unchanged. The next order in line is
  promoted naturally.

### Result and rejections

`cancel` returns a `CancelResult` with a `CancelStatus`:

- **Cancelled** — the order was resting and has been removed; the result carries
  the removed remaining quantity.
- **AlreadyFilled** — the id belonged to an order that rested but has since been
  fully filled. The book remembers ids of orders that filled while resting, so
  this case is reported distinctly rather than as "not found".
- **NotFound** — no such order is or ever was resting (unknown id).

Cancelling never throws for an absent order and never disturbs any other order,
so it composes cleanly with subsequent matching: a cancelled order simply no
longer provides liquidity.

### Scope

Cancellation covers resting limit orders. Amend/replace is still out of scope
(modelled as cancel-and-resubmit for now).

## Market orders

A market order (`OrderType::Market`) executes immediately against the best
available liquidity and **never rests** on the book. It is submitted through the
same `MatchingEngine::submit` entry point and reuses the same strongly typed
`Order` model — there is no separate, loosely typed market-order path.

### Representation

A market order carries no meaningful price, so its `Price` field is ignored and
its construction skips the "price must be positive" check that applies to limit
orders (quantity and id validity are still enforced). All other fields — id,
side, quantity, sequence — are identical to a limit order's.

### Matching behaviour

Market and limit matching share one loop; they differ only in the stop condition
and the fate of any remainder:

- **Limit** matches while the best opposite level *crosses* its price, then rests
  the remainder.
- **Market** ignores price entirely: it keeps consuming the best opposite level
  while any opposing liquidity remains, and rests nothing.

Everything else is unchanged and shared:

- **Price-time priority.** Levels are consumed best-first and, within a level,
  oldest-first, exactly as for a limit taker.
- **Maker-price execution.** Each trade prints at the resting maker's price; a
  market order accepts whatever price the book offers.
- **Bounded by liquidity.** A market order can never execute more than the total
  resting quantity on the opposite side. When the opposite side empties, matching
  stops:
  - If it fully filled, the result is `fully_filled()` with `resting == false`.
  - If liquidity was insufficient (or the book was empty), the unfilled quantity
    is reported in `remaining_quantity`, `resting` is `false`, and nothing is
    added to the book — the remainder is simply dropped.

### Scope

Market and limit orders are supported. Immediate-or-cancel and fill-or-kill
variants, which build on the same machinery, remain future work.

## Market-data events and replay

A recording/replay layer sits *around* the matching engine, not inside it, so
the engine's behaviour and API are unchanged and the layer could be driven by an
external historical feed instead.

### Event model

A `MarketEvent` is a monotonic `index` (the deterministic ordering key) plus a
strongly typed payload — a `std::variant` of exactly three alternatives:

- **SubmissionEvent** — an order arrived, captured as the full `Order`.
- **CancellationEvent** — a resting order was cancelled, by id.
- **ExecutionEvent** — two orders traded, captured as a `Trade`.

Submissions and cancellations are *commands* (they drive state); executions are
*derived output*. Keeping all three in one stream gives a faithful market-data
record, while the command/derived distinction is what makes replay possible.

### Recording

`RecordingEngine` wraps a `MatchingEngine`. It forwards `submit`/`cancel`
unchanged and appends events as they occur:

- `submit(order)` records the submission, then one execution per resulting
  trade, in trade order.
- `cancel(id)` records a cancellation only when it actually removed a resting
  order, so the log reflects real state changes rather than rejected requests.

Every appended event takes the next value of a monotonic counter, giving the log
a total, gap-free order.

### Serialization

`EventLog` serializes to a simple, self-contained, line-based text format (a
`LOBLOG v1` header then one event per line). Prices are written as integer
**ticks**, so the fixed-point representation round-trips exactly — no
floating-point is ever involved. Parsing re-validates each order through the
`Order` constructor and raises a uniform parse error on a bad header, unknown
tag, missing/non-numeric field, or invariant violation. No database or network
is used.

### Replay and determinism

`replay(log)` feeds the log's **command** events (submissions and cancellations)
through a fresh `RecordingEngine`; execution events are skipped because the
engine regenerates them. Because matching is deterministic, replay reconstructs
both the same book and the same executions:

- The replayed recorder's own log **equals** the original log (same events, same
  indices) — a strong determinism check.
- The reconstructed book **equals** the original book.

### Comparing book state

`BookSnapshot::of(book)` captures full state deterministically: every level in
price-priority order and, within each level, every resting order in FIFO order
with its id, remaining quantity, and sequence number. Two snapshots compare
equal exactly when the books agree on price priority, time priority, and resting
quantity — the definition of "the book after replay matches the book before".

### Scope

Recording, serialization, and replay cover submissions, cancellations, and
executions. There is no networking, no persistence beyond the text format, and
no external feed adapter yet — but the layer is deliberately shaped so one could
be added without touching the engine.

## Market-making simulator

A research-oriented layer runs a market-making **strategy** against a
deterministic synthetic market, using only the public engine APIs.

> **Simulation, not reality.** The synthetic market is a toy: a random-walk mid
> and coin-flip aggressor flow. It exists to exercise the strategy and accounting
> code deterministically. It is **not** a model of any real venue and its results
> say nothing about real-market performance.

### Strategy interface

`Strategy` is a tiny interface: given a `MarketState` (reference mid, current
best bid/ask, the strategy's own inventory, and the step index) it returns a
`QuoteDecision` — an optional bid and an optional ask, each with a price and
size. The strategy only *decides*; it never touches the book. `FixedSpreadMarketMaker`
is the first implementation: it quotes `mid ± spread/2`.

### Risk limits

A symmetric inventory cap `±max_inventory` is enforced *before* orders reach the
book: the shared `inventory_room` helper clamps each side's size to the room
remaining (`max_inventory − inventory` for the bid, `max_inventory + inventory`
for the ask) and drops a side with no room. Because a single aggressor hits only
one side per step, inventory provably stays within `[−max_inventory, +max_inventory]`.

### Simulation loop

Each step, the `MarketMakerSimulator`:

1. random-walks the reference mid (seeded);
2. cancels the previous quotes via `MatchingEngine::cancel`;
3. asks the strategy to quote and submits the quotes via `MatchingEngine::submit`;
4. with the configured probability, submits a synthetic market order on a random
   side that crosses one of the quotes;
5. books any fills against the strategy's quotes into the `PnLAccount` and records
   the portfolio state.

The random stream depends only on the config, never on the strategy's choices, so
two strategies can be run on **identical** market conditions for a fair comparison.
Everything is integer/seeded, so a config yields exactly one run.

### Accounting conventions

`PnLAccount` keeps all money in integer **ticks** and uses average-cost
accounting (see the comments in `pnl_account.hpp`):

- A buy fill raises the position and lowers cash by `price × quantity`; a sell
  fill is the reverse.
- **Realized** P&L is booked via average cost whenever a fill reduces or closes
  the position; flipping through zero realizes the old side and reopens the
  remainder at the fill price.
- **Total** P&L marked at price *m* is the exact cash figure
  `cash + position × m − starting_cash`, and **unrealized** is defined as
  `total − realized`, so `realized + unrealized == total` holds exactly despite
  integer rounding in the average cost.
- A transaction fee is a realized cost (it lowers both cash and realized).

### Tracked series

Per step the simulator records the mid, both quote prices/sizes, inventory, cash,
realized/unrealized/total P&L, and fills; the summary adds final inventory and
P&L, quotes placed, fills, filled quantity, and the fill rate (fills per quote).
