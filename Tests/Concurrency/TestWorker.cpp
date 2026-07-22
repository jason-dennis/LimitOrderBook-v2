//
// Created by Ognean Jason Dennis on 13.07.2026.
//

#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <chrono>
#include <vector>

#include "Engine/CoreEngine.h"
#include "Concurrency/OrderQueue.h"
#include "Concurrency/Worker.h"

using namespace std::chrono_literals;

namespace {

// Simbol dedicat testelor, ca sa nu se suprapuna cu orice ar fi
// incarcat de CoreEngine din orders.csv la constructie.
    const std::string kTestSymbol = "WORKERTEST";

    OrderRequest MakeRequest(int traderId, const std::string& symbol = kTestSymbol) {
        OrderRequest req;
        req.Price = 100.0f;
        req.Quantity = 10;
        req.Type = "LIMIT";
        req.Symbol = symbol;
        req.TIF = "GTC";
        req.TraderID = traderId;
        req.Side = "BUY";
        return req;
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// Worker: comportament de baza, cu CoreEngine real (nu mock).
// Verificarea se face indirect, prin GetOrders(Symbol).
// ══════════════════════════════════════════════════════════════

TEST(WorkerTest, ProcessesSingleMessageAndCreatesOneOrder) {
    CoreEngine engine;
    OrderQueue queue;

    queue.Push(MakeRequest(1));
    queue.Shutdown(); // nu mai vine nimic nou; run() proceseaza mesajul ramas si iese

    Worker worker(engine, queue);
    worker.run();

    EXPECT_EQ(engine.GetOrders(kTestSymbol).size(), 1u);
}

TEST(WorkerTest, ProcessesMultipleMessagesCreatesOrderPerMessage) {
    CoreEngine engine;
    OrderQueue queue;

    queue.Push(MakeRequest(1));
    queue.Push(MakeRequest(2));
    queue.Push(MakeRequest(3));
    queue.Shutdown();

    Worker worker(engine, queue);
    worker.run();

    EXPECT_EQ(engine.GetOrders(kTestSymbol).size(), 3u);
}

TEST(WorkerTest, StopsImmediatelyOnShutdownWithEmptyQueue) {
    CoreEngine engine;
    OrderQueue queue;
    queue.Shutdown();

    Worker worker(engine, queue);

    // Rulam pe thread separat, cu timeout: daca run() nu iese corect
    // cand nu mai e nimic de procesat, testul pica in loc sa ramana
    // agatat la infinit.
    auto future = std::async(std::launch::async, [&worker]() {
        worker.run();
    });

    auto status = future.wait_for(1s);
    ASSERT_EQ(status, std::future_status::ready)
                                << "run() nu s-a intors desi coada era goala si shutdown activ";

    EXPECT_EQ(engine.GetOrders(kTestSymbol).size(), 0u);
}

TEST(WorkerTest, UnblocksAndStopsWhenShutdownCalledWhileWaiting) {
    CoreEngine engine;
    OrderQueue queue;

    Worker worker(engine, queue);

    auto future = std::async(std::launch::async, [&worker]() {
        worker.run();
    });

    // worker-ul e pornit si asteapta blocat in Pop(), coada e goala
    std::this_thread::sleep_for(50ms);

    queue.Shutdown();

    auto status = future.wait_for(1s);
    ASSERT_EQ(status, std::future_status::ready)
                                << "run() nu s-a deblocat dupa shutdown(), ar ramane agatat la infinit";

    EXPECT_EQ(engine.GetOrders(kTestSymbol).size(), 0u);
}

// ══════════════════════════════════════════════════════════════
// Worker: integrare reala sub concurenta (producatori multipli +
// worker rulat efectiv pe thread propriu, engine real).
// ══════════════════════════════════════════════════════════════

TEST(WorkerTest, ProcessesAllMessagesFromConcurrentProducers) {
    CoreEngine engine;
    OrderQueue queue;

    constexpr int kProducers = 4;
    constexpr int kPerProducer = 500;
    constexpr int kTotal = kProducers * kPerProducer;

    Worker worker(engine, queue);
    auto workerFuture = std::async(std::launch::async, [&worker]() {
        worker.run();
    });

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, p]() {
            for (int i = 0; i < kPerProducer; ++i) {
                queue.Push(MakeRequest(p * kPerProducer + i));
            }
        });
    }
    for (auto& t : producers) t.join();

    queue.Shutdown();

    auto status = workerFuture.wait_for(5s);
    ASSERT_EQ(status, std::future_status::ready)
                                << "run() nu s-a terminat dupa shutdown, la finalul producatorilor";

    EXPECT_EQ(engine.GetOrders(kTestSymbol).size(), static_cast<size_t>(kTotal));
}