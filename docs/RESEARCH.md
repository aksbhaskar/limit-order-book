# Market-making parameter study — research report

> **Simulated research, not real-market evidence.** Every number in this report is
> produced by the project's toy synthetic market (see the
> [Synthetic market model](#synthetic-market-model) below and
> [DESIGN.md](DESIGN.md#synthetic-market-model)). It is a deliberately simple,
> transparent model — **not** calibrated to, or evidence about, any real venue.
> All figures are generated at run time by `examples/lob_param_study` and
> `viz/plot_study.py`; none are hand-entered. Re-running reproduces them exactly.

## Executive summary

Under a simple price-sensitive synthetic market, an **inventory-aware** market
maker — which shifts and widens its quotes against inventory — delivered **better
risk-adjusted performance and much lower inventory exposure** than a plain
**fixed-spread** maker, across nearly the entire parameter grid. Aggregating over
48 parameter configurations × 20 random seeds, the inventory-aware strategy's mean
per-step Sharpe was **0.528 vs 0.375**, it had the higher mean Sharpe in **44 of
48** configurations, and it held a smaller mean absolute inventory in **48 of 48**.
It achieved this at a modest cost in gross P&L. **These conclusions describe the
simulation only and do not generalize to real markets.**

## Research question

Under a simple price-sensitive market, how do a market maker's parameters (quoted
spread, order quantity, inventory limit, inventory-skew strength, transaction
cost) affect risk-adjusted performance — and does an inventory-aware quoting rule
improve on a fixed-spread rule on the *same* market?

## Market-making strategies

Both implement one `Strategy` interface and run through the identical engine,
accounting, and market generation.

- **Fixed-spread.** Quotes symmetric about the mid: bid at `mid − spread/2`, ask
  at `mid + spread/2`. Sizes are clamped to the remaining room under a symmetric
  inventory cap `±max_inventory`; a side is dropped when it has no room.
- **Inventory-aware.** Quotes around a *reservation price*
  `r = mid − skew · inventory`, with a half-spread that widens as inventory grows:
  `half = base_spread/2 + |inventory| · widen`. A long book lowers both quotes
  (encouraging sells, discouraging buys); a short book raises them. Same inventory
  cap and sizing.

## Model / mathematical assumptions

- **Money is exact.** Prices and P&L are integer ticks (fixed point); there is no
  floating-point rounding in the monetary path. Statistics (Sharpe, etc.) convert
  to floating point only for reporting.
- **Average-cost P&L.** Realized P&L is booked by average cost on position
  reductions; total P&L marked at price *m* is the exact cash figure
  `cash + position·m − starting_cash`, and unrealized is `total − realized`, so
  `realized + unrealized = total` holds exactly.
- **Fill model.** An aggressor takes a quote at distance `d` from the mid with
  probability `exp(−d / reach_mean)` (exponential reach; see below). Fills occur
  at the resting quote's price (maker-price rule).
- **Independence.** Order arrivals are independent across steps; the mid increment
  is i.i.d. uniform; seeds are independent. There is no autocorrelation, latency,
  or adverse-selection beyond what the reach distribution induces.

## Synthetic market model

Each step (fully seeded, deterministic):

1. **Mid.** The reference mid moves by an integer uniform on
   `[−mid_volatility_ticks, +mid_volatility_ticks]` (clamped positive). Higher
   volatility widens the range the mid explores.
2. **Arrival.** With probability `order_arrival_permille/1000`, one aggressor
   arrives on a fair-coin side (so both buy and sell flow occur).
3. **Price sensitivity.** The aggressor is a limit order priced a random *reach*
   from the mid, where `reach = −reach_mean · ln u`, `u ~ Uniform(0,1]`
   (inverse-CDF exponential). It crosses a quote only if the quote is within
   reach, so a quote at distance `d` fills with probability `exp(−d/reach_mean)`.
   It is immediate-or-cancel and never priced below one tick.

Only the strategy's own quotes provide liquidity. Base market for this study:
`steps=3000`, `initial_mid=1000.0000`, `mid_volatility_ticks=8`,
`order_arrival_permille=800`, `liquidity_reach_ticks=80`, `max_aggressor_qty=5`,
`starting_cash=100000.0000`.

## Experimental design

- **Fair comparison.** For each parameter point and seed, both strategies run on
  the *identical* market (same seed, mid path, arrivals, sides, sizes, reaches).
  The market's random stream does not depend on the strategy.
- **Distributions.** Each configuration runs over **20 independent seeds** (1..20);
  we report the distribution of per-run metrics.
- **Determinism.** A given `StudyConfig` yields exactly one result; the executable
  hard-codes no numbers.

## Parameter grid

Cartesian product (**48 points**):

| Parameter | Values |
|-----------|--------|
| quoted spread (ticks) | 60, 100, 160 |
| order quantity | 5, 10 |
| inventory limit | 30, 60 |
| inventory-skew strength (ticks/unit) | 2, 6 |
| transaction cost (ticks/unit) | 0, 2 |

→ 48 points × 2 strategies × 20 seeds = **1,920 backtests**.

## Evaluation metrics

Per run: total / realized / unrealized P&L, per-step Sharpe (risk-free 0, **not**
annualised — there is no calendar), maximum drawdown, fill rate, average
inventory, maximum absolute inventory, trade count. Each is aggregated across
seeds into mean / median / sample standard deviation and, for the mean, a 95%
confidence interval.

## Results

All numbers are the actual output of the run (see
[RESULTS.md](RESULTS.md) for the figures).

### Headline (representative config, 20 seeds)

`spread=100, order_qty=5, inventory_limit=60, skew=6`:

| Strategy | Cost | Mean P&L | P&L 95% CI | Sharpe | Avg inv | Max abs inv |
|----------|-----:|---------:|:----------:|-------:|--------:|------------:|
| fixed-spread    | 0 | 18.63 | [18.17, 19.10] | 0.325 | -0.29 | 59.9 |
| inventory-aware | 0 | 16.85 | [16.69, 17.00] | 0.557 |  0.07 | 18.1 |
| fixed-spread    | 2 | 17.89 | [17.43, 18.35] | 0.315 | -0.29 | 59.9 |
| inventory-aware | 2 | 16.07 | [15.92, 16.22] | 0.545 |  0.07 | 18.1 |

### Aggregate across the grid (48 points × 20 seeds)

- Mean per-step Sharpe: **fixed-spread 0.375**, **inventory-aware 0.528**.
- Inventory-aware had the higher mean Sharpe in **44 / 48** configs.
- Inventory-aware held a smaller `|average inventory|` in **48 / 48** configs.

### Parameter effects (inventory-aware, qty=5, limit=60, skew=6, cost=0)

| Spread (ticks) | Mean P&L | Sharpe | Fill rate |
|---------------:|---------:|-------:|----------:|
| 60  | 11.35 | 0.432 | 0.271 |
| 100 | 16.85 | 0.557 | 0.217 |
| 160 | 19.56 | 0.507 | 0.149 |

Wider spreads capture more P&L per fill but fill less often; Sharpe has an
interior optimum. Transaction costs reduce P&L roughly in proportion to trade
count (representative fixed-spread P&L falls 18.63 → 17.89 as cost goes 0 → 2).

## Statistical uncertainty / confidence intervals

Each reported mean carries a 95% confidence interval computed as
`mean ± 1.96 · s/√n` with the sample (n−1) standard deviation over `n = 20` seeds
(the same estimator in the C++ `Aggregate` and the Python loader, cross-checked by
tests). At the representative config the intervals are tight and **non-overlapping**
between strategies (e.g. fixed `[17.43, 18.35]` vs inventory-aware `[15.92, 16.22]`
for P&L; the Sharpe gap 0.315 vs 0.545 is far larger than seed noise). The
`pnl_confidence_intervals.svg` and `pnl_distributions.svg` figures show these
intervals and the per-seed spread directly. The CIs quantify **seed variance
only**, not model uncertainty.

## Interpretation

Because the market is price-sensitive, quote placement changes *which* fills
occur, not just their price. The inventory-aware maker exploits this: when it
accumulates a position it skews quotes to offload it, so it spends far less time
carrying inventory (max ~18 vs ~60 units at the representative config). Lower
inventory means lower exposure to the mid's random walk, which tightens its P&L
distribution and raises its per-step Sharpe even though its gross P&L is slightly
lower (it forgoes some spread capture to stay flat). This pattern — **better
risk-adjusted performance and lower inventory exposure for inventory-aware
quoting** — holds across almost the whole grid, not at a single cherry-picked
point. It is exactly the qualitative behaviour inventory-aware market making is
designed to produce, here demonstrated in a controlled, reproducible sandbox.

## Limitations

- The market is a toy: no real price dynamics, order-flow clustering, latency,
  queue priority beyond FIFO, or adverse selection beyond the reach model.
- Sharpe is per simulation step and not annualised; drawdowns are near zero
  because the mid walk is bounded in range.
- Results are conditional on the chosen base-market parameters and grid; they are
  **not** transferable to real venues.
- Confidence intervals capture seed variance only, not model or specification
  uncertainty.
- "P&L" is in abstract currency units on a single synthetic instrument.

## Reproducibility

```bash
scripts/run_research.sh        # build + run study + render figures (or .ps1 on Windows)
# or manually:
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
mkdir -p results && (cd results && ../build/examples/lob_param_study)
python viz/plot_study.py --runs results/study_runs.csv --outdir docs/figures
```

Everything is seeded and integer-based, so the study output and figures reproduce
bit-for-bit on the same build.

## Future work

- A price model with drift, mean reversion, or autocorrelated flow, and a
  price-impact / adverse-selection component, to stress the strategies harder.
- Additional strategies (e.g. an Avellaneda–Stoikov-style optimal quoter) using
  the same interface and harness.
- Latency and queue-position modelling in the matching path.
- Multi-instrument / correlated-inventory studies.

None of these change the standing disclaimer: results here describe the
simulation, not real markets.
