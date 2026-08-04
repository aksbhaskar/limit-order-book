#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "lob/market_event.hpp"

namespace lob {

// An ordered, in-memory record of market events.
//
// The log is a plain sequence of MarketEvents in the order they occurred. It can
// be serialized to, and parsed from, a simple line-based text format so a
// session can be saved and replayed later. The format is self-contained: no
// database or network is involved, and the layer is deliberately decoupled from
// the engine so an external historical feed could produce a log the same way.
//
// Text format (one event per line, integer fields, prices in ticks):
//
//     LOBLOG v1
//     <index> SUBMIT <id> <B|S> <L|M> <price_ticks> <qty> <seq>
//     <index> CANCEL <id>
//     <index> TRADE  <maker_id> <taker_id> <price_ticks> <qty> <maker_seq> <taker_seq>
class EventLog {
public:
    using const_iterator = std::vector<MarketEvent>::const_iterator;

    void append(const MarketEvent& event) { events_.push_back(event); }

    bool empty() const noexcept { return events_.empty(); }
    std::size_t size() const noexcept { return events_.size(); }
    const MarketEvent& operator[](std::size_t i) const { return events_[i]; }

    const_iterator begin() const noexcept { return events_.begin(); }
    const_iterator end() const noexcept { return events_.end(); }

    const std::vector<MarketEvent>& events() const noexcept { return events_; }

    // Writes the log in the text format described above.
    void serialize(std::ostream& out) const;
    std::string to_string() const;

    // Parses a log from the text format. Throws std::runtime_error on malformed
    // input (bad header, unknown tag, missing/non-numeric fields, or an event
    // that violates the order invariants).
    static EventLog deserialize(std::istream& in);
    static EventLog from_string(const std::string& text);

    friend bool operator==(const EventLog&, const EventLog&) = default;

private:
    std::vector<MarketEvent> events_;
};

}  // namespace lob
