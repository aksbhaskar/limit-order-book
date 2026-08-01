#pragma once

#include <cstdint>
#include <compare>

namespace lob {

// A strongly-typed order quantity (number of units / lots / shares).
//
// Quantity is a whole, non-negative count, so it is backed by an unsigned 64-bit
// integer. Wrapping it in a dedicated type keeps it from being confused with
// prices, ids, or plain integers, while still exposing the small amount of
// arithmetic the order model needs (comparing sizes and reducing a remaining
// amount as it fills).
class Quantity {
public:
    using rep = std::uint64_t;

    constexpr Quantity() noexcept = default;
    explicit constexpr Quantity(rep value) noexcept : value_(value) {}

    constexpr rep value() const noexcept { return value_; }
    constexpr bool is_zero() const noexcept { return value_ == 0; }

    friend constexpr auto operator<=>(const Quantity&, const Quantity&) noexcept = default;

    friend constexpr Quantity operator+(Quantity lhs, Quantity rhs) noexcept {
        return Quantity(lhs.value_ + rhs.value_);
    }
    friend constexpr Quantity operator-(Quantity lhs, Quantity rhs) noexcept {
        return Quantity(lhs.value_ - rhs.value_);
    }

private:
    rep value_ = 0;
};

}  // namespace lob
