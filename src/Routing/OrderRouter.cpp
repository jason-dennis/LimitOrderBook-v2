//
// Created by Ognean Jason Dennis on 19.07.2026.
//
#include "Routing/OrderRouter.h"

uint32_t OrderRouter::CheckRegistery(const std::string& Symbol) {
    {
        std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
        auto it = SymbolHash_.find(Symbol);
        if(it != SymbolHash_.end()){
            return it->second;
        }
    }

    std::unique_lock<std::shared_mutex> lock(RegisteryMutex_);
    auto it = SymbolHash_.find(Symbol);
    if(it != SymbolHash_.end()){
        return it->second;
    }

    uint32_t NewId = SymbolID_++;
    Resources_[NewId] = std::make_unique<SymbolResources>(NewId);
    Worker_[NewId] = std::make_unique<SymbolWorker>(*Resources_[NewId]);
    SymbolHash_[Symbol] = NewId;
    return NewId;
}

std::optional<uint32_t> OrderRouter::CheckSymbolId(const std::string Symbol) {
    std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
    auto it = SymbolHash_.find(Symbol);
    if(it == SymbolHash_.end()) return std::nullopt;
    return it->second;
}

void OrderRouter::Load(OrderRequest& request, uint64_t timestamp){
    auto SymbolHash = CheckRegistery(request.Symbol);
    OrderPod order = OrderPod{static_cast<uint64_t>(request.Price * PriceTick),
                              timestamp,
                              UINT32_MAX,
                              static_cast<uint32_t>(request.TraderID),
                              static_cast<uint32_t>(request.Quantity),
                              SymbolHash,
                              ToOrderSide(request.Side),
                              ToOrderType(request.Type),
                              ToTimeInForce(request.TIF),
                              OrderStatus::NEW
    };
    std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
    auto& Resources = Resources_.at(SymbolHash);
    Resources->Queue.Push(order);
}
void OrderRouter::Route(OrderRequest& request) {

    auto SymbolHash = CheckRegistery(request.Symbol);;
    uint64_t timestamp = NowTimestamp();
    OrderPod order = OrderPod{static_cast<uint64_t>(request.Price * PriceTick),
                              timestamp,
                              UINT32_MAX,
                              static_cast<uint32_t>(request.TraderID),
                              static_cast<uint32_t>(request.Quantity),
                              SymbolHash,
                              ToOrderSide(request.Side),
                              ToOrderType(request.Type),
                              ToTimeInForce(request.TIF),
                              OrderStatus::NEW
    };
    std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
    auto& Resources = Resources_.at(SymbolHash);
    Resources->Queue.Push(order);
}
void OrderRouter::CancelOrder(int order_id, const std::string &Symbol) {
    auto SymbolHash = CheckSymbolId(Symbol);
    if(!SymbolHash.has_value()) return;
    SymbolResources* Resources;
    {
        std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
        Resources = Resources_.at(*SymbolHash).get();
    }
    std::unique_lock<std::shared_mutex> SymbolLock(Resources->StateMutex);
    Resources->OrderBook.CancelOrder(order_id);

}

std::vector<std::pair<std::string,std::vector<OrderPod>>> OrderRouter::GetAllOrders(){
    std::vector<std::string>Symbols;
    {
        std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
        for(const auto& [Symbol, SymbolId] : SymbolHash_){
            Symbols.push_back(Symbol);
        }
    }
    std::vector<std::pair<std::string,std::vector<OrderPod>>> result;
    for(const auto& Symbol: Symbols){
        result.push_back({Symbol, GetOrders(Symbol)});
    }
    return result;
}
std::vector<TradePod> OrderRouter::GetTradesHistory(const std::string &Symbol) {
    auto SymbolHash = CheckSymbolId(Symbol);
    if(!SymbolHash.has_value()) return {};
    SymbolResources* Resources;
    {
        std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
        Resources = Resources_.at(*SymbolHash).get();
    }
    std::shared_lock<std::shared_mutex> SymbolLock(Resources->StateMutex);
    return Resources->Trades;
}

std::vector<OrderPod> OrderRouter::GetOrders(const std::string &Symbol) {
    auto SymbolHash = CheckSymbolId(Symbol);
    if(!SymbolHash.has_value()) return {};
    SymbolResources* Resources;
    {
        std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
        Resources = Resources_.at(*SymbolHash).get();
    }
    std::shared_lock<std::shared_mutex> SymbolLock(Resources->StateMutex);
    std::vector<OrderPod> result;
    for(auto IndexOrder : Resources->OrderBook.GetBestBids(DefaultCapacity)){
        result.push_back(Resources->Pool.Get(IndexOrder));
    }
    for(auto IndexOrder : Resources->OrderBook.GetBestAsks(DefaultCapacity)){
        result.push_back(Resources->Pool.Get(IndexOrder));
    }
    return result;
}

std::vector<OrderPod> OrderRouter::GetBestBids(int x, std::string &Symbol) {
    auto SymbolHash = CheckSymbolId(Symbol);
    if(!SymbolHash.has_value()) return {};
    SymbolResources* Resources;
    {
        std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
        Resources = Resources_.at(*SymbolHash).get();
    }
    std::shared_lock<std::shared_mutex> SymbolLock(Resources->StateMutex);
    std::vector<OrderPod> result;
    for(auto IndexOrder : Resources->OrderBook.GetBestBids(x)){
        result.push_back(Resources->Pool.Get(IndexOrder));
    }
    return result;

}
std::vector<OrderPod> OrderRouter::GetBestAsk(int x, std::string &Symbol) {
    auto SymbolHash = CheckSymbolId(Symbol);
    if(!SymbolHash.has_value()) return {};
    SymbolResources* Resources;
    {
        std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);
        Resources = Resources_.at(*SymbolHash).get();
    }
    std::shared_lock<std::shared_mutex> SymbolLock(Resources->StateMutex);
    std::vector<OrderPod> result;
    for(auto IndexOrder : Resources->OrderBook.GetBestAsks(x)){
        result.push_back(Resources->Pool.Get(IndexOrder));
    }
    return result;
}

void OrderRouter::ShutDown() {
    std::shared_lock<std::shared_mutex> lock(RegisteryMutex_);

    for(auto& [SymbolID,Worker] : Worker_){
        Worker->Stop();
    }
    for(auto& [SymbolID,Worker] : Worker_){
        Worker->Join();
    }
}

