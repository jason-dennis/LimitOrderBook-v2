//
// Created by denni on 3/15/2026.
//
#include "Storage/BinaryOrderBookStorage.h"


uint64_t BinaryOrderBook::CalcInd(uint64_t x) {
    return x/64;
}

uint64_t BinaryOrderBook::CalcBit(uint64_t x) {
    return x%64;
}

void BinaryOrderBook::AddOrder(uint32_t index) {

    auto& order = Pool_.Get(index);
    if (order.Side==OrderSide::BUY) {
        uint64_t Price = order.Price;
        Node* node = BidList_[Price].AddNode(index);
        BidLocation[order.OrderId]=node;

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
    }
    else {
        uint64_t Price = order.Price;
        Node* node = AskList_[Price].AddNode(index);
        AskLocation[order.OrderId]=node;

        uint64_t ind_level1 = CalcInd(Price);
        uint64_t bit_level1= CalcBit(Price);
        level1_ask[ind_level1]|=(1ULL<<bit_level1);

        uint64_t ind_level2 = CalcInd(ind_level1);
        uint64_t bit_level2= CalcBit(ind_level1);
        level2_ask[ind_level2]|=(1ULL<<bit_level2);

        uint64_t ind_level3 = CalcInd(ind_level2);
        uint64_t bit_level3= CalcBit(ind_level2);
        level3_ask[ind_level3]|=(1ULL<<bit_level3);

        uint64_t ind_level4 = CalcInd(ind_level3);
        uint64_t bit_level4= CalcBit(ind_level3);
        level4_ask[ind_level4]|=(1ULL<<bit_level4);
    }


}

void BinaryOrderBook::DeleteBid(int order_id, Node* node, uint64_t Price) {

    BidList_[Price].DeleteNode(node);
    BidLocation.erase(order_id);

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
}

void BinaryOrderBook::DeleteAsk(int order_id, Node* node, uint64_t Price) {
    AskList_[Price].DeleteNode(node);
    AskLocation.erase(order_id);

    if (AskList_[Price].IsEmpty()) {
        uint64_t ind_level1 = CalcInd(Price);
        uint64_t bit_level1= CalcBit(Price);
        level1_ask[ind_level1] &= ~(1ULL<<bit_level1);

        if (!level1_ask[ind_level1]) {
            uint64_t ind_level2 = CalcInd(ind_level1);
            uint64_t bit_level2= CalcBit(ind_level1);
            level2_ask[ind_level2] &= ~(1ULL<<bit_level2);

            if (!level2_ask[ind_level2]) {
                uint64_t ind_level3 = CalcInd(ind_level2);
                uint64_t bit_level3= CalcBit(ind_level2);
                level3_ask[ind_level3] &= ~(1ULL<<bit_level3);

                if (!level3_ask[ind_level3]) {
                    uint64_t ind_level4 = CalcInd(ind_level3);
                    uint64_t bit_level4= CalcBit(ind_level3);
                    level4_ask[ind_level4] &= ~(1ULL<<bit_level4);
                }
            }
        }
    }
}

void BinaryOrderBook::CancelOrder(int order_id) {

    auto bidIt = BidLocation.find(order_id);
    if (bidIt != BidLocation.end()) {
        auto* node = bidIt->second;
        auto index = node->GetIndex();;
        auto& order = Pool_.Get(index);
        uint64_t Price = order.Price;
        order.Status = (OrderStatus::CANCELED);
        DeleteBid(order_id, bidIt->second, Price);
        Pool_.Free(index);
        return;
    }

    auto askIt =  AskLocation.find(order_id);
    if (askIt != AskLocation.end()) {
        auto* node = askIt->second;
        auto index = node->GetIndex();
        auto& order = Pool_.Get(index);
        uint64_t Price = order.Price;
        order.Status = (OrderStatus::CANCELED);
        DeleteAsk(order_id, askIt->second, Price);
        Pool_.Free(index);
    }
}

