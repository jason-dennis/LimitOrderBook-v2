//
// Created by Ognean Jason Dennis on 21.07.2026.
//

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <set>
#include <atomic>

#include "Routing/OrderRouter.h"

using namespace std::chrono_literals;

namespace {

    OrderRequest MakeRequest(int traderId, const std::string& symbol,
                             const std::string& side = "BUY",
                             float price = 100.0f, int quantity = 10,
                             const std::string& tif = "GTC",
                             const std::string& type = "LIMIT") {
        OrderRequest req;
        req.Price = price;
        req.Quantity = quantity;
        req.Type = type;
        req.Symbol = symbol;
        req.TIF = tif;
        req.TraderID = traderId;
        req.Side = side;
        return req;
    }

    template <typename Func>
    bool WaitUntil(Func condition, std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition()) return true;
            std::this_thread::sleep_for(5ms);
        }
        return false;
    }

    uint32_t RemainingQuantity(OrderRouter& router, const std::string& symbol) {
        uint32_t total = 0;
        for (const auto& order : router.GetOrders(symbol)) {
            total += order.Quantity;
        }
        return total;
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// Flux de baza: Route() -> procesare asincrona -> GetOrders()
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, RouteSingleOrder_EventuallyAppearsInGetOrders) {
    OrderRouter router;
    const std::string symbol = "RT_BASIC1";

    auto request = MakeRequest(42, symbol);
    router.Route(request);

    ASSERT_TRUE(WaitUntil([&]() { return router.GetOrders(symbol).size() == 1; }, 2s));

    auto orders = router.GetOrders(symbol);
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_EQ(orders[0].TraderId, 42u);
    EXPECT_EQ(orders[0].Quantity, 10u);
}

TEST(OrderRouterTest, RouteMultipleOrdersSameSymbol_AllEventuallyProcessed) {
    OrderRouter router;
    const std::string symbol = "RT_MULTI1";
    constexpr int kCount = 50;

    for (int i = 0; i < kCount; ++i) {
        auto request = MakeRequest(i, symbol);
        router.Route(request);
    }

    ASSERT_TRUE(WaitUntil([&]() {
        return router.GetOrders(symbol).size() == static_cast<size_t>(kCount);
    }, 2s));

    std::set<uint32_t> traderIds;
    for (const auto& order : router.GetOrders(symbol)) {
        traderIds.insert(order.TraderId);
    }
    EXPECT_EQ(traderIds.size(), static_cast<size_t>(kCount));
}

TEST(OrderRouterTest, OrdersOnDifferentSymbolsAreIsolated) {
    OrderRouter router;
    const std::string symbolA = "RT_ISO_A";
    const std::string symbolB = "RT_ISO_B";

    auto reqA = MakeRequest(1, symbolA);
    auto reqB = MakeRequest(2, symbolB);
    router.Route(reqA);
    router.Route(reqB);

    ASSERT_TRUE(WaitUntil([&]() {
        return router.GetOrders(symbolA).size() == 1 && router.GetOrders(symbolB).size() == 1;
    }, 2s));

    EXPECT_EQ(router.GetOrders(symbolA)[0].TraderId, 1u);
    EXPECT_EQ(router.GetOrders(symbolB)[0].TraderId, 2u);
}

TEST(OrderRouterTest, GetOrders_OnUnknownSymbol_ReturnsEmpty) {
    OrderRouter router;
    EXPECT_TRUE(router.GetOrders("RT_NEVER_SEEN").empty());
}

// ══════════════════════════════════════════════════════════════
// Concurenta reala: multi-producer prin Route(), pe acelasi simbol --
// valideaza end-to-end exact scopul redesign-ului (single writer per
// symbol, coada lock-free MPSC), nu doar componentele izolat.
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, ConcurrentRouteFromMultipleThreads_NoLossNoDuplication) {
    OrderRouter router;
    const std::string symbol = "RT_CONCURRENT1";
    constexpr int kProducers = 8;
    constexpr int kPerProducer = 100;
    constexpr int kTotal = kProducers * kPerProducer;

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&router, &symbol, p]() {
            for (int i = 0; i < kPerProducer; ++i) {
                auto request = MakeRequest(p * kPerProducer + i, symbol);
                router.Route(request);
            }
        });
    }
    for (auto& t : producers) t.join();

    ASSERT_TRUE(WaitUntil([&]() {
        return router.GetOrders(symbol).size() == static_cast<size_t>(kTotal);
    }, 5s)) << "Numar final: " << router.GetOrders(symbol).size() << " / asteptat " << kTotal;

    std::set<uint32_t> traderIds;
    for (const auto& order : router.GetOrders(symbol)) {
        traderIds.insert(order.TraderId);
    }
    EXPECT_EQ(traderIds.size(), static_cast<size_t>(kTotal))
                        << "Ordine pierdute sau duplicate sub concurenta multi-producator";
}

