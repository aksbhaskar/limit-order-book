#pragma once

#include <cstdint>
#include <string_view>

namespace lob {

// The side of an order in the book.
enum class Side : std::uint8_t {
    Buy,
    Sell,
};

// The order's execution type.
//
//   * Limit  - executes only at its stated price or better, and rests on the
//              book if it cannot fully match.
//   * Market - executes against the best available liquidity regardless of
//              price and never rests; any unfilled quantity is simply dropped.
//
// The enum is extensible so later types (IOC, FOK, ...) can be added without
// touching call sites that already switch on the type.
enum class OrderType : std::uint8_t {
    Limit,
    Market,
};

// Strong identifier for an order. Modelled as a scoped enumeration so it carries
// identity and ordering but not accidental arithmetic (you cannot add two order
// ids). The value 0 is reserved to mean "no / invalid id".
enum class OrderId : std::uint64_t {};

// Monotonically increasing sequence number. It timestamps the arrival order of
// events and is the tie-breaker for price-time priority: for a given price, the
// order with the smaller sequence number has priority. Scoped enums support the
// relational comparisons needed for that ordering.
enum class Sequence : std::uint64_t {};

// Human-readable rendering, useful for logging and test diagnostics.
constexpr std::string_view to_string(Side side) noexcept {
    return side == Side::Buy ? "Buy" : "Sell";
}

constexpr std::string_view to_string(OrderType type) noexcept {
    switch (type) {
        case OrderType::Limit:
            return "Limit";
        case OrderType::Market:
            return "Market";
    }
    return "Unknown";
}

}  // namespace lob
