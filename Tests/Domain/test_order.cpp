#include <gtest/gtest.h>
#include "../../include/Domain/order.h"
#include <chrono>
#include <sstream>

class OrderTest : public ::testing::Test {
protected:
    std::chrono::system_clock::time_point Now = std::chrono::system_clock::now();

    Order CreateDefaultOrder() {
        return Order(1, 42, OrderSide::BUY, OrderType::LIMIT,
                     "BTCUSD", 100, 10, Now, TimeInForce::GTC, OrderStatus::NEW);
    }
};

// ==========================================
// 1. CONSTRUCTOR & GETTERS
// ==========================================

TEST_F(OrderTest, Constructor_StoresOrderID) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetOrderID(), 1);
}

TEST_F(OrderTest, Constructor_StoresTraderID) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetTraderID(), 42);
}

TEST_F(OrderTest, Constructor_StoresSide_Buy) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetSide(), OrderSide::BUY);
}

TEST_F(OrderTest, Constructor_StoresSide_Sell) {
    Order o(2, 10, OrderSide::SELL, OrderType::MARKET,
            "ETHUSD", 200, 5, Now, TimeInForce::IOC, OrderStatus::NEW);
    EXPECT_EQ(o.GetSide(), OrderSide::SELL);
}

TEST_F(OrderTest, Constructor_StoresType_Limit) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetType(), OrderType::LIMIT);
}

TEST_F(OrderTest, Constructor_StoresType_Market) {
    Order o(2, 10, OrderSide::BUY, OrderType::MARKET,
            "ETHUSD", 50, 5, Now, TimeInForce::IOC, OrderStatus::NEW);
    EXPECT_EQ(o.GetType(), OrderType::MARKET);
}

TEST_F(OrderTest, Constructor_StoresType_Stop) {
    Order o(3, 10, OrderSide::SELL, OrderType::STOP,
            "ETHUSD", 150, 5, Now, TimeInForce::GTC, OrderStatus::NEW);
    EXPECT_EQ(o.GetType(), OrderType::STOP);
}

TEST_F(OrderTest, Constructor_StoresType_StopLimit) {
    Order o(4, 10, OrderSide::BUY, OrderType::STOP_LIMIT,
            "ETHUSD", 150, 5, Now, TimeInForce::GTC, OrderStatus::NEW);
    EXPECT_EQ(o.GetType(), OrderType::STOP_LIMIT);
}

TEST_F(OrderTest, Constructor_StoresSymbol) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetSymbol(), "BTCUSD");
}

TEST_F(OrderTest, Constructor_StoresPrice) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetPrice(), 100u);
}

TEST_F(OrderTest, Constructor_StoresPriceLargeValue) {
    uint64_t bigPrice = 999999999999ULL;
    Order o(2, 10, OrderSide::BUY, OrderType::LIMIT,
            "BTCUSD", bigPrice, 5, Now, TimeInForce::GTC, OrderStatus::NEW);
    EXPECT_EQ(o.GetPrice(), bigPrice);
}

TEST_F(OrderTest, Constructor_ThrowsOnPriceZero) {
    EXPECT_THROW(
            Order(2, 10, OrderSide::BUY, OrderType::LIMIT,
                  "BTCUSD", 0, 5, Now, TimeInForce::GTC, OrderStatus::NEW),
            std::exception
    );
}

TEST_F(OrderTest, Constructor_StoresQuantity) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetQuantity(), 10);
}

TEST_F(OrderTest, Constructor_StoresTimestamp) {
    Order o = CreateDefaultOrder();
    EXPECT_TRUE(o.GetTime()== Now);
}

TEST_F(OrderTest, Constructor_StoresTIF_GTC) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetTIF(), TimeInForce::GTC);
}

TEST_F(OrderTest, Constructor_StoresTIF_IOC) {
    Order o(2, 10, OrderSide::BUY, OrderType::LIMIT,
            "BTCUSD", 100, 10, Now, TimeInForce::IOC, OrderStatus::NEW);
    EXPECT_EQ(o.GetTIF(), TimeInForce::IOC);
}

TEST_F(OrderTest, Constructor_StoresTIF_FOK) {
    Order o(2, 10, OrderSide::BUY, OrderType::LIMIT,
            "BTCUSD", 100, 10, Now, TimeInForce::FOK, OrderStatus::NEW);
    EXPECT_EQ(o.GetTIF(), TimeInForce::FOK);
}

TEST_F(OrderTest, Constructor_StoresStatus_New) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetStatus(), OrderStatus::NEW);
}

