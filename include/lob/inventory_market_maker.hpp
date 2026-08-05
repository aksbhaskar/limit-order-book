#pragma once

#include <cstdint>
#include <string>

#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/strategy.hpp"

namespace lob {

// Configuration for the inventory-aware market maker.
struct InventoryAwareConfig {
    Price base_spread = Price::from_units(0, 100);   // full spread at zero inventory
    std::int64_t max_inventory = 100;
    Quantity order_quantity{10};
    std::int64_t skew_ticks_per_unit = 2;    // shift quotes down/up per unit of inventory
    std::int64_t widen_ticks_per_unit = 1;   // widen half-spread per unit of |inventory|
};

// A market maker that leans against its inventory.
//
// It quotes around a *reservation price* shifted away from the mid in proportion
// to inventory (`reservation = mid − skew × inventory`), so a long book lowers
// both quotes to encourage selling and discourage buying, and a short book does
// the reverse. It also widens the half-spread in proportion to |inventory|, so
// quotes become less aggressive as risk grows. The same symmetric inventory cap
// as the fixed-spread maker is enforced on sizes. This is a deliberately simple
// linear rule, not a sophisticated optimal-quoting model.
class InventoryAwareMarketMaker : public Strategy {
public:
    explicit InventoryAwareMarketMaker(InventoryAwareConfig config) : config_(config) {}

    QuoteDecision quote(const MarketState& state) override;
    std::string name() const override { return "inventory-aware"; }

    const InventoryAwareConfig& config() const noexcept { return config_; }

private:
    InventoryAwareConfig config_;
};

}  // namespace lob
