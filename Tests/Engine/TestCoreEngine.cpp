//
// Created by Ognean Jason Dennis on 21.07.2026.
//

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <set>
#include <cstdio>
#include <random>

#include "Engine/CoreEngine.h"

using namespace std::chrono_literals;

namespace {

    template <typename Func>
    bool WaitUntil(Func condition, std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition()) return true;
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    int RemainingQuantity(CoreEngine& engine, const std::string& symbol) {
        int total = 0;
        for (const auto& order : engine.GetOrders(symbol)) {
            total += order.GetQuantity();
        }
        return total;
    }

    void CleanPersistenceFile() {
        std::remove("orders.csv");
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// Flux de baza prin fatada
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, CreateOrder_EventuallyAppearsInGetOrders) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_BASIC1";

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "BUY");

    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    auto orders = engine.GetOrders(symbol);
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].GetTraderID(), 1);
    EXPECT_EQ(orders[0].GetQuantity(), 10);
    EXPECT_EQ(orders[0].GetSymbol(), symbol);
    EXPECT_EQ(orders[0].GetSide(), OrderSide::BUY);
    EXPECT_EQ(orders[0].GetType(), OrderType::LIMIT);
    EXPECT_EQ(orders[0].GetTIF(), TimeInForce::GTC);
}

TEST(CoreEngineTest, GetOrders_OnUnknownSymbol_ReturnsEmpty) {
    CleanPersistenceFile();
    CoreEngine engine;
    EXPECT_TRUE(engine.GetOrders("CE_NEVER_SEEN").empty());
}

TEST(CoreEngineTest, CreateOrder_PriceIsScaledCorrectly) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_PRICE1";

    engine.CreateOrder(123.45f, 1, "LIMIT", symbol, "GTC", 1, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    double reconstructed = static_cast<double>(engine.GetOrders(symbol)[0].GetPrice()) / 100.0;
    EXPECT_NEAR(reconstructed, 123.45, 0.01);
}

// ══════════════════════════════════════════════════════════════
// Matching complet: ordinele care se umplu total DISPAR din
// GetOrders (design v2, fara istoric persistent in carte) -- semnalul
// corect de "s-a procesat" e GetTradesHistory, nu GetOrders.
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, CreateOrder_CrossingOrders_FullyMatchAndVanishFromBook) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_CROSS1";
    constexpr int kOrdersPerSide = 100;

    for (int i = 0; i < kOrdersPerSide; ++i) {
        engine.CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC", i, "BUY");
        engine.CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC", kOrdersPerSide + i, "SELL");
    }

    ASSERT_TRUE(WaitUntil([&]() {
        return engine.GetTradesHistory(symbol).size() == static_cast<size_t>(kOrdersPerSide);
    }, 5s)) << "Trade-uri inregistrate: " << engine.GetTradesHistory(symbol).size()
            << " / asteptat " << kOrdersPerSide;

    EXPECT_TRUE(engine.GetOrders(symbol).empty())
                        << "Cartea ar trebui sa ramana complet goala dupa matching simetric total";
    EXPECT_EQ(RemainingQuantity(engine, symbol), 0);
}

TEST(CoreEngineTest, CreateOrder_PartialMatch_RemainderStaysInBookWithReducedQuantity) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_PARTIAL1";

    engine.CreateOrder(100.0f, 20, "LIMIT", symbol, "GTC", 1, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    engine.CreateOrder(100.0f, 8, "LIMIT", symbol, "GTC", 2, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return !engine.GetTradesHistory(symbol).empty(); }, 2s));

    auto orders = engine.GetOrders(symbol);
    ASSERT_EQ(orders.size(), 1u) << "Doar restul ordinii SELL ar trebui sa ramana";
    EXPECT_EQ(orders[0].GetTraderID(), 1);
    EXPECT_EQ(orders[0].GetQuantity(), 12);

    auto trades = engine.GetTradesHistory(symbol);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].GetQuantity(), 8);
}

// ══════════════════════════════════════════════════════════════
// IOC / FOK / MARKET prin fatada completa
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, IOC_PartialMatch_RemainderDiscardedNotAddedToBook) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_IOC1";

    engine.CreateOrder(100.0f, 5, "LIMIT", symbol, "GTC", 1, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "IOC", 2, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return !engine.GetTradesHistory(symbol).empty(); }, 2s));

    EXPECT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).empty(); }, 2s))
                        << "Dupa IOC partial, ambele parti (resting matchat + remainder IOC) ar trebui sa dispara";

    auto trades = engine.GetTradesHistory(symbol);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].GetQuantity(), 5);
}

