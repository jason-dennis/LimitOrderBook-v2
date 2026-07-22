//
// Created by Ognean Jason Dennis on 13.07.2026.
//

#ifndef LIMITORDERBOOK_WORKER_H
#define LIMITORDERBOOK_WORKER_H
#include "../include/Engine/CoreEngine.h"
#include "../include/Concurrency/OrderQueue.h"
class Worker{
private:
    CoreEngine& Engine_;
    OrderQueue& Queue_;
public:

    Worker(CoreEngine& Engine, OrderQueue& Queue):
    Engine_(Engine),
    Queue_(Queue){}

    void run();
};
#endif //LIMITORDERBOOK_WORKER_H
