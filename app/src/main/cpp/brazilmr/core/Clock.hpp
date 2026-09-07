// BrazilMR — relógio monotônico de alta resolução (nanossegundos).
#pragma once

#include <chrono>
#include <cstdint>

namespace brazilmr {

// Nanossegundos desde um ponto fixo arbitrário (relógio monotônico).
inline int64_t nowNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

inline double nsToSeconds(int64_t ns) { return static_cast<double>(ns) * 1e-9; }
inline int64_t secondsToNs(double s) {
    return static_cast<int64_t>(s * 1e9 + 0.5);
}

} // namespace brazilmr
