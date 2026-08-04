#pragma once

#include <cstdint>

#include "lob/event_log.hpp"
#include "lob/matching_engine.hpp"
#include "lob/order.hpp"

namespace lob {

// A thin recording wrapper around MatchingEngine.
//
// It forwards submit() and cancel() to a real engine — behaviour and results are
// unchanged — while appending the corresponding market events to an EventLog in
// deterministic order:
//
//   * submit(order)  records a Submission event, then one Execution event per
//                    resulting trade, in trade order.
//   * cancel(id)     records a Cancellation event only when the cancel actually
//                    removed a resting order (so the log reflects real state
//                    changes, not rejected requests).
//
// Every appended event gets the next value of a monotonic index, giving the log
// a total, deterministic order. The engine itself is untouched, so the recorder
// is fully optional and could be replaced by any other event source.
class RecordingEngine {
public:
    SubmitResult submit(const Order& order);
    CancelResult cancel(OrderId id);

    const MatchingEngine& engine() const noexcept { return engine_; }
    const OrderBook& book() const noexcept { return engine_.book(); }
    const EventLog& log() const noexcept { return log_; }

private:
    MatchingEngine engine_;
    EventLog log_;
    std::uint64_t next_index_ = 0;
};

// Reconstructs engine/book state by replaying an event log through a fresh
// RecordingEngine. Only the command events (submissions and cancellations) drive
// the replay; execution events are derived and are regenerated, not consumed.
//
// Replay is deterministic: the returned recorder's own log equals the input log
// (given a log produced by this system), and the reconstructed book matches the
// original. Unknown/rejected commands cannot occur in a well-formed log, so
// replay performs no matching beyond what the recorded commands imply.
RecordingEngine replay(const EventLog& log);

}  // namespace lob
