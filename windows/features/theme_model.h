#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>
namespace dt
{
enum class ThemeMode
{
    Auto,
    Light,
    Dark
};
struct ThemeModel
{
    double x = -.16, y = -.03, lightness = .90, hue = 0;
    bool explicitLightness = true, grayscale = false;
    int count = 3, harmony = 0;
    ThemeMode mode = ThemeMode::Dark;
};
struct ThemeDot
{
    double x, y;
    uint32_t color;
};
inline void sanitize_theme(ThemeModel &m)
{
    if (!std::isfinite(m.x))
        m.x = 0;
    if (!std::isfinite(m.y))
        m.y = 0;
    if (!std::isfinite(m.lightness))
        m.lightness = .9;
    if (!std::isfinite(m.hue))
        m.hue = 0;
    double r = std::hypot(m.x, m.y);
    if (r > 1)
    {
        m.x /= r;
        m.y /= r;
    }
    m.lightness = std::clamp(m.lightness, 0.0, 1.0);
    m.count = std::clamp(m.count, 0, 3);
    m.harmony = std::clamp(m.harmony, 0, m.count == 2 ? 1 : 2);
    if (static_cast<int>(m.mode) < 0 || static_cast<int>(m.mode) > 2)
        m.mode = ThemeMode::Auto;
}
inline void move_theme(ThemeModel &m, double x, double y)
{
    m.x = x;
    m.y = y;
    sanitize_theme(m);
    if (std::hypot(m.x, m.y) > 1e-10)
        m.hue = std::atan2(m.y, m.x);
}
inline uint32_t theme_hsl(double h, double s, double l)
{
    h = h - std::floor(h);
    s = std::clamp(s, 0.0, 1.0);
    l = std::clamp(l, 0.0, 1.0);
    const double c = (1 - std::abs(2 * l - 1)) * s, v = h * 6,
                 x = c * (1 - std::abs(std::fmod(v, 2) - 1)), a = l - c / 2;
    double r = 0, g = 0, b = 0;
    if (v < 1)
    {
        r = c;
        g = x;
    }
    else if (v < 2)
    {
        r = x;
        g = c;
    }
    else if (v < 3)
    {
        g = c;
        b = x;
    }
    else if (v < 4)
    {
        g = x;
        b = c;
    }
    else if (v < 5)
    {
        r = x;
        b = c;
    }
    else
    {
        r = c;
        b = x;
    }
    return (uint32_t(std::lround((r + a) * 255)) << 16) |
           (uint32_t(std::lround((g + a) * 255)) << 8) | uint32_t(std::lround((b + a) * 255));
}
inline std::vector<ThemeDot> theme_dots(ThemeModel m)
{
    sanitize_theme(m);
    std::vector<ThemeDot> out;
    double r = std::hypot(m.x, m.y), a = r > 1e-10 ? std::atan2(m.y, m.x) : m.hue;
    const double pairs[2] = {180, 310};
    const double triples[3][2] = {{150, 210}, {50, 310}, {120, 240}};
    for (int i = 0; i < m.count; ++i)
    {
        double angle = a + (i == 0         ? 0
                            : m.count == 2 ? pairs[m.harmony % 2]
                                           : triples[m.harmony][i - 1]) *
                               std::numbers::pi / 180;
        out.push_back({r * std::cos(angle), r * std::sin(angle),
                       theme_hsl(angle / (2 * std::numbers::pi),
                                 m.grayscale           ? 0
                                 : m.explicitLightness ? 1 - r
                                                       : .9 + .1 * r,
                                 m.grayscale           ? r
                                 : m.explicitLightness ? m.lightness
                                                       : r)});
    }
    return out;
}
inline ThemeModel theme_from_rgb(uint32_t rgb)
{
    double r = ((rgb >> 16) & 255) / 255., g = ((rgb >> 8) & 255) / 255., b = (rgb & 255) / 255.;
    double hi = std::max({r, g, b}), lo = std::min({r, g, b}), d = hi - lo, l = (hi + lo) / 2,
           h = 0, s = 0;
    if (d > 0)
    {
        s = d / (1 - std::abs(2 * l - 1));
        if (hi == r)
            h = std::fmod((g - b) / d, 6.);
        else if (hi == g)
            h = (b - r) / d + 2;
        else
            h = (r - g) / d + 4;
        h *= std::numbers::pi / 3;
    }
    ThemeModel m;
    m.x = (1 - s) * std::cos(h);
    m.y = (1 - s) * std::sin(h);
    m.hue = h;
    m.lightness = l;
    m.count = 1;
    return m;
}
inline constexpr int ThemePresetCount = 40;
inline uint32_t theme_preset_color(int index)
{
    constexpr uint32_t pastel[] = {0xf4efdf, 0xf0b8cd, 0xe9c3e3, 0xda7682,
                                   0xeb8570, 0xdcce7f, 0x5becad, 0x919bb5};
    constexpr uint32_t dark[] = {0x5d566a, 0x997096, 0x956066, 0x9c6645,
                                 0x517b6c, 0x576e75, 0x836d5f, 0x447464};
    index = std::clamp(index, 0, 39);
    if (index < 16)
        return pastel[index % 8];
    if (index < 32)
        return dark[index % 8];
    uint32_t v = 255 - (index - 32) * 32;
    return (v << 16) | (v << 8) | v;
}
inline ThemeModel theme_preset(int index)
{
    index = std::clamp(index, 0, ThemePresetCount - 1);
    auto m = theme_from_rgb(theme_preset_color(index));
    m.count = (index / 8 == 1 || index / 8 == 3) ? 3 : 1;
    m.harmony = 1;
    if (index >= 32)
    {
        m.grayscale = true;
        m.x = m.lightness;
        m.y = 0;
    }
    return m;
}
struct ThemeRender
{
    std::vector<ThemeDot> dots;
    uint32_t base;
    ThemeRender(const ThemeModel &m, bool light)
        : dots(theme_dots(m)),
          base((m.mode == ThemeMode::Light || (m.mode == ThemeMode::Auto && light)) ? 0xf2f2f2
                                                                                    : 0x202124)
    {
    }
    uint32_t pixel(double u, double v) const
    {
        if (dots.empty())
            return base;
        if (dots.size() == 1)
            return dots[0].color;
        std::array<double, 3> c{double((base >> 16) & 255), double((base >> 8) & 255),
                                double(base & 255)};
        auto mix = [&](uint32_t color, double a) {
            a = std::clamp(a, 0., 1.);
            for (int k = 0; k < 3; ++k)
                c[k] = c[k] * (1 - a) + double((color >> (16 - 8 * k)) & 255) * a;
        };
        auto linear = [&](double degrees, double start, double end) {
            double a = degrees * std::numbers::pi / 180, s = std::sin(a), co = -std::cos(a);
            double t = .5 + ((u - .5) * s + (v - .5) * co) / (std::abs(s) + std::abs(co));
            return 1 - (t - start) / (end - start);
        };
        if (dots.size() == 2)
        {
            mix(dots[1].color, linear(-30, .3, 1.2));
            mix(dots[0].color, linear(150, .3, 1.2));
        }
        else
        {
            mix(dots[0].color, 1 - (std::hypot(u, v) / std::sqrt(2.) - .1) / .6);
            mix(dots[1].color, 1 - std::hypot(u - .95, v) / (std::hypot(.95, 1.) * .75));
            mix(dots[2].color, linear(-5, .1, .8));
        }
        return (uint32_t(std::lround(c[0])) << 16) | (uint32_t(std::lround(c[1])) << 8) |
               uint32_t(std::lround(c[2]));
    }
};
} // namespace dt
