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
    if (ids_.contains(order.id())) {
        throw std::invalid_argument("duplicate order id in book");
    }

    if (order.side() == Side::Buy) {
        auto [it, inserted] = bids_.try_emplace(order.price(), order.price());
        it->second.add(order);
    } else {
        auto [it, inserted] = asks_.try_emplace(order.price(), order.price());
        it->second.add(order);
    }
    ids_.insert(order.id());
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
            ids_.erase(result.completed_id);
        }
        if (it->second.empty()) {
            bids_.erase(it);
        }
    } else {
        auto it = asks_.begin();
        const auto result = it->second.reduce_front(qty);
        if (result.order_completed) {
            ids_.erase(result.completed_id);
        }
        if (it->second.empty()) {
            asks_.erase(it);
        }
    }
}

}  // namespace lob
