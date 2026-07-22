//
// Created by denni on 3/7/2026.
//
#pragma once
#ifndef LIMITORDERBOOK_ORDER_H
#define LIMITORDERBOOK_ORDER_H
#include <string>
#include <chrono>

/** @enum OrderStatus Represents the current lifecycle stage of an order. */
enum class OrderStatus {
    NEW,
    PARTIALLY_FILLED,
    FILLED,
    CANCELED,
    REJECTED
};

/** @enum OrderSide Represents the direction of the trade. */
enum class OrderSide {
    BUY,
    SELL
};

/** @enum TimeInForce Defines how long the order remains active. */
enum class TimeInForce {
    GTC,
    IOC,
    FOK
};

/** @enum OrderType Represents the execution constraint of the order. */
enum class OrderType {
    MARKET,
    LIMIT,
    STOP,
    STOP_LIMIT
};

// Helper functions for string conversion
constexpr const char* ToString(OrderStatus status) {
    constexpr const char* names[] = {"NEW", "PARTIALLY_FILLED", "FILLED", "CANCELED", "REJECTED"};
    return names[static_cast<int>(status)];
}
constexpr const char* ToString(OrderSide side) {
    constexpr const char* names[] = {"BUY", "SELL"};
    return names[static_cast<int>(side)];
}
constexpr const char* ToString(OrderType type) {
    constexpr const char* names[] = {"MARKET", "LIMIT", "STOP","STOP_LIMIT"};
    return names[static_cast<int>(type)];
}
constexpr const char* ToString(TimeInForce tif) {
    constexpr const char* names[] = {"GTC", "IOC", "FOK"};
    return names[static_cast<int>(tif)];
}
// Helper functions for enum conversion
inline OrderStatus ToOrderStatus(const std::string& s) {
    if (s == "NEW")              return OrderStatus::NEW;
    if (s == "PARTIALLY_FILLED") return OrderStatus::PARTIALLY_FILLED;
    if (s == "FILLED")           return OrderStatus::FILLED;
    if (s == "CANCELED")         return OrderStatus::CANCELED;
    if (s == "REJECTED")         return OrderStatus::REJECTED;
    throw std::invalid_argument("Valoare OrderStatus necunoscuta: " + s);
}

inline OrderSide ToOrderSide(const std::string& s) {
    if (s == "BUY")  return OrderSide::BUY;
    if (s == "SELL") return OrderSide::SELL;
    throw std::invalid_argument("Valoare OrderSide necunoscuta: " + s);

}

inline OrderType ToOrderType(const std::string& s) {
    if (s == "MARKET")     return OrderType::MARKET;
    if (s == "LIMIT")      return OrderType::LIMIT;
    if (s == "STOP")       return OrderType::STOP;
    if (s == "STOP_LIMIT") return OrderType::STOP_LIMIT;
    throw std::invalid_argument("Valoare OrderTyoe necunoscuta: " + s);
}

inline TimeInForce ToTimeInForce(const std::string& s) {
    if (s == "GTC") return TimeInForce::GTC;
    if (s == "IOC") return TimeInForce::IOC;
    if (s == "FOK") return TimeInForce::FOK;
    throw std::invalid_argument("Valoare OrderTIF necunoscuta: " + s);
}

class Order {
    /**
     * @class Order
     * @brief Represents a financial order in the trading system.
     * This class is immutable for its core identity (ID, Trader, Symbol)
     * but allows modification of quantity and status via 'mutable' fields
     * to support high-performance updates within sorted containers.
     */
private:

    const int Order_ID_;
    const int Trader_ID_;
    OrderSide Side_;
    OrderType Type_;
    std::string Symbol_;
    uint64_t Price_;
    mutable int Quantity_;
    std::chrono::system_clock::time_point Timestamp_;
    TimeInForce TIF_;
    mutable OrderStatus Status_;

public:
    virtual ~Order() = default;
    /**
     * @brief Constructs a new Order object.
     * @param Order_ID Unique ID.
     * @param Trader_ID Owner ID.
     * @param Side Buy/Sell.
     * @param Type Execution type.
     * @param Symbol Asset ticker.
     * @param Price Price in integer format.
     * @param Quantity Initial volume.
     * @param Timestamp Entry time.
     * @param TIF Time in force policy.
     * @param Status Initial status.
     */
    Order(int Order_ID, int Trader_ID, OrderSide Side,
          OrderType Type,
          const std::string& Symbol, uint64_t Price, int Quantity,
          std::chrono::system_clock::time_point Timestamp,
          TimeInForce TIF,
          OrderStatus Status);

    int GetOrderID() const {return Order_ID_;}
    int GetTraderID() const {return Trader_ID_;}
    uint64_t GetPrice() const {return Price_;}
    int GetQuantity() const {return Quantity_;}
    std::chrono::system_clock::time_point GetTime() const {return Timestamp_;}

    const std::string& GetSymbol() const {return Symbol_;}
    OrderType GetType() const {return Type_;}
    TimeInForce GetTIF() const {return TIF_;}
    OrderStatus GetStatus() const {return Status_;}
    OrderSide GetSide() const {return Side_;}

    /**
     * @brief Updates the order status.
     * Marked const because Status_ is mutable, allowing updates in multisets.
     */
    void SetStatus(OrderStatus New_Status) const {Status_=New_Status;}
    /**
     * @brief Updates the remaining quantity.
     * Marked const because Quantity_ is mutable.
     */
    void SetQuantity(int new_quantity)const {Quantity_=new_quantity;}

};
/** @brief Global operator for printing Order details. */
std::ostream& operator<<(std::ostream& os,const Order& order);

#endif //LIMITORDERBOOK_ORDER_H