void BinaryOrderBook::UpdateQuantity(int order_id, int new_quantity) {

    auto bidIt = BidLocation.find(order_id);
    if (bidIt != BidLocation.end()) {
        auto* node = bidIt->second;
        auto index = node->GetIndex();
        auto& order = Pool_.Get(index);
        order.Quantity = (new_quantity);
        if (new_quantity > 0) {
            order.Status = (OrderStatus::PARTIALLY_FILLED);
        }
        else {
            order.Status = (OrderStatus::FILLED);
            DeleteBid(order_id,bidIt->second,order.Price);
            Pool_.Free(index);
        }
        return;
    }

    auto askIt = AskLocation.find(order_id);
    if (askIt != AskLocation.end()) {
        auto* node = askIt->second;
        auto index = node->GetIndex();
        auto& order = Pool_.Get(index);
        order.Quantity = (new_quantity);
        if (new_quantity > 0) {
            order.Status = (OrderStatus::PARTIALLY_FILLED);
        }
        else {
            order.Status = (OrderStatus::FILLED);
            DeleteAsk(order_id,askIt->second,order.Price);
            Pool_.Free(index);
        }
    }


}

const uint32_t BinaryOrderBook::GetBestBid()  {

    for (int i=(int)DIM_level4-1;i>=0;--i) {
        if (level4_bid[i] > 0) {

            uint64_t bit_level4 = (63 - __builtin_clzll(level4_bid[i]));
            uint64_t ind_level3 = i*64 + bit_level4;

            uint64_t bit_level3 = (63 - __builtin_clzll(level3_bid[ind_level3]));
            uint64_t ind_level2 = ind_level3*64 + bit_level3;

            uint64_t bit_level2 = (63 - __builtin_clzll(level2_bid[ind_level2]));
            uint64_t ind_level1 = ind_level2*64 + bit_level2;

            uint64_t bit_level1 = (63 - __builtin_clzll(level1_bid[ind_level1]));

            uint64_t Price = ind_level1*64 + bit_level1;

            Node* node = BidList_[Price].GetHead();
            return node->GetIndex();
        }
    }

    return UINT32_MAX;
}

const uint32_t BinaryOrderBook::GetBestAsk() {
    for (uint64_t i=0;i< DIM_level4; ++i) {
        if (level4_ask[i] > 0) {

            uint64_t bit_level4 = (__builtin_ctzll(level4_ask[i]));
            uint64_t ind_level3 = i*64 + bit_level4;

            uint64_t bit_level3 = (__builtin_ctzll(level3_ask[ind_level3]));
            uint64_t ind_level2 = ind_level3*64 + bit_level3;

            uint64_t bit_level2 = (__builtin_ctzll(level2_ask[ind_level2]));
            uint64_t ind_level1 = ind_level2*64 + bit_level2;

            uint64_t bit_level1 = (__builtin_ctzll(level1_ask[ind_level1]));

            uint64_t Price = ind_level1*64 + bit_level1;

            Node* node = AskList_[Price].GetHead();
            return node->GetIndex();
        }
    }

    return UINT32_MAX;

}

std::vector<uint32_t> BinaryOrderBook::GetBestBids(uint32_t x)  {
    std::vector<uint32_t> result;

    for (int i=(int)DIM_level4-1;i>=0;--i) {
        uint64_t L4 = level4_bid[i];

        while (L4 > 0 and (uint32_t)result.size() < x) {
            uint64_t bit_level4 = (63 - __builtin_clzll(L4));
            L4 &= ~(1ULL<<bit_level4);

            uint64_t ind_level3 = i*64 + bit_level4;
            uint64_t L3 = level3_bid[ind_level3];

            while (L3 > 0 and (uint32_t)result.size() < x) {
                uint64_t bit_level3 = (63 - __builtin_clzll(L3));
                L3 &= ~(1ULL<<bit_level3);

                uint64_t ind_level2 = ind_level3*64 + bit_level3;
                uint64_t L2 = level2_bid[ind_level2];

                while (L2 > 0 and (uint32_t)result.size() < x) {
                    uint64_t bit_level2 = (63 - __builtin_clzll(L2));
                    L2 &= ~(1ULL<<bit_level2);

                    uint64_t ind_level1 = ind_level2*64 + bit_level2;
                    uint64_t L1 = level1_bid[ind_level1];

                    while (L1 > 0 and (uint32_t)result.size() < x) {
                        uint64_t bit_level1 = (63 - __builtin_clzll(L1));
                        L1 &= ~(1ULL<<bit_level1);

                        uint64_t Price = ind_level1*64 + bit_level1;
                        Node* node = BidList_[Price].GetHead();

                        while (node != nullptr) {
                            result.push_back(node->GetIndex());
                            if (result.size() >= x) {
                                return result;
                            }
                            node = node->GetNext();
                        }
                    }
                }
            }

        }
    }
    return result;
}

