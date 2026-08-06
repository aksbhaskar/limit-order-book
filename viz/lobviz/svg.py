"""Minimal, dependency-free SVG plotting primitives.

A small linear-axes plot plus heatmap/bar/line/scatter builders, enough for
publication-quality static figures (labels, units, legend, title, caption) with
no third-party libraries. Colours are a fixed, consistent palette so a strategy
keeps its colour across every figure.
"""

from __future__ import annotations

import html
import math
from dataclasses import dataclass, field
from typing import List, Optional, Sequence, Tuple

# Consistent palette across figures.
COLORS = {
    "fixed-spread": "#1f6feb",
    "inventory-aware": "#d9730d",
    "grid": "#e3e3e3",
    "axis": "#333333",
    "text": "#1a1a1a",
    "muted": "#666666",
}
SERIES_COLORS = ["#1f6feb", "#d9730d", "#2e8b57", "#8a5a44", "#7d3cc0"]

_FONT = "font-family='Helvetica,Arial,sans-serif'"


def _fmt(v: float) -> str:
    if v == int(v):
        return str(int(v))
    return f"{v:.4g}"


def _esc(s: str) -> str:
    return html.escape(str(s))


@dataclass
class Plot:
    """A single linear-axes chart that renders to a standalone SVG string."""

    width: int = 720
    height: int = 460
    title: str = ""
    xlabel: str = ""
    ylabel: str = ""
    caption: str = ""
    margin_left: int = 78
    margin_right: int = 150   # room for a legend
    margin_top: int = 54
    margin_bottom: int = 88

    _elems: List[str] = field(default_factory=list)
    _legend: List[Tuple[str, str]] = field(default_factory=list)
    xmin: float = 0.0
    xmax: float = 1.0
    ymin: float = 0.0
    ymax: float = 1.0

    # -- coordinate helpers -------------------------------------------------
    @property
    def _px0(self) -> int:
        return self.margin_left

    @property
    def _px1(self) -> int:
        return self.width - self.margin_right

    @property
    def _py0(self) -> int:
        return self.height - self.margin_bottom

    @property
    def _py1(self) -> int:
        return self.margin_top

    def set_xlim(self, lo: float, hi: float) -> None:
        self.xmin, self.xmax = (lo, hi) if lo != hi else (lo - 1, hi + 1)

    def set_ylim(self, lo: float, hi: float) -> None:
        if lo == hi:
            lo, hi = lo - 1, hi + 1
        pad = (hi - lo) * 0.08
        self.ymin, self.ymax = lo - pad, hi + pad

    def x(self, v: float) -> float:
        return self._px0 + (v - self.xmin) / (self.xmax - self.xmin) * (self._px1 - self._px0)

    def y(self, v: float) -> float:
        return self._py0 - (v - self.ymin) / (self.ymax - self.ymin) * (self._py0 - self._py1)

    # -- drawing ------------------------------------------------------------
    def _tick_values(self, lo: float, hi: float, n: int = 6) -> List[float]:
        span = hi - lo
        if span <= 0:
            return [lo]
        raw = span / n
        mag = 10 ** math.floor(math.log10(raw))
        for step in (1, 2, 2.5, 5, 10):
            if raw <= step * mag:
                nice = step * mag
                break
        else:
            nice = 10 * mag
        start = math.ceil(lo / nice) * nice
        vals = []
        v = start
        while v <= hi + 1e-9:
            vals.append(round(v, 10))
            v += nice
        return vals

    def axes(self, x_ticks: Optional[Sequence[float]] = None) -> None:
        xt = list(x_ticks) if x_ticks is not None else self._tick_values(self.xmin, self.xmax)
        yt = self._tick_values(self.ymin, self.ymax)
        for yv in yt:
            py = self.y(yv)
            self._elems.append(
                f"<line x1='{self._px0}' y1='{py:.1f}' x2='{self._px1}' y2='{py:.1f}' "
                f"stroke='{COLORS['grid']}' stroke-width='1'/>"
            )
            self._elems.append(
                f"<text x='{self._px0 - 8}' y='{py + 4:.1f}' text-anchor='end' "
                f"font-size='12' fill='{COLORS['muted']}' {_FONT}>{_fmt(yv)}</text>"
            )
        for xv in xt:
            px = self.x(xv)
            self._elems.append(
                f"<line x1='{px:.1f}' y1='{self._py0}' x2='{px:.1f}' y2='{self._py1}' "
                f"stroke='{COLORS['grid']}' stroke-width='1'/>"
            )
            self._elems.append(
                f"<text x='{px:.1f}' y='{self._py0 + 20}' text-anchor='middle' "
                f"font-size='12' fill='{COLORS['muted']}' {_FONT}>{_fmt(xv)}</text>"
            )
        # frame
        self._elems.append(
            f"<rect x='{self._px0}' y='{self._py1}' width='{self._px1 - self._px0}' "
            f"height='{self._py0 - self._py1}' fill='none' stroke='{COLORS['axis']}' "
            f"stroke-width='1.2'/>"
        )

    def band(self, xs: Sequence[float], lo: Sequence[float], hi: Sequence[float], color: str) -> None:
        pts_top = " ".join(f"{self.x(x):.1f},{self.y(h):.1f}" for x, h in zip(xs, hi))
        pts_bot = " ".join(f"{self.x(x):.1f},{self.y(l):.1f}" for x, l in reversed(list(zip(xs, lo))))
        self._elems.append(
            f"<polygon points='{pts_top} {pts_bot}' fill='{color}' fill-opacity='0.15' "
            f"stroke='none'/>"
        )

    def line(self, xs: Sequence[float], ys: Sequence[float], color: str, label: str = "",
             markers: bool = True) -> None:
        pts = " ".join(f"{self.x(x):.1f},{self.y(y):.1f}" for x, y in zip(xs, ys))
        self._elems.append(
            f"<polyline points='{pts}' fill='none' stroke='{color}' stroke-width='2.2'/>"
        )
        if markers:
            for x, y in zip(xs, ys):
                self._elems.append(
                    f"<circle cx='{self.x(x):.1f}' cy='{self.y(y):.1f}' r='3.2' fill='{color}'/>"
                )
        if label:
            self._legend.append((label, color))

    def points(self, xs: Sequence[float], ys: Sequence[float], color: str, r: float = 2.6,
               opacity: float = 0.55) -> None:
        for x, y in zip(xs, ys):
            self._elems.append(
                f"<circle cx='{self.x(x):.1f}' cy='{self.y(y):.1f}' r='{r}' fill='{color}' "
                f"fill-opacity='{opacity}'/>"
            )

    def hbar_marker(self, x: float, y: float, color: str, half_width: float = 10.0) -> None:
        """A short horizontal tick (used to mark a mean over a strip of points)."""
        self._elems.append(
            f"<line x1='{self.x(x) - half_width:.1f}' y1='{self.y(y):.1f}' "
            f"x2='{self.x(x) + half_width:.1f}' y2='{self.y(y):.1f}' stroke='{color}' "
            f"stroke-width='2.4'/>"
        )

    def errorbar(self, x: float, lo: float, hi: float, color: str) -> None:
        cap = 5
        self._elems.append(
            f"<line x1='{self.x(x):.1f}' y1='{self.y(lo):.1f}' x2='{self.x(x):.1f}' "
            f"y2='{self.y(hi):.1f}' stroke='{color}' stroke-width='1.6'/>"
        )
        for yv in (lo, hi):
            self._elems.append(
                f"<line x1='{self.x(x) - cap:.1f}' y1='{self.y(yv):.1f}' "
                f"x2='{self.x(x) + cap:.1f}' y2='{self.y(yv):.1f}' stroke='{color}' "
                f"stroke-width='1.6'/>"
            )

    def bar(self, x_center: float, width_data: float, value: float, color: str,
            base: float = 0.0, label: str = "") -> None:
        x0 = self.x(x_center - width_data / 2)
        x1 = self.x(x_center + width_data / 2)
        y_top = self.y(max(value, base))
        y_bot = self.y(min(value, base))
        self._elems.append(
            f"<rect x='{x0:.1f}' y='{y_top:.1f}' width='{x1 - x0:.1f}' "
            f"height='{max(0.5, y_bot - y_top):.1f}' fill='{color}' fill-opacity='0.85'/>"
        )
        if label and label not in [l for l, _ in self._legend]:
            self._legend.append((label, color))

    def text(self, px: float, py: float, s: str, size: int = 12, anchor: str = "middle",
             fill: Optional[str] = None) -> None:
        self._elems.append(
            f"<text x='{px:.1f}' y='{py:.1f}' text-anchor='{anchor}' font-size='{size}' "
            f"fill='{fill or COLORS['text']}' {_FONT}>{_esc(s)}</text>"
        )

    def render(self) -> str:
        parts = [
            f"<svg xmlns='http://www.w3.org/2000/svg' width='{self.width}' "
            f"height='{self.height}' viewBox='0 0 {self.width} {self.height}'>",
            f"<rect width='{self.width}' height='{self.height}' fill='white'/>",
        ]
        if self.title:
            parts.append(
                f"<text x='{self.width / 2:.0f}' y='26' text-anchor='middle' "
                f"font-size='17' font-weight='bold' fill='{COLORS['text']}' {_FONT}>"
                f"{_esc(self.title)}</text>"
            )
        parts.extend(self._elems)
        # axis labels
        if self.xlabel:
            parts.append(
                f"<text x='{(self._px0 + self._px1) / 2:.0f}' y='{self.height - self.margin_bottom + 44}' "
                f"text-anchor='middle' font-size='13' fill='{COLORS['text']}' {_FONT}>"
                f"{_esc(self.xlabel)}</text>"
            )
        if self.ylabel:
            cx, cy = 20, (self._py0 + self._py1) / 2
            parts.append(
                f"<text x='{cx}' y='{cy:.0f}' text-anchor='middle' font-size='13' "
                f"fill='{COLORS['text']}' transform='rotate(-90 {cx} {cy:.0f})' {_FONT}>"
                f"{_esc(self.ylabel)}</text>"
            )
        # legend
        lx = self._px1 + 16
        ly = self._py1 + 6
        for i, (label, color) in enumerate(self._legend):
            y = ly + i * 20
            parts.append(f"<rect x='{lx}' y='{y - 9}' width='12' height='12' fill='{color}'/>")
            parts.append(
                f"<text x='{lx + 18}' y='{y + 1}' font-size='12' fill='{COLORS['text']}' "
                f"{_FONT}>{_esc(label)}</text>"
            )
        # caption
        if self.caption:
            parts.append(
                f"<text x='{self._px0}' y='{self.height - 10}' font-size='11' "
                f"fill='{COLORS['muted']}' {_FONT}>{_esc(self.caption)}</text>"
            )
        parts.append("</svg>")
        return "\n".join(parts)
