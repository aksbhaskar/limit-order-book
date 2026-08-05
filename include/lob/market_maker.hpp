#pragma once

#include <cstdint>
#include <string>

#include "lob/price.hpp"
#include "lob/quantity.hpp"
#include "lob/strategy.hpp"

namespace lob {

// Configuration for the fixed-spread market maker.
struct MarketMakerConfig {
    Price spread = Price::from_units(0, 100);   // full quoted spread (default 0.0100)
    std::int64_t max_inventory = 100;           // symmetric inventory cap
    Quantity order_quantity{10};                // desired size per quote
};

// A basic market maker that quotes a fixed spread symmetrically around the mid.
//
// Each step it places a bid at mid - spread/2 and an ask at mid + spread/2. It
// never quotes size that could push its inventory past +/- max_inventory: the
// quantity on each side is clamped to the remaining room, and a side is dropped
// entirely when there is no room. It applies no inventory skew — that is the
// job of the inventory-aware variant.
class FixedSpreadMarketMaker : public Strategy {
public:
    explicit FixedSpreadMarketMaker(MarketMakerConfig config) : config_(config) {}

    QuoteDecision quote(const MarketState& state) override;
    std::string name() const override { return "fixed-spread"; }

    const MarketMakerConfig& config() const noexcept { return config_; }

private:
    MarketMakerConfig config_;
};

}  // namespace lob
