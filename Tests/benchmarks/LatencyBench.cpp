//
// Created by denni on 10/10/2026.
//

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <random>
#include <vector>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

#include "Domain/order.h"
#include "Domain/OrderPod.h"
#include "Domain/TradePod.h"
#include "Engine/MatchingEngine.h"
#include "Memory/OrderPool.h"
#include "Storage/BinaryOrderBookStorage.h"

// ----------------------------------------------------------------------------
// Configuration 
// ----------------------------------------------------------------------------
namespace cfg {
constexpr uint32_t SEED          = 42;
constexpr uint32_t MID           = 10'000;      // mid price, in ticks
constexpr uint32_t MAX_DIST      = 499;         // max distance from mid, in ticks
constexpr double   DIST_P        = 0.3;         // geometric: most orders 0-3 ticks from mid
constexpr uint32_t PREFILL       = 50'000;      // resting orders before the test
constexpr uint32_t WARMUP        = 500'000;     // executed, not recorded
constexpr uint32_t MEASURED      = 10'000'000;  // recorded
constexpr uint32_t ADD_PCT       = 45;          // passive limit adds (never cross)
constexpr uint32_t CANCEL_PCT    = 45;          // cancels of resting orders
                                                // rest (10%) = market orders
constexpr uint32_t MAKER_QTY_MIN = 10, MAKER_QTY_MAX = 60;
constexpr uint32_t TAKER_QTY_MIN = 1,  TAKER_QTY_MAX = 10;
}

// ----------------------------------------------------------------------------
// Operations
// ----------------------------------------------------------------------------
enum class OpType : uint8_t { Add, Cancel, Market };

struct Op {
    OpType    type;
    OrderSide side;
    uint32_t  price;
    uint32_t  qty;
    uint32_t  id;
};

class Generator {
public:
    explicit Generator(uint32_t seed) : rng_(seed) {}

    Op MakeAdd() {
        const OrderSide side = sideDist_(rng_) ? OrderSide::SELL : OrderSide::BUY;
        const uint32_t dist  = std::min<uint32_t>(static_cast<uint32_t>(distDist_(rng_)), cfg::MAX_DIST);
        const uint32_t price = (side == OrderSide::BUY) ? cfg::MID - 1 - dist
                                                        : cfg::MID + 1 + dist;
        const uint32_t id = nextId_++;
        live_.push_back(id);
        return {OpType::Add, side, price, makerQty_(rng_), id};
    }

    Op MakeCancel() {
        if (live_.empty()) {
            return MakeAdd();
        }
        const std::size_t k = rng_() % live_.size();
        const uint32_t id = live_[k];
        live_[k] = live_.back();
        live_.pop_back();
        return {OpType::Cancel, OrderSide::BUY, 0, 0, id};
    }

    Op MakeMarket() {
        const OrderSide side = sideDist_(rng_) ? OrderSide::SELL : OrderSide::BUY;
        return {OpType::Market, side, 0, takerQty_(rng_), nextId_++};
    }

    Op MakeMixed() {
        const uint32_t r = roleDist_(rng_);
        if (r < cfg::ADD_PCT)                  return MakeAdd();
        if (r < cfg::ADD_PCT + cfg::CANCEL_PCT) return MakeCancel();
        return MakeMarket();
    }

    uint32_t IdsIssued() const { return nextId_; }

private:
    std::mt19937 rng_;
    std::uniform_int_distribution<int>      sideDist_{0, 1};
    std::uniform_int_distribution<uint32_t> roleDist_{0, 99};
    std::uniform_int_distribution<uint32_t> makerQty_{cfg::MAKER_QTY_MIN, cfg::MAKER_QTY_MAX};
    std::uniform_int_distribution<uint32_t> takerQty_{cfg::TAKER_QTY_MIN, cfg::TAKER_QTY_MAX};
    std::geometric_distribution<int>        distDist_{cfg::DIST_P};
    std::vector<uint32_t> live_;   // ids the generator believes are resting
    uint32_t nextId_ = 0;
};

// ----------------------------------------------------------------------------
// Engine under test
// ----------------------------------------------------------------------------
struct Engine {
    std::unique_ptr<OrderPool>       pool;
    std::unique_ptr<BinaryOrderBook> book;    // large (bitmaps): keep on the heap
    std::atomic<int>                 tradeCounter{0};
    std::unique_ptr<MatchingEngine<BinaryOrderBook>> engine;
    std::vector<TradePod>            trades;

    Engine()
        : pool(std::make_unique<OrderPool>()),
          book(std::make_unique<BinaryOrderBook>(*pool)) {
        engine = std::make_unique<MatchingEngine<BinaryOrderBook>>(*book, tradeCounter, *pool);
        trades.reserve(1024);
    }
};