TEST(CoreEngineTest, FOK_CannotFillCompletely_NoTradeRestingOrderUnaffected) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_FOK1";

    engine.CreateOrder(100.0f, 5, "LIMIT", symbol, "GTC", 1, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "FOK", 2, "BUY");
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(engine.GetTradesHistory(symbol).empty());
    auto orders = engine.GetOrders(symbol);
    ASSERT_EQ(orders.size(), 1u) << "Ordinea SELL initiala ar trebui neatinsa";
    EXPECT_EQ(orders[0].GetQuantity(), 5);
}

TEST(CoreEngineTest, FOK_CanFillCompletely_ExecutesFully) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_FOK2";

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "FOK", 2, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return !engine.GetTradesHistory(symbol).empty(); }, 2s));

    EXPECT_TRUE(engine.GetOrders(symbol).empty());
    EXPECT_EQ(engine.GetTradesHistory(symbol)[0].GetQuantity(), 10);
}

TEST(CoreEngineTest, MarketOrder_MatchesAgainstBestAvailablePrice) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_MARKET1";

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    engine.CreateOrder(0.0f, 10, "MARKET", symbol, "GTC", 2, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return !engine.GetTradesHistory(symbol).empty(); }, 2s));

    EXPECT_TRUE(engine.GetOrders(symbol).empty());
    ASSERT_EQ(engine.GetTradesHistory(symbol).size(), 1u);
    EXPECT_EQ(engine.GetTradesHistory(symbol)[0].GetQuantity(), 10);
}

TEST(CoreEngineTest, MarketOrder_NoLiquidity_NoTradeNoLeak) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_MARKET2";

    engine.CreateOrder(0.0f, 10, "MARKET", symbol, "GTC", 1, "BUY");
    std::this_thread::sleep_for(200ms);

    EXPECT_TRUE(engine.GetTradesHistory(symbol).empty());
    EXPECT_TRUE(engine.GetOrders(symbol).empty());
}

// ══════════════════════════════════════════════════════════════
// Price-time priority: trade-ul se executa la pretul MAKER-ului
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, Trade_ExecutesAtMakerPrice_NotTakerPrice) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_PRICEIMPROVE1";

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    engine.CreateOrder(105.0f, 10, "LIMIT", symbol, "GTC", 2, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return !engine.GetTradesHistory(symbol).empty(); }, 2s));

    auto trades = engine.GetTradesHistory(symbol);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].GetPrice(), 100 * 100u)
                        << "Trade-ul trebuie executat la pretul maker-ului (100), nu al taker-ului (105)";
}

// ══════════════════════════════════════════════════════════════
// CancelOrder
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, CancelOrder_RemovesOrderFromBook) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_CANCEL1";

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    int orderId = engine.GetOrders(symbol)[0].GetOrderID();
    engine.CancelOrder(orderId, symbol);

    EXPECT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).empty(); }, 2s));
}

TEST(CoreEngineTest, CancelOrder_NonExistentId_DoesNotCrash) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_CANCEL2";

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    EXPECT_NO_THROW(engine.CancelOrder(999999, symbol));
    EXPECT_EQ(engine.GetOrders(symbol).size(), 1u);
}

TEST(CoreEngineTest, CancelOrder_UnknownSymbol_DoesNotCrash) {
    CleanPersistenceFile();
    CoreEngine engine;
    EXPECT_NO_THROW(engine.CancelOrder(1, "CE_NEVER_REGISTERED"));
}

TEST(CoreEngineTest, CancelOrder_OnlyRemovesTargetOrder_OthersUnaffected) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_CANCEL3";

    engine.CreateOrder(100.0f, 5, "LIMIT", symbol, "GTC", 1, "BUY");
    engine.CreateOrder(101.0f, 7, "LIMIT", symbol, "GTC", 2, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 2; }, 2s));

    auto orders = engine.GetOrders(symbol);
    int targetId = (orders[0].GetTraderID() == 1) ? orders[0].GetOrderID() : orders[1].GetOrderID();
    engine.CancelOrder(targetId, symbol);

    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));
    EXPECT_EQ(engine.GetOrders(symbol)[0].GetTraderID(), 2);
}

// ══════════════════════════════════════════════════════════════
// GetBestBids / GetBestAsks -- sortare si limita x
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, GetBestBids_ReturnsHighestPriceFirst) {
    CleanPersistenceFile();
    CoreEngine engine;
    std::string symbol = "CE_BIDS1";

    engine.CreateOrder(95.0f, 1, "LIMIT", symbol, "GTC", 1, "BUY");
    engine.CreateOrder(105.0f, 1, "LIMIT", symbol, "GTC", 2, "BUY");
    engine.CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC", 3, "BUY");

    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 3; }, 2s));

    auto bids = engine.GetBestBids(3, symbol);
    ASSERT_EQ(bids.size(), 3u);
    EXPECT_GE(bids[0].GetPrice(), bids[1].GetPrice());
    EXPECT_GE(bids[1].GetPrice(), bids[2].GetPrice());
}

