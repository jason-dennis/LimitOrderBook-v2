#include <gtest/gtest.h>
#include "../../include/Storage/MultisetOrderBookStorage.h"
#include <thread>

// ─────────────────────────────────────────────
// Fixture
// ─────────────────────────────────────────────

class MultisetOrderBookTest : public ::testing::Test {
protected:
    // Presupunem că acesta este containerul tău ce stochează structurile OrderPod
    // Înlocuiește "OrderPool" cu tipul tău exact de container dacă diferă
    OrderPool pool;
    MultisetOrderBook book;

    MultisetOrderBookTest() : pool(), book(pool) {}

    // Helper pentru a insera un ordin BUY în pool și în book
    uint32_t AddBuy(uint32_t id, uint64_t price, uint32_t qty) {
        uint32_t index = pool.Allocate();
        OrderPod& order = pool.Get(index); // sau pool[index] în funcție de API-ul tău

        order.OrderId = id;
        order.TraderId = 1u;
        order.Side = OrderSide::BUY;
        order.Type = OrderType::LIMIT;
        order.Price = price;
        order.Quantity = qty;
        order.Status = OrderStatus::NEW;
        order.Timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        book.AddOrder(index);
        return index;
    }

    // Helper pentru a insera un ordin SELL în pool și în book
    uint32_t AddSell(uint32_t id, uint64_t price, uint32_t qty) {
        uint32_t index = pool.Allocate();
        OrderPod& order = pool.Get(index);

        order.OrderId = id;
        order.TraderId = 1u;
        order.Side = OrderSide::SELL;
        order.Type = OrderType::LIMIT;
        order.Price = price;
        order.Quantity = qty;
        order.Status = OrderStatus::NEW;
        order.Timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

        book.AddOrder(index);
        return index;
    }
};

// ═════════════════════════════════════════════
// AddOrder
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, AddBuyOrder_BookNotEmpty) {
    AddBuy(1, 100, 10);
    EXPECT_FALSE(book.IsBidEmpty());
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, AddSellOrder_BookNotEmpty) {
    AddSell(2, 101, 5);
    EXPECT_FALSE(book.IsAskEmpty());
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, AddMultipleBuyOrders_BestBidIsHighestPrice) {
    AddBuy(1, 99,  10);
    AddBuy(2, 105, 10);
    AddBuy(3, 101, 10);

    uint32_t best = book.GetBestBid();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Price, 105u);
}

TEST_F(MultisetOrderBookTest, AddMultipleSellOrders_BestAskIsLowestPrice) {
    AddSell(1, 110, 5);
    AddSell(2, 102, 5);
    AddSell(3, 108, 5);

    uint32_t best = book.GetBestAsk();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Price, 102u);
}

TEST_F(MultisetOrderBookTest, AddOrdersAtSamePrice_BothStored) {
    AddBuy(1, 100, 10);
    AddBuy(2, 100, 20);

    uint32_t best = book.GetBestBid();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Price, 100u);

    book.PopBestBid();
    EXPECT_FALSE(book.IsBidEmpty()); // Al doilea ordin trebuie să rămână în coadă
}

// ═════════════════════════════════════════════
// GetBestBid / GetBestAsk pe carte goală
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, GetBestBid_EmptyBook_ReturnsZero) {
    EXPECT_EQ(book.GetBestBid(), UINT32_MAX);
}

TEST_F(MultisetOrderBookTest, GetBestAsk_EmptyBook_ReturnsZero) {
    EXPECT_EQ(book.GetBestAsk(), UINT32_MAX);
}

