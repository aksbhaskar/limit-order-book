# Market-making parameter study

> **Simulated research, not real-market evidence.** Every number in this report
> is produced by the project's toy synthetic market (see
> [DESIGN.md](DESIGN.md#synthetic-market-model)). It is a deliberately simple
> model and is **not** calibrated to, or evidence about, any real venue. All
> figures are generated at run time by `examples/lob_param_study`; none are
> hand-entered. Re-running the executable reproduces them exactly.

## Research question

Under a simple price-sensitive market, how do a market maker's parameters
(quoted spread, order quantity, inventory limit, inventory-skew strength, and
transaction cost) affect risk-adjusted performance — and does an inventory-aware
quoting rule improve on a fixed-spread rule on the same market?

## Synthetic market assumptions

The market is the one described in [DESIGN.md](DESIGN.md#synthetic-market-model):

- The reference mid follows a bounded random walk with per-step increment uniform
  on `[-mid_volatility_ticks, +mid_volatility_ticks]`.
- Each step, with probability `order_arrival_permille/1000`, one aggressor arrives
  on a fair-coin side.
- The aggressor is a limit order whose "reach" from the mid is exponentially
  distributed (mean `liquidity_reach_ticks`), so a quote at distance `d` from the
  mid is taken with probability `exp(-d/mean)` — tighter quotes fill more often.
- Only the strategy's own quotes provide liquidity; the aggressor is
  immediate-or-cancel. There is no latency, queueing beyond FIFO, autocorrelated
  flow, or adverse-selection model beyond the reach distribution.

Base market for this study: `steps=3000`, `initial_mid=1000.0000`,
`mid_volatility_ticks=8`, `order_arrival_permille=800`, `liquidity_reach_ticks=80`,
`max_aggressor_qty=5`, `starting_cash=100000.0000`.

## Experimental design

- **Strategies.** `FixedSpreadMarketMaker` vs `InventoryAwareMarketMaker`, both
  using the same engine, accounting, and market generation.
- **Fair comparison.** For each parameter point and each seed, both strategies run
  on the *identical* market (same seed, mid path, arrivals, sides, sizes, and
  reaches). The market's random stream does not depend on the strategy.
- **Distributions, not points.** Each configuration is run over **20 independent
  seeds** (1..20). We report the distribution of per-run metrics, including a 95%
  confidence interval of the mean P&L (normal approximation,
  `mean ± 1.96·s/√n`).

## Parameter grid

The Cartesian product of:

| Parameter | Values |
|-----------|--------|
| quoted spread (ticks) | 60, 100, 160 |
| order quantity | 5, 10 |
| inventory limit | 30, 60 |
| inventory-skew strength (ticks/unit) | 2, 6 |
| transaction cost (ticks/unit) | 0, 2 |

= **48 parameter points × 2 strategies × 20 seeds = 1,920 backtests.**

## Evaluation metrics

Per run: total P&L, per-step Sharpe (risk-free 0, not annualised), maximum
drawdown, fill rate, average inventory, maximum absolute inventory, and trade
count. Each is aggregated across seeds into mean / median / standard deviation and
(for P&L) a 95% confidence interval.

## Results

All numbers below are the actual output of the run.

### Headline comparison (representative config)

`spread=100, order_qty=5, inventory_limit=60, skew=6`, over 20 seeds:

| Strategy | Cost | Mean P&L | P&L 95% CI | Sharpe | Avg inv | Max abs inv |
|----------|-----:|---------:|:----------:|-------:|--------:|------------:|
| fixed-spread    | 0 | 18.63 | [18.17, 19.10] | 0.325 | -0.29 | 59.9 |
| inventory-aware | 0 | 16.85 | [16.69, 17.00] | 0.557 |  0.07 | 18.1 |
| fixed-spread    | 2 | 17.89 | [17.43, 18.35] | 0.315 | -0.29 | 59.9 |
| inventory-aware | 2 | 16.07 | [15.92, 16.22] | 0.545 |  0.07 | 18.1 |

The inventory-aware maker earns modestly *less* gross P&L but with **much lower
inventory** (max ~18 vs ~60) and much tighter P&L dispersion, so its per-step
Sharpe is materially higher and its confidence interval is far narrower.

### Aggregate across the whole grid (48 points × 20 seeds)

- Mean Sharpe: **fixed-spread 0.375**, **inventory-aware 0.528**.
- Inventory-aware had the higher mean Sharpe in **44 / 48** configurations.
- Inventory-aware held a smaller `|average inventory|` in **48 / 48** configurations.

### Parameter effects (inventory-aware, qty=5, limit=60, skew=6, cost=0)

| Spread (ticks) | Mean P&L | Sharpe | Fill rate |
|---------------:|---------:|-------:|----------:|
| 60  | 11.35 | 0.432 | 0.271 |
| 100 | 16.85 | 0.557 | 0.217 |
| 160 | 19.56 | 0.507 | 0.149 |

Wider spreads capture more P&L per fill but fill less often; risk-adjusted return
(Sharpe) peaks at an intermediate spread. Transaction costs reduce P&L roughly in
proportion to trade count (e.g. the representative fixed-spread P&L falls from
18.63 to 17.89 as cost rises from 0 to 2 ticks/unit).

## Limitations

- The market is a toy: no real price dynamics, order-flow clustering, latency,
  queue priority beyond FIFO, or adverse selection beyond the reach model.
- Sharpe is per simulation step and not annualised; drawdowns are near zero
  because the mid walk is bounded and mean-reverting in range.
- Results are conditional on the chosen base-market parameters and the specific
  grid; they are **not** transferable to real venues.
- The confidence intervals quantify seed variance only, not model uncertainty.

## Conclusions (supported by the simulation only)

Within this synthetic market: leaning quotes against inventory sharply reduces
inventory risk at a small cost in gross P&L, improving risk-adjusted return
across almost the entire parameter grid; spread has an interior optimum for
Sharpe; and transaction costs erode P&L in proportion to activity. These
conclusions describe the model and must not be read as claims about real markets.

## Reproducing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/examples/lob_param_study      # prints the summary; writes study_results.{json,csv}
```
