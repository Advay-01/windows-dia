#include "url_contrast.h"
namespace dt
{
static HWND owner(HWND w)
{
    return w ? GetAncestor(w, GA_ROOTOWNER) : nullptr;
}
static std::optional<double> sample_region(HDC dc, HWND browser, RECT area, LONG inset)
{
    if (area.right - area.left <= 2 * inset || area.bottom - area.top < 24)
        return {};
    std::vector<uint32_t> samples;
    for (double y : {.22, .40, .60, .78})
        for (LONG x : {area.left + inset, area.right - 1 - inset})
        {
            POINT point{x, area.top + LONG((area.bottom - area.top) * y)};
            if (owner(GetForegroundWindow()) != browser)
                return {};
            if (owner(WindowFromPoint(point)) != browser)
                continue;
            auto rgb = GetPixel(dc, point.x, point.y);
            if (rgb != CLR_INVALID)
                samples.push_back((uint32_t(GetRValue(rgb)) << 16) |
                                  (uint32_t(GetGValue(rgb)) << 8) | GetBValue(rgb));
        }
    return page_luminance(samples);
}

GlassLuminance sample_glass_luminance(HWND browser, RECT panel, RECT header, LONG headerInset)
{
    if (!browser || owner(GetForegroundWindow()) != browser || IsIconic(browser))
        return {};
    DWORD pid = 0;
    GetWindowThreadProcessId(browser, &pid);
    if (pid != GetCurrentProcessId())
        return {};
    HDC dc = GetDC(nullptr);
    if (!dc)
        return {};
    struct ScreenDc
    {
        HDC value;
        ~ScreenDc()
        {
            ReleaseDC(nullptr, value);
        }
    } lease{dc};
    auto input = sample_region(dc, browser, header, headerInset);
    auto body = header.bottom >= panel.bottom
                    ? input
                    : sample_region(dc, browser, panel, std::max(2L, (panel.right - panel.left) / 200));
    return {body, input};
}
} // namespace dt
