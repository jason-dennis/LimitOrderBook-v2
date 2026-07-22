#include <gtest/gtest.h>
#include "../../include/Engine/MatchingEngine.h"
#include "../../include/Storage/MultisetOrderBookStorage.h"
#include "../../include/Storage/BinaryOrderBookStorage.h"
#include "../../include/Domain/order.h"
#include "Domain/TradePod.h"
#include "Memory/OrderPool.h"
#include <chrono>
#include <atomic>
#include <vector>
#include <stdexcept>

// ─────────────────────────────────────────────
// Fixture
// ─────────────────────────────────────────────

class MatchingEngineTest : public ::testing::Test {
protected:
    std::atomic<int> TradeCounter_;
    OrderPool pool;
    MultisetOrderBook book;
    MatchingEngine<MultisetOrderBook> engine;

    std::chrono::system_clock::time_point Now = std::chrono::system_clock::now();

    // Constructor pentru inițializarea corectă în ordinea dependențelor
    MatchingEngineTest()
            : TradeCounter_(1),
              pool(),
              book(pool),
              engine(book, TradeCounter_, pool) {}

    uint32_t MakeBuy(int id, uint64_t price, int qty,
                     OrderType type = OrderType::LIMIT,
                     TimeInForce tif = TimeInForce::GTC) {
        uint32_t index = pool.Allocate();
        if (index == UINT32_MAX) {
            throw std::runtime_error("OrderPool is full!");
        }

        auto& order = pool.Get(index);
        order.OrderId = id;
        order.TraderId = id * 10;
        order.Side = OrderSide::BUY;
        order.Type = type;
        order.Symbol = 12345; // uint32_t representation of pair
        order.Price = price;
        order.Quantity = qty;
        order.TIF = tif;
        order.Status = OrderStatus::NEW;
        order.Timestamp = 0;
        return index;
    }

    uint32_t MakeSell(int id, uint64_t price, int qty,
                      OrderType type = OrderType::LIMIT,
                      TimeInForce tif = TimeInForce::GTC) {
        uint32_t index = pool.Allocate();
        if (index == UINT32_MAX) {
            throw std::runtime_error("OrderPool is full!");
        }

        auto& order = pool.Get(index);
        order.OrderId = id;
        order.TraderId = id * 10;
        order.Side = OrderSide::SELL;
        order.Type = type;
        order.Symbol = 12345;
        order.Price = price;
        order.Quantity = qty;
        order.TIF = tif;
        order.Status = OrderStatus::NEW;
        order.Timestamp = 0;
        return index;
    }
};

// ═════════════════════════════════════════════
// ProcessBuyLimit — GTC
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, BuyLimit_GTC_NoMatch_AddsToBook) {
    auto buy = MakeBuy(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_FALSE(book.IsBidEmpty());
}

TEST_F(MatchingEngineTest, BuyLimit_GTC_FullMatch_NoRemainder) {
    auto sell = MakeSell(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades); // resting ask

    trades.clear();
    auto buy = MakeBuy(2, 100, 10);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 10);
    EXPECT_TRUE(book.IsBidEmpty());
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MatchingEngineTest, BuyLimit_GTC_PartialMatch_RemainderAddedToBook) {
    auto sell = MakeSell(1, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades); // resting ask of 5

    trades.clear();
    auto buy = MakeBuy(2, 100, 10);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 5);
    EXPECT_FALSE(book.IsBidEmpty());
    EXPECT_EQ(pool.Get(book.GetBestBid()).Quantity, 5);
}

TEST_F(MatchingEngineTest, BuyLimit_GTC_PriceTooLow_NoMatch_AddsToBook) {
    auto sell = MakeSell(1, 110, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 10);
    engine.ProcessOrder(buy, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_FALSE(book.IsBidEmpty());
    EXPECT_FALSE(book.IsAskEmpty());
}

TEST_F(MatchingEngineTest, BuyLimit_GTC_MultipleAsks_MatchesMultiple) {
    auto sell1 = MakeSell(1, 100, 5);
    auto sell2 = MakeSell(2, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell1, trades);
    engine.ProcessOrder(sell2, trades);

    trades.clear();
    auto buy = MakeBuy(3, 100, 10);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 2u);
    EXPECT_TRUE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// ProcessBuyLimit — IOC
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, BuyLimit_IOC_PartialMatch_RemainderDiscarded) {
    auto sell = MakeSell(1, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 10, OrderType::LIMIT, TimeInForce::IOC);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 5);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MatchingEngineTest, BuyLimit_IOC_NoMatch_NothingAdded) {
    auto buy = MakeBuy(1, 100, 10, OrderType::LIMIT, TimeInForce::IOC);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(book.IsBidEmpty());
}

