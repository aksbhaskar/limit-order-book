# Limit Order Book & Matching Engine — with a market-making research stack

A from-scratch **C++20** limit order book and exchange matching engine, plus a
deterministic market simulator and a reproducible market-making research pipeline
built on top of it. It is a portfolio project in market microstructure and
low-level systems design: correct integer money, strict price-time priority, and
everything seeded so results reproduce exactly.

The engine core (order book, matching, cancellation, market orders, event
recording/replay) is a self-contained library. On top of it sits a research
layer: a synthetic market, two market-making strategies, a backtester with P&L
and risk analytics, a parameter study, and a visualization pipeline that renders
figures directly from the experiment output.

> **On "results".** Every strategy, backtest, and parameter-study number in this
> repository comes from a **toy synthetic market** — a random-walk mid and a
> simple price-sensitive order-flow model. It is deliberately transparent and is
> **not** a model of any real venue. The research numbers are meaningful only for
> comparing the strategies against each other inside the simulation; they are
> **not** evidence about real-market performance, and no such claim is made.

## Architecture

```
      Market events            (synthetic, seeded order flow)
           │
           ▼
      Order Book               price-time priority, O(1) best quote
           │
           ▼
      Matching Engine          limit + market orders, maker-price fills
           │
           ▼
      Event Log / Replay       record & deterministically reconstruct state
           │
           ▼
      Market Simulator         price-sensitive market + strategy loop
           │
           ▼
      Market-Making Strategy   fixed-spread / inventory-aware quoting
           │
           ▼
      Backtester               run a strategy over the simulated market
           │
           ▼
      Risk / P&L Analytics     average-cost P&L, Sharpe, drawdown, inventory
           │
           ▼
      Research Results         parameter study → JSON/CSV → SVG figures
```

## Components

- **Order book** (`OrderBook`, `PriceLevel`) — separate bid/ask sides as
  price-ordered maps (best quote is always `begin()`); FIFO time priority within
  a level; best-quote / level / depth queries; cancellation by id.
- **Matching engine** (`MatchingEngine`) — continuous price-time-priority
  matching; limit and market orders; trades execute at the resting maker's price;
  full and partial fills; deterministic.
- **Event log & replay** (`MarketEvent`, `EventLog`, `RecordingEngine`) — records
  submissions, cancellations, and executions to a portable text log and replays
  the command stream to reconstruct identical state.
- **Market simulator** (`MarketMakerSimulator`) — a seeded synthetic market where
  each aggressor's exponential "reach" from the mid makes fill probability decay
  with quote distance (`exp(−d/reach_mean)`); volatility drives the mid; regimes
  for calm / volatile / high-volume.
- **Market makers** (`FixedSpreadMarketMaker`, `InventoryAwareMarketMaker`) — a
  common `Strategy` interface; the second leans quotes against inventory.
- **Accounting** (`PnLAccount`) — average-cost P&L entirely in integer ticks;
  realized / unrealized separated with `realized + unrealized == total`.
- **Backtester & analytics** (`Backtester`, `BacktestMetrics`) — P&L, return,
  Sharpe, drawdown, fill rate, inventory, with machine-readable JSON.
- **Parameter study** (`parameter_study`) — grids over strategy parameters, many
  seeds per point, distributions with 95% confidence intervals, JSON + CSV.
- **Visualization** (`viz/`) — pure-stdlib Python that renders SVG figures from
  the study output (no matplotlib/numpy).

Design principles, data structures, and the synthetic-market model are described
in [docs/DESIGN.md](docs/DESIGN.md).

## Building and testing

Requires a C++20 compiler and CMake 3.20+. From a clean checkout:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

This builds the library, the unit tests (doctest, fetched automatically), the
benchmarks (nanobench), and the example executables. The visualization layer uses
only the Python 3 standard library; its data-layer tests run with:

```bash
python -m unittest discover -s viz/tests
```

## Performance (benchmarks)

Indicative microbenchmark results from **one developer laptop** — for regression
tracking and rough relative cost, **not** a low-latency or production claim.
Regenerate locally (`./build/benchmarks/lob_benchmarks`); see
[benchmarks/README.md](benchmarks/README.md) for methodology.

Environment: 12th Gen Intel Core i7-1255U (10C/12T, base ~1.70 GHz), 15.6 GB RAM,
Windows 11 Pro (build 26200), GCC 16.1.0 (MSYS2 UCRT64), C++20 `-O3` Release,
nanobench v4.3.11.