// Allocates a pool slot, fills it and hands it to the engine.
// Returns the pool index, or UINT32_MAX if the pool is full.
static inline uint32_t Submit(Engine& e, const Op& op) {
    const uint32_t idx = e.pool->Allocate();
    if (idx == UINT32_MAX) {
        return idx;
    }
    OrderPod& o = e.pool->Get(idx);
    o.Price     = op.price;
    o.Timestamp = 0;
    o.OrderId   = op.id;
    o.TraderId  = 0;
    o.Quantity  = op.qty;
    o.Symbol    = 0;
    o.Side      = op.side;
    o.Type      = (op.type == OpType::Market) ? OrderType::MARKET : OrderType::LIMIT;
    o.TIF       = TimeInForce::GTC;
    o.Status    = OrderStatus::NEW;
    e.engine->ProcessOrder(idx, e.trades);
    return idx;
}

// True if `id` is still resting in the book (checked outside the timed region).
static inline bool IsResting(Engine& e, uint32_t id, const std::vector<uint32_t>& idToIdx) {
    const uint32_t idx = idToIdx[id];
    if (idx == UINT32_MAX) {
        return false;
    }
    const OrderPod& o = e.pool->Get(idx);
    return o.OrderId == id &&
           (o.Status == OrderStatus::NEW || o.Status == OrderStatus::PARTIALLY_FILLED);
}

// ----------------------------------------------------------------------------
// Statistics
// ----------------------------------------------------------------------------
struct Summary {
    std::size_t n = 0;
    double mean = 0;
    uint32_t p50 = 0, p90 = 0, p99 = 0, p999 = 0, p9999 = 0, max = 0;
};

static Summary Summarize(std::vector<uint32_t>& v) {
    Summary s;
    s.n = v.size();
    if (v.empty()) {
        return s;
    }
    std::sort(v.begin(), v.end());
    auto pct = [&](double q) {
        return v[std::min(v.size() - 1, static_cast<std::size_t>(q * static_cast<double>(v.size() - 1)))];
    };
    double sum = 0;
    for (uint32_t x : v) sum += x;
    s.mean  = sum / static_cast<double>(v.size());
    s.p50   = pct(0.50);
    s.p90   = pct(0.90);
    s.p99   = pct(0.99);
    s.p999  = pct(0.999);
    s.p9999 = pct(0.9999);
    s.max   = v.back();
    return s;
}

static void PrintRow(const char* name, const Summary& s) {
    std::printf("%-14s %10zu %8.1f %7u %7u %7u %8u %9u %9u\n",
                name, s.n, s.mean, s.p50, s.p90, s.p99, s.p999, s.p9999, s.max);
}

using Clock = std::chrono::steady_clock;

static inline uint32_t ElapsedNs(Clock::time_point a, Clock::time_point b) {
    return static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count());
}

static uint32_t MeasureClockOverhead() {
    std::vector<uint32_t> v(1'000'000);
    for (auto& x : v) {
        const auto a = Clock::now();
        const auto b = Clock::now();
        x = ElapsedNs(a, b);
    }
    std::sort(v.begin(), v.end());
    return v[v.size() / 2];
}

static void PinToCpu(int cpu) {
#if defined(__linux__)
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0) {
        std::printf("Pinned to CPU %d\n", cpu);
    } else {
        std::printf("WARNING: could not pin to CPU %d\n", cpu);
    }
#else
    (void)cpu;
    std::printf("WARNING: CPU pinning not supported on this platform\n");
#endif
}

