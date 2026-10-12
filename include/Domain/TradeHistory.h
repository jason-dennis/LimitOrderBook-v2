//
// TradeHistory: fixed-capacity ring buffer with the most recent trades.
//

#ifndef LIMITORDERBOOK_TRADEHISTORY_H
#define LIMITORDERBOOK_TRADEHISTORY_H

#include <cstddef>
#include <vector>

#include "Domain/TradePod.h"

/**
 * @class TradeHistory
 * @brief Keeps the last `capacity` trades for the UI and for queries.
 *
 * Memory is allocated once, in the constructor. Push never allocates and
 * overwrites the oldest trade when the buffer is full, so the history cannot
 * grow without bound under sustained load.
 * Not thread-safe: callers synchronise access (SymbolResources::StateMutex).
 */
class TradeHistory {
public:
    explicit TradeHistory(std::size_t capacity) : buffer_(capacity) {}

    void Push(const TradePod& trade) {
        buffer_[head_] = trade;
        head_ = (head_ + 1) % buffer_.size();
        if (count_ < buffer_.size()) {
            ++count_;
        }
    }

    /// @brief Copy of the stored trades, oldest first.
    std::vector<TradePod> Snapshot() const {
        std::vector<TradePod> result;
        result.reserve(count_);
        const std::size_t start = (head_ + buffer_.size() - count_) % buffer_.size();
        for (std::size_t i = 0; i < count_; ++i) {
            result.push_back(buffer_[(start + i) % buffer_.size()]);
        }
        return result;
    }

    std::size_t Size() const { return count_; }
    std::size_t Capacity() const { return buffer_.size(); }

private:
    std::vector<TradePod> buffer_;
    std::size_t head_ = 0;    ///< next position to write
    std::size_t count_ = 0;   ///< number of valid trades (<= capacity)
};

#endif //LIMITORDERBOOK_TRADEHISTORY_H