// ═════════════════════════════════════════════
// ProcessBuyLimit — FOK
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, BuyLimit_FOK_CanFill_ExecutesFully) {
    auto sell = MakeSell(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 10, OrderType::LIMIT, TimeInForce::FOK);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 10);
}

TEST_F(MatchingEngineTest, BuyLimit_FOK_CannotFill_NoTrades) {
    auto sell = MakeSell(1, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 10, OrderType::LIMIT, TimeInForce::FOK);
    engine.ProcessOrder(buy, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_FALSE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// ProcessBuyMarket
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, BuyMarket_GTC_FullMatch) {
    auto sell = MakeSell(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 0, 10, OrderType::MARKET, TimeInForce::GTC);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 10);
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MatchingEngineTest, BuyMarket_GTC_NoAsks_NoTrades) {
    auto buy = MakeBuy(1, 0, 10, OrderType::MARKET, TimeInForce::GTC);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);
    EXPECT_TRUE(trades.empty());
}

TEST_F(MatchingEngineTest, BuyMarket_FOK_CanFill_Executes) {
    auto sell = MakeSell(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 10, OrderType::MARKET, TimeInForce::FOK);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 1u);
}

TEST_F(MatchingEngineTest, BuyMarket_FOK_CannotFill_NoTrades) {
    auto sell = MakeSell(1, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 10, OrderType::MARKET, TimeInForce::FOK);
    engine.ProcessOrder(buy, trades);
    EXPECT_TRUE(trades.empty());
}

// ═════════════════════════════════════════════
// ProcessSellLimit — GTC
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, SellLimit_GTC_NoMatch_AddsToBook) {
    auto sell = MakeSell(1, 110, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_FALSE(book.IsAskEmpty());
}

TEST_F(MatchingEngineTest, SellLimit_GTC_FullMatch_NoRemainder) {
    auto buy = MakeBuy(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 10);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 10);
    EXPECT_TRUE(book.IsBidEmpty());
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MatchingEngineTest, SellLimit_GTC_PartialMatch_RemainderAddedToBook) {
    auto buy = MakeBuy(1, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 10);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 5);
    EXPECT_FALSE(book.IsAskEmpty());
    EXPECT_EQ(pool.Get(book.GetBestAsk()).Quantity, 5);
}

TEST_F(MatchingEngineTest, SellLimit_GTC_PriceTooHigh_NoMatch_AddsToBook) {
    auto buy = MakeBuy(1, 90, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 10);
    engine.ProcessOrder(sell, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_FALSE(book.IsAskEmpty());
    EXPECT_FALSE(book.IsBidEmpty());
}

TEST_F(MatchingEngineTest, SellLimit_GTC_MultipleBids_MatchesMultiple) {
    auto buy1 = MakeBuy(1, 100, 5);
    auto buy2 = MakeBuy(2, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy1, trades);
    engine.ProcessOrder(buy2, trades);

    trades.clear();
    auto sell = MakeSell(3, 100, 10);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 2u);
    EXPECT_TRUE(book.IsBidEmpty());
}

// ═════════════════════════════════════════════
// ProcessSellLimit — IOC
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, SellLimit_IOC_PartialMatch_RemainderDiscarded) {
    auto buy = MakeBuy(1, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 10, OrderType::LIMIT, TimeInForce::IOC);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MatchingEngineTest, SellLimit_IOC_NoMatch_NothingAdded) {
    auto sell = MakeSell(1, 100, 10, OrderType::LIMIT, TimeInForce::IOC);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_TRUE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// ProcessSellLimit — FOK
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, SellLimit_FOK_CanFill_ExecutesFully) {
    auto buy = MakeBuy(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 10, OrderType::LIMIT, TimeInForce::FOK);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 10);
}

TEST_F(MatchingEngineTest, SellLimit_FOK_CannotFill_NoTrades) {
    auto buy = MakeBuy(1, 100, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 10, OrderType::LIMIT, TimeInForce::FOK);
    engine.ProcessOrder(sell, trades);
    EXPECT_TRUE(trades.empty());
    EXPECT_FALSE(book.IsBidEmpty());
}

