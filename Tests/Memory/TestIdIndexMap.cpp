//
// Created by denni on 10/12/2026.
//
#include <gtest/gtest.h>

#include <random>
#include <unordered_map>
#include <vector>

#include "Memory/IdIndexMap.h"

TEST(IdIndexMapTest, EmptyMap_GetReturnsNil) {
    IdIndexMap map;
    EXPECT_EQ(map.Get(0), NIL);
    EXPECT_EQ(map.Get(12345), NIL);
    EXPECT_EQ(map.Get(UINT32_MAX - 1), NIL);
    EXPECT_EQ(map.AllocatedPages(), 0u);
}

TEST(IdIndexMapTest, SetThenGet_ReturnsIndex) {
    IdIndexMap map;
    map.Set(42, 7);
    EXPECT_EQ(map.Get(42), 7u);
    EXPECT_EQ(map.Get(41), NIL);
    EXPECT_EQ(map.Get(43), NIL);
}

TEST(IdIndexMapTest, Clear_RemovesEntry) {
    IdIndexMap map;
    map.Set(42, 7);
    map.Clear(42);
    EXPECT_EQ(map.Get(42), NIL);
}

TEST(IdIndexMapTest, ClearNonExistent_DoesNothing) {
    IdIndexMap map;
    map.Clear(5);                 // nothing allocated yet
    map.Set(10, 1);
    map.Clear(11);                // same page, empty slot
    map.Clear(100'000'000);       // beyond directory
    EXPECT_EQ(map.Get(10), 1u);
    EXPECT_EQ(map.ActivePages(), 1u);
}

TEST(IdIndexMapTest, ClearTwice_DoesNotUnderflowLiveCount) {
    IdIndexMap map;
    map.Set(1, 10);
    map.Set(2, 20);
    map.Clear(1);
    map.Clear(1);                 // second clear must be a no-op
    EXPECT_EQ(map.Get(2), 20u);
    EXPECT_EQ(map.ActivePages(), 1u);   // page still holds id 2
}

TEST(IdIndexMapTest, Overwrite_DoesNotDoubleCount) {
    IdIndexMap map;
    map.Set(5, 1);
    map.Set(5, 2);                // overwrite, live count stays 1
    EXPECT_EQ(map.Get(5), 2u);
    map.Clear(5);
    EXPECT_EQ(map.ActivePages(), 0u);   // page released after a single clear
}

TEST(IdIndexMapTest, FarApartIds_UseSeparatePages) {
    IdIndexMap map;
    map.Set(0, 1);
    map.Set(100'000'000, 2);
    EXPECT_EQ(map.Get(0), 1u);
    EXPECT_EQ(map.Get(100'000'000), 2u);
    EXPECT_EQ(map.ActivePages(), 2u);
}

TEST(IdIndexMapTest, LargestIds_Work) {
    IdIndexMap map;
    map.Set(UINT32_MAX - 1, 3);
    EXPECT_EQ(map.Get(UINT32_MAX - 1), 3u);
    map.Clear(UINT32_MAX - 1);
    EXPECT_EQ(map.Get(UINT32_MAX - 1), NIL);
}

TEST(IdIndexMapTest, EmptyPage_IsReleasedAndReused) {
    IdIndexMap map;
    const uint32_t P = IdIndexMap::PAGE_SIZE;

    for (uint32_t id = 0; id < P; ++id) {
        map.Set(id, id);
    }
    EXPECT_EQ(map.ActivePages(), 1u);
    EXPECT_EQ(map.AllocatedPages(), 1u);

    for (uint32_t id = 0; id < P; ++id) {
        map.Clear(id);
    }
    EXPECT_EQ(map.ActivePages(), 0u);
    EXPECT_EQ(map.AllocatedPages(), 1u);   // kept in the free list

    map.Set(10 * P + 3, 99);               // different page range
    EXPECT_EQ(map.AllocatedPages(), 1u);   // reused, not newly allocated
    EXPECT_EQ(map.ActivePages(), 1u);
    EXPECT_EQ(map.Get(10 * P + 3), 99u);
}

TEST(IdIndexMapTest, RecycledPage_HasNoStaleEntries) {
    IdIndexMap map;
    const uint32_t P = IdIndexMap::PAGE_SIZE;

    for (uint32_t id = 0; id < P; ++id) {
        map.Set(id, id + 1);
    }
    for (uint32_t id = 0; id < P; ++id) {
        map.Clear(id);
    }

    map.Set(5 * P, 7);                     // recycled page now covers [5P, 6P)
    for (uint32_t off = 1; off < P; ++off) {
        ASSERT_EQ(map.Get(5 * P + off), NIL) << "stale slot at offset " << off;
    }
    for (uint32_t id = 0; id < P; ++id) {
        ASSERT_EQ(map.Get(id), NIL);       // old range is gone
    }
}

TEST(IdIndexMapTest, RandomOperations_MatchReferenceMap) {
    IdIndexMap map;
    std::unordered_map<uint32_t, uint32_t> reference;
    std::vector<uint32_t> live;

    std::mt19937 rng(7);
    uint32_t nextId = 0;

    for (int step = 0; step < 500'000; ++step) {
        const int op = static_cast<int>(rng() % 100);

        if (op < 55 || live.empty()) {            // insert a new, increasing id
            const uint32_t id = nextId;
            nextId += 1 + rng() % 3;              // gaps, like ids taken by other books
            const uint32_t index = rng() % 1'000'000;
            map.Set(id, index);
            reference[id] = index;
            live.push_back(id);
        } else {                                   // clear a random live id
            const std::size_t k = rng() % live.size();
            const uint32_t id = live[k];
            live[k] = live.back();
            live.pop_back();
            map.Clear(id);
            reference.erase(id);
        }

        if (step % 997 == 0) {                     // spot-check lookups
            const uint32_t probe = nextId == 0 ? 0 : rng() % nextId;
            auto it = reference.find(probe);
            const uint32_t expected = (it == reference.end()) ? NIL : it->second;
            ASSERT_EQ(map.Get(probe), expected) << "step " << step << " id " << probe;
        }
    }

    for (const auto& [id, index] : reference) {
        ASSERT_EQ(map.Get(id), index);
    }
    // Memory should track live ids, not every id ever issued.
    EXPECT_LT(map.ActivePages(), nextId / IdIndexMap::PAGE_SIZE + 1);
}