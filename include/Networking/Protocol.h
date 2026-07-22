//
// Created by Ognean Jason Dennis on 15.07.2026.
//

#ifndef LIMITORDERBOOK_PROTOCOL_H
#define LIMITORDERBOOK_PROTOCOL_H
#include "Concurrency/OrderQueue.h"
#include <vector>
#include <cstdint>
#include <optional>

std::vector<uint8_t> Encode(const OrderRequest& request);
std::optional<OrderRequest> Decode(const std::vector<uint8_t>& bytes);
std::vector<uint8_t> EncodeUint64(uint64_t x);
uint64_t DecodeUint64(const std::vector<uint8_t>& bytes);

std::vector<uint8_t> EncodeUint32(uint32_t x);
uint32_t DecodeUint32(const std::vector<uint8_t>& bytes);

std::vector<uint8_t> EncodeSymbol(const std::string& Symbol);
std::optional<std::string> DecodeSymbol(const std::vector<uint8_t>& bytes);
#endif //LIMITORDERBOOK_PROTOCOL_H
