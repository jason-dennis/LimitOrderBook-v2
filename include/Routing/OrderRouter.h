//
// Created by Ognean Jason Dennis on 17.07.2026.
//

#ifndef LIMITORDERBOOK_ORDERROUTER_H
#define LIMITORDERBOOK_ORDERROUTER_H
#include <unordered_map>
#include "Concurrency/LockFreeQueue.h"
#include "Memory/OrderPool.h"
#include "Engine/MatchingEngine.h"
#include "Storage/BinaryOrderBookStorage.h"
#include "Domain/OrderRequest.h"
#include "Concurrency/SymbolWorker.h"
#include <shared_mutex>
#include <thread>
constexpr uint32_t PriceTick = 100;

class OrderRouter{
private:

    std::shared_mutex RegisteryMutex_;
    std::unordered_map<uint32_t, std::unique_ptr<SymbolResources>> Resources_;
    std::unordered_map<uint32_t, std::unique_ptr<SymbolWorker>> Worker_;
    std::unordered_map<std::string, uint32_t> SymbolHash_;
    uint32_t SymbolID_ = 0;
public:
    OrderRouter() = default;
    std::optional<uint32_t>CheckSymbolId(const std::string Symbol);
    uint32_t CheckRegistery(const std::string& Symbol);
    void Route(OrderRequest& request);
    void Load(OrderRequest& request, uint64_t timestamp);
    void CancelOrder(int order_id, const std::string& Symbol);
    std::vector<TradePod> GetTradesHistory(const std::string &Symbol);
    std::vector<OrderPod> GetOrders(const std::string &Symbol);
    std::vector<OrderPod> GetBestBids(int x, std::string& Symbol);
    std::vector<OrderPod> GetBestAsk(int x, std::string& Symbol);
    std::vector<std::pair<std::string,std::vector<OrderPod>>> GetAllOrders();
    void ShutDown();
};

#endif //LIMITORDERBOOK_ORDERROUTER_H
