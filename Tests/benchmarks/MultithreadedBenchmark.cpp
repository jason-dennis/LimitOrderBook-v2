//
// Created by Ognean Jason Dennis on 21.07.2026.
//
#include <benchmark/benchmark.h>
#include <thread>
#include <atomic>
#include <vector>
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <iostream>

#include "Engine/CoreEngine.h"

namespace {

    void CleanPersistenceFile() {
        std::remove("orders.csv");
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// 1. Single-symbol saturation -- un singur worker, N producatori
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
        ->UseRealTime()
        ->Repetitions(5)
        ->ReportAggregatesOnly(true);

// ══════════════════════════════════════════════════════════════
// 2. Multi-symbol scaling -- producatori FICSI (2), decuplati de
// numarul de simboluri; testat in buget (pana la 6 simboluri, cu
// 2 producatori = 8 threaduri totale, exact la limita masinii) si
// peste buget (8 simboluri = 10 threaduri, suprasolicitare explicita).
// ══════════════════════════════════════════════════════════════

void BM_MultiThreaded_MultiSymbol_Throughput(benchmark::State& state) {
    CleanPersistenceFile();
    int symbolCount = static_cast<int>(state.range(0));
    constexpr int kOrdersPerSymbolFixed = 6250;
    constexpr int kFixedProducerCount = 2;

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
        int totalOrders = kOrdersPerSymbolFixed * symbolCount;
        std::atomic<int> nextOrderIndex{0};
        state.ResumeTiming();

        std::vector<std::thread> producers;
        for (int p = 0; p < kFixedProducerCount; ++p) {
            producers.emplace_back([&engine, &symbols, &nextOrderIndex, totalOrders, symbolCount]() {
                int idx;
                while ((idx = nextOrderIndex.fetch_add(1)) < totalOrders) {
                    const std::string& symbol = symbols[idx % symbolCount];
                    // side-ul vine din RUNDA curenta (idx / symbolCount), nu din
                    // idx direct -- complet decuplat de simbol (fara corelatie
                    // de moduli), dar tot perfect alternant, deci balans EXACT
                    // garantat per simbol (necesar ca "carte goala" sa fie
                    // atins vreodata, nu doar statistic probabil)
                    bool isBuy = ((idx / symbolCount) % 2 == 0);
                    engine->CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC", idx,
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
            cumulativeOrders += kOrdersPerSymbolFixed;
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
// 1,2,4,6 -> 3-8 threaduri totale (2 producatori + N workeri), IN buget
// pe o masina de 8 nuclee. 8 -> 10 threaduri, suprasolicitare EXPLICITA,
// pastrat deliberat ca sa se vada exact unde incepe caderea.
BENCHMARK(BM_MultiThreaded_MultiSymbol_Throughput)
        ->Arg(1)->Arg(2)->Arg(4)->Arg(6)->Arg(8)
        ->Unit(benchmark::kMillisecond)
        ->UseRealTime()
        ->Repetitions(5)
        ->ReportAggregatesOnly(true);

// ══════════════════════════════════════════════════════════════
// 3. Round-trip latency percentiles -- un singur producator, un
// singur simbol, matching imediat garantat (lichiditate uriasa
// pre-semanata). Izoleaza costul real de procesare asincrona
// (submit -> Router -> Queue -> Worker -> match vizibil), fara
// nicio contentie de coada intre producatori multipli.
// ══════════════════════════════════════════════════════════════

void BM_OrderRoundTripLatency(benchmark::State& state) {
    CleanPersistenceFile();
    constexpr int kSampleCount = 20000;
    constexpr uint32_t kSeedQuantity = 1'000'000'000;

    for (auto _ : state) {
        state.PauseTiming();
        auto engine = std::make_unique<CoreEngine>();
        std::string symbol = "LATENCY_BENCH";
        // lichiditate uriasa, un singur nivel de pret -- fiecare BUY
        // MARKET urmator se matcheaza garantat, imediat, contra ei
        engine->CreateOrder(100.0f, static_cast<int>(kSeedQuantity), "LIMIT",
                            symbol, "GTC", 0, "SELL");
        std::vector<double> latenciesUs;
        latenciesUs.reserve(kSampleCount);
        state.ResumeTiming();

        for (int i = 1; i <= kSampleCount; ++i) {
            auto submitTime = std::chrono::steady_clock::now();
            engine->CreateOrder(100.0f, 1, "MARKET", symbol, "GTC", i, "BUY");

            // asteapta pana cand cantitatea ramasa a lichiditatii semanate
            // reflecta exact acest fill -- citire ieftina (un singur nivel
            // de pret), nu o copiere completa a istoricului de trade-uri
            int expectedRemaining = static_cast<int>(kSeedQuantity) - i;
            while (true) {
                auto asks = engine->GetBestAsks(1, symbol);
                if (!asks.empty() && asks[0].GetQuantity() == expectedRemaining) {
                    break;
                }
            }

            auto observedTime = std::chrono::steady_clock::now();
            double latencyUs = std::chrono::duration<double, std::micro>(
                    observedTime - submitTime).count();
            latenciesUs.push_back(latencyUs);
        }

        state.PauseTiming();
        std::sort(latenciesUs.begin(), latenciesUs.end());
        auto percentile = [&](double p) -> double {
            size_t idx = static_cast<size_t>(p * static_cast<double>(latenciesUs.size() - 1));
            return latenciesUs[idx];
        };

        state.counters["p50_us"] = percentile(0.50);
        state.counters["p90_us"] = percentile(0.90);
        state.counters["p99_us"] = percentile(0.99);
        state.counters["p999_us"] = percentile(0.999);
        state.counters["max_us"] = latenciesUs.back();

        engine.reset();
        state.ResumeTiming();
    }
}
BENCHMARK(BM_OrderRoundTripLatency)
        ->Unit(benchmark::kMicrosecond)
        ->Iterations(1)
        ->Repetitions(5)
        ->ReportAggregatesOnly(false); // aici vrem sa vedem fiecare rulare, nu doar media

// ══════════════════════════════════════════════════════════════
// main() custom -- raporteaza bugetul real de nuclee inainte de
// rulare, ca rezultatele sa poata fi interpretate corect (nu doar
// citite orb, fara context despre masina pe care au fost obtinute)
// ══════════════════════════════════════════════════════════════

int main(int argc, char** argv) {
    unsigned int cores = std::thread::hardware_concurrency();
    std::cout << "=== Hardware ===\n";
    std::cout << "Nuclee logice disponibile: " << cores << "\n";
    std::cout << "Nota: BM_MultiThreaded_MultiSymbol_Throughput foloseste 2 "
              << "producatori ficsi + N workeri (1 per simbol) -- la N > "
              << (cores > 2 ? cores - 2 : 0)
              << " simboluri, testul intra deliberat in regim de "
              << "suprasolicitare a nucleelor.\n\n";

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}