#include "lob/recording_engine.hpp"

#include <variant>

#include "lob/market_event.hpp"
#include "lob/trade.hpp"

namespace lob {

SubmitResult RecordingEngine::submit(const Order& order) {
    // Record the arrival first, then the fills it produces, so indices run in
    // chronological order.
    log_.append(MarketEvent{next_index_++, SubmissionEvent{order}});

    SubmitResult result = engine_.submit(order);

    for (const Trade& trade : result.trades) {
        log_.append(MarketEvent{next_index_++, ExecutionEvent{trade}});
    }
    return result;
}

CancelResult RecordingEngine::cancel(OrderId id) {
    CancelResult result = engine_.cancel(id);
    if (result.ok()) {
        log_.append(MarketEvent{next_index_++, CancellationEvent{id}});
    }
    return result;
}

RecordingEngine replay(const EventLog& log) {
    RecordingEngine engine;
    for (const MarketEvent& event : log) {
        if (const auto* s = std::get_if<SubmissionEvent>(&event.payload)) {
            engine.submit(s->order);
        } else if (const auto* c = std::get_if<CancellationEvent>(&event.payload)) {
            engine.cancel(c->order_id);
        }
        // ExecutionEvents are derived output; replay regenerates them.
    }
    return engine;
}

}  // namespace lob
