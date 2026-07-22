//
// Created by denni on 3/11/2026.
//

#ifndef LIMITORDERBOOK_MATCHINGENGINE_H
#define LIMITORDERBOOK_MATCHINGENGINE_H
#include "../../include/Domain/trade.h"
#include "Domain/TradePod.h"
#include "Memory/OrderPool.h"
#include "Domain/Time.h"

#include <vector>
#include <atomic>

/**
 * @class MatchingEngine
 * @brief Processes incoming orders and matches them against the order book.
 * Handles all order types (MARKET, LIMIT) and time-in-force policies (GTC, IOC, FOK).
 * Generates trades when orders are matched.
 */
template<typename StorageBook>
class MatchingEngine {
private:
    StorageBook& OrderBook_; ///< Reference to the order book for this symbol
    std::atomic<int>& TradeCounter_; ///< Shared trade ID counter
    OrderPool& Pool_;

public:
    ~MatchingEngine() = default;

    /**
     * @brief Constructs a matching engine for a specific order book.
     * @param OrderBook Reference to the order book.
     * @param TradeCounter Reference to the shared trade ID counter.
     */
    MatchingEngine(StorageBook& OrderBook, std::atomic<int>& TradeCounter,
                   OrderPool& Pool)
        : OrderBook_(OrderBook),
        TradeCounter_(TradeCounter),
        Pool_(Pool){};

    /**
     * @brief Processes a new order, routing it to the appropriate handler.
     * @param index The index of new order to process.
     * @param Trades Output vector where generated trades are appended.
     */
    void ProcessOrder(const uint32_t index, std::vector<TradePod>& Trades){

        auto& NewOrder = Pool_.Get(index);
        OrderType Type = NewOrder.Type;
        OrderSide Side = NewOrder.Side;

        if (Side == OrderSide::BUY) {

            if (Type == OrderType::LIMIT) {
                ProcessBuyLimit(index,Trades);
            }
            else {
                ProcessBuyMarket(index,Trades);
            }
        }
        else  {
            if (Type == OrderType::LIMIT) {
                ProcessSellLimit(index, Trades);
            }
            else  {
                ProcessSellMarket(index, Trades);
            }
        }
    }

    /**
     * @brief Returns a const reference to the order book.
     * @return The order book.
     */
    const StorageBook& GetOrderBook() const { return OrderBook_; }

    /**
     * @brief Matches a buy order against existing asks.
     * @param index The index of buy order to match.
     * @param Trades Output vector for generated trades.
     */
    void MatchOrderBid(const uint32_t index, std::vector<TradePod>& Trades){
        auto& NewOrder = Pool_.Get(index);

        while (!OrderBook_.IsAskEmpty()
               and NewOrder.Quantity > 0
               and (NewOrder.Price >= Pool_.Get(OrderBook_.GetBestAsk()).Price
                    or NewOrder.Type == OrderType::MARKET)) {

            auto BestAskIndex = OrderBook_.GetBestAsk();
            auto& BestAsk = Pool_.Get(BestAskIndex);
            uint64_t timestamp = NowTimestamp();

            uint32_t Quantity = std::min(NewOrder.Quantity, BestAsk.Quantity);
            uint32_t ID = TradeCounter_++;
            TradePod NewTrade;
            NewTrade = TradePod{BestAsk.Price,
                                timestamp,
                                ID,
                                BestAsk.TraderId,
                                NewOrder.TraderId,
                                Quantity,
                                NewOrder.Symbol,
            };
            Trades.push_back(std::move(NewTrade));
            NewOrder.Quantity = (NewOrder.Quantity - Quantity);
            if (BestAsk.Quantity - Quantity == 0) {
                BestAsk.Quantity = 0;
                OrderBook_.PopBestAsk();
            } else {
                OrderBook_.UpdateQuantity(BestAsk.OrderId,
                                          BestAsk.Quantity - Quantity);
            }
        }
    }

