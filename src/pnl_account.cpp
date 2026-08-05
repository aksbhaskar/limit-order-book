#include "lob/pnl_account.hpp"

#include <cstdlib>

namespace lob {

void PnLAccount::apply_fill(Side side, Price price, Quantity quantity) noexcept {
    const std::int64_t p = price.ticks();
    const std::int64_t q = static_cast<std::int64_t>(quantity.value());
    const std::int64_t dq = (side == Side::Buy) ? q : -q;

    // Cash leg: buying spends cash, selling receives it.
    cash_ -= dq * p;

    if (position_ == 0) {
        position_ = dq;
        average_cost_ = p;
        return;
    }

    const bool increasing = (position_ > 0) == (dq > 0);
    if (increasing) {
        const std::int64_t old_abs = std::llabs(position_);
        const std::int64_t add_abs = std::llabs(dq);
        average_cost_ =
            (average_cost_ * old_abs + p * add_abs) / (old_abs + add_abs);
        position_ += dq;
        return;
    }

    // Opposite direction: this fill reduces, closes, or flips the position.
    const std::int64_t closed = std::min(std::llabs(position_), std::llabs(dq));
    const std::int64_t sign = (position_ > 0) ? 1 : -1;
    realized_ += sign * (p - average_cost_) * closed;

    position_ += dq;
    if (position_ == 0) {
        average_cost_ = 0;
    } else if ((position_ > 0) != (sign > 0)) {
        // Flipped through zero: the surviving quantity is opened at this price.
        average_cost_ = p;
    }
    // Otherwise a partial close leaves the average cost unchanged.
}

}  // namespace lob