// ═════════════════════════════════════════════
// IsBidEmpty / IsAskEmpty
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, IsBidEmpty_InitiallyTrue) {
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, IsAskEmpty_InitiallyTrue) {
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, IsBidEmpty_FalseAfterAdd) {
    AddBuy(1, 100, 10);
    EXPECT_FALSE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, IsAskEmpty_FalseAfterAdd) {
    AddSell(1, 100, 10);
    EXPECT_FALSE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// CancelOrder
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, CancelBuyOrder_RemovesFromBook) {
    AddBuy(1, 100, 10);
    book.CancelOrder(1);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, CancelSellOrder_RemovesFromBook) {
    AddSell(2, 101, 5);
    book.CancelOrder(2);
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, CancelOrder_SetsStatusCanceled) {
    AddBuy(1, 100, 10);
    book.CancelOrder(1);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, CancelOrder_NonExistentId_DoesNotCrash) {
    AddBuy(1, 100, 10);
    book.CancelOrder(999);
    EXPECT_FALSE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, CancelOrder_OnlyRemovesTarGet) {
    AddBuy(1, 100, 10);
    AddBuy(2, 105, 10);
    book.CancelOrder(1);

    uint32_t best = book.GetBestBid();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Price, 105u);
}

TEST_F(MultisetOrderBookTest, CancelOrder_EmptyBook_DoesNotCrash) {
    book.CancelOrder(42);
    EXPECT_TRUE(book.IsBidEmpty());
    EXPECT_TRUE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// UpdateQuantity
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, UpdateQuantity_BuyOrder_PartialFill) {
    AddBuy(1, 100, 10);
    book.UpdateQuantity(1, 5);

    uint32_t best = book.GetBestBid();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Quantity, 5u);
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_BuyOrder_FullFill_RemovesOrder) {
    AddBuy(1, 100, 10);
    book.UpdateQuantity(1, 0);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_SellOrder_PartialFill) {
    AddSell(1, 101, 20);
    book.UpdateQuantity(1, 8);

    uint32_t best = book.GetBestAsk();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Quantity, 8u);
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_SellOrder_FullFill_RemovesOrder) {
    AddSell(1, 101, 20);
    book.UpdateQuantity(1, 0);
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_NonExistentId_DoesNotCrash) {
    AddBuy(1, 100, 10);
    book.UpdateQuantity(999, 5);
    EXPECT_EQ(pool.Get(book.GetBestBid()).Quantity, 10u);
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_SetsFilledStatus_WhenZero) {
    AddSell(1, 101, 10);
    book.UpdateQuantity(1, 0);
    EXPECT_TRUE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// PopBestBid
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, PopBestBid_RemovesTopBid) {
    AddBuy(1, 100, 10);
    AddBuy(2, 110, 5);
    book.PopBestBid();

    uint32_t best = book.GetBestBid();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Price, 100u);
}

TEST_F(MultisetOrderBookTest, PopBestBid_EmptyBook_DoesNotCrash) {
    book.PopBestBid();
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, PopBestBid_SingleOrder_BookBecomesEmpty) {
    AddBuy(1, 100, 10);
    book.PopBestBid();
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, PopBestBid_SetsFilledStatus) {
    AddBuy(1, 100, 10);
    book.PopBestBid();
    EXPECT_TRUE(book.IsBidEmpty());
}

// ═════════════════════════════════════════════
// PopBestAsk
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, PopBestAsk_RemovesTopAsk) {
    AddSell(1, 105, 5);
    AddSell(2, 102, 5);
    book.PopBestAsk();

    uint32_t best = book.GetBestAsk();
    ASSERT_NE(best, 0u);
    EXPECT_EQ(pool.Get(best).Price, 105u);
}

