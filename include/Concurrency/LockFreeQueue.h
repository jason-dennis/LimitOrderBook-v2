//
// Created by Ognean Jason Dennis on 17.07.2026.
//

#ifndef LIMITORDERBOOK_LOCKFREEQUEUE_H
#define LIMITORDERBOOK_LOCKFREEQUEUE_H
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <type_traits>
#include <optional>

template<typename T, size_t Capacity>
class LockFreeQueue{
    static_assert((Capacity & (Capacity - 1)) == 0,"Capacity must be a power of 2");
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
private:
    T queue_[Capacity];
    std::atomic<uint32_t> check_[Capacity];
    alignas(64) std::atomic<uint32_t> head_{0};
    alignas(64) std::atomic<uint32_t> tail_{0};
public:

    LockFreeQueue(){
        for(size_t i = 0; i < Capacity; ++i){
            check_[i].store(static_cast<uint32_t>(i), std::memory_order_relaxed);
        }
    }

    bool Push(T item){
        while(true) {
            auto value = tail_.load(std::memory_order_relaxed);
            auto index = (value & (Capacity - 1));
            auto expected = check_[index].load(std::memory_order_acquire);
            if (expected == value) {
                if(tail_.compare_exchange_weak(value, value+1)){
                    queue_[index] = item;
                    check_[index].store(value + 1, std::memory_order_release);
                    return true;
                }
            }else if(expected < value){
                return false;
            }
        }
    }

    std::optional<T> Pop(){
        auto value = head_.load(std::memory_order_relaxed);
        auto index = (value & (Capacity - 1));;
        auto expected = check_[index].load(std::memory_order_acquire);

        if(expected == value + 1){
            auto item = queue_[index];
            check_[index].store(value + Capacity, std::memory_order_release);
            head_.store(value + 1, std::memory_order_relaxed);
            return item;
        }

        return std::nullopt;
    }

};

#endif //LIMITORDERBOOK_LOCKFREEQUEUE_H
