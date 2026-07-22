//
// Created by denni on 3/10/2026.
//

#ifndef LIMITORDERBOOK_MULTISETORDERBOOKSTORAGE_H
#define LIMITORDERBOOK_MULTISETORDERBOOKSTORAGE_H
#include "../Storage/IOrderBookStorage.h"
#include <set>
#include<unordered_map>
#include "Memory/OrderPool.h"

struct BidOrder {
    /**
     * @struct BidOrder
     * @brief Comparator for the Buy side of the order book.
     * Orders are sorted by Price (Descending) and then by Time (Ascending).
     */
    OrderPool& Pool_;
    bool operator()(uint32_t lhs, uint32_t rhs) const {
        auto& lhsOrder = Pool_.Get(lhs);
        auto& rhsOrder = Pool_.Get(rhs);
        if (lhsOrder.Price == rhsOrder.Price) {
            return lhsOrder.Timestamp < rhsOrder.Timestamp;
        }
        return lhsOrder.Price > rhsOrder.Price;
    }
};

struct AskOrder {
    /**
     * @struct AskOrder
     * @brief Comparator for the Sell side of the order book.
     * Orders are sorted by Price (Ascending) and then by Time (Ascending).
     */
    OrderPool& Pool_;
    bool operator()(uint32_t lhs, uint32_t rhs) const {
        auto& lhsOrder = Pool_.Get(lhs);
        auto& rhsOrder = Pool_.Get(rhs);
        if (lhsOrder.Price == rhsOrder.Price) {
            return lhsOrder.Timestamp < rhsOrder.Timestamp;
        }
        return lhsOrder.Price < rhsOrder.Price;
    }
};


class MultisetOrderBook{
    /**
     * @class MultisetOrderBook
     * @brief A high-performance order book storage implementation.
     * Uses std::multiset to keep orders sorted by price-time priority and
     * std::unordered_map to store iterators for O(1) access by Order ID.
     */
private:
    OrderPool& Pool_;

    /// Sorted container for Buy orders
    std::multiset<uint32_t,BidOrder>bids{BidOrder{Pool_}};
    /// Sorted container for Sell orders
    std::multiset<uint32_t ,AskOrder>asks{AskOrder{Pool_}};

    /// Index for quick access to Bid iterators by ID
    std::unordered_map<int,std::multiset<uint32_t ,BidOrder>::iterator>bidLocation;
    /// Index for quick access to Ask iterators by ID
    std::unordered_map<int,std::multiset<std::uint32_t ,AskOrder>::iterator>askLocation;
public:

    MultisetOrderBook(OrderPool& Pool):Pool_(Pool){}

    ~MultisetOrderBook()  = default;
    /**
         * @brief Adds a new order to the book.
         * @param order The order object to be registered.
         */
    void AddOrder(uint32_t index);

    /**
     * @brief Removes an order from the book based on its ID.
     * @param order_id The unique identifier of the order to be removed.
     */
    void CancelOrder(int order_id) ;

    /**
     * @brief Modifies the volume of an existing order.
     * @param order_id The unique identifier of the order.
     * @param new_quantity The updated quantity.
     */
    void UpdateQuantity(int order_id,int new_quantity);

    /**
 * @brief Accesses the highest-priced Buy order.
 * @return Pointer to the best Bid, or nullptr if the side is empty.
 */
    const uint32_t GetBestBid();

    /**
     * @brief Accesses the lowest-priced Ask order.
     * @return Pointer to the best Ask, or nullptr if the side is empty.
     */
    const uint32_t GetBestAsk();


    std::vector<uint32_t> GetBestBids(uint32_t x);
    std::vector<uint32_t> GetBestAsks(uint32_t x);

    /**
     * @brief Checks for the presence of Buy orders.
     * @return true if the Bid side is empty.
     */
    bool IsBidEmpty() const ;

    /**
     * @brief Checks for the presence of Sell orders.
     * @return true if the Ask side is empty.
     */
    bool IsAskEmpty() const;
    bool CanFillQuantityAsks(uint32_t Quantity, uint64_t Price);
    bool CanFillQuantityBids(uint32_t Quantity, uint64_t Price) ;

    /**
     * @brief Removes the top-priority Buy order from the book.
     */
    void PopBestBid();

    /**
    * @brief Removes the top-priority Buy order from the book.
    */
    void PopBestAsk();


};
#endif //LIMITORDERBOOK_MULTISETORDERBOOKSTORAGE_H