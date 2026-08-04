#include "lob/book_snapshot.hpp"

#include "lob/order.hpp"
#include "lob/price_level.hpp"

namespace lob {

namespace {

std::vector<LevelSnapshot> snapshot_side(const OrderBook& book, Side side) {
    std::vector<LevelSnapshot> out;
    for (const PriceLevel* level : book.levels(side)) {
        LevelSnapshot ls;
        ls.price = level->price();
        ls.total_quantity = level->total_quantity();
        ls.orders.reserve(level->order_count());
        for (const Order& order : level->orders()) {
            ls.orders.push_back(OrderSnapshot{
                order.id(), order.remaining_quantity(), order.sequence()});
        }
        out.push_back(std::move(ls));
    }
    return out;
}

}  // namespace

BookSnapshot BookSnapshot::of(const OrderBook& book) {
    return BookSnapshot{snapshot_side(book, Side::Buy),
                        snapshot_side(book, Side::Sell)};
}

}  // namespace lob
