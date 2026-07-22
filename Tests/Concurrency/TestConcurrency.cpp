//
// Created by Ognean Jason Dennis on 13.07.2026.
//

#include <gtest/gtest.h>
#include <thread>
#include <future>
#include <chrono>
#include <vector>
#include <set>
#include <atomic>

#include "Concurrency/OrderQueue.h"

using namespace std::chrono_literals;

namespace {

    OrderRequest MakeRequest(int traderId) {
        OrderRequest req;
        req.Price = 100.0f;
        req.Quantity = 10;
        req.Type = "LIMIT";
        req.Symbol = "AAPL";
        req.TIF = "GTC";
        req.TraderID = traderId;
        req.Side = "BUY";
        return req;
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// Black-box: comportament de bază (FIFO, blocking, shutdown)
// ══════════════════════════════════════════════════════════════

TEST(OrderQueueTest, PushThenPopReturnsSameOrder) {
    OrderQueue queue;
    queue.Push(MakeRequest(1));

    auto result = queue.Pop();

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->TraderID, 1);
    EXPECT_EQ(result->Price, 100.0f);
    EXPECT_EQ(result->Quantity, 10);
    EXPECT_EQ(result->Symbol, "AAPL");
}

TEST(OrderQueueTest, PreservesFIFOOrder) {
    OrderQueue queue;
    queue.Push(MakeRequest(1));
    queue.Push(MakeRequest(2));
    queue.Push(MakeRequest(3));

    auto first = queue.Pop();
    auto second = queue.Pop();
    auto third = queue.Pop();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(third.has_value());
    EXPECT_EQ(first->TraderID, 1);
    EXPECT_EQ(second->TraderID, 2);
    EXPECT_EQ(third->TraderID, 3);
}

TEST(OrderQueueTest, PopBlocksWhenQueueIsEmpty) {
    OrderQueue queue;

    auto future = std::async(std::launch::async, [&queue]() {
        return queue.Pop();
    });

    auto status = future.wait_for(100ms);
    EXPECT_EQ(status, std::future_status::timeout)
                        << "Pop() a returnat pe o coada goala, in loc sa blocheze";

    queue.Push(MakeRequest(42));

    status = future.wait_for(1s);
    ASSERT_EQ(status, std::future_status::ready)
                                << "Pop() nu s-a deblocat dupa push(), posibil deadlock";

    auto result = future.get();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->TraderID, 42);
}

TEST(OrderQueueTest, ShutdownUnblocksWaitingPop) {
    OrderQueue queue;

    auto future = std::async(std::launch::async, [&queue]() {
        return queue.Pop();
    });

    auto status = future.wait_for(100ms);
    EXPECT_EQ(status, std::future_status::timeout);

    queue.Shutdown();

    status = future.wait_for(1s);
    ASSERT_EQ(status, std::future_status::ready)
                                << "Pop() nu s-a deblocat dupa shutdown(), worker thread ar ramane agatat";

    auto result = future.get();
    EXPECT_FALSE(result.has_value())
                        << "Pop() dupa shutdown pe coada goala ar trebui sa intoarca nullopt";
}

TEST(OrderQueueTest, PopReturnsNulloptImmediatelyIfAlreadyShutdownAndEmpty) {
    OrderQueue queue;
    queue.Shutdown();

    // Nu trebuie sa blocheze deloc, coada e goala si shutdown deja activ.
    auto future = std::async(std::launch::async, [&queue]() {
        return queue.Pop();
    });

    auto status = future.wait_for(200ms);
    ASSERT_EQ(status, std::future_status::ready)
                                << "Pop() a blocat desi shutdown era deja activ pe coada goala";

    EXPECT_FALSE(future.get().has_value());
}

TEST(OrderQueueTest, ShutdownDoesNotDiscardRemainingMessages) {
    OrderQueue queue;
    queue.Push(MakeRequest(1));
    queue.Push(MakeRequest(2));
    queue.Shutdown();

    // Mesajele deja aflate in coada la momentul shutdown-ului tot trebuie livrate.
    auto first = queue.Pop();
    auto second = queue.Pop();
    auto third = queue.Pop();

    ASSERT_TRUE(first.has_value());
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(first->TraderID, 1);
    EXPECT_EQ(second->TraderID, 2);
    EXPECT_FALSE(third.has_value());
}

TEST(OrderQueueTest, ShutdownIsIdempotentUnderConcurrentCalls) {
    OrderQueue queue;

    constexpr int kCallers = 8;
    std::vector<std::thread> callers;
    for (int i = 0; i < kCallers; ++i) {
        callers.emplace_back([&queue]() { queue.Shutdown(); });
    }
    for (auto& t : callers) t.join();

    // Daca shutdown-ul concurent a coruput ceva, testul ar crapa sau ar bloca aici.
    auto result = queue.Pop();
    EXPECT_FALSE(result.has_value());
}

// ══════════════════════════════════════════════════════════════
// Black-box: stress concurrent (multi-producer, multi-consumer)
// Gandite explicit pentru rulare sub ThreadSanitizer.
// ══════════════════════════════════════════════════════════════