// ═════════════════════════════════════════════
// ProcessSellMarket
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, SellMarket_GTC_FullMatch) {
    auto buy = MakeBuy(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 0, 10, OrderType::MARKET, TimeInForce::GTC);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 10);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MatchingEngineTest, SellMarket_GTC_NoBids_NoTrades) {
    auto sell = MakeSell(1, 0, 10, OrderType::MARKET, TimeInForce::GTC);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);
    EXPECT_TRUE(trades.empty());
}

// ═════════════════════════════════════════════
// Trade fields correctness
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, Trade_HasCorrectPrice) {
    auto sell = MakeSell(1, 105, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 105, 10);
    engine.ProcessOrder(buy, trades);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Price, 105u);
}

TEST_F(MatchingEngineTest, Trade_HasCorrectMakerAndTakerID) {
    auto sell = MakeSell(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 10);
    engine.ProcessOrder(buy, trades);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].MakerId, 10);
    EXPECT_EQ(trades[0].TakerId, 20);
}

// ═════════════════════════════════════════════
// GetOrderBook
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, GetOrderBook_ReturnsCorrectState) {
    auto buy = MakeBuy(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);
    const auto& ob = engine.GetOrderBook();
    EXPECT_FALSE(ob.IsBidEmpty());
    EXPECT_TRUE(ob.IsAskEmpty());
}

// ═════════════════════════════════════════════
// Integration scenarios
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, Integration_BuyAndSell_CrossingPrices_Match) {
    auto buy = MakeBuy(1, 105, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 10);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_TRUE(book.IsBidEmpty());
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MatchingEngineTest, Integration_MultipleOrders_CorrectMatchOrder) {
    auto buy1 = MakeBuy(1, 100, 5);
    auto buy2 = MakeBuy(2, 105, 5);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy1, trades);
    engine.ProcessOrder(buy2, trades);

    trades.clear();
    auto sell = MakeSell(3, 100, 5);
    engine.ProcessOrder(sell, trades);
    ASSERT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Price, 105u);
}

// ═════════════════════════════════════════════
// UpdateQuantity correctness
// ═════════════════════════════════════════════

TEST_F(MatchingEngineTest, SellLimit_GTC_PartialMatch_BidQuantityUpdatedCorrectly) {
    auto buy = MakeBuy(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(buy, trades);

    trades.clear();
    auto sell = MakeSell(2, 100, 4);
    engine.ProcessOrder(sell, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 4);
    EXPECT_FALSE(book.IsBidEmpty());
    EXPECT_EQ(pool.Get(book.GetBestBid()).Quantity, 6);
}

TEST_F(MatchingEngineTest, BuyLimit_GTC_PartialMatch_AskQuantityUpdatedCorrectly) {
    auto sell = MakeSell(1, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell, trades);

    trades.clear();
    auto buy = MakeBuy(2, 100, 4);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 1u);
    EXPECT_EQ(trades[0].Quantity, 4);
    EXPECT_FALSE(book.IsAskEmpty());
    EXPECT_EQ(pool.Get(book.GetBestAsk()).Quantity, 6);
}

TEST_F(MatchingEngineTest, BuyLimit_GTC_MultipleAsks_PopAndUpdateInSameOrder) {
    auto sell1 = MakeSell(1, 100, 3);
    auto sell2 = MakeSell(2, 100, 10);
    std::vector<TradePod> trades;
    engine.ProcessOrder(sell1, trades);
    engine.ProcessOrder(sell2, trades);

    trades.clear();
    auto buy = MakeBuy(3, 100, 8);
    engine.ProcessOrder(buy, trades);
    EXPECT_EQ(trades.size(), 2u);
    EXPECT_EQ(trades[0].Quantity, 3);
    EXPECT_EQ(trades[1].Quantity, 5);
    EXPECT_FALSE(book.IsAskEmpty());
    EXPECT_EQ(pool.Get(book.GetBestAsk()).Quantity, 5);
}

// ═════════════════════════════════════════════
// Regresie: Pool_.Free() pe toate caile posibile
// (adauga aceste teste in acelasi fisier, in fixture-ul MatchingEngineTest)
// ═════════════════════════════════════════════

