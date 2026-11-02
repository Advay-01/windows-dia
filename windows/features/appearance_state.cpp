#include "appearance.h"
#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <mutex>
#include <sstream>
namespace dt
{
static std::mutex themeMutex;
static std::wstring config_path()
{
    wchar_t folder[32768]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", folder, 32768))
        throw E_FAIL;
    auto dir = std::filesystem::path(folder) / L"DiaTweaks";
    std::filesystem::create_directories(dir);
    return (dir / L"appearance.ini").wstring();
}
static AppearanceValues &values()
{
    static AppearanceValues value = [] {
        AppearanceValues s;
        try
        {
            auto p = config_path();
            s.palette = std::clamp(
                static_cast<int>(GetPrivateProfileIntW(L"Theme", L"Palette", 0, p.c_str())), 0, 5);
            s.opacity = std::clamp(
                static_cast<int>(GetPrivateProfileIntW(L"Theme", L"Opacity", 80, p.c_str())), 0,
                100);
            s.texture = std::clamp(
                static_cast<int>(GetPrivateProfileIntW(L"Theme", L"Texture", 0, p.c_str())), 0, 15);
        }
        catch (...)
        {
        }
        try
        {
            auto p = config_path();
            s.color = GetPrivateProfileIntW(L"Theme", L"Color", 0x244c68, p.c_str()) & 0xffffff;
            wchar_t savedColor[32]{};
            GetPrivateProfileStringW(L"Theme", L"Color", L"", savedColor, 32, p.c_str());
            if (*savedColor)
                s.theme = theme_from_rgb(s.color);
        }
        catch (...)
        {
        }
        try
        {
            wchar_t data[256]{};
            auto p = config_path();
            GetPrivateProfileStringW(L"Theme", L"Canvas", L"", data, 256, p.c_str());
            if (*data)
            {
                std::wistringstream in(data);
                int mode = 2, kind = 1;
                in >> s.theme.x >> s.theme.y >> s.theme.lightness >> s.theme.count >>
                    s.theme.harmony >> mode >> kind;
                s.theme.mode = static_cast<ThemeMode>(mode);
                s.theme.explicitLightness = kind != 0;
                in >> s.theme.hue >> s.theme.grayscale;
                sanitize_theme(s.theme);
            }
        }
        catch (...)
        {
        }
        return s;
    }();
    return value;
}
AppearanceValues appearance_values()
{
    std::lock_guard lock(themeMutex);
    return values();
}
void change_appearance(int palette, int opacity, int texture, bool persist)
{
    std::lock_guard lock(themeMutex);
    auto &s = values();
    s.palette = std::clamp(palette, 0, 5);
    s.opacity = std::clamp(opacity, 0, 100);
    s.texture = std::clamp(texture, 0, 15);
    s.preview = !persist;
    ++s.revision;
    if (!persist)
        return;
    try
    {
        auto p = config_path();
        WritePrivateProfileStringW(L"Theme", L"Palette", std::to_wstring(s.palette).c_str(),
                                   p.c_str());
        WritePrivateProfileStringW(L"Theme", L"Opacity", std::to_wstring(s.opacity).c_str(),
                                   p.c_str());
        WritePrivateProfileStringW(L"Theme", L"Texture", std::to_wstring(s.texture).c_str(),
                                   p.c_str());
    }
    catch (...)
    {
    }
}
} // namespace dt

namespace dt
{
void change_appearance_color(uint32_t rgb)
{
    std::lock_guard lock(themeMutex);
    auto &s = values();
    s.color = rgb & 0xffffff;
    s.theme = theme_from_rgb(s.color);
    ++s.revision;
    try
    {
        auto p = config_path();
        WritePrivateProfileStringW(L"Theme", L"Color", std::to_wstring(s.color).c_str(), p.c_str());
    }
    catch (...)
    {
    }
}
} // namespace dt

namespace dt
{
void change_theme(const ThemeModel &model, bool persist)
{
    std::lock_guard lock(themeMutex);
    auto &s = values();
    s.theme = model;
    s.preview = !persist;
    sanitize_theme(s.theme);
    ++s.revision;
    if (!persist)
        return;
    try
    {
        std::wostringstream out;
        out << std::setprecision(8) << s.theme.x << L' ' << s.theme.y << L' ' << s.theme.lightness
            << L' ' << s.theme.count << L' ' << s.theme.harmony << L' '
            << static_cast<int>(s.theme.mode) << L' ' << s.theme.explicitLightness << L' '
            << s.theme.hue << L' ' << s.theme.grayscale;
        auto p = config_path();
        WritePrivateProfileStringW(L"Theme", L"Canvas", out.str().c_str(), p.c_str());
    }
    catch (...)
    {
    }
}
} // namespace dt