TEST(CoreEngineTest, GetBestBids_RespectsLimitSmallerThanAvailable) {
    CleanPersistenceFile();
    CoreEngine engine;
    std::string symbol = "CE_BIDS2";

    for (int i = 0; i < 5; ++i) {
        engine.CreateOrder(100.0f + i, 1, "LIMIT", symbol, "GTC", i, "BUY");
    }
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 5; }, 2s));

    auto bids = engine.GetBestBids(2, symbol);
    EXPECT_EQ(bids.size(), 2u);
}

TEST(CoreEngineTest, GetBestAsks_ReturnsLowestPriceFirst) {
    CleanPersistenceFile();
    CoreEngine engine;
    std::string symbol = "CE_ASKS1";

    engine.CreateOrder(105.0f, 1, "LIMIT", symbol, "GTC", 1, "SELL");
    engine.CreateOrder(95.0f, 1, "LIMIT", symbol, "GTC", 2, "SELL");
    engine.CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC", 3, "SELL");

    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 3; }, 2s));

    auto asks = engine.GetBestAsks(3, symbol);
    ASSERT_EQ(asks.size(), 3u);
    EXPECT_LE(asks[0].GetPrice(), asks[1].GetPrice());
    EXPECT_LE(asks[1].GetPrice(), asks[2].GetPrice());
}

// ══════════════════════════════════════════════════════════════
// GetTradesHistory -- corectitudine completa a campurilor
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, GetTradesHistory_ReturnsCorrectTradeDetails) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_TRADES1";

    auto beforeTrade = std::chrono::system_clock::now();

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));

    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 2, "BUY");
    ASSERT_TRUE(WaitUntil([&]() { return !engine.GetTradesHistory(symbol).empty(); }, 2s));

    auto afterTrade = std::chrono::system_clock::now();

    auto trades = engine.GetTradesHistory(symbol);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].GetQuantity(), 10);
    EXPECT_EQ(trades[0].GetMakerID(), 1);
    EXPECT_EQ(trades[0].GetTakerID(), 2);

    EXPECT_GE(trades[0].GetTimestamp(), beforeTrade);
    EXPECT_LE(trades[0].GetTimestamp(), afterTrade);
}

TEST(CoreEngineTest, GetTradesHistory_MultipleMatches_RecordedInOrder) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_TRADES2";

    engine.CreateOrder(100.0f, 3, "LIMIT", symbol, "GTC", 1, "SELL");
    engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 2, "SELL");
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 2; }, 2s));

    engine.CreateOrder(100.0f, 8, "LIMIT", symbol, "GTC", 3, "BUY");
    ASSERT_TRUE(WaitUntil([&]() {
        return engine.GetTradesHistory(symbol).size() == 2;
    }, 2s));

    auto trades = engine.GetTradesHistory(symbol);
    EXPECT_EQ(trades[0].GetQuantity(), 3);
    EXPECT_EQ(trades[1].GetQuantity(), 5);
}

TEST(CoreEngineTest, GetTradesHistory_OnUnknownSymbol_ReturnsEmpty) {
    CleanPersistenceFile();
    CoreEngine engine;
    EXPECT_TRUE(engine.GetTradesHistory("CE_NEVER_SEEN").empty());
}

// ══════════════════════════════════════════════════════════════
// Izolare multi-simbol prin fatada
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, OrdersOnDifferentSymbolsAreFullyIsolated) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbolA = "CE_ISO_A";
    const std::string symbolB = "CE_ISO_B";

    engine.CreateOrder(100.0f, 10, "LIMIT", symbolA, "GTC", 1, "BUY");
    engine.CreateOrder(100.0f, 10, "LIMIT", symbolA, "GTC", 2, "SELL");
    engine.CreateOrder(50.0f, 5, "LIMIT", symbolB, "GTC", 3, "BUY");

    ASSERT_TRUE(WaitUntil([&]() {
        return engine.GetTradesHistory(symbolA).size() == 1 && engine.GetOrders(symbolB).size() == 1;
    }, 3s));

    EXPECT_TRUE(engine.GetOrders(symbolA).empty());
    EXPECT_EQ(engine.GetOrders(symbolB)[0].GetTraderID(), 3);
    EXPECT_TRUE(engine.GetTradesHistory(symbolB).empty());
}

