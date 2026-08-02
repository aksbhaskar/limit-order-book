#pragma once

#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/types.hpp"

namespace lob {

// A single execution produced when an incoming (taker) order matches a resting
// (maker) order.
//
// By convention the trade executes at the *maker's* (resting order's) price,
// which is the price that was already displayed in the book. Both order ids and
// both sequence numbers are recorded so an execution can be attributed to the
// exact orders involved and ordered deterministically.
struct Trade {
    OrderId maker_id{};          // resting order that was hit
    OrderId taker_id{};          // incoming order that crossed
    Price price{};               // execution price (== maker's resting price)
    Quantity quantity{};         // quantity executed in this fill
    Sequence maker_sequence{};   // resting order's arrival sequence
    Sequence taker_sequence{};   // incoming order's arrival sequence

    friend bool operator==(const Trade&, const Trade&) = default;
};

}  // namespace lob
