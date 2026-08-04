#include "lob/event_log.hpp"

#include <cstdint>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>

#include "lob/order.hpp"
#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/trade.hpp"
#include "lob/types.hpp"

namespace lob {

namespace {

constexpr const char* kMagic = "LOBLOG v1";

char side_char(Side side) { return side == Side::Buy ? 'B' : 'S'; }
char type_char(OrderType type) { return type == OrderType::Market ? 'M' : 'L'; }

Side parse_side(char c) {
    if (c == 'B') return Side::Buy;
    if (c == 'S') return Side::Sell;
    throw std::runtime_error("event log parse error: bad side");
}

OrderType parse_type(char c) {
    if (c == 'L') return OrderType::Limit;
    if (c == 'M') return OrderType::Market;
    throw std::runtime_error("event log parse error: bad order type");
}

std::uint64_t raw_id(OrderId id) { return static_cast<std::uint64_t>(id); }
std::uint64_t raw_seq(Sequence s) { return static_cast<std::uint64_t>(s); }

// Reads one whitespace-separated field of type T, throwing on failure so that a
// truncated or non-numeric line is reported rather than silently defaulted.
template <typename T>
T read_field(std::istringstream& line, const char* what) {
    T value{};
    if (!(line >> value)) {
        throw std::runtime_error(std::string("event log parse error: missing ") + what);
    }
    return value;
}

}  // namespace

void EventLog::serialize(std::ostream& out) const {
    out << kMagic << '\n';
    for (const MarketEvent& event : events_) {
        if (const auto* s = std::get_if<SubmissionEvent>(&event.payload)) {
            const Order& o = s->order;
            out << event.index << " SUBMIT " << raw_id(o.id()) << ' '
                << side_char(o.side()) << ' ' << type_char(o.type()) << ' '
                << o.price().ticks() << ' ' << o.quantity().value() << ' '
                << raw_seq(o.sequence()) << '\n';
        } else if (const auto* c = std::get_if<CancellationEvent>(&event.payload)) {
            out << event.index << " CANCEL " << raw_id(c->order_id) << '\n';
        } else if (const auto* e = std::get_if<ExecutionEvent>(&event.payload)) {
            const Trade& t = e->trade;
            out << event.index << " TRADE " << raw_id(t.maker_id) << ' '
                << raw_id(t.taker_id) << ' ' << t.price.ticks() << ' '
                << t.quantity.value() << ' ' << raw_seq(t.maker_sequence) << ' '
                << raw_seq(t.taker_sequence) << '\n';
        }
    }
}

std::string EventLog::to_string() const {
    std::ostringstream out;
    serialize(out);
    return out.str();
}

EventLog EventLog::deserialize(std::istream& in) {
    std::string header;
    if (!std::getline(in, header) || header != kMagic) {
        throw std::runtime_error("event log parse error: bad or missing header");
    }

    EventLog log;
    std::string raw;
    while (std::getline(in, raw)) {
        if (raw.empty() || raw[0] == '#') {
            continue;
        }
        std::istringstream line(raw);

        const auto index = read_field<std::uint64_t>(line, "index");
        const auto tag = read_field<std::string>(line, "tag");

        if (tag == "SUBMIT") {
            const auto id = read_field<std::uint64_t>(line, "id");
            const auto side = read_field<char>(line, "side");
            const auto type = read_field<char>(line, "type");
            const auto ticks = read_field<std::int64_t>(line, "price");
            const auto qty = read_field<std::uint64_t>(line, "quantity");
            const auto seq = read_field<std::uint64_t>(line, "sequence");
            // The Order constructor re-validates invariants; surface any
            // violation as a uniform parse error.
            try {
                Order order(OrderId{id}, parse_side(side), parse_type(type),
                            Price::from_ticks(ticks), Quantity{qty}, Sequence{seq});
                log.append(MarketEvent{index, SubmissionEvent{order}});
            } catch (const std::invalid_argument& ex) {
                throw std::runtime_error(
                    std::string("event log parse error: invalid order: ") + ex.what());
            }
        } else if (tag == "CANCEL") {
            const auto id = read_field<std::uint64_t>(line, "id");
            log.append(MarketEvent{index, CancellationEvent{OrderId{id}}});
        } else if (tag == "TRADE") {
            const auto maker = read_field<std::uint64_t>(line, "maker_id");
            const auto taker = read_field<std::uint64_t>(line, "taker_id");
            const auto ticks = read_field<std::int64_t>(line, "price");
            const auto qty = read_field<std::uint64_t>(line, "quantity");
            const auto mseq = read_field<std::uint64_t>(line, "maker_sequence");
            const auto tseq = read_field<std::uint64_t>(line, "taker_sequence");
            log.append(MarketEvent{index, ExecutionEvent{Trade{
                OrderId{maker}, OrderId{taker}, Price::from_ticks(ticks),
                Quantity{qty}, Sequence{mseq}, Sequence{tseq}}}});
        } else {
            throw std::runtime_error("event log parse error: unknown tag '" + tag + "'");
        }
    }
    return log;
}

EventLog EventLog::from_string(const std::string& text) {
    std::istringstream in(text);
    return deserialize(in);
}

}  // namespace lob