    /**
     * @brief Matches a sell order against existing bids.
     * @param index The index of sell order to match.
     * @param Trades Output vector for generated trades.
     */
    void MatchOrderAsk(const uint32_t index, std::vector<TradePod>& Trades){
        auto& NewOrder = Pool_.Get(index);

        while (!OrderBook_.IsBidEmpty()
               and NewOrder.Quantity > 0
               and (NewOrder.Price <= Pool_.Get(OrderBook_.GetBestBid()).Price
                    or NewOrder.Type == OrderType::MARKET)) {

            auto BestBidIndex = OrderBook_.GetBestBid();
            auto& BestBid = Pool_.Get(BestBidIndex);

            uint64_t timestamp = NowTimestamp();

            uint32_t Quantity = std::min(NewOrder.Quantity, BestBid.Quantity);
            uint32_t ID = TradeCounter_++;
            TradePod NewTrade;
            NewTrade = TradePod{BestBid.Price,
                                timestamp,
                                ID,
                                BestBid.TraderId,
                                NewOrder.TraderId,
                                Quantity,
                                NewOrder.Symbol,
            };
            NewOrder.Quantity -= Quantity;
            Trades.push_back(std::move(NewTrade));
            if (BestBid.Quantity - Quantity == 0) {
                BestBid.Quantity = 0;
                OrderBook_.PopBestBid();
            } else {
                OrderBook_.UpdateQuantity(BestBid.OrderId,
                                          BestBid.Quantity - Quantity);
            }
        }
    }

    /**
     * @brief Processes a buy limit order.
     */
    void ProcessBuyLimit(const uint32_t index, std::vector<TradePod>& Trades){
        auto& NewOrder = Pool_.Get(index);

        if (NewOrder.TIF == TimeInForce::FOK) {
            if (OrderBook_.CanFillQuantityAsks(NewOrder.Quantity,NewOrder.Price)) {
                MatchOrderBid(index, Trades);
                Pool_.Free(index);
            }
            else {
                NewOrder.Status = OrderStatus::REJECTED;
                Pool_.Free(index);
            }
            return;
        }
        MatchOrderBid(index, Trades);
        if (NewOrder.TIF == TimeInForce::GTC and NewOrder.Quantity > 0) {
            OrderBook_.AddOrder(index);
        }
        else{
            Pool_.Free(index);
        }
    }

    /**
     * @brief Processes a buy market order.
     */
    void ProcessBuyMarket(const uint32_t index, std::vector<TradePod>& Trades){
        auto& NewOrder = Pool_.Get(index);

        if (NewOrder.TIF == TimeInForce::FOK) {
            if (OrderBook_.CanFillQuantityAsks(NewOrder.Quantity,NewOrder.Price)) {
                MatchOrderBid(index, Trades);
                Pool_.Free(index);
            }
            else {
                NewOrder.Status = OrderStatus::REJECTED;
                Pool_.Free(index);
            }
            return;
        }
        MatchOrderBid(index, Trades);
        Pool_.Free(index);
    }

    /**
     * @brief Processes a sell limit order.
     */
    void ProcessSellLimit(const uint32_t index, std::vector<TradePod>& Trades){
        auto& NewOrder = Pool_.Get(index);

        if (NewOrder.TIF == TimeInForce::FOK) {
            if (OrderBook_.CanFillQuantityBids(NewOrder.Quantity,NewOrder.Price)) {
                MatchOrderAsk(index, Trades);
                Pool_.Free(index);
            }
            else {
                NewOrder.Status = OrderStatus::REJECTED;
                Pool_.Free(index);
            }
            return;
        }
        MatchOrderAsk(index, Trades);
        if (NewOrder.TIF == TimeInForce::GTC and NewOrder.Quantity > 0) {
            OrderBook_.AddOrder(index);
        }
        else{
            Pool_.Free(index);
        }
    }

    /**
     * @brief Processes a sell market order.
     */
    void ProcessSellMarket(const uint32_t index, std::vector<TradePod>& Trades){
        auto& NewOrder = Pool_.Get(index);

        if (NewOrder.TIF == TimeInForce::FOK) {
            if (OrderBook_.CanFillQuantityBids(NewOrder.Quantity,NewOrder.Price)) {
                MatchOrderAsk(index, Trades);
                Pool_.Free(index);
            }
            else {
                NewOrder.Status = OrderStatus::REJECTED;
                Pool_.Free(index);
            }
            return;
        }
        MatchOrderAsk(index, Trades);
        Pool_.Free(index);
    }
};

#endif //LIMITORDERBOOK_MATCHINGENGINE_H