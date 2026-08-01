# Limit Order Book & Exchange Matching Engine

A high-performance limit order book (LOB) and exchange matching engine written in
modern **C++20**. The project is a study in market microstructure and low-latency
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

This repository is in its **early scaffolding stage**. What exists today:

- A CMake-based C++20 build.
- A minimal executable that builds and runs.
- A unit-testing setup ([doctest](https://github.com/doctest/doctest)) with
  passing tests.

Matching, the order book itself, and later engine functionality are **not yet
implemented**. Features are only listed here once the code behind them exists.

## Layout

```
include/lob/    Public headers (the library interface)
src/            Library and executable sources
tests/          Unit tests
benchmarks/     Performance benchmarks (planned)
docs/           Design notes and roadmap
```

## Building

Requires a C++20 compiler and CMake 3.20+.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Roadmap

See [docs/DESIGN.md](docs/DESIGN.md) for the design notes and the planned
milestone roadmap.

## License

MIT — see [LICENSE](LICENSE).
