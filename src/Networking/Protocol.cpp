//
// Created by Ognean Jason Dennis on 15.07.2026.
//
#include "Networking/Protocol.h"

namespace {
    constexpr int Tick = 100;
}
// Encode
std::vector<uint8_t> EncodeUint64(uint64_t x){
    std::vector<uint8_t> bytes;

    { // Encode High Register
        uint32_t high64 = (uint32_t) (x >> 32);
        bytes.push_back((uint8_t)((high64 >> 24) & 0xFF));
        bytes.push_back((uint8_t)((high64>> 16) & 0xFF));
        bytes.push_back((uint8_t)((high64 >> 8) & 0xFF));
        bytes.push_back((uint8_t)((high64) & 0xFF));
    }

    { // Encode Low Register
        uint32_t low64 = (uint32_t) (x & 0xFFFFFFFF);
        bytes.push_back((uint8_t)((low64 >> 24) & 0xFF));
        bytes.push_back((uint8_t)((low64 >> 16) & 0xFF));
        bytes.push_back((uint8_t)((low64 >> 8) & 0xFF));
        bytes.push_back((uint8_t)((low64) & 0xFF));
    }

    return bytes;
}

std::vector<uint8_t> EncodeUint32(uint32_t x){
    std::vector<uint8_t> bytes;
    bytes.push_back((uint8_t)((x >> 24) & 0xFF));
    bytes.push_back((uint8_t)((x >> 16) & 0xFF));
    bytes.push_back((uint8_t)((x >> 8) & 0xFF));
    bytes.push_back((uint8_t)((x) & 0xFF));

    return bytes;
}

std::vector<uint8_t> EncodeSymbol(const std::string& Symbol){
    std::vector<uint8_t> bytes;
    uint8_t length = Symbol.size();
    bytes.push_back(length);
    for(auto character : Symbol){
        bytes.push_back(character);
    }
    return bytes;
}

std::vector<uint8_t> Encode(const OrderRequest& request){
    // Type, Side, Price, Quantity, TraderID, TIF, SymbolLength, Symbol
    std::vector<uint8_t> bytes;

    { // Encode Type
        auto Type = request.Type;
        uint8_t type_byte;
        if(Type == "LIMIT")
            type_byte = 0;
        else
            type_byte = 1;
        bytes.push_back(type_byte);
    }

    { // Encode Side
        auto Side = request.Side;
        uint8_t side_byte;
        if(Side == "BUY")
            side_byte = 0;
        else
            side_byte = 1;
        bytes.push_back(side_byte);
    }

    { // Encode Price
        uint64_t Price = (uint64_t)(request.Price * Tick);
        auto price_bytes = EncodeUint64(Price);
        for(auto price_byte : price_bytes)
            bytes.push_back(price_byte);
    }

    { // Encode Quantity
        uint32_t Quantity = (uint32_t)(request.Quantity);
        auto quantity_bytes = EncodeUint32(Quantity);
        for(auto quantity_byte : quantity_bytes)
            bytes.push_back(quantity_byte);
    }

    { // Encode TraderID
        uint32_t TraderID = (uint32_t)(request.TraderID);
        auto traderid_bytes = EncodeUint32(TraderID);
        for(auto trederid_byte : traderid_bytes)
            bytes.push_back(trederid_byte);
    }

    { // Encode TIF
        auto TIF = request.TIF;
        uint8_t tif_byte;
        if(TIF == "GTC")
            tif_byte = 0;
        else
            tif_byte = 1;
        bytes.push_back(tif_byte);
    }

    { // Encode Symbol
        auto Symbol = request.Symbol;
        auto symbol_bytes = EncodeSymbol(Symbol);
        for(auto symbol_byte : symbol_bytes)
            bytes.push_back(symbol_byte);
    }
    return bytes;
}

// Decode
uint64_t DecodeUint64(const std::vector<uint8_t>& bytes){
    uint64_t x = 0;
    for(auto byte : bytes){
        x <<= 8;
        x |= byte;
    }
    return x;
}

uint32_t DecodeUint32(const std::vector<uint8_t>& bytes){
    uint32_t x = 0;
    for(auto byte : bytes){
        x <<= 8;
        x |= byte;
    }
    return x;
}

std::optional<std::string> DecodeSymbol(const std::vector<uint8_t>& bytes){
    if(bytes.empty()) return std::nullopt;

    auto length = bytes[0];
    if(bytes.size() < length + 1) return std::nullopt;

    std::string symbol = "";
    for(int i = 1; i <= length; ++i){
        symbol.push_back(bytes[i]);
    }

    return symbol;
}

std::optional<OrderRequest> Decode(const std::vector<uint8_t>& bytes){
// Type, Side, Price, Quantity, TraderID, TIF, SymbolLength, Symbol
    if(bytes.size() < 20) return std::nullopt;

    int index = 0;
    auto type_byte = bytes[index++];
    auto side_byte = bytes[index++];
    auto price_bytes = {bytes[index], bytes[index+1],
                        bytes[index+2], bytes[index+3],bytes[index+4],
                        bytes[index+5],bytes[index+6],bytes[index+7]};
    index += 8;

    auto quantity_bytes = {bytes[index],bytes[index+1],
                           bytes[index+2],bytes[index+3]};
    index += 4;
    auto traderid_bytes = {bytes[index],bytes[index+1],
                           bytes[index+2],bytes[index+3]};
    index += 4;
    auto tif_byte = bytes[index++];
    std::vector<uint8_t> symbol_bytes(bytes.begin() + index,bytes.end());

    OrderRequest request;

    request.Type = (type_byte == 0 ? "LIMIT" : "MARKET");
    request.Side = (side_byte == 0 ? "BUY" : "SELL");
    request.Price = static_cast<float>(DecodeUint64(price_bytes)) / Tick;
    request.Quantity = DecodeUint32(quantity_bytes);
    request.TraderID = DecodeUint32(traderid_bytes);
    request.TIF = (tif_byte == 0 ? "GTC" : "IOC");
    auto Symbol = DecodeSymbol(symbol_bytes);
    if(Symbol == std::nullopt) return std::nullopt;
    request.Symbol = *Symbol;

    return request;
}