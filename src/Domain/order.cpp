//
// Created by denni on 3/7/2026.
//

#include "../../include/Domain/order.h"
#include <stdexcept>
#include <iomanip>


Order::Order(int Order_ID, int Trader_ID, OrderSide Side,
             OrderType Type,
             const std::string& Symbol, uint64_t Price,int Quantity,
             std::chrono::system_clock::time_point Timestamp,
             TimeInForce TIF,
             OrderStatus Status):
        Order_ID_(Order_ID),
        Trader_ID_(Trader_ID),
        Side_(Side),
        Type_(Type),
        Symbol_(Symbol),
        Price_(Price),
        Quantity_(Quantity),
        Timestamp_(Timestamp),
        TIF_(TIF),
        Status_(Status){
    /**
     * @brief Constructor for the Order class.
     * Performs validation on price, quantity, and symbol to ensure data integrity.
     * * @throw std::invalid_argument If price <= 0, quantity <= 0, or symbol is empty.
     */
    if (Price_ == 0 and Type_ != OrderType::MARKET) {
        throw std::invalid_argument("Price must be higher than 0.0");
    }
    if (Quantity_<= 0) {
        throw std::invalid_argument("Quantity must be higher than 0");
    }
    if (Symbol_.empty()) {
        throw std::invalid_argument("Symbol shouldn't be empty");
    }
}

std::ostream& operator<<(std::ostream& os, const Order& order) {
    /**
     * @brief Overloads the stream insertion operator for the Order class.
     * Formats the order data into a human-readable string, including
     * millisecond-precision timestamps (HH:MM:SS.mmm).
     * * @param os The output stream.
     * @param order The order object to be printed.
     * @return std::ostream& The modified output stream.
     */

    auto tp=order.GetTime();
    std::time_t t =std::chrono::system_clock::to_time_t(tp);
    std::tm bt;
#ifdef _WIN32
    localtime_s(&bt,&t);
#else
    localtime_r(&t,&bt);
#endif

    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(
            tp.time_since_epoch()%std::chrono::seconds(1)).count();

    os << "Order[ ID: " << order.GetOrderID()
       << " | Trader: " <<order.GetTraderID()
       << " | Side: " <<ToString(order.GetSide())
       << " | Time: " << std::put_time(&bt,"%H:%M:%S.")<<std::setfill('0')<<std::setw(3)<<ms
       << " | Symbol: " << order.GetSymbol()
       << " | Price: " << order.GetPrice()
       << " | Type: " <<ToString(order.GetType())
       << " | Quantity: " <<order.GetQuantity()
       << " | TIF: " << ToString(order.GetTIF())
       << " | Status: "<< ToString(order.GetStatus())
       << " ]";

    return os;
}