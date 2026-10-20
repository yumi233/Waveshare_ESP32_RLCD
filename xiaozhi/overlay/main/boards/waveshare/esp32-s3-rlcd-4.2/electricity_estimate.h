#pragma once
#include <cmath>
#include <cstdint>

inline float PanelDailyElectricity(float previous, float current, int64_t elapsed_seconds) {
    if (!std::isfinite(previous) || !std::isfinite(current) || current < previous ||
        elapsed_seconds < 86400 || elapsed_seconds > 14 * 86400) return -1;
    const float daily = (current - previous) * 86400.0f / elapsed_seconds;
    return daily >= 0.05f && daily <= 200.0f ? daily : -1;
}
