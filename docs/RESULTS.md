# Research figures & visualization pipeline

> **Simulated results.** Every figure here is generated from the project's toy
> synthetic market (see [DESIGN.md](DESIGN.md#synthetic-market-model) and the full
> writeup in [RESEARCH.md](RESEARCH.md)). Nothing is hand-drawn or hand-entered;
> all figures are rendered directly from the experiment's CSV output. They are
> **not** evidence about real markets.

## Pipeline

The visualization layer is pure-standard-library Python (no matplotlib / numpy /
pandas) that reads the parameter study's output and emits SVG figures:

```
build/examples/lob_param_study   ->  study_runs.csv (raw per-seed runs)
                                      study_results.csv / .json (aggregates)
                                            |
viz/plot_study.py  --runs study_runs.csv   |   (loads, groups, aggregates,
                                            v    computes 95% CIs)
                                      docs/figures/*.svg
```

- `viz/lobviz/data.py` — loads the CSV and provides the grouping/aggregation
  transforms (mean, median, sample std dev, 95% CI). This is the tested layer.
- `viz/lobviz/svg.py` — a small, reusable, configurable SVG plotting toolkit
  (axes, CI bands, error bars, bars, strip plots, heatmaps).
- `viz/lobviz/figures.py` — the figure builders; each aggregates over all seeds
  and over the parameter axes not shown, so no single cell is cherry-picked.
- `viz/plot_study.py` — CLI that writes every figure.

Tests for the data layer live in `viz/tests/` and run with:

```bash
python -m unittest discover -s viz/tests
```

## Reproducing

One command builds the project, runs the experiment, and renders all figures:

```bash
scripts/run_research.sh          # macOS / Linux / Git Bash
# or, on Windows PowerShell:
scripts\run_research.ps1
```

It writes the study CSV/JSON to `results/` (git-ignored, regenerated
deterministically) and the SVG figures to `docs/figures/`.

## Figures

All aggregate curves are over the full 48-point grid × 20 seeds; bands and
whiskers are 95% confidence intervals of the mean.

| Figure | Shows |
|--------|-------|
| [sharpe_vs_spread.svg](figures/sharpe_vs_spread.svg) | Risk-adjusted return vs quoted spread, per strategy. Inventory-aware sits well above fixed-spread across every spread; Sharpe peaks at an interior spread. |
| [pnl_vs_spread.svg](figures/pnl_vs_spread.svg) | Mean total P&L vs spread. Wider spreads capture more P&L per fill. |
| [inventory_vs_spread.svg](figures/inventory_vs_spread.svg) | Mean max \|inventory\| vs spread. Inventory-aware holds far less inventory at every spread. |
| [sharpe_vs_inventory_limit.svg](figures/sharpe_vs_inventory_limit.svg) | Sharpe vs the inventory cap. |
| [strategy_comparison.svg](figures/strategy_comparison.svg) | Whole-grid mean P&L and Sharpe per strategy, with 95% CIs. |
| [pnl_distributions.svg](figures/pnl_distributions.svg) | Per-seed P&L spread at a representative config — variation across the 20 seeds. |
| [heatmap_sharpe_fixed_spread.svg](figures/heatmap_sharpe_fixed_spread.svg) | Mean-Sharpe heatmap over (spread × inventory limit), fixed-spread. |
| [heatmap_sharpe_inventory_aware.svg](figures/heatmap_sharpe_inventory_aware.svg) | Same heatmap for inventory-aware — uniformly higher. |
| [pnl_confidence_intervals.svg](figures/pnl_confidence_intervals.svg) | P&L mean ± 95% CI per strategy across spreads; the intervals are tight and mostly non-overlapping. |

## What the figures show

- **Where inventory-aware wins.** Across the grid it earns a higher per-step
  Sharpe (0.53 vs 0.38 on average) while holding a fraction of the inventory —
  visible in `sharpe_vs_spread`, `inventory_vs_spread`, and both heatmaps.
- **The spread trade-off.** Wider spreads raise P&L-per-fill but lower fill rate;
  Sharpe has an interior optimum (`sharpe_vs_spread`, `pnl_vs_spread`).
- **Parameter sensitivity.** The heatmaps and per-axis curves show how results
  move with spread and inventory limit.
- **Seed variation.** `pnl_distributions` and the CI bands make the spread across
  random seeds explicit, so no claim rests on a single lucky run.

Again: these describe the **simulation only**.
