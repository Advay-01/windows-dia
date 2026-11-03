#pragma once
#include <cstdint>
#include <filesystem>
#include <string_view>
namespace dt
{
enum class UrlBackdrop { System, InApp };
struct UrlStyle
{
    double width = 680, radius = 16, tintOpacity = .60, luminosityOpacity = .10;
    double borderOpacity = .10, selectionOpacity = .09, secondaryOpacity = .50;
    uint32_t tint = 0x92979d, accent = 0xffffff;
    bool blur = true;
    UrlBackdrop backdrop = UrlBackdrop::System;
};
UrlStyle parse_url_style(std::string_view css);
std::filesystem::path url_style_path();
// Creates the editable defaults once. Reads never replace an existing user file.
UrlStyle read_url_style(const std::filesystem::path &path);
namespace url_style
{
inline constexpr double HeaderHeight = 56, InputFontSize = 18, RowHeight = 46;
inline constexpr int OpenMilliseconds = 150, CloseMilliseconds = 120;
} // namespace url_style
} // namespace dt
