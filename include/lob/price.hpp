#pragma once

#include <cstdint>
#include <compare>

namespace lob {

// A monetary price represented as a fixed-point integer.
//
// Prices are stored as a signed 64-bit count of "ticks", where one whole
// currency unit equals `kTicksPerUnit` ticks (four decimal places). Storing
// money as an integer avoids the rounding and comparison hazards of binary
// floating point: two prices that should be equal always compare equal, and
// there is no representational drift when prices are added or subtracted.
//
// Construction is deliberately explicit and integer-based. There is no
// constructor from `double` so that no floating-point value can silently enter
// the monetary path.
class Price {
public:
    using rep = std::int64_t;

    // Number of ticks in one whole currency unit (i.e. 4 decimal places).
    static constexpr rep kTicksPerUnit = 10'000;

    constexpr Price() noexcept = default;

    // Construct directly from a raw tick count (e.g. 101'2500 == 101.2500).
    static constexpr Price from_ticks(rep ticks) noexcept { return Price(ticks); }

    // Construct from a whole number of currency units (e.g. 101 == 101.0000).
    static constexpr Price from_units(rep whole) noexcept {
        return Price(whole * kTicksPerUnit);
    }

    // Construct from whole units plus a fractional tick remainder. `fraction`
    // is expressed in ticks within a single unit, i.e. in [0, kTicksPerUnit).
    static constexpr Price from_units(rep whole, rep fraction) noexcept {
        return Price(whole * kTicksPerUnit + fraction);
    }

    // The raw tick count backing this price.
    constexpr rep ticks() const noexcept { return ticks_; }

    // Lossy conversion to floating point, provided only for display/reporting.
    // Never use the result to make monetary decisions.
    constexpr double to_double() const noexcept {
        return static_cast<double>(ticks_) / static_cast<double>(kTicksPerUnit);
    }

    constexpr bool is_positive() const noexcept { return ticks_ > 0; }

    // Total ordering and equality follow directly from the tick count.
    friend constexpr auto operator<=>(const Price&, const Price&) noexcept = default;

    friend constexpr Price operator+(Price lhs, Price rhs) noexcept {
        return Price(lhs.ticks_ + rhs.ticks_);
    }
    friend constexpr Price operator-(Price lhs, Price rhs) noexcept {
        return Price(lhs.ticks_ - rhs.ticks_);
    }

private:
    explicit constexpr Price(rep ticks) noexcept : ticks_(ticks) {}

    rep ticks_ = 0;
};

}  // namespace lob
