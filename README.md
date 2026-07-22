# Limit Order Book

> A high-performance C++ limit order book implementation using a 4-level bitmap index for O(1) best price lookup, order insertion and cancellation.

![UI](assets/UI.png)

---

## What is a Limit Order Book?

A **Limit Order Book (LOB)** is the core data structure of every financial exchange. It maintains two sorted collections of orders:

- **Bids** — buy orders, sorted by price descending (highest price has priority)
- **Asks** — sell orders, sorted by price ascending (lowest price has priority)

When a new order arrives, the matching engine checks whether it can be filled against the opposite side. If a buy price ≥ the best ask, a trade executes. Otherwise, the order rests in the book waiting for a counterpart.

---

## Why Data Structure Choice Matters

The three operations that dominate a real LOB workload are:

| Operation | Frequency | What it needs |
|---|---|---|
| `AddOrder` | Very high | Insert at correct price level |
| `CancelOrder` | High | Remove by order ID in O(1) |
| `GetBestBid/Ask` | Every match cycle | Access top of book instantly |

A naive `std::map` gives O(log n) on all three. A `std::multiset` gives O(1) `GetBest` via `begin()` but O(log n) insertion. The ideal structure provides **true O(1) on all three** — which is what the bitmap index achieves.

---

## Implementation — 4-Level Bitmap Index

### The core idea

Prices are stored as integers (e.g. $150.25 → 15025 with tick = 0.01). The bitmap index maps each possible price to a single bit. Finding the best bid means finding the **highest set bit** — a single CPU instruction: `BSR` on x86, `CLZ` on ARM.

### Structure
    
```
Bitmap dimensions per level (price range / 64 per level)
Max PRICE 20.000.000 -> 200.000 *tick(100)
Level 4 (2 × 64-bit words)     — 1 bit covers 64³ prices
Level 3 (77 × 64-bit words)   — 1 bit covers 64² prices  
Level 2 (4'884 × 64-bit words) — 1 bit covers 64 prices
Level 1 (312'501 × 64-bit words) — 1 bit = 1 price level
```

Each level is an index into the level below. A bit is set at level N only if at least one order exists somewhere beneath it.

### Why it's O(1)

```cpp
// GetBestBid — find highest set bit, 4 levels deep
  
  uint64_t bit_level4 = (63 - __builtin_clzll(level4_bid[i]));
  uint64_t ind_level3 = i*64 + bit_level4;
  
  uint64_t bit_level3 = (63 - __builtin_clzll(level3_bid[ind_level3]));
  uint64_t ind_level2 = ind_level3*64 + bit_level3;
  
  uint64_t bit_level2 = (63 - __builtin_clzll(level2_bid[ind_level2]));
  uint64_t ind_level1 = ind_level2*64 + bit_level2;
  
  uint64_t bit_level1 = (63 - __builtin_clzll(level1_bid[ind_level1]));
  uint64_t Price = ind_level1*64 + bit_level1;
// → exact price in 4 steps, regardless of how many orders exist
```

The number of steps is always exactly 4 — independent of book size n. This is **true O(1)**, not amortized.

### AddOrder

When inserting at price Price:
```cpp
CalcInd(uint64_t x){
    return x/64;
}
CalcBit(uint64_t x) {
    return x%64;
}
uint64_t ind_level1 = CalcInd(Price);
uint64_t bit_level1= CalcBit(Price);
level1_bid[ind_level1]|=(1ULL<<bit_level1);

uint64_t ind_level2 = CalcInd(ind_level1);
uint64_t bit_level2= CalcBit(ind_level1);
level2_bid[ind_level2]|=(1ULL<<bit_level2);

uint64_t ind_level3 = CalcInd(ind_level2);
uint64_t bit_level3= CalcBit(ind_level2);
level3_bid[ind_level3]|=(1ULL<<bit_level3);

uint64_t ind_level4 = CalcInd(ind_level3);
uint64_t bit_level4= CalcBit(ind_level3);
level4_bid[ind_level4]|=(1ULL<<bit_level4);
```

