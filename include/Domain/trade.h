//
// Created by denni on 3/11/2026.
//

#ifndef LIMITORDERBOOK_TRADE_H
#define LIMITORDERBOOK_TRADE_H
#include <chrono>
class Trade {
    /**
     *@class Trade
     *@brief Represent a financial trade between 2 orders in trading system
     *
     */
private:
    int ID_;
    int MakerID_;
    int TakerID_;
    uint64_t Price_;
    int Quantity_;
    std::string Symbol_;
    std::chrono::system_clock::time_point Timestamp_;

public:
     ~Trade()=default;

    /**
     *
     *
     * @param ID /
     * @param MakerID
     * @param TakerID
     * @param Price
     * @param Quantity
     * @param Symbol
     * @param Timestamp
     */
    Trade(int ID, int MakerID, int TakerID,
          uint64_t Price,int Quantity,std::string Symbol,
          std::chrono::system_clock::time_point Timestamp):
        ID_(ID),
        MakerID_(MakerID),
        TakerID_(TakerID),
        Price_(Price),
        Quantity_(Quantity),
        Symbol_(Symbol),
        Timestamp_(Timestamp){
        }

    /**
     * Getter functions
     */
    int GetID() const {return ID_;}
    int GetMakerID() const {return MakerID_;}
    int GetTakerID() const {return TakerID_;}
    uint64_t GetPrice() const {return Price_;}
    int GetQuantity() const {return Quantity_;}
    std::string GetSymbol() const {return Symbol_;}
    std::chrono::system_clock::time_point GetTimestamp() const {return Timestamp_;}
};
#endif //LIMITORDERBOOK_TRADE_H