TEST_F(OrderTest, Constructor_StoresStatus_Rejected) {
    Order o(2, 10, OrderSide::BUY, OrderType::LIMIT,
            "BTCUSD", 100, 10, Now, TimeInForce::GTC, OrderStatus::REJECTED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::REJECTED);
}

// ==========================================
// 2. SETTERS (mutable fields)
// ==========================================

TEST_F(OrderTest, SetQuantity_UpdatesQuantity) {
    Order o = CreateDefaultOrder();
    o.SetQuantity(50);
    EXPECT_EQ(o.GetQuantity(), 50);
}

TEST_F(OrderTest, SetQuantity_ToZero) {
    Order o = CreateDefaultOrder();
    o.SetQuantity(0);
    EXPECT_EQ(o.GetQuantity(), 0);
}

TEST_F(OrderTest, SetQuantity_MultipleUpdates) {
    Order o = CreateDefaultOrder();
    o.SetQuantity(100);
    o.SetQuantity(75);
    o.SetQuantity(0);
    EXPECT_EQ(o.GetQuantity(), 0);
}

TEST_F(OrderTest, SetStatus_ToPartiallyFilled) {
    Order o = CreateDefaultOrder();
    o.SetStatus(OrderStatus::PARTIALLY_FILLED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::PARTIALLY_FILLED);
}

TEST_F(OrderTest, SetStatus_ToFilled) {
    Order o = CreateDefaultOrder();
    o.SetStatus(OrderStatus::FILLED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::FILLED);
}

TEST_F(OrderTest, SetStatus_ToCanceled) {
    Order o = CreateDefaultOrder();
    o.SetStatus(OrderStatus::CANCELED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::CANCELED);
}

TEST_F(OrderTest, SetStatus_ToRejected) {
    Order o = CreateDefaultOrder();
    o.SetStatus(OrderStatus::REJECTED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::REJECTED);
}

TEST_F(OrderTest, SetStatus_Transition_NewToFilledToCanceled) {
    Order o = CreateDefaultOrder();
    EXPECT_EQ(o.GetStatus(), OrderStatus::NEW);
    o.SetStatus(OrderStatus::FILLED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::FILLED);
    o.SetStatus(OrderStatus::CANCELED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::CANCELED);
}

TEST_F(OrderTest, SetQuantity_WorksOnConstRef) {
    const Order o(1, 42, OrderSide::BUY, OrderType::LIMIT,
                  "BTCUSD", 100, 10, Now, TimeInForce::GTC, OrderStatus::NEW);
    o.SetQuantity(5);
    EXPECT_EQ(o.GetQuantity(), 5);
}

TEST_F(OrderTest, SetStatus_WorksOnConstRef) {
    const Order o(1, 42, OrderSide::BUY, OrderType::LIMIT,
                  "BTCUSD", 100, 10, Now, TimeInForce::GTC, OrderStatus::NEW);
    o.SetStatus(OrderStatus::FILLED);
    EXPECT_EQ(o.GetStatus(), OrderStatus::FILLED);
}

// ==========================================
// 3. ToString HELPERS
// ==========================================

TEST_F(OrderTest, ToString_OrderStatus_New) {
    EXPECT_STREQ(ToString(OrderStatus::NEW), "NEW");
}

TEST_F(OrderTest, ToString_OrderStatus_PartiallyFilled) {
    EXPECT_STREQ(ToString(OrderStatus::PARTIALLY_FILLED), "PARTIALLY_FILLED");
}

TEST_F(OrderTest, ToString_OrderStatus_Filled) {
    EXPECT_STREQ(ToString(OrderStatus::FILLED), "FILLED");
}

TEST_F(OrderTest, ToString_OrderStatus_Canceled) {
    EXPECT_STREQ(ToString(OrderStatus::CANCELED), "CANCELED");
}

TEST_F(OrderTest, ToString_OrderStatus_Rejected) {
    EXPECT_STREQ(ToString(OrderStatus::REJECTED), "REJECTED");
}

TEST_F(OrderTest, ToString_OrderSide_Buy) {
    EXPECT_STREQ(ToString(OrderSide::BUY), "BUY");
}

TEST_F(OrderTest, ToString_OrderSide_Sell) {
    EXPECT_STREQ(ToString(OrderSide::SELL), "SELL");
}

TEST_F(OrderTest, ToString_OrderType_Market) {
    EXPECT_STREQ(ToString(OrderType::MARKET), "MARKET");
}

TEST_F(OrderTest, ToString_OrderType_Limit) {
    EXPECT_STREQ(ToString(OrderType::LIMIT), "LIMIT");
}

