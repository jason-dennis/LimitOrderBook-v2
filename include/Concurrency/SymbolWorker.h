//
// Created by Ognean Jason Dennis on 21.07.2026.
//

#ifndef LIMITORDERBOOK_SYMBOLWORKER_H
#define LIMITORDERBOOK_SYMBOLWORKER_H
#include <atomic>
#include <cstdint>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include "Concurrency/LockFreeQueue.h"
#include "Memory/OrderPool.h"
#include "Engine/MatchingEngine.h"
#include "Storage/BinaryOrderBookStorage.h"
#include "Domain/OrderRequest.h"
#include "Domain/TradeHistory.h"
#define EngineStorage BinaryOrderBook
constexpr uint32_t QueueCapacity = 524288;
constexpr std::size_t TradeHistoryCapacity = 10'000;

/// Lifetime counters for one symbol. Read without locks.
struct SymbolStats {
    uint64_t OrdersProcessed = 0;  ///< orders taken from the queue and run through matching
    uint64_t OrdersRejected  = 0;  ///< dropped by the worker because the pool was full
    uint64_t QueueDrops      = 0;  ///< dropped by the router because the queue was full
    uint64_t TradesTotal     = 0;
    uint64_t VolumeTotal     = 0;
};

struct SymbolResources{
    uint32_t SymbolId;
    OrderPool Pool;
    LockFreeQueue<OrderPod, QueueCapacity> Queue;
    BinaryOrderBook OrderBook;
    std::atomic<int> TraderCounter;
    MatchingEngine<EngineStorage> Engine;
    std::shared_mutex StateMutex;
    TradeHistory Trades{TradeHistoryCapacity};   ///< last N trades (UI, queries)
    std::atomic<uint64_t> OrdersProcessed{0};
    std::atomic<uint64_t> OrdersRejected{0};
    std::atomic<uint64_t> QueueDrops{0};
    std::atomic<uint64_t> TradesTotal{0};
    std::atomic<uint64_t> VolumeTotal{0};
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
    uint32_t OrderId = 0;
    std::thread Thread_;   // last member: the thread starts only after everything above is initialised

public:
    SymbolWorker(SymbolResources& Resources): Resources_(Resources),
    Thread_(&SymbolWorker::Run,this){}
    ~SymbolWorker();
    void Run();
    void Stop();
    void Join();
};
#endif //LIMITORDERBOOK_SYMBOLWORKER_H
