#include "url_style.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <regex>
#include <string>
#include <windows.h>
namespace dt
{
static constexpr auto defaults =
    R"(/* Dia-native search popup. Save to apply while DiaTweaks is running.
   This is a supported variable subset, not a web stylesheet.
   Windows controls the acrylic blur radius.
   --blur accepts system, acrylic, in-app or none. Reopen the popup after changing modes. */
:root {
  --width: 680px;
  --radius: 16px;
  --blur: system;
  --tint: #92979d;
  --tint-opacity: 0.60;
  --luminosity-opacity: 0.10;
  --border-opacity: 0.10;
  --selection-opacity: 0.09;
  --secondary-opacity: 0.50;
  --accent: #ffffff;
}
)";
UrlStyle parse_url_style(std::string_view css)
{
    UrlStyle s;
    std::string text = std::regex_replace(std::string(css), std::regex(R"(/\*[\s\S]*?\*/)"), "");
    static const std::regex declaration(R"(--([a-z-]+)\s*:\s*([^;{}]+)\s*;)");
    for (auto i = std::sregex_iterator(text.begin(), text.end(), declaration);
         i != std::sregex_iterator(); ++i)
    {
        auto key = (*i)[1].str(), value = (*i)[2].str();
        value.erase(0, value.find_first_not_of(" \r\n\t"));
        value.erase(value.find_last_not_of(" \r\n\t") + 1);
        if (key == "blur")
        {
            if (value == "none" || value == "system" || value == "acrylic" || value == "in-app")
            {
                s.blur = value != "none";
                s.backdrop = value == "in-app" ? UrlBackdrop::InApp : UrlBackdrop::System;
            }
            continue;
        }
        if (key == "accent" || key == "tint")
        {
            if (std::regex_match(value, std::regex("#[0-9a-fA-F]{6}")))
            {
                auto rgb = uint32_t(std::stoul(value.substr(1), nullptr, 16));
                (key == "accent" ? s.accent : s.tint) = rgb;
            }
            continue;
        }
        try
        {
            size_t end = 0;
            double n = std::stod(value, &end);
            bool pixel = key == "width" || key == "radius";
            auto unit = value.substr(end);
            if (!std::isfinite(n) || (!unit.empty() && !(pixel && unit == "px")))
                continue;
            if (key == "width")
                s.width = std::clamp(n, 400., 1200.);
            else if (key == "radius")
                s.radius = std::clamp(n, 0., 40.);
            else if (key == "tint-opacity")
                s.tintOpacity = std::clamp(n, 0., 1.);
            else if (key == "luminosity-opacity")
                s.luminosityOpacity = std::clamp(n, 0., 1.);
            else if (key == "border-opacity")
                s.borderOpacity = std::clamp(n, 0., 1.);
            else if (key == "selection-opacity")
                s.selectionOpacity = std::clamp(n, 0., 1.);
            else if (key == "secondary-opacity")
                s.secondaryOpacity = std::clamp(n, 0., 1.);
        }
        catch (...)
        {
        }
    }
    return s;
}
std::filesystem::path url_style_path()
{
    wchar_t folder[32768]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", folder, 32768))
        throw std::runtime_error("No app data");
    return std::filesystem::path(folder) / L"DiaTweaks" / L"url-overlay.css";
}
UrlStyle read_url_style(const std::filesystem::path &path)
{
    try
    {
        if (!std::filesystem::exists(path))
        {
            std::filesystem::create_directories(path.parent_path());
            HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                                      CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file != INVALID_HANDLE_VALUE)
            {
                DWORD written;
                WriteFile(file, defaults, DWORD(std::char_traits<char>::length(defaults)), &written,
                          nullptr);
                CloseHandle(file);
            }
        }
        if (std::filesystem::file_size(path) > 16384)
            return {};
        std::ifstream in(path, std::ios::binary);
        return parse_url_style(std::string(std::istreambuf_iterator<char>(in), {}));
    }
    catch (...)
    {
        return {};
    }
}
} // namespace dt
