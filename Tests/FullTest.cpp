//
// Created by Ognean Jason Dennis on 21.07.2026.
//
// Load test sustinut, de sine statator. Generator AGRESIV -- loveste
// mereu cel mai bun pret disponibil de partea opusa, garantand matching
// imediat cand exista lichiditate. Cand o parte se goleste, urmatoarea
// ordine de acea parte devine automat "maker" (cantitate mare, pret fix),
// realimentand lichiditatea fara nicio anulare explicita.
//
// Rulare: ./FullTest [durata_in_secunde] (implicit 75)
//


#include <iostream>
#include <iomanip>
#include <thread>
#include <atomic>
#include <vector>
#include <random>
#include <chrono>
#include <cstdio>

#include "Engine/CoreEngine.h"

using namespace std::chrono_literals;

namespace {

    constexpr int kSymbolCount = 8;
    constexpr int kGeneratorThreadsPerSymbol = 1;
    constexpr int kTargetBookDepth = 2000;     // adancime tinta per simbol
    constexpr int kCancelCheckInterval = 500;  // la cate iteratii verificam adancimea
    constexpr int kMaxCancelsPerCheck = 200;

    struct SymbolTraffic {
        std::string symbol;
        std::atomic<long long> ordersSubmitted{0};
        std::atomic<long long> cancelsSubmitted{0};
    };

    void GeneratorLoop(CoreEngine& engine, SymbolTraffic& traffic,
                       std::atomic<bool>& running, int generatorId) {
        std::mt19937 rng(std::random_device{}() + generatorId);
        std::uniform_int_distribution<int> sideDist(0, 1);
        std::uniform_int_distribution<int> roleDist(0, 99);      // 0-74 = taker MARKET, 75-99 = maker
        std::uniform_int_distribution<int> takerQtyDist(1, 10);
        std::uniform_int_distribution<int> makerQtyDist(10, 60);
        // makerii coteaza IN JURUL spread-ului -- BUY sub mid, SELL peste mid,
        // pe ~500 tick-uri fiecare parte: adancime reala, multi-nivel
        std::uniform_int_distribution<int> bidTickDist(9500, 9999);   // 95.00 - 99.99
        std::uniform_int_distribution<int> askTickDist(10001, 10500); // 100.01 - 105.00

        int localTraderId = generatorId * 10'000'000;
        int itersSinceCheck = 0;

        while (running.load(std::memory_order_relaxed)) {
            bool isBuy = (sideDist(rng) == 0);
            bool isTaker = (roleDist(rng) < 75); // 75% taker, 25% maker

            if (isTaker) {
                // MARKET -- consuma de la cel mai bun pret REAL la momentul
                // procesarii; nu ramane niciodata in carte
                int quantity = takerQtyDist(rng) +1;
                engine.CreateOrder(0.0f, quantity, "MARKET", traffic.symbol, "GTC",
                                   localTraderId++, isBuy ? "BUY" : "SELL");
            } else {
                float price = isBuy
                              ? static_cast<float>(bidTickDist(rng)) / 100.0f
                              : static_cast<float>(askTickDist(rng)) / 100.0f;
                int quantity = makerQtyDist(rng) +1;
                engine.CreateOrder(price, quantity, "LIMIT", traffic.symbol,
                                   "GTC", localTraderId++, isBuy ? "BUY" : "SELL");
            }

            traffic.ordersSubmitted.fetch_add(1, std::memory_order_relaxed);

            // "termostat" de anulare -- exact ce fac pietele reale: majoritatea
            // ordinelor odihnite se ANULEAZA, nu se executa. Mentine adancimea
            // in jurul tintei, auto-reglat, indiferent de deriva raporturilor.
            if (++itersSinceCheck >= kCancelCheckInterval) {
                itersSinceCheck = 0;
                auto resting = engine.GetOrders(traffic.symbol);
                int excess = static_cast<int>(resting.size()) - kTargetBookDepth;
                if (excess > 0) {
                    int toCancel = std::min(excess, kMaxCancelsPerCheck);
                    std::uniform_int_distribution<size_t> pick(0, resting.size() - 1);
                    for (int i = 0; i < toCancel; ++i) {
                        const auto& victim = resting[pick(rng)];
                        engine.CancelOrder(victim.GetOrderID(), traffic.symbol);
                        traffic.cancelsSubmitted.fetch_add(1, std::memory_order_relaxed);
                    }
                }
            }
        }
    }

} // namespace

