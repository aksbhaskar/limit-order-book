"""Loading and transformation of parameter-study output.

Consumes the CSV produced by the ``lob_param_study`` executable (both the
aggregated ``study_results.csv`` and the raw per-seed ``study_runs.csv``) and
provides small, testable grouping/aggregation helpers. No data is hard-coded and
no third-party packages are required.
"""

from __future__ import annotations

import csv
import math
from dataclasses import dataclass
from typing import Dict, Iterable, List, Optional, Sequence

# Numeric columns are parsed to int/float; everything else stays a string.
_INT_FIELDS = {
    "spread_ticks",
    "order_quantity",
    "max_inventory",
    "skew_ticks_per_unit",
    "transaction_cost_ticks",
    "seeds",
    "seed",
    "max_abs_inventory",
    "trade_count",
}


def _coerce(field: str, value: str):
    if field in _INT_FIELDS:
        try:
            return int(value)
        except ValueError:
            return float(value)
    try:
        return float(value)
    except ValueError:
        return value


def load_rows(path: str) -> List[dict]:
    """Loads a study CSV into a list of typed dict rows."""
    with open(path, newline="") as handle:
        reader = csv.DictReader(handle)
        rows = []
        for raw in reader:
            rows.append({k: _coerce(k, v) for k, v in raw.items()})
    return rows


@dataclass(frozen=True)
class Stats:
    """Distribution summary of a sample with a 95% CI of the mean."""

    n: int
    mean: float
    median: float
    std: float
    ci_low: float
    ci_high: float


def summarize(samples: Sequence[float]) -> Stats:
    """Mean/median/sample-stddev and a normal-approx 95% CI of the mean.

    Matches the C++ ``Aggregate`` (sample n-1 std dev, mean +/- 1.96*s/sqrt(n)).
    """
    n = len(samples)
    if n == 0:
        return Stats(0, 0.0, 0.0, 0.0, 0.0, 0.0)
    ordered = sorted(samples)
    mean = sum(ordered) / n
    if n % 2 == 1:
        median = ordered[n // 2]
    else:
        median = 0.5 * (ordered[n // 2 - 1] + ordered[n // 2])
    if n > 1:
        var = sum((x - mean) ** 2 for x in ordered) / (n - 1)
        std = math.sqrt(var)
        se = std / math.sqrt(n)
        return Stats(n, mean, median, std, mean - 1.96 * se, mean + 1.96 * se)
    return Stats(n, mean, median, 0.0, mean, mean)


class Dataset:
    """A filterable view over study rows."""

    def __init__(self, rows: Iterable[dict]):
        self.rows: List[dict] = list(rows)

    @classmethod
    def load(cls, path: str) -> "Dataset":
        return cls(load_rows(path))

    def __len__(self) -> int:
        return len(self.rows)

    def select(self, **filters) -> "Dataset":
        """Returns the subset of rows matching every field==value filter."""
        def keep(row: dict) -> bool:
            return all(row.get(k) == v for k, v in filters.items())

        return Dataset(r for r in self.rows if keep(r))

    def values(self, field: str) -> List:
        """Sorted unique values of a field."""
        return sorted({r[field] for r in self.rows})

    def samples(self, metric: str) -> List[float]:
        return [float(r[metric]) for r in self.rows]

    def summary(self, metric: str) -> Stats:
        return summarize(self.samples(metric))

    def group(self, field: str) -> Dict[object, "Dataset"]:
        """Groups rows by a field's value, preserving sorted key order."""
        out: Dict[object, List[dict]] = {}
        for row in self.rows:
            out.setdefault(row[field], []).append(row)
        return {k: Dataset(out[k]) for k in sorted(out)}

    def mean_by(
        self,
        x_field: str,
        metric: str,
        where: Optional[dict] = None,
    ) -> List[tuple]:
        """Returns [(x, Stats-over-metric), ...] grouped by x_field.

        Aggregates over whatever other axes remain after applying ``where``, so a
        curve shows the broad parameter study, never a single cherry-picked cell.
        """
        base = self.select(**where) if where else self
        return [(x, sub.summary(metric)) for x, sub in base.group(x_field).items()]
