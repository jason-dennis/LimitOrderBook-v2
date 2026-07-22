//
// Created by denni on 3/10/2026.
//
#include "../../include/Storage/MultisetOrderBookStorage.h"

void MultisetOrderBook::AddOrder(uint32_t index) {
    /**
     * @brief Adds a new order to the storage.
     * Inserts the order into the appropriate side (Bid/Ask) and indexes its location.
     * @param order The order object to be added.
     * @complexity O(log N) for insertion, O(1) for indexing.
    */
    auto& order = Pool_.Get(index);
    if (order.Side==OrderSide::BUY) {
        auto it = bids.insert(index);
        bidLocation[order.OrderId]=it;
    }
    else {
        auto it=asks.insert(index);
        askLocation[order.OrderId]=it;
    }
}

void MultisetOrderBook::CancelOrder(int order_id) {
    /**
     * @brief Cancel an order from the storage by its id.
     * Update the order status to CANCELED and removes it from all internal structures
     * @param order_id  Unique identifier of the order to cancel.
     * @complexity O(log N).
     */

    auto bidIt = bidLocation.find(order_id);
    if (bidIt != bidLocation.end()) {
        auto index = (*bidIt->second);
        auto& order = Pool_.Get(index);
        order.Status = (OrderStatus::CANCELED);
        bids.erase(bidIt->second);
        bidLocation.erase(order_id);
        Pool_.Free(index);
        return;
    }

    auto askIt = askLocation.find(order_id);
    if (askIt != askLocation.end()) {
        auto index = (*askIt->second);
        auto& order = Pool_.Get(index);
        order.Status = (OrderStatus::CANCELED);
        asks.erase(askIt->second);
        askLocation.erase(order_id);
        Pool_.Free(index);
    }
}

void MultisetOrderBook::UpdateQuantity(int order_id, int new_quantity) {
    /**
     * @brief Updates the quantity and status of an existing order->
     * If the new quantity is 0, the order is marked as FILLED and removed from storage.
     * Otherwise, the status is updated to PARTIALLY_FILLED.
     * @param order_id Unique identifier of the order to be updated.
     * @param new_quantity The new volume for the order->
     * @complexity O(1) to find the order in the map and O(log N) if removal from the multiset is required.
     */
    auto bidIt = bidLocation.find(order_id);
    if (bidIt != bidLocation.end()) {
        auto index = (*bidIt->second);
        auto& order = Pool_.Get(index);
        order.Quantity = new_quantity;
        if (new_quantity>0) {
            order.Status = (OrderStatus::PARTIALLY_FILLED);
        }
        else {
            order.Status = (OrderStatus::FILLED);
            bids.erase(bidIt->second);
            bidLocation.erase(order_id);
            Pool_.Free(index);
        }
        return;
    }

    auto askIt = askLocation.find(order_id);
    if (askIt != askLocation.end()) {
        auto index = (*askIt->second);
        auto& order = Pool_.Get(index);
        order.Quantity = (new_quantity);
        if (new_quantity>0) {
            order.Status = (OrderStatus::PARTIALLY_FILLED);
        }
        else {
            order.Status = (OrderStatus::FILLED);
            asks.erase(askIt->second);
            askLocation.erase(order_id);
            Pool_.Free(index);
        }
    }

}

const uint32_t MultisetOrderBook::GetBestBid()  {
    /**
     * @brief Returns the best (highest price) Bid order->
     * @return Const pointer to the top Bid, or nullptr if empty.
     */
    if (bids.empty()) {
        return UINT32_MAX;
    }
    return (*bids.begin());
}

const uint32_t MultisetOrderBook::GetBestAsk()  {
    /**
     * @brief Returns the best (lowest price) Ask order->
     * @return Const pointer to the top Ask, or nullptr if empty.
     */
    if (asks.empty()) {
        return UINT32_MAX;
    }
    return (*asks.begin());
}

std::vector<uint32_t> MultisetOrderBook::GetBestBids(uint32_t x)  {
    std::vector<uint32_t> result;
    auto it = bids.begin();
    for (uint32_t i = 0; i < x && it != bids.end(); ++i, ++it) {
        result.push_back(*it);
    }
    return result;
}

std::vector<uint32_t> MultisetOrderBook::GetBestAsks(uint32_t x)  {
    std::vector<uint32_t> result;
    auto it = asks.begin();
    for (uint32_t i = 0; i < x && it != asks.end(); ++i, ++it) {
        result.push_back(*it);
    }
    return result;
}

bool MultisetOrderBook::IsBidEmpty() const {
    /**
     * @brief Checks if there are any Buy orders in the book.
     */
    return bids.empty();
}

bool MultisetOrderBook::IsAskEmpty() const {
    /**
     * @brief Checks if there are any Sell orders in the book.
     */
    return asks.empty();
}

bool MultisetOrderBook::CanFillQuantityAsks(uint32_t Quantity, uint64_t Price)  {

    for (auto index : asks) {
        auto& order = Pool_.Get(index);
        if (order.Price > Price) {
            break;
        }
        Quantity-=std::min(Quantity,order.Quantity);
        if (Quantity == 0) {
            return true;
        }
    }

    return false;
}

bool MultisetOrderBook::CanFillQuantityBids(uint32_t Quantity, uint64_t Price)  {

    for (auto index : bids) {
        auto& order = Pool_.Get(index);
        if (order.Price < Price) {
            break;
        }
        Quantity-=std::min(Quantity,order.Quantity);
        if (Quantity == 0) {
            return true;
        }
    }

    return false;
}

void MultisetOrderBook::PopBestBid() {
    /**
     * @brief Removes the top Bid order from the book.
     * Usually called after a full match. Sets status to FILLED before removal.
     */
    if (bids.empty()) {
        return;
    }
    auto it=bids.begin();
    auto index = *it;
    auto& order = Pool_.Get(index);
    order.Status = (OrderStatus::FILLED);
    int order_id=order.OrderId;
    bids.erase(it);
    bidLocation.erase(order_id);
    Pool_.Free(index);
}

void MultisetOrderBook::PopBestAsk() {
    /**
     * @brief Removes the top Ask order from the book.
     * Usually called after a full match. Sets status to FILLED before removal.
     */
    if (asks.empty()){
        return;
    }
    auto it=asks.begin();
    auto index = *it;
    auto& order = Pool_.Get(index);
    order.Status = (OrderStatus::FILLED);
    int order_id = order.OrderId;
    asks.erase(it);
    askLocation.erase(order_id);
    Pool_.Free(index);
}
