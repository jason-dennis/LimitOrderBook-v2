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
            if(index == UINT32_MAX) continue;

            auto& slot = Resources_.Pool.Get(index);
            slot = tmpOrder;
            slot.OrderId = OrderId++;

            std::vector<TradePod> trades;

            Resources_.Engine.ProcessOrder(index,trades);
            Resources_.Trades.insert(Resources_.Trades.end(),trades.begin(),trades.end());

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