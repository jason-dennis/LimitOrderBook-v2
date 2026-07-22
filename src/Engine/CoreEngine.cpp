//
// Created by denni on 3/17/2026.
//

#include "Engine/CoreEngine.h"
#include <fstream>
#include <sstream>
#include <string>


CoreEngine::CoreEngine(): Router_() {
    this->LoadFromFile();
}

void CoreEngine::Save() {
    this->SaveToFile();
}
void CoreEngine ::LoadFromFile() {

    std::ifstream file("orders.csv");
    if (!file.is_open()) return;

    std::string line;

    std::getline(file,line);
    while (std::getline(file,line)) {
        std::stringstream ss(line);

        std::string item;

        std::getline(ss,item,',');
        int ID = std::stoi(item);

        std::getline(ss,item,',');
        int TraderID =std::stoi(item);

        std::getline(ss,item,',');
        auto Side = item;

        std::getline(ss,item,',');
        auto Type = (item);

        std::getline(ss,item,',');
        auto Symbol = item;

        std::getline(ss,item,',');
        float Price = std::stof(item);

        std::getline(ss,item,',');
        int Qty = std::stoi(item);

        std::getline(ss,item,',');
        auto ns = std::stoll(item);
        std::getline(ss,item,',');
        auto TIF = item;

        std::getline(ss,item,',');
        auto Status = item;

        if (Qty > 0) {
            OrderRequest NewOrder = OrderRequest{Price,
                                                 Qty,
                                                 Type,
                                                 Symbol,
                                                 TIF,
                                                 TraderID,
                                                 Side};
            Router_.Load(NewOrder,ns);
        }
    }

    file.close();
}

void CoreEngine ::SaveToFile() {

    std::ofstream file("orders.csv");

    if (!file.is_open()) return;

    file << "ID,TraderID,Side,Type,Symbol,Price,Qty,Timestamp,TIF,Status" << std::endl;
    for (auto [Symbol,Orders]: Router_.GetAllOrders()) {
        for (auto order:Orders) {
            if (order.Quantity == 0)
                continue;
            file << order.OrderId<<",";
            file << order.TraderId<<",";
            file << ToString(order.Side)<<",";
            file << ToString(order.Type)<<",";
            file << Symbol<<",";
            file << (static_cast<float>(order.Price) / PriceTick)<<",";
            file << static_cast<int>(order.Quantity)<<",";
            file << order.Timestamp<<",";
            file<< ToString(order.TIF)<<",";
            file << ToString(order.Status)<<std::endl;
        }
    }
}



void CoreEngine::CreateOrder(float Price, int Quantity, const std::string& Type,
                             const std::string& Symbol, const std::string& TIF,int TraderID,const std::string& Side) {

    OrderRequest request = OrderRequest{
        Price,
        Quantity,
        Type,
        Symbol,
        TIF,
        TraderID,
        Side
    };
    Router_.Route(request);
}

void CoreEngine::CancelOrder(int order_id,const std::string& Symbol) {
    Router_.CancelOrder(order_id,Symbol);
}

std::vector<Trade> CoreEngine::GetTradesHistory(const std::string &Symbol) {
    auto trades = Router_.GetTradesHistory(Symbol);
    std::vector<Trade> result;
    for(const auto& item : trades){
        auto timePoint = ToTimePoint(item.Timestamp);
        Trade trade = Trade(item.TradeId,
                            static_cast<int>(item.MakerId),
                            static_cast<int>(item.TakerId),
                            item.Price,
                            static_cast<int>(item.Quantity),
                            Symbol,
                            timePoint);
        result.push_back(trade);
    }

    return result;
}
std::vector<Order> CoreEngine::GetOrders(const std::string& Symbol) {
    auto orders = Router_.GetOrders(Symbol);
    std::vector<Order> result;
    for(const auto& item : orders){
        auto timePoint = ToTimePoint(item.Timestamp);
        Order order = GetOrder(Symbol, item, timePoint);
        result.push_back(order);

    }
    return result;

}

std::vector<Order> CoreEngine::GetBestBids(int x, std::string& Symbol) {
    auto orders = Router_.GetBestBids(x,Symbol);
    std::vector<Order> result;
    for(const auto& item : orders){
        auto timePoint = ToTimePoint(item.Timestamp);
        Order order = GetOrder(Symbol, item, timePoint);
        result.push_back(order);

    }
    return result;
}

std::vector<Order> CoreEngine::GetBestAsks(int x, std::string& Symbol) {
    auto orders = Router_.GetBestAsk(x,Symbol);
    std::vector<Order> result;
    for(const auto& item : orders){
        auto timePoint = ToTimePoint(item.Timestamp);
        Order order = GetOrder(Symbol, item, timePoint);
        result.push_back(order);

    }
    return result;
}

Order CoreEngine::GetOrder(const std::string &Symbol, const OrderPod& item, std::chrono::system_clock::time_point &timePoint) const {
    Order order = Order(static_cast<int>(item.OrderId),
                    static_cast<int>(item.TraderId),
                    item.Side,
                    item.Type,
                    Symbol,
                    item.Price,
                    static_cast<int>(item.Quantity),
                    timePoint,
                    item.TIF,
                    item.Status);
    return order;
}