| Benchmark | ns/op | op/s |
|-----------|------:|-----:|
| limit insertion (N=1000) | 251.9 | 3.97 M |
| limit insertion (N=10000) | 208.0 | 4.81 M |
| best bid/ask lookup | 0.49 | 2.03 B |
| insert+cancel cycle (N=1000) | 300.9 | 3.32 M |
| insert+cancel cycle (N=10000) | 245.4 | 4.07 M |
| single-order matching (N=2000) | 271.6 | 3.68 M |
| multi-level matching (K=64) | 272.0 | 3.68 M |
| market-order execution (K=64) | 256.2 | 3.90 M |
| mixed order flow (N=20000) | 165.4 | 6.05 M |

## Strategy research (simulated)

Two market makers are compared on identical, deterministic simulated markets. The
market is **price-sensitive**: a quote at distance `d` from the mid fills with
probability `exp(−d/reach_mean)`, so quote placement matters.

**Single backtest** (`./build/examples/lob_backtest`, `seed=42`, `steps=5000`):

| Strategy | Total P&L | Sharpe (per-step) | Trades | Avg inv | Max abs inv |
|----------|----------:|------------------:|-------:|--------:|------------:|
| fixed-spread    | 34.33 | 0.350 | 2174 | 4.79 | 50 |
| inventory-aware | 34.00 | 0.590 | 2054 | 0.81 | 32 |

**Parameter study** (`./build/examples/lob_param_study`, 48-point grid × 20 seeds):

- Mean per-step Sharpe: **fixed-spread 0.375**, **inventory-aware 0.528**.
- Inventory-aware had the higher mean Sharpe in **44/48** configs and lower
  `|average inventory|` in **48/48**.

Representative config (`spread=100, qty=5, inv_limit=60, skew=6, cost=2`, 20 seeds):

| Strategy | Mean P&L | P&L 95% CI | Sharpe | Max abs inv |
|----------|---------:|:----------:|-------:|------------:|
| fixed-spread    | 17.89 | [17.43, 18.35] | 0.315 | 60 |
| inventory-aware | 16.07 | [15.92, 16.22] | 0.545 | 18 |

Across the grid the inventory-aware maker trades modestly less gross P&L for much
lower inventory and tighter dispersion, i.e. better risk-adjusted return — **in
this simulation only.** Full methodology, statistics, and interpretation are in
[docs/RESEARCH.md](docs/RESEARCH.md); the figures and pipeline are in
[docs/RESULTS.md](docs/RESULTS.md).

### Figures

Generated from the experiment output (a few of nine):

| | |
|---|---|
| [Sharpe vs spread](docs/figures/sharpe_vs_spread.svg) | [Inventory vs spread](docs/figures/inventory_vs_spread.svg) |
| [Strategy comparison](docs/figures/strategy_comparison.svg) | [P&L 95% CIs](docs/figures/pnl_confidence_intervals.svg) |
| [Sharpe heatmap (inventory-aware)](docs/figures/heatmap_sharpe_inventory_aware.svg) | [Per-seed P&L distribution](docs/figures/pnl_distributions.svg) |

## Reproducing the research end-to-end

One command builds the project, runs the parameter study, and renders every
figure (requires a C++20 toolchain, CMake, and Python 3 on `PATH`):

```bash
scripts/run_research.sh          # macOS / Linux / Git Bash
scripts\run_research.ps1         # Windows PowerShell
```

Study CSV/JSON land in `results/` (git-ignored, regenerated deterministically);
figures land in `docs/figures/`.

## Layout

```
include/lob/    Public headers (the library interface)
src/            Library sources and the executable
tests/          C++ unit tests (doctest)
benchmarks/     Performance benchmarks (nanobench)
examples/       Runnable examples: backtest, parameter study
viz/            Python (stdlib) visualization package + its tests
scripts/        Reproducible research pipeline
docs/           Design notes, research report, results, figures
```

## Roadmap

Implemented: order book, matching engine, cancellation, market orders, event
recording/replay, benchmarks, market-making simulator, backtesting, a
price-sensitive market model, the parameter study, and the visualization layer.
Planned: order amend/replace and further order types (IOC, FOK). See
[docs/DESIGN.md](docs/DESIGN.md) for the full milestone roadmap.

## License

MIT — see [LICENSE](LICENSE).
