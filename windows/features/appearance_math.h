#pragma once
#include <cmath>
#include <cstdint>
#include <numbers>
namespace dt
{
inline int fine_grain(uint32_t x, uint32_t y, int strength)
{
    // Avalanche both coordinates before extracting two independent samples.
    // Their sum gives a softer, zero-centered distribution than uniform specks.
    uint32_t noise = x * 0x9e3779b9u ^ y * 0x85ebca6bu;
    noise ^= noise >> 16;
    noise *= 0x7feb352du;
    noise ^= noise >> 15;
    noise *= 0x846ca68bu;
    noise ^= noise >> 16;
    return (int(noise & 255) + int((noise >> 8) & 255) - 255) * strength / 160;
}
inline int texture_detent(double x, double y, int current)
{
    if (std::hypot(x, y) < 6)
        return current;
    double angle = std::atan2(y, x) + std::numbers::pi / 2;
    if (angle < 0)
        angle += 2 * std::numbers::pi;
    return static_cast<int>(std::round(angle / (2 * std::numbers::pi) * 16)) % 16;
}
} // namespace dt
