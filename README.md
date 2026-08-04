# Limit Order Book & Exchange Matching Engine

A limit order book (LOB) and exchange matching engine written in modern
**C++20**. The project is a study in market microstructure and matching-engine
systems design, built as a portfolio piece for quantitative trading engineering.

## Project vision

The goal is to build, incrementally and from first principles, the core of an
exchange matching engine:

- A price-time-priority limit order book.
- A deterministic matching engine that produces trades and executions.
- Support for the common order lifecycle (submit, amend, cancel).
- Realistic, integer-based price and quantity representation (no floating-point
  money).
- Benchmarks characterising throughput and latency.

## Current status

Implemented so far (each feature is listed only once the code and tests exist):

- A CMake-based C++20 build with a [doctest](https://github.com/doctest/doctest)
  unit-test suite.
- **Order model**: strongly typed ids, sides, order types, fixed-point integer
  prices, quantities, and sequence numbers.
- **Price-time-priority order book**: separate bid/ask sides, correct price and
  FIFO ordering, best-quote/level/depth queries.
- **Matching engine**: deterministic limit-order matching at the maker's price,
  with full and partial fills.
- **Order cancellation** by id.
- **Market orders** (never rest; bounded by available liquidity).
- **Market-data event recording and deterministic replay**, with a portable
  text log format.
- **Benchmarks** for the book and engine (see below).

Not yet implemented: order amend/replace and further order types (IOC, FOK, ...).
See [docs/DESIGN.md](docs/DESIGN.md) for the full roadmap.

## Layout

```
include/lob/    Public headers (the library interface)
src/            Library sources and the executable
tests/          Unit tests
benchmarks/     Performance benchmarks (nanobench)
docs/           Design notes and roadmap
```

## Building

Requires a C++20 compiler and CMake 3.20+.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/benchmarks/lob_benchmarks   # optional: run the benchmark suite
```

## Performance

Indicative microbenchmark results from **one developer laptop** — these are for
regression tracking and rough relative cost, **not** a claim of low latency or
production-exchange throughput. Regenerate them locally before drawing any
conclusions; absolute numbers depend heavily on hardware, compiler, and build
flags. See [benchmarks/README.md](benchmarks/README.md) for the methodology and
what each workload includes.

Environment: 12th Gen Intel Core i7-1255U (10C/12T, base ~1.70 GHz), 15.6 GB RAM,
Windows 11 Pro (build 26200), GCC 16.1.0 (MSYS2 UCRT64), C++20, `-O3` Release,
nanobench v4.3.11 (median of ≥20 iterations/epoch).

| Benchmark | ns/op | op/s | err% |
|-----------|------:|-----:|-----:|
| limit insertion (N=1000) | 251.9 | 3.97 M | 2.8% |
| limit insertion (N=10000) | 208.0 | 4.81 M | 1.3% |
| best bid/ask lookup | 0.49 | 2.03 B | 0.3% |
| insert+cancel cycle (N=1000) | 300.9 | 3.32 M | 3.7% |
| insert+cancel cycle (N=10000) | 245.4 | 4.07 M | 5.6% |
| single-order matching (N=2000) | 271.6 | 3.68 M | 3.3% |
| multi-level matching (K=64) | 272.0 | 3.68 M | 0.9% |
| market-order execution (K=64) | 256.2 | 3.90 M | 3.2% |
| mixed order flow (N=20000) | 165.4 | 6.05 M | 3.1% |

The `insert+cancel cycle` rows include the insertion cost (you cannot cancel an
order you did not insert); subtract the matching `limit insertion` baseline to
isolate cancellation. `best bid/ask lookup` is a pure `const` read and is orders
of magnitude cheaper than the mutating operations.

## Roadmap

See [docs/DESIGN.md](docs/DESIGN.md) for the design notes and the planned
milestone roadmap.

## License

MIT — see [LICENSE](LICENSE).