// ----------------------------------------------------------------------------
int main(int argc, char** argv) {
    if (argc > 1) {
        PinToCpu(std::atoi(argv[1]));
    }

#ifndef NDEBUG
    std::printf("WARNING: assertions enabled (not a Release build) - numbers are not representative\n");
#endif

    // ---- 1. Generate every operation up front (RNG never runs in the timed loop)
    Generator gen(cfg::SEED);
    std::vector<Op> prefill, warmup, measured;
    prefill.reserve(cfg::PREFILL);
    warmup.reserve(cfg::WARMUP);
    measured.reserve(cfg::MEASURED);
    for (uint32_t i = 0; i < cfg::PREFILL;  ++i) prefill.push_back(gen.MakeAdd());
    for (uint32_t i = 0; i < cfg::WARMUP;   ++i) warmup.push_back(gen.MakeMixed());
    for (uint32_t i = 0; i < cfg::MEASURED; ++i) measured.push_back(gen.MakeMixed());

    const uint32_t clockOverhead = MeasureClockOverhead();

    // ---- 2. Latency run: one timestamp pair per operation
    uint64_t poolRejects = 0, totalTrades = 0, cancelMisses = 0;
    std::vector<uint32_t> latAdd, latCancel, latMarket;
    latAdd.reserve(cfg::MEASURED / 2);
    latCancel.reserve(cfg::MEASURED / 2);
    latMarket.reserve(cfg::MEASURED / 8);

    {
        auto e = std::make_unique<Engine>();
        std::vector<uint32_t> idToIdx(gen.IdsIssued(), UINT32_MAX);   // harness bookkeeping only

        auto runUntimed = [&](const std::vector<Op>& ops) {
            for (const Op& op : ops) {
                if (op.type == OpType::Cancel) {
                    e->book->CancelOrder(static_cast<int>(op.id));
                } else {
                    const uint32_t idx = Submit(*e, op);
                    if (idx == UINT32_MAX) ++poolRejects; else idToIdx[op.id] = idx;
                    e->trades.clear();
                }
            }
        };
        runUntimed(prefill);
        runUntimed(warmup);

        for (const Op& op : measured) {
            if (op.type == OpType::Cancel) {
                const bool resting = IsResting(*e, op.id, idToIdx);
                const auto t0 = Clock::now();
                e->book->CancelOrder(static_cast<int>(op.id));
                const auto t1 = Clock::now();
                if (resting) latCancel.push_back(ElapsedNs(t0, t1));
                else         ++cancelMisses;          // already filled by a market order
            } else {
                const auto t0 = Clock::now();
                const uint32_t idx = Submit(*e, op);
                const auto t1 = Clock::now();
                if (idx == UINT32_MAX) { ++poolRejects; continue; }
                idToIdx[op.id] = idx;
                (op.type == OpType::Add ? latAdd : latMarket).push_back(ElapsedNs(t0, t1));
                totalTrades += e->trades.size();
                e->trades.clear();
            }
        }
    }

    // ---- 3. Throughput run: fresh engine, same operations, no per-op timing
    double throughputOps = 0, nsPerOp = 0;
    {
        auto e = std::make_unique<Engine>();
        auto run = [&](const std::vector<Op>& ops) {
            for (const Op& op : ops) {
                if (op.type == OpType::Cancel) {
                    e->book->CancelOrder(static_cast<int>(op.id));
                } else {
                    Submit(*e, op);
                    e->trades.clear();
                }
            }
        };
        run(prefill);
        run(warmup);
        const auto t0 = Clock::now();
        run(measured);
        const auto t1 = Clock::now();
        const double secs = std::chrono::duration<double>(t1 - t0).count();
        throughputOps = static_cast<double>(measured.size()) / secs;
        nsPerOp = secs * 1e9 / static_cast<double>(measured.size());
    }

    // ---- 4. Report
    const Summary sAdd    = Summarize(latAdd);
    const Summary sCancel = Summarize(latCancel);
    const Summary sMarket = Summarize(latMarket);

    std::printf("\nLatencyBench - MatchingEngine<BinaryOrderBook>, single thread\n");
    std::printf("compiler: %s\n", __VERSION__);
    std::printf("seed %u | prefill %u | warmup %u | measured %u | mix add/cancel/market = %u/%u/%u\n",
                cfg::SEED, cfg::PREFILL, cfg::WARMUP, cfg::MEASURED,
                cfg::ADD_PCT, cfg::CANCEL_PCT, 100 - cfg::ADD_PCT - cfg::CANCEL_PCT);
    std::printf("prices: mid %u, geometric(p=%.1f) distance from mid, capped at %u ticks\n",
                cfg::MID, cfg::DIST_P, cfg::MAX_DIST);
    std::printf("clock overhead (median now()->now()): %u ns, included in the numbers below\n\n",
                clockOverhead);

    std::printf("%-14s %10s %8s %7s %7s %7s %8s %9s %9s\n",
                "op", "count", "mean", "p50", "p90", "p99", "p99.9", "p99.99", "max");
    std::printf("%-14s %10s %8s %7s %7s %7s %8s %9s %9s\n",
                "", "", "(ns)", "(ns)", "(ns)", "(ns)", "(ns)", "(ns)", "(ns)");
    PrintRow("add (limit)", sAdd);
    PrintRow("cancel", sCancel);
    PrintRow("market", sMarket);

    std::printf("\nthroughput (no per-op timing): %.2f M ops/s  (%.1f ns/op)\n",
                throughputOps / 1e6, nsPerOp);
    std::printf("trades: %llu | cancels of already-filled orders (excluded): %llu (%.2f%%)\n",
                static_cast<unsigned long long>(totalTrades),
                static_cast<unsigned long long>(cancelMisses),
                100.0 * static_cast<double>(cancelMisses) /
                    static_cast<double>(sCancel.n + cancelMisses));
    std::printf("pool rejects: %llu %s\n",
                static_cast<unsigned long long>(poolRejects),
                poolRejects == 0 ? "(ok)" : "<-- INVALID RUN: increase pool capacity");
    return poolRejects == 0 ? 0 : 1;
}