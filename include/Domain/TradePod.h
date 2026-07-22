//
// Created by Ognean Jason Dennis on 17.07.2026.
//

#ifndef LIMITORDERBOOK_TRADEPOD_H
#define LIMITORDERBOOK_TRADEPOD_H
#include <cstdint>

struct TradePod {
    uint64_t Price;
    uint64_t Timestamp;
    uint32_t TradeId;
    uint32_t MakerId;
    uint32_t TakerId;
    uint32_t Quantity;
    uint32_t Symbol;
};
#endif //LIMITORDERBOOK_TRADEPOD_H
