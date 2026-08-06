#!/usr/bin/env python3
"""Generate all research figures from the parameter-study output.

Reads the raw per-seed runs written by ``lob_param_study`` (``study_runs.csv``)
and writes a fixed set of SVG figures. No values are hard-coded; every figure is
computed from the experiment output. Pure standard library.

Usage:
    python viz/plot_study.py --runs results/study_runs.csv --outdir docs/figures
"""

from __future__ import annotations

import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from lobviz import Dataset  # noqa: E402
from lobviz import figures  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser(description="Render market-making study figures.")
    parser.add_argument("--runs", default="results/study_runs.csv",
                        help="Path to study_runs.csv (raw per-seed runs).")
    parser.add_argument("--outdir", default="docs/figures",
                        help="Directory to write SVG figures into.")
    args = parser.parse_args()

    if not os.path.exists(args.runs):
        parser.error(f"runs file not found: {args.runs} (run lob_param_study first)")

    data = Dataset.load(args.runs)
    os.makedirs(args.outdir, exist_ok=True)

    # Representative configuration for the per-seed distribution figure.
    rep = dict(spread=100, order_qty=5, max_inv=60, skew=6, cost=2)

    outputs = {
        "sharpe_vs_spread.svg": figures.sharpe_vs_spread(data),
        "pnl_vs_spread.svg": figures.pnl_vs_spread(data),
        "inventory_vs_spread.svg": figures.inventory_vs_spread(data),
        "sharpe_vs_inventory_limit.svg": figures.sharpe_vs_inventory_limit(data),
        "strategy_comparison.svg": figures.strategy_comparison(data),
        "pnl_distributions.svg": figures.pnl_distributions(data, **rep),
        "heatmap_sharpe_fixed_spread.svg": figures.grid_heatmap(
            data, "fixed-spread", "sharpe", "spread_ticks", "max_inventory",
            title="Mean Sharpe grid -- fixed-spread", unit="Sharpe"),
        "heatmap_sharpe_inventory_aware.svg": figures.grid_heatmap(
            data, "inventory-aware", "sharpe", "spread_ticks", "max_inventory",
            title="Mean Sharpe grid -- inventory-aware", unit="Sharpe"),
        "pnl_confidence_intervals.svg": figures.ci_comparison(data),
    }

    for name, svg in outputs.items():
        path = os.path.join(args.outdir, name)
        with open(path, "w", encoding="utf-8") as handle:
            handle.write(svg)
        print(f"wrote {path}")

    print(f"\n{len(outputs)} figures written to {args.outdir} "
          f"from {len(data)} runs in {args.runs}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
