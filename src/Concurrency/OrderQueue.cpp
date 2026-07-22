//
// Created by Ognean Jason Dennis on 13.07.2026.
//
#include "../include/Concurrency/OrderQueue.h"

void OrderQueue::Push(const OrderRequest &request) {

    std::lock_guard<std::mutex>lock(queue_mutex);
    MyQueue.push(request);
    pop_condition.notify_one();
}

std::optional<OrderRequest> OrderQueue::Pop() {

    std::unique_lock lk(queue_mutex);
    while(MyQueue.empty() and !shutdown)
        pop_condition.wait(lk);

    if(MyQueue.empty())
        return std::nullopt;


    auto request = MyQueue.front();
    MyQueue.pop();

    return request;
}

void OrderQueue::Shutdown() {
    std::lock_guard<std::mutex> lock(queue_mutex);
    shutdown = true;
    pop_condition.notify_all();
}

size_t OrderQueue::Size() {
    std::lock_guard<std::mutex>lock(queue_mutex);
    return MyQueue.size();

}