//
// Created by Ognean Jason Dennis on 17.07.2026.
//

#ifndef LIMITORDERBOOK_ORDERPOD_H
#define LIMITORDERBOOK_ORDERPOD_H
#include <cstdint>
#include "Domain/order.h"

struct OrderPod {
    uint64_t Price;
    uint64_t Timestamp;
    uint32_t OrderId;
    uint32_t TraderId;
    uint32_t Quantity;
    uint32_t Symbol;
    OrderSide Side;
    OrderType Type;
    TimeInForce TIF;
    OrderStatus Status;
};

#endif //LIMITORDERBOOK_ORDERPOD_H
