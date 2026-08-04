#pragma once

#include <cstdint>
#include <variant>

#include "lob/order.hpp"
#include "lob/trade.hpp"
#include "lob/types.hpp"

namespace lob {

// The three kinds of market-data event the engine emits. The tag is stable and
// is what the serialized log records.
enum class MarketEventType : std::uint8_t {
    Submission,     // an order arrived and was submitted for matching
    Cancellation,   // a resting order was cancelled
    Execution,      // two orders traded (a fill)
};

// An order arrived. Carries the order exactly as submitted, which is all replay
// needs to reproduce the resulting book state and executions.
struct SubmissionEvent {
    Order order;

    friend bool operator==(const SubmissionEvent&, const SubmissionEvent&) = default;
};

// A resting order was cancelled (only effective cancellations are recorded).
struct CancellationEvent {
    OrderId order_id{};

    friend bool operator==(const CancellationEvent&, const CancellationEvent&) = default;
};

// Two orders traded. This is a derived event: it is emitted for a faithful
// market-data record, but replay regenerates it rather than consuming it.
struct ExecutionEvent {
    Trade trade;

    friend bool operator==(const ExecutionEvent&, const ExecutionEvent&) = default;
};

using MarketEventPayload =
    std::variant<SubmissionEvent, CancellationEvent, ExecutionEvent>;

// A single market event: a monotonically increasing index (the deterministic
// ordering key) plus the typed payload.
struct MarketEvent {
    std::uint64_t index = 0;
    MarketEventPayload payload;

    MarketEventType type() const noexcept {
        return static_cast<MarketEventType>(payload.index());
    }

    friend bool operator==(const MarketEvent&, const MarketEvent&) = default;
};

}  // namespace lob