// ══════════════════════════════════════════════════════════════
// Concurenta reala prin fatada
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, ConcurrentCreateOrderFromMultipleThreads_NoLossNoDuplication) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_CONCURRENT1";
    constexpr int kProducers = 8;
    constexpr int kPerProducer = 50;
    constexpr int kTotal = kProducers * kPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&engine, &symbol, p]() {
            for (int i = 0; i < kPerProducer; ++i) {
                engine.CreateOrder(100.0f, 1, "LIMIT", symbol, "GTC",
                                   p * kPerProducer + i, "BUY");
            }
        });
    }
    for (auto& t : producers) t.join();

    ASSERT_TRUE(WaitUntil([&]() {
        return engine.GetOrders(symbol).size() == static_cast<size_t>(kTotal);
    }, 5s)) << "Primite: " << engine.GetOrders(symbol).size() << " / " << kTotal;

    std::set<int> traderIds;
    for (const auto& order : engine.GetOrders(symbol)) {
        traderIds.insert(order.GetTraderID());
    }
    EXPECT_EQ(traderIds.size(), static_cast<size_t>(kTotal));
}

// ══════════════════════════════════════════════════════════════
// Persistenta: Save() -> Load() (constructor) round-trip
// ══════════════════════════════════════════════════════════════

TEST(CoreEngineTest, SaveThenLoad_RestoresRestingOrdersWithoutOriginalId) {
    CleanPersistenceFile();
    const std::string symbol = "CE_PERSIST1";

    {
        CoreEngine engine;
        engine.CreateOrder(100.0f, 5, "LIMIT", symbol, "GTC", 11, "BUY");
        engine.CreateOrder(101.0f, 7, "LIMIT", symbol, "GTC", 22, "BUY");
        engine.CreateOrder(102.0f, 3, "LIMIT", symbol, "GTC", 33, "BUY");

        ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 3; }, 2s));
        engine.Save();
    }

    CoreEngine engine2;
    ASSERT_TRUE(WaitUntil([&]() { return engine2.GetOrders(symbol).size() == 3; }, 3s))
                                << "Ordinele nu au fost reincarcate din orders.csv";

    std::set<int> traderIds;
    int totalQuantity = 0;
    for (const auto& order : engine2.GetOrders(symbol)) {
        traderIds.insert(order.GetTraderID());
        totalQuantity += order.GetQuantity();
    }

    EXPECT_EQ(traderIds, (std::set<int>{11, 22, 33}));
    EXPECT_EQ(totalQuantity, 5 + 7 + 3);

    CleanPersistenceFile();
}

TEST(CoreEngineTest, SaveThenLoad_MixedSidesAndMultipleSymbols) {
    CleanPersistenceFile();
    const std::string symbolA = "CE_PERSIST_A";
    const std::string symbolB = "CE_PERSIST_B";

    {
        CoreEngine engine;
        engine.CreateOrder(100.0f, 5, "LIMIT", symbolA, "GTC", 1, "BUY");
        engine.CreateOrder(200.0f, 8, "LIMIT", symbolA, "GTC", 2, "SELL");
        engine.CreateOrder(50.0f, 3, "LIMIT", symbolB, "GTC", 3, "BUY");

        ASSERT_TRUE(WaitUntil([&]() {
            return engine.GetOrders(symbolA).size() == 2 && engine.GetOrders(symbolB).size() == 1;
        }, 2s));
        engine.Save();
    }

    CoreEngine engine2;
    ASSERT_TRUE(WaitUntil([&]() {
        return engine2.GetOrders(symbolA).size() == 2 && engine2.GetOrders(symbolB).size() == 1;
    }, 3s));

    bool foundBuy = false, foundSell = false;
    for (const auto& order : engine2.GetOrders(symbolA)) {
        if (order.GetSide() == OrderSide::BUY) foundBuy = true;
        if (order.GetSide() == OrderSide::SELL) foundSell = true;
    }
    EXPECT_TRUE(foundBuy);
    EXPECT_TRUE(foundSell);
    EXPECT_EQ(engine2.GetOrders(symbolB)[0].GetTraderID(), 3);

    CleanPersistenceFile();
}

TEST(CoreEngineTest, SaveThenLoad_PreservesApproximateTimestamp) {
    CleanPersistenceFile();
    const std::string symbol = "CE_PERSIST_TIME1";
    auto beforeCreate = std::chrono::system_clock::now();

    {
        CoreEngine engine;
        engine.CreateOrder(100.0f, 5, "LIMIT", symbol, "GTC", 1, "BUY");
        ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));
        engine.Save();
    }

    CoreEngine engine2;
    ASSERT_TRUE(WaitUntil([&]() { return engine2.GetOrders(symbol).size() == 1; }, 3s));

    auto reloadedTimestamp = engine2.GetOrders(symbol)[0].GetTime();
    auto afterReload = std::chrono::system_clock::now();

    EXPECT_GE(reloadedTimestamp, beforeCreate);
    EXPECT_LE(reloadedTimestamp, afterReload);

    CleanPersistenceFile();
}

