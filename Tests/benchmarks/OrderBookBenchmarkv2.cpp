
// Created by Ognean Jason Dennis on 21.07.2026.
//
// Benchmark v2: Multiset vs Binary, pe doua niveluri.
//
// 1. Componenta izolata -- AddOrder, GetBestBid, CanFillQuantity, direct
//    pe storage, cu ciclu alocare->adaugare->eliberare (necesar acum,
//    spre deosebire de vechiul benchmark cu shared_ptr, pentru ca
//    OrderPool are capacitate FIXA -- nu poti creste cartea nelimitat).
//
// 2. Pipeline complet -- prin MatchingEngine<StorageType> real, exact
//    calea de productie (fara Router/TCP/threading, doar engine-ul
//    single-threaded, cum ruleaza de fapt in productie per simbol).
//    Doua scenarii: ordine care raman in carte (GTC resting) si
//    ordine care se matcheaza imediat (matching throughput real).
//

#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <chrono>

#include "Memory/OrderPool.h"
#include "Storage/MultisetOrderBookStorage.h"
#include "Storage/BinaryOrderBookStorage.h"
#include "Engine/MatchingEngine.h"
#include "Domain/Time.h"

namespace {

    std::mt19937 rng(42);

    uint64_t RandPrice(uint64_t mid = 10'000, uint64_t spread = 5'000) {
        std::uniform_int_distribution<uint64_t> dist(mid - spread, mid + spread);
        return dist(rng);
    }

    void PopulateOrder(OrderPod& order, uint32_t traderId, uint64_t price,
                       uint32_t quantity, OrderSide side) {
        order.Price = price;
        order.Timestamp = NowTimestamp();
        order.OrderId = traderId;
        order.TraderId = traderId;
        order.Quantity = quantity;
        order.Symbol = 1;
        order.Side = side;
        order.Type = OrderType::LIMIT;
        order.TIF = TimeInForce::GTC;
        order.Status = OrderStatus::NEW;
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// 1a. AddOrder -- ciclu alocare/adaugare/eliberare, mentine
// dimensiunea cartii constanta.
// ══════════════════════════════════════════════════════════════

template <typename StorageType>
void BM_AddOrder(benchmark::State& state) {
    int steadyDepth = static_cast<int>(state.range(0));
    OrderPool pool;
    StorageType book(pool);

    for (int i = 0; i < steadyDepth; ++i) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), i, RandPrice(), 100, OrderSide::BUY);
        book.AddOrder(index);
    }

    int traderId = steadyDepth;
    for (auto _ : state) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), traderId++, RandPrice(), 100, OrderSide::BUY);
        book.AddOrder(index);
        book.PopBestBid();
    }

    state.counters["Ops/sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK_TEMPLATE(BM_AddOrder, MultisetOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_AddOrder, BinaryOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);

// ══════════════════════════════════════════════════════════════
// 1b. GetBestBid -- doar citire.
// ══════════════════════════════════════════════════════════════

template <typename StorageType>
void BM_GetBestBid(benchmark::State& state) {
    int depth = static_cast<int>(state.range(0));
    OrderPool pool;
    StorageType book(pool);

    for (int i = 0; i < depth; ++i) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), i, RandPrice(), 100, OrderSide::BUY);
        book.AddOrder(index);
    }

    for (auto _ : state) {
        auto result = book.GetBestBid();
        benchmark::DoNotOptimize(result);
    }

    state.counters["Ops/sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK_TEMPLATE(BM_GetBestBid, MultisetOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_GetBestBid, BinaryOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);

// ══════════════════════════════════════════════════════════════
// 1c. CanFillQuantityBids -- doar citire.
// ══════════════════════════════════════════════════════════════

template <typename StorageType>
void BM_CanFillQuantityBids(benchmark::State& state) {
    int depth = static_cast<int>(state.range(0));
    OrderPool pool;
    StorageType book(pool);

    for (int i = 0; i < depth; ++i) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), i, RandPrice(), 100, OrderSide::BUY);
        book.AddOrder(index);
    }

    for (auto _ : state) {
        auto result = book.CanFillQuantityBids(500, 10'000);
        benchmark::DoNotOptimize(result);
    }

    state.counters["Ops/sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK_TEMPLATE(BM_CanFillQuantityBids, MultisetOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_CanFillQuantityBids, BinaryOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);

// ══════════════════════════════════════════════════════════════
// 2a. Pipeline complet, prin MatchingEngine<StorageType> real --
// ordine GTC care raman in carte, ciclu add+pop.
// ══════════════════════════════════════════════════════════════

template <typename StorageType>
void BM_Pipeline_RestingOrders(benchmark::State& state) {
    int steadyDepth = static_cast<int>(state.range(0));
    OrderPool pool;
    StorageType book(pool);
    std::atomic<int> tradeCounter{1};
    MatchingEngine<StorageType> engine(book, tradeCounter, pool);

    for (int i = 0; i < steadyDepth; ++i) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), i, RandPrice(1000, 500), 100, OrderSide::BUY);
        std::vector<TradePod> trades;
        engine.ProcessOrder(index, trades);
    }

    int traderId = steadyDepth;
    for (auto _ : state) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), traderId++, RandPrice(1000, 500), 100, OrderSide::BUY);
        std::vector<TradePod> trades;
        engine.ProcessOrder(index, trades);
        book.PopBestBid();
    }

    state.counters["Ops/sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK_TEMPLATE(BM_Pipeline_RestingOrders, MultisetOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Pipeline_RestingOrders, BinaryOrderBook)->Range(1000, 90000)->Unit(benchmark::kMicrosecond);

// ══════════════════════════════════════════════════════════════
// 2b. Pipeline complet -- matching REAL, imediat, contra
// lichiditate uriasa pre-semanata (nu se epuizeaza).
// ══════════════════════════════════════════════════════════════

template <typename StorageType>
void BM_Pipeline_ImmediateMatching(benchmark::State& state) {
    OrderPool pool;
    StorageType book(pool);
    std::atomic<int> tradeCounter{1};
    MatchingEngine<StorageType> engine(book, tradeCounter, pool);

    constexpr int kSeedOrders = 10;
    constexpr uint32_t kHugeQuantity = 10'000'000;
    for (int i = 0; i < kSeedOrders; ++i) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), i, 10'000, kHugeQuantity, OrderSide::SELL);
        std::vector<TradePod> trades;
        engine.ProcessOrder(index, trades);
    }

    int traderId = kSeedOrders;
    for (auto _ : state) {
        uint32_t index = pool.Allocate();
        PopulateOrder(pool.Get(index), traderId++, 10'000, 1, OrderSide::BUY);
        std::vector<TradePod> trades;
        engine.ProcessOrder(index, trades);
    }

    state.counters["Ops/sec"] = benchmark::Counter(state.iterations(), benchmark::Counter::kIsRate);
}
BENCHMARK_TEMPLATE(BM_Pipeline_ImmediateMatching, MultisetOrderBook)->Unit(benchmark::kMicrosecond);
BENCHMARK_TEMPLATE(BM_Pipeline_ImmediateMatching, BinaryOrderBook)->Unit(benchmark::kMicrosecond);