std::vector<uint32_t> BinaryOrderBook::GetBestAsks(uint32_t x)  {
    std::vector<uint32_t> result;

    for (int i=0;i<DIM_level4;++i) {
        uint64_t L4 = level4_ask[i];

        while (L4 > 0 and (uint32_t)result.size() < x) {
            uint64_t bit_level4 = ( __builtin_ctzll(L4));
            L4 &= ~(1ULL<<bit_level4);

            uint64_t ind_level3 = i*64 + bit_level4;
            uint64_t L3 = level3_ask[ind_level3];
            while (L3 > 0 and (uint32_t)result.size() < x) {
                uint64_t bit_level3 = (__builtin_ctzll(L3));
                L3 &= ~(1ULL<<bit_level3);

                uint64_t ind_level2 = ind_level3*64 + bit_level3;
                uint64_t L2 = level2_ask[ind_level2];

                while (L2 > 0 and (uint32_t)result.size() < x) {
                    uint64_t bit_level2 = (__builtin_ctzll(L2));
                    L2 &= ~(1ULL<<bit_level2);

                    uint64_t ind_level1 = ind_level2*64 + bit_level2;
                    uint64_t L1 = level1_ask[ind_level1];

                    while (L1 > 0 and (uint32_t)result.size() < x) {
                        uint64_t bit_level1 = ( __builtin_ctzll(L1));
                        L1 &= ~(1ULL<<bit_level1);

                        uint64_t Price = ind_level1*64 + bit_level1;
                        Node* node = AskList_[Price].GetHead();
                        while (node != nullptr) {
                            result.push_back(node->GetIndex());
                            if (result.size() >= x) {
                                return result;
                            }
                            node = node->GetNext();
                        }
                    }
                }
            }
        }
    }
    return result;
}

bool BinaryOrderBook::IsBidEmpty() const {

    for (int i=0;i<DIM_level4;++i) {
        if (level4_bid[i] > 0) {
            return false;
        }
    }

    return true;
}

bool BinaryOrderBook::IsAskEmpty() const {
    for (uint64_t i=0;i<DIM_level4;++i) {
        if (level4_ask[i] > 0) {
            return false;
        }
    }
    return true;
}

bool BinaryOrderBook::CanFillQuantityAsks(uint32_t Quantity, uint64_t Price)  {

    uint32_t Q = 0;

    for (int i=0;i<DIM_level4;++i) {
        uint64_t L4 = level4_ask[i];

        while (L4 > 0 and Q < Quantity) {
            uint64_t bit_level4 = ( __builtin_ctzll(L4));
            L4 &= ~(1ULL<<bit_level4);

            uint64_t ind_level3 = i*64 + bit_level4;
            uint64_t L3 = level3_ask[ind_level3];
            while (L3 > 0 and Q < Quantity) {
                uint64_t bit_level3 = (__builtin_ctzll(L3));
                L3 &= ~(1ULL<<bit_level3);

                uint64_t ind_level2 = ind_level3*64 + bit_level3;
                uint64_t L2 = level2_ask[ind_level2];

                while (L2 > 0 and Q < Quantity) {
                    uint64_t bit_level2 = (__builtin_ctzll(L2));
                    L2 &= ~(1ULL<<bit_level2);

                    uint64_t ind_level1 = ind_level2*64 + bit_level2;
                    uint64_t L1 = level1_ask[ind_level1];

                    while (L1 > 0 and Q < Quantity) {
                        uint64_t bit_level1 = ( __builtin_ctzll(L1));
                        L1 &= ~(1ULL<<bit_level1);

                        uint64_t Price_ = ind_level1*64 + bit_level1;
                        if (Price_ > Price) {
                            return false;
                        }
                        Node* node = AskList_[Price_].GetHead();
                        while (node != nullptr) {
                            auto index = node->GetIndex();
                            auto& order = Pool_.Get(index);
                            Q += order.Quantity;
                            if (Q >= Quantity) {
                                return true;
                            }
                            node = node->GetNext();
                        }
                    }
                }
            }
        }
    }
    return Q >= Quantity;
}