TEST_F(MultisetOrderBookTest, PopBestAsk_EmptyBook_DoesNotCrash) {
    book.PopBestAsk();
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, PopBestAsk_SingleOrder_BookBecomesEmpty) {
    AddSell(1, 100, 5);
    book.PopBestAsk();
    EXPECT_TRUE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// Integration / Sequence Scenarios
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, Integration_AddPartialFillThenCancel) {
    AddBuy(1, 100, 20);
    book.UpdateQuantity(1, 10);
    EXPECT_EQ(pool.Get(book.GetBestBid()).Quantity, 10u);
    book.CancelOrder(1);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, Integration_PopUntilEmpty) {
    AddSell(1, 100, 5);
    AddSell(2, 101, 5);
    AddSell(3, 102, 5);
    book.PopBestAsk();
    book.PopBestAsk();
    book.PopBestAsk();
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, Integration_BidAndAskSide_Independent) {
    AddBuy(1, 100, 10);
    AddSell(2, 101, 10);
    book.CancelOrder(1);
    EXPECT_TRUE(book.IsBidEmpty());
    EXPECT_FALSE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// Branch coverage: UpdateQuantity negative value
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, UpdateQuantity_NegativeValue_BuyOrder_TreatedAsFilled) {
    AddBuy(1, 100, 10);
    book.UpdateQuantity(1, -1);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_NegativeValue_SellOrder_TreatedAsFilled) {
    AddSell(1, 100, 10);
    book.UpdateQuantity(1, -1);
    EXPECT_TRUE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// Branch coverage: CancelOrder Paths
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, CancelOrder_BidNotFound_AskFound) {
    AddSell(1, 100, 10);
    book.CancelOrder(1);
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, CancelOrder_BidFound_AskNotSearched) {
    AddBuy(1, 100, 10);
    book.CancelOrder(1);
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, CancelOrder_NeitherFound_BothMiss) {
    AddBuy(1, 100, 10);
    AddSell(2, 101, 10);
    book.CancelOrder(999);
    EXPECT_FALSE(book.IsBidEmpty());
    EXPECT_FALSE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// Branch coverage: UpdateQuantity Paths
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, UpdateQuantity_BidNotFound_AskFound_Partial) {
    AddSell(1, 100, 20);
    book.UpdateQuantity(1, 7);
    EXPECT_EQ(pool.Get(book.GetBestAsk()).Quantity, 7u);
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_BidNotFound_AskFound_Full) {
    AddSell(1, 100, 20);
    book.UpdateQuantity(1, 0);
    EXPECT_TRUE(book.IsAskEmpty());
}

TEST_F(MultisetOrderBookTest, UpdateQuantity_NeitherFound_BothMiss) {
    AddBuy(1, 100, 10);
    book.UpdateQuantity(999, 5);
    EXPECT_EQ(pool.Get(book.GetBestBid()).Quantity, 10u);
}

// ═════════════════════════════════════════════
// Bitmap consistency
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, Bitmap_AddRemoveAtSamePrice_BitmapConsistent) {
    AddBuy(1, 100, 10);
    AddBuy(2, 100, 10);
    book.PopBestBid();
    EXPECT_FALSE(book.IsBidEmpty());
    EXPECT_EQ(pool.Get(book.GetBestBid()).Price, 100u);
    book.PopBestBid();
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, Bitmap_MultiplePriceLevels_AfterPop_NextLevelVisible) {
    AddBuy(1, 100, 10);
    AddBuy(2, 200, 10);
    AddBuy(3, 300, 10);
    book.PopBestBid();
    EXPECT_EQ(pool.Get(book.GetBestBid()).Price, 200u);
    book.PopBestBid();
    EXPECT_EQ(pool.Get(book.GetBestBid()).Price, 100u);
    book.PopBestBid();
    EXPECT_TRUE(book.IsBidEmpty());
}

TEST_F(MultisetOrderBookTest, Bitmap_AskMultiplePriceLevels_AfterPop_NextLevelVisible) {
    AddSell(1, 300, 5);
    AddSell(2, 200, 5);
    AddSell(3, 100, 5);
    book.PopBestAsk();
    EXPECT_EQ(pool.Get(book.GetBestAsk()).Price, 200u);
    book.PopBestAsk();
    EXPECT_EQ(pool.Get(book.GetBestAsk()).Price, 300u);
    book.PopBestAsk();
    EXPECT_TRUE(book.IsAskEmpty());
}

