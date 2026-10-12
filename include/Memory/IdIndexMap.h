//
// Created by denni on 10/12/2026.
//

#ifndef LIMITORDERBOOK_IDINDEXMAP_H
#define LIMITORDERBOOK_IDINDEXMAP_H
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <vector>

#include "Domain/OrderPod.h"   // NIL

/**
 * @class IdIndexMap
 * @brief Maps an order id to its pool index in O(1), without hashing.
 *
 * Ids are split into pages of PAGE_SIZE consecutive ids. A page is allocated
 * only when the first id in its range is set, and is returned to a free list
 * (not deleted) when its last live entry is cleared. Since order ids grow
 * monotonically, old pages empty out over time and get recycled, so memory is
 * proportional to the span of live ids, not to the total number of ids ever seen.
 *
 * Invariant: a page's `live` counter equals the number of non-NIL slots in it.
 * Therefore a page with live == 0 is entirely NIL and can be reused as is.
 */
class IdIndexMap {
public:
    static constexpr uint32_t PAGE_BITS = 12;
    static constexpr uint32_t PAGE_SIZE = 1u << PAGE_BITS;   // 4096 ids per page
    static constexpr uint32_t PAGE_MASK = PAGE_SIZE - 1;

    IdIndexMap() {
        freePages_.reserve(64);
    }
 
    /// @brief Returns the pool index for `id`, or NIL if the id is not present.
    uint32_t Get(uint32_t id) const {
        const uint32_t page = id >> PAGE_BITS;
        if (page >= directory_.size()) {
            return NIL;
        }
        const Page* p = directory_[page].get();
        if (!p) {
            return NIL;
        }
        return p->slots[id & PAGE_MASK];
    }

    /// @brief Associates `id` with `index`. Overwrites an existing entry.
    void Set(uint32_t id, uint32_t index) {
        const uint32_t page = id >> PAGE_BITS;
        if (page >= directory_.size()) {
            directory_.resize(page + 1);
        }
        auto& p = directory_[page];
        if (!p) {
            p = AcquirePage();
        }
        uint32_t& slot = p->slots[id & PAGE_MASK];
        if (slot == NIL) {
            ++p->live;
        }
        slot = index;
    }

    /// @brief Removes `id`. Does nothing if the id is not present.
    void Clear(uint32_t id) {
        const uint32_t page = id >> PAGE_BITS;
        if (page >= directory_.size()) {
            return;
        }
        auto& p = directory_[page];
        if (!p) {
            return;
        }
        uint32_t& slot = p->slots[id & PAGE_MASK];
        if (slot == NIL) {
            return;
        }
        slot = NIL;
        if (--p->live == 0) {
            freePages_.push_back(std::move(p));   // p becomes nullptr
        }
    }

    /// @brief Pages ever allocated (in use + in the free list).
    std::size_t AllocatedPages() const { return allocatedPages_; }

    /// @brief Pages currently holding at least one live id.
    std::size_t ActivePages() const { return allocatedPages_ - freePages_.size(); }

    /// @brief Approximate memory used by pages and the directory, in bytes.
    std::size_t MemoryBytes() const {
        return allocatedPages_ * sizeof(Page)
             + directory_.capacity() * sizeof(std::unique_ptr<Page>);
    }

private:
    struct Page {
        uint32_t slots[PAGE_SIZE];
        uint32_t live = 0;
    };

    std::unique_ptr<Page> AcquirePage() {
        if (!freePages_.empty()) {
            // A recycled page has live == 0, so all its slots are already NIL.
            auto p = std::move(freePages_.back());
            freePages_.pop_back();
            return p;
        }
        auto p = std::make_unique<Page>();
        std::fill(std::begin(p->slots), std::end(p->slots), NIL);
        p->live = 0;
        ++allocatedPages_;
        return p;
    }

    std::vector<std::unique_ptr<Page>> directory_;  ///< page number -> page (or nullptr)
    std::vector<std::unique_ptr<Page>> freePages_;  ///< empty pages kept for reuse
    std::size_t allocatedPages_ = 0;
};
#endif //LIMITORDERBOOK_IDINDEXMAP_H
