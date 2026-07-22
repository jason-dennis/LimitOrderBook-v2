//
// Created by Ognean Jason Dennis on 21.07.2026.
//
// Benchmark multithreaded, prin sistemul COMPLET de productie
// (CoreEngine -> Router -> LockFreeQueue -> SymbolWorker ->
// MatchingEngine), nu microbenchmark izolat. Masoara exact cele
// trei metrici raportate uzual pentru matching engines:
// orders/sec, matches/sec, volume/sec.
//
// Doua scenarii, deliberat diferite:
//
// 1. Single-simbol, N producatori -- arata limita REALA a arhitecturii
//    single-thread-per-symbol: un singur worker proceseaza, indiferent
//    cati producatori trimit concurent (acelasi tipar gasit la v1
//    PipelineFixture -- throughput-ul NU creste liniar cu producatorii).
//
// 2. Multi-simbol, N producatori pe simboluri DIFERITE -- arata
//    beneficiul real al arhitecturii: N workeri independenti, paraleli,
//    fara nicio contentie intre ei. Aici throughput-ul ar trebui sa
//    scaleze aproape liniar cu numarul de simboluri/producatori.
//

#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdio>
#include <algorithm>

#include "Engine/CoreEngine.h"

namespace {

    void CleanPersistenceFile() {
        std::remove("orders.csv");
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// 1. Single-simbol, N producatori -- limita reala (un singur worker)
// ══════════════════════════════════════════════════════════════

void BM_MultiThreaded_SingleSymbol_Throughput(benchmark::State& state) {
    CleanPersistenceFile();
    int producerThreads = static_cast<int>(state.range(0));
    constexpr int kTotalOrders = 50000;

    long long cumulativeOrders = 0;
    long long cumulativeMatches = 0;
    long long cumulativeVolume = 0;

    for (auto _ : state) {
        state.PauseTiming();
        auto engine = std::make_unique<CoreEngine>();
        const std::string symbol = "BENCH_SINGLE";
        std::atomic<int> nextId{0};
        state.ResumeTiming();

        std::vector<std::thread> producers;
        for (int p = 0; p < producerThreads; ++p) {
            producers.emplace_back([&engine, &nextId, &symbol]() {
                int id;
                while ((id = nextId.fetch_add(1)) < kTotalOrders) {
                    bool isBuy = (id % 2 == 0);
                    engine->CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC", id,
                                        isBuy ? "BUY" : "SELL");
                }
            });
        }
        for (auto& t : producers) t.join();

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (!engine->GetOrders(symbol).empty()
               && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        state.PauseTiming();
        auto trades = engine->GetTradesHistory(symbol);
        cumulativeOrders += kTotalOrders;
        cumulativeMatches += static_cast<long long>(trades.size());
        for (const auto& t : trades) cumulativeVolume += t.GetQuantity();

        engine.reset();
        state.ResumeTiming();
    }

    state.counters["Orders_per_sec"] =
            benchmark::Counter(static_cast<double>(cumulativeOrders), benchmark::Counter::kIsRate);
    state.counters["Matches_per_sec"] =
            benchmark::Counter(static_cast<double>(cumulativeMatches), benchmark::Counter::kIsRate);
    state.counters["Volume_per_sec"] =
            benchmark::Counter(static_cast<double>(cumulativeVolume), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_MultiThreaded_SingleSymbol_Throughput)
->Arg(1)->Arg(2)->Arg(4)->Arg(8)
->Unit(benchmark::kMillisecond)
->UseRealTime();

// ══════════════════════════════════════════════════════════════
// 2. Multi-simbol, N producatori, fiecare pe simbolul lui dedicat --
// workeri independenti, paraleli, fara contentie intre ei.
// ══════════════════════════════════════════════════════════════

void BM_MultiThreaded_MultiSymbol_Throughput(benchmark::State& state) {
    CleanPersistenceFile();
    int symbolCount = static_cast<int>(state.range(0));
    constexpr int kTotalOrdersOverall = 50000;

    long long cumulativeOrders = 0;
    long long cumulativeMatches = 0;
    long long cumulativeVolume = 0;

    for (auto _ : state) {
        state.PauseTiming();
        auto engine = std::make_unique<CoreEngine>();
        std::vector<std::string> symbols;
        for (int s = 0; s < symbolCount; ++s) {
            symbols.push_back("BENCH_MULTI_" + std::to_string(s));
        }
        int perSymbol = kTotalOrdersOverall / std::max(1, symbolCount);
        state.ResumeTiming();

        std::vector<std::thread> producers;
        for (int s = 0; s < symbolCount; ++s) {
            producers.emplace_back([&engine, &symbols, s, perSymbol]() {
                const std::string& symbol = symbols[s];
                for (int i = 0; i < perSymbol; ++i) {
                    bool isBuy = (i % 2 == 0);
                    engine->CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC", i,
                                        isBuy ? "BUY" : "SELL");
                }
            });
        }
        for (auto& t : producers) t.join();

        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        while (std::chrono::steady_clock::now() < deadline) {
            bool allDrained = true;
            for (const auto& symbol : symbols) {
                if (!engine->GetOrders(symbol).empty()) { allDrained = false; break; }
            }
            if (allDrained) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        state.PauseTiming();
        for (const auto& symbol : symbols) {
            auto trades = engine->GetTradesHistory(symbol);
            cumulativeOrders += perSymbol;
            cumulativeMatches += static_cast<long long>(trades.size());
            for (const auto& t : trades) cumulativeVolume += t.GetQuantity();
        }

        engine.reset();
        state.ResumeTiming();
    }

    state.counters["Orders_per_sec"] =
            benchmark::Counter(static_cast<double>(cumulativeOrders), benchmark::Counter::kIsRate);
    state.counters["Matches_per_sec"] =
            benchmark::Counter(static_cast<double>(cumulativeMatches), benchmark::Counter::kIsRate);
    state.counters["Volume_per_sec"] =
            benchmark::Counter(static_cast<double>(cumulativeVolume), benchmark::Counter::kIsRate);
}
BENCHMARK(BM_MultiThreaded_MultiSymbol_Throughput)
->Arg(1)->Arg(2)->Arg(4)->Arg(8)
->Unit(benchmark::kMillisecond)
->UseRealTime();
