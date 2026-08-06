# Limit Order Book & Exchange Matching Engine

A limit order book (LOB) and exchange matching engine written in modern
**C++20**. The project is a study in market microstructure and matching-engine
systems design, built as a portfolio piece for quantitative trading engineering.

## Project vision

The goal is to build, incrementally and from first principles, the core of an
exchange matching engine:

- A price-time-priority limit order book.
- A deterministic matching engine that produces trades and executions.
- Support for the common order lifecycle (submit, amend, cancel).
- Realistic, integer-based price and quantity representation (no floating-point
  money).
- Benchmarks characterising throughput and latency.

## Current status

Implemented so far (each feature is listed only once the code and tests exist):

- A CMake-based C++20 build with a [doctest](https://github.com/doctest/doctest)
  unit-test suite.
- **Order model**: strongly typed ids, sides, order types, fixed-point integer
  prices, quantities, and sequence numbers.
- **Price-time-priority order book**: separate bid/ask sides, correct price and
  FIFO ordering, best-quote/level/depth queries.
- **Matching engine**: deterministic limit-order matching at the maker's price,
  with full and partial fills.
- **Order cancellation** by id.
- **Market orders** (never rest; bounded by available liquidity).
- **Market-data event recording and deterministic replay**, with a portable
  text log format.
- **Benchmarks** for the book and engine (see below).
- **Market-making simulator** with a strategy interface, average-cost P&L
  accounting, and inventory risk limits.
- **Backtesting and analytics** with two strategies (fixed-spread and
  inventory-aware) and machine-readable results (see Strategy research below).

Not yet implemented: order amend/replace and further order types (IOC, FOK, ...).
See [docs/DESIGN.md](docs/DESIGN.md) for the full roadmap.

All strategy/backtest numbers are from a toy simulator and are **not** claims
about real-market performance.

## Layout

```
include/lob/    Public headers (the library interface)
src/            Library sources and the executable
tests/          Unit tests
benchmarks/     Performance benchmarks (nanobench)
examples/       Runnable examples (e.g. the strategy backtest)
docs/           Design notes and roadmap
```

## Building

Requires a C++20 compiler and CMake 3.20+.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/benchmarks/lob_benchmarks   # optional: run the benchmark suite
```

## Performance

Indicative microbenchmark results from **one developer laptop** — these are for
regression tracking and rough relative cost, **not** a claim of low latency or
production-exchange throughput. Regenerate them locally before drawing any
conclusions; absolute numbers depend heavily on hardware, compiler, and build
flags. See [benchmarks/README.md](benchmarks/README.md) for the methodology and
what each workload includes.

Environment: 12th Gen Intel Core i7-1255U (10C/12T, base ~1.70 GHz), 15.6 GB RAM,
Windows 11 Pro (build 26200), GCC 16.1.0 (MSYS2 UCRT64), C++20, `-O3` Release,
nanobench v4.3.11 (median of ≥20 iterations/epoch).

| Benchmark | ns/op | op/s | err% |
|-----------|------:|-----:|-----:|
| limit insertion (N=1000) | 251.9 | 3.97 M | 2.8% |
| limit insertion (N=10000) | 208.0 | 4.81 M | 1.3% |
| best bid/ask lookup | 0.49 | 2.03 B | 0.3% |
| insert+cancel cycle (N=1000) | 300.9 | 3.32 M | 3.7% |
| insert+cancel cycle (N=10000) | 245.4 | 4.07 M | 5.6% |
| single-order matching (N=2000) | 271.6 | 3.68 M | 3.3% |
| multi-level matching (K=64) | 272.0 | 3.68 M | 0.9% |
| market-order execution (K=64) | 256.2 | 3.90 M | 3.2% |
| mixed order flow (N=20000) | 165.4 | 6.05 M | 3.1% |

The `insert+cancel cycle` rows include the insertion cost (you cannot cancel an
order you did not insert); subtract the matching `limit insertion` baseline to
isolate cancellation. `best bid/ask lookup` is a pure `const` read and is orders
of magnitude cheaper than the mutating operations.

## Strategy research

The project includes a deterministic market-making **simulator** and a
**backtester** that runs strategies over a toy synthetic market and reports
performance analytics.

> **These are simulated results, not real-market performance.** The market is a
> toy (random-walk mid, coin-flip market-order flow); it is not a model of any
> real venue and says nothing about how a strategy would perform live. The
> numbers exist only to compare the two strategies against each other under
> identical, reproducible conditions.

The synthetic market is **price-sensitive**: each aggressor is a limit order
whose exponential "reach" from the mid decides how far it will trade, so a quote
at distance `d` from the mid fills with probability `exp(−d / reach_mean)` —
tighter quotes fill more often, wider quotes less often. Quote placement
therefore genuinely affects results. Two strategies — a fixed-spread maker and an
inventory-aware maker (which shifts and widens its quotes against inventory) — are
run on the **same** deterministic market. Reproduce with:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/examples/lob_backtest
```

Config: `seed=42`, `steps=5000`, starting cash `100000.0000`, transaction cost
`0.0002/unit`, spread `0.0100`, max inventory `50`, quote size `5`. Actual output
from the run:

| Strategy | Total P&L | Return | Sharpe (per-step) | Trades | Fill rate | Avg inv | Max abs inv |
|----------|----------:|-------:|------------------:|-------:|----------:|--------:|------------:|
| fixed-spread    | 34.33 | 0.034% | 0.350 | 2174 | 0.222 | 4.79 | 50 |
| inventory-aware | 34.00 | 0.034% | 0.590 | 2054 | 0.205 | 0.81 | 32 |

On this market the inventory-aware maker holds far less inventory (avg 0.81 vs
4.79, max 32 vs 50) at a comparable P&L, giving a higher risk-adjusted return.
This is a property of the **simulation only** — see [docs/DESIGN.md](docs/DESIGN.md)
for the market model, its assumptions and limitations, and the accounting and
metric definitions.

## Roadmap

See [docs/DESIGN.md](docs/DESIGN.md) for the design notes and the planned
milestone roadmap.

## License

MIT — see [LICENSE](LICENSE).
