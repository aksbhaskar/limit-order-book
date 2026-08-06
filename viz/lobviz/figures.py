"""Figure builders for the parameter study.

Each function takes a raw-runs ``Dataset`` (loaded from ``study_runs.csv``) and
returns an SVG string. Curves aggregate over all seeds and over the parameter
axes not shown, so every figure reflects the broad study rather than a single
cherry-picked configuration. Confidence bands/bars are 95% CIs of the mean.
"""

from __future__ import annotations

from typing import List

from .data import Dataset
from .svg import COLORS, Plot

STRATS = ["fixed-spread", "inventory-aware"]
_SIM = "Simulated toy market -- not real-market performance."


def _autoscale_y(plot: Plot, *value_lists: List[float]) -> None:
    vals = [v for lst in value_lists for v in lst]
    plot.set_ylim(min(vals), max(vals))


def metric_vs_axis(data: Dataset, x_field: str, metric: str, *, title: str,
                   xlabel: str, ylabel: str, caption_extra: str = "") -> str:
    """Line chart of mean(metric) vs x_field, one curve per strategy, 95% CI band."""
    plot = Plot(title=title, xlabel=xlabel, ylabel=ylabel,
                caption=f"{_SIM} {caption_extra}".strip())
    xs_all = data.values(x_field)
    plot.set_xlim(min(xs_all), max(xs_all))

    lows, highs = [], []
    curves = []
    for strat in STRATS:
        sub = data.select(strategy=strat)
        series = sub.mean_by(x_field, metric)
        xs = [x for x, _ in series]
        means = [s.mean for _, s in series]
        lo = [s.ci_low for _, s in series]
        hi = [s.ci_high for _, s in series]
        curves.append((strat, xs, means, lo, hi))
        lows += lo
        highs += hi
    _autoscale_y(plot, lows, highs)
    plot.axes(x_ticks=xs_all)
    for strat, xs, means, lo, hi in curves:
        color = COLORS[strat]
        plot.band(xs, lo, hi, color)
        plot.line(xs, means, color, label=strat)
    return plot.render()


def sharpe_vs_spread(data: Dataset) -> str:
    return metric_vs_axis(
        data, "spread_ticks", "sharpe",
        title="Risk-adjusted return vs quoted spread",
        xlabel="Quoted spread (ticks)", ylabel="Mean per-step Sharpe",
        caption_extra="Aggregated over all other parameters and 20 seeds; band = 95% CI.")


def pnl_vs_spread(data: Dataset) -> str:
    return metric_vs_axis(
        data, "spread_ticks", "total_pnl",
        title="Total P&L vs quoted spread",
        xlabel="Quoted spread (ticks)", ylabel="Mean total P&L (currency units)",
        caption_extra="Aggregated over all other parameters and 20 seeds; band = 95% CI.")


def inventory_vs_spread(data: Dataset) -> str:
    """Mean maximum absolute inventory vs spread, per strategy."""
    return metric_vs_axis(
        data, "spread_ticks", "max_abs_inventory",
        title="Inventory exposure vs quoted spread",
        xlabel="Quoted spread (ticks)", ylabel="Mean max |inventory| (units)",
        caption_extra="Lower is less inventory risk. Band = 95% CI over seeds.")


def sharpe_vs_inventory_limit(data: Dataset) -> str:
    return metric_vs_axis(
        data, "max_inventory", "sharpe",
        title="Risk-adjusted return vs inventory limit",
        xlabel="Inventory limit (units)", ylabel="Mean per-step Sharpe",
        caption_extra="Aggregated over all other parameters and 20 seeds; band = 95% CI.")


def strategy_comparison(data: Dataset) -> str:
    """Grouped bars: mean P&L and mean Sharpe per strategy over the whole grid."""
    plot = Plot(title="Fixed-spread vs inventory-aware (whole grid)",
                xlabel="", ylabel="Mean value over all runs",
                caption=f"{_SIM} Bars aggregate every grid point and seed; error bars = 95% CI.",
                margin_bottom=96)
    # Two metric groups on a shared axis, scaled to each other via normalisation.
    metrics = [("total_pnl", "Total P&L"), ("sharpe", "Sharpe x10")]
    scale = {"total_pnl": 1.0, "sharpe": 10.0}
    plot.set_xlim(-0.5, len(metrics) - 0.5)

    bar_vals, los, his = [], [], []
    prepared = []
    for gi, (metric, _label) in enumerate(metrics):
        for si, strat in enumerate(STRATS):
            st = data.select(strategy=strat).summary(metric)
            v = st.mean * scale[metric]
            lo = st.ci_low * scale[metric]
            hi = st.ci_high * scale[metric]
            xc = gi + (si - 0.5) * 0.36
            prepared.append((xc, v, lo, hi, strat))
            bar_vals.append(v)
            los.append(lo)
            his.append(hi)
    _autoscale_y(plot, bar_vals, los, his, [0.0])
    plot.axes(x_ticks=[])
    for xc, v, lo, hi, strat in prepared:
        plot.bar(xc, 0.32, v, COLORS[strat], base=0.0, label=strat)
        plot.errorbar(xc, lo, hi, COLORS["axis"])
    for gi, (_metric, label) in enumerate(metrics):
        plot.text(plot.x(gi), plot._py0 + 20, label, size=12)
    return plot.render()