TEST(CoreEngineTest, SaveThenLoad_SkipsFullyFilledOrders) {
    CleanPersistenceFile();
    const std::string symbol = "CE_PERSIST_SKIP1";

    {
        CoreEngine engine;
        engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 1, "SELL");
        engine.CreateOrder(100.0f, 10, "LIMIT", symbol, "GTC", 2, "BUY");
        engine.CreateOrder(105.0f, 5, "LIMIT", symbol, "GTC", 3, "SELL");

        ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s));
        engine.Save();
    }

    CoreEngine engine2;
    ASSERT_TRUE(WaitUntil([&]() { return engine2.GetOrders(symbol).size() == 1; }, 3s));
    EXPECT_EQ(engine2.GetOrders(symbol)[0].GetTraderID(), 3);

    CleanPersistenceFile();
}

TEST(CoreEngineTest, SaveWithNoOrders_ProducesValidEmptyFile) {
    CleanPersistenceFile();
    {
        CoreEngine engine;
        engine.Save();
    }

    CoreEngine engine2;
    EXPECT_TRUE(engine2.GetOrders("CE_ANY_SYMBOL").empty());

    CleanPersistenceFile();
}


TEST(CoreEngineTest, ConcurrentCreateAndCancel_NoPoolCorruption) {
    CleanPersistenceFile();
    CoreEngine engine;
    const std::string symbol = "CE_RACE_REGRESSION1";

    std::atomic<bool> running{true};
    std::atomic<int> traderId{0};
    std::atomic<bool> corruptionDetected{false};
    std::string corruptionReason;
    std::mutex reasonMutex;

    // producatori: creeaza continuu ordine GTC, la preturi variate,
    // ca sa ramana odihnite (nu se matcheaza intre ele)
    std::vector<std::thread> producers;
    for (int p = 0; p < 4; ++p) {
        producers.emplace_back([&]() {
            while (running.load(std::memory_order_relaxed)) {
                int id = traderId.fetch_add(1);
                float price = 100.0f + static_cast<float>(id % 50);
                engine.CreateOrder(price, 1, "LIMIT", symbol, "GTC", id, "BUY");
            }
        });
    }

    // anulatori: concurenti cu producatorii, citesc cartea si anuleaza
    // agresiv -- exact scenariul care a expus cursa (Allocate in worker
    // vs Free in CancelOrder, ambele atingand OrderPool)
    std::vector<std::thread> cancellers;
    for (int c = 0; c < 4; ++c) {
        cancellers.emplace_back([&, c]() {
            std::mt19937 rng(c);
            while (running.load(std::memory_order_relaxed)) {
                try {
                    auto orders = engine.GetOrders(symbol);
                    if (!orders.empty()) {
                        std::uniform_int_distribution<size_t> pick(0, orders.size() - 1);
                        engine.CancelOrder(orders[pick(rng)].GetOrderID(), symbol);
                    }
                } catch (const std::exception& e) {
                    std::lock_guard<std::mutex> lock(reasonMutex);
                    corruptionDetected.store(true, std::memory_order_relaxed);
                    corruptionReason = e.what();
                    running.store(false, std::memory_order_relaxed);
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    running.store(false, std::memory_order_relaxed);
    for (auto& t : producers) t.join();
    for (auto& t : cancellers) t.join();

    EXPECT_FALSE(corruptionDetected.load())
                        << "Corupere de OrderPool detectata sub Create+Cancel concurent: "
                        << corruptionReason;

    // verificare finala: orice ordine ramasa in carte trebuie sa fie
    // valida (cantitate pozitiva) si unica (fara ID duplicat, semn ca
    // acelasi slot din pool ar fi fost alocat de doua ori)
    ASSERT_NO_THROW({
                        auto finalOrders = engine.GetOrders(symbol);
                        std::set<int> seenIds;
                        for (const auto& order : finalOrders) {
                            EXPECT_GT(order.GetQuantity(), 0)
                                    << "Ordine cu cantitate invalida (<=0) gasita in carte";
                            EXPECT_TRUE(seenIds.insert(order.GetOrderID()).second)
                                    << "OrderId duplicat gasit -- semn de corupere a free-list-ului din OrderPool";
                        }
                    });
}