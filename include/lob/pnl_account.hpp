#pragma once

#include <cstdint>

#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/types.hpp"

namespace lob {

// Average-cost profit-and-loss accounting for a single instrument.
//
// All money is kept in integer price *ticks* (the same fixed-point scale as
// Price), so the monetary path never touches floating point. A signed position
// is tracked together with the average cost of the currently open position.
//
// Accounting conventions:
//   * A buy fill increases the position and decreases cash by price * quantity;
//     a sell fill does the reverse.
//   * Realized P&L is booked (via average cost) whenever a fill reduces or
//     closes the open position.
//   * Total P&L, marked at some price m, is the exact cash-based figure
//         cash + position * m - starting_cash,
//     and unrealized P&L is defined as total - realized, so the identity
//         realized + unrealized == total
//     always holds regardless of integer rounding in the average cost.
//   * A transaction fee is a realized cost: it reduces both cash and realized.
class PnLAccount {
public:
    explicit PnLAccount(std::int64_t starting_cash_ticks = 0) noexcept
        : start_(starting_cash_ticks), cash_(starting_cash_ticks) {}

    // Applies a fill from the account owner's perspective: Side::Buy adds to the
    // position, Side::Sell reduces it.
    void apply_fill(Side side, Price price, Quantity quantity) noexcept;

    // Charges a transaction fee (in ticks): a realized cost.
    void charge_fee(std::int64_t fee_ticks) noexcept {
        cash_ -= fee_ticks;
        realized_ -= fee_ticks;
    }

    std::int64_t position() const noexcept { return position_; }
    std::int64_t starting_cash_ticks() const noexcept { return start_; }
    std::int64_t cash_ticks() const noexcept { return cash_; }
    std::int64_t average_cost_ticks() const noexcept { return average_cost_; }
    std::int64_t realized_ticks() const noexcept { return realized_; }

    // Mark-to-market equity and P&L at mark price `mark`.
    std::int64_t equity_ticks(Price mark) const noexcept {
        return cash_ + position_ * mark.ticks();
    }
    std::int64_t total_pnl_ticks(Price mark) const noexcept {
        return equity_ticks(mark) - start_;
    }
    std::int64_t unrealized_ticks(Price mark) const noexcept {
        return total_pnl_ticks(mark) - realized_;
    }

private:
    std::int64_t start_ = 0;
    std::int64_t cash_ = 0;
    std::int64_t position_ = 0;       // signed units
    std::int64_t average_cost_ = 0;   // ticks; meaningful when position_ != 0
    std::int64_t realized_ = 0;       // ticks
};

}  // namespace lob
