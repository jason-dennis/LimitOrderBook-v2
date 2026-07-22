//
// Created by Ognean Jason Dennis on 17.07.2026.
//

#ifndef LIMITORDERBOOK_ORDERPOOL_H
#define LIMITORDERBOOK_ORDERPOOL_H
#include <vector>
#include "Domain/OrderPod.h"

static constexpr uint32_t DefaultCapacity = 100000;

class OrderPool{
private:
    std::vector<OrderPod> Storage_;
    std::vector<uint32_t> FreeIndex_;
public:

    OrderPool();
    uint32_t Allocate();
    void Free(uint32_t index);
    OrderPod& Get(uint32_t index);
};

#endif //LIMITORDERBOOK_ORDERPOOL_H