TEST(OrderQueueTest, MultiProducerSingleConsumerNoLossNoDuplication) {
    OrderQueue queue;
    constexpr int kProducers = 8;
    constexpr int kPerProducer = 2000;
    constexpr int kTotal = kProducers * kPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, p]() {
            for (int i = 0; i < kPerProducer; ++i) {
                queue.Push(MakeRequest(p * kPerProducer + i));
            }
        });
    }

    std::set<int> seenIds;
    for (int i = 0; i < kTotal; ++i) {
        auto result = queue.Pop();
        ASSERT_TRUE(result.has_value());
        auto [it, inserted] = seenIds.insert(result->TraderID);
        EXPECT_TRUE(inserted) << "TraderID duplicat: " << result->TraderID;
    }

    for (auto& t : producers) t.join();
    EXPECT_EQ(seenIds.size(), static_cast<size_t>(kTotal));
}

TEST(OrderQueueTest, MultiProducerMultiConsumerNoLossNoDuplication) {
    OrderQueue queue;
    constexpr int kProducers = 6;
    constexpr int kConsumers = 4;
    constexpr int kPerProducer = 1500;
    constexpr int kTotal = kProducers * kPerProducer;

    // Fiecare consumator isi tine propria lista locala -> zero curse pe
    // structura de rezultate in timpul testului, se combina abia dupa join.
    std::vector<std::vector<int>> consumerResults(kConsumers);

    std::vector<std::thread> consumers;
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&queue, &consumerResults, c]() {
            while (auto msg = queue.Pop()) {
                consumerResults[c].push_back(msg->TraderID);
            }
        });
    }

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
    for (auto& t : consumers) t.join();

    std::set<int> seenIds;
    size_t totalReceived = 0;
    for (auto& results : consumerResults) {
        totalReceived += results.size();
        for (int id : results) {
            auto [it, inserted] = seenIds.insert(id);
            EXPECT_TRUE(inserted) << "TraderID duplicat: " << id;
        }
    }

    EXPECT_EQ(totalReceived, static_cast<size_t>(kTotal));
    EXPECT_EQ(seenIds.size(), static_cast<size_t>(kTotal));
}

TEST(OrderQueueTest, PerProducerRelativeOrderIsPreservedUnderConcurrency) {
    OrderQueue queue;
    constexpr int kProducers = 6;
    constexpr int kPerProducer = 500;
    constexpr int kTotal = kProducers * kPerProducer;

    // Codificam producatorul si secventa in TraderID: producer * 100000 + seq.
    // Push-ul e protejat de un singur mutex, deci ordinea de push a UNUI
    // producator (in raport cu el insusi) trebuie sa se pastreze in coada,
    // chiar daca interclasarea intre producatori diferiti e nedeterminista.
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, p]() {
            for (int seq = 0; seq < kPerProducer; ++seq) {
                queue.Push(MakeRequest(p * 100000 + seq));
            }
        });
    }

    std::vector<int> lastSeqPerProducer(kProducers, -1);
    for (int i = 0; i < kTotal; ++i) {
        auto result = queue.Pop();
        ASSERT_TRUE(result.has_value());

        int producer = result->TraderID / 100000;
        int seq = result->TraderID % 100000;

        ASSERT_GT(seq, lastSeqPerProducer[producer])
                                    << "Ordinea in cadrul producatorului " << producer << " a fost incalcata";
        lastSeqPerProducer[producer] = seq;
    }

    for (auto& t : producers) t.join();
}

// ══════════════════════════════════════════════════════════════
// White-box: verifica starea interna prin Size()
// Necesita metoda publica OrderQueue::Size() (thread-safe),
// pe care o implementezi tu: lock_guard + return MyQueue.size().
// ══════════════════════════════════════════════════════════════

TEST(OrderQueueWhiteBoxTest, SizeReflectsPushAndPop) {
    OrderQueue queue;
    EXPECT_EQ(queue.Size(), 0u);

    queue.Push(MakeRequest(1));
    queue.Push(MakeRequest(2));
    queue.Push(MakeRequest(3));
    EXPECT_EQ(queue.Size(), 3u);

    queue.Pop();
    EXPECT_EQ(queue.Size(), 2u);

    queue.Pop();
    queue.Pop();
    EXPECT_EQ(queue.Size(), 0u);
}

TEST(OrderQueueWhiteBoxTest, SizeIsZeroAfterFullConcurrentDrain) {
    OrderQueue queue;
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 1000;
    constexpr int kTotal = kProducers * kPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, p]() {
            for (int i = 0; i < kPerProducer; ++i) {
                queue.Push(MakeRequest(p * kPerProducer + i));
            }
        });
    }
    for (auto& t : producers) t.join();

    EXPECT_EQ(queue.Size(), static_cast<size_t>(kTotal));

    for (int i = 0; i < kTotal; ++i) {
        queue.Pop();
    }

    EXPECT_EQ(queue.Size(), 0u);
}