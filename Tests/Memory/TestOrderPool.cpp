//
// Created by Ognean Jason Dennis on 17.07.2026.
//

#include <gtest/gtest.h>
#include <set>
#include <vector>

#include "Memory/OrderPool.h"

TEST(OrderPoolTest, AllocateReturnsUniqueIndices) {
    OrderPool pool;
    std::set<uint32_t> seen;

    for (int i = 0; i < 1000; ++i) {
        uint32_t index = pool.Allocate();
        ASSERT_NE(index, UINT32_MAX);
        auto [it, inserted] = seen.insert(index);
        EXPECT_TRUE(inserted) << "Index duplicat: " << index;
    }
}

TEST(OrderPoolTest, FreedIndexIsReusedByNextAllocate) {
    OrderPool pool;

    uint32_t first = pool.Allocate();
    pool.Free(first);
    uint32_t second = pool.Allocate();

    EXPECT_EQ(first, second)
                        << "Free() urmat imediat de Allocate() ar trebui sa refoloseasca acelasi slot (LIFO)";
}

TEST(OrderPoolTest, GetGivesAccessToCorrectSlotData) {
    OrderPool pool;

    uint32_t index = pool.Allocate();
    pool.Get(index).OrderId = 42;
    pool.Get(index).Quantity = 100;

    EXPECT_EQ(pool.Get(index).OrderId, 42u);
    EXPECT_EQ(pool.Get(index).Quantity, 100u);
}

TEST(OrderPoolTest, AllocatingBeyondCapacityReturnsSentinel) {
    OrderPool pool;

    for (uint32_t i = 0; i < DefaultCapacity; ++i) {
        uint32_t index = pool.Allocate();
        ASSERT_NE(index, UINT32_MAX) << "Pool-ul s-a epuizat prematur, la iteratia " << i;
    }

    uint32_t overflow = pool.Allocate();
    EXPECT_EQ(overflow, UINT32_MAX)
                        << "Allocate() dupa epuizare ar trebui sa intoarca sentinel-ul, nu un index valid";
}

TEST(OrderPoolTest, FullCycleAllocateFreeReallocateWorksCorrectly) {
    OrderPool pool;
    std::vector<uint32_t> allocated;
    allocated.reserve(DefaultCapacity);

    for (uint32_t i = 0; i < DefaultCapacity; ++i) {
        uint32_t index = pool.Allocate();
        ASSERT_NE(index, UINT32_MAX);
        allocated.push_back(index);
    }

    for (uint32_t index : allocated) {
        pool.Free(index);
    }

    std::set<uint32_t> reallocated;
    for (uint32_t i = 0; i < DefaultCapacity; ++i) {
        uint32_t index = pool.Allocate();
        ASSERT_NE(index, UINT32_MAX)
                                    << "Pool-ul ar trebui sa aiba din nou capacitate completa dupa eliberarea tuturor sloturilor";
        auto [it, inserted] = reallocated.insert(index);
        EXPECT_TRUE(inserted) << "Index duplicat la realocare: " << index;
    }

    EXPECT_EQ(reallocated.size(), static_cast<size_t>(DefaultCapacity));
}