int main(int argc, char** argv) {
    int durationSeconds = 75;
    if (argc > 1) {
        durationSeconds = std::atoi(argv[1]);
    }

    std::remove("orders.csv");

    std::cout << "=== Load Test Sustinut (generator agresiv) ===\n";
    std::cout << "Durata: " << durationSeconds << " secunde\n";
    std::cout << "Simboluri: " << kSymbolCount
              << " (" << kGeneratorThreadsPerSymbol << " generator/simbol)\n";
    std::cout << "Pornire...\n\n";

    CoreEngine engine;

    std::vector<SymbolTraffic> traffics(kSymbolCount);
    for (int s = 0; s < kSymbolCount; ++s) {
        traffics[s].symbol = "LOAD_SYM_" + std::to_string(s);
    }

    std::atomic<bool> running{true};
    std::vector<std::thread> generators;
    int generatorId = 0;
    for (int s = 0; s < kSymbolCount; ++s) {
        for (int g = 0; g < kGeneratorThreadsPerSymbol; ++g) {
            generators.emplace_back(GeneratorLoop, std::ref(engine),
                                    std::ref(traffics[s]), std::ref(running),
                                    generatorId++);
        }
    }

    auto startTime = std::chrono::steady_clock::now();

    for (int elapsed = 10; elapsed <= durationSeconds; elapsed += 10) {
        std::this_thread::sleep_for(10s);
        long long totalSoFar = 0;
        long long totalRestingSoFar = 0;
        for (auto& t : traffics) totalSoFar += t.ordersSubmitted.load();
        for (auto& t : traffics) totalRestingSoFar += static_cast<long long>(engine.GetOrders(t.symbol).size());
        std::cout << "[" << elapsed << "s] Ordine trimise: " << totalSoFar
                  << "  |  ramase in carte acum: " << totalRestingSoFar << "\n";
    }
    int remainder = durationSeconds % 10;
    if (remainder > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(remainder));
    }

    auto endTimeGeneration = std::chrono::steady_clock::now();

    running.store(false, std::memory_order_relaxed);
    for (auto& t : generators) t.join();

    long long totalOrdersSubmitted = 0;
    long long totalCancelsSubmitted = 0;
    for (auto& t : traffics) totalOrdersSubmitted += t.ordersSubmitted.load();
    for (auto& t : traffics) totalCancelsSubmitted += t.cancelsSubmitted.load();

    std::cout << "\nGenerare oprita, astept finalizarea procesarii...\n";
    std::this_thread::sleep_for(2s);

    double generationElapsedSec =
            std::chrono::duration<double>(endTimeGeneration - startTime).count();

    long long totalMatches = 0;
    long long totalVolume = 0;
    long long totalRemainingInBook = 0;
    for (auto& traffic : traffics) {
        auto trades = engine.GetTradesHistory(traffic.symbol);
        totalMatches += static_cast<long long>(trades.size());
        for (const auto& trade : trades) totalVolume += trade.GetQuantity();
        totalRemainingInBook += static_cast<long long>(engine.GetOrders(traffic.symbol).size());
    }

    std::cout << "\n=== REZULTATE (masurate pe " << std::fixed << std::setprecision(1)
              << generationElapsedSec << "s de generare sustinuta) ===\n";
    std::cout << "Orders/sec:  " << std::fixed << std::setprecision(1)
              << (totalOrdersSubmitted / generationElapsedSec) << "/s"
              << "  (total: " << totalOrdersSubmitted << ")\n";
    std::cout << "Matches/sec: "
              << (totalMatches / generationElapsedSec) << "/s"
              << "  (total: " << totalMatches << ")\n";
    std::cout << "Volume/sec:  "
              << (totalVolume / generationElapsedSec) << "/s"
              << "  (total: " << totalVolume << ")\n";
    std::cout << "Cancels/sec: "
              << (totalCancelsSubmitted / generationElapsedSec) << "/s"
              << "  (total: " << totalCancelsSubmitted << ")\n";
    std::cout << "Ordine ramase in carte la final: " << totalRemainingInBook
              << "  (ar trebui sa fie MIC, nu saturat la capacitate)\n";

    engine.Shutdown();
    std::remove("orders.csv");

    return 0;
}