//
// Created by Ognean Jason Dennis on 17.07.2026.

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <set>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "Engine/CoreEngine.h"
#include "Concurrency/OrderQueue.h"
#include "Concurrency/Worker.h"
#include "Networking/TcpServer.h"
#include "Networking/Protocol.h"

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <vector>
#include <set>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "Networking/TcpServer.h"
#include "Networking/Protocol.h"
#include "Engine/CoreEngine.h"

using namespace std::chrono_literals;

namespace {

    OrderRequest MakeRequest(int traderId, const std::string& symbol,
                             const std::string& side = "BUY", float price = 100.0f,
                             int quantity = 10) {
        OrderRequest req;
        req.Price = price;
        req.Quantity = quantity;
        req.Type = "LIMIT";
        req.Symbol = symbol;
        req.TIF = "GTC";
        req.TraderID = traderId;
        req.Side = side;
        return req;
    }

    int ConnectToServer(int port) {
        for (int attempt = 0; attempt < 50; ++attempt) {
            int sock = socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

            if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0) {
                return sock;
            }
            close(sock);
            std::this_thread::sleep_for(20ms);
        }
        return -1;
    }

    void SendOrder(int port, const OrderRequest& request) {
        int sock = ConnectToServer(port);
        if (sock == -1) return;
        auto bytes = Encode(request);
        send(sock, bytes.data(), bytes.size(), 0);
        close(sock);
    }

    template <typename Func>
    bool WaitUntil(Func condition, std::chrono::milliseconds timeout) {
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (std::chrono::steady_clock::now() < deadline) {
            if (condition()) return true;
            std::this_thread::sleep_for(10ms);
        }
        return false;
    }

} // namespace

// ══════════════════════════════════════════════════════════════
// Izolare multi-simbol prin retea reala -- confirma ca izolarea
// deja testata direct pe CoreEngine se pastreaza si cand ordinele
// vin prin conexiuni TCP separate, nu doar prin apeluri in memorie.
// ══════════════════════════════════════════════════════════════

TEST(EndToEndTest, OrdersFromDifferentSymbolsViaTcp_AreIsolatedCorrectly) {
    constexpr int kPort = 54101;
    const std::vector<std::string> symbols = {"E2E_A", "E2E_B", "E2E_C"};
    constexpr int kOrdersPerSymbol = 5;

    CoreEngine engine;
    TcpServer server(kPort, engine);
    server.Start();

    std::vector<std::thread> clients;
    for (const auto& symbol : symbols) {
        for (int i = 0; i < kOrdersPerSymbol; ++i) {
            clients.emplace_back([i, kPort, &symbol]() {
                SendOrder(kPort, MakeRequest(i, symbol));
            });
        }
    }
    for (auto& t : clients) t.join();

    for (const auto& symbol : symbols) {
        ASSERT_TRUE(WaitUntil([&]() {
            return engine.GetOrders(symbol).size() == static_cast<size_t>(kOrdersPerSymbol);
        }, 5s)) << "Simbolul " << symbol << " nu a primit toate ordinele la timp";

        EXPECT_EQ(engine.GetOrders(symbol).size(), static_cast<size_t>(kOrdersPerSymbol))
                            << "Simbolul " << symbol << " a fost afectat de trafic pe alte simboluri";
    }

    server.Stop();
}

// ══════════════════════════════════════════════════════════════
// Matching real, prin reteaua reala -- nu doar ca ordinele ajung,
// ci ca chiar se MATCHEAZA corect (trade inregistrat), validand
// intregul lant: TCP -> Decode -> Router -> Queue -> Worker ->
// MatchingEngine -> Trade.
// ══════════════════════════════════════════════════════════════

TEST(EndToEndTest, CrossingOrdersViaTcp_ActuallyMatchAndProduceTrade) {
    constexpr int kPort = 54102;
    const std::string symbol = "E2E_CROSS1";

    CoreEngine engine;
    TcpServer server(kPort, engine);
    server.Start();

    SendOrder(kPort, MakeRequest(1, symbol, "SELL", 100.0f, 10));
    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s))
                                << "Ordinea SELL initiala nu a ajuns";

    SendOrder(kPort, MakeRequest(2, symbol, "BUY", 100.0f, 10));
    ASSERT_TRUE(WaitUntil([&]() { return !engine.GetTradesHistory(symbol).empty(); }, 2s))
                                << "Trade-ul nu a fost inregistrat dupa matching prin TCP";

    EXPECT_TRUE(engine.GetOrders(symbol).empty())
                        << "Cartea ar trebui goala dupa matching complet";

    auto trades = engine.GetTradesHistory(symbol);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].GetQuantity(), 10);
    EXPECT_EQ(trades[0].GetMakerID(), 1);
    EXPECT_EQ(trades[0].GetTakerID(), 2);

    server.Stop();
}

// ══════════════════════════════════════════════════════════════
// Scenariu complet, aproape de productie: mai multi clienti, mai
// multe simboluri, trafic amestecat (unele se matcheaza, altele
// raman in carte) -- validare finala holistica a intregului v2.
// ══════════════════════════════════════════════════════════════

TEST(EndToEndTest, FullProductionLikeScenario_MixedSymbolsAndMatching) {
    constexpr int kPort = 54103;
    const std::string symbolMatched = "E2E_PROD_MATCH";
    const std::string symbolResting = "E2E_PROD_REST";

    CoreEngine engine;
    TcpServer server(kPort, engine);
    server.Start();

    std::vector<std::thread> clients;

    // Simbol cu matching: 20 perechi BUY/SELL simetrice
    constexpr int kMatchPairs = 20;
    for (int i = 0; i < kMatchPairs; ++i) {
        clients.emplace_back([i, kPort, &symbolMatched]() {
            SendOrder(kPort, MakeRequest(i, symbolMatched, "BUY", 100.0f, 1));
        });
        clients.emplace_back([i, kPort, &symbolMatched]() {
            SendOrder(kPort, MakeRequest(kMatchPairs + i, symbolMatched, "SELL", 100.0f, 1));
        });
    }

    // Simbol fara matching: doar ordine BUY, la preturi diferite,
    // toate raman in carte
    constexpr int kRestingCount = 15;
    for (int i = 0; i < kRestingCount; ++i) {
        clients.emplace_back([i, kPort, &symbolResting]() {
            SendOrder(kPort, MakeRequest(i, symbolResting, "BUY", 90.0f + i, 1));
        });
    }

    for (auto& t : clients) t.join();

    ASSERT_TRUE(WaitUntil([&]() {
        return engine.GetTradesHistory(symbolMatched).size() == static_cast<size_t>(kMatchPairs);
    }, 5s)) << "Trade-uri pe simbolul cu matching: "
            << engine.GetTradesHistory(symbolMatched).size() << " / " << kMatchPairs;

    ASSERT_TRUE(WaitUntil([&]() {
        return engine.GetOrders(symbolResting).size() == static_cast<size_t>(kRestingCount);
    }, 5s)) << "Ordine ramase pe simbolul fara matching: "
            << engine.GetOrders(symbolResting).size() << " / " << kRestingCount;

    EXPECT_TRUE(engine.GetOrders(symbolMatched).empty())
                        << "Simbolul cu matching ar trebui sa aiba cartea goala";
    EXPECT_TRUE(engine.GetTradesHistory(symbolResting).empty())
                        << "Simbolul fara matching nu ar trebui sa aiba niciun trade";

    server.Stop();
}