//
// Created by Ognean Jason Dennis on 16.07.2026.
//

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

    OrderRequest MakeRequest(int traderId, const std::string& symbol = "AAPL") {
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
// Flux de baza: un client real, un mesaj -- verifica ca ajunge
// decodat corect, prin Router, pana in CoreEngine::GetOrders.
// ══════════════════════════════════════════════════════════════

TEST(TcpServerTest, SendingEncodedOrderReachesCoreEngine) {
    constexpr int kPort = 54001;
    const std::string symbol = "TCP_BASIC1";

    CoreEngine engine;
    TcpServer server(kPort, engine);
    server.Start();

    int clientSocket = ConnectToServer(kPort);
    ASSERT_NE(clientSocket, -1) << "Nu m-am putut conecta la server";

    auto original = MakeRequest(42, symbol);
    auto bytes = Encode(original);
    send(clientSocket, bytes.data(), bytes.size(), 0);
    close(clientSocket);

    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s))
                                << "Nimic nu a ajuns in CoreEngine la timp";

    auto orders = engine.GetOrders(symbol);
    ASSERT_EQ(orders.size(), 1u);
    EXPECT_NEAR(static_cast<float>(orders[0].GetPrice()) / 100.0f, original.Price, 0.01f);
    EXPECT_EQ(orders[0].GetQuantity(), original.Quantity);
    EXPECT_EQ(orders[0].GetSymbol(), original.Symbol);
    EXPECT_EQ(orders[0].GetTraderID(), original.TraderID);

    server.Stop();
}

// ══════════════════════════════════════════════════════════════
// Mai multe mesaje intr-un singur send() -- testeaza bucla interioara
// din HandleConnection care extrage toate mesajele complete deodata.
// ══════════════════════════════════════════════════════════════

TEST(TcpServerTest, MultipleMessagesInOneSend_AllEventuallyReachCoreEngine) {
    constexpr int kPort = 54002;
    const std::string symbol = "TCP_MULTI1";

    CoreEngine engine;
    TcpServer server(kPort, engine);
    server.Start();

    int clientSocket = ConnectToServer(kPort);
    ASSERT_NE(clientSocket, -1);

    std::vector<uint8_t> combined;
    for (int id : {1, 2, 3}) {
        auto bytes = Encode(MakeRequest(id, symbol));
        combined.insert(combined.end(), bytes.begin(), bytes.end());
    }
    send(clientSocket, combined.data(), combined.size(), 0);
    close(clientSocket);

    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 3; }, 2s));

    std::set<int> traderIds;
    for (const auto& order : engine.GetOrders(symbol)) {
        traderIds.insert(order.GetTraderID());
    }
    EXPECT_EQ(traderIds, (std::set<int>{1, 2, 3}))
                        << "Mesajele nu au fost toate extrase corect din buffer-ul de conexiune";

    server.Stop();
}

// ══════════════════════════════════════════════════════════════
// Un singur mesaj, trimis in DOUA send()-uri separate -- testeaza
// acumularea reala intre citiri partiale, pe un socket adevarat.
// ══════════════════════════════════════════════════════════════

TEST(TcpServerTest, MessageSplitAcrossTwoSends_StillReachesCoreEngine) {
    constexpr int kPort = 54003;
    const std::string symbol = "TCP_SPLIT1";

    CoreEngine engine;
    TcpServer server(kPort, engine);
    server.Start();

    int clientSocket = ConnectToServer(kPort);
    ASSERT_NE(clientSocket, -1);

    auto bytes = Encode(MakeRequest(77, symbol));
    size_t splitPoint = bytes.size() / 2;

    send(clientSocket, bytes.data(), splitPoint, 0);
    std::this_thread::sleep_for(100ms); // simuleaza intarzierea reala de retea
    send(clientSocket, bytes.data() + splitPoint, bytes.size() - splitPoint, 0);
    close(clientSocket);

    ASSERT_TRUE(WaitUntil([&]() { return engine.GetOrders(symbol).size() == 1; }, 2s))
                                << "Mesajul trimis in doua bucati nu a fost reasamblat corect";
    EXPECT_EQ(engine.GetOrders(symbol)[0].GetTraderID(), 77);

    server.Stop();
}

// ══════════════════════════════════════════════════════════════
// Mai multi clienti reali, simultan, fiecare cu propria conexiune --
// verifica ca fiecare thread de conexiune functioneaza corect
// concurent, prin tot lantul pana in CoreEngine, fara pierderi.
// ══════════════════════════════════════════════════════════════

TEST(TcpServerTest, MultipleConcurrentClients_AllReachCoreEngineNoLossNoDuplication) {
    constexpr int kPort = 54004;
    constexpr int kClientCount = 10;
    const std::string symbol = "TCP_CONCURRENT1";

    CoreEngine engine;
    TcpServer server(kPort, engine);
    server.Start();

    std::vector<std::thread> clients;
    for (int i = 0; i < kClientCount; ++i) {
        clients.emplace_back([i, kPort, &symbol]() {
            int sock = ConnectToServer(kPort);
            if (sock == -1) return;
            auto bytes = Encode(MakeRequest(i, symbol));
            send(sock, bytes.data(), bytes.size(), 0);
            close(sock);
        });
    }
    for (auto& t : clients) t.join();

    ASSERT_TRUE(WaitUntil([&]() {
        return engine.GetOrders(symbol).size() == static_cast<size_t>(kClientCount);
    }, 5s)) << "Primite: " << engine.GetOrders(symbol).size() << " / " << kClientCount;

    std::set<int> traderIds;
    for (const auto& order : engine.GetOrders(symbol)) {
        traderIds.insert(order.GetTraderID());
    }
    EXPECT_EQ(traderIds.size(), static_cast<size_t>(kClientCount))
                        << "Unele mesaje au fost pierdute sau duplicate";

    server.Stop();
}