Worst case: 4 bit-set operations. O(1).

### CancelOrder

Each order ID maps directly to its `Node*` via `unordered_map`. Cancel finds the node in O(1), removes it from the doubly-linked list at that price level, then clears the bitmap bit if the list is now empty — propagating up only if needed.
```cpp
if (BidList_[Price].IsEmpty()) {
        uint64_t ind_level1 = CalcInd(Price);
        uint64_t bit_level1= CalcBit(Price);
        level1_bid[ind_level1] &= ~(1ULL<<bit_level1);

        if (!level1_bid[ind_level1]) {
            uint64_t ind_level2 = CalcInd(ind_level1);
            uint64_t bit_level2= CalcBit(ind_level1);
            level2_bid[ind_level2] &= ~(1ULL<<bit_level2);

            if (!level2_bid[ind_level2]) {
                uint64_t ind_level3 = CalcInd(ind_level2);
                uint64_t bit_level3= CalcBit(ind_level2);
                level3_bid[ind_level3] &= ~(1ULL<<bit_level3);

                if (!level3_bid[ind_level3]) {
                    uint64_t ind_level4 = CalcInd(ind_level3);
                    uint64_t bit_level4= CalcBit(ind_level3);
                    level4_bid[ind_level4] &= ~(1ULL<<bit_level4);
                }
            }
        }
    }
```
---

## Project Architecture

The project is structured in clean layers with dependency injection throughout, ensuring each component is independently testable and replaceable.

```
CoreEngine
    └── AppEngine
            └── MatchingEngine
                    └── IOrderBook  ←  BinaryOrderBook | MultisetOrderBook
```

### Layers

**Domain** — `Order`, `Trade`. Pure value types with no dependencies.

**Storage (`IOrderBook`)** — Abstract interface implemented by:
- `BinaryOrderBook` — 4-level bitmap index, O(1) on all core operations
- `MultisetOrderBook` — `std::multiset` with iterator map, used as reference implementation

**MatchingEngine** — Receives an `IOrderBook&` via constructor injection. Handles order matching logic for all order types (LIMIT, MARKET) and time-in-force policies (GTC, IOC, FOK).

**AppEngine** — Manages one `MatchingEngine` per symbol. Routes orders by symbol string, maintains trade history per symbol.

**CoreEngine** — Public API layer. Converts string-based inputs (from UI) to domain types, applies the tick multiplier, generates order IDs atomically.

### UI

The project includes a real-time desktop UI built with **[Dear ImGui](https://github.com/ocornut/imgui)** and **[ImPlot](https://github.com/epezent/implot)**:

- Live order book depth display (top N bids/asks)
- Trade history feed
- Price chart with real-time updates via ImPlot
- Order entry panel (symbol, side, type, price, quantity, TIF)

---

## Benchmark — Bitmap vs Multiset

> Measured on two platforms. Values at 100k orders, Release build, 5 repetitions.

![benchmark](assets/Benchmark.png)

| Operation | PC (i5-12400F) | Mac (M2) | Winner |
|---|---|---|---|
| AddOrder random | +6.5× | +9.6× | Binary |
| AddOrder clustered | +10× | +19× | Binary |
| CancelOrder | +26% | +2.3× | Binary |
| GetBestBid | tie | tie | — |
| GetBestAsk | tie | Multiset +17% | — |
| CanFillQuantity | Multiset +5.2× | Multiset +4.6× | Multiset |
| UpdateQuantity | Multiset +51% | Multiset +91% | Multiset |
| PopBestBid @ 100k | Multiset wins | Multiset wins | Multiset |
| Mixed workload | +11% | +12% | Binary |

**For a real LOB workload** dominated by `AddOrder` and `CancelOrder`, the bitmap index is the correct choice. `CanFillQuantity` and `UpdateQuantity` favour `Multiset` — but these are comparatively rare operations in a live book.

---

**Dependencies** — CMake ≥ 3.20, C++20, Google Benchmark, Google Test, ImGui, ImPlot.

