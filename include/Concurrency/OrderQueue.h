//
// Created by Ognean Jason Dennis on 13.07.2026.
//

#ifndef LIMITORDERBOOK_ORDERQUEUE_H
#define LIMITORDERBOOK_ORDERQUEUE_H
#include <queue>
#include <tuple>
#include <mutex>
#include <condition_variable>
#include "Domain/OrderRequest.h"

class OrderQueue{
private:
    std::queue<OrderRequest> MyQueue;
    std::mutex queue_mutex;
    std::condition_variable pop_condition;
    bool shutdown = false;


public:

    void Push(const OrderRequest& request);
    std::optional<OrderRequest> Pop();
    void Shutdown();
    size_t Size();

};

#endif //LIMITORDERBOOK_ORDERQUEUE_H
