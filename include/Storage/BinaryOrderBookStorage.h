//
// Created by denni on 3/14/2026.
//

#ifndef LIMITORDERBOOK_BINARYORDERBOOK_H
#define LIMITORDERBOOK_BINARYORDERBOOK_H
#include "Memory/OrderPool.h"

#include<unordered_map>
/**
 * @class BinaryOrderBook
 * @brief High-performance order book implementation using a binary-indexed (bitmap) structure.
 * Uses a 4-level bitmap hierarchy for O(1) best bid/ask lookups,
 * with linked lists at each price level for FIFO order management.
 * Optimized for low-latency trading with paged memory allocation.
 */
class BinaryOrderBook {
private:
    OrderPool& Pool_;
    class Node {
    private:
        const uint32_t index_;
        Node* next_;
        Node* prev_;
    public:
        Node(const uint32_t index): index_(index), next_(nullptr),prev_(nullptr){}

        Node* GetNext() const {return next_;}
        Node* GetPrev() const {return prev_;}

        uint32_t GetIndex() const {return index_;}

        void SetPrev(Node* prev){prev_ = prev;}
        void SetNext(Node* next) {next_ = next;}

    };
    class LinkedList {
    private:
        Node* head_;
        Node* tail_;
    public:

        LinkedList(): head_(nullptr), tail_(nullptr){}

        Node* AddNode(uint32_t index) {
            Node* node= new Node(index);
            if (!tail_) {
                head_=tail_=node;
            }
            else {
                tail_->SetNext(node);
                node->SetPrev(tail_);
                tail_ = node;
            }
            return node;
        }

        void DeleteNode(Node* node) {
            if (node->GetPrev()) {
                node->GetPrev()->SetNext(node->GetNext());
            }
            if (node->GetNext()) {
                node->GetNext()->SetPrev(node->GetPrev());
            }

            if (node == head_) {
                head_=node->GetNext();
            }
            if (node == tail_) {
                tail_=node->GetPrev();
            }
            delete node;
        }

        bool IsEmpty() const {
            return head_ == nullptr;
        }

        Node* GetHead() const{
            return head_;
        }
    };
    class ListMap {
        static constexpr uint64_t PAGE_SIZE = 64;
        static constexpr uint64_t NUM_PAGES = 20'000'000 / PAGE_SIZE + 1;

        std::vector<LinkedList*> pages_;

    public:
        ListMap() : pages_(NUM_PAGES, nullptr) {}

        ~ListMap() {
            for (auto* p : pages_)
                delete[] p;
        }


        ListMap(const ListMap&) = delete;
        ListMap& operator=(const ListMap&) = delete;

        ListMap(ListMap&& other) noexcept : pages_(std::move(other.pages_)) {}
        ListMap& operator=(ListMap&& other) noexcept {
            if (this!= &other) {
                for (auto* p : pages_) delete[] p;
                pages_ = std::move(other.pages_);
            }
            return *this;
        }

        LinkedList& operator[](uint64_t price) {
            uint64_t page_idx = price / PAGE_SIZE;
            uint64_t slot_idx = price % PAGE_SIZE;
            if (!pages_[page_idx]) {
                pages_[page_idx] = new LinkedList[PAGE_SIZE]();
            }
            return pages_[page_idx][slot_idx];
        }
    };
    /// Bitmap dimensions per level (price range / 64 per level)
    /// Max PRICE 20.000.000 -> 200.000 *tick(100)
    const int DIM_level1 = 312'501;
    const int DIM_level2 =  4'884;
    const int DIM_level3 = 77;
    const int DIM_level4 = 2;

    std::vector<uint64_t>level1_bid;///< Level 1 bitmap for bids
    std::vector<uint64_t>level2_bid;///< Level 2 bitmap for bids
    std::vector<uint64_t>level3_bid;///< Level 3 bitmap for bids
    std::vector<uint64_t>level4_bid;///< Level 4 bitmap for bids

    std::vector<uint64_t>level1_ask;///< Level 1 bitmap for asks
    std::vector<uint64_t>level2_ask;///< Level 2 bitmap for asks
    std::vector<uint64_t>level3_ask;///< Level 3 bitmap for asks
    std::vector<uint64_t>level4_ask;///< Level 4 bitmap for asks

    ListMap BidList_;///< Price-to-order-queue map for bids
    ListMap AskList_;///< Price-to-order-queue map for asks
    std::unordered_map<int,Node*>BidLocation;///< Order ID to node lookup for bids
    std::unordered_map<int,Node*>AskLocation;///< Order ID to node lookup for asks

public:

    BinaryOrderBook(OrderPool& Pool)
    : Pool_(Pool),
      level1_bid(DIM_level1), level2_bid(DIM_level2),
      level3_bid(DIM_level3), level4_bid(DIM_level4),
      level1_ask(DIM_level1), level2_ask(DIM_level2),
      level3_ask(DIM_level3), level4_ask(DIM_level4)
    {}
    ~BinaryOrderBook() = default;

    /// @brief Calculates the index for a bitmap level.
    uint64_t CalcInd(uint64_t x);

    /// @brief Calculates the bit position within a bitmap word.
    uint64_t CalcBit(uint64_t x);

    /**
     * @brief Adds a new order to the book.
     * @param order The order object to be registered.
     */
    void AddOrder(uint32_t index);

    /**
     * @brief Removes an order from the book based on its ID.
     * @param order_id The unique identifier of the order to be removed.
     */
    void CancelOrder(int order_id);

    /**
     * @brief Modifies the volume of an existing order.
     * @param order_id The unique identifier of the order.
     * @param new_quantity The updated quantity.
     */
    void UpdateQuantity(int order_id,int new_quantity);

    /// @brief Removes a bid order from the book.
    void DeleteBid(int order_id,Node* node, uint64_t Price);

    /// @brief Removes an ask order from the book.
    void DeleteAsk(int order_id, Node* node, uint64_t Price);

    /**
     * @brief Accesses the highest-priced Buy order.
     * @return Pointer to the best Bid, or nullptr if the side is empty.
     */
    const uint32_t GetBestBid()  ;

    /**
     * @brief Accesses the lowest-priced Ask order.
     * @return Pointer to the best Ask, or nullptr if the side is empty.
     */
    const uint32_t GetBestAsk();

    /// @brief Returns the top x bid price levels.
    std::vector<uint32_t> GetBestBids(uint32_t x) ;

    /// @brief Returns the top x ask price levels.
    std::vector<uint32_t> GetBestAsks(uint32_t x);

    /**
     * @brief Checks for the presence of Buy orders.
     * @return true if the Bid side is empty.
     */
    bool IsBidEmpty() const;

     /**
     * @brief Checks for the presence of Sell orders.
     * @return true if the Ask side is empty.
     */
    bool IsAskEmpty() const;


    bool CanFillQuantityAsks(uint32_t Quantity, uint64_t Price) ;


    bool CanFillQuantityBids(uint32_t Quantity, uint64_t Price);

    /**
         * @brief Removes the top-priority Buy order from the book.
         */
    void PopBestBid();

    /**
         * @brief Removes the top-priority Buy order from the book.
         */
    void PopBestAsk();
};

#endif //LIMITORDERBOOK_BINARYORDERBOOK_H