# Benchmarks

Microbenchmarks for the order book and matching engine, built on
[nanobench](https://github.com/martinus/nanobench) (a single-header, dependency-
free benchmarking library). They exist to catch regressions and to give a rough,
reproducible sense of relative cost — **not** to make claims about low latency or
production-exchange throughput. The numbers are from one developer laptop; treat
them as indicative, not universal.

## Building and running

Benchmarks build with the rest of the project (option `LOB_BUILD_BENCHMARKS`,
on by default at the top level). Build in **Release** for meaningful figures:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/benchmarks/lob_benchmarks
```

nanobench prints a table of median `ns/op`, `op/s`, and a noise estimate
(`err%`). Re-running reproduces the same workloads because every generator is
seeded with a fixed constant.

## Methodology

- **Deterministic workloads.** Orders are generated from fixed PRNG seeds
  (`std::mt19937_64`), so the same sequence of orders is used on every run and
  across machines. Prices are integer ticks around a 1000.0000 mid; bids sit
  below the mid and asks above it.
- **Setup vs. measurement.** Workload generation and any pre-populated book are
  built *outside* the timed lambda. The measured region performs only the
  operation(s) under test, plus the data structure's own unavoidable allocation
  (e.g. a level node on first insert at a price) — which is a real part of the
  operation's cost.
- **Per-operation figures.** For batched workloads the lambda performs *N*
  logical operations and `bench.batch(N)` divides the time by *N*, so the table
  reads as cost per operation.
- **No measured noise.** The timed region contains no assertions, logging, or
  printing; `doNotOptimizeAway` prevents the optimiser from deleting the work.
- **Stability.** A minimum epoch-iteration floor is set so heavy batch lambdas
  run enough times for the median to settle (small `err%`).

## Workloads

| Benchmark | What it measures | Notes / assumptions |
|-----------|------------------|---------------------|
| `limit insertion (N)` | Inserting *N* non-crossing limit orders into a fresh book | Empty-book construction is negligible vs. *N* inserts, so this is effectively pure insertion. |
| `best bid/ask lookup` | Two `best_bid()`/`best_ask()` reads on a pre-populated 10k-order book | Pure `const` reads; the fastest operation by far. |
| `insert+cancel cycle (N)` | Inserting *N* orders then cancelling all *N* | You cannot cancel *N* orders without first inserting them, so cancellation is reported as a full cycle. **To isolate cancel cost, subtract the insertion baseline** of the same *N*. |
| `single-order matching (N)` | Resting a maker and fully filling it with a crossing taker, *N* times | Each unit is one submitted maker + one submitted taker producing one trade; includes the maker insertion. |
| `multi-level matching (K)` | One limit taker sweeping *K* resting price levels | Batch is the *K* consumed makers; includes building the *K* levels. |
| `market-order execution (K)` | One market taker sweeping *K* resting price levels | Same as above with an `OrderType::Market` taker. |
| `mixed order flow (N)` | A blended feed of *N* actions: ~65% resting limits, ~20% market takers, ~15% cancellations of earlier ids | End-to-end engine throughput on a realistic mix. |

## Interpreting the numbers

The cancellation and matching benchmarks deliberately include the cost of
building the scenario they operate on, because those operations are inherently
paired with book population. The insertion and lookup benchmarks are the two
"pure" primitives; use them as baselines when reasoning about the composite
numbers. Absolute values depend heavily on CPU, memory, compiler, and build
flags — always regenerate them locally before drawing conclusions.