// Tinteste bug-ul: ProcessBuyMarket/ProcessSellMarket nu eliberau
// niciodata slotul dupa matching, indiferent de rezultat.
TEST_F(MatchingEngineTest, MarketOrders_NeverLeakPoolSlots) {
    std::vector<TradePod> trades;

    ASSERT_NO_THROW({
                        for (uint32_t i = 0; i < DefaultCapacity + 1000; ++i) {
                            // fara lichiditate in carte -- MARKET-ul nu se matcheaza
                            // niciodata, dar tot trebuie eliberat imediat
                            auto buy = MakeBuy(static_cast<int>(i), 0, 10,
                                               OrderType::MARKET, TimeInForce::GTC);
                            trades.clear();
                            engine.ProcessOrder(buy, trades);
                        }
                    }) << "Pool-ul s-a epuizat -- ordinele MARKET nu se elibereaza corect";

    // confirmare suplimentara: pool-ul chiar mai are capacitate libera
    uint32_t testAlloc = pool.Allocate();
    EXPECT_NE(testAlloc, UINT32_MAX);
}

// Tinteste bug-ul: pe ramura de succes FOK, slotul nu se elibera
// niciodata (doar pe ramura de esec/REJECTED se elibera).
TEST_F(MatchingEngineTest, FOK_SuccessfulFill_NeverLeaksPoolSlots) {
    std::vector<TradePod> trades;

    ASSERT_NO_THROW({
                        for (uint32_t i = 0; i < DefaultCapacity; ++i) {
                            // resting sell care va fi matchat COMPLET de FOK-ul care urmeaza
                            auto sell = MakeSell(static_cast<int>(i) * 2, 100, 5);
                            trades.clear();
                            engine.ProcessOrder(sell, trades);

                            auto buy = MakeBuy(static_cast<int>(i) * 2 + 1, 100, 5,
                                               OrderType::LIMIT, TimeInForce::FOK);
                            trades.clear();
                            engine.ProcessOrder(buy, trades);
                        }
                    }) << "Pool-ul s-a epuizat -- FOK reusit nu elibereaza slotul ordinii incoming";

    EXPECT_TRUE(book.IsBidEmpty());
    EXPECT_TRUE(book.IsAskEmpty());
}

// Tinteste bug-ul: pe ramura LIMIT non-FOK, cand ordinea NU ajunge in
// carte (IOC fara match complet, sau GTC dar cantitate ramasa 0 dupa
// match complet), slotul nu se elibera.
TEST_F(MatchingEngineTest, IOC_UnmatchedRemainder_NeverLeaksPoolSlots) {
    std::vector<TradePod> trades;

    ASSERT_NO_THROW({
                        for (uint32_t i = 0; i < DefaultCapacity + 1000; ++i) {
                            // fara lichiditate opusa -- IOC-ul nu se matcheaza deloc,
                            // dar tot nu trebuie sa ramana in carte (nu e GTC)
                            auto buy = MakeBuy(static_cast<int>(i), 100, 10,
                                               OrderType::LIMIT, TimeInForce::IOC);
                            trades.clear();
                            engine.ProcessOrder(buy, trades);
                        }
                    }) << "Pool-ul s-a epuizat -- IOC nematchat nu elibereaza slotul";

    EXPECT_TRUE(book.IsBidEmpty());
}

// Tinteste bug-ul: LIMIT GTC care se matcheaza COMPLET (Quantity ajunge
// la 0) nu trecea niciodata prin AddOrder, dar nici nu se elibera --
// exact conditia "GTC and Quantity > 0" fiind falsa din motivul gresit.
TEST_F(MatchingEngineTest, FullyMatchedLimitGTC_NeverLeaksPoolSlots) {
    std::vector<TradePod> trades;

    ASSERT_NO_THROW({
                        for (uint32_t i = 0; i < DefaultCapacity; ++i) {
                            auto sell = MakeSell(static_cast<int>(i) * 2, 100, 5);
                            trades.clear();
                            engine.ProcessOrder(sell, trades);

                            // GTC, nu FOK -- dar se matcheaza complet oricum, Quantity -> 0
                            auto buy = MakeBuy(static_cast<int>(i) * 2 + 1, 100, 5,
                                               OrderType::LIMIT, TimeInForce::GTC);
                            trades.clear();
                            engine.ProcessOrder(buy, trades);
                        }
                    }) << "Pool-ul s-a epuizat -- LIMIT GTC complet matchat nu elibereaza slotul";

    EXPECT_TRUE(book.IsBidEmpty());
    EXPECT_TRUE(book.IsAskEmpty());
}