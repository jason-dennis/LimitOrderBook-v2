//
// Created by Ognean Jason Dennis on 21.07.2026.
//

#include "Concurrency/SymbolWorker.h"
SymbolWorker::~SymbolWorker() {
    Stop();
    if (Thread_.joinable()) {
        Thread_.join();
    }
}
void SymbolWorker::Run(){
    std::vector<TradePod> trades;   // reused for every order: no per-order allocation
    trades.reserve(256);

    while(Running_.load()){
        try{
            auto result = Resources_.Queue.Pop();
            if(!result.has_value()){
                std::this_thread::yield();
                continue;
            }

            auto tmpOrder = *result;
            std::unique_lock<std::shared_mutex>lock(Resources_.StateMutex);
            auto index = Resources_.Pool.Allocate();
            if(index == UINT32_MAX) {
                Resources_.OrdersRejected.fetch_add(1, std::memory_order_relaxed);
                continue;
            }

            auto& slot = Resources_.Pool.Get(index);
            slot = tmpOrder;
            slot.OrderId = OrderId++;

            trades.clear();
            Resources_.Engine.ProcessOrder(index,trades);

            uint64_t volume = 0;
            for (const auto& trade : trades) {
                Resources_.Trades.Push(trade);
                volume += trade.Quantity;
            }
            Resources_.TradesTotal.fetch_add(trades.size(), std::memory_order_relaxed);
            Resources_.VolumeTotal.fetch_add(volume, std::memory_order_relaxed);
            Resources_.OrdersProcessed.fetch_add(1, std::memory_order_relaxed);

        } catch (const std::exception& e) {
            fprintf(stderr, "EXCEPTIE PRINSA in SymbolWorker::Run: %s\n", e.what());
            throw;
        } catch (...) {
            fprintf(stderr, "EXCEPTIE NECUNOSCUTA PRINSA in SymbolWorker::Run\n");
            throw;
        }
    }
}
void SymbolWorker::Join() {
    Thread_.join();
}
void SymbolWorker::Stop() {
    Running_.store(false);
}