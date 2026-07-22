//
// Created by denni on 3/10/2026.
//
#ifndef LIMITORDERBOOK_IORDERBOOKSTORAGE_H
#define LIMITORDERBOOK_IORDERBOOKSTORAGE_H
#include "../Domain/order.h"
#include <vector>
class IOrderBook {
    /**
     * @class IOrderBook
     * @brief Pure virtual class defining the storage and retrieval contract for orders.
     */
public:
    /** @brief Virtual destructor to ensure proper cleanup of derived classes. */
    virtual ~IOrderBook()=default;


    /**
     * @brief Adds a new order to the book.
     * @param order The order object to be registered.
     */
    virtual void AddOrder(std::shared_ptr<Order> order) = 0;

    /**
     * @brief Removes an order from the book based on its ID.
     * @param order_id The unique identifier of the order to be removed.
     */
    virtual void CancelOrder(int order_id) = 0;

    /**
     * @brief Modifies the volume of an existing order.
     * @param order_id The unique identifier of the order.
     * @param new_quantity The updated quantity.
     */
    virtual void UpdateQuantity(int order_id,int new_quantity) = 0;

    /**
     * @brief Accesses the highest-priced Buy order.
     * @return Pointer to the best Bid, or nullptr if the side is empty.
     */
    virtual const std::shared_ptr<Order> GetBestBid()  = 0;

    /**
     * @brief Accesses the lowest-priced Ask order.
     * @return Pointer to the best Ask, or nullptr if the side is empty.
     */
    virtual const std::shared_ptr<Order> GetBestAsk()  = 0;

    virtual std::vector<std::shared_ptr<Order>> GetBestBids(int x)  = 0;
    virtual std::vector<std::shared_ptr<Order>> GetBestAsks(int x)  = 0;

    /**
     * @brief Checks for the presence of Buy orders.
     * @return true if the Bid side is empty.
     */
    virtual  bool IsBidEmpty() const = 0;

    /**
     * @brief Checks for the presence of Sell orders.
     * @return true if the Ask side is empty.
     */
    virtual bool IsAskEmpty() const = 0;
    virtual bool CanFillQuantityAsks(int Quantity, uint64_t Price)  = 0;
    virtual bool CanFillQuantityBids(int Quantity, uint64_t Price)  = 0;

    /**
     * @brief Removes the top-priority Buy order from the book.
     */
    virtual void PopBestBid() = 0;

    /**
     * @brief Removes the top-priority Buy order from the book.
     */
    virtual void PopBestAsk() = 0;
};
#endif //LIMITORDERBOOK_IORDERBOOKSTORAGE_H