// ══════════════════════════════════════════════════════════════
// Matching real prin pipeline-ul complet asincron -- invariantul
// central: volum simetric BUY/SELL => 0 ramas, indiferent de ordinea
// de sosire prin coada.
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, CrossingOrdersFullyMatch_QuantityConserved) {
    OrderRouter router;
    const std::string symbol = "RT_CROSS1";
    constexpr int kOrdersPerSide = 200;

    for (int i = 0; i < kOrdersPerSide; ++i) {
        auto buy = MakeRequest(i, symbol, "BUY", 100.0f, 1);
        router.Route(buy);
        auto sell = MakeRequest(kOrdersPerSide + i, symbol, "SELL", 100.0f, 1);
        router.Route(sell);
    }

    // Ordinele complet matchate DISPAR din carte (design v2, fara istoric
    // persistent) -- semnalul corect de "s-a terminat" e numarul de
    // trade-uri, nu numarul de ordine ramase in GetOrders.
    ASSERT_TRUE(WaitUntil([&]() {
        return router.GetTradesHistory(symbol).size() == static_cast<size_t>(kOrdersPerSide);
    }, 5s)) << "Trade-uri: " << router.GetTradesHistory(symbol).size() << " / " << kOrdersPerSide;

    EXPECT_TRUE(router.GetOrders(symbol).empty())
                        << "Cartea ar trebui sa ramana complet goala dupa matching simetric total";
    EXPECT_EQ(RemainingQuantity(router, symbol), 0u);
}

// ══════════════════════════════════════════════════════════════
// CancelOrder
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, CancelOrder_RemovesOrderFromBook) {
    OrderRouter router;
    const std::string symbol = "RT_CANCEL1";

    auto request = MakeRequest(7, symbol);
    router.Route(request);

    ASSERT_TRUE(WaitUntil([&]() { return router.GetOrders(symbol).size() == 1; }, 2s));

    int orderId = static_cast<int>(router.GetOrders(symbol)[0].OrderId);
    router.CancelOrder(orderId, symbol);

    EXPECT_TRUE(WaitUntil([&]() { return router.GetOrders(symbol).empty(); }, 2s))
                        << "Ordinea nu a fost eliminata dupa CancelOrder";
}

// ══════════════════════════════════════════════════════════════
// GetBestBids / GetBestAsk
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, GetBestBids_ReturnsHighestPriceFirst) {
    OrderRouter router;
    std::string symbol = "RT_BIDS1";

    auto low = MakeRequest(1, symbol, "BUY", 95.0f, 1);
    auto high = MakeRequest(2, symbol, "BUY", 105.0f, 1);
    auto mid = MakeRequest(3, symbol, "BUY", 100.0f, 1);
    router.Route(low);
    router.Route(high);
    router.Route(mid);

    ASSERT_TRUE(WaitUntil([&]() { return router.GetOrders(symbol).size() == 3; }, 2s));

    auto bids = router.GetBestBids(3, symbol);
    ASSERT_EQ(bids.size(), 3u);
    EXPECT_GE(bids[0].Price, bids[1].Price);
    EXPECT_GE(bids[1].Price, bids[2].Price);
}

TEST(OrderRouterTest, GetBestAsk_ReturnsLowestPriceFirst) {
    OrderRouter router;
    std::string symbol = "RT_ASKS1";

    auto high = MakeRequest(1, symbol, "SELL", 105.0f, 1);
    auto low = MakeRequest(2, symbol, "SELL", 95.0f, 1);
    auto mid = MakeRequest(3, symbol, "SELL", 100.0f, 1);
    router.Route(high);
    router.Route(low);
    router.Route(mid);

    ASSERT_TRUE(WaitUntil([&]() { return router.GetOrders(symbol).size() == 3; }, 2s));

    auto asks = router.GetBestAsk(3, symbol);
    ASSERT_EQ(asks.size(), 3u);
    EXPECT_LE(asks[0].Price, asks[1].Price);
    EXPECT_LE(asks[1].Price, asks[2].Price);
}

// ══════════════════════════════════════════════════════════════
// GetTradesHistory
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, GetTradesHistory_RecordsTradeAfterMatch) {
    OrderRouter router;
    const std::string symbol = "RT_TRADES1";

    auto sell = MakeRequest(1, symbol, "SELL", 100.0f, 10);
    router.Route(sell);
    ASSERT_TRUE(WaitUntil([&]() { return router.GetOrders(symbol).size() == 1; }, 2s));

    auto buy = MakeRequest(2, symbol, "BUY", 100.0f, 10);
    router.Route(buy);

    ASSERT_TRUE(WaitUntil([&]() { return !router.GetTradesHistory(symbol).empty(); }, 2s));

    auto trades = router.GetTradesHistory(symbol);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 10u);
    EXPECT_EQ(trades[0].MakerId, 1u);
    EXPECT_EQ(trades[0].TakerId, 2u);
}

// ══════════════════════════════════════════════════════════════
// GetAllOrders
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, GetAllOrders_CoversMultipleSymbols) {
    OrderRouter router;
    const std::vector<std::string> symbols = {"RT_ALL_A", "RT_ALL_B", "RT_ALL_C"};

    for (size_t i = 0; i < symbols.size(); ++i) {
        auto request = MakeRequest(static_cast<int>(i), symbols[i]);
        router.Route(request);
    }

    ASSERT_TRUE(WaitUntil([&]() {
        for (const auto& s : symbols) {
            if (router.GetOrders(s).size() != 1) return false;
        }
        return true;
    }, 3s));

    auto all = router.GetAllOrders();
    std::set<std::string> foundSymbols;
    for (const auto& [symbol, orders] : all) {
        foundSymbols.insert(symbol);
    }
    for (const auto& s : symbols) {
        EXPECT_TRUE(foundSymbols.count(s) == 1) << "Lipseste simbolul " << s;
    }
}

// ══════════════════════════════════════════════════════════════
// Load() -- pastreaza timestamp-ul explicit, nu genereaza "acum"
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, Load_PreservesExplicitTimestamp) {
    OrderRouter router;
    const std::string symbol = "RT_LOAD1";
    constexpr uint64_t customTimestamp = 123456789000ULL;

    auto request = MakeRequest(5, symbol);
    router.Load(request, customTimestamp);

    ASSERT_TRUE(WaitUntil([&]() { return router.GetOrders(symbol).size() == 1; }, 2s));

    EXPECT_EQ(router.GetOrders(symbol)[0].Timestamp, customTimestamp)
                        << "Load() ar trebui sa pastreze timestamp-ul dat, nu sa genereze unul nou";
}

// ══════════════════════════════════════════════════════════════
// Gol arhitectural cunoscut: Route()/Load() nu au cum sa semnaleze
// esec daca Push() pe coada esueaza (coada plina). Acest test
// documenteaza comportamentul actual, nu il valideaza ca "dorit".
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, KnownGap_RouteDoesNotReportQueueFullSilentDrop) {
    OrderRouter router;
    const std::string symbol = "RT_CAPACITY1";
    // sub capacitatea cozii (QueueCapacity), ca sa nu declansam
    // efectiv pierderea -- testul confirma doar ca la volum normal,
    // sub capacitate, nu apare nicio pierdere (regresie de baza)
    constexpr int kCount = 500;

    for (int i = 0; i < kCount; ++i) {
        auto request = MakeRequest(i, symbol);
        router.Route(request);
    }

    bool allArrived = WaitUntil([&]() {
        return router.GetOrders(symbol).size() == static_cast<size_t>(kCount);
    }, 5s);

    EXPECT_TRUE(allArrived)
                        << "La volum sub capacitatea cozii, toate ordinele ar trebui sa ajunga -- "
                        << "primite: " << router.GetOrders(symbol).size() << " / " << kCount;
}

// ══════════════════════════════════════════════════════════════
// Shutdown
// ══════════════════════════════════════════════════════════════

TEST(OrderRouterTest, Shutdown_StopsAllWorkersWithoutHanging) {
    OrderRouter router;
    const std::vector<std::string> symbols = {"RT_SHUT_A", "RT_SHUT_B", "RT_SHUT_C"};

    for (size_t i = 0; i < symbols.size(); ++i) {
        auto request = MakeRequest(static_cast<int>(i), symbols[i]);
        router.Route(request);
    }

    for (const auto& s : symbols) {
        WaitUntil([&]() { return !router.GetOrders(s).empty(); }, 2s);
    }

    // daca Shutdown() ramane agatat (join() pe vreun worker blocat),
    // testul insusi ar ramane agatat -- absenta hang-ului E testul
    router.ShutDown();
    SUCCEED();
}