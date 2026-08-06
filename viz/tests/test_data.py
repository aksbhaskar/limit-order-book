"""Unit tests for the lobviz data-loading / transformation layer.

Run with:  python -m unittest discover -s viz/tests
"""

import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from lobviz import Dataset, summarize  # noqa: E402
from lobviz.data import load_rows  # noqa: E402


def _rows():
    # Two strategies x two spreads x two seeds, with known P&L values.
    data = []
    for strat in ("fixed-spread", "inventory-aware"):
        for spread in (60, 100):
            for seed, pnl in ((1, 10.0), (2, 20.0)):
                data.append({
                    "strategy": strat,
                    "spread_ticks": spread,
                    "seed": seed,
                    "total_pnl": pnl + (0.0 if strat == "fixed-spread" else 5.0),
                    "sharpe": 0.5,
                })
    return data


class SummarizeTest(unittest.TestCase):
    def test_matches_cpp_aggregate(self):
        s = summarize([1, 2, 3, 4, 5])
        self.assertEqual(s.n, 5)
        self.assertAlmostEqual(s.mean, 3.0)
        self.assertAlmostEqual(s.median, 3.0)
        self.assertAlmostEqual(s.std, 1.5811388, places=6)
        self.assertAlmostEqual(s.ci_low, 1.6142, places=3)
        self.assertAlmostEqual(s.ci_high, 4.3858, places=3)

    def test_even_count_median(self):
        self.assertAlmostEqual(summarize([1, 2, 3, 4]).median, 2.5)

    def test_degenerate(self):
        one = summarize([7])
        self.assertEqual(one.n, 1)
        self.assertAlmostEqual(one.std, 0.0)
        self.assertAlmostEqual(one.ci_low, 7.0)
        self.assertEqual(summarize([]).n, 0)


class DatasetTest(unittest.TestCase):
    def setUp(self):
        self.data = Dataset(_rows())

    def test_values_are_sorted_unique(self):
        self.assertEqual(self.data.values("spread_ticks"), [60, 100])
        self.assertEqual(self.data.values("strategy"),
                         ["fixed-spread", "inventory-aware"])

    def test_select_filters(self):
        sub = self.data.select(strategy="fixed-spread", spread_ticks=60)
        self.assertEqual(len(sub), 2)
        self.assertEqual(sorted(sub.samples("total_pnl")), [10.0, 20.0])

    def test_group(self):
        groups = self.data.group("strategy")
        self.assertEqual(set(groups), {"fixed-spread", "inventory-aware"})
        self.assertEqual(len(groups["fixed-spread"]), 4)

    def test_mean_by_aggregates_over_other_axes(self):
        # For fixed-spread, mean P&L at each spread is (10+20)/2 = 15.
        series = self.data.select(strategy="fixed-spread").mean_by("spread_ticks", "total_pnl")
        xs = [x for x, _ in series]
        means = [round(s.mean, 6) for _, s in series]
        self.assertEqual(xs, [60, 100])
        self.assertEqual(means, [15.0, 15.0])

    def test_load_rows_types(self):
        with tempfile.TemporaryDirectory() as d:
            path = os.path.join(d, "runs.csv")
            with open(path, "w", encoding="utf-8") as fh:
                fh.write("strategy,spread_ticks,seed,total_pnl\n")
                fh.write("fixed-spread,60,1,12.5\n")
            rows = load_rows(path)
            self.assertEqual(rows[0]["strategy"], "fixed-spread")
            self.assertEqual(rows[0]["spread_ticks"], 60)     # int
            self.assertEqual(rows[0]["seed"], 1)              # int
            self.assertAlmostEqual(rows[0]["total_pnl"], 12.5)  # float


if __name__ == "__main__":
    unittest.main()