def pnl_distributions(data: Dataset, spread: int, order_qty: int, max_inv: int,
                      skew: int, cost: int) -> str:
    """Per-seed P&L strip plot for both strategies at one configuration."""
    plot = Plot(
        title="P&L distribution across seeds",
        xlabel="", ylabel="Total P&L per run (currency units)",
        caption=(f"{_SIM} Config: spread={spread} qty={order_qty} inv_limit={max_inv} "
                 f"skew={skew} cost={cost}; each dot is one of 20 seeds, tick = mean."),
        margin_bottom=96)
    where = dict(spread_ticks=spread, order_quantity=order_qty, max_inventory=max_inv,
                 skew_ticks_per_unit=skew, transaction_cost_ticks=cost)
    plot.set_xlim(-0.5, len(STRATS) - 0.5)

    all_vals = []
    columns = []
    for si, strat in enumerate(STRATS):
        vals = data.select(strategy=strat, **where).samples("total_pnl")
        columns.append((si, strat, vals))
        all_vals += vals
    plot.set_ylim(min(all_vals), max(all_vals))
    plot.axes(x_ticks=[])
    for si, strat, vals in columns:
        color = COLORS[strat]
        # slight deterministic horizontal jitter by index
        n = len(vals)
        xs = [si + ((i / max(1, n - 1)) - 0.5) * 0.28 for i in range(n)]
        plot.points(xs, sorted(vals), color)
        mean = sum(vals) / len(vals)
        plot.hbar_marker(si, mean, COLORS["axis"], half_width=26)
        plot.text(plot.x(si), plot._py0 + 20, strat, size=12, fill=color)
    return plot.render()


def grid_heatmap(data: Dataset, strategy: str, metric: str, row_field: str,
                 col_field: str, *, title: str, unit: str) -> str:
    """Heatmap of mean(metric) over a (row_field x col_field) grid for one strategy."""
    sub = data.select(strategy=strategy)
    rows = sub.values(row_field)
    cols = sub.values(col_field)

    matrix = []
    vmin = vmax = None
    for r in rows:
        line = []
        for c in cols:
            st = sub.select(**{row_field: r, col_field: c}).summary(metric)
            line.append(st.mean)
            vmin = st.mean if vmin is None else min(vmin, st.mean)
            vmax = st.mean if vmax is None else max(vmax, st.mean)
        matrix.append(line)

    plot = Plot(title=title, xlabel=col_field.replace("_", " "),
                ylabel=row_field.replace("_", " "),
                caption=f"{_SIM} {strategy}; cell = mean {metric} ({unit}) over other axes and seeds.",
                margin_left=96, margin_right=90, margin_bottom=88)
    n_c, n_r = len(cols), len(rows)
    x0, x1 = plot.margin_left, plot.width - plot.margin_right
    y0, y1 = plot.margin_top, plot.height - plot.margin_bottom
    cw = (x1 - x0) / n_c
    ch = (y0 - 0 + (y1 - y0)) / n_r if n_r else 0
    ch = (y1 - y0) / n_r

    def color_for(v: float) -> str:
        if vmax == vmin:
            t = 0.5
        else:
            t = (v - vmin) / (vmax - vmin)
        # blue (low) -> yellow (mid) -> red (high)
        r = int(255 * min(1.0, 0.2 + 1.6 * t))
        g = int(255 * (0.5 + 0.5 * (1 - abs(t - 0.5) * 2)))
        b = int(255 * max(0.0, 0.9 - 1.6 * t))
        return f"#{r:02x}{g:02x}{b:02x}"

    for ri, r in enumerate(rows):
        for ci, c in enumerate(cols):
            v = matrix[ri][ci]
            px = x0 + ci * cw
            py = y1 - (ri + 1) * ch
            plot._elems.append(
                f"<rect x='{px:.1f}' y='{py:.1f}' width='{cw:.1f}' height='{ch:.1f}' "
                f"fill='{color_for(v)}' stroke='white' stroke-width='1'/>"
            )
            plot._elems.append(
                f"<text x='{px + cw / 2:.1f}' y='{py + ch / 2 + 4:.1f}' text-anchor='middle' "
                f"font-size='11' fill='#111'>{v:.2f}</text>"
            )
        plot._elems.append(
            f"<text x='{x0 - 8:.1f}' y='{y1 - ri * ch - ch / 2 + 4:.1f}' text-anchor='end' "
            f"font-size='12' fill='{COLORS['muted']}'>{r}</text>"
        )
    for ci, c in enumerate(cols):
        plot._elems.append(
            f"<text x='{x0 + ci * cw + cw / 2:.1f}' y='{y1 + 18:.1f}' text-anchor='middle' "
            f"font-size='12' fill='{COLORS['muted']}'>{c}</text>"
        )
    return plot.render()


def ci_comparison(data: Dataset) -> str:
    """P&L mean with 95% CI for both strategies across spreads (interval clarity)."""
    plot = Plot(title="Total P&L with 95% confidence intervals",
                xlabel="Quoted spread (ticks)", ylabel="Mean total P&L (currency units)",
                caption=f"{_SIM} Points offset per strategy; whiskers = 95% CI of the mean over seeds.")
    spreads = data.values("spread_ticks")
    plot.set_xlim(min(spreads) - 8, max(spreads) + 8)
    los, his = [], []
    prepared = []
    for si, strat in enumerate(STRATS):
        sub = data.select(strategy=strat)
        for x, st in sub.mean_by("spread_ticks", "total_pnl"):
            xoff = x + (si - 0.5) * 6
            prepared.append((xoff, st.mean, st.ci_low, st.ci_high, strat))
            los.append(st.ci_low)
            his.append(st.ci_high)
    _autoscale_y(plot, los, his)
    plot.axes(x_ticks=spreads)
    seen = set()
    for xoff, mean, lo, hi, strat in prepared:
        color = COLORS[strat]
        plot.errorbar(xoff, lo, hi, color)
        plot._elems.append(
            f"<circle cx='{plot.x(xoff):.1f}' cy='{plot.y(mean):.1f}' r='3.4' fill='{color}'/>"
        )
        if strat not in seen:
            plot._legend.append((strat, color))
            seen.add(strat)
    return plot.render()