TEST_F(OrderTest, ToString_OrderType_Stop) {
    EXPECT_STREQ(ToString(OrderType::STOP), "STOP");
}

TEST_F(OrderTest, ToString_TimeInForce_GTC) {
    EXPECT_STREQ(ToString(TimeInForce::GTC), "GTC");
}

TEST_F(OrderTest, ToString_TimeInForce_IOC) {
    EXPECT_STREQ(ToString(TimeInForce::IOC), "IOC");
}

TEST_F(OrderTest, ToString_TimeInForce_FOK) {
    EXPECT_STREQ(ToString(TimeInForce::FOK), "FOK");
}

// ==========================================
// 4. STREAM OPERATOR (operator<<)
// ==========================================

TEST_F(OrderTest, StreamOperator_NotEmpty) {
    Order o = CreateDefaultOrder();
    std::ostringstream oss;
    oss << o;
    EXPECT_FALSE(oss.str().empty());
}

TEST_F(OrderTest, StreamOperator_ContainsSymbol) {
    Order o = CreateDefaultOrder();
    std::ostringstream oss;
    oss << o;
    EXPECT_NE(oss.str().find("BTCUSD"), std::string::npos);
}

TEST_F(OrderTest, StreamOperator_ContainsPrice) {
    Order o = CreateDefaultOrder();
    std::ostringstream oss;
    oss << o;
    EXPECT_NE(oss.str().find("100"), std::string::npos);
}

TEST_F(OrderTest, StreamOperator_ContainsQuantity) {
    Order o = CreateDefaultOrder();
    std::ostringstream oss;
    oss << o;
    EXPECT_NE(oss.str().find("10"), std::string::npos);
}

// ==========================================
// 5. EDGE CASES & INVARIANTS
// ==========================================

TEST_F(OrderTest, OrderID_ImmutableAfterSetQuantity) {
    Order o = CreateDefaultOrder();
    o.SetQuantity(999);
    EXPECT_EQ(o.GetOrderID(), 1);
}

TEST_F(OrderTest, TraderID_ImmutableAfterSetStatus) {
    Order o = CreateDefaultOrder();
    o.SetStatus(OrderStatus::CANCELED);
    EXPECT_EQ(o.GetTraderID(), 42);
}

TEST_F(OrderTest, Symbol_ImmutableAfterMutation) {
    Order o = CreateDefaultOrder();
    o.SetQuantity(0);
    o.SetStatus(OrderStatus::FILLED);
    EXPECT_EQ(o.GetSymbol(), "BTCUSD");
}

TEST_F(OrderTest, Price_ImmutableAfterMutation) {
    Order o = CreateDefaultOrder();
    o.SetQuantity(0);
    EXPECT_EQ(o.GetPrice(), 100u);
}

TEST_F(OrderTest, TwoOrders_DifferentIDs_AreDistinct) {
    Order o1(1, 10, OrderSide::BUY, OrderType::LIMIT, "BTCUSD", 100, 5, Now, TimeInForce::GTC, OrderStatus::NEW);
    Order o2(2, 10, OrderSide::BUY, OrderType::LIMIT, "BTCUSD", 100, 5, Now, TimeInForce::GTC, OrderStatus::NEW);
    EXPECT_NE(o1.GetOrderID(), o2.GetOrderID());
}

TEST_F(OrderTest, SetQuantity_SimulatesPartialFill) {
    Order o = CreateDefaultOrder(); // qty = 10
    o.SetQuantity(o.GetQuantity() - 4);
    EXPECT_EQ(o.GetQuantity(), 6);
    o.SetQuantity(o.GetQuantity() - 6);
    EXPECT_EQ(o.GetQuantity(), 0);
}
// ==========================================
// ADDITIONAL: missing throw coverage
// ==========================================

TEST_F(OrderTest, Constructor_ThrowsOnNegativeQuantity) {
    EXPECT_THROW(
            Order(2, 10, OrderSide::BUY, OrderType::LIMIT,
                  "BTCUSD", 100, -1, Now, TimeInForce::GTC, OrderStatus::NEW),
            std::invalid_argument
    );
}

TEST_F(OrderTest, Constructor_ThrowsOnEmptySymbol) {
    EXPECT_THROW(
            Order(2, 10, OrderSide::BUY, OrderType::LIMIT,
                  "", 100, 5, Now, TimeInForce::GTC, OrderStatus::NEW),
            std::invalid_argument
    );
}

TEST_F(OrderTest, ToString_OrderType_StopLimit) {
    EXPECT_STREQ(ToString(OrderType::STOP_LIMIT), "STOP_LIMIT");
}