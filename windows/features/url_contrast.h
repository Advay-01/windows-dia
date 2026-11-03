#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>
#include <windows.h>
namespace dt
{
inline double relative_luminance(uint32_t rgb)
{
    auto channel = [](unsigned n) {
        double s = n / 255.;
        return s <= .04045 ? s / 12.92 : std::pow((s + .055) / 1.055, 2.4);
    };
    return .2126 * channel((rgb >> 16) & 255) + .7152 * channel((rgb >> 8) & 255) +
           .0722 * channel(rgb & 255);
}
inline std::optional<double> page_luminance(const std::vector<uint32_t> &samples)
{
    if (samples.size() < 3)
        return {};
    std::vector<double> values;
    for (auto rgb : samples)
        values.push_back(relative_luminance(rgb));
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}
struct PageContrast
{
    std::optional<bool> light, candidate;
    int confirmations = 0;
    bool update(std::optional<double> luminance)
    {
        if (!luminance)
            return false;
        bool next = !light ? *luminance >= .179 : (*light ? *luminance >= .15 : *luminance > .21);
        if (!light)
        {
            light = next;
            return true;
        }
        if (next == *light)
        {
            candidate.reset();
            confirmations = 0;
            return false;
        }
        if (candidate != next)
        {
            candidate = next;
            confirmations = 1;
            return false;
        }
        if (++confirmations < 2)
            return false;
        light = next;
        candidate.reset();
        confirmations = 0;
        return true;
    }
};
struct GlassLuminance
{
    std::optional<double> panel;
    std::optional<double> input;
};
// Reads the empty side insets of the rendered glass. No images are created.
GlassLuminance sample_glass_luminance(HWND browser, RECT panel, RECT header, LONG headerInset);
} // namespace dt
