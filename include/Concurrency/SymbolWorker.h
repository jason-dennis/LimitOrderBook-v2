//
// Created by Ognean Jason Dennis on 21.07.2026.
//

#ifndef LIMITORDERBOOK_SYMBOLWORKER_H
#define LIMITORDERBOOK_SYMBOLWORKER_H
#include <atomic>
#include <cstdint>
#include <thread>
#include <shared_mutex>
#include "Concurrency/LockFreeQueue.h"
#include "Memory/OrderPool.h"
#include "Engine/MatchingEngine.h"
#include "Storage/BinaryOrderBookStorage.h"
#include "Domain/OrderRequest.h"
#define EngineStorage BinaryOrderBook
constexpr uint32_t QueueCapacity = 524288;

struct SymbolResources{
    uint32_t SymbolId;
    OrderPool Pool;
    LockFreeQueue<OrderPod, QueueCapacity> Queue;
    BinaryOrderBook OrderBook;
    std::atomic<int> TraderCounter;
    MatchingEngine<EngineStorage> Engine;
    std::shared_mutex StateMutex;
    std::vector<TradePod> Trades;
    SymbolResources(uint32_t Symbol):SymbolId(Symbol),
                                   Pool(),Queue(), OrderBook(Pool),
                                   TraderCounter(1),
                                   Engine(OrderBook,TraderCounter,Pool){
    }
};

class SymbolWorker{
private:
    SymbolResources& Resources_;
    std::atomic<bool> Running_{true};
    std::thread Thread_;
    uint32_t OrderId = 0;

public:
    SymbolWorker(SymbolResources& Resources): Resources_(Resources),
    Thread_(&SymbolWorker::Run,this){}
    ~SymbolWorker();
    void Run();
    void Stop();
    void Join();
};
#endif //LIMITORDERBOOK_SYMBOLWORKER_H
