//
// Created by Ognean Jason Dennis on 21.07.2026.
//

#ifndef LIMITORDERBOOK_TIME_H
#define LIMITORDERBOOK_TIME_H
#include <cstdint>
#include <chrono>


inline uint64_t NowTimestamp() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
    ).count();
}
inline std::chrono::system_clock::time_point ToTimePoint(uint64_t nanos) {
    return std::chrono::system_clock::time_point(
            std::chrono::duration_cast<std::chrono::system_clock::duration>(
                    std::chrono::nanoseconds(nanos)
            )
    );
}

#endif //LIMITORDERBOOK_TIME_H
