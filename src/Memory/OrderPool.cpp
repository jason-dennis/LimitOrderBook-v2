//
// Created by Ognean Jason Dennis on 17.07.2026.
//

#include "Memory/OrderPool.h"


OrderPool::OrderPool(){
    Storage_.resize(DefaultCapacity);
    FreeIndex_.reserve(DefaultCapacity);
    for(uint32_t index = 0; index < DefaultCapacity; ++index){
        FreeIndex_.push_back(index);
    }
}

uint32_t OrderPool::Allocate() {
    if(FreeIndex_.empty()){
        return UINT32_MAX;
    }
    auto index = FreeIndex_.back();
    FreeIndex_.pop_back();
    return index;
}

void OrderPool::Free(uint32_t index) {
    FreeIndex_.push_back(index);
}

OrderPod& OrderPool::Get(uint32_t index) {
    return Storage_[index];
}

