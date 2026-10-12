//
// Created by denni on 10/10/2026.
//
#include <chrono>
#include <random>
#include <vector>
#include <algorithm>
#include "Domain/order.h"
#include "Engine/MatchingEngine.h"
#include "Storage/BinaryOrderBookStorage.h"
#include "Memory/OrderPool.h"

enum class Type{ ADD, CANCEL, MARKET};
struct operation {
    Type type;
    OrderSide side;
    uint32_t price;
    uint32_t qnty;
    uint32_t id;
};
int main() {
   auto pool = std::make_unique<OrderPool>();
   auto book = std::make_unique<BinaryOrderBook>(*pool);
   std::atomic<int> TradeCounter{0};
   auto matching_engine = std::make_unique<MatchingEngine<BinaryOrderBook>>(*book,TradeCounter, *pool);


   std::mt19937 rng(2023);
   std::uniform_int_distribution<int> sideDist(0, 1);
   std::uniform_int_distribution<int> roleDist(0, 99);
   std::uniform_int_distribution<int> takerQtyDist(1, 10);
   std::uniform_int_distribution<int> makerQtyDist(10, 60);
   std::geometric_distribution<int>tickDist(0.3);


}