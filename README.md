# Limit Order Book & Market-Making Research Platform

![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![CMake](https://img.shields.io/badge/CMake-%E2%89%A5%203.20-informational)
![License: MIT](https://img.shields.io/badge/License-MIT-green)

A C++20 market-microstructure and quantitative-trading research platform: an
exchange-style price-time-priority limit order book and matching engine, a
deterministic event-replay layer, a seeded synthetic market, two market-making
strategies, a backtester with P&L and risk analytics, and a reproducible
parameter-research pipeline with generated figures.

## Highlights

All figures below are produced by the code in this repository; none are
hand-entered. Strategy and study numbers come from a **synthetic simulation**
(see [Limitations](#limitations)).

| | |
|---|---|
| Language / build | C++20, CMake ≥ 3.20 |
| Engine | price-time-priority matching, limit + market orders, cancellation |
| Determinism | seeded simulation and event replay reproduce state exactly |
| C++ tests | **143 test cases / 26,868 assertions** (doctest) |
| Python tests | **8 tests** for the visualization data layer (stdlib `unittest`) |
| Research | **48 parameter configurations × 20 seeds = 1,920 backtests** |
| Risk-adjusted result | inventory-aware mean per-step Sharpe **0.528** vs fixed-spread **0.375** |
| Robustness | inventory-aware higher Sharpe in **44/48** configs; lower mean \|inventory\| in **48/48** |
| Benchmarks | best bid/ask lookup ~**0.49 ns/op**; limit insertion ~**208 ns/op**; single-order matching ~**272 ns/op** |

## What this project does

- **Limit order book** — separate bid/ask sides as price-ordered maps (best quote
  is always `begin()`), FIFO time priority within a price level, and best-quote /
  level / depth queries.
- **Matching engine** — continuous price-time-priority matching; trades execute at
  the resting maker's price; full and partial fills.
- **Market orders** — execute against available liquidity and never rest.
- **Cancellations** — withdraw a resting order by id, with correct aggregate and
  level cleanup.
- **Event recording / replay** — record submissions, cancellations, and
  executions to a portable text log and deterministically reconstruct book state.
- **Synthetic market dynamics** — a seeded, price-sensitive order-flow model where
  fill probability decays with quote distance from the mid.
- **Market-making strategies** — a fixed-spread and an inventory-aware quoter
  behind one `Strategy` interface.
- **P&L accounting** — average-cost P&L entirely in integer ticks, with realized
  and unrealized separated.
- **Backtesting** — run a strategy over the simulated market and compute return,
  Sharpe, drawdown, fill rate, and inventory metrics.
- **Parameter study** — sweep strategy parameters over a grid, many seeds per
  point, with distributions and 95% confidence intervals; JSON + CSV output.
- **Visualization** — pure-standard-library Python that renders SVG figures
  directly from the study output.

## Architecture

```
      Synthetic Market
             │
             ▼
   Market-Making Strategy
             │
             ▼
 Order Submission / Cancellation
             │
             ▼
     Limit Order Book
             │
             ▼
     Matching Engine
             │
             ▼
      Trades / Events
             │
             ▼
    Event Log / Replay
             │
             ▼
        Backtester
             │
             ▼
   P&L / Risk Analytics
             │
             ▼
     Research Results
```

Each stage builds only on the public API of the stage(s) below it, so the engine
core has no knowledge of the research layer above it.

- **Synthetic market** (`MarketMakerSimulator`) generates seeded order flow.
- **Strategy** (`Strategy`, `FixedSpreadMarketMaker`, `InventoryAwareMarketMaker`)
  decides quotes from observed state.
- **Order book & matching engine** (`OrderBook`, `PriceLevel`, `MatchingEngine`)
  hold liquidity and produce trades.
- **Event log / replay** (`MarketEvent`, `EventLog`, `RecordingEngine`) records
  and reconstructs the session.
- **Backtester & analytics** (`Backtester`, `BacktestMetrics`, `PnLAccount`)
  compute performance and risk.
- **Research** (`parameter_study`, `viz/`) aggregates many runs into results and
  figures.

## Market-making model

Two strategies implement the same `Strategy` interface and run through the
identical engine, accounting, and market generation, so they can be compared
directly.

- **`FixedSpreadMarketMaker`** — quotes symmetrically around the mid: bid at
  `mid − spread/2`, ask at `mid + spread/2`. Quote sizes are clamped to the room
  left under a symmetric inventory cap `±max_inventory`; a side is dropped when it
  has no room. It applies no inventory skew.
- **`InventoryAwareMarketMaker`** — quotes around a *reservation price*
  `mid − skew · inventory` and widens its half-spread as `|inventory|` grows. A
  long position lowers both quotes (encouraging sells, discouraging buys); a short
  position raises them. This leans the book back toward flat.

Fills update a signed inventory and an average-cost `PnLAccount`; realized P&L is
booked on position reductions, unrealized is marked at the current mid, and a
per-unit transaction cost can be charged. Full accounting conventions are in
[docs/DESIGN.md](docs/DESIGN.md).

## Synthetic market

The market is deliberately simple and **price-sensitive**. Each step:

- The reference mid moves by a volatility-scaled uniform random-walk increment.
- With a configurable probability, one aggressor arrives on a buy or sell side.
- The aggressor's willingness to trade away from the mid — its *reach* — is
  exponentially distributed with a configurable mean, so a quote at distance `d`
  from the mid is taken with probability `exp(−d / reach_mean)`. Tighter quotes
  fill more often; wider quotes fill less often. The aggressor is
  immediate-or-cancel and never rests.

Named regimes (`Calm`, `Volatile`, `HighVolume`) provide ready-made parameter
sets, and every run is driven by explicit seeds, so results are fully
deterministic. **This is a synthetic toy market. It is not intended to reproduce
real exchange behavior, and its results are not evidence about real markets.**

## Research experiment

The parameter study sweeps a grid of market-maker parameters (spread, order
quantity, inventory limit, inventory-skew strength, transaction cost) and runs
both strategies over **20 independent seeds** per configuration. For each
configuration and seed, both strategies run on the *identical* market, so the
comparison is fair. It measures total/realized/unrealized P&L, per-step Sharpe,
maximum drawdown, fill rate, average inventory, maximum absolute inventory, and
trade count, and aggregates each into a mean with a 95% confidence interval.

Headline result over **48 configurations × 20 seeds** (simulation only):

- Mean per-step Sharpe: **inventory-aware 0.528** vs **fixed-spread 0.375**.
- Inventory-aware had the higher mean Sharpe in **44 / 48** configurations.
- Inventory-aware held a lower mean absolute inventory in **48 / 48**.

A single backtest (`./build/examples/lob_backtest`, `seed=42`, `steps=5000`):

| Strategy | Total P&L | Sharpe (per-step) | Trades | Avg inv | Max abs inv |
|----------|----------:|------------------:|-------:|--------:|------------:|
| fixed-spread    | 34.33 | 0.350 | 2174 | 4.79 | 50 |
| inventory-aware | 34.00 | 0.590 | 2054 | 0.81 | 32 |

Representative grid config (`spread=100, qty=5, inv_limit=60, skew=6, cost=2`, 20 seeds):

| Strategy | Mean P&L | P&L 95% CI | Sharpe | Max abs inv |
|----------|---------:|:----------:|-------:|------------:|
| fixed-spread    | 17.89 | [17.43, 18.35] | 0.315 | 60 |
| inventory-aware | 16.07 | [15.92, 16.22] | 0.545 | 18 |

The inventory-aware maker trades modestly less gross P&L for much lower inventory
and tighter dispersion — a better risk-adjusted result **in this simulation
only**. Full methodology and statistics are in [docs/RESEARCH.md](docs/RESEARCH.md).

## Performance

Indicative microbenchmarks from **one developer laptop**, for regression tracking
and rough relative cost. They are **not** a claim about production exchange
latency. Regenerate locally with `./build/benchmarks/lob_benchmarks`; methodology
is in [benchmarks/README.md](benchmarks/README.md).

Environment: 12th Gen Intel Core i7-1255U (10C/12T, base ~1.70 GHz), 15.6 GB RAM,
Windows 11 Pro (build 26200), GCC 16.1.0 (MSYS2 UCRT64), C++20 `-O3` Release,
nanobench v4.3.11.

| Benchmark | ns/op | op/s |
|-----------|------:|-----:|
| best bid/ask lookup | 0.49 | 2.03 B |
| limit insertion (N=10000) | 208.0 | 4.81 M |
| insert+cancel cycle (N=10000) | 245.4 | 4.07 M |
| single-order matching (N=2000) | 271.6 | 3.68 M |
| multi-level matching (K=64) | 272.0 | 3.68 M |
| market-order execution (K=64) | 256.2 | 3.90 M |
| mixed order flow (N=20000) | 165.4 | 6.05 M |

## Results / figures

All figures are generated from the experiment output and aggregate over all seeds
and the parameter axes not shown. See [docs/RESULTS.md](docs/RESULTS.md) for the
full set and pipeline.

| Figure | Shows |
|--------|-------|
| [Sharpe vs spread](docs/figures/sharpe_vs_spread.svg) | Risk-adjusted return vs quoted spread, per strategy (95% CI band). |
| [P&L vs spread](docs/figures/pnl_vs_spread.svg) | Mean total P&L vs spread. |
| [Inventory vs spread](docs/figures/inventory_vs_spread.svg) | Mean max \|inventory\| vs spread. |
| [Sharpe vs inventory limit](docs/figures/sharpe_vs_inventory_limit.svg) | Sharpe vs the inventory cap. |
| [Strategy comparison](docs/figures/strategy_comparison.svg) | Whole-grid mean P&L and Sharpe per strategy, with CIs. |
| [P&L distributions](docs/figures/pnl_distributions.svg) | Per-seed P&L spread at a representative config. |
| [Sharpe heatmap — fixed](docs/figures/heatmap_sharpe_fixed_spread.svg) | Mean Sharpe over (spread × inventory limit), fixed-spread. |
| [Sharpe heatmap — inventory-aware](docs/figures/heatmap_sharpe_inventory_aware.svg) | Same heatmap for inventory-aware. |
| [P&L confidence intervals](docs/figures/pnl_confidence_intervals.svg) | P&L mean ± 95% CI per strategy across spreads. |

## Repository structure

```
include/lob/    Public headers (the library interface)
src/            Library sources and the placeholder executable
tests/          C++ unit tests (doctest)
benchmarks/     Performance benchmarks (nanobench)
examples/       Runnable examples: backtest, parameter study
viz/            Python (standard library) visualization package + tests
scripts/        Reproducible research pipeline (bash / PowerShell)
docs/           Design notes, research report, results, and figures/
```

## Reproducibility

Requires a C++20 compiler, CMake ≥ 3.20, and Python 3 (standard library only).
doctest and nanobench are fetched automatically at configure time.

```bash
# Clean build (library, tests, benchmarks, examples)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# C++ tests
ctest --test-dir build --output-on-failure

# Python (visualization data-layer) tests
python -m unittest discover -s viz/tests

# Benchmarks
./build/benchmarks/lob_benchmarks

# Single backtest
./build/examples/lob_backtest

# Parameter study (writes study_results.{json,csv} and study_runs.csv)
mkdir -p results && (cd results && ../build/examples/lob_param_study)

# Figures from the study output
python viz/plot_study.py --runs results/study_runs.csv --outdir docs/figures
```

The full research pipeline (build → study → figures) is also wrapped in one
command:

```bash
scripts/run_research.sh        # macOS / Linux / Git Bash
scripts\run_research.ps1       # Windows PowerShell
```

Everything is seeded and integer-based, so the study output and figures reproduce
exactly on the same build.

## Documentation

- [docs/DESIGN.md](docs/DESIGN.md) — architecture, data structures, matching and
  accounting semantics, and the synthetic market model.
- [docs/RESEARCH.md](docs/RESEARCH.md) — the research report: question, model
  assumptions, experimental design, results, statistical uncertainty, and
  interpretation.
- [docs/RESULTS.md](docs/RESULTS.md) — the visualization pipeline and every figure.

## Limitations

- The market is a **synthetic model**, not historical or live exchange data.
- The order-flow model is intentionally simplified (independent arrivals, an
  exponential reach model, no latency, queue priority beyond FIFO, or
  adverse-selection component).
- All strategy and study figures are **simulation results**; per-step Sharpe is
  not annualised and P&L is in abstract currency units on a single instrument.
- Benchmarks are hardware-, compiler-, and build-dependent and are indicative
  only.
- Nothing here is a claim of real-world profitability or of production exchange
  performance.

## Future work

- Historical market-data replay through the existing event-replay layer.
- Richer order-flow models (drift, mean reversion, autocorrelated flow, price
  impact / adverse selection).
- Additional market-making models (e.g. an Avellaneda–Stoikov-style quoter) using
  the same `Strategy` interface and backtesting harness.

## License

MIT — see [LICENSE](LICENSE).
