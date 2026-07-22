//
// Created by Ognean Jason Dennis on 19.07.2026.
//


#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <set>
#include <atomic>

#include "Concurrency/LockFreeQueue.h"

namespace {

// Structura mica de test, trivially copyable, ca sa nu amestecam
// complexitatea lui OrderPod cu testarea mecanicii pure a cozii.
    struct TestItem {
        uint32_t Value;
    };

} // namespace

// ══════════════════════════════════════════════════════════════
// Corectitudine de baza, single-thread
// ══════════════════════════════════════════════════════════════

TEST(LockFreeQueueTest, PushThenPopReturnsSameItem) {
LockFreeQueue<TestItem, 8> queue;

ASSERT_TRUE(queue.Push(TestItem{42}));
auto result = queue.Pop();

ASSERT_TRUE(result.has_value());
EXPECT_EQ(result->Value, 42u);
}

TEST(LockFreeQueueTest, PreservesFIFOOrder) {
LockFreeQueue<TestItem, 8> queue;

queue.Push(TestItem{1});
queue.Push(TestItem{2});
queue.Push(TestItem{3});

auto first = queue.Pop();
auto second = queue.Pop();
auto third = queue.Pop();

ASSERT_TRUE(first.has_value());
ASSERT_TRUE(second.has_value());
ASSERT_TRUE(third.has_value());
EXPECT_EQ(first->Value, 1u);
EXPECT_EQ(second->Value, 2u);
EXPECT_EQ(third->Value, 3u);
}

TEST(LockFreeQueueTest, PopOnEmptyQueueReturnsNullopt) {
LockFreeQueue<TestItem, 8> queue;
auto result = queue.Pop();
EXPECT_FALSE(result.has_value());
}

TEST(LockFreeQueueTest, PopAfterDrainingReturnsNullopt) {
LockFreeQueue<TestItem, 8> queue;
queue.Push(TestItem{1});
queue.Pop();

auto result = queue.Pop();
EXPECT_FALSE(result.has_value());
}

// ══════════════════════════════════════════════════════════════
// Coada plina
// ══════════════════════════════════════════════════════════════

TEST(LockFreeQueueTest, PushReturnsFalseWhenQueueIsFull) {
LockFreeQueue<TestItem, 4> queue;

EXPECT_TRUE(queue.Push(TestItem{1}));
EXPECT_TRUE(queue.Push(TestItem{2}));
EXPECT_TRUE(queue.Push(TestItem{3}));
EXPECT_TRUE(queue.Push(TestItem{4}));

EXPECT_FALSE(queue.Push(TestItem{5}))
<< "Push ar trebui sa esueze explicit cand coada e plina, nu sa blocheze sau sa corupa";
}

TEST(LockFreeQueueTest, PushSucceedsAgainAfterPopFreesSlot) {
LockFreeQueue<TestItem, 4> queue;
queue.Push(TestItem{1});
queue.Push(TestItem{2});
queue.Push(TestItem{3});
queue.Push(TestItem{4});

ASSERT_FALSE(queue.Push(TestItem{5}));

queue.Pop();
EXPECT_TRUE(queue.Push(TestItem{5}))
<< "Dupa ce se elibereaza un slot prin Pop(), Push() ar trebui sa reuseasca din nou";
}

TEST(LockFreeQueueTest, WrapsAroundCorrectlyAfterMultipleFullCycles) {
LockFreeQueue<TestItem, 4> queue;

// trece prin capacitate de mai multe ori, verificand FIFO la fiecare ciclu
for (uint32_t cycle = 0; cycle < 10; ++cycle) {
for (uint32_t i = 0; i < 4; ++i) {
ASSERT_TRUE(queue.Push(TestItem{cycle * 4 + i}));
}
for (uint32_t i = 0; i < 4; ++i) {
auto result = queue.Pop();
ASSERT_TRUE(result.has_value());
EXPECT_EQ(result->Value, cycle * 4 + i);
}
}
}

// ══════════════════════════════════════════════════════════════
// Multi-producer, single-consumer -- exact tiparul MPSC pentru
// care a fost proiectata coada
// ══════════════════════════════════════════════════════════════

TEST(LockFreeQueueTest, MultiProducerSingleConsumer_NoLossNoDuplication) {
constexpr size_t kCapacity = 1024;
LockFreeQueue<TestItem, kCapacity> queue;

constexpr int kProducers = 8;
constexpr int kPerProducer = 2000;
constexpr int kTotal = kProducers * kPerProducer;

std::atomic<int> producedCount{0};
std::atomic<bool> producersDone{false};

std::vector<std::thread> producers;
for (int p = 0; p < kProducers; ++p) {
producers.emplace_back([&queue, &producedCount, p]() {
for (int i = 0; i < kPerProducer; ++i) {
uint32_t value = static_cast<uint32_t>(p * kPerProducer + i);
// retry pe Push, ca sa nu pierdem elemente doar pentru ca
// producatorul a fost mai rapid decat consumatorul la un moment dat
while (!queue.Push(TestItem{value})) {
std::this_thread::yield();
}
producedCount.fetch_add(1, std::memory_order_relaxed);
}
});
}

std::vector<uint32_t> consumed;
consumed.reserve(kTotal);

std::thread consumer([&]() {
    while (static_cast<int>(consumed.size()) < kTotal) {
        auto result = queue.Pop();
        if (result.has_value()) {
            consumed.push_back(result->Value);
        } else {
            std::this_thread::yield();
        }
    }
});

for (auto& t : producers) t.join();
consumer.join();

std::set<uint32_t> uniqueValues(consumed.begin(), consumed.end());
EXPECT_EQ(consumed.size(), static_cast<size_t>(kTotal));
EXPECT_EQ(uniqueValues.size(), static_cast<size_t>(kTotal))
<< "Au aparut valori duplicate -- posibila cursa de date in Push";
}

// ══════════════════════════════════════════════════════════════
// Stress test, pentru rulare sub ThreadSanitizer cu --gtest_repeat
// ══════════════════════════════════════════════════════════════

TEST(LockFreeQueueTest, StressManyProducersSmallCapacity) {
// capacitate mica, deliberat, ca sa forteze des situatia de "coada plina"
// si sa oblige producatorii sa reincerce des -- exact zona de cod cea
// mai predispusa la curse subtile
constexpr size_t kCapacity = 64;
LockFreeQueue<TestItem, kCapacity> queue;

constexpr int kProducers = 8;
constexpr int kPerProducer = 1000;
constexpr int kTotal = kProducers * kPerProducer;

std::vector<std::thread> producers;
for (int p = 0; p < kProducers; ++p) {
producers.emplace_back([&queue, p]() {
for (int i = 0; i < kPerProducer; ++i) {
uint32_t value = static_cast<uint32_t>(p * kPerProducer + i);
while (!queue.Push(TestItem{value})) {
std::this_thread::yield();
}
}
});
}

std::vector<uint32_t> consumed;
consumed.reserve(kTotal);
std::thread consumer([&]() {
    while (static_cast<int>(consumed.size()) < kTotal) {
        auto result = queue.Pop();
        if (result.has_value()) {
            consumed.push_back(result->Value);
        } else {
            std::this_thread::yield();
        }
    }
});

for (auto& t : producers) t.join();
consumer.join();

std::set<uint32_t> uniqueValues(consumed.begin(), consumed.end());
EXPECT_EQ(uniqueValues.size(), static_cast<size_t>(kTotal));
}