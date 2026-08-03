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

void OrderBook::add_order(const Order& order) {
    if (order.type() != OrderType::Limit) {
        throw std::invalid_argument("order book only accepts limit orders");
    }
    if (live_.contains(order.id())) {
        throw std::invalid_argument("duplicate order id in book");
    }

    if (order.side() == Side::Buy) {
        auto [it, inserted] = bids_.try_emplace(order.price(), order.price());
        it->second.add(order);
    } else {
        auto [it, inserted] = asks_.try_emplace(order.price(), order.price());
        it->second.add(order);
    }
    live_.emplace(order.id(), Locator{order.side(), order.price()});
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
        auto level_it = bids_.find(loc.price);
        const Quantity removed = level_it->second.remove(id);
        if (level_it->second.empty()) {
            bids_.erase(level_it);
        }
        live_.erase(it);
        return CancelResult{id, CancelStatus::Cancelled, removed};
    }

    auto level_it = asks_.find(loc.price);
    const Quantity removed = level_it->second.remove(id);
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