bool BinaryOrderBook::CanFillQuantityBids(uint32_t Quantity, uint64_t Price)  {
    uint32_t Q = 0;
    for (int i=(int)DIM_level4-1;i>=0;--i) {
        uint64_t L4 = level4_bid[i];

        while (L4 > 0 and Q < Quantity) {
            uint64_t bit_level4 = (63 - __builtin_clzll(L4));
            L4 &= ~(1ULL<<bit_level4);
            uint64_t ind_level3 = i*64 + bit_level4;
            uint64_t L3 = level3_bid[ind_level3];
            while (L3 > 0 and Q < Quantity) {
                uint64_t bit_level3 = (63 - __builtin_clzll(L3));
                L3 &= ~(1ULL<<bit_level3);

                uint64_t ind_level2 = ind_level3*64 + bit_level3;
                uint64_t L2 = level2_bid[ind_level2];

                while (L2 > 0 and Q < Quantity) {
                    uint64_t bit_level2 = (63 - __builtin_clzll(L2));
                    L2 &= ~(1ULL<<bit_level2);

                    uint64_t ind_level1 = ind_level2*64 + bit_level2;
                    uint64_t L1 = level1_bid[ind_level1];

                    while (L1 > 0 and Q < Quantity) {
                        uint64_t bit_level1 = (63 - __builtin_clzll(L1));
                        L1 &= ~(1ULL<<bit_level1);

                        uint64_t Price_ = ind_level1*64 + bit_level1;
                        if (Price_ < Price) {
                            return false;
                        }
                        Node* node = BidList_[Price_].GetHead();
                        while (node != nullptr) {
                            auto index = node->GetIndex();
                            auto& order = Pool_.Get(index);
                            Q += order.Quantity;
                            if (Q >= Quantity) {
                                return true;
                            }
                            node = node->GetNext();
                        }
                    }
                }
            }

        }
    }
    return Q >= Quantity;
}

void BinaryOrderBook::PopBestBid() {
    for (int i=DIM_level4-1;i>=0;--i) {
        if (level4_bid[i] > 0) {

            uint64_t bit_level4 = (63 - __builtin_clzll(level4_bid[i]));
            uint64_t ind_level3 = i*64 + bit_level4;

            uint64_t bit_level3 = (63 - __builtin_clzll(level3_bid[ind_level3]));
            uint64_t ind_level2 = ind_level3*64 + bit_level3;

            uint64_t bit_level2 = (63 - __builtin_clzll(level2_bid[ind_level2]));
            uint64_t ind_level1 = ind_level2*64 + bit_level2;

            uint64_t bit_level1 = (63 - __builtin_clzll(level1_bid[ind_level1]));

            uint64_t Price = ind_level1*64 + bit_level1;

            Node* node = BidList_[Price].GetHead();
            auto index = node->GetIndex();
            auto& order = Pool_.Get(index);
            DeleteBid(order.OrderId,node,Price);
            order.Status = (OrderStatus::FILLED);
            Pool_.Free(index);
            return ;
        }
    }
}

void BinaryOrderBook::PopBestAsk() {
    for (uint64_t i=0;i< DIM_level4; ++i) {
        if (level4_ask[i] > 0) {

            uint64_t bit_level4 = ( __builtin_ctzll(level4_ask[i]));
            uint64_t ind_level3 = i*64 + bit_level4;

            uint64_t bit_level3 = (__builtin_ctzll(level3_ask[ind_level3]));
            uint64_t ind_level2 = ind_level3*64 + bit_level3;

            uint64_t bit_level2 = ( __builtin_ctzll(level2_ask[ind_level2]));
            uint64_t ind_level1 = ind_level2*64 + bit_level2;

            uint64_t bit_level1 = (__builtin_ctzll(level1_ask[ind_level1]));

            uint64_t Price = ind_level1*64 + bit_level1;

            Node* node = AskList_[Price].GetHead();
            auto index = node->GetIndex();
            auto& order = Pool_.Get(index);
            DeleteAsk(order.OrderId,node,Price);
            order.Status = (OrderStatus::FILLED);
            Pool_.Free(index);
            return;
        }
    }

}