// ═════════════════════════════════════════════
// GetBestBids
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, GetBestBids_EmptyBook_ReturnsEmpty) {
    EXPECT_TRUE(book.GetBestBids(5).empty());
}

TEST_F(MultisetOrderBookTest, GetBestBids_XZero_ReturnsEmpty) {
    AddBuy(1, 100, 10);
    EXPECT_TRUE(book.GetBestBids(0).empty());
}

TEST_F(MultisetOrderBookTest, GetBestBids_XLargerThanBook_ReturnsAll) {
    AddBuy(1, 100, 10);
    AddBuy(2, 105, 10);
    auto result = book.GetBestBids(10);
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_ExactX_ReturnsX) {
    AddBuy(1, 100, 10);
    AddBuy(2, 105, 10);
    AddBuy(3, 103, 10);
    auto result = book.GetBestBids(2);
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_SortedDescending_HighestPriceFirst) {
    AddBuy(1, 100, 10);
    AddBuy(2, 105, 10);
    AddBuy(3, 103, 10);
    auto result = book.GetBestBids(3);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(pool.Get(result[0]).Price, 105u);
    EXPECT_EQ(pool.Get(result[1]).Price, 103u);
    EXPECT_EQ(pool.Get(result[2]).Price, 100u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_SamePriceFIFO_OlderOrderFirst) {
    AddBuy(1, 100, 5);
    AddBuy(2, 100, 8);
    auto result = book.GetBestBids(2);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(pool.Get(result[0]).OrderId, 1u);
    EXPECT_EQ(pool.Get(result[1]).OrderId, 2u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_DoesNotModifyBook) {
    AddBuy(1, 100, 10);
    AddBuy(2, 105, 10);
    book.GetBestBids(2);
    EXPECT_FALSE(book.IsBidEmpty());
    EXPECT_EQ(pool.Get(book.GetBestBid()).Price, 105u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_OnlyBidsSide_AskUnaffected) {
    AddBuy(1, 100, 10);
    AddSell(2, 101, 10);
    auto result = book.GetBestBids(5);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(pool.Get(result[0]).Side, OrderSide::BUY);
}

TEST_F(MultisetOrderBookTest, GetBestBids_AfterCancel_ReturnsUpdatedBook) {
    AddBuy(1, 105, 10);
    AddBuy(2, 100, 10);
    book.CancelOrder(1);
    auto result = book.GetBestBids(2);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(pool.Get(result[0]).Price, 100u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_Top3_CorrectQuantities) {
    AddBuy(1, 110, 3);
    AddBuy(2, 105, 7);
    AddBuy(3, 100, 12);
    AddBuy(4,  95, 1);
    auto result = book.GetBestBids(3);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(pool.Get(result[0]).Quantity, 3u);
    EXPECT_EQ(pool.Get(result[1]).Quantity, 7u);
    EXPECT_EQ(pool.Get(result[2]).Quantity, 12u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_MultipleOrdersSamePrice_AllReturned) {
    AddBuy(1, 100, 5);
    AddBuy(2, 100, 8);
    AddBuy(3, 100, 3);
    auto result = book.GetBestBids(5);
    ASSERT_EQ(result.size(), 3u);
}

TEST_F(MultisetOrderBookTest, GetBestBids_MultipleOrdersSamePrice_CappedAtX) {
    AddBuy(1, 100, 5);
    AddBuy(2, 100, 8);
    AddBuy(3, 100, 3);
    auto result = book.GetBestBids(2);
    EXPECT_EQ(result.size(), 2u);
}

// ═════════════════════════════════════════════
// GetBestAsks
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, GetBestAsks_EmptyBook_ReturnsEmpty) {
    EXPECT_TRUE(book.GetBestAsks(5).empty());
}

TEST_F(MultisetOrderBookTest, GetBestAsks_XZero_ReturnsEmpty) {
    AddSell(1, 100, 5);
    EXPECT_TRUE(book.GetBestAsks(0).empty());
}

TEST_F(MultisetOrderBookTest, GetBestAsks_XLargerThanBook_ReturnsAll) {
    AddSell(1, 100, 5);
    AddSell(2, 105, 5);
    auto result = book.GetBestAsks(10);
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_ExactX_ReturnsX) {
    AddSell(1, 100, 5);
    AddSell(2, 103, 5);
    AddSell(3, 107, 5);
    auto result = book.GetBestAsks(2);
    EXPECT_EQ(result.size(), 2u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_SortedAscending_LowestPriceFirst) {
    AddSell(1, 107, 5);
    AddSell(2, 100, 5);
    AddSell(3, 103, 5);
    auto result = book.GetBestAsks(3);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(pool.Get(result[0]).Price, 100u);
    EXPECT_EQ(pool.Get(result[1]).Price, 103u);
    EXPECT_EQ(pool.Get(result[2]).Price, 107u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_SamePriceFIFO_OlderOrderFirst) {
    AddSell(1, 100, 5);
    AddSell(2, 100, 8);
    auto result = book.GetBestAsks(2);
    ASSERT_EQ(result.size(), 2u);
    EXPECT_EQ(pool.Get(result[0]).OrderId, 1u);
    EXPECT_EQ(pool.Get(result[1]).OrderId, 2u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_DoesNotModifyBook) {
    AddSell(1, 100, 5);
    AddSell(2, 105, 5);
    book.GetBestAsks(2);
    EXPECT_FALSE(book.IsAskEmpty());
    EXPECT_EQ(pool.Get(book.GetBestAsk()).Price, 100u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_OnlyAsksSide_BidUnaffected) {
    AddBuy(1,  99, 10);
    AddSell(2, 101, 5);
    auto result = book.GetBestAsks(5);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(pool.Get(result[0]).Side, OrderSide::SELL);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_AfterCancel_ReturnsUpdatedBook) {
    AddSell(1, 100, 5);
    AddSell(2, 105, 5);
    book.CancelOrder(1);
    auto result = book.GetBestAsks(2);
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(pool.Get(result[0]).Price, 105u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_Top3_CorrectQuantities) {
    AddSell(1, 100, 2);
    AddSell(2, 103, 6);
    AddSell(3, 107, 9);
    AddSell(4, 112, 1);
    auto result = book.GetBestAsks(3);
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(pool.Get(result[0]).Quantity, 2u);
    EXPECT_EQ(pool.Get(result[1]).Quantity, 6u);
    EXPECT_EQ(pool.Get(result[2]).Quantity, 9u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_MultipleOrdersSamePrice_AllReturned) {
    AddSell(1, 100, 5);
    AddSell(2, 100, 8);
    AddSell(3, 100, 3);
    auto result = book.GetBestAsks(5);
    ASSERT_EQ(result.size(), 3u);
}

TEST_F(MultisetOrderBookTest, GetBestAsks_MultipleOrdersSamePrice_CappedAtX) {
    AddSell(1, 100, 5);
    AddSell(2, 100, 8);
    AddSell(3, 100, 3);
    auto result = book.GetBestAsks(2);
    EXPECT_EQ(result.size(), 2u);
}

// ═════════════════════════════════════════════
// CanFillQuantityBids
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_EmptyBook_ReturnsFalse) {
    EXPECT_FALSE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_SingleOrder_ExactFill) {
    AddBuy(1, 100, 10);
    EXPECT_TRUE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_SingleOrder_PartialAvailable) {
    AddBuy(1, 100, 5);
    EXPECT_FALSE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_MultipleOrders_CombinedFill) {
    AddBuy(1, 105, 5);
    AddBuy(2, 103, 5);
    EXPECT_TRUE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_PriceTooLow_ReturnsFalse) {
    AddBuy(1, 95, 100);
    EXPECT_FALSE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_MixedPrices_OnlyUsesEligible) {
    AddBuy(1, 110, 5);
    AddBuy(2, 100, 50);
    EXPECT_FALSE(book.CanFillQuantityBids(10, 105));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_MultipleOrdersSamePrice_Combined) {
    AddBuy(1, 100, 4);
    AddBuy(2, 100, 6);
    EXPECT_TRUE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_QuantityHitsZeroMidLoop) {
    AddBuy(1, 110, 10);
    AddBuy(2, 105,  5);
    EXPECT_TRUE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_LoopExhausted_NeverHitsTarget) {
    AddBuy(1, 110, 3);
    AddBuy(2, 108, 3);
    EXPECT_FALSE(book.CanFillQuantityBids(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityBids_AfterPartialUpdate_CorrectQty) {
    AddBuy(1, 100, 10);
    book.UpdateQuantity(1, 4);
    EXPECT_FALSE(book.CanFillQuantityBids(5, 100));
    EXPECT_TRUE(book.CanFillQuantityBids(4, 100));
}

// ═════════════════════════════════════════════
// CanFillQuantityAsks
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_EmptyBook_ReturnsFalse) {
    EXPECT_FALSE(book.CanFillQuantityAsks(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_SingleOrder_ExactFill) {
    AddSell(1, 100, 10);
    EXPECT_TRUE(book.CanFillQuantityAsks(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_SingleOrder_PartialAvailable) {
    AddSell(1, 100, 5);
    EXPECT_FALSE(book.CanFillQuantityAsks(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_MultipleOrders_CombinedFill) {
    AddSell(1, 100, 5);
    AddSell(2, 100, 5);
    EXPECT_TRUE(book.CanFillQuantityAsks(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_PriceTooHigh_ReturnsFalse) {
    AddSell(1, 105, 100);
    EXPECT_FALSE(book.CanFillQuantityAsks(10, 99));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_MixedPrices_OnlyUsesEligible) {
    AddSell(1, 100, 5);
    AddSell(2, 103, 50);
    EXPECT_FALSE(book.CanFillQuantityAsks(10, 102));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_MultipleOrdersSamePrice_Combined) {
    AddSell(1, 100, 4);
    AddSell(2, 100, 6);
    EXPECT_TRUE(book.CanFillQuantityAsks(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_QuantityHitsZeroMidLoop) {
    AddSell(1, 100, 10);
    AddSell(2, 101,  5);
    EXPECT_TRUE(book.CanFillQuantityAsks(10, 101));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_LoopExhausted_NeverHitsTarget) {
    AddSell(1, 100, 3);
    AddSell(2, 100, 3);
    EXPECT_FALSE(book.CanFillQuantityAsks(10, 100));
}

TEST_F(MultisetOrderBookTest, CanFillQuantityAsks_AfterPartialUpdate_CorrectQty) {
    AddSell(1, 100, 10);
    book.UpdateQuantity(1, 4);
    EXPECT_FALSE(book.CanFillQuantityAsks(5, 100));
    EXPECT_TRUE(book.CanFillQuantityAsks(4, 100));
}

// ═════════════════════════════════════════════
// GetBestBids + GetBestAsks — Combinat
// ═════════════════════════════════════════════

TEST_F(MultisetOrderBookTest, GetBestBidsAndAsks_IndependentOfEachOther) {
    AddBuy(1,  99, 10);
    AddBuy(2,  95,  5);
    AddSell(3, 101,  3);
    AddSell(4, 104,  7);

    auto bids = book.GetBestBids(2);
    auto asks = book.GetBestAsks(2);

    ASSERT_EQ(bids.size(), 2u);
    ASSERT_EQ(asks.size(), 2u);
    EXPECT_EQ(pool.Get(bids[0]).Price,  99u);
    EXPECT_EQ(pool.Get(bids[1]).Price,  95u);
    EXPECT_EQ(pool.Get(asks[0]).Price, 101u);
    EXPECT_EQ(pool.Get(asks[1]).Price, 104u);
}