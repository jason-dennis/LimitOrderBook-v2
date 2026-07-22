//
// Created by denni on 3/17/2026.
//

#ifndef LIMITORDERBOOK_COREENGINE_H
#define LIMITORDERBOOK_COREENGINE_H
#include "Domain/order.h"
#include "Routing/OrderRouter.h"
#include "Domain/Time.h"
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <shared_mutex>


/**
 * @class CoreEngine
 * @brief High-level trading engine that coordinates order management and market data.
 * Owns the AppEngine for matching/trading and maintains all orders per symbol.
 * Provides the interface between the UI layer and the trading logic.
 */
class CoreEngine {
private:
    OrderRouter Router_;

    /**
     * @brief Generates a unique order ID.
     * @return The next available order ID.
     */
    int GenerateID();

public:
    CoreEngine();
    ~CoreEngine() = default;

    /**
     * @brief Creates and submits a new order.
     * @param Price Order price as float (will be multiplied by Tick).
     * @param Quantity Number of units.
     * @param Type Order type ("MARKET" or "LIMIT").
     * @param Symbol Ticker symbol (e.g. "AAPL").
     * @param TIF Time in force ("GTC", "IOC").
     * @param TraderID ID of the trader placing the order.
     * @param Side Order side ("BUY" or "SELL").
     */
    void CreateOrder(float Price, int Quantity, const std::string& Type, const std::string& Symbol,
                    const std::string& TIF, int TraderID, const std::string& Side);

    /**
     * @brief Cancels an existing order.
     * @param order_id The ID of the order to cancel.
     * @param Symbol The ticker symbol.
     */
    void CancelOrder(int order_id, const std::string& Symbol);

    /**
     * @brief Retrieves trade history for a symbol.
     * @param Symbol The ticker symbol.
     * @return Vector of completed trades.
     */
    std::vector<Trade> GetTradesHistory(const std::string& Symbol);

    /**
     * @brief Retrieves all orders for a symbol.
     * @param Symbol The ticker symbol.
     * @return Vector of all orders (any status).
     */
    std::vector<Order> GetOrders(const std::string& Symbol);

    /**
     * @brief Retrieves the best bid orders for a symbol.
     * @param x Number of price levels.
     * @param Symbol The ticker symbol.
     * @return Vector of bid orders sorted by price descending.
     */
    std::vector<Order> GetBestBids(int x, std::string& Symbol);

    /**
     * @brief Retrieves the best ask orders for a symbol.
     * @param x Number of price levels.
     * @param Symbol The ticker symbol.
     * @return Vector of ask orders sorted by price ascending.
     */
    std::vector<Order> GetBestAsks(int x, std::string& Symbol);

    /**
     * @brief Loads orders from orders.csv.
     */
    void LoadFromFile();

    /**
     * @brief Saves orders to orders.csv.
     */
    void SaveToFile();

    /**
     * @brief Saves both orders and trades to CSV files.
     */
    void Save();
    void Shutdown(){Router_.ShutDown();}
    Order GetOrder(const std::string &Symbol, const OrderPod& item, std::chrono::system_clock::time_point &timePoint) const;
};
#endif //LIMITORDERBOOK_COREENGINE_H