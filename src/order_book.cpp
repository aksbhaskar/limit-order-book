#include "lob/order_book.hpp"

#include <algorithm>
#include <stdexcept>

namespace lob {

namespace {

// Collects up to max_levels depth snapshots from a side map, best price first.
template <typename Map>
std::vector<LevelView> collect_depth(const Map& levels, std::size_t max_levels) {
    std::vector<LevelView> view;
    view.reserve(std::min(max_levels, levels.size()));
    for (const auto& [price, level] : levels) {
        if (view.size() >= max_levels) {
            break;
        }
        view.push_back(LevelView{price, level.total_quantity(), level.order_count()});
    }
    return view;
}

}  // namespace

OrderBook::OrderBook(const OrderBook& other)
    : bids_(other.bids_), asks_(other.asks_), filled_(other.filled_) {
    live_.reserve(other.live_.size());
    for (auto level = bids_.begin(); level != bids_.end(); ++level) {
        for (auto order = level->second.orders_.begin();
             order != level->second.orders_.end(); ++order) {
            live_.emplace(order->id(), Locator{Side::Buy, order, level, {}});
        }
    }
    for (auto level = asks_.begin(); level != asks_.end(); ++level) {
        for (auto order = level->second.orders_.begin();
             order != level->second.orders_.end(); ++order) {
            live_.emplace(order->id(), Locator{Side::Sell, order, {}, level});
        }
    }
}

OrderBook& OrderBook::operator=(const OrderBook& other) {
    if (this != &other) {
        *this = OrderBook(other);
    }
    return *this;
}

void OrderBook::add_order(const Order& order) {
    if (order.type() != OrderType::Limit) {
        throw std::invalid_argument("order book only accepts limit orders");
    }
    if (live_.contains(order.id())) {
        throw std::invalid_argument("duplicate order id in book");
    }

    if (order.side() == Side::Buy) {
        auto [it, inserted] = bids_.try_emplace(order.price(), order.price());
        const auto handle = it->second.append(order);
        live_.emplace(order.id(), Locator{Side::Buy, handle, it, {}});
    } else {
        auto [it, inserted] = asks_.try_emplace(order.price(), order.price());
        const auto handle = it->second.append(order);
        live_.emplace(order.id(), Locator{Side::Sell, handle, {}, it});
    }
}

CancelResult OrderBook::cancel(OrderId id) {
    const auto it = live_.find(id);
    if (it == live_.end()) {
        const CancelStatus status =
            filled_.contains(id) ? CancelStatus::AlreadyFilled : CancelStatus::NotFound;
        return CancelResult{id, status, Quantity{}};
    }

    const Locator loc = it->second;
    if (loc.side == Side::Buy) {
        const auto level_it = loc.bid_level;
        const Quantity removed = level_it->second.remove(loc.order);
        if (level_it->second.empty()) {
            bids_.erase(level_it);
        }
        live_.erase(it);
        return CancelResult{id, CancelStatus::Cancelled, removed};
    }

    const auto level_it = loc.ask_level;
    const Quantity removed = level_it->second.remove(loc.order);
    if (level_it->second.empty()) {
        asks_.erase(level_it);
    }
    live_.erase(it);
    return CancelResult{id, CancelStatus::Cancelled, removed};
}

const PriceLevel* OrderBook::best_bid() const noexcept {
    return bids_.empty() ? nullptr : &bids_.begin()->second;
}

const PriceLevel* OrderBook::best_ask() const noexcept {
    return asks_.empty() ? nullptr : &asks_.begin()->second;
}

const PriceLevel* OrderBook::level_at(Side side, Price price) const {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        return it == bids_.end() ? nullptr : &it->second;
    }
    auto it = asks_.find(price);
    return it == asks_.end() ? nullptr : &it->second;
}

std::size_t OrderBook::level_count(Side side) const noexcept {
    return side == Side::Buy ? bids_.size() : asks_.size();
}

std::vector<const PriceLevel*> OrderBook::levels(Side side) const {
    std::vector<const PriceLevel*> out;
    if (side == Side::Buy) {
        out.reserve(bids_.size());
        for (const auto& [price, level] : bids_) {
            out.push_back(&level);
        }
    } else {
        out.reserve(asks_.size());
        for (const auto& [price, level] : asks_) {
            out.push_back(&level);
        }
    }
    return out;
}

std::vector<LevelView> OrderBook::depth(Side side, std::size_t max_levels) const {
    return side == Side::Buy ? collect_depth(bids_, max_levels)
                             : collect_depth(asks_, max_levels);
}

void OrderBook::reduce_best(Side side, Quantity qty) {
    if (side == Side::Buy) {
        auto it = bids_.begin();
        const auto result = it->second.reduce_front(qty);
        if (result.order_completed) {
            live_.erase(result.completed_id);
            filled_.insert(result.completed_id);
        }
        if (it->second.empty()) {
            bids_.erase(it);
        }
    } else {
        auto it = asks_.begin();
        const auto result = it->second.reduce_front(qty);
        if (result.order_completed) {
            live_.erase(result.completed_id);
            filled_.insert(result.completed_id);
        }
        if (it->second.empty()) {
            asks_.erase(it);
        }
    }
}

}  // namespace lob
