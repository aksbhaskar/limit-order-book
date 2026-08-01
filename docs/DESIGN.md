# Design notes & roadmap

This document records the design intent and the planned milestone roadmap. It is
aspirational: sections describe where the project is heading, and each milestone
is only marked complete once the corresponding code and tests land.

## Design principles

- **Correct money representation.** Prices are stored as fixed-point integers
  (ticks), never as floating-point values, to avoid rounding error in the
  monetary path.
- **Strong typing.** Distinct concepts (order ids, sides, order types, prices,
  quantities, sequence numbers) use dedicated types rather than bare integers or
  strings, so that the compiler catches category errors.
- **Determinism.** Given the same ordered input, the engine must produce exactly
  the same trades. A monotonic sequence number establishes time priority.
- **Testability first.** Every layer ships with unit tests before the next layer
  is built on top of it.

## Roadmap

| Milestone | Description                                             | Status      |
|-----------|---------------------------------------------------------|-------------|
| 1         | Project initialisation (build, tests, layout)           | Done        |
| 2         | Core order data structures and supporting value types   | Done        |
| 3         | Price-time-priority limit order book                    | Planned     |
| 4         | Matching engine producing trades / executions           | Planned     |
| 5         | Order lifecycle: amend and cancel                       | Planned     |
| 6         | Additional order types (market, IOC, FOK, ...)          | Planned     |
| 7         | Benchmarks: throughput and latency characterisation     | Planned     |

Only milestones 1 and 2 are implemented at present. Everything from milestone 3
onward is a plan, not a promise of existing functionality.
