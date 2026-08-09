#ifndef CLIP_DETECTOR_H
#define CLIP_DETECTOR_H

#include <vector>
#include <cmath>
#include <atomic>

template<typename T>
inline bool detectClipping(const std::vector<T>& buffer) {
    for (const auto& sample : buffer) {
        if (std::fabs(sample) > T(1.0))
            return true;
    }
    return false;
}

inline void incrementIfClipping(std::atomic<int>& counter, const std::vector<float>& buffer) {
    if (detectClipping(buffer))
        counter.fetch_add(1, std::memory_order_relaxed);
}

inline void incrementIfClipping(std::atomic<int>& counter, const std::vector<double>& buffer) {
    if (detectClipping(buffer))
        counter.fetch_add(1, std::memory_order_relaxed);
}